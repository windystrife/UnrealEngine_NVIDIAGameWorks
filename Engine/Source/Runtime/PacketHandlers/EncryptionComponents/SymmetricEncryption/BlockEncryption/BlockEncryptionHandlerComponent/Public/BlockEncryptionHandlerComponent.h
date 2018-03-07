// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PacketHandler.h"

#include "CryptoPP/5.6.5/include/osrng.h"

/*
* Abstract class for block encryptors
*/
class BLOCKENCRYPTIONHANDLERCOMPONENT_API BlockEncryptor
{
public:
	/* Initialized the encryptor */
	virtual void Initialize(TArray<byte>* Key) PURE_VIRTUAL(BlockEncryptor::Initialize, );

	/* Encrypts block */
	virtual void EncryptBlock(byte* Block) PURE_VIRTUAL(BlockEncryptor::EncryptBlock, );

	/* Decrypts incoming packets */
	virtual void DecryptBlock(byte* Block) PURE_VIRTUAL(BlockEncryptor::DecryptBlock, );

	/* Gets the default key size for this encryptor */
	virtual uint32 GetDefaultKeySize() PURE_VIRTUAL(BlockEncryptor::GetDefaultKeySize, return 0;);

	/* Gets the fixed size of the block */
	uint32 GetFixedBlockSize();

protected:
	/* Fixed size of block */
	uint32 FixedBlockSize;

	/* Key, this is initialized when Initialize() is called */
	TArray<byte>* Key;
};

/* Block Encryptor Module Interface */
class FBlockEncryptorModuleInterface : public IModuleInterface
{
public:
	virtual BlockEncryptor* CreateBlockEncryptorInstance() = 0;
};

/*
* Symmetric block cipher handler component using AES as the cipher
*/
class BLOCKENCRYPTIONHANDLERCOMPONENT_API BlockEncryptionHandlerComponent : public HandlerComponent
{
public:
	/* Initializes default data, can provide the key size in bytes */
	BlockEncryptionHandlerComponent(BlockEncryptor* Encryptor = nullptr, uint32 KeySizeInBytes = 0);

	/* Initializes the handler component */
	virtual void Initialize() override;

	/* Whether the handler component is valid */
	virtual bool IsValid() const override;

	/* Handles any incoming packets */
	virtual void Incoming(FBitReader& Packet) override;

	/* Handles any outgoing packets */
	virtual void Outgoing(FBitWriter& Packet) override;

protected:
	void EncryptBlock(FBitWriter& Packet);
	void DecryptBlock(FBitReader& Packet);

	/* The encryptor used */
	BlockEncryptor* Encryptor;

	/** Size of Key in Bits */
	uint32 KeySizeInBytes;

	// Key used for symmetric encryption
	TArray<uint8> Key;
};

/* Block Encryption Handler Component Module Interface */
class FBlockEncryptionHandlerComponentModuleInterface : public FPacketHandlerComponentModuleInterface
{
public:
	/* Creates an instance of this component */
	virtual TSharedPtr<HandlerComponent> CreateComponentInstance(FString& Options) override;
};
