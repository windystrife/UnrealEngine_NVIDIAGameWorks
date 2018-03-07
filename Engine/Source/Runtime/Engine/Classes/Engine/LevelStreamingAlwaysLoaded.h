// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/**
 * LevelStreamingAlwaysLoaded
 *
 * @documentation
 *
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/LevelStreaming.h"
#include "LevelStreamingAlwaysLoaded.generated.h"

UCLASS(MinimalAPI)
class ULevelStreamingAlwaysLoaded : public ULevelStreaming
{
	GENERATED_UCLASS_BODY()

	//~ Begin UObject Interface
	virtual void GetPrestreamPackages(TArray<UObject*>& OutPrestream) override;
	//~ End UObject Interface

	//~ Begin ULevelStreaming Interface
	virtual bool ShouldBeLoaded() const override;
	virtual bool ShouldBeAlwaysLoaded() const override { return true; } 
	//~ End ULevelStreaming Interface


};

