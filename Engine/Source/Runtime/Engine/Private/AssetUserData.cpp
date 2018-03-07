// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Engine/AssetUserData.h"
#include "Interfaces/Interface_AssetUserData.h"


UAssetUserData::UAssetUserData(const FObjectInitializer& ObjectInitializer)
	: UObject(ObjectInitializer)
{

}

//////////////////////////////////////////////////////////////////////////

UInterface_AssetUserData::UInterface_AssetUserData(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}
