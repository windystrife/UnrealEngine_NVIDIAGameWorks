// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "TwoFishBlockEncryptor.h"

IMPLEMENT_MODULE(FTwoFishBlockEncryptorModuleInterface, TwoFishBlockEncryptor);

// MODULE INTERFACE
BlockEncryptor* FTwoFishBlockEncryptorModuleInterface::CreateBlockEncryptorInstance()
{
	return new TwoFishBlockEncryptor;
}

// TWOFISH
void TwoFishBlockEncryptor::Initialize(TArray<uint8>* InKey)
{
	Key = InKey;

	if (Key->Num() != 16 && Key->Num() != 24 && Key->Num() != 32)
	{
		LowLevelFatalError(TEXT("Incorrect key size. %i size chosen"), Key->Num());
	}

	Encryptor = CryptoPP::Twofish::Encryption(Key->GetData(), Key->Num());
	Decryptor = CryptoPP::Twofish::Decryption(Key->GetData(), Key->Num());

	FixedBlockSize = 16;
}

void TwoFishBlockEncryptor::EncryptBlock(uint8* Block)
{
	uint8* Output = new uint8[FixedBlockSize];
	Encryptor.ProcessBlock(Block, Output);
	memcpy(Block, Output, FixedBlockSize);
	delete[] Output;

	//UE_LOG(PacketHandlerLog, Log, TEXT("TwoFish Encrypted"));
}

void TwoFishBlockEncryptor::DecryptBlock(uint8* Block)
{
	uint8* Output = new uint8[FixedBlockSize];
	Decryptor.ProcessBlock(Block, Output);
	memcpy(Block, Output, FixedBlockSize);
	delete[] Output;

	//UE_LOG(PacketHandlerLog, Log, TEXT("TwoFish Decrypted"));
}
