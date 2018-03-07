// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "StreamEncryptionHandlerComponent.h"
#include "XORStreamEncryptor.h"

#include "Modules/ModuleManager.h"

#include "CryptoPP/5.6.5/include/osrng.h"

IMPLEMENT_MODULE(FStreamEncryptionHandlerComponentModuleInterface, StreamEncryptionHandlerComponent);

///////////////////////////////////////////////////
// STREAM ENCRYPTION
///////////////////////////////////////////////////

// Stream Encryption Handler
StreamEncryptionHandlerComponent::StreamEncryptionHandlerComponent(StreamEncryptor* InEncryptor, uint32 InKeySizeInBytes)
	: Key()
{
	if (InEncryptor != nullptr)
	{
		Encryptor = InEncryptor;
	} else
	{
		Encryptor = new XORStreamEncryptor;
	}

	// Set key size, if no key size passed in then get default key size for the encryptor
	KeySizeInBytes = InKeySizeInBytes != 0 ? InKeySizeInBytes : Encryptor->GetDefaultKeySize();
}

void StreamEncryptionHandlerComponent::Initialize()
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

bool StreamEncryptionHandlerComponent::IsValid() const
{
	return Key.Num() > 0;
}

void StreamEncryptionHandlerComponent::Incoming(FBitReader& Packet)
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
				DecryptStream(Packet);
			}
			break;
		}
		default:
		{
			break;
		}
	}
}

void StreamEncryptionHandlerComponent::Outgoing(FBitWriter& Packet)
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
				EncryptStream(Packet);
			}
			break;
		}
		default:
		{
			break;
		}
	}
}

void StreamEncryptionHandlerComponent::EncryptStream(FBitWriter& Packet)
{
	Encryptor->EncryptStream(Packet.GetData(), Packet.GetNumBytes());
}

void StreamEncryptionHandlerComponent::DecryptStream(FBitReader& Packet)
{
	Encryptor->DecryptStream(Packet.GetData(), Packet.GetNumBytes());
}

// MODULE INTERFACE

TSharedPtr<HandlerComponent> FStreamEncryptionHandlerComponentModuleInterface::CreateComponentInstance(FString& Options)
{
	TSharedPtr<HandlerComponent> ReturnVal = NULL;

	if (!Options.IsEmpty())
	{
		FStreamEncryptorModuleInterface* Interface = FModuleManager::LoadModulePtr<FStreamEncryptorModuleInterface>(FName(*Options));
		TSharedPtr<FStreamEncryptorModuleInterface> StreamEncryptorInterface(Interface);

		ReturnVal = MakeShareable(new StreamEncryptionHandlerComponent(StreamEncryptorInterface->CreateStreamEncryptorInstance()));
	}
	else
	{
		ReturnVal = MakeShareable(new StreamEncryptionHandlerComponent);
	}

	return ReturnVal;
}