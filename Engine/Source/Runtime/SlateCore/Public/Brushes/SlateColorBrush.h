// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/Margin.h"
#include "Styling/SlateBrush.h"

/**
 * A color brush.  Draws a box with no margins that only has a color applied to it                   
 */
struct SLATECORE_API FSlateColorBrush
	: public FSlateBrush
{
	/** 
	 * Creates and initializes a new instance with the specified linear color.
	 *
	 * @param InColor The linear color to use
	 */
	FORCENOINLINE FSlateColorBrush( const FLinearColor& InColor )
		: FSlateBrush(ESlateBrushDrawType::Box, NAME_None, FMargin(0.0f), ESlateBrushTileType::NoTile, ESlateBrushImageType::NoImage, FVector2D::ZeroVector, InColor)
	{ }

	/** 
	 * Creates and initializes a new instance with the specified color value.
	 *
	 * @param InColor The color value to use
	 */
	FORCENOINLINE FSlateColorBrush( const FColor& InColor )
		: FSlateBrush(ESlateBrushDrawType::Box, NAME_None, FMargin(0.0f), ESlateBrushTileType::NoTile, ESlateBrushImageType::NoImage, FVector2D::ZeroVector, InColor.ReinterpretAsLinear())
	{ }

	/** 
	 * Creates and initializes a new instance with the specified shared color.
	 *
	 * @param InColor The shared color to use
	 */
	FORCENOINLINE FSlateColorBrush( const TSharedRef< FLinearColor >& InColor )
		: FSlateBrush(ESlateBrushDrawType::Box, NAME_None, FMargin(0.0f), ESlateBrushTileType::NoTile, ESlateBrushImageType::NoImage, FVector2D::ZeroVector, InColor)
	{ }
};
