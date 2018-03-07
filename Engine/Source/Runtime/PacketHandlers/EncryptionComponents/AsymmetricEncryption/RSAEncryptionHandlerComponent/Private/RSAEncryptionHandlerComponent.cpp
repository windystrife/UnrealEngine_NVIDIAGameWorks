// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "RSAEncryptionHandlerComponent.h"

#include "PacketAudit.h"

#include "Modules/ModuleManager.h"


DEFINE_LOG_CATEGORY(PacketHandlerLog);
IMPLEMENT_MODULE(FRSAEncryptorHandlerComponentModuleInterface, RSAEncryptionHandlerComponent);


// @todo #JohnB: Add/test support for CanReadUnaligned.


// Defines

/** Puts a limit on the maximum size of the exponent. */
// @todo: This should be limited further, as a high exponent could potentially be used for performing a DoS attack,
//			by making it very costly to decrypt packets. The limit is only this high temporarily.
#define MAX_EXPONENT_BITS 1024

/**
 * RSAEncryptionHandlerComponent
 */
RSAEncryptionHandlerComponent::RSAEncryptionHandlerComponent(int32 InKeySizeInBits)
	: PrivateKeyMaxPlaintextLength(0)
	, PrivateKeyFixedCiphertextLength(0)
	, RemotePublicKeyMaxPlaintextLength(0)
	, RemotePublicKeyFixedCiphertextLength(0)
	, KeySizeInBits(InKeySizeInBits)
	, State(ERSAEncryptionHandler::State::UnInitialized)
	, Rng()
	, Params()
	, RemotePublicEncryptor()
	, PrivateEncryptor()
	, PrivateDecryptor()
	, PublicKey()
	, PrivateKey()
	, RemotePublicKey()
{
	SetActive(true);

	bRequiresHandshake = true;
}

void RSAEncryptionHandlerComponent::Initialize()
{
	Params.GenerateRandomWithKeySize(Rng, KeySizeInBits);
	PublicKey = CryptoPP::RSA::PublicKey(Params);
	PrivateKey = CryptoPP::RSA::PrivateKey(Params);

	PrivateEncryptor = CryptoPP::RSAES_OAEP_SHA_Encryptor(PrivateKey);
	PrivateDecryptor = CryptoPP::RSAES_OAEP_SHA_Decryptor(PrivateKey);

	PrivateKeyMaxPlaintextLength = static_cast<uint32>(PrivateEncryptor.FixedMaxPlaintextLength());
	PrivateKeyFixedCiphertextLength = static_cast<uint32>(PrivateEncryptor.FixedCiphertextLength());
}

void RSAEncryptionHandlerComponent::NotifyHandshakeBegin()
{
	// Send the local key
	FBitWriter OutPacket;

	PackLocalKey(OutPacket);
	SetState(ERSAEncryptionHandler::State::InitializedLocalKeysSentLocal);

	FPacketAudit::AddStage(TEXT("RSAHandshake"), OutPacket);

	Handler->SendHandlerPacket(this, OutPacket);
}

bool RSAEncryptionHandlerComponent::IsValid() const
{
	return PrivateKeyMaxPlaintextLength > 0;
}

void RSAEncryptionHandlerComponent::SetState(ERSAEncryptionHandler::State InState)
{
	State = InState;
}

void RSAEncryptionHandlerComponent::Outgoing(FBitWriter& Packet)
{
	if (State == ERSAEncryptionHandler::State::Initialized)
	{
		if (Packet.GetNumBytes() > 0)
		{
			Encrypt(Packet);
		}
	}
	else
	{
		UE_LOG(PacketHandlerLog, Warning, TEXT("RSAEncryptionHandlerComponent: Got outgoing packet when not yet initialized."));

#if !UE_BUILD_SHIPPING
		check(false);
#endif

		Packet.SetError();
	}
}

void RSAEncryptionHandlerComponent::Incoming(FBitReader& Packet)
{
	if (State == ERSAEncryptionHandler::State::Initialized)
	{
		Decrypt(Packet);
	}
	else if (State == ERSAEncryptionHandler::State::InitializedLocalKeysSentLocal)
	{
		FPacketAudit::CheckStage(TEXT("RSAHandshake"), Packet);

		UnPackRemoteKey(Packet);
		Initialized();
		SetState(ERSAEncryptionHandler::State::Initialized);
	}
	else
	{
		UE_LOG(PacketHandlerLog, Warning, TEXT("RSAEncryptionHandlerComponent: Got incoming packet when not yet initialized."));

#if !UE_BUILD_SHIPPING
		check(false);
#endif

		Packet.SetError();
	}
}

void RSAEncryptionHandlerComponent::PackLocalKey(FBitWriter& Packet)
{
	FBitWriter Local;
	Local.AllowAppend(true);
	Local.SetAllowResize(true);

	// Save remote public key information
	TArray<uint8> ModulusArray;
	TArray<uint8> ExponentArray;

	SavePublicKeyModulus(ModulusArray);
	SavePublicKeyExponent(ExponentArray);

	if ((ModulusArray.Num() * 8u) > KeySizeInBits || ModulusArray.Num() == 0)
	{
		LowLevelFatalError(TEXT("Modulus size '%i bits' must be greater than zero, and must not exceed key size '%i'"),
							(ModulusArray.Num() * 8), KeySizeInBits);
	}

	if ((ExponentArray.Num() * 8) > MAX_EXPONENT_BITS || ExponentArray.Num() == 0)
	{
		LowLevelFatalError(TEXT("Exponent size '%i bits' must be greater than zero, and must not exceed MAX_EXPONENT_BITS"),
							(ExponentArray.Num() * 8));
	}


	const uint32 MaxModulusNum = (KeySizeInBits+7) >> 3;
	const uint32 MaxExponentNum = (MAX_EXPONENT_BITS + 7) >> 3;

	// Decrement by one, to allow serialization of #Num == Max#Num
	uint32 ModulusSerializeNum = ModulusArray.Num() - 1;
	uint32 ExponentSerializeNum = ExponentArray.Num() - 1;

	Local.SerializeInt(ModulusSerializeNum, MaxModulusNum);
	Local.Serialize(ModulusArray.GetData(), ModulusArray.Num());


	Local.SerializeInt(ExponentSerializeNum, MaxExponentNum);
	Local.Serialize(ExponentArray.GetData(), ExponentArray.Num());

	Local.Serialize(Packet.GetData(), Packet.GetNumBytes());

	Packet = Local;
}

void RSAEncryptionHandlerComponent::UnPackRemoteKey(FBitReader& Packet)
{
	// Save remote public key
	TArray<uint8> ModulusArray;
	TArray<uint8> ExponentArray;
	const uint32 MaxModulusNum = (KeySizeInBits+7) >> 3;
	const uint32 MaxExponentNum = (MAX_EXPONENT_BITS + 7) >> 3;

	uint32 ModulusNum = 0;

	Packet.SerializeInt(ModulusNum, MaxModulusNum);

	ModulusNum++;

	if ((ModulusNum * 8) > KeySizeInBits)
	{
		UE_LOG(PacketHandlerLog, Warning, TEXT("Modulus size '%i bits' should not exceed key size '%i'"),
							(ModulusNum * 8), KeySizeInBits);

		Packet.SetError();
	}

	if (!Packet.IsError())
	{
		ModulusArray.SetNumUninitialized(ModulusNum);
		Packet.Serialize(ModulusArray.GetData(), ModulusNum);

		uint32 ExponentNum = 0;

		Packet.SerializeInt(ExponentNum, MaxExponentNum);

		ExponentNum++;

		if ((ExponentNum * 8) <= MAX_EXPONENT_BITS)
		{
			ExponentArray.SetNumUninitialized(ExponentNum);
			Packet.Serialize(ExponentArray.GetData(), ExponentNum);
		}
		else
		{
			UE_LOG(PacketHandlerLog, Warning, TEXT("Exponent size '%i bits' should not exceed MAX_EXPONENT_BITS"),
								(ExponentNum * 8));

			Packet.SetError();
		}
	}

	if (!Packet.IsError())
	{
		// Make sure the packet has no remaining data now
#if !UE_BUILD_SHIPPING
		check(Packet.GetBitsLeft() == 0);
#endif

		CryptoPP::Integer Modulus;
		CryptoPP::Integer Exponent;

		for (int32 i = 0; i < ModulusArray.Num(); ++i)
		{
			Modulus.SetByte(i, ModulusArray[i]);
		}

		for (int32 i = 0; i < ExponentArray.Num(); ++i)
		{
			Exponent.SetByte(i, ExponentArray[i]);
		}

		RemotePublicKey.SetModulus(Modulus);
		RemotePublicKey.SetPublicExponent(Exponent);
		RemotePublicEncryptor = CryptoPP::RSAES_OAEP_SHA_Encryptor(RemotePublicKey);
		RemotePublicKeyMaxPlaintextLength = static_cast<uint32>(RemotePublicEncryptor.FixedMaxPlaintextLength());
		RemotePublicKeyFixedCiphertextLength = static_cast<uint32>(RemotePublicEncryptor.FixedCiphertextLength());
	}
}

void RSAEncryptionHandlerComponent::Encrypt(FBitWriter& Packet)
{
	// Serialize size of plain text data
	uint32 NumberOfBytesInPlaintext = static_cast<uint32>(Packet.GetNumBytes());

	TArray<uint8> PlainText;

	// Copy byte stream to PlainText from Packet
	for (int32 i = 0; i < Packet.GetNumBytes(); ++i)
	{
		PlainText.Add(Packet.GetData()[i]);
	}

	Packet.Reset();

	if (NumberOfBytesInPlaintext == 0 || NumberOfBytesInPlaintext > RemotePublicKeyMaxPlaintextLength)
	{
		if (NumberOfBytesInPlaintext > RemotePublicKeyMaxPlaintextLength)
		{
			UE_LOG(PacketHandlerLog, Warning, TEXT("RSA Encryption skipped as plain text size is too large for this key size. Increase key size or send smaller packets."));
		}

		uint32 NoBytesEncrypted = 0;
		Packet.SerializeIntPacked(NoBytesEncrypted);
		Packet.Serialize(PlainText.GetData(), PlainText.Num());
		return;
	}

	// Serialize invalid amount of bytes
	Packet.SerializeIntPacked(NumberOfBytesInPlaintext);

	TArray<uint8> CipherText;

	// The number of bytes in the plaintext is too large for the cipher text
	EncryptWithRemotePublic(PlainText, CipherText);

	// Copy byte stream to Packet from CipherText
	for (int i = 0; i < CipherText.Num(); ++i)
	{
		Packet << CipherText[i];
	}
}

void RSAEncryptionHandlerComponent::Decrypt(FBitReader& Packet)
{
	// Serialize size of plain text data
	uint32 NumberOfBytesInPlaintext;
	Packet.SerializeIntPacked(NumberOfBytesInPlaintext);

	if (NumberOfBytesInPlaintext == 0 || NumberOfBytesInPlaintext > RemotePublicKeyMaxPlaintextLength)
	{
		// Remove header
		FBitReader Copy(Packet.GetData() + (Packet.GetNumBytes() - Packet.GetBytesLeft()), Packet.GetBitsLeft());
		Packet = Copy;

		if (NumberOfBytesInPlaintext > RemotePublicKeyMaxPlaintextLength)
		{
			UE_LOG(PacketHandlerLog, Warning, TEXT("RSA Decryption skipped as cipher text size is too large for this key size. Increase key size or send smaller packets."));
		}
		return;
	}

	TArray<uint8> PlainText;
	TArray<uint8> CipherText;

	// Copy byte stream to PlainText from Packet
	// not including the NumOfBytesPT field
	for (int32 i = Packet.GetPosBits() / 8; i < Packet.GetNumBytes(); ++i)
	{
		CipherText.Add(Packet.GetData()[i]);
	}

	DecryptWithPrivate(CipherText, PlainText);

	FBitReader Copy(PlainText.GetData(), NumberOfBytesInPlaintext * 8);
	Packet = Copy;
}

void RSAEncryptionHandlerComponent::SavePublicKeyModulus(TArray<uint8>& OutModulus)
{
	uint32 ModulusSize = static_cast<uint32>(PublicKey.GetModulus().ByteCount());

	CryptoPP::Integer Modulus = PublicKey.GetModulus();

	for (uint32 i = 0; i < ModulusSize; ++i)
	{
		OutModulus.Add(Modulus.GetByte(i));
	}
}

void RSAEncryptionHandlerComponent::SavePublicKeyExponent(TArray<uint8>& OutExponent)
{
	uint32 ExponentSize = static_cast<uint32>(PublicKey.GetPublicExponent().ByteCount());

	CryptoPP::Integer Exponent = PublicKey.GetPublicExponent();

	for (uint32 i = 0; i < ExponentSize; ++i)
	{
		OutExponent.Add(Exponent.GetByte(i));
	}
}

void RSAEncryptionHandlerComponent::EncryptWithRemotePublic(const TArray<uint8>& InPlainText, TArray<uint8>& OutCipherText)
{
	OutCipherText.Reset(); // Reset array
	OutCipherText.Reserve(RemotePublicKeyFixedCiphertextLength); // Reserve memory

	uint8* CipherTextBuffer = new uint8[RemotePublicKeyFixedCiphertextLength];

	RemotePublicEncryptor.Encrypt(Rng, InPlainText.GetData(), InPlainText.Num(), CipherTextBuffer);

	for (uint32 i = 0; i < RemotePublicKeyFixedCiphertextLength; ++i)
	{
		OutCipherText.Add(CipherTextBuffer[i]);
	}

	delete[] CipherTextBuffer;
}

void RSAEncryptionHandlerComponent::DecryptWithPrivate(const TArray<uint8>& InCipherText, TArray<uint8>& OutPlainText)
{
	uint8* PlainTextTextBuffer = new uint8[PrivateKeyMaxPlaintextLength];

	PrivateDecryptor.Decrypt(Rng, InCipherText.GetData(), InCipherText.Num(), PlainTextTextBuffer);

	for (uint32 i = 0; i < PrivateKeyMaxPlaintextLength; ++i)
	{
		OutPlainText.Add(PlainTextTextBuffer[i]);
	}

	delete[] PlainTextTextBuffer;
}

// MODULE INTERFACE
TSharedPtr<HandlerComponent> FRSAEncryptorHandlerComponentModuleInterface::CreateComponentInstance(FString& Options)
{
	return MakeShareable(new RSAEncryptionHandlerComponent);
}