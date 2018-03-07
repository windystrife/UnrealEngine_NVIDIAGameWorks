// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "XORBlockEncryptor.h"

#include "Modules/ModuleManager.h"

IMPLEMENT_MODULE(FXORBlockEncryptorModuleInterface, XORBlockEncryptor);

// MODULE INTERFACE
BlockEncryptor* FXORBlockEncryptorModuleInterface::CreateBlockEncryptorInstance()
{
	return new XORBlockEncryptor;
}

// XOR BLOCK
void XORBlockEncryptor::Initialize(TArray<uint8>* InKey)
{
	Key = InKey;

	if (Key->Num() != sizeof(int8) && Key->Num() != sizeof(int16) && Key->Num() != sizeof(int32) && Key->Num() != sizeof(int64))
	{
		LowLevelFatalError(TEXT("Incorrect key size. %i size chosen"), Key->Num());
	}

	FixedBlockSize = Key->Num();
}

void XORBlockEncryptor::EncryptBlock(uint8* Block)
{
	if (Key->Num() == sizeof(int8))
	{
		int8 Output = *reinterpret_cast<int8*>(Block) ^ *reinterpret_cast<int8*>(Key->GetData());
		memcpy(Block, &Output, sizeof(int8));
	} 
	else if (Key->Num() == sizeof(int16))
	{
		int16 Output = *reinterpret_cast<int16*>(Block) ^ *reinterpret_cast<int16*>(Key->GetData());
		memcpy(Block, &Output, sizeof(int16));
	} 
	else if (Key->Num() == sizeof(int32))
	{
		int32 Output = *reinterpret_cast<int32*>(Block) ^ *reinterpret_cast<int32*>(Key->GetData());
		memcpy(Block, &Output, sizeof(int32));
	} 
	else if (Key->Num() == sizeof(int64))
	{
		int64 Output = *reinterpret_cast<int64*>(Block) ^ *reinterpret_cast<int64*>(Key->GetData());
		memcpy(Block, &Output, sizeof(int64));
	}

	//UE_LOG(PacketHandlerLog, Log, TEXT("XOR Block Encrypted"));
}

void XORBlockEncryptor::DecryptBlock(uint8* Block)
{
	if (Key->Num() == sizeof(int8))
	{
		int8 Output = *reinterpret_cast<int8*>(Block) ^ *reinterpret_cast<int8*>(Key->GetData());
		memcpy(Block, &Output, sizeof(int8));
	} 
	else if (Key->Num() == sizeof(int16))
	{
		int16 Output = *reinterpret_cast<int16*>(Block) ^ *reinterpret_cast<int16*>(Key->GetData());
		memcpy(Block, &Output, sizeof(int16));
	} 
	else if (Key->Num() == sizeof(int32))
	{
		int32 Output = *reinterpret_cast<int32*>(Block) ^ *reinterpret_cast<int32*>(Key->GetData());
		memcpy(Block, &Output, sizeof(int32));
	} 
	else if (Key->Num() == sizeof(int64))
	{
		int64 Output = *reinterpret_cast<int64*>(Block) ^ *reinterpret_cast<int64*>(Key->GetData());
		memcpy(Block, &Output, sizeof(int64));
	}

	//UE_LOG(PacketHandlerLog, Log, TEXT("XOR Block Decrypted"));
}