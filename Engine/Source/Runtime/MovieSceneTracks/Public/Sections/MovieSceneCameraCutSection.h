// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Misc/Guid.h"
#include "Curves/KeyHandle.h"
#include "MovieSceneSection.h"
#include "MovieSceneCameraCutSection.generated.h"


/**
 * Movie CameraCuts are sections on the CameraCuts track, that show what the viewer "sees"
 */
UCLASS(MinimalAPI)
class UMovieSceneCameraCutSection 
	: public UMovieSceneSection
{
	GENERATED_BODY()

public:
	UMovieSceneCameraCutSection(const FObjectInitializer& Init)
		: Super(Init)
	{
		EvalOptions.EnableAndSetCompletionMode(EMovieSceneCompletionMode::RestoreState);
	}

	/** Gets the camera guid for this CameraCut section */
	FGuid GetCameraGuid() const
	{
		return CameraGuid;
	}

	/** Sets the camera guid for this CameraCut section */
	void SetCameraGuid(const FGuid& InGuid)
	{
		CameraGuid = InGuid;
	}

	virtual FMovieSceneEvalTemplatePtr GenerateTemplate() const override;

	// UMovieSceneSection interface
	virtual TOptional<float> GetKeyTime(FKeyHandle KeyHandle) const override { return TOptional<float>(); }
	virtual void SetKeyTime(FKeyHandle KeyHandle, float Time) override { }
	virtual void OnBindingsUpdated(const TMap<FGuid, FGuid>& OldGuidToNewGuidMap) override;

private:

	/** The camera possessable or spawnable that this movie CameraCut uses */
	UPROPERTY()
	FGuid CameraGuid;

#if WITH_EDITORONLY_DATA
public:
	/** @return The thumbnail reference frame offset from the start of this section */
	float GetThumbnailReferenceOffset() const
	{
		return ThumbnailReferenceOffset;
	}

	/** Set the thumbnail reference offset */
	void SetThumbnailReferenceOffset(float InNewOffset)
	{
		Modify();
		ThumbnailReferenceOffset = InNewOffset;
	}

private:

	/** The reference frame offset for single thumbnail rendering */
	UPROPERTY()
	float ThumbnailReferenceOffset;
#endif
};
