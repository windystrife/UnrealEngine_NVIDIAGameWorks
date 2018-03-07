// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions_FlexFluidSurface.h"

#include "PhysicsEngine/FlexFluidSurface.h"

UClass* FAssetTypeActions_FlexFluidSurface::GetSupportedClass() const
{
	return UFlexFluidSurface::StaticClass();
}