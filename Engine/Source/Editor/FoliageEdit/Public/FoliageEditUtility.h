// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/**
* Utility used for Foliage Edition
*/

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"

class UFoliageType;
struct FFoliageMeshUIInfo;
class ULevel;

class FFoliageEditUtility
{
public:
	/** Save the foliage type object. If it isn't an asset, will prompt the user for a location to save the new asset. */
	static FOLIAGEEDIT_API UFoliageType* SaveFoliageTypeObject(UFoliageType* Settings);
	static FOLIAGEEDIT_API void ReplaceFoliageTypeObject(UWorld* InWorld, UFoliageType* OldType, UFoliageType* NewType);

	static FOLIAGEEDIT_API void MoveActorFoliageInstancesToLevel(ULevel* InTargetLevel);
};