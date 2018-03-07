// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "IMediaTextureSample.h"
#include "Math/IntPoint.h"
#include "Templates/SharedPointer.h"


/**
 * Information about an image sequence frame.
 */
struct FImgMediaFrameInfo
{
	/** Name of the image compression algorithm (i.e. "ZIP"). */
	FString CompressionName;

	/** Width and height of the frame (in pixels). */
	FIntPoint Dim;

	/** Name of the image format (i.e. "EXR"). */
	FString FormatName;

	/** Frames per second. */
	float Fps;

	/** Whether the frame is in sRGB color space. */
	bool Srgb;

	/** Uncompressed size (in bytes). */
	SIZE_T UncompressedSize;
};


/**
 * A single frame of an image sequence.
 */
struct FImgMediaFrame
{
	/** The frame's data. */
	TSharedPtr<void, ESPMode::ThreadSafe> Data;

	/** The frame's sample format. */
	EMediaTextureSampleFormat Format;

	/** Additional information about the frame. */
	FImgMediaFrameInfo Info;

	/** The frame's horizontal stride (in bytes). */
	uint32 Stride;
};


/**
 * Interface for image sequence readers.
 */
class IImgMediaReader
{
public:

	/**
	 * Get information about an image sequence frame.
	 *
	 * @param ImagePath Path to the image file containing the frame.
	 * @param OutInfo Will contain the frame info.
	 * @return true on success, false otherwise.
	 * @see ReadFrame
	 */
	virtual bool GetFrameInfo(const FString& ImagePath, FImgMediaFrameInfo& OutInfo) = 0;

	/**
	 * Read a single image frame.
	 *
	 * @param ImagePath Path to the image file to read.
	 * @param OutFrame Will contain the frame.
	 * @return true on success, false otherwise.
	 * @see GetFrameInfo
	 */
	virtual bool ReadFrame(const FString& ImagePath, FImgMediaFrame& OutFrame) = 0;

public:

	/** Virtual destructor. */
	virtual ~IImgMediaReader() { }
};
