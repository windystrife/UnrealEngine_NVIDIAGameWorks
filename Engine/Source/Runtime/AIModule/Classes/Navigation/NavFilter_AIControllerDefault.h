// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "Templates/SubclassOf.h"
#include "AI/Navigation/NavFilters/NavigationQueryFilter.h"
#include "NavFilter_AIControllerDefault.generated.h"

UCLASS()
class AIMODULE_API UNavFilter_AIControllerDefault : public UNavigationQueryFilter
{
	GENERATED_BODY()
public:
	UNavFilter_AIControllerDefault(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual TSubclassOf<UNavigationQueryFilter> GetSimpleFilterForAgent(const UObject& Querier) const;
};
