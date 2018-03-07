// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FBitmapFileHeader;
struct FBitmapInfoHeader;

// Bitmap compression types.
enum EBitmapCompression
{
	BCBI_RGB       = 0,
	BCBI_RLE8      = 1,
	BCBI_RLE4      = 2,
	BCBI_BITFIELDS = 3,
};

// .BMP file header.
#pragma pack(push,1)
struct FBitmapFileHeader
{
	uint16 bfType;
	uint32 bfSize;
	uint16 bfReserved1;
	uint16 bfReserved2;
	uint32 bfOffBits;
	friend FArchive& operator<<( FArchive& Ar, FBitmapFileHeader& H )
	{
		Ar << H.bfType << H.bfSize << H.bfReserved1 << H.bfReserved2 << H.bfOffBits;
		return Ar;
	}
};
#pragma pack(pop)

// .BMP subheader.
#pragma pack(push,1)
struct FBitmapInfoHeader
{
	uint32 biSize;
	uint32 biWidth;
	int32 biHeight;
	uint16 biPlanes;
	uint16 biBitCount;
	uint32 biCompression;
	uint32 biSizeImage;
	uint32 biXPelsPerMeter;
	uint32 biYPelsPerMeter;
	uint32 biClrUsed;
	uint32 biClrImportant;
	friend FArchive& operator<<( FArchive& Ar, FBitmapInfoHeader& H )
	{
		Ar << H.biSize << H.biWidth << H.biHeight;
		Ar << H.biPlanes << H.biBitCount;
		Ar << H.biCompression << H.biSizeImage;
		Ar << H.biXPelsPerMeter << H.biYPelsPerMeter;
		Ar << H.biClrUsed << H.biClrImportant;
		return Ar;
	}
};
#pragma pack(pop)

