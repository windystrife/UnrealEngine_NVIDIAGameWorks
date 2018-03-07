// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

class UBlueprint;

/**
 * The public interface of BlutilityModule
 */
class IBlutilityModule : public IModuleInterface
{
public:

	/** Returns if the blueprint is blutility based */
	virtual bool IsBlutility( const UBlueprint* Blueprint ) const = 0;

};

