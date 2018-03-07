// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PacketHandler.h"

class STREAMENCRYPTIONHANDLERCOMPONENT_API StreamEncryptor
{
public:
	/* Initialized the encryptor */
	virtual void Initialize(TArray<uint8>* Key) PURE_VIRTUAL(StreamEncryptor::Initialize, );

	/* Encrypts block */
	virtual void EncryptStream(uint8* Stream, uint32 BytesCount) PURE_VIRTUAL(StreamEncryptor::EncryptBlock, );

	/* Decrypts incoming packets */
	virtual void DecryptStream(uint8* Stream, uint32 BytesCount) PURE_VIRTUAL(StreamEncryptor::DecryptBlock, );

	/* Gets the default key size for this encryptor */
	virtual uint32 GetDefaultKeySize() PURE_VIRTUAL(StreamEncryptor::GetDefaultKeySize, return 0;);

protected:
	/* Key, this is initialized when Initialize() is called */
	TArray<uint8>* Key;
};

/* Stream Encryptor Module Interface */
class FStreamEncryptorModuleInterface : public IModuleInterface
{
public:
	virtual StreamEncryptor* CreateStreamEncryptorInstance() = 0;
};

/*
* Symmetric block cipher handler component using AES as the cipher
*/
class STREAMENCRYPTIONHANDLERCOMPONENT_API StreamEncryptionHandlerComponent : public HandlerComponent
{
public:
	/* Initializes default data, can provide the key size in bytes */
	StreamEncryptionHandlerComponent(StreamEncryptor* Encryptor = nullptr, uint32 KeySizeInBytes = 0);

	/* Initializes the handler component */
	virtual void Initialize() override;

	/* Whether the handler component is valid */
	virtual bool IsValid() const override;

	/* Handles any incoming packets */
	virtual void Incoming(FBitReader& Packet) override;

	/* Handles any outgoing packets */
	virtual void Outgoing(FBitWriter& Packet) override;

protected:
	void EncryptStream(FBitWriter& Packet);
	void DecryptStream(FBitReader& Packet);

	/* The encryptor used */
	StreamEncryptor* Encryptor;

	/** Size of Key in Bits */
	uint32 KeySizeInBytes;

	// Key used for symmetric encryption
	TArray<uint8> Key;
};

/* Stream Encryption Handler Component Module Interface */
class FStreamEncryptionHandlerComponentModuleInterface : public FPacketHandlerComponentModuleInterface
{
public:
	/* Creates an instance of this component */
	virtual TSharedPtr<HandlerComponent> CreateComponentInstance(FString& Options) override;
};
