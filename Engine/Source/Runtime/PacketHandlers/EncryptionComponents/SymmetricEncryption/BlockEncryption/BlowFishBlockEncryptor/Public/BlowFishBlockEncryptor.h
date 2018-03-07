// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BlockEncryptionHandlerComponent.h"
#include "CryptoPP/5.6.5/include/blowfish.h"

/* BlowFish Block Encryptor Module Interface */
class FBlowFishBlockEncryptorModuleInterface : public FBlockEncryptorModuleInterface
{
	virtual BlockEncryptor* CreateBlockEncryptorInstance() override;
};


/*
* BlowFish Block encryption
*/
class BLOWFISHBLOCKENCRYPTOR_API BlowFishBlockEncryptor : public BlockEncryptor
{
public:
	/* Initialized the encryptor */
	void Initialize(TArray<uint8>* Key) override;

	/* Encrypts outgoing packets */
	void EncryptBlock(uint8* Block) override;

	/* Decrypts incoming packets */
	void DecryptBlock(uint8* Block) override;

	/* Get the default key size for this encryptor */
	uint32 GetDefaultKeySize() { return 8; }

private:
	/* Encryptors for AES */
	CryptoPP::Blowfish::Encryption Encryptor;
	CryptoPP::Blowfish::Decryption Decryptor;
};