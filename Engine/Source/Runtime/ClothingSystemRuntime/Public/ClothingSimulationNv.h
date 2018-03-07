// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_NVCLOTH

#include "ClothingSimulation.h"

#include "Assets/ClothingAsset.h"

namespace nv
{
	namespace cloth
	{
		class Fabric;
		class Cloth;
		class Solver;
		class Factory;
		struct PhaseConfig;
	}
}

namespace physx
{
	class PxVec4;
}

class FClothingSimulationContextNv final : public FClothingSimulationContextBase
{
public:
	// Ref to local matrices from the owning component (for skinning fixed verts)
	TArray<FMatrix> RefToLocals;
};

// Scratch data for simulation to avoid allocations while processing, per actor data
struct FClothingActorScratchData
{
	void Reset();

	TArray<physx::PxVec4> SphereData;
	TArray<uint32> CapsuleSphereIndices;
	TArray<physx::PxVec4> PlaneData;
	TArray<uint32> ConvexMasks;
	TArray<FVector> ParticleVelocities;
};

class FClothingActorNv final : public FClothingActorBase
{
public:

	FClothingActorNv();

	// Runtime data need per-lod for each actor
	struct FActorLodData
	{
		// The fabric object created for this LOD
		nv::cloth::Fabric* Fabric;

		// The cloth (simulation) object for this LOD
		nv::cloth::Cloth* Cloth;

		// Original rest positions for this LOD (needed for self collisions)
		TArray<physx::PxVec4> Px_RestPositions;

		// List of phase configs for this actor. Phases are the different
		// constraint groups that are present (horz, vert, stretch, shear, bend)
		TArray<nv::cloth::PhaseConfig> PhaseConfigs;

		// The types of each phase in the above PhaseConfigs array
		TArray<int32> PhaseTypes;
	};

	// Skins the physics mesh to the current bone transforms, necessary to build motion constraints
	// Stores the results internally (see below)
	void SkinPhysicsMesh(FClothingSimulationContextNv* InContext);

	// Updates the motion constraints for this actor (needs a skinned physics mesh)
	void UpdateMotionConstraints(FClothingSimulationContextNv* InContext);

	// Updates the wind effects on the currently active cloth
	void UpdateWind(FClothingSimulationContextNv* InContext, const FVector& InWindVelocity);

	// Conditional rebuild of the aggregated collisions list
	void ConditionalRebuildCollisions();

private:

	// Builds a list of particle velocities given the current simulation state
	// Required to emulate the legacy wind method that APEX used to use
	void CalculateParticleVelocities(TArray<FVector>& OutVelocities);

	// Current cloth (not mesh) LOD that this actor is running
	int32 CurrentLodIndex;

	// List of all active collisions currently affecting the simulation
	FClothCollisionData AggregatedCollisions;

	// List of collisions that were injected from an external source
	FClothCollisionData ExternalCollisions;

	// Collisions extracted from our physics asset
	FClothCollisionData ExtractedCollisions;

	// Whether or not we need to rebuild our collisions on the next simulation setp
	bool bCollisionsDirty;

	// Index to write back to on GetSimulationData for this actor
	int32 SimDataIndex;

	// Actual clothing LOD data (simulation objects)
	TArray<FActorLodData> LodData;

	// How we're going to calculate our wind data (see EClothingWindMethod for method descriptions)
	EClothingWindMethod WindMethod;

	// Thickness to add to collisions to fake cloth thickness
	float CollisionThickness;

	// Skinned physics mesh information for the active LOD. This is
	// generated once per tick for the currently active clothing LOD
	TArray<FVector> SkinnedPhysicsMeshPositions;
	TArray<FVector> SkinnedPhysicsMeshNormals;

	// Current computed normals of the simulation mesh, the normals abov
	// are the skinned static normals of the physics mesh, this is the current
	// set of normals for the simulation mesh
	TArray<FVector> CurrentNormals;

	// Time step of the last tick, used for velocity calculations
	float PreviousTimestep;

	// Scratch arrays for processing during simulate, grow-only to avoid repeated allocations.
	FClothingActorScratchData Scratch;

	// Simuation given access to our data
	friend class FClothingSimulationNv;
};

class FClothingSimulationNv final : public FClothingSimulationBase
{
	// Cached from the module for speed, DO NOT DELETE, only for creating cloth objects
	nv::cloth::Factory* CachedFactory;

	// Solver object for this simulation
	nv::cloth::Solver* Solver;

	// Currently valid actors (some may not be running depending on LOD)
	TArray<FClothingActorNv> Actors;

public:

	FClothingSimulationNv();
	virtual ~FClothingSimulationNv();

	// IClothingSimulation Interface
	virtual void CreateActor(USkeletalMeshComponent* InOwnerComponent, UClothingAssetBase* InAsset, int32 InSimDataIndex) override;
	virtual IClothingSimulationContext* CreateContext() override;
	virtual void FillContext(USkeletalMeshComponent* InComponent, float InDeltaTime, IClothingSimulationContext* InOutContext) override;
	virtual void Initialize() override;
	virtual void Shutdown() override;
	virtual bool ShouldSimulate() const override;
	virtual void Simulate(IClothingSimulationContext* InContext) override;
	virtual FBoxSphereBounds GetBounds(const USkeletalMeshComponent* InOwnerComponent) const override;

	virtual void DestroyActors() override;
	virtual void DestroyContext(IClothingSimulationContext* InContext) override;
	virtual void GetSimulationData(TMap<int32, FClothSimulData>& OutData, USkeletalMeshComponent* InOwnerComponent, USkeletalMeshComponent* InOverrideComponent) const override;
	virtual void AddExternalCollisions(const FClothCollisionData& InData) override;
	virtual void ClearExternalCollisions() override;
	virtual void GetCollisions(FClothCollisionData& OutCollisions, bool bIncludeExternal = true) const override;
	virtual void GatherStats() const override;
	//////////////////////////////////////////////////////////////////////////

private:

	// Update the LOD for the current actors, this is more complex than just updating a LOD value,
	// we need to skin the incoming simulation mesh to the outgoing mesh (the weighting data should have been
	// built in the asset already) to make sure it matches up without popping
	void UpdateLod(int32 InPredictedLod, const FTransform& ComponentToWorld, const TArray<FTransform>& CSTransforms, bool bForceNoRemap = false);

	// The core simulation is only solving unoriented particles, so we need to compute normals after the
	// simulation runs
	void ComputePhysicalMeshNormals(FClothingActorNv &Actor);

	// Given a clothing config from an asset, apply it to the provided actor. Currently
	// this is only used from CreateActor, but could be exposed for runtime changes
	void ApplyClothConfig(FClothConfig &Config, FClothingActorNv &InActor, USkeletalMeshComponent* InOwnerComponent);

	// Extract collisions from the physics asset inside Asset and apply them to InActor
	// Not safe to call from workers (i.e. inside the simulation).
	void ExtractActorCollisions(USkeletalMeshComponent* InOwnerComponent, UClothingAsset* Asset, FClothingActorNv &InActor);

	// The current LOD index for the owning skeletal mesh component
	int32 CurrentMeshLodIndex;

#if WITH_EDITOR
public:
	CLOTHINGSYSTEMRUNTIME_API void DebugDraw_PhysMesh(USkeletalMeshComponent* OwnerComponent, FPrimitiveDrawInterface* PDI) const;
	CLOTHINGSYSTEMRUNTIME_API void DebugDraw_Normals(USkeletalMeshComponent* OwnerComponent, FPrimitiveDrawInterface* PDI) const;
	CLOTHINGSYSTEMRUNTIME_API void DebugDraw_Collision(USkeletalMeshComponent* OwnerComponent, FPrimitiveDrawInterface* PDI) const;
	CLOTHINGSYSTEMRUNTIME_API void DebugDraw_Backstops(USkeletalMeshComponent* OwnerComponent, FPrimitiveDrawInterface* PDI) const;
	CLOTHINGSYSTEMRUNTIME_API void DebugDraw_MaxDistances(USkeletalMeshComponent* OwnerComponent, FPrimitiveDrawInterface* PDI) const;
	CLOTHINGSYSTEMRUNTIME_API void DebugDraw_SelfCollision(USkeletalMeshComponent* OwnerComponent, FPrimitiveDrawInterface* PDI) const;

#endif
};

#endif
