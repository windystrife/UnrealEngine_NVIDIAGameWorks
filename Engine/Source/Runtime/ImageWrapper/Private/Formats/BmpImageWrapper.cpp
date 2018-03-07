// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "BmpImageWrapper.h"
#include "ImageWrapperPrivate.h"

#include "BmpImageSupport.h"


/**
 * BMP image wrapper class.
 * This code was adapted from UTextureFactory::ImportTexture, but has not been throughly tested.
 */

FBmpImageWrapper::FBmpImageWrapper(bool bInHasHeader, bool bInHalfHeight)
	: FImageWrapperBase()
	, bHasHeader(bInHasHeader)
	, bHalfHeight(bInHalfHeight)
{
}


void FBmpImageWrapper::Compress(int32 Quality)
{
	checkf(false, TEXT("BMP compression not supported"));
}


void FBmpImageWrapper::Uncompress(const ERGBFormat InFormat, const int32 InBitDepth)
{
	const uint8* Buffer = CompressedData.GetData();

	if (!bHasHeader || ((CompressedData.Num()>=sizeof(FBitmapFileHeader)+sizeof(FBitmapInfoHeader)) && Buffer[0]=='B' && Buffer[1]=='M'))
	{
		UncompressBMPData(InFormat, InBitDepth);
	}
}


void FBmpImageWrapper::UncompressBMPData(const ERGBFormat InFormat, const int32 InBitDepth)
{
	const uint8* Buffer = CompressedData.GetData();
	const FBitmapInfoHeader* bmhdr = nullptr;
	const uint8* Bits = nullptr;

	if (bHasHeader)
	{
		bmhdr = (FBitmapInfoHeader *)(Buffer + sizeof(FBitmapFileHeader));
		Bits = Buffer + ((FBitmapFileHeader *)Buffer)->bfOffBits;
	}
	else
	{
		bmhdr = (FBitmapInfoHeader *)Buffer;
		Bits = Buffer + sizeof(FBitmapInfoHeader);
	}

	if (bmhdr->biCompression != BCBI_RGB)
	{
		UE_LOG(LogImageWrapper, Error, TEXT("RLE compression of BMP images not supported"));
		return;
	}

	if (bmhdr->biPlanes==1 && bmhdr->biBitCount==8)
	{
		// Do palette.
		const uint8* bmpal = (uint8*)CompressedData.GetData() + sizeof(FBitmapFileHeader) + sizeof(FBitmapInfoHeader);

		// Set texture properties.
		Width = bmhdr->biWidth;
		const bool bNegativeHeight = (bmhdr->biHeight < 0);
		Height = FMath::Abs(bHalfHeight ? bmhdr->biHeight / 2 : bmhdr->biHeight);
		Format = ERGBFormat::BGRA;
		RawData.Empty(Height * Width * 4);
		RawData.AddUninitialized(Height * Width * 4);

		FColor* ImageData = (FColor*)RawData.GetData();

		// If the number for color palette entries is 0, we need to default to 2^biBitCount entries.  In this case 2^8 = 256
		int32 clrPaletteCount = bmhdr->biClrUsed ? bmhdr->biClrUsed : 256;
		TArray<FColor>	Palette;

		for (int32 i = 0; i < clrPaletteCount; i++)
		{
			Palette.Add(FColor(bmpal[i * 4 + 2], bmpal[i * 4 + 1], bmpal[i * 4 + 0], 255));
		}

		while (Palette.Num() < 256)
		{
			Palette.Add(FColor(0, 0, 0, 255));
		}

		// Copy scanlines, accounting for scanline direction according to the Height field.
		const int32 SrcStride = Align(Width, 4);
		const int32 SrcPtrDiff = bNegativeHeight ? SrcStride : -SrcStride;
		const uint8* SrcPtr = Bits + (bNegativeHeight ? 0 : Height - 1) * SrcStride;

		for (int32 Y = 0; Y < Height; Y++)
		{
			for (int32 X = 0; X < Width; X++)
			{
				*ImageData++ = Palette[SrcPtr[X]];
			}

			SrcPtr += SrcPtrDiff;
		}
	}
	else if (bmhdr->biPlanes==1 && bmhdr->biBitCount==24)
	{
		// Set texture properties.
		Width = bmhdr->biWidth;
		const bool bNegativeHeight = (bmhdr->biHeight < 0);
		Height = FMath::Abs(bHalfHeight ? bmhdr->biHeight / 2 : bmhdr->biHeight);
		Format = ERGBFormat::BGRA;
		RawData.Empty(Height * Width * 4);
		RawData.AddUninitialized(Height * Width * 4);

		uint8* ImageData = RawData.GetData();

		// Copy scanlines, accounting for scanline direction according to the Height field.
		const int32 SrcStride = Align(Width * 3, 4);
		const int32 SrcPtrDiff = bNegativeHeight ? SrcStride : -SrcStride;
		const uint8* SrcPtr = Bits + (bNegativeHeight ? 0 : Height - 1) * SrcStride;

		for (int32 Y = 0; Y < Height; Y++)
		{
			const uint8* SrcRowPtr = SrcPtr;
			for (int32 X = 0; X < Width; X++)
			{
				*ImageData++ = *SrcRowPtr++;
				*ImageData++ = *SrcRowPtr++;
				*ImageData++ = *SrcRowPtr++;
				*ImageData++ = 0xFF;
			}

			SrcPtr += SrcPtrDiff;
		}
	}
	else if (bmhdr->biPlanes==1 && bmhdr->biBitCount==32)
	{
		// Set texture properties.
		Width = bmhdr->biWidth;
		const bool bNegativeHeight = (bmhdr->biHeight < 0);
		Height = FMath::Abs(bHalfHeight ? bmhdr->biHeight / 2 : bmhdr->biHeight);
		Format = ERGBFormat::BGRA;
		RawData.Empty(Height * Width * 4);
		RawData.AddUninitialized(Height * Width * 4);

		uint8* ImageData = RawData.GetData();

		// Copy scanlines, accounting for scanline direction according to the Height field.
		const int32 SrcStride = Width * 4;
		const int32 SrcPtrDiff = bNegativeHeight ? SrcStride : -SrcStride;
		const uint8* SrcPtr = Bits + (bNegativeHeight ? 0 : Height - 1) * SrcStride;

		for (int32 Y = 0; Y < Height; Y++)
		{
			const uint8* SrcRowPtr = SrcPtr;
			for (int32 X = 0; X < Width; X++)
			{
				*ImageData++ = *SrcRowPtr++;
				*ImageData++ = *SrcRowPtr++;
				*ImageData++ = *SrcRowPtr++;
				*ImageData++ = *SrcRowPtr++;
			}

			SrcPtr += SrcPtrDiff;
		}
	}
	else if (bmhdr->biPlanes==1 && bmhdr->biBitCount==16)
	{
		UE_LOG(LogImageWrapper, Error, TEXT("BMP 16 bit format no longer supported. Use terrain tools for importing/exporting heightmaps."));
	}
	else
	{
		UE_LOG(LogImageWrapper, Error, TEXT("BMP uses an unsupported format (%i/%i)"), bmhdr->biPlanes, bmhdr->biBitCount);
	}
}


bool FBmpImageWrapper::SetCompressed(const void* InCompressedData, int32 InCompressedSize)
{
	bool bResult = FImageWrapperBase::SetCompressed(InCompressedData, InCompressedSize);

	return bResult && (bHasHeader ? LoadBMPHeader() : LoadBMPInfoHeader());	// Fetch the variables from the header info
}


bool FBmpImageWrapper::LoadBMPHeader()
{
	const FBitmapInfoHeader* bmhdr = (FBitmapInfoHeader *)(CompressedData.GetData() + sizeof(FBitmapFileHeader));
	const FBitmapFileHeader* bmf   = (FBitmapFileHeader *)(CompressedData.GetData() + 0);
	if ((CompressedData.Num() >= sizeof(FBitmapFileHeader) + sizeof(FBitmapInfoHeader)) && CompressedData.GetData()[0] == 'B' && CompressedData.GetData()[1] == 'M')
	{
		if (bmhdr->biCompression != BCBI_RGB)
		{
			UE_LOG(LogImageWrapper, Error, TEXT("RLE compression of BMP images not supported"));
			return false;
		}

		if (bmhdr->biPlanes==1 && (bmhdr->biBitCount==8 || bmhdr->biBitCount==24 || bmhdr->biBitCount==32))
		{
			// Set texture properties.
			Width = bmhdr->biWidth;
			Height = FMath::Abs(bmhdr->biHeight);
			Format = ERGBFormat::BGRA;
			BitDepth = bmhdr->biBitCount;

			return true;
		}
		
		if (bmhdr->biPlanes == 1 && bmhdr->biBitCount == 16)
		{
			UE_LOG(LogImageWrapper, Error, TEXT("BMP 16 bit format no longer supported. Use terrain tools for importing/exporting heightmaps."));
		}
		else
		{
			UE_LOG(LogImageWrapper, Error, TEXT("BMP uses an unsupported format (%i/%i)"), bmhdr->biPlanes, bmhdr->biBitCount);
		}
	}

	return false;
}


bool FBmpImageWrapper::LoadBMPInfoHeader()
{
	const FBitmapInfoHeader* bmhdr = (FBitmapInfoHeader *)CompressedData.GetData();

	if (bmhdr->biCompression != BCBI_RGB)
	{
		UE_LOG(LogImageWrapper, Error, TEXT("RLE compression of BMP images not supported"));
		return false;
	}

	if (bmhdr->biPlanes==1 && (bmhdr->biBitCount==8 || bmhdr->biBitCount==24 || bmhdr->biBitCount==32))
	{
		// Set texture properties.
		Width = bmhdr->biWidth;
		Height = FMath::Abs(bmhdr->biHeight);
		Format = ERGBFormat::BGRA;
		BitDepth = bmhdr->biBitCount;

		return true;
	}
	
	if (bmhdr->biPlanes == 1 && bmhdr->biBitCount == 16)
	{
		UE_LOG(LogImageWrapper, Error, TEXT("BMP 16 bit format no longer supported. Use terrain tools for importing/exporting heightmaps."));
	}
	else
	{
		UE_LOG(LogImageWrapper, Error, TEXT("BMP uses an unsupported format (%i/%i)"), bmhdr->biPlanes, bmhdr->biBitCount);
	}

	return false;
}
