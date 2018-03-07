// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "IcnsImageWrapper.h"


/* FIcnsImageWrapper structors
 *****************************************************************************/

FIcnsImageWrapper::FIcnsImageWrapper()
	: FImageWrapperBase()
{ }


/* FImageWrapper interface
 *****************************************************************************/

bool FIcnsImageWrapper::SetCompressed(const void* InCompressedData, int32 InCompressedSize)
{
#if PLATFORM_MAC
	return FImageWrapperBase::SetCompressed(InCompressedData, InCompressedSize);
#else
	return false;
#endif
}


bool FIcnsImageWrapper::SetRaw(const void* InRawData, int32 InRawSize, const int32 InWidth, const int32 InHeight, const ERGBFormat InFormat, const int32 InBitDepth)
{
#if PLATFORM_MAC
	return FImageWrapperBase::SetRaw(InRawData, InRawSize, InWidth, InHeight, InFormat, InBitDepth);
#else
	return false;
#endif
}


void FIcnsImageWrapper::Compress(int32 Quality)
{
	checkf(false, TEXT("ICNS compression not supported"));
}


void FIcnsImageWrapper::Uncompress(const ERGBFormat InFormat, const int32 InBitDepth)
{
#if PLATFORM_MAC
	SCOPED_AUTORELEASE_POOL;

	NSData* ImageData = [NSData dataWithBytesNoCopy:CompressedData.GetData() length:CompressedData.Num() freeWhenDone:NO];
	NSImage* Image = [[NSImage alloc] initWithData:ImageData];
	if (Image)
	{
		NSBitmapImageRep* Bitmap = [NSBitmapImageRep imageRepWithData:[Image TIFFRepresentation]];
		if (Bitmap)
		{
			check(InFormat == ERGBFormat::BGRA || InFormat == ERGBFormat::RGBA);
			check(InBitDepth == 8);

			RawData.Empty();
			RawData.Append([Bitmap bitmapData], [Bitmap bytesPerPlane]);

			RawFormat = Format = InFormat;
			RawBitDepth = BitDepth = InBitDepth;

			Width = [Bitmap pixelsWide];
			Height = [Bitmap pixelsHigh];

			if (Format == ERGBFormat::BGRA)
			{
				for (int32 Index = 0; Index < [Bitmap bytesPerPlane]; Index += 4)
				{
					uint8 Byte = RawData[Index];
					RawData[Index] = RawData[Index + 2];
					RawData[Index + 2] = Byte;
				}
			}
		}
		[Image release];
	}
#else
	checkf(false, TEXT("ICNS uncompressing not supported on this platform"));
#endif
}
