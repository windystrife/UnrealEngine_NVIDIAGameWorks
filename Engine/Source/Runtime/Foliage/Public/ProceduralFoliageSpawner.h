// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/ThreadSafeCounter.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Math/RandomStream.h"
#include "ProceduralFoliageInstance.h"
#include "FoliageTypeObject.h"
#include "ProceduralFoliageSpawner.generated.h"

class UProceduralFoliageTile;

UCLASS(BlueprintType, Blueprintable)
class FOLIAGE_API UProceduralFoliageSpawner : public UObject
{
	GENERATED_UCLASS_BODY()

	/** The seed used for generating the randomness of the simulation. */
	UPROPERTY(Category = ProceduralFoliageSimulation, EditAnywhere, BlueprintReadOnly)
	int32 RandomSeed;

	/** Length of the tile (in cm) along one axis. The total area of the tile will be TileSize*TileSize. */
	UPROPERTY(Category = ProceduralFoliageSimulation, EditAnywhere, BlueprintReadOnly)
	float TileSize;

	/** The number of unique tiles to generate. The final simulation is a procedurally determined combination of the various unique tiles. */
	UPROPERTY(Category = ProceduralFoliageSimulation, EditAnywhere, BlueprintReadOnly)
	int32 NumUniqueTiles;

	/** Minimum size of the quad tree used during the simulation. Reduce if too many instances are in splittable leaf quads (as warned in the log). */
	UPROPERTY(Category = ProceduralFoliageSimulation, EditAnywhere, BlueprintReadOnly)
	float MinimumQuadTreeSize;

	FThreadSafeCounter LastCancel;

private:
	/** The types of foliage to procedurally spawn. */
	UPROPERTY(Category = ProceduralFoliageSimulation, EditAnywhere)
	TArray<FFoliageTypeObject> FoliageTypes;

	UPROPERTY()
	bool bNeedsSimulation;

public:
	UFUNCTION(BlueprintCallable, Category = ProceduralFoliageSimulation)
	void Simulate(int32 NumSteps = -1);

	int32 GetRandomNumber();

	const TArray<FFoliageTypeObject>& GetFoliageTypes() const { return FoliageTypes; }

	/** Returns the instances that need to spawn for a given min,max region */
	void GetInstancesToSpawn(TArray<FProceduralFoliageInstance>& OutInstances, const FVector& Min = FVector(-FLT_MAX, -FLT_MAX, -FLT_MAX), const FVector& Max = FVector(FLT_MAX, FLT_MAX, FLT_MAX) ) const;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);
#endif

	virtual void Serialize(FArchive& Ar);

	/** Simulates tiles if needed */
	void SimulateIfNeeded();

	/** Takes a tile index and returns a random tile associated with that index. */
	const UProceduralFoliageTile* GetRandomTile(int32 X, int32 Y);

	/** Creates a temporary empty tile with the appropriate settings created for it. */
	UProceduralFoliageTile* CreateTempTile();

private:
	void CreateProceduralFoliageInstances();

	bool AnyDirty() const;

	void SetClean();
private:
	TArray<TWeakObjectPtr<UProceduralFoliageTile>> PrecomputedTiles;

	FRandomStream RandomStream;
};
