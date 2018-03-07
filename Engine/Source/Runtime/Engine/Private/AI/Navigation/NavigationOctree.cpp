// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AI/NavigationOctree.h"
#include "AI/Navigation/NavRelevantInterface.h"

//----------------------------------------------------------------------//
// FNavigationOctree
//----------------------------------------------------------------------//
FNavigationOctree::FNavigationOctree(const FVector& Origin, float Radius)
	: TOctree<FNavigationOctreeElement, FNavigationOctreeSemantics>(Origin, Radius)
	, DefaultGeometryGatheringMode(ENavDataGatheringMode::Instant)
	, bGatherGeometry(false)
	, NodesMemory(0)
{
	INC_DWORD_STAT_BY( STAT_NavigationMemory, sizeof(*this) );
}

FNavigationOctree::~FNavigationOctree()
{
	DEC_DWORD_STAT_BY( STAT_NavigationMemory, sizeof(*this) );
	DEC_MEMORY_STAT_BY(STAT_Navigation_CollisionTreeMemory, NodesMemory);
}

void FNavigationOctree::SetDataGatheringMode(ENavDataGatheringModeConfig Mode)
{
	check(Mode != ENavDataGatheringModeConfig::Invalid);
	DefaultGeometryGatheringMode = ENavDataGatheringMode(Mode);
}

void FNavigationOctree::SetNavigableGeometryStoringMode(ENavGeometryStoringMode NavGeometryMode)
{
	bGatherGeometry = (NavGeometryMode == FNavigationOctree::StoreNavGeometry);
}

void FNavigationOctree::DemandLazyDataGathering(FNavigationRelevantData& ElementData)
{
	UObject* ElementOb = ElementData.GetOwner();
	if (ElementOb == nullptr)
	{
		return;
	}

	bool bShrink = false;
	const int32 OrgElementMemory = ElementData.GetGeometryAllocatedSize();

	if (ElementData.IsPendingLazyGeometryGathering() == true && ElementData.SupportsGatheringGeometrySlices() == false)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_RecastNavMeshGenerator_LazyGeometryExport);
		UActorComponent& ActorComp = *CastChecked<UActorComponent>(ElementOb);
		ComponentExportDelegate.ExecuteIfBound(&ActorComp, ElementData);

		bShrink = true;

		// mark this element as no longer needing geometry gathering
		ElementData.bPendingLazyGeometryGathering = false;
	}

	if (ElementData.IsPendingLazyModifiersGathering())
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_RecastNavMeshGenerator_LazyModifiersExport);
		INavRelevantInterface* NavElement = Cast<INavRelevantInterface>(ElementOb);
		check(NavElement);
		NavElement->GetNavigationData(ElementData);
		ElementData.bPendingLazyModifiersGathering = false;
		bShrink = true;
	}

	if (bShrink)
	{
		// shrink arrays before counting memory
		// it will be reallocated when adding to octree and RemoveNode will have different value returned by GetAllocatedSize()
		ElementData.Shrink();
	}

	const int32 ElementMemoryChange = ElementData.GetGeometryAllocatedSize() - OrgElementMemory;
	const_cast<FNavigationOctree*>(this)->NodesMemory += ElementMemoryChange;
	INC_MEMORY_STAT_BY(STAT_Navigation_CollisionTreeMemory, ElementMemoryChange);
}

void FNavigationOctree::DemandLazyDataGathering(const FNavigationOctreeElement& Element)
{
	FNavigationRelevantData& MutableData = const_cast<FNavigationRelevantData&>(*Element.Data);
	DemandLazyDataGathering(MutableData);
}

void FNavigationOctree::AddNode(UObject* ElementOb, INavRelevantInterface* NavElement, const FBox& Bounds, FNavigationOctreeElement& Element)
{
	// we assume NavElement is ElementOb already cast
	Element.Bounds = Bounds;	

	if (NavElement)
	{
		const ENavDataGatheringMode GatheringMode = NavElement->GetGeometryGatheringMode();
		bool bDoInstantGathering = ((GatheringMode == ENavDataGatheringMode::Default && DefaultGeometryGatheringMode == ENavDataGatheringMode::Instant) 
			|| GatheringMode == ENavDataGatheringMode::Instant);

		if (bGatherGeometry)
		{
			UActorComponent* ActorComp = Cast<UActorComponent>(ElementOb);
			if (ActorComp)
			{
				if (bDoInstantGathering)
				{
					ComponentExportDelegate.ExecuteIfBound(ActorComp, *Element.Data);
				}
				else
				{
					Element.Data->bPendingLazyGeometryGathering = true;
					Element.Data->bSupportsGatheringGeometrySlices = NavElement && NavElement->SupportsGatheringGeometrySlices();
				}
			}
		}

		SCOPE_CYCLE_COUNTER(STAT_Navigation_GatheringNavigationModifiersSync);
		if (bDoInstantGathering)
		{
			NavElement->GetNavigationData(*Element.Data);
		}
		else
		{
			Element.Data->bPendingLazyModifiersGathering = true;
		}
	}

	// shrink arrays before counting memory
	// it will be reallocated when adding to octree and RemoveNode will have different value returned by GetAllocatedSize()
	Element.Shrink();

	const int32 ElementMemory = Element.GetAllocatedSize();
	NodesMemory += ElementMemory;
	INC_MEMORY_STAT_BY(STAT_Navigation_CollisionTreeMemory, ElementMemory);

	AddElement(Element);
}

void FNavigationOctree::AppendToNode(const FOctreeElementId& Id, INavRelevantInterface* NavElement, const FBox& Bounds, FNavigationOctreeElement& Element)
{
	FNavigationOctreeElement OrgData = GetElementById(Id);

	Element = OrgData;
	Element.Bounds = Bounds + OrgData.Bounds.GetBox();

	if (NavElement)
	{
		SCOPE_CYCLE_COUNTER(STAT_Navigation_GatheringNavigationModifiersSync);
		NavElement->GetNavigationData(*Element.Data);
	}

	// shrink arrays before counting memory
	// it will be reallocated when adding to octree and RemoveNode will have different value returned by GetAllocatedSize()
	Element.Shrink();

	const int32 OrgElementMemory = OrgData.GetAllocatedSize();
	const int32 NewElementMemory = Element.GetAllocatedSize();
	const int32 MemoryDelta = NewElementMemory - OrgElementMemory;

	NodesMemory += MemoryDelta;
	INC_MEMORY_STAT_BY(STAT_Navigation_CollisionTreeMemory, MemoryDelta);

	RemoveElement(Id);
	AddElement(Element);
}

void FNavigationOctree::UpdateNode(const FOctreeElementId& Id, const FBox& NewBounds)
{
	FNavigationOctreeElement ElementCopy = GetElementById(Id);
	RemoveElement(Id);
	ElementCopy.Bounds = NewBounds;
	AddElement(ElementCopy);
}

void FNavigationOctree::RemoveNode(const FOctreeElementId& Id)
{
	FNavigationOctreeElement& Element = GetElementById(Id);
	const int32 ElementMemory = Element.GetAllocatedSize();
	NodesMemory -= ElementMemory;
	DEC_MEMORY_STAT_BY(STAT_Navigation_CollisionTreeMemory, ElementMemory);

	RemoveElement(Id);
}

const FNavigationRelevantData* FNavigationOctree::GetDataForID(const FOctreeElementId& Id) const
{
	if (Id.IsValidId() == false)
	{
		return nullptr;
	}

	const FNavigationOctreeElement& OctreeElement = GetElementById(Id);

	return &*OctreeElement.Data;
}

//----------------------------------------------------------------------//
// FNavigationRelevantData
//----------------------------------------------------------------------//

bool FNavigationRelevantData::HasPerInstanceTransforms() const
{
	return NavDataPerInstanceTransformDelegate.IsBound();
}

bool FNavigationRelevantData::IsMatchingFilter(const FNavigationOctreeFilter& Filter) const
{
	return (Filter.bIncludeGeometry && HasGeometry()) ||
		(Filter.bIncludeOffmeshLinks && (Modifiers.HasPotentialLinks() || Modifiers.HasLinks())) ||
		(Filter.bIncludeAreas && Modifiers.HasAreas()) ||
		(Filter.bIncludeMetaAreas && Modifiers.HasMetaAreas());
}

void FNavigationRelevantData::Shrink()
{
	CollisionData.Shrink();
	VoxelData.Shrink();
	Modifiers.Shrink();
}

//----------------------------------------------------------------------//
// FNavigationOctreeSemantics
//----------------------------------------------------------------------//
#if NAVSYS_DEBUG
FORCENOINLINE
#endif // NAVSYS_DEBUG
void FNavigationOctreeSemantics::SetElementId(const FNavigationOctreeElement& Element, FOctreeElementId Id)
{
	UWorld* World = NULL;
	UObject* ElementOwner = Element.GetOwner();

	if (AActor* Actor = Cast<AActor>(ElementOwner))
	{
		World = Actor->GetWorld();
	}
	else if (UActorComponent* AC = Cast<UActorComponent>(ElementOwner))
	{
		World = AC->GetWorld();
	}
	else if (ULevel* Level = Cast<ULevel>(ElementOwner))
	{
		World = Level->OwningWorld;
	}

	UNavigationSystem* NavSys = UNavigationSystem::GetCurrent(World);
	if (NavSys)
	{
		NavSys->SetObjectsNavOctreeId(ElementOwner, Id);
	}
}
