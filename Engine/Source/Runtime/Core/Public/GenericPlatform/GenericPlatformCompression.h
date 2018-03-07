// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


/*=============================================================================================
GenericPlatformCompression.h: Generic platform compress and decompress.
==============================================================================================*/

#pragma once

#include "CoreTypes.h"
#include "Misc/Compression.h"

/**
* Interface for platform specific compression routines
**/
class CORE_API IPlatformCompression
{
public:

	/** Virtual destructor */
	virtual ~IPlatformCompression() {}

	/**
	 * Gets the bit window for compressor for this platform.
	 *
	 * @return Compression bit window.
	 */
	virtual int32 GetCompressionBitWindow() const = 0;
	
	/**
	* Thread-safe abstract compression routine to query memory requirements for a compression operation.
	* @param	Flags						Flags to control what method to use and optionally control memory vs speed
	* @param	UncompressedSize			Size of uncompressed data in bytes
	* @return The maximum possible bytes needed for compression of data buffer of size UncompressedSize
	**/
	virtual int32 CompressMemoryBound(ECompressionFlags Flags, int32 UncompressedSize, int32 BitWindow) = 0;

	/**
	* Thread-safe abstract compression routine. Compresses memory from uncompressed buffer and writes it to compressed
	* buffer. Updates CompressedSize with size of compressed data. Compression controlled by the passed in flags.
	*
	* @param	Flags						Flags to control what method to use and optionally control memory vs speed
	* @param	CompressedBuffer			Buffer compressed data is going to be written to
	* @param	CompressedSize	[in/out]	Size of CompressedBuffer, at exit will be size of compressed data
	* @param	UncompressedBuffer			Buffer containing uncompressed data
	* @param	UncompressedSize			Size of uncompressed data in bytes
	* @return true if compression succeeds, false if it fails because CompressedBuffer was too small or other reasons
	**/
	virtual bool CompressMemory(ECompressionFlags Flags, void* CompressedBuffer, int32& CompressedSize, const void* UncompressedBuffer, int32 UncompressedSize, int32 BitWindow) = 0;

	/**
	* Thread-safe abstract decompression routine. Uncompresses memory from compressed buffer and writes it to uncompressed
	* buffer. UncompressedSize is expected to be the exact size of the data after decompression.
	*
	* @param	Flags						Flags to control what method to use to decompress
	* @param	UncompressedBuffer			Buffer containing uncompressed data
	* @param	UncompressedSize			Size of uncompressed data in bytes
	* @param	CompressedBuffer			Buffer compressed data is going to be read from
	* @param	CompressedSize				Size of CompressedBuffer data in bytes
	* @param	bIsSourcePadded		Whether the source memory is padded with a full cache line at the end
	* @return true if compression succeeds, false if it fails because CompressedBuffer was too small or other reasons
	*/
	virtual bool UncompressMemory(ECompressionFlags Flags, void* UncompressedBuffer, int32 UncompressedSize, const void* CompressedBuffer, int32 CompressedSize, bool bIsSourcePadded, int32 BitWindow) = 0;
};


/**
* Generic implementation of platform specific compression routines
**/
class CORE_API FGenericPlatformCompression : public IPlatformCompression
{
public:
	/**
	 * Gets the bit window for compressor for this platform.
	 *
	 * @return Compression bit window.
	 */
	virtual int32 GetCompressionBitWindow() const override
	{
		return DEFAULT_ZLIB_BIT_WINDOW;
	}
	
	/**
	* Thread-safe abstract compression routine to query memory requirements for a compression operation.
	* @param	Flags						Flags to control what method to use and optionally control memory vs speed
	* @param	UncompressedSize			Size of uncompressed data in bytes
	* @return The maximum possible bytes needed for compression of data buffer of size UncompressedSize
	**/
	virtual int32 CompressMemoryBound(ECompressionFlags Flags, int32 UncompressedSize, int32 BitWindow) override
	{
		return -1;
	}

	/**
	* Thread-safe abstract compression routine. Compresses memory from uncompressed buffer and writes it to compressed
	* buffer. Updates CompressedSize with size of compressed data. Compression controlled by the passed in flags.
	*
	* @param	Flags						Flags to control what method to use and optionally control memory vs speed
	* @param	CompressedBuffer			Buffer compressed data is going to be written to
	* @param	CompressedSize	[in/out]	Size of CompressedBuffer, at exit will be size of compressed data
	* @param	UncompressedBuffer			Buffer containing uncompressed data
	* @param	UncompressedSize			Size of uncompressed data in bytes
	* @return true if compression succeeds, false if it fails because CompressedBuffer was too small or other reasons
	**/
	virtual bool CompressMemory(ECompressionFlags Flags, void* CompressedBuffer, int32& CompressedSize, const void* UncompressedBuffer, int32 UncompressedSize, int32 BitWindow) override
	{
		return false;
	}

	/**
	* Thread-safe abstract decompression routine. Uncompresses memory from compressed buffer and writes it to uncompressed
	* buffer. UncompressedSize is expected to be the exact size of the data after decompression.
	*
	* @param	Flags						Flags to control what method to use to decompress
	* @param	UncompressedBuffer			Buffer containing uncompressed data
	* @param	UncompressedSize			Size of uncompressed data in bytes
	* @param	CompressedBuffer			Buffer compressed data is going to be read from
	* @param	CompressedSize				Size of CompressedBuffer data in bytes
	* @param	bIsSourcePadded		Whether the source memory is padded with a full cache line at the end
	* @return true if compression succeeds, false if it fails because CompressedBuffer was too small or other reasons
	*/
	virtual bool UncompressMemory(ECompressionFlags Flags, void* UncompressedBuffer, int32 UncompressedSize, const void* CompressedBuffer, int32 CompressedSize, bool bIsSourcePadded, int32 BitWindow) override
	{
		return false;
	}
};
