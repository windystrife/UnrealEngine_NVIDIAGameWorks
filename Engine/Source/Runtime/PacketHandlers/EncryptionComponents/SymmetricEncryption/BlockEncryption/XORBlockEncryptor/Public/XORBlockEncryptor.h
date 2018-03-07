// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BlockEncryptionHandlerComponent.h"

/* XOR Block Encryptor Module Interface */
class FXORBlockEncryptorModuleInterface : public FBlockEncryptorModuleInterface
{
	virtual BlockEncryptor* CreateBlockEncryptorInstance() override;
};

/*
* XOR Block encryption
*/
class XORBLOCKENCRYPTOR_API XORBlockEncryptor : public BlockEncryptor
{
public:
	/* Initialized the encryptor */
	void Initialize(TArray<uint8>* Key) override;

	/* Encrypts outgoing packets */
	void EncryptBlock(uint8* Block) override;

	/* Decrypts incoming packets */
	void DecryptBlock(uint8* Block) override;

	/* Get the default key size for this encryptor */
	uint32 GetDefaultKeySize() { return 4; }
};
