// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/**
 * LevelStreamingKismet
 *
 * Kismet triggerable streaming implementation.
 *
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/LevelStreaming.h"
#include "LevelStreamingKismet.generated.h"

UCLASS(MinimalAPI, BlueprintType)
class ULevelStreamingKismet : public ULevelStreaming
{
	GENERATED_UCLASS_BODY()

	/** Whether the level should be loaded at startup																			*/
	UPROPERTY(Category=LevelStreaming, EditAnywhere)
	uint32 bInitiallyLoaded:1;

	/** Whether the level should be visible at startup if it is loaded 															*/
	UPROPERTY(Category=LevelStreaming, EditAnywhere)
	uint32 bInitiallyVisible:1;
	
	/**  
 	* Stream in a level with a specific location and rotation. You can create multiple instances of the same level!
 	*
 	* The level to be loaded does not have to be in the persistent map's Levels list, however to ensure that the .umap does get
 	* packaged, please be sure to include the .umap in your Packaging Settings:
 	*
 	*   Project Settings -> Packaging -> List of Maps to Include in a Packaged Build (you may have to show advanced or type in filter)
 	* 
 	* @param LevelName - Level package name, ex: /Game/Maps/MyMapName, specifying short name like MyMapName will force very slow search on disk
 	* @param Location - World space location where the level should be spawned
 	* @param Rotation - World space rotation for rotating the entire level
	* @param bOutSuccess - Whether operation was successful (map was found and added to the sub-levels list)
 	* @return Streaming level object for a level instance
 	*/ 
 	UFUNCTION(BlueprintCallable, Category = LevelStreaming,meta=(WorldContext="WorldContextObject"))
 	static ENGINE_API ULevelStreamingKismet* LoadLevelInstance(UObject* WorldContextObject, const FString& LevelName, const FVector& Location, const FRotator& Rotation, bool& bOutSuccess);
 	
	//~ Begin UObject Interface
	virtual void PostLoad() override;
	//~ End UObject Interface

	//~ Begin ULevelStreaming Interface
	virtual bool ShouldBeLoaded() const override;
	//~ End ULevelStreaming Interface

private:
	// Counter used by LoadLevelInstance to create unique level names
	static int32 UniqueLevelInstanceId;
};

