// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AI/Navigation/NavFilters/RecastFilter_UseDefaultArea.h"
#include "Templates/Casts.h"
#include "AI/Navigation/RecastNavMesh.h"
#include "AI/Navigation/RecastQueryFilter.h"

URecastFilter_UseDefaultArea::URecastFilter_UseDefaultArea(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

void URecastFilter_UseDefaultArea::InitializeFilter(const ANavigationData& NavData, const UObject* Querier, FNavigationQueryFilter& Filter) const
{
#if WITH_RECAST
	Filter.SetFilterImplementation(dynamic_cast<const INavigationQueryFilterInterface*>(ARecastNavMesh::GetNamedFilter(ERecastNamedFilter::FilterOutAreas)));
#endif // WITH_RECAST

	Super::InitializeFilter(NavData, Querier, Filter);
}
