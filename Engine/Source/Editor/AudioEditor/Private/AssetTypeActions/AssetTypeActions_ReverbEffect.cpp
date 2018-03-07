// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions/AssetTypeActions_ReverbEffect.h"
#include "Sound/ReverbEffect.h"

UClass* FAssetTypeActions_ReverbEffect::GetSupportedClass() const
{
	return UReverbEffect::StaticClass();
}
