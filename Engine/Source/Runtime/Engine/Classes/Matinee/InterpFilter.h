// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/** 
 * InterpFilter: Filter class for filtering matinee groups.  
 * By default no groups are filtered.
 *
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "InterpFilter.generated.h"

UCLASS()
class UInterpFilter : public UObject
{
	GENERATED_UCLASS_BODY()

	/** Caption for this filter. */
	UPROPERTY()
	FString Caption;


	/** 
	 * Given a matinee actor, updates visibility of groups and tracks based on the filter settings
	 *
	 * @param InMatineeActor			Matinee actor with interp data to filter.
	 */
	virtual void FilterData(class AMatineeActor* InMatineeActor);
};

