// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/Margin.h"
#include "Styling/SlateBrush.h"

/**
 * Ignores the Margin. Just renders the image. Can tile the image instead of stretching.
 */
struct SLATECORE_API FSlateImageBrush
	: public FSlateBrush
{
	/**
	 * @param InImageName	The name of the texture to draw
	 * @param InImageSize	How large should the image be (not necessarily the image size on disk)
	 * @param InTint		The tint of the image.
	 * @param InTiling		How do we tile if at all?
	 * @param InImageType	The type of image this this is
	 */
	FORCENOINLINE FSlateImageBrush( const FName& InImageName, const FVector2D& InImageSize, const FLinearColor& InTint = FLinearColor(1,1,1,1), ESlateBrushTileType::Type InTiling = ESlateBrushTileType::NoTile, ESlateBrushImageType::Type InImageType = ESlateBrushImageType::FullColor )
		: FSlateBrush(ESlateBrushDrawType::Image, InImageName, FMargin(0), InTiling, InImageType, InImageSize, InTint)
	{ }

	FORCENOINLINE FSlateImageBrush( const FString& InImageName, const FVector2D& InImageSize, const FLinearColor& InTint = FLinearColor(1,1,1,1), ESlateBrushTileType::Type InTiling = ESlateBrushTileType::NoTile, ESlateBrushImageType::Type InImageType = ESlateBrushImageType::FullColor )
		: FSlateBrush(ESlateBrushDrawType::Image, *InImageName, FMargin(0), InTiling, InImageType, InImageSize, InTint)
	{ }

	FORCENOINLINE FSlateImageBrush( const ANSICHAR* InImageName, const FVector2D& InImageSize, const FLinearColor& InTint = FLinearColor(1,1,1,1), ESlateBrushTileType::Type InTiling = ESlateBrushTileType::NoTile, ESlateBrushImageType::Type InImageType = ESlateBrushImageType::FullColor )
		: FSlateBrush(ESlateBrushDrawType::Image, InImageName, FMargin(0), InTiling, InImageType, InImageSize, InTint)
	{ }

	FORCENOINLINE FSlateImageBrush( const WIDECHAR* InImageName, const FVector2D& InImageSize, const FLinearColor& InTint = FLinearColor(1,1,1,1), ESlateBrushTileType::Type InTiling = ESlateBrushTileType::NoTile, ESlateBrushImageType::Type InImageType = ESlateBrushImageType::FullColor )
		: FSlateBrush(ESlateBrushDrawType::Image, InImageName, FMargin(0), InTiling, InImageType, InImageSize, InTint)
	{ }

	FORCENOINLINE FSlateImageBrush( const FName& InImageName, const FVector2D& InImageSize, const TSharedRef< FLinearColor >& InTint, ESlateBrushTileType::Type InTiling = ESlateBrushTileType::NoTile, ESlateBrushImageType::Type InImageType = ESlateBrushImageType::FullColor )
		: FSlateBrush(ESlateBrushDrawType::Image, InImageName, FMargin(0), InTiling, InImageType, InImageSize, InTint)
	{ }

	FORCENOINLINE FSlateImageBrush( const FString& InImageName, const FVector2D& InImageSize, const TSharedRef< FLinearColor >& InTint, ESlateBrushTileType::Type InTiling = ESlateBrushTileType::NoTile, ESlateBrushImageType::Type InImageType = ESlateBrushImageType::FullColor )
		: FSlateBrush(ESlateBrushDrawType::Image, *InImageName, FMargin(0), InTiling, InImageType, InImageSize, InTint)
	{ }

	FORCENOINLINE FSlateImageBrush( const ANSICHAR* InImageName, const FVector2D& InImageSize, const TSharedRef< FLinearColor >& InTint, ESlateBrushTileType::Type InTiling = ESlateBrushTileType::NoTile, ESlateBrushImageType::Type InImageType = ESlateBrushImageType::FullColor )
		: FSlateBrush(ESlateBrushDrawType::Image, InImageName, FMargin(0), InTiling, InImageType, InImageSize, InTint)
	{ }

	FORCENOINLINE FSlateImageBrush( const TCHAR* InImageName, const FVector2D& InImageSize, const TSharedRef< FLinearColor >& InTint, ESlateBrushTileType::Type InTiling = ESlateBrushTileType::NoTile, ESlateBrushImageType::Type InImageType = ESlateBrushImageType::FullColor )
		: FSlateBrush(ESlateBrushDrawType::Image, InImageName, FMargin(0), InTiling, InImageType, InImageSize, InTint)
	{ }

	FORCENOINLINE FSlateImageBrush( const FName& InImageName, const FVector2D& InImageSize, const FSlateColor& InTint, ESlateBrushTileType::Type InTiling = ESlateBrushTileType::NoTile, ESlateBrushImageType::Type InImageType = ESlateBrushImageType::FullColor )
		: FSlateBrush(ESlateBrushDrawType::Image, InImageName, FMargin(0), InTiling, InImageType, InImageSize, InTint)
	{ }

	FORCENOINLINE FSlateImageBrush( const FString& InImageName, const FVector2D& InImageSize, const FSlateColor& InTint, ESlateBrushTileType::Type InTiling = ESlateBrushTileType::NoTile, ESlateBrushImageType::Type InImageType = ESlateBrushImageType::FullColor )
		: FSlateBrush(ESlateBrushDrawType::Image, *InImageName, FMargin(0), InTiling, InImageType, InImageSize, InTint)
	{ }

	FORCENOINLINE FSlateImageBrush( const ANSICHAR* InImageName, const FVector2D& InImageSize, const FSlateColor& InTint, ESlateBrushTileType::Type InTiling = ESlateBrushTileType::NoTile, ESlateBrushImageType::Type InImageType = ESlateBrushImageType::FullColor )
		: FSlateBrush(ESlateBrushDrawType::Image, InImageName, FMargin(0), InTiling, InImageType, InImageSize, InTint)
	{ }

	FORCENOINLINE FSlateImageBrush( const TCHAR* InImageName, const FVector2D& InImageSize, const FSlateColor& InTint, ESlateBrushTileType::Type InTiling = ESlateBrushTileType::NoTile, ESlateBrushImageType::Type InImageType = ESlateBrushImageType::FullColor )
		: FSlateBrush(ESlateBrushDrawType::Image, InImageName, FMargin(0), InTiling, InImageType, InImageSize, InTint)
	{ }

	/**
	 * @param InResourceObject	The image to render for this brush, can be a UTexture, UMaterialInterface, or AtlasedTextureInterface
	 * @param InImageSize		How large should the image be (not necessarily the image size on disk)
	 * @param InTint			The tint of the image.
	 * @param InTiling			How do we tile if at all?
	 * @param InImageType		The type of image this this is
	 */

	FORCENOINLINE FSlateImageBrush(UObject* InResourceObject, const FVector2D& InImageSize, const FSlateColor& InTint = FSlateColor(FLinearColor(1, 1, 1, 1)), ESlateBrushTileType::Type InTiling = ESlateBrushTileType::NoTile, ESlateBrushImageType::Type InImageType = ESlateBrushImageType::FullColor)
		: FSlateBrush(ESlateBrushDrawType::Image, NAME_None, FMargin(0), InTiling, InImageType, InImageSize, InTint, InResourceObject)
	{ }

};
