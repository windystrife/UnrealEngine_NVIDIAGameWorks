// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AESHandlerComponent.h"

IMPLEMENT_MODULE( FAESHandlerComponentModule, AESHandlerComponent )

TSharedPtr<HandlerComponent> FAESHandlerComponentModule::CreateComponentInstance(FString& Options)
{
	TSharedPtr<HandlerComponent> ReturnVal = NULL;

	ReturnVal = MakeShared<FAESHandlerComponent>();

	return ReturnVal;
}


FAESHandlerComponent::FAESHandlerComponent()
	: bEncryptionEnabled(false)
{
	EncryptionContext = IPlatformCrypto::Get().CreateContext();
}

void FAESHandlerComponent::SetEncryptionKey(TArrayView<const uint8> NewKey)
{
	if (NewKey.Num() != KeySizeInBytes)
	{
		UE_LOG(PacketHandlerLog, Log, TEXT("FAESHandlerComponent::SetEncryptionKey. NewKey is not %d bytes long, ignoring."), KeySizeInBytes);
		return;
	}

	Key.Reset(KeySizeInBytes);
	Key.Append(NewKey.GetData(), NewKey.Num());
}

void FAESHandlerComponent::EnableEncryption()
{
	bEncryptionEnabled = true;
}

void FAESHandlerComponent::DisableEncryption()
{
	bEncryptionEnabled = false;
}

void FAESHandlerComponent::Initialize()
{
	SetActive(true);
	SetState(Handler::Component::State::Initialized);
	Initialized();
}

bool FAESHandlerComponent::IsValid() const
{
	return true;
}

void FAESHandlerComponent::Incoming(FBitReader& Packet)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("PacketHandler AES Decrypt"), STAT_PacketHandler_AES_Decrypt, STATGROUP_Net);

	// Handle this packet
	if (IsValid() && Packet.GetNumBytes() > 0)
	{
		// Check first bit to see whether payload is encrypted
		if (Packet.ReadBit() != 0)
		{
			// If the key hasn't been set yet, we can't decrypt, so ignore this packet. We don't set an error in this case because it may just be an out-of-order packet.
			if (Key.Num() == 0)
			{
				UE_LOG(PacketHandlerLog, Log, TEXT("FAESHandlerComponent::Incoming: received encrypted packet before key was set, ignoring."));
				FBitReader EmptyPacket(nullptr, 0);
				Packet = EmptyPacket;
				return;
			}

			// Copy remaining bits to a TArray so that they are byte-aligned.
			TArray<uint8> Ciphertext;
			Ciphertext.AddZeroed(Packet.GetBytesLeft());
			Packet.SerializeBits(Ciphertext.GetData(), Packet.GetBitsLeft());

			UE_LOG(PacketHandlerLog, VeryVerbose, TEXT("AES packet handler received %ld bytes before decryption."), Ciphertext.Num());

			EPlatformCryptoResult DecryptResult = EPlatformCryptoResult::Failure;
			TArray<uint8> Plaintext = EncryptionContext->Decrypt_AES_256_ECB(Ciphertext, Key, DecryptResult);

			if (DecryptResult == EPlatformCryptoResult::Failure)
			{
				UE_LOG(PacketHandlerLog, Log, TEXT("FAESHandlerComponent::Incoming: failed to decrypt packet."));
				Packet.SetError();
				return;
			}
			else
			{
				// Look for the termination bit that was written in Outgoing() to determine the exact bit size.
				uint8 LastByte = Plaintext.Last();

				if (LastByte != 0)
				{
					int32 BitSize = (Plaintext.Num() * 8) - 1;

					// Bit streaming, starts at the Least Significant Bit, and ends at the MSB.
					while (!(LastByte & 0x80))
					{
						LastByte *= 2;
						BitSize--;
					}

					UE_LOG(PacketHandlerLog, VeryVerbose, TEXT("  Have %d bits after decryption."), BitSize);

					FBitReader OutPacket(Plaintext.GetData(), BitSize);
					Packet = OutPacket;
				}
				else
				{
					UE_LOG(PacketHandlerLog, Log, TEXT("FAESHandlerComponent::Incoming: malformed packet, last byte was 0."));
					Packet.SetError();
				}
			}
		}
	}
}

void FAESHandlerComponent::Outgoing(FBitWriter& Packet)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("PacketHandler AES Encrypt"), STAT_PacketHandler_AES_Encrypt, STATGROUP_Net);

	// Handle this packet
	if (IsValid() && Packet.GetNumBytes() > 0)
	{
		// Allow for encryption enabled bit and termination bit. Allow resizing to account for encryption padding.
		FBitWriter NewPacket(Packet.GetNumBits() + 2, true);
		NewPacket.WriteBit(bEncryptionEnabled ? 1 : 0);

		if (NewPacket.IsError())
		{
			UE_LOG(PacketHandlerLog, Log, TEXT("FAESHandlerComponent::Outgoing: failed to write encryption bit."));
			Packet.SetError();
			return;
		}

		if (bEncryptionEnabled)
		{
			UE_LOG(PacketHandlerLog, VeryVerbose, TEXT("AES packet handler sending %ld bits before encryption."), Packet.GetNumBits());

			// Write a termination bit so that the receiving side can calculate the exact number of bits sent.
			// Same technique used in UNetConnection.
			Packet.WriteBit(1);

			if (Packet.IsError())
			{
				UE_LOG(PacketHandlerLog, Log, TEXT("FAESHandlerComponent::Outgoing: failed to write termination bit."));
				return;
			}

			EPlatformCryptoResult EncryptResult = EPlatformCryptoResult::Failure;
			TArray<uint8> Ciphertext = EncryptionContext->Encrypt_AES_256_ECB(TArrayView<uint8>(Packet.GetData(), Packet.GetNumBytes()), Key, EncryptResult);

			if (EncryptResult == EPlatformCryptoResult::Failure)
			{
				UE_LOG(PacketHandlerLog, Log, TEXT("FAESHandlerComponent::Outgoing: failed to encrypt packet."));
				Packet.SetError();
				return;
			}
			else
			{
				NewPacket.Serialize(Ciphertext.GetData(), Ciphertext.Num());

				if (NewPacket.IsError())
				{
					UE_LOG(PacketHandlerLog, Log, TEXT("FAESHandlerComponent::Outgoing: failed to write ciphertext to packet."));
					Packet.SetError();
					return;
				}

				UE_LOG(PacketHandlerLog, VeryVerbose, TEXT("  AES packet handler sending %d bytes after encryption."), NewPacket.GetNumBytes());
			}
		}
		else
		{
			NewPacket.SerializeBits(Packet.GetData(), Packet.GetNumBits());
		}

		Packet.Reset();
		Packet.SerializeBits(NewPacket.GetData(), NewPacket.GetNumBits());
	}
}

void FAESHandlerComponent::IncomingConnectionless(FString Address, FBitReader& Packet)
{
}

void FAESHandlerComponent::OutgoingConnectionless(FString Address, FBitWriter& Packet)
{
}

int32 FAESHandlerComponent::GetReservedPacketBits()
{
	// Worst case includes the encryption enabled bit, the termination bit, padding up to the next whole byte, and a block of padding.
	return 2 + 7 + (BlockSizeInBytes * 8);
}
