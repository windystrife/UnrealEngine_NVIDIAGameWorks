// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DerivedDataPluginInterface.h"

class IAudioFormat;
class USoundWave;

class FDerivedAudioDataCompressor : public FDerivedDataPluginInterface
{
private:
	USoundWave*			SoundNode;
	FName				Format;
	const IAudioFormat*	Compressor;

public:

	FDerivedAudioDataCompressor(USoundWave* InSoundNode, FName InFormat);

	virtual const TCHAR* GetPluginName() const override
	{
		return TEXT("Audio");
	}

	virtual const TCHAR* GetVersionString() const override
	{
		// This is a version string that mimics the old versioning scheme. If you
		// want to bump this version, generate a new guid using VS->Tools->Create GUID and
		// return it here. Ex.
		// return TEXT("855EE5B3574C43ABACC6700C4ADC62E6");
		return TEXT("0005_0000");
	}

	virtual FString GetPluginSpecificCacheKeySuffix() const override;
	
	virtual bool IsBuildThreadsafe() const override
	{
		return false;
	}

	virtual bool Build(TArray<uint8>& OutData) override;
};
