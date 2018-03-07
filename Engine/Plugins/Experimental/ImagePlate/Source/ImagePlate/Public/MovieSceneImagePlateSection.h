// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MovieSceneSection.h"
#include "MovieSceneImagePlateSection.generated.h"

class UImagePlateFileSequence;

/**
 *
 */
UCLASS(MinimalAPI)
class UMovieSceneImagePlateSection
	: public UMovieSceneSection
{
public:

	GENERATED_BODY()

public:

	UMovieSceneImagePlateSection(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(EditAnywhere, Category="General")
	UImagePlateFileSequence* FileSequence;

	/** Specifies whether this section can reuse a texture or render target already specified on the property. When false, a dynamic texture 2D will be created at runtime and assigned to the property where possible. */
	UPROPERTY(EditAnywhere, Category="Image Plate")
	bool bReuseExistingTexture;

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
