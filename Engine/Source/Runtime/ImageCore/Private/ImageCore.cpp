// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ImageCore.h"
#include "Modules/ModuleManager.h"


IMPLEMENT_MODULE(FDefaultModuleImpl, ImageCore);


/* Local helper functions
 *****************************************************************************/

/**
 * Initializes storage for an image.
 *
 * @param Image - The image to initialize storage for.
 */
static void InitImageStorage(FImage& Image)
{
	int32 NumBytes = Image.SizeX * Image.SizeY * Image.NumSlices * Image.GetBytesPerPixel();
	Image.RawData.Empty(NumBytes);
	Image.RawData.AddUninitialized(NumBytes);
}


/**
 * Copies an image accounting for format differences. Sizes must match.
 *
 * @param SrcImage - The source image to copy from.
 * @param DestImage - The destination image to copy to.
 */
static void CopyImage(const FImage& SrcImage, FImage& DestImage)
{
	check(SrcImage.SizeX == DestImage.SizeX);
	check(SrcImage.SizeY == DestImage.SizeY);
	check(SrcImage.NumSlices == DestImage.NumSlices);

	const int32 NumTexels = SrcImage.SizeX * SrcImage.SizeY * SrcImage.NumSlices;
	
	if (SrcImage.Format == DestImage.Format &&
		SrcImage.GammaSpace == DestImage.GammaSpace)
	{
		DestImage.RawData = SrcImage.RawData;
	}
	else if (SrcImage.Format == ERawImageFormat::RGBA32F)
	{
		// Convert from 32-bit linear floating point.
		const FLinearColor* SrcColors = SrcImage.AsRGBA32F();
	
		switch (DestImage.Format)
		{
		case ERawImageFormat::G8:
			{
				uint8* DestLum = DestImage.AsG8();
				for (int32 TexelIndex = 0; TexelIndex < NumTexels; ++TexelIndex)
				{
					DestLum[TexelIndex] = SrcColors[TexelIndex].ToFColor(DestImage.IsGammaCorrected()).R;
				}
			}
			break;
		
		case ERawImageFormat::BGRA8:
			{
				FColor* DestColors = DestImage.AsBGRA8();
				for (int32 TexelIndex = 0; TexelIndex < NumTexels; ++TexelIndex)
				{
					DestColors[TexelIndex] = SrcColors[TexelIndex].ToFColor(DestImage.IsGammaCorrected());
				}
			}
			break;
		
		case ERawImageFormat::BGRE8:
			{
				FColor* DestColors = DestImage.AsBGRE8();
				for (int32 TexelIndex = 0; TexelIndex < NumTexels; ++TexelIndex)
				{
					DestColors[TexelIndex] = SrcColors[TexelIndex].ToRGBE();
				}
			}
			break;
		
		case ERawImageFormat::RGBA16:
			{
				uint16* DestColors = DestImage.AsRGBA16();
				for (int32 TexelIndex = 0; TexelIndex < NumTexels; ++TexelIndex)
				{
					int32 DestIndex = TexelIndex * 4;
					DestColors[DestIndex + 0] = FMath::Clamp(FMath::FloorToInt(SrcColors[TexelIndex].R * 65535.999f), 0, 65535);
					DestColors[DestIndex + 1] = FMath::Clamp(FMath::FloorToInt(SrcColors[TexelIndex].G * 65535.999f), 0, 65535);
					DestColors[DestIndex + 2] = FMath::Clamp(FMath::FloorToInt(SrcColors[TexelIndex].B * 65535.999f), 0, 65535);
					DestColors[DestIndex + 3] = FMath::Clamp(FMath::FloorToInt(SrcColors[TexelIndex].A * 65535.999f), 0, 65535);
				}
			}
			break;
		
		case ERawImageFormat::RGBA16F:
			{
				FFloat16Color* DestColors = DestImage.AsRGBA16F();
				for (int32 TexelIndex = 0; TexelIndex < NumTexels; ++TexelIndex)
				{
					DestColors[TexelIndex] = FFloat16Color(SrcColors[TexelIndex]);
				}
			}
			break;
		}
	}
	else if (DestImage.Format == ERawImageFormat::RGBA32F)
	{
		// Convert to 32-bit linear floating point.
		FLinearColor* DestColors = DestImage.AsRGBA32F();
		switch (SrcImage.Format)
		{
		case ERawImageFormat::G8:
			{
				const uint8* SrcLum = SrcImage.AsG8();
				for (int32 TexelIndex = 0; TexelIndex < NumTexels; ++TexelIndex)
				{
					FColor SrcColor(SrcLum[TexelIndex],SrcLum[TexelIndex],SrcLum[TexelIndex],255);
					
					switch ( SrcImage.GammaSpace )
					{
					case EGammaSpace::Linear:
						DestColors[TexelIndex] = SrcColor.ReinterpretAsLinear();
						break;
					case EGammaSpace::sRGB:
						DestColors[TexelIndex] = FLinearColor(SrcColor);
						break;
					case EGammaSpace::Pow22:
						DestColors[TexelIndex] = FLinearColor::FromPow22Color(SrcColor);
						break;
					}
				}
			}
			break;

		case ERawImageFormat::BGRA8:
			{
				const FColor* SrcColors = SrcImage.AsBGRA8();
				for (int32 TexelIndex = 0; TexelIndex < NumTexels; ++TexelIndex)
				{
					switch ( SrcImage.GammaSpace )
					{
					case EGammaSpace::Linear:
						DestColors[TexelIndex] = SrcColors[TexelIndex].ReinterpretAsLinear();
						break;
					case EGammaSpace::sRGB:
						DestColors[TexelIndex] = FLinearColor(SrcColors[TexelIndex]);
						break;
					case EGammaSpace::Pow22:
						DestColors[TexelIndex] = FLinearColor::FromPow22Color(SrcColors[TexelIndex]);
						break;
					}
				}
			}
			break;

		case ERawImageFormat::BGRE8:
			{
				const FColor* SrcColors = SrcImage.AsBGRE8();
				for (int32 TexelIndex = 0; TexelIndex < NumTexels; ++TexelIndex)
				{
					DestColors[TexelIndex] = SrcColors[TexelIndex].FromRGBE();
				}
			}
			break;

		case ERawImageFormat::RGBA16:
			{
				const uint16* SrcColors = SrcImage.AsRGBA16();
				for (int32 TexelIndex = 0; TexelIndex < NumTexels; ++TexelIndex)
				{
					int32 SrcIndex = TexelIndex * 4;
					DestColors[TexelIndex] = FLinearColor(
						SrcColors[SrcIndex + 0] / 65535.0f,
						SrcColors[SrcIndex + 1] / 65535.0f,
						SrcColors[SrcIndex + 2] / 65535.0f,
						SrcColors[SrcIndex + 3] / 65535.0f
						);
				}
			}
			break;

		case ERawImageFormat::RGBA16F:
			{
				const FFloat16Color* SrcColors = SrcImage.AsRGBA16F();
				for (int32 TexelIndex = 0; TexelIndex < NumTexels; ++TexelIndex)
				{
					DestColors[TexelIndex] = FLinearColor(SrcColors[TexelIndex]);
				}
			}
			break;
		}
	}
	else
	{
		// Arbitrary conversion, use 32-bit linear float as an intermediate format.
		FImage TempImage(SrcImage.SizeX, SrcImage.SizeY, ERawImageFormat::RGBA32F);
		CopyImage(SrcImage, TempImage);
		CopyImage(TempImage, DestImage);
	}
}


/* FImage structors
 *****************************************************************************/

FImage::FImage(int32 InSizeX, int32 InSizeY, int32 InNumSlices, ERawImageFormat::Type InFormat, EGammaSpace InGammaSpace)
	: SizeX(InSizeX)
	, SizeY(InSizeY)
	, NumSlices(InNumSlices)
	, Format(InFormat)
	, GammaSpace(InGammaSpace)
{
	InitImageStorage(*this);
}


FImage::FImage(int32 InSizeX, int32 InSizeY, ERawImageFormat::Type InFormat, EGammaSpace InGammaSpace)
	: SizeX(InSizeX)
	, SizeY(InSizeY)
	, NumSlices(1)
	, Format(InFormat)
	, GammaSpace(InGammaSpace)
{
	InitImageStorage(*this);
}


void FImage::Init(int32 InSizeX, int32 InSizeY, int32 InNumSlices, ERawImageFormat::Type InFormat, EGammaSpace InGammaSpace)
{
	SizeX = InSizeX;
	SizeY = InSizeY;
	NumSlices = InNumSlices;
	Format = InFormat;
	GammaSpace = InGammaSpace;
	InitImageStorage(*this);
}


void FImage::Init(int32 InSizeX, int32 InSizeY, ERawImageFormat::Type InFormat, EGammaSpace InGammaSpace)
{
	SizeX = InSizeX;
	SizeY = InSizeY;
	NumSlices = 1;
	Format = InFormat;
	GammaSpace = InGammaSpace;
	InitImageStorage(*this);
}


/* FImage interface
 *****************************************************************************/

void FImage::CopyTo(FImage& DestImage, ERawImageFormat::Type DestFormat, EGammaSpace DestGammaSpace) const
{
	DestImage.SizeX = SizeX;
	DestImage.SizeY = SizeY;
	DestImage.NumSlices = NumSlices;
	DestImage.Format = DestFormat;
	DestImage.GammaSpace = DestGammaSpace;
	InitImageStorage(DestImage);
	CopyImage(*this, DestImage);
}


int32 FImage::GetBytesPerPixel() const
{
	int32 BytesPerPixel = 0;
	switch (Format)
	{
	case ERawImageFormat::G8:
		BytesPerPixel = 1;
		break;

	case ERawImageFormat::BGRA8:
	case ERawImageFormat::BGRE8:
		BytesPerPixel = 4;
		break;
			
	case ERawImageFormat::RGBA16:
	case ERawImageFormat::RGBA16F:
		BytesPerPixel = 8;
		break;

	case ERawImageFormat::RGBA32F:
		BytesPerPixel = 16;
		break;
	}
	return BytesPerPixel;
}
