// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "AI/Navigation/NavigationTypes.h"
#include "GenericOctreePublic.h"
#include "AI/Navigation/NavigationSystem.h"
#include "EngineStats.h"
#include "AI/NavigationModifier.h"
#include "GenericOctree.h"

class INavRelevantInterface;

struct ENGINE_API FNavigationOctreeFilter
{
	/** pass when actor has geometry */
	uint32 bIncludeGeometry : 1;
	/** pass when actor has any offmesh link modifier */
	uint32 bIncludeOffmeshLinks : 1;
	/** pass when actor has any area modifier */
	uint32 bIncludeAreas : 1;
	/** pass when actor has any modifier with meta area */
	uint32 bIncludeMetaAreas : 1;

	FNavigationOctreeFilter() :
		bIncludeGeometry(false), bIncludeOffmeshLinks(false), bIncludeAreas(false), bIncludeMetaAreas(false)
	{}
};

// @todo consider optional structures that can contain a delegate instead of 
// actual copy of collision data
struct ENGINE_API FNavigationRelevantData : public TSharedFromThis<FNavigationRelevantData, ESPMode::ThreadSafe>
{
	DECLARE_DELEGATE_RetVal_OneParam(bool, FFilterNavDataDelegate, const struct FNavDataConfig*);

	/** exported geometry (used by recast navmesh as FRecastGeometryCache) */
	TNavStatArray<uint8> CollisionData;

	/** cached voxels (used by recast navmesh as FRecastVoxelCache) */
	TNavStatArray<uint8> VoxelData;

	/** bounds of geometry (unreal coords) */
	FBox Bounds;

	/** Gathers per instance data for navigation geometry in a specified area box */
	FNavDataPerInstanceTransformDelegate NavDataPerInstanceTransformDelegate;

	/** called to check if hosted geometry should be used for given FNavDataConfig. If not set then "true" is assumed. */
	FFilterNavDataDelegate ShouldUseGeometryDelegate;

	/** additional modifiers: areas and external links */
	FCompositeNavModifier Modifiers;

	/** UObject these data represents */
	const TWeakObjectPtr<UObject> SourceObject;

	/** get set to true when lazy navigation exporting is enabled and this navigation data has "potential" of 
	 *	containing geometry data. First access will result in gathering the data and setting this flag back to false.
	 *	Mind that this flag can go back to 'true' if related data gets cleared out. */
	uint32 bPendingLazyGeometryGathering : 1;
	uint32 bPendingLazyModifiersGathering : 1;

	uint32 bSupportsGatheringGeometrySlices : 1;
	
	FNavigationRelevantData(UObject& Source) 
		: SourceObject(&Source)
		, bPendingLazyGeometryGathering(false)
		, bPendingLazyModifiersGathering(false)
	{}

	FORCEINLINE bool HasGeometry() const { return VoxelData.Num() || CollisionData.Num(); }
	FORCEINLINE bool HasModifiers() const { return !Modifiers.IsEmpty(); }
	FORCEINLINE bool IsPendingLazyGeometryGathering() const { return bPendingLazyGeometryGathering; }
	FORCEINLINE bool IsPendingLazyModifiersGathering() const { return bPendingLazyModifiersGathering; }
	FORCEINLINE bool SupportsGatheringGeometrySlices() const { return bSupportsGatheringGeometrySlices; }
	FORCEINLINE bool IsEmpty() const { return !HasGeometry() && !HasModifiers(); }
	FORCEINLINE uint32 GetAllocatedSize() const { return CollisionData.GetAllocatedSize() + VoxelData.GetAllocatedSize() + Modifiers.GetAllocatedSize(); }
	FORCEINLINE uint32 GetGeometryAllocatedSize() const { return CollisionData.GetAllocatedSize() + VoxelData.GetAllocatedSize(); }
	FORCEINLINE int32 GetDirtyFlag() const
	{
		return ((HasGeometry() || IsPendingLazyGeometryGathering()) ? ENavigationDirtyFlag::Geometry : 0) |
			((HasModifiers() || IsPendingLazyModifiersGathering())? ENavigationDirtyFlag::DynamicModifier : 0) |
			(Modifiers.HasAgentHeightAdjust() ? ENavigationDirtyFlag::UseAgentHeight : 0);
	}
	
	bool HasPerInstanceTransforms() const;
	bool IsMatchingFilter(const FNavigationOctreeFilter& Filter) const;
	void Shrink();

	FORCEINLINE UObject* GetOwner() const { return SourceObject.Get(); }
};

struct ENGINE_API FNavigationOctreeElement
{
	FBoxSphereBounds Bounds;
	TSharedRef<FNavigationRelevantData, ESPMode::ThreadSafe> Data;

	FNavigationOctreeElement(UObject& SourceObject)
		: Data(new FNavigationRelevantData(SourceObject))
	{

	}

	FORCEINLINE bool IsEmpty() const
	{
		const FBox BBox = Bounds.GetBox();
		return Data->IsEmpty() && (BBox.IsValid == 0 || BBox.GetSize().IsNearlyZero());
	}

	FORCEINLINE bool IsMatchingFilter(const FNavigationOctreeFilter& Filter) const
	{
		return Data->IsMatchingFilter(Filter);
	}

	/** 
	 *	retrieves Modifier, if it doesn't contain any "Meta Navigation Areas". 
	 *	If it does then retrieves a copy with meta areas substituted with
	 *	appropriate non-meta areas, depending on NavAgent
	 */
	FORCEINLINE FCompositeNavModifier GetModifierForAgent(const struct FNavAgentProperties* NavAgent = NULL) const 
	{ 
		return Data->Modifiers.HasMetaAreas() ? Data->Modifiers.GetInstantiatedMetaModifier(NavAgent, Data->SourceObject) : Data->Modifiers;
	}

	FORCEINLINE bool ShouldUseGeometry(const FNavDataConfig& NavConfig) const
	{ 
		return !Data->ShouldUseGeometryDelegate.IsBound() || Data->ShouldUseGeometryDelegate.Execute(&NavConfig);
	}

	FORCEINLINE int32 GetAllocatedSize() const
	{
		return Data->GetAllocatedSize();
	}

	FORCEINLINE void Shrink()
	{
		Data->Shrink();
	}

	FORCEINLINE UObject* GetOwner() const { return Data->SourceObject.Get(); }
};

struct FNavigationOctreeSemantics
{
	enum { MaxElementsPerLeaf = 16 };
	enum { MinInclusiveElementsPerNode = 7 };
	enum { MaxNodeDepth = 12 };

	typedef TInlineAllocator<MaxElementsPerLeaf> ElementAllocator;
	//typedef FDefaultAllocator ElementAllocator;

	FORCEINLINE static const FBoxSphereBounds& GetBoundingBox(const FNavigationOctreeElement& NavData)
	{
		return NavData.Bounds;
	}

	FORCEINLINE static bool AreElementsEqual(const FNavigationOctreeElement& A, const FNavigationOctreeElement& B)
	{
		return A.Data->SourceObject == B.Data->SourceObject;
	}

#if NAVSYS_DEBUG
	FORCENOINLINE 
#endif // NAVSYS_DEBUG
	static void SetElementId(const FNavigationOctreeElement& Element, FOctreeElementId Id);
};

class FNavigationOctree : public TOctree<FNavigationOctreeElement, FNavigationOctreeSemantics>, public TSharedFromThis<FNavigationOctree, ESPMode::ThreadSafe>
{
public:
	DECLARE_DELEGATE_TwoParams(FNavigableGeometryComponentExportDelegate, UActorComponent*, FNavigationRelevantData&);
	FNavigableGeometryComponentExportDelegate ComponentExportDelegate;

	enum ENavGeometryStoringMode {
		SkipNavGeometry,
		StoreNavGeometry,
	};

	FNavigationOctree(const FVector& Origin, float Radius);
	virtual ~FNavigationOctree();

	/** Add new node and fill it with navigation export data */
	void AddNode(UObject* ElementOb, INavRelevantInterface* NavElement, const FBox& Bounds, FNavigationOctreeElement& Data);

	/** Append new data to existing node */
	void AppendToNode(const FOctreeElementId& Id, INavRelevantInterface* NavElement, const FBox& Bounds, FNavigationOctreeElement& Data);

	/** Updates element bounds remove/add operation */
	void UpdateNode(const FOctreeElementId& Id, const FBox& NewBounds);

	/** Remove node */
	void RemoveNode(const FOctreeElementId& Id);

	void SetNavigableGeometryStoringMode(ENavGeometryStoringMode NavGeometryMode);

	const FNavigationRelevantData* GetDataForID(const FOctreeElementId& Id) const;

	ENavGeometryStoringMode GetNavGeometryStoringMode() const
	{
		return bGatherGeometry ? StoreNavGeometry : SkipNavGeometry;
	}

	void SetDataGatheringMode(ENavDataGatheringModeConfig Mode);
	
	// @hack! TO BE FIXED
	void DemandLazyDataGathering(const FNavigationOctreeElement& Element);
	void DemandLazyDataGathering(FNavigationRelevantData& ElementData);

protected:
	ENavDataGatheringMode DefaultGeometryGatheringMode;
	uint32 bGatherGeometry : 1;
	uint32 NodesMemory;
};

template<>
FORCEINLINE_DEBUGGABLE void SetOctreeMemoryUsage(TOctree<FNavigationOctreeElement, FNavigationOctreeSemantics>* Octree, int32 NewSize)
{
	{
		DEC_DWORD_STAT_BY( STAT_NavigationMemory, Octree->TotalSizeBytes );
		DEC_DWORD_STAT_BY(STAT_Navigation_CollisionTreeMemory, Octree->TotalSizeBytes);
	}
	Octree->TotalSizeBytes = NewSize;
	{
		INC_DWORD_STAT_BY( STAT_NavigationMemory, NewSize );
		INC_DWORD_STAT_BY(STAT_Navigation_CollisionTreeMemory, NewSize);
	}
}
