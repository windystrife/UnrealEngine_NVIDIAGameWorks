// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "ImageWrapperBase.h"

#if WITH_UNREALJPEG


/**
 * Uncompresses JPEG data to raw 24bit RGB image that can be used by Unreal textures.
 *
 * Source code for JPEG decompression from http://code.google.com/p/jpeg-compressor/
 */
class FJpegImageWrapper
	: public FImageWrapperBase
{
public:

	/** Default constructor. */
	FJpegImageWrapper(int32 InNumComponents = 4);

public:

	//~ FImageWrapperBase interface

	virtual bool SetCompressed(const void* InCompressedData, int32 InCompressedSize) override;
	virtual bool SetRaw(const void* InRawData, int32 InRawSize, const int32 InWidth, const int32 InHeight, const ERGBFormat InFormat, const int32 InBitDepth) override;
	virtual void Uncompress(const ERGBFormat InFormat, int32 InBitDepth) override;
	virtual void Compress(int32 Quality) override;

private:

	int32 NumComponents;
};


#endif //WITH_JPEG
