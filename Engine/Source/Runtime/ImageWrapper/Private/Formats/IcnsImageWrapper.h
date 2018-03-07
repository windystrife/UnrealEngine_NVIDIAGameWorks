// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ImageWrapperBase.h"


/**
 * ICNS implementation of the helper class.
 */
class FIcnsImageWrapper
	: public FImageWrapperBase
{
public:

	/** Default Constructor. */
	FIcnsImageWrapper();

public:

	//~ FImageWrapper Interface

	virtual bool SetCompressed(const void* InCompressedData, int32 InCompressedSize) override;
	virtual bool SetRaw(const void* InRawData, int32 InRawSize, const int32 InWidth, const int32 InHeight, const ERGBFormat InFormat, const int32 InBitDepth) override;
	virtual void Compress(int32 Quality) override;
	virtual void Uncompress(const ERGBFormat InFormat, int32 InBitDepth) override;
};
