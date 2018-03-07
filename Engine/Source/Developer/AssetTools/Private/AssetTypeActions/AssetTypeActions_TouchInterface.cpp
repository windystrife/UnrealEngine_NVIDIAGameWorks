// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions/AssetTypeActions_TouchInterface.h"
#include "GameFramework/TouchInterface.h"

UClass* FAssetTypeActions_TouchInterface::GetSupportedClass() const
{
	return UTouchInterface::StaticClass();
}
