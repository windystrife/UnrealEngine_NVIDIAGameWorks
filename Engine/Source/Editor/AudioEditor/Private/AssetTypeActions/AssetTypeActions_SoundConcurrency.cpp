// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions/AssetTypeActions_SoundConcurrency.h"
#include "Sound/SoundConcurrency.h"

UClass* FAssetTypeActions_SoundConcurrency::GetSupportedClass() const
{
	return USoundConcurrency::StaticClass();
}
