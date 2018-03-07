// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CriticalSection.h"
#include "Math/BoxSphereBounds.h"
#include "ClothingSimulationInterface.h"

class UClothingAsset;
class USkeletalMeshComponent;
struct FClothCollisionData;
struct FClothSimulData;
struct FMatrix;
struct FClothPhysicalMeshData;

// Base clothing actor just needs a link back to its parent asset, everything else defined by derived simulation
class FClothingActorBase
{
public:

	// Link back to parent asset
	UClothingAsset* AssetCreatedFrom;
};

// Base simlation data that just about every simulation would need
class FClothingSimulationContextBase : public IClothingSimulationContext
{
public:

	FClothingSimulationContextBase()
		: DeltaSeconds(0.0f)
		, PredictedLod(0)
		, WindVelocity(FVector::ZeroVector)
		, WindAdaption(0.0f)
		, TeleportMode(EClothingTeleportMode::None)
	{}

	// Delta for this tick
	float DeltaSeconds;

	// World space bone transforms of the owning component
	TArray<FTransform> BoneTransforms;

	// Component to world transform of the owning component
	FTransform ComponentToWorld;

	// The predicted LOD of the skeletal mesh component running the simulation
	int32 PredictedLod;

	// Wind velocity at the component location
	FVector WindVelocity;

	// Wind adaption, a measure of how quickly to adapt to the wind speed
	// when using the legacy wind calculation mode
	float WindAdaption;

	// Whether and how we should teleport the simulation this tick
	EClothingTeleportMode TeleportMode;

	// Scale for the max distance constraints of the simulation mesh
	float MaxDistanceScale;
};

// Base simulation to fill in common data for the base context
class FClothingSimulationBase : public IClothingSimulation
{
public:

	FClothingSimulationBase();

	virtual ~FClothingSimulationBase()
	{}

	// Static method for calculating a skinned mesh result from source data
	static CLOTHINGSYSTEMRUNTIME_API void SkinPhysicsMesh(UClothingAsset* InAsset, const FClothPhysicalMeshData& InMesh, const FTransform& RootBoneTransform, const FMatrix* InBoneMatrices, const int32 InNumBoneMatrices, TArray<FVector>& OutPositions, TArray<FVector>& OutNormals);

protected:

	/** Fills in the base data for a clothing simulation */
	virtual void FillContext(USkeletalMeshComponent* InComponent, float InDeltaTime, IClothingSimulationContext* InOutContext) override;

	/** Maximum physics time, incoming deltas will be clamped down to this value on long frames */
	float MaxPhysicsDelta;

};
