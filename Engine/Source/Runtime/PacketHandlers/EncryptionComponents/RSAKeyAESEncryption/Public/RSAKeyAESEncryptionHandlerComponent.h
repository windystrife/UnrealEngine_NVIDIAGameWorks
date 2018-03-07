// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PacketHandler.h"


// Crypto++ uses exceptions, which triggers this warning
#pragma warning(disable : 4530)

THIRD_PARTY_INCLUDES_START
#include "CryptoPP/5.6.5/include/rsa.h"
#include "CryptoPP/5.6.5/include/osrng.h"
#include "CryptoPP/5.6.5/include/modes.h"
THIRD_PARTY_INCLUDES_END



/**
 * Module Interface
 */
class FRSAKeyAESEncryptionModuleInterface : public FPacketHandlerComponentModuleInterface
{
public:
	virtual TSharedPtr<HandlerComponent> CreateComponentInstance(FString& Options) override;
};


namespace ERSAKeyAESEncryptionHandler
{
	enum State
	{
		UnInitialized,
		SentKey,
		Initialized
	};
}

/**
 * Exchanges a key using RSA as the handshake, and implements symmetric encryption with that key using AES
 */
class RSAKEYAESENCRYPTION_API RSAKeyAESEncryptionHandlerComponent : public HandlerComponent
{
public:
	/**
	 * Initializes default data, can specify key size in bits,
	 * increasing the key size will increase the fixed cipher text and max plain text sizes
	 *
	 * @param InAsymmetricKeySize	The size to use for the asymmetric/RSA key
	 * @param InSessionKeySize	The size to use for the symmetric session key
	 */
	RSAKeyAESEncryptionHandlerComponent(uint32 InAsymmetricKeySize=2048, uint32 InSessionKeySize=256);

	virtual void Initialize() override;

	virtual void NotifyHandshakeBegin() override;

	virtual bool IsValid() const override;

	virtual void Incoming(FBitReader& Packet) override;

	virtual void Outgoing(FBitWriter& Packet) override;

	virtual int32 GetReservedPacketBits() override;

protected:
	/**
	 * Processes an incoming packet during the handshake stage.
	 */
	void IncomingHandshake(FBitReader& Packet);

	/* Set the state of the handler */
	void SetState(ERSAKeyAESEncryptionHandler::State State);

	/* Asymmetrically encrypt outgoing packets */
	void AsymmetricEncryptPacket(FBitWriter& Packet);

	/* Asymmetrically decrypt incoming packets */
	void AsymmetricDecryptPacket(FBitReader& Packet);

	/* Pack the asymmetric key into a packet */
	void PackAsymmetricKey(FBitWriter& Packet);

	/* Unpack the asymmetric key from a packet */
	void UnPackAsymmetricKey(FBitReader& Packet);


	/* Save the public key's modulus in the provided array */
	void SavePublicKeyModulus(TArray<uint8>& OutModulus);

	/* Save the public key's exponent in the provided array */
	void SavePublicKeyExponent(TArray<uint8>& OutExponent);


protected:
	/** Maximum plain text length that can be encrypted with private asymmetric key */
	uint32 AsymmetricKeyMaxPlaintextLength;

	/** Fixed cipher text length that will result of private asymmetric key encryption */
	uint32 AsymmetricKeyFixedCiphertextLength;


	/** Size of the asymmetric key in bits */
	uint32 AsymmetricKeySize;

	/** Size of the symmetric session key in bits */
	uint32 SessionKeySize;

	/** The session key */
	TArray<uint8> SessionKey;


	/* State of the handler */
	ERSAKeyAESEncryptionHandler::State State;

	/* Random number generator used to generate asymmetric key */
	CryptoPP::AutoSeededRandomPool AsymmetricRng;

	/* RSA parameters for generating key */
	CryptoPP::InvertibleRSAFunction Params;


	/* Encryptor for encrypting with local private or remote public key */
	CryptoPP::RSAES_OAEP_SHA_Encryptor AsymmetricEncrypt;

	/* Decryptor for decrypting with local private or remote public key */
	CryptoPP::RSAES_OAEP_SHA_Decryptor AsymmetricDecrypt;

	/* Local Public asymmetric Key */
	CryptoPP::RSA::PublicKey PublicKey;

	/* Local Private asymmetric Key */
	CryptoPP::RSA::PrivateKey PrivateKey;


	/** Encryptor for encrypting with the symmetric session key */
	CryptoPP::CBC_CTS_Mode<CryptoPP::AES>::Encryption SymmetricEncrypt;

	/** Decryptor for encrypting with the symmetric session key */
	CryptoPP::CBC_CTS_Mode<CryptoPP::AES>::Decryption SymmetricDecrypt;
};
