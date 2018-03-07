// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "SceneOutlinerSettings.generated.h"


/**
 * Implements the settings for the Scene Outliner.
 */
UCLASS(config=EditorPerProjectUserSettings)
class USceneOutlinerSettings
	: public UObject
{
	GENERATED_UCLASS_BODY()

	/** True when the Scene Outliner is hiding temporary/run-time Actors */
	UPROPERTY(config)
	uint32 bHideTemporaryActors:1;

	/** True when the Scene Outliner is showing only Actors that exist in the current level */
	UPROPERTY(config)
	uint32 bShowOnlyActorsInCurrentLevel:1;

	/** True when the Scene Outliner is only displaying selected Actors */
	UPROPERTY(config)
	uint32 bShowOnlySelectedActors:1;
};
