// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Matinee/InterpTrackInst.h"
#include "InterpTrackInstProperty.generated.h"

class AActor;
class UInterpTrack;

UCLASS()
class UInterpTrackInstProperty : public UInterpTrackInst
{
	GENERATED_UCLASS_BODY()

	/** Function to call after updating the value of the color property. */
	UPROPERTY()
	class UProperty* InterpProperty;

	/** Pointer to the UObject instance that is the outer of the color property we are interpolating on, this is used to process the property update callback. */
	UPROPERTY()
	class UObject* PropertyOuterObjectInst;


	/**
	 * Retrieve the update callback from the interp property's metadata and stores it.
	 *
	 * @param InActor			Actor we are operating on.
	 * @param TrackProperty		Property we are interpolating.
	 */
	void SetupPropertyUpdateCallback(AActor* InActor, const FName& TrackPropertyName);

	/** 
	 * Attempt to call the property update callback.
	 */
	void CallPropertyUpdateCallback();

	// Begin UInterpTrackInst Instance
	virtual void TermTrackInst(UInterpTrack* Track) override;
	// End UInterpTrackInst Instance
};

