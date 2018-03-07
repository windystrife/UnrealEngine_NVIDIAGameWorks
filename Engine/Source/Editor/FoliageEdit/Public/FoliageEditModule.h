// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

extern const FName FoliageEditAppIdentifier;

class ULevel;

/**
 * Foliage Edit mode module interface
 */
class IFoliageEditModule : public IModuleInterface
{
public:

#if WITH_EDITOR
	/** Move the selected foliage to the specified level */
	virtual void MoveSelectedFoliageToLevel(ULevel* InTargetLevel) = 0;
	virtual bool CanMoveSelectedFoliageToLevel(ULevel* InTargetLevel) const = 0;
#endif
};
