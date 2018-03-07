// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "CoreFwd.h"

/**
 * Implements a helper structure for compression support
 *
 * This structure contains information on the compressed and uncompressed size of a chunk of data.
 */
struct FCompressedChunkInfo
{
	/** Holds the data's compressed size. */
	int64 CompressedSize;

	/** Holds the data's uncompresses size. */
	int64 UncompressedSize;
};

/**
 * Serializes an FCompressedChunkInfo value from or into an archive.
 *
 * @param Ar The archive to serialize from or to.
 * @param Value The value to serialize.
 */
CORE_API FArchive& operator<<(FArchive& Ar, FCompressedChunkInfo& Value);
