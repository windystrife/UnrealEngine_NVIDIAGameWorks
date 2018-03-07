// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "ImageWrapperBase.h"

#if WITH_UNREALPNG

THIRD_PARTY_INCLUDES_START
	#include "ThirdParty/zlib/zlib-1.2.5/Inc/zlib.h"
	#include "ThirdParty/libPNG/libPNG-1.5.2/png.h"
	#include "ThirdParty/libPNG/libPNG-1.5.2/pnginfo.h"
	#include <setjmp.h>
THIRD_PARTY_INCLUDES_END


/**
 * PNG implementation of the helper class.
 *
 * The implementation of this class is almost entirely based on the sample from the libPNG documentation.
 * See http://www.libpng.org/pub/png/libpng-1.2.5-manual.html for details.
 *	
 * InitCompressed and InitRaw will set initial state and you will then be able to fill in Raw or
 * CompressedData by calling Uncompress or Compress respectively.
 */
class FPngImageWrapper
	: public FImageWrapperBase
{
public:

	/** Default Constructor. */
	FPngImageWrapper();

public:

	//~ FImageWrapper interface

	virtual void Compress(int32 Quality) override;
	virtual void Reset() override;
	virtual bool SetCompressed(const void* InCompressedData, int32 InCompressedSize) override;
	virtual void Uncompress(const ERGBFormat InFormat, int32 InBitDepth) override;

public:

	/** 
	 * Query whether this is a valid PNG type.
	 *
	 * @return true if data a PNG
	 */
	bool IsPNG() const;

	/** 
	 * Load the header information, returns true if successful.
	 *
	 * @return true if successful
	 */
	bool LoadPNGHeader();

	/** Helper function used to uncompress PNG data from a buffer */
	void UncompressPNGData(const ERGBFormat InFormat, const int32 InBitDepth);

protected:

	// Callbacks for the pnglibs
	static void  user_read_compressed(png_structp png_ptr, png_bytep data, png_size_t length);
	static void  user_write_compressed(png_structp png_ptr, png_bytep data, png_size_t length);
	static void  user_flush_data(png_structp png_ptr);
	static void  user_error_fn(png_structp png_ptr, png_const_charp error_msg);
	static void  user_warning_fn(png_structp png_ptr, png_const_charp warning_msg);
	static void* user_malloc(png_structp png_ptr, png_size_t size);
	static void  user_free(png_structp png_ptr, png_voidp struct_ptr);

private:

	/** The read offset into our array. */
	int32 ReadOffset;

	/** The color type as defined in the header. */
	int32 ColorType;

	/** The number of channels. */
	uint8 Channels;

	/** setjmp buffer for error recovery. */
	jmp_buf SetjmpBuffer;
};


#endif	//WITH_UNREALPNG
