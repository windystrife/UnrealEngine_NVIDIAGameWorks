// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"

// http://radsite.lbl.gov/radiance/refer/Notes/picture_format.html
// http://paulbourke.net/dataformats/pic/

/** To load the HDR file image format. Does not support all possible types HDR formats (e.g. xyze is not supported) */
class FHDRLoadHelper
{
public:
	FHDRLoadHelper(const uint8* Buffer, uint32 BufferLength);

	bool IsValid() const;

	uint32 GetWidth() const;

	uint32 GetHeight() const;

	/** @param OutDDSFile order in bytes: RGBE */
	void ExtractDDSInRGBE(TArray<uint8>& OutDDSFile) const;

private:
	/** 0 if not valid */
	const uint8* RGBDataStart;
	/** 0 if not valid */
	uint32 Width;
	/** 0 if not valid */
	uint32 Height;

	/** @param OutData[Width * Height * 4] will be filled in this order: R,G,B,0, R,G,B,0,... */
	void DecompressWholeImage(uint32* OutData) const;

	void GetHeaderLine(const uint8*& BufferPos, char Line[256]) const;

	/** @param Out order in bytes: RGBE */
	void DecompressScanline(uint8* Out, const uint8*& In) const;

	static void OldDecompressScanline(uint8* Out, const uint8*& In, uint32 Len);
};
