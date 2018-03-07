// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Serialization/Archive.h"

/*----------------------------------------------------------------------------
	FArchiveLoadCompressedProxy.
----------------------------------------------------------------------------*/

/**
 * FArchive Proxy to transparently load compressed data from an array.
 */
class CORE_API FArchiveLoadCompressedProxy : public FArchive
{
public:
	/** 
	 * Constructor, initializing all member variables and allocating temp memory.
	 *
	 * @param	InCompressedData	Array of bytes that is holding compressed data
	 * @param	InCompressionFlags	Compression flags that were used to compress data
	 */
	FArchiveLoadCompressedProxy( const TArray<uint8>& InCompressedData, ECompressionFlags InCompressionFlags );

	/** Destructor, freeing temporary memory. */
	virtual ~FArchiveLoadCompressedProxy();

	/**
	 * Serializes data from archive. This function is called recursively and determines where to serialize
	 * from and how to do so based on internal state.
	 *
	 * @param	Data	Pointer to serialize to
	 * @param	Count	Number of bytes to read
	 */
	virtual void Serialize( void* Data, int64 Count );

	/**
	 * Seeks to the passed in position in the stream. This archive only supports forward seeking
	 * and implements it by serializing data till it reaches the position.
	 */
	virtual void Seek( int64 InPos );

	/**
	 * @return current position in uncompressed stream in bytes.
	 */
	virtual int64 Tell();

private:
	/**
	 * Flushes tmp data to array.
	 */
	void DecompressMoreData();

	/** Array to write compressed data to.						*/
	const TArray<uint8>&	CompressedData;
	/** Current index into compressed data array.				*/
	int32				CurrentIndex;
	/** Pointer to start of temporary buffer.					*/
	uint8*			TmpDataStart;
	/** Pointer to end of temporary buffer.						*/
	uint8*			TmpDataEnd;
	/** Pointer to current position in temporary buffer.		*/
	uint8*			TmpData;
	/** Whether to serialize from temporary buffer of array.	*/
	bool			bShouldSerializeFromArray;
	/** Number of raw (uncompressed) bytes serialized.			*/
	int64		RawBytesSerialized;
	/** Flags used for compression.								*/
	ECompressionFlags CompressionFlags;
};
