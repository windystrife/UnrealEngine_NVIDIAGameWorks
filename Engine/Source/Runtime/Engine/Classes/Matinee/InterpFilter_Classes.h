// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/** 
 * InterpFilter_Classes: Filter class for filtering matinee groups.  
 * Used by the matinee editor to filter groups to specific classes of attached actors.
 *
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Templates/SubclassOf.h"
#include "Matinee/InterpFilter.h"
#include "InterpFilter_Classes.generated.h"

UCLASS()
class UInterpFilter_Classes : public UInterpFilter
{
	GENERATED_UCLASS_BODY()

#if WITH_EDITORONLY_DATA
	/** Which class to filter groups by. */
	UPROPERTY()
	TSubclassOf<class UObject>  ClassToFilterBy;

	/** List of allowed track classes.  If empty, then all track classes will be included.  Only groups that
		contain at least one of these types of tracks will be displayed. */
	UPROPERTY()
	TArray<TSubclassOf<class UObject> > TrackClasses;

#endif // WITH_EDITORONLY_DATA

	//~ Begin UInterpFilter Interface
	virtual void FilterData(class AMatineeActor* InMatineeActor) override;
	//~ End UInterpFilter Interface
};

