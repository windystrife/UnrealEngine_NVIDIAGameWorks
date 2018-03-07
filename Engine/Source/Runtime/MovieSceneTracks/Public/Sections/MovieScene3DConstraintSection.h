// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Misc/Guid.h"
#include "Curves/KeyHandle.h"
#include "MovieSceneSection.h"
#include "MovieScene3DConstraintSection.generated.h"


/**
 * Base class for 3D constraint section
 */
UCLASS(MinimalAPI)
class UMovieScene3DConstraintSection
	: public UMovieSceneSection
{
	GENERATED_UCLASS_BODY()

public:

	/** Sets the constraint id for this section */
	virtual void SetConstraintId(const FGuid& InId);

	/** Gets the constraint id for this section */
	virtual FGuid GetConstraintId() const;

public:

	//~ UMovieSceneSection interface

	virtual TOptional<float> GetKeyTime(FKeyHandle KeyHandle) const override;
	virtual void SetKeyTime(FKeyHandle KeyHandle, float Time) override;
	virtual void OnBindingsUpdated(const TMap<FGuid, FGuid>& OldGuidToNewGuidMap) override;
	
protected:

	/** The possessable guid that this constraint uses */
	UPROPERTY()
	FGuid ConstraintId;
};
