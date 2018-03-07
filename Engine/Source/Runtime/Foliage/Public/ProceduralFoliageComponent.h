// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Misc/Guid.h"
#include "Components/ActorComponent.h"
#include "InstancedFoliage.h"
#include "ProceduralFoliageComponent.generated.h"

class AVolume;
class UProceduralFoliageSpawner;
struct FBodyInstance;

/** Describes the layout of the tiles used for procedural foliage simulation */
struct FTileLayout
{
	FTileLayout()
		: BottomLeftX(0), BottomLeftY(0), NumTilesX(0), NumTilesY(0), HalfHeight(0.f)
	{
	}

	// The X coordinate (in whole tiles) of the bottom-left-most active tile
	int32 BottomLeftX;

	// The Y coordinate (in whole tiles) of the bottom-left-most active tile
	int32 BottomLeftY;

	// The total number of active tiles along the x-axis
	int32 NumTilesX;

	// The total number of active tiles along the y-axis
	int32 NumTilesY;
	

	float HalfHeight;
};

UCLASS(BlueprintType)
class FOLIAGE_API UProceduralFoliageComponent : public UActorComponent
{
	GENERATED_UCLASS_BODY()

	/** The procedural foliage spawner used to generate foliage instances within this volume. */
	UPROPERTY(Category = "ProceduralFoliage", BlueprintReadWrite, EditAnywhere)
	UProceduralFoliageSpawner* FoliageSpawner;

	/** The amount of overlap between simulation tiles (in cm). */
	UPROPERTY(Category = "ProceduralFoliage", BlueprintReadWrite, EditAnywhere)
	float TileOverlap;

#if WITH_EDITORONLY_DATA

	/** Whether to place foliage on landscape */
	UPROPERTY(Category = "ProceduralFoliage", BlueprintReadWrite, EditAnywhere)
	bool bAllowLandscape;

	/** Whether to place foliage on BSP */
	UPROPERTY(Category = "ProceduralFoliage", BlueprintReadWrite, EditAnywhere)
	bool bAllowBSP;

	/** Whether to place foliage on StaticMesh */
	UPROPERTY(Category = "ProceduralFoliage", BlueprintReadWrite, EditAnywhere)
	bool bAllowStaticMesh;

	/** Whether to place foliage on translucent geometry */
	UPROPERTY(Category = "ProceduralFoliage", BlueprintReadWrite, EditAnywhere)
	bool bAllowTranslucent;

	/** Whether to place foliage on other blocking foliage geometry */
	UPROPERTY(Category = "ProceduralFoliage", BlueprintReadWrite, EditAnywhere)
	bool bAllowFoliage;

	/** Whether to visualize the tiles used for the foliage spawner simulation */
	UPROPERTY(Category = "ProceduralFoliage", BlueprintReadWrite, EditAnywhere)
	bool bShowDebugTiles;
#endif

	// UObject interface
	virtual void PostEditImport() override;

	/** 
	 * Runs the procedural foliage simulation to generate a list of desired instances to spawn.
	 * @return True if the simulation succeeded
	 */
	bool GenerateProceduralContent(TArray<FDesiredFoliageInstance>& OutFoliageInstances);
	
	/** Removes all spawned foliage instances in the level that were spawned by this component */
	void RemoveProceduralContent();

	/** @return True if any foliage instances in the level were spawned by this component */
	bool HasSpawnedAnyInstances();
	
	/** @return The position in world space of the bottom-left corner of the bottom-left-most active tile */
	FVector GetWorldPosition() const;

	/** @return The bounds of area encompassed by the simulation */
	FBox GetBounds() const;

	/** @return The body instanced used for bounds checking */
	FBodyInstance* GetBoundsBodyInstance() const;

	/** Determines the basic layout of the tiles used in the simulation */
	void GetTileLayout(FTileLayout& OutTileLayout) const;

	void SetSpawningVolume(AVolume* InSpawningVolume) { SpawningVolume = InSpawningVolume; }
	const FGuid& GetProceduralGuid() const { return ProceduralGuid; }

private:
	/** Does all the actual work of executing the procedural foliage simulation */
	bool ExecuteSimulation(TArray<FDesiredFoliageInstance>& OutFoliageInstances);

private:
	UPROPERTY()
	AVolume* SpawningVolume;
	
	UPROPERTY()
	FGuid ProceduralGuid;
};
