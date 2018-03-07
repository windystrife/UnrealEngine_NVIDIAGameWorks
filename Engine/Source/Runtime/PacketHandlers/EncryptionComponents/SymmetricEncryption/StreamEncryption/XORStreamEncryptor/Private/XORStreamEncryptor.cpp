// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "XORStreamEncryptor.h"

#include "Modules/ModuleManager.h"

IMPLEMENT_MODULE(FXORStreamEncryptorModuleInterface, XORStreamEncryptor);

StreamEncryptor* FXORStreamEncryptorModuleInterface::CreateStreamEncryptorInstance()
{
	return new XORStreamEncryptor;
}

// XOR Stream
void XORStreamEncryptor::Initialize(TArray<uint8>* InKey)
{
	Key = InKey;
}

void XORStreamEncryptor::EncryptStream(uint8* Block, uint32 BytesCount)
{
	// XOR
	for (uint32 i = 0; i < BytesCount; ++i)
	{
		Block[i] ^= (*Key)[i % (Key->Num() - 1)];
	}

	//UE_LOG(PacketHandlerLog, Log, TEXT("XOR Stream Encrypted"));
}

void XORStreamEncryptor::DecryptStream(uint8* Block, uint32 BytesCount)
{
	// XOR
	for (uint32 i = 0; i < BytesCount; ++i)
	{
		Block[i] ^= (*Key)[i % (Key->Num() - 1)];
	}

	//UE_LOG(PacketHandlerLog, Log, TEXT("XOR Stream Decrypted"));
}
