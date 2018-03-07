// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/**
 *
 * This thumbnail renderer holds some commonly shared properties
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "ThumbnailRendering/ThumbnailRenderer.h"
#include "DefaultSizedThumbnailRenderer.generated.h"

UCLASS(abstract, config=Editor, MinimalAPI)
class UDefaultSizedThumbnailRenderer : public UThumbnailRenderer
{
	GENERATED_UCLASS_BODY()

	/**
	 * The default width of this thumbnail
	 */
	UPROPERTY(config)
	int32 DefaultSizeX;

	/**
	 * The default height of this thumbnail
	 */
	UPROPERTY(config)
	int32 DefaultSizeY;


	// Begin UThumbnailRenderer Object
	UNREALED_API virtual void GetThumbnailSize(UObject* Object, float Zoom, uint32& OutWidth, uint32& OutHeight) const override;
	// End UThumbnailRenderer Object
};

