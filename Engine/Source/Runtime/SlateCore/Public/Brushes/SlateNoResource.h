// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/Margin.h"
#include "Styling/SlateBrush.h"

/**
 * A resource that has no appearance
 */
struct SLATECORE_API FSlateNoResource
	: public FSlateBrush
{
	/**
	 * Creates and initializes a new instance.
	 *
	 * @param InImageSize An optional image size (default is zero).
	 */
	FORCENOINLINE FSlateNoResource( const FVector2D& InImageSize = FVector2D::ZeroVector )
		: FSlateBrush(ESlateBrushDrawType::NoDrawType, FName(NAME_None), FMargin(0), ESlateBrushTileType::NoTile, ESlateBrushImageType::NoImage, InImageSize)
	{ }
};
