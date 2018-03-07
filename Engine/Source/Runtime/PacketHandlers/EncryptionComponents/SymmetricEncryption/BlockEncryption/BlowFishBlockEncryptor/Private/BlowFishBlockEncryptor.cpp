// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "BlowFishBlockEncryptor.h"

IMPLEMENT_MODULE(FBlowFishBlockEncryptorModuleInterface, BlowFishBlockEncryptor);

// MODULE INTERFACE
BlockEncryptor* FBlowFishBlockEncryptorModuleInterface::CreateBlockEncryptorInstance()
{
	return new BlowFishBlockEncryptor;
}

// BLOWFISH
void BlowFishBlockEncryptor::Initialize(TArray<uint8>* InKey)
{
	Key = InKey;

	if (Key->Num() <= 4 || Key->Num() >= 54 || Key->Num() % 8 != 0)
	{
		LowLevelFatalError(TEXT("Incorrect key size. %i size chosen"), Key->Num());
	}

	Encryptor = CryptoPP::Blowfish::Encryption(Key->GetData(), Key->Num());
	Decryptor = CryptoPP::Blowfish::Decryption(Key->GetData(), Key->Num());

	FixedBlockSize = 8;
}

void BlowFishBlockEncryptor::EncryptBlock(uint8* Block)
{
	uint8* Output = new uint8[FixedBlockSize];
	Encryptor.ProcessBlock(Block, Output);
	memcpy(Block, Output, FixedBlockSize);
	delete[] Output;

	//UE_LOG(PacketHandlerLog, Log, TEXT("BlowFish Encrypted"));
}

void BlowFishBlockEncryptor::DecryptBlock(uint8* Block)
{
	uint8* Output = new uint8[FixedBlockSize];
	Decryptor.ProcessBlock(Block, Output);
	memcpy(Block, Output, FixedBlockSize);
	delete[] Output;

	//UE_LOG(PacketHandlerLog, Log, TEXT("BlowFish Decrypted"));
}