// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "RSAKeyAESEncryptionHandlerComponent.h"

#include "PacketAudit.h"
#include "PacketHandler.h"

#include "Modules/ModuleManager.h"
#include "UObject/CoreNet.h"


IMPLEMENT_MODULE(FRSAKeyAESEncryptionModuleInterface, RSAKeyAESEncryption);


// @todo #JohnB: IMPORTANT: See if you can replace CryptoPP with OpenSSL API's instead, as that makes it easier for platform teams


// @todo #JohnB: Add/test support for CanReadUnaligned.

// @todo #JohnB: The entire encryption implementation should be very carefully audited, and something like DTLS reviewed,
//					and a new document written up on how encryption would be implemented from scratch
//					(starting from the stateless handshake code), and then compared against this

// @todo #JohnB: Add supporting for increasing 'AsymmetricKeySize' to 4096 - currently, this triggers a limit with MaxOutgoingBits,
//					and thus is limited to 2048, until the code is expanded to support values greater than this.


// Defines

/** Puts a limit on the maximum size of the exponent. */
// @todo: This should be limited further, as a high exponent could potentially be used for performing a DoS attack,
//			by making it very costly to decrypt packets. The limit is only this high temporarily.
#define MAX_EXPONENT_BITS 1024

#define AES_BLOCK_SIZE 16

/** The maximum size for a packet being compressed */
// @todo #JohnB: This actually overshoots the maximum, and should probably be refined further
#define MAX_COMPRESSED_PACKET_SIZE (MAX_PACKET_SIZE * 8)


/**
 * Handshake sequence:
 *	Server												Client
 *
 *	AsymmetricKey = Rand()
 *
 *	[AsymmetricKey]					->
 *
 *														SessionKey = Rand()
 *														EncryptedSessionKey = Encrypt(SessionKey, AsymmetricKey)
 *
 *									<-					[EncryptedSessionKey]
 *
 *														*Handshake Complete*
 *
 *	SessionKey=Decrypt(SessionKey, AsymmetricKey)
 *
 *	*Handshake Complete*
 */


/**
 * RSAKeyAESEncryptionHandlerComponent
 */
RSAKeyAESEncryptionHandlerComponent::RSAKeyAESEncryptionHandlerComponent(uint32 InAsymmetricKeySize, uint32 InSessionKeySize)
	: AsymmetricKeyMaxPlaintextLength(0)
	, AsymmetricKeyFixedCiphertextLength(0)
	, AsymmetricKeySize(InAsymmetricKeySize)
	, SessionKeySize(InSessionKeySize)
	, State(ERSAKeyAESEncryptionHandler::State::UnInitialized)
	, AsymmetricRng()
	, Params()
	, AsymmetricEncrypt()
	, AsymmetricDecrypt()
	, PublicKey()
	, PrivateKey()
	, SymmetricEncrypt()
	, SymmetricDecrypt()
{
	SetActive(true);

	bRequiresHandshake = true;
	bRequiresReliability = true;
}

void RSAKeyAESEncryptionHandlerComponent::Initialize()
{
	Params.GenerateRandomWithKeySize(AsymmetricRng, AsymmetricKeySize);

	PublicKey = CryptoPP::RSA::PublicKey(Params);
	PrivateKey = CryptoPP::RSA::PrivateKey(Params);
	AsymmetricEncrypt = CryptoPP::RSAES_OAEP_SHA_Encryptor(PrivateKey);
	AsymmetricDecrypt = CryptoPP::RSAES_OAEP_SHA_Decryptor(PrivateKey);
	AsymmetricKeyMaxPlaintextLength = static_cast<uint32>(AsymmetricEncrypt.FixedMaxPlaintextLength());
	AsymmetricKeyFixedCiphertextLength = static_cast<uint32>(AsymmetricEncrypt.FixedCiphertextLength());
}

void RSAKeyAESEncryptionHandlerComponent::NotifyHandshakeBegin()
{
	// The server sends the asymmetric key
	if (Handler->Mode == Handler::Mode::Server)
	{
		FBitWriter OutPacket;

		PackAsymmetricKey(OutPacket);
		SetState(ERSAKeyAESEncryptionHandler::State::SentKey);

		FPacketAudit::AddStage(TEXT("RSAHandshake"), OutPacket);

		Handler->SendHandlerPacket(this, OutPacket);
	}
}

bool RSAKeyAESEncryptionHandlerComponent::IsValid() const
{
	return AsymmetricKeyMaxPlaintextLength > 0;
}

void RSAKeyAESEncryptionHandlerComponent::SetState(ERSAKeyAESEncryptionHandler::State InState)
{
	State = InState;
}

void RSAKeyAESEncryptionHandlerComponent::Outgoing(FBitWriter& Packet)
{
	if (State == ERSAKeyAESEncryptionHandler::State::Initialized)
	{
		int32 PacketNumBytes = Packet.GetNumBytes();

		if (PacketNumBytes > 0)
		{
			// Pad along 16 byte boundary, in order to encrypt properly
			// @todo: Review the 16 byte boundary requirement
			uint32 NumberOfBitsInPlaintext = Packet.GetNumBits();
			int32 PaddedSize = (PacketNumBytes + 15) / 16 * 16;
			TArray<uint8> PaddedPlainText;
			TArray<uint8> CipherText;

			PaddedPlainText.AddUninitialized(PaddedSize);
			CipherText.AddUninitialized(PaddedSize);

			FMemory::Memcpy(PaddedPlainText.GetData(), Packet.GetData(), PacketNumBytes);

			if (PaddedSize > PacketNumBytes)
			{
				FMemory::Memzero(PaddedPlainText.GetData() + PacketNumBytes, (PaddedSize - PacketNumBytes));
			}

			SymmetricEncrypt.ProcessData(CipherText.GetData(), PaddedPlainText.GetData(), PaddedSize);


			// @todo: Optimize this.
			Packet.Reset();

			NumberOfBitsInPlaintext--;
			Packet.SerializeInt(NumberOfBitsInPlaintext, MAX_COMPRESSED_PACKET_SIZE);


			Packet.Serialize(CipherText.GetData(), PaddedSize);
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

void RSAKeyAESEncryptionHandlerComponent::Incoming(FBitReader& Packet)
{
	if (State == ERSAKeyAESEncryptionHandler::State::Initialized)
	{
		uint32 NumberOfBitsInPlaintext = 0;

		Packet.SerializeInt(NumberOfBitsInPlaintext, MAX_COMPRESSED_PACKET_SIZE);
		NumberOfBitsInPlaintext++;

		if (!Packet.IsError())
		{
			if (NumberOfBitsInPlaintext <= MAX_COMPRESSED_PACKET_SIZE)
			{
				int32 NumberOfBytesInPlaintext = (NumberOfBitsInPlaintext + 7) >> 3;
				int32 PaddedSize = (NumberOfBytesInPlaintext + 15) / 16 * 16;
				TArray<uint8> CipherText;
				TArray<uint8> PlainText;

				CipherText.AddUninitialized(PaddedSize);
				PlainText.AddUninitialized(PaddedSize);

				Packet.Serialize(CipherText.GetData(), PaddedSize);

				if (!Packet.IsError())
				{
					SymmetricDecrypt.ProcessData(PlainText.GetData(), CipherText.GetData(), PaddedSize);


					// @todo: Could do with optimization
					FBitReader Copy(PlainText.GetData(), NumberOfBitsInPlaintext);

					Packet = Copy;
				}
				else
				{
					UE_LOG(PacketHandlerLog, Error, TEXT("AES: Error serializing PlainText."));

#if !UE_BUILD_SHIPPING
					check(false);
#endif
				}
			}
			else
			{
				Packet.SetError();

				UE_LOG(PacketHandlerLog, Error, TEXT("AES: Specified PlainText size exceeds MAX_COMPRESSED_PACKET_SIZE."));

#if !UE_BUILD_SHIPPING
				check(false);
#endif
			}
		}
		else
		{
			UE_LOG(PacketHandlerLog, Error, TEXT("AES: Error serializing incoming packet."));

#if !UE_BUILD_SHIPPING
			check(false);
#endif
		}
	}
	else
	{
		IncomingHandshake(Packet);
	}
}

void RSAKeyAESEncryptionHandlerComponent::IncomingHandshake(FBitReader& Packet)
{
	bool bHandledPacket = true;

	if (Handler->Mode == Handler::Mode::Server)
	{
		if (State == ERSAKeyAESEncryptionHandler::State::SentKey)
		{
			FPacketAudit::CheckStage(TEXT("SessionKeyExchangeEncrypt"), Packet);

			AsymmetricDecryptPacket(Packet);

			FPacketAudit::CheckStage(TEXT("SessionKeyExchangeDecrypt"), Packet);


			uint32 KeySizeBytes = SessionKeySize / 8;
			TArray<uint8> InitializationVector;

			InitializationVector.SetNumUninitialized(AES_BLOCK_SIZE);
			SessionKey.SetNumUninitialized(KeySizeBytes);

			Packet.Serialize(InitializationVector.GetData(), AES_BLOCK_SIZE);
			Packet.Serialize(SessionKey.GetData(), KeySizeBytes);

			if (!Packet.IsError())
			{
				SymmetricEncrypt.SetKeyWithIV(SessionKey.GetData(), KeySizeBytes, InitializationVector.GetData(), AES_BLOCK_SIZE);
				SymmetricDecrypt.SetKeyWithIV(SessionKey.GetData(), KeySizeBytes, InitializationVector.GetData(), AES_BLOCK_SIZE);

				// Now mark as initialized
				SetState(ERSAKeyAESEncryptionHandler::State::Initialized);
				Initialized();
			}
			else
			{
				UE_LOG(PacketHandlerLog, Error, TEXT("RSA: Failed to initialize symmetric encryption."));

#if !UE_BUILD_SHIPPING
				check(false);
#endif
			}
		}
		else
		{
			bHandledPacket = false;
		}
	}
	else if (Handler->Mode == Handler::Mode::Client)
	{
		if (State == ERSAKeyAESEncryptionHandler::State::UnInitialized)
		{
			FPacketAudit::CheckStage(TEXT("RSAHandshake"), Packet);

			UnPackAsymmetricKey(Packet);

			// Generate/send the session key and initialization vector, and setup the symmetric encryption locally
			if (!Packet.IsError())
			{
				check(AES_BLOCK_SIZE == CryptoPP::AES::BLOCKSIZE);
				check((SessionKeySize % 8) == 0);

				CryptoPP::AutoSeededRandomPool SessionRng;
				TArray<uint8> InitializationVector;
				uint32 KeySizeBytes = SessionKeySize / 8;

				SessionKey.SetNumUninitialized(KeySizeBytes);
				InitializationVector.SetNumUninitialized(AES_BLOCK_SIZE);

				SessionRng.GenerateBlock(SessionKey.GetData(), KeySizeBytes);
				SessionRng.GenerateBlock(InitializationVector.GetData(), AES_BLOCK_SIZE);

				SymmetricEncrypt.SetKeyWithIV(SessionKey.GetData(), KeySizeBytes, InitializationVector.GetData(), AES_BLOCK_SIZE);
				SymmetricDecrypt.SetKeyWithIV(SessionKey.GetData(), KeySizeBytes, InitializationVector.GetData(), AES_BLOCK_SIZE);


				// Now send the initialization vector and session key
				FBitWriter OutPacket((AES_BLOCK_SIZE + KeySizeBytes) * 8, true);

				OutPacket.Serialize(InitializationVector.GetData(), AES_BLOCK_SIZE);
				OutPacket.Serialize(SessionKey.GetData(), KeySizeBytes);


				FPacketAudit::AddStage(TEXT("SessionKeyExchangeDecrypt"), OutPacket);

				AsymmetricEncryptPacket(OutPacket);

				FPacketAudit::AddStage(TEXT("SessionKeyExchangeEncrypt"), OutPacket);

				Handler->SendHandlerPacket(this, OutPacket);


				// Now mark as initialized
				SetState(ERSAKeyAESEncryptionHandler::State::Initialized);
				Initialized();
			}
			else
			{
				UE_LOG(PacketHandlerLog, Error, TEXT("RSA: Error unpacking the asymmetric key, can't complete handshake."));

#if !UE_BUILD_SHIPPING
				check(false);
#endif
			}
		}
		else
		{
			bHandledPacket = false;
		}
	}


	if (!bHandledPacket)
	{
		UE_LOG(PacketHandlerLog, Warning, TEXT("RSAEncryptionHandlerComponent: Got incoming packet when not yet initialized."));

#if !UE_BUILD_SHIPPING
		check(false);
#endif

		Packet.SetError();
	}
}

int32 RSAKeyAESEncryptionHandlerComponent::GetReservedPacketBits()
{
	int32 ReturnVal = 0;

	// Count the size of the value, representing the packet size in bits
	FBitWriter MeasureAr(0, true);
	uint32 TestVal = MAX_COMPRESSED_PACKET_SIZE - 1;

	MeasureAr.SerializeInt(TestVal, MAX_COMPRESSED_PACKET_SIZE);

	check(!MeasureAr.IsError());

	ReturnVal += MeasureAr.GetNumBits();


	// Count the worst case amount of padding that may be added to a packet
	ReturnVal += (16 * 8) - 1;

	return ReturnVal;
}

void RSAKeyAESEncryptionHandlerComponent::PackAsymmetricKey(FBitWriter& Packet)
{
	FBitWriter Local;
	Local.AllowAppend(true);
	Local.SetAllowResize(true);

	// Save remote public key information
	TArray<uint8> ModulusArray;
	TArray<uint8> ExponentArray;

	SavePublicKeyModulus(ModulusArray);
	SavePublicKeyExponent(ExponentArray);

	if ((ModulusArray.Num() * 8u) > AsymmetricKeySize || ModulusArray.Num() == 0)
	{
		LowLevelFatalError(TEXT("Modulus size '%i bits' must be greater than zero, and must not exceed key size '%i'"),
							(ModulusArray.Num() * 8), AsymmetricKeySize);
	}

	if ((ExponentArray.Num() * 8) > MAX_EXPONENT_BITS || ExponentArray.Num() == 0)
	{
		LowLevelFatalError(TEXT("Exponent size '%i bits' must be greater than zero, and must not exceed MAX_EXPONENT_BITS"),
							(ExponentArray.Num() * 8));
	}


	const uint32 MaxModulusNum = (AsymmetricKeySize + 7) >> 3;
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

void RSAKeyAESEncryptionHandlerComponent::UnPackAsymmetricKey(FBitReader& Packet)
{
	// Save remote public key
	TArray<uint8> ModulusArray;
	TArray<uint8> ExponentArray;
	const uint32 MaxModulusNum = (AsymmetricKeySize + 7) >> 3;
	const uint32 MaxExponentNum = (MAX_EXPONENT_BITS + 7) >> 3;

	uint32 ModulusNum = 0;

	Packet.SerializeInt(ModulusNum, MaxModulusNum);

	ModulusNum++;

	if ((ModulusNum * 8) > AsymmetricKeySize)
	{
		UE_LOG(PacketHandlerLog, Warning, TEXT("RSA: Modulus size '%i bits' should not exceed key size '%i'"),
							(ModulusNum * 8), AsymmetricKeySize);

		Packet.SetError();
	}

	if (!Packet.IsError())
	{
		uint32 ExponentNum = 0;

		ModulusArray.SetNumUninitialized(ModulusNum);

		Packet.Serialize(ModulusArray.GetData(), ModulusNum);
		Packet.SerializeInt(ExponentNum, MaxExponentNum);

		if (!Packet.IsError())
		{
			ExponentNum++;

			if ((ExponentNum * 8) <= MAX_EXPONENT_BITS)
			{
				ExponentArray.SetNumUninitialized(ExponentNum);
				Packet.Serialize(ExponentArray.GetData(), ExponentNum);
			}
			else
			{
				UE_LOG(PacketHandlerLog, Warning, TEXT("RSA: Exponent size '%i bits' should not exceed MAX_EXPONENT_BITS"),
									(ExponentNum * 8));

				Packet.SetError();
			}
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

		PublicKey.SetModulus(Modulus);
		PublicKey.SetPublicExponent(Exponent);

		AsymmetricEncrypt = CryptoPP::RSAES_OAEP_SHA_Encryptor(PublicKey);
		AsymmetricKeyMaxPlaintextLength = static_cast<uint32>(AsymmetricEncrypt.FixedMaxPlaintextLength());
		AsymmetricKeyFixedCiphertextLength = static_cast<uint32>(AsymmetricEncrypt.FixedCiphertextLength());
	}
}

void RSAKeyAESEncryptionHandlerComponent::AsymmetricEncryptPacket(FBitWriter& Packet)
{
	// Serialize size of plain text data
	uint32 NumberOfBytesInPlaintext = static_cast<uint32>(Packet.GetNumBytes());
	TArray<uint8> PlainText;

	PlainText.AddUninitialized(NumberOfBytesInPlaintext);

	FMemory::Memcpy(PlainText.GetData(), Packet.GetData(), NumberOfBytesInPlaintext);

	Packet.Reset();

	if (NumberOfBytesInPlaintext == 0 || NumberOfBytesInPlaintext > AsymmetricKeyMaxPlaintextLength)
	{
		UE_LOG(PacketHandlerLog, Warning, TEXT("RSA: Encryption failed due to invalid plain text size '%i/%i'."),
				NumberOfBytesInPlaintext, AsymmetricKeyMaxPlaintextLength);

		Packet.SetError();

#if !UE_BUILD_SHIPPING
		check(false);
#endif

		return;
	}


	TArray<uint8> CipherText;

	CipherText.SetNumUninitialized(AsymmetricKeyFixedCiphertextLength);
	AsymmetricEncrypt.Encrypt(AsymmetricRng, PlainText.GetData(), PlainText.Num(), CipherText.GetData());

	NumberOfBytesInPlaintext--;
	Packet.SerializeInt(NumberOfBytesInPlaintext, AsymmetricKeyMaxPlaintextLength);

	Packet.Serialize(CipherText.GetData(), CipherText.Num());
}

void RSAKeyAESEncryptionHandlerComponent::AsymmetricDecryptPacket(FBitReader& Packet)
{
	// Serialize size of plain text data
	uint32 NumberOfBytesInPlaintext;

	Packet.SerializeInt(NumberOfBytesInPlaintext, AsymmetricKeyMaxPlaintextLength);
	NumberOfBytesInPlaintext++;

	if (NumberOfBytesInPlaintext == 0 || NumberOfBytesInPlaintext > AsymmetricKeyMaxPlaintextLength)
	{
		UE_LOG(PacketHandlerLog, Warning, TEXT("RSA: Decryption failed due to invalid cipher text size '%i/%i'."),
				NumberOfBytesInPlaintext, AsymmetricKeyMaxPlaintextLength);

		Packet.SetError();

#if !UE_BUILD_SHIPPING
		check(false);
#endif
	}


	// Copy byte stream to PlainText from Packet; not including the NumOfBytesPT field
	if (!Packet.IsError())
	{
		// @todo #JohnB: This is not bit-safe (but should not matter, since this code is only used during handshake, but still...)
		TArray<uint8> PlainText;
		int32 StartPos = Packet.GetPosBits() / 8;

		PlainText.SetNumUninitialized(AsymmetricKeyMaxPlaintextLength);
		AsymmetricDecrypt.Decrypt(AsymmetricRng, (Packet.GetData() + StartPos), (Packet.GetNumBytes() - StartPos), PlainText.GetData());


		// @todo #JohnB: Optimize this
		FBitReader Copy(PlainText.GetData(), NumberOfBytesInPlaintext * 8);
		Packet = Copy;
	}
}

void RSAKeyAESEncryptionHandlerComponent::SavePublicKeyModulus(TArray<uint8>& OutModulus)
{
	uint32 ModulusSize = static_cast<uint32>(PublicKey.GetModulus().ByteCount());

	CryptoPP::Integer Modulus = PublicKey.GetModulus();

	for (uint32 i = 0; i < ModulusSize; ++i)
	{
		OutModulus.Add(Modulus.GetByte(i));
	}
}

void RSAKeyAESEncryptionHandlerComponent::SavePublicKeyExponent(TArray<uint8>& OutExponent)
{
	uint32 ExponentSize = static_cast<uint32>(PublicKey.GetPublicExponent().ByteCount());

	CryptoPP::Integer Exponent = PublicKey.GetPublicExponent();

	for (uint32 i = 0; i < ExponentSize; ++i)
	{
		OutExponent.Add(Exponent.GetByte(i));
	}
}


// MODULE INTERFACE
TSharedPtr<HandlerComponent> FRSAKeyAESEncryptionModuleInterface::CreateComponentInstance(FString& Options)
{
	return MakeShareable(new RSAKeyAESEncryptionHandlerComponent());
}
