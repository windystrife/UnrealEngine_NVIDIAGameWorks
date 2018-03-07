// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ImageWrapperBase.h"


/* FImageWrapperBase structors
 *****************************************************************************/

FImageWrapperBase::FImageWrapperBase()
	: RawFormat(ERGBFormat::Invalid)
	, RawBitDepth(0)
	, Format(ERGBFormat::Invalid)
	, BitDepth(0)
	, Width(0)
	, Height(0)
{ }


/* FImageWrapperBase interface
 *****************************************************************************/

void FImageWrapperBase::Reset()
{
	LastError.Empty();

	RawFormat = ERGBFormat::Invalid;
	RawBitDepth = 0;
	Format = ERGBFormat::Invalid;
	BitDepth = 0;
	Width = 0;
	Height = 0;
}


void FImageWrapperBase::SetError(const TCHAR* ErrorMessage)
{
	LastError = ErrorMessage;
}


/* IImageWrapper structors
 *****************************************************************************/

const TArray<uint8>& FImageWrapperBase::GetCompressed(int32 Quality)
{
	LastError.Empty();
	Compress(Quality);

	return CompressedData;
}


bool FImageWrapperBase::GetRaw(const ERGBFormat InFormat, int32 InBitDepth, const TArray<uint8>*& OutRawData)
{
	LastError.Empty();
	Uncompress(InFormat, InBitDepth);

	if (LastError.IsEmpty())
	{
		OutRawData = &RawData;
	}

	return LastError.IsEmpty();
}


bool FImageWrapperBase::SetCompressed(const void* InCompressedData, int32 InCompressedSize)
{
	if(InCompressedSize > 0 && InCompressedData != nullptr)
	{
		Reset();
		RawData.Empty();			// Invalidates the raw data too

		CompressedData.Empty(InCompressedSize);
		CompressedData.AddUninitialized(InCompressedSize);
		FMemory::Memcpy(CompressedData.GetData(), InCompressedData, InCompressedSize);

		return true;
	}

	return false;
}


bool FImageWrapperBase::SetRaw(const void* InRawData, int32 InRawSize, const int32 InWidth, const int32 InHeight, const ERGBFormat InFormat, const int32 InBitDepth)
{
	check(InRawData != NULL);
	check(InRawSize > 0);
	check(InWidth > 0);
	check(InHeight > 0);

	Reset();
	CompressedData.Empty();		// Invalidates the compressed data too

	RawData.Empty(InRawSize);
	RawData.AddUninitialized(InRawSize);
	FMemory::Memcpy(RawData.GetData(), InRawData, InRawSize);

	RawFormat = InFormat;
	RawBitDepth = InBitDepth;

	Width = InWidth;
	Height = InHeight;

	return true;
}
