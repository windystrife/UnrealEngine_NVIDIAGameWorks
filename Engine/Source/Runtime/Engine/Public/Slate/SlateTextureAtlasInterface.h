// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Interface.h"
#include "Engine/Texture.h"
#include "SlateTextureAtlasInterface.generated.h"

/**
 * The data representing a region of a UTexture atlas.
 */
struct FSlateAtlasData
{
public:
	FSlateAtlasData(UTexture* InAtlasTexture, FVector2D InStartUV, FVector2D InSizeUV)
		: AtlasTexture(InAtlasTexture)
		, StartUV(InStartUV)
		, SizeUV(InSizeUV)
	{
	}

public:
	/** The texture pointer for the Atlas */
	UTexture* AtlasTexture;

	/** The region start position in UVs */
	FVector2D StartUV;

	/** The region size in UVs. */
	FVector2D SizeUV;

	/**
	 * Gets the dimensions of the atlas region in pixel coordinates.
	 */
	FVector2D GetSourceDimensions() const
	{
		if ( AtlasTexture )
		{
			return FVector2D(AtlasTexture->GetSurfaceWidth() * SizeUV.X, AtlasTexture->GetSurfaceHeight() * SizeUV.Y);
		}
		else
		{
			return FVector2D(0, 0);
		}
	}
};


/**
 * 
 */
UINTERFACE(meta=( CannotImplementInterfaceInBlueprint ))
class ENGINE_API USlateTextureAtlasInterface : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};

/**
 * 
 */
class ENGINE_API ISlateTextureAtlasInterface
{
	GENERATED_IINTERFACE_BODY()

public:

	/** Gets the atlas data to use when rendering with slate. */
	virtual FSlateAtlasData GetSlateAtlasData() const = 0;
};
