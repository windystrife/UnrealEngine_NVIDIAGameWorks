// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions_FlexContainer.h"

#include "PhysicsEngine/FlexContainer.h"

UClass* FAssetTypeActions_FlexContainer::GetSupportedClass() const
{
	return UFlexContainer::StaticClass();
}