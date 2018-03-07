// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Serialization/CompressedChunkInfo.h"
#include "Serialization/Archive.h"

/** 
 * FCompressedChunkInfo serialize operator.
 */
FArchive& operator<<( FArchive& Ar, FCompressedChunkInfo& Chunk )
{
	// The order of serialization needs to be identical to the memory layout as the async IO code is reading it in bulk.
	// The size of the structure also needs to match what is being serialized.
	Ar << Chunk.CompressedSize;
	Ar << Chunk.UncompressedSize;
	return Ar;
}
