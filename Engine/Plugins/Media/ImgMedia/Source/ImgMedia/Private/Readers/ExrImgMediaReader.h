// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ImgMediaPrivate.h"

#if IMGMEDIA_EXR_SUPPORTED_PLATFORM

#include "IImgMediaReader.h"

class FRgbaInputFile;


/**
 * Implements a reader for EXR image sequences.
 */
class FExrImgMediaReader
	: public IImgMediaReader
{
public:

	/** Default constructor. */
	FExrImgMediaReader();

public:

	//~ IImgMediaReader interface

	virtual bool GetFrameInfo(const FString& ImagePath, FImgMediaFrameInfo& OutInfo) override;
	virtual bool ReadFrame(const FString& ImagePath, FImgMediaFrame& OutFrame) override;

protected:

	/**
	 * Get the frame information from the given input file.
	 *
	 * @param InputFile The input file containing the frame.
	 * @param OutInfo Will contain the frame information.
	 * @return true on success, false otherwise.
	 */
	bool GetInfo(FRgbaInputFile& InputFile, FImgMediaFrameInfo& OutInfo) const;
};


#endif //IMGMEDIA_EXR_SUPPORTED_PLATFORM
