// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once


/* Dependencies
 *****************************************************************************/

#include "CoreMinimal.h"

/* Types
 *****************************************************************************/

namespace ERawImageFormat
{
	/**
	 * Enumerates supported raw image formats.
	 */
	enum Type
	{
		G8,
		BGRA8,
		BGRE8,
		RGBA16,
		RGBA16F,
		RGBA32F
	};
};


/**
 * Structure for raw image data.
 */
struct FImage
{
	/** Raw image data. */
	TArray<uint8> RawData;

	/** Width of the image. */
	int32 SizeX;
	
	/** Height of the image. */
	int32 SizeY;
	
	/** Number of image slices. */
	int32 NumSlices;
	
	/** Format in which the image is stored. */
	ERawImageFormat::Type Format;
	
	/** The gamma space the image is stored in. */
	EGammaSpace GammaSpace;

public:

	/**
	 * Default constructor.
	 */
	FImage() { }

	/**
	 * Creates and initializes a new image with the specified number of slices.
	 *
	 * @param InSizeX - The image width.
	 * @param InSizeY - The image height.
	 * @param InNumSlices - The number of slices.
	 * @param InFormat - The image format.
	 * @param bInSRGB - Whether the color values are in SRGB format.
	 */
	IMAGECORE_API FImage(int32 InSizeX, int32 InSizeY, int32 InNumSlices, ERawImageFormat::Type InFormat, EGammaSpace InGammaSpace = EGammaSpace::Linear);

	/**
	 * Creates and initializes a new image with a single slice.
	 *
	 * @param InSizeX - The image width.
	 * @param InSizeY - The image height.
	 * @param InFormat - The image format.
	 * @param bInSRGB - Whether the color values are in SRGB format.
	 */
	IMAGECORE_API FImage(int32 InSizeX, int32 InSizeY, ERawImageFormat::Type InFormat, EGammaSpace InGammaSpace = EGammaSpace::Linear);


public:

	/**
	 * Copies the image to a destination image with the specified format.
	 *
	 * @param DestImage - The destination image.
	 * @param DestFormat - The destination image format.
	 * @param DestSRGB - Whether the destination image is in SRGB format.
	 */
	IMAGECORE_API void CopyTo(FImage& DestImage, ERawImageFormat::Type DestFormat, EGammaSpace DestGammaSpace) const;

	/**
	 * Gets the number of bytes per pixel.
	 *
	 * @return Bytes per pixel.
	 */
	IMAGECORE_API int32 GetBytesPerPixel() const;

	/**
	 * Initializes this image with the specified number of slices.
	 *
	 * @param InSizeX - The image width.
	 * @param InSizeY - The image height.
	 * @param InNumSlices - The number of slices.
	 * @param InFormat - The image format.
	 * @param bInSRGB - Whether the color values are in SRGB format.
	 */
	IMAGECORE_API void Init(int32 InSizeX, int32 InSizeY, int32 InNumSlices, ERawImageFormat::Type InFormat, EGammaSpace InGammaSpace = EGammaSpace::Linear);

	/**
	 * Initializes this image with a single slice.
	 *
	 * @param InSizeX - The image width.
	 * @param InSizeY - The image height.
	 * @param InFormat - The image format.
	 * @param bInSRGB - Whether the color values are in SRGB format.
	 */
	IMAGECORE_API void Init(int32 InSizeX, int32 InSizeY, ERawImageFormat::Type InFormat, EGammaSpace InGammaSpace = EGammaSpace::Linear);


public:

	// Convenience accessors to raw data
	
	uint8* AsG8()
	{
		check(Format == ERawImageFormat::G8);
		return RawData.GetData();
	}

	struct FColor* AsBGRA8()
	{
		check(Format == ERawImageFormat::BGRA8);
		return (struct FColor*)RawData.GetData();
	}

	struct FColor* AsBGRE8()
	{
		check(Format == ERawImageFormat::BGRE8);
		return (struct FColor*)RawData.GetData();
	}

	uint16* AsRGBA16()
	{
		check(Format == ERawImageFormat::RGBA16);
		return (uint16*)RawData.GetData();
	}

	class FFloat16Color* AsRGBA16F()
	{
		check(Format == ERawImageFormat::RGBA16F);
		return (class FFloat16Color*)RawData.GetData();
	}

	struct FLinearColor* AsRGBA32F()
	{
		check(Format == ERawImageFormat::RGBA32F);
		return (struct FLinearColor*)RawData.GetData();
	}

	// Convenience accessors to const raw data

	const uint8* AsG8() const
	{
		check(Format == ERawImageFormat::G8);
		return RawData.GetData();
	}

	const struct FColor* AsBGRA8() const
	{
		check(Format == ERawImageFormat::BGRA8);
		return (const struct FColor*)RawData.GetData();
	}

	const struct FColor* AsBGRE8() const
	{
		check(Format == ERawImageFormat::BGRE8);
		return (struct FColor*)RawData.GetData();
	}

	const uint16* AsRGBA16() const
	{
		check(Format == ERawImageFormat::RGBA16);
		return (const uint16*)RawData.GetData();
	}

	const class FFloat16Color* AsRGBA16F() const
	{
		check(Format == ERawImageFormat::RGBA16F);
		return (const class FFloat16Color*)RawData.GetData();
	}

	const struct FLinearColor* AsRGBA32F() const
	{
		check(Format == ERawImageFormat::RGBA32F);
		return (struct FLinearColor*)RawData.GetData();
	}

	FORCEINLINE bool IsGammaCorrected() const
	{
		return GammaSpace != EGammaSpace::Linear;
	}
};
