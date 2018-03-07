// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "BlockEncryptionHandlerComponent.h"
#include "XORBlockEncryptor.h"

#include "Modules/ModuleManager.h"

IMPLEMENT_MODULE(FBlockEncryptionHandlerComponentModuleInterface, BlockEncryptionHandlerComponent);

///////////////////////////////////////////////////
// BLOCK ENCRYPTION
///////////////////////////////////////////////////

// Block Encryption Handler
BlockEncryptionHandlerComponent::BlockEncryptionHandlerComponent(BlockEncryptor* InEncryptor, uint32 InKeySizeInBytes)
	: Key()
{
	if (InEncryptor != nullptr)
	{
		Encryptor = InEncryptor;
	}
	else
	{
		Encryptor = new XORBlockEncryptor;
	}

	// Set key size, if no key size passed in then get default key size for the encryptor
	KeySizeInBytes = InKeySizeInBytes != 0 ? InKeySizeInBytes : Encryptor->GetDefaultKeySize();
}

void BlockEncryptionHandlerComponent::Initialize()
{
	if (Handler->Mode == Handler::Mode::Client)
	{
		// Generate the key
		CryptoPP::AutoSeededRandomPool Rng;

		Key.Reserve(KeySizeInBytes);
		uint8* Secret = new uint8[KeySizeInBytes];

		Rng.GenerateBlock(Secret, KeySizeInBytes);

		for (uint32 i = 0; i < KeySizeInBytes; ++i)
		{
			Key.Add(Secret[i]);
		}

		Encryptor->Initialize(&Key);
		SetState(Handler::Component::State::InitializedOnLocal);
	}
	SetActive(true);
}

bool BlockEncryptionHandlerComponent::IsValid() const
{
	return Key.Num() > 0;
}

void BlockEncryptionHandlerComponent::Incoming(FBitReader& Packet)
{
	switch (State)
	{
		case Handler::Component::State::InitializedOnLocal:
		case Handler::Component::State::UnInitialized:
		{
			TArray<uint8> ReceivedKey;
			Packet << ReceivedKey;

			// Set key
			Key = ReceivedKey;
			KeySizeInBytes = uint32(Key.Num());

			Encryptor->Initialize(&Key);
			SetState(Handler::Component::State::Initialized);
			Initialized();
			break;
		}
		case Handler::Component::State::Initialized:
		{
			// Handle this packet
			if (IsValid() && Packet.GetNumBytes() > 0)
			{
				DecryptBlock(Packet);
			}
			break;
		}
		default:
		{
			break;
		}
	}
}

void BlockEncryptionHandlerComponent::Outgoing(FBitWriter& Packet)
{
	switch (State)
	{
		case Handler::Component::State::UnInitialized:
		{
			break;
		}
		// Send packet initializing remote handler
		case Handler::Component::State::InitializedOnLocal:
		{
			FBitWriter Local;
			Local.AllowAppend(true);
			Local.SetAllowResize(true);

			Local << Key;
			Local.Serialize(Packet.GetData(), Packet.GetNumBytes());
			Packet = Local;

			SetState(Handler::Component::State::Initialized);
			SetActive(true);
			Initialized();
			break;
		}
		case Handler::Component::State::Initialized:
		{
			// Handle this packet
			if (IsValid() && Packet.GetNumBytes() > 0)
			{
				EncryptBlock(Packet);
			}
			break;
		}
		default:
		{
			break;
		}
	}
}

void BlockEncryptionHandlerComponent::EncryptBlock(FBitWriter& Packet)
{
	uint32 BlockSize = Encryptor->GetFixedBlockSize();

	TArray<uint8> Block;
	Block.Reserve((Packet.GetNumBytes() / BlockSize) + 1);

	// Copy data
	for (int32 i = 0; i < Packet.GetNumBytes(); ++i)
	{
		Block.Add(Packet.GetData()[i]);
	}

	// Add padding
	while (Block.Num() % BlockSize != 0)
	{
		Block.Add(0x00);
	}

	int32 BlockCount = Block.Num();

	for (int32 i = 0; i < int32(BlockCount / BlockSize); ++i)
	{
		Encryptor->EncryptBlock(Block.GetData() + i * KeySizeInBytes);
	}

	uint32 PacketSizeBeforeEncryption = static_cast<uint32>(Packet.GetNumBytes());

	Packet.Reset();

	// Serialize size of packet before encryption
	Packet.SerializeIntPacked(PacketSizeBeforeEncryption);

	// Copy data back
	for (int32 i = 0; i < BlockCount; ++i)
	{
		Packet << Block[i];
	}
}

void BlockEncryptionHandlerComponent::DecryptBlock(FBitReader& Packet)
{
	uint32 BlockSize = Encryptor->GetFixedBlockSize();

	TArray<uint8> Block;
	Block.Reserve((Packet.GetNumBytes() / BlockSize) + 1);

	uint32 PacketSizeBeforeEncryption;
	Packet.SerializeIntPacked(PacketSizeBeforeEncryption);

	// Copy data
	for (int32 i = Packet.GetPosBits() / 8; i < Packet.GetNumBytes(); ++i)
	{
		Block.Add(Packet.GetData()[i]);
	}

	int32 BlockCount = Block.Num();

	// Not a valid sized block to decrypt, seek to end to discontinue handling
	if (BlockCount % BlockSize != 0)
	{
		Packet.Seek(Packet.GetNumBytes());
		return;
	}

	for (int32 i = 0; i < int32(BlockCount / BlockSize); ++i)
	{
		Encryptor->DecryptBlock(Block.GetData() + i * KeySizeInBytes);
	}

	// Packet size before encryption should always be
	// smaller or equal to the block size
	if (PacketSizeBeforeEncryption > static_cast<uint32>(Block.Num()))
	{
		abort();
	}

	FBitReader Copy(Block.GetData(), PacketSizeBeforeEncryption * 8);
	Packet = Copy;

	FString KeyString;

	for (int32 i = 0; i < Key.Num(); ++i)
	{
		KeyString.AppendChar(Key[i]);
	}
}

// BLOCK ENCRYPTOR
uint32 BlockEncryptor::GetFixedBlockSize()
{
	return FixedBlockSize;
}

// MODULE INTERFACE
TSharedPtr<HandlerComponent> FBlockEncryptionHandlerComponentModuleInterface::CreateComponentInstance(FString& Options)
{
	TSharedPtr<HandlerComponent> ReturnVal = NULL;

	if (!Options.IsEmpty())
	{
		FBlockEncryptorModuleInterface* Interface = FModuleManager::LoadModulePtr<FBlockEncryptorModuleInterface>(FName(*Options));
		TSharedPtr<FBlockEncryptorModuleInterface> BlockEncryptorInterface(Interface);

		ReturnVal = MakeShareable(new BlockEncryptionHandlerComponent(BlockEncryptorInterface->CreateBlockEncryptorInstance()));
	}
	else
	{
		ReturnVal = MakeShareable(new BlockEncryptionHandlerComponent);
	}

	return ReturnVal;
}