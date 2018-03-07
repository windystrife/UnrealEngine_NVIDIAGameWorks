// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "AI/Navigation/NavFilters/NavigationQueryFilter.h"
#include "RecastFilter_UseDefaultArea.generated.h"

class ANavigationData;

/** Regular navigation area, applied to entire navigation data by default */
UCLASS(MinimalAPI)
class URecastFilter_UseDefaultArea : public UNavigationQueryFilter
{
	GENERATED_UCLASS_BODY()

	virtual void InitializeFilter(const ANavigationData& NavData, const UObject* Querier, FNavigationQueryFilter& Filter) const override;
};
