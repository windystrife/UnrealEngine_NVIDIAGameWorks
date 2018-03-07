// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
InstancedFoliage.h: Instanced foliage type definitions.
=============================================================================*/
#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include "FoliageInstanceBase.h"

class AInstancedFoliageActor;
class UActorComponent;
class UFoliageType;
class UHierarchicalInstancedStaticMeshComponent;
class UPrimitiveComponent;
struct FFoliageInstanceHash;

//
// Forward declarations.
//
class UHierarchicalInstancedStaticMeshComponent;
class AInstancedFoliageActor;
class UFoliageType;
struct FFoliageInstanceHash;

DECLARE_LOG_CATEGORY_EXTERN(LogInstancedFoliage, Log, All);

/**
* Flags stored with each instance
*/
enum EFoliageInstanceFlags
{
	FOLIAGE_AlignToNormal = 0x00000001,
	FOLIAGE_NoRandomYaw = 0x00000002,
	FOLIAGE_Readjusted = 0x00000004,
	FOLIAGE_InstanceDeleted = 0x00000008,	// Used only for migration from pre-HierarchicalISM foliage.
};

/**
*	FFoliageInstancePlacementInfo - placement info an individual instance
*/
struct FFoliageInstancePlacementInfo
{
	FVector Location;
	FRotator Rotation;
	FRotator PreAlignRotation;
	FVector DrawScale3D;
	float ZOffset;
	uint32 Flags;

	FFoliageInstancePlacementInfo()
		: Location(0.f, 0.f, 0.f)
		, Rotation(0, 0, 0)
		, PreAlignRotation(0, 0, 0)
		, DrawScale3D(1.f, 1.f, 1.f)
		, ZOffset(0.f)
		, Flags(0)
	{}
};

/**
*	Legacy instance
*/
struct FFoliageInstance_Deprecated : public FFoliageInstancePlacementInfo
{
	UActorComponent* Base;
	FGuid ProceduralGuid;
	friend FArchive& operator<<(FArchive& Ar, FFoliageInstance_Deprecated& Instance);
};

/**
*	FFoliageInstance - editor info an individual instance
*/
struct FFoliageInstance : public FFoliageInstancePlacementInfo
{
	// ID of base this instance was painted on
	FFoliageInstanceBaseId BaseId;

	FGuid ProceduralGuid;

	FFoliageInstance()
		: BaseId(0)
	{}

	friend FArchive& operator<<(FArchive& Ar, FFoliageInstance& Instance);

	FTransform GetInstanceWorldTransform() const
	{
		return FTransform(Rotation, Location, DrawScale3D);
	}

	void AlignToNormal(const FVector& InNormal, float AlignMaxAngle = 0.f)
	{
		Flags |= FOLIAGE_AlignToNormal;

		FRotator AlignRotation = InNormal.Rotation();
		// Static meshes are authored along the vertical axis rather than the X axis, so we add 90 degrees to the static mesh's Pitch.
		AlignRotation.Pitch -= 90.f;
		// Clamp its value inside +/- one rotation
		AlignRotation.Pitch = FRotator::NormalizeAxis(AlignRotation.Pitch);

		// limit the maximum pitch angle if it's > 0.
		if (AlignMaxAngle > 0.f)
		{
			int32 MaxPitch = AlignMaxAngle;
			if (AlignRotation.Pitch > MaxPitch)
			{
				AlignRotation.Pitch = MaxPitch;
			}
			else if (AlignRotation.Pitch < -MaxPitch)
			{
				AlignRotation.Pitch = -MaxPitch;
			}
		}

		PreAlignRotation = Rotation;
		Rotation = FRotator(FQuat(AlignRotation) * FQuat(Rotation));
	}
};

struct FFoliageMeshInfo_Deprecated
{
	UHierarchicalInstancedStaticMeshComponent* Component;

#if WITH_EDITORONLY_DATA
	// Allows us to detect if FoliageType was updated while this level wasn't loaded
	FGuid FoliageTypeUpdateGuid;

	// Editor-only placed instances
	TArray<FFoliageInstance_Deprecated> Instances;
#endif

	FFoliageMeshInfo_Deprecated()
		: Component(nullptr)
	{
	}

	friend FArchive& operator<<(FArchive& Ar, FFoliageMeshInfo_Deprecated& MeshInfo);
};

/**
*	FFoliageMeshInfo - editor info for all matching foliage meshes
*/
struct FFoliageMeshInfo
{
	UHierarchicalInstancedStaticMeshComponent* Component;

#if WITH_EDITORONLY_DATA
	// Allows us to detect if FoliageType was updated while this level wasn't loaded
	FGuid FoliageTypeUpdateGuid;

	// Editor-only placed instances
	TArray<FFoliageInstance> Instances;

	// Transient, editor-only locality hash of instances
	TUniquePtr<FFoliageInstanceHash> InstanceHash;

	// Transient, editor-only set of instances per component
	TMap<FFoliageInstanceBaseId, TSet<int32>> ComponentHash;

	// Transient, editor-only list of selected instances.
	TSet<int32> SelectedIndices;
#endif

	FOLIAGE_API FFoliageMeshInfo();
	FOLIAGE_API ~FFoliageMeshInfo();

	FFoliageMeshInfo(FFoliageMeshInfo&& Other)
		// even VC++2013 doesn't support "=default" on move constructors
		: Component(Other.Component)
#if WITH_EDITORONLY_DATA
		, FoliageTypeUpdateGuid(MoveTemp(Other.FoliageTypeUpdateGuid))
		, Instances(MoveTemp(Other.Instances))
		, InstanceHash(MoveTemp(Other.InstanceHash))
		, ComponentHash(MoveTemp(Other.ComponentHash))
		, SelectedIndices(MoveTemp(Other.SelectedIndices))
#endif
	{ }

	FFoliageMeshInfo& operator=(FFoliageMeshInfo&& Other)
		// even VC++2013 doesn't support "=default" on move assignment
	{
		Component = Other.Component;
#if WITH_EDITORONLY_DATA
		FoliageTypeUpdateGuid = MoveTemp(Other.FoliageTypeUpdateGuid);
		Instances = MoveTemp(Other.Instances);
		InstanceHash = MoveTemp(Other.InstanceHash);
		ComponentHash = MoveTemp(Other.ComponentHash);
		SelectedIndices = MoveTemp(Other.SelectedIndices);
#endif

		return *this;
	}

#if WITH_EDITOR
	FOLIAGE_API void AddInstance(AInstancedFoliageActor* InIFA, const UFoliageType* InSettings, const FFoliageInstance& InNewInstance, bool RebuildFoliageTree);
	FOLIAGE_API void AddInstance(AInstancedFoliageActor* InIFA, const UFoliageType* InSettings, const FFoliageInstance& InNewInstance, UActorComponent* InBaseComponent, bool RebuildFoliageTree);
	FOLIAGE_API void RemoveInstances(AInstancedFoliageActor* InIFA, const TArray<int32>& InInstancesToRemove, bool RebuildFoliageTree);
	// Apply changes in the FoliageType to the component
	FOLIAGE_API void UpdateComponentSettings(const UFoliageType* InSettings);
	// Recreate the component if the FoliageType's ComponentClass doesn't match the Component's class
	FOLIAGE_API void CheckComponentClass(AInstancedFoliageActor* InIFA, const UFoliageType* InSettings);
	FOLIAGE_API void PreMoveInstances(AInstancedFoliageActor* InIFA, const TArray<int32>& InInstancesToMove);
	FOLIAGE_API void PostMoveInstances(AInstancedFoliageActor* InIFA, const TArray<int32>& InInstancesMoved);
	FOLIAGE_API void PostUpdateInstances(AInstancedFoliageActor* InIFA, const TArray<int32>& InInstancesUpdated, bool bReAddToHash = false);
	FOLIAGE_API void DuplicateInstances(AInstancedFoliageActor* InIFA, UFoliageType* InSettings, const TArray<int32>& InInstancesToDuplicate);
	FOLIAGE_API void GetInstancesInsideSphere(const FSphere& Sphere, TArray<int32>& OutInstances);
	FOLIAGE_API void GetInstanceAtLocation(const FVector& Location, int32& OutInstance, bool& bOutSucess);
	FOLIAGE_API bool CheckForOverlappingSphere(const FSphere& Sphere);
	FOLIAGE_API bool CheckForOverlappingInstanceExcluding(int32 TestInstanceIdx, float Radius, TSet<int32>& ExcludeInstances);

	// Destroy existing clusters and reassign all instances to new clusters
	FOLIAGE_API void ReallocateClusters(AInstancedFoliageActor* InIFA, UFoliageType* InSettings);

	FOLIAGE_API void ReapplyInstancesToComponent();

	FOLIAGE_API void SelectInstances(AInstancedFoliageActor* InIFA, bool bSelect, TArray<int32>& Instances);

	FOLIAGE_API void SelectInstances(AInstancedFoliageActor* InIFA, bool bSelect);

	// Get the number of placed instances
	FOLIAGE_API int32 GetInstanceCount() const;

	FOLIAGE_API void AddToBaseHash(int32 InstanceIdx);
	FOLIAGE_API void RemoveFromBaseHash(int32 InstanceIdx);

	// Create and register a new component
	void CreateNewComponent(AInstancedFoliageActor* InIFA, const UFoliageType* InSettings);

	// For debugging. Validate state after editing.
	void CheckValid();

	FOLIAGE_API void HandleComponentMeshBoundsChanged(const FBoxSphereBounds& NewBounds);
#endif

	friend FArchive& operator<<(FArchive& Ar, FFoliageMeshInfo& MeshInfo);

#if PLATFORM_COMPILER_HAS_DEFAULTED_FUNCTIONS
	FFoliageMeshInfo(const FFoliageMeshInfo& Other) = delete;
	const FFoliageMeshInfo& operator=(const FFoliageMeshInfo& Other) = delete;
#else
private:
	FFoliageMeshInfo(const FFoliageMeshInfo& Other);
	const FFoliageMeshInfo& operator=(const FFoliageMeshInfo& Other);
#endif
};

#if WITH_EDITORONLY_DATA
//
// FFoliageInstanceHash
//

#define FOLIAGE_HASH_CELL_BITS 9	// 512x512 grid

struct FFoliageInstanceHash
{
private:
	const int32 HashCellBits;
	TMap<uint64, TSet<int32>> CellMap;

	uint64 MakeKey(int32 CellX, int32 CellY) const
	{
		return ((uint64)(*(uint32*)(&CellX)) << 32) | (*(uint32*)(&CellY) & 0xffffffff);
	}

	uint64 MakeKey(const FVector& Location) const
	{
		return  MakeKey(FMath::FloorToInt(Location.X) >> HashCellBits, FMath::FloorToInt(Location.Y) >> HashCellBits);
	}

public:
	FFoliageInstanceHash(int32 InHashCellBits = FOLIAGE_HASH_CELL_BITS)
		: HashCellBits(InHashCellBits)
	{}

	void InsertInstance(const FVector& InstanceLocation, int32 InstanceIndex)
	{
		uint64 Key = MakeKey(InstanceLocation);

		CellMap.FindOrAdd(Key).Add(InstanceIndex);
	}

	void RemoveInstance(const FVector& InstanceLocation, int32 InstanceIndex)
	{
		uint64 Key = MakeKey(InstanceLocation);

		int32 RemoveCount = CellMap.FindChecked(Key).Remove(InstanceIndex);
		check(RemoveCount == 1);
	}

	void GetInstancesOverlappingBox(const FBox& InBox, TArray<int32>& OutInstanceIndices) const
	{
		int32 MinX = FMath::FloorToInt(InBox.Min.X) >> HashCellBits;
		int32 MinY = FMath::FloorToInt(InBox.Min.Y) >> HashCellBits;
		int32 MaxX = FMath::FloorToInt(InBox.Max.X) >> HashCellBits;
		int32 MaxY = FMath::FloorToInt(InBox.Max.Y) >> HashCellBits;

		for (int32 y = MinY; y <= MaxY; y++)
		{
			for (int32 x = MinX; x <= MaxX; x++)
			{
				uint64 Key = MakeKey(x, y);
				auto* SetPtr = CellMap.Find(Key);
				if (SetPtr)
				{
					OutInstanceIndices.Append(SetPtr->Array());
				}
			}
		}
	}

	TArray<int32> GetInstancesOverlappingBox(const FBox& InBox) const
	{
		TArray<int32> Result;
		GetInstancesOverlappingBox(InBox, Result);
		return Result;
	}

#if UE_BUILD_DEBUG
	void CheckInstanceCount(int32 InCount) const
	{
		int32 HashCount = 0;
		for (const auto& Pair : CellMap)
		{
			HashCount += Pair.Value.Num();
		}

		check(HashCount == InCount);
	}
#endif

	void Empty()
	{
		CellMap.Empty();
	}

	friend FArchive& operator<<(FArchive& Ar, FFoliageInstanceHash& Hash)
	{
		Ar << Hash.CellMap;
		return Ar;
	}
};
#endif

/** This is kind of a hack, but is needed right now for backwards compat of code. We use it to describe the placement mode (procedural vs manual)*/
namespace EFoliagePlacementMode
{
	enum Type
	{
		Manual = 0,
		Procedural = 1,
	};

}

/** Used to define a vector along which we'd like to spawn an instance. */
struct FDesiredFoliageInstance
{
	FDesiredFoliageInstance()
		: FoliageType(nullptr)
		, StartTrace(ForceInit)
		, EndTrace(ForceInit)
		, Rotation(ForceInit)
		, TraceRadius(0.f)
		, Age(0.f)
		, PlacementMode(EFoliagePlacementMode::Manual)
	{

	}

	FDesiredFoliageInstance(const FVector& InStartTrace, const FVector& InEndTrace, const float InTraceRadius = 0.f)
		: FoliageType(nullptr)
		, StartTrace(InStartTrace)
		, EndTrace(InEndTrace)
		, Rotation(ForceInit)
		, TraceRadius(InTraceRadius)
		, Age(0.f)
		, PlacementMode(EFoliagePlacementMode::Manual)
	{
	}

	const UFoliageType* FoliageType;
	FGuid ProceduralGuid;
	FVector StartTrace;
	FVector EndTrace;
	FQuat Rotation;
	float TraceRadius;
	float Age;
	const struct FBodyInstance* ProceduralVolumeBodyInstance;
	EFoliagePlacementMode::Type PlacementMode;
};

#if WITH_EDITOR
// Struct to hold potential instances we've sampled
struct FOLIAGE_API FPotentialInstance
{
	FVector HitLocation;
	FVector HitNormal;
	UPrimitiveComponent* HitComponent;
	float HitWeight;
	FDesiredFoliageInstance DesiredInstance;

	FPotentialInstance(FVector InHitLocation, FVector InHitNormal, UPrimitiveComponent* InHitComponent, float InHitWeight, const FDesiredFoliageInstance& InDesiredInstance = FDesiredFoliageInstance());
	bool PlaceInstance(const UWorld* InWorld, const UFoliageType* Settings, FFoliageInstance& Inst, bool bSkipCollision = false);
};
#endif
