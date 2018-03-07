// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Installer/ChunkSource.h"

namespace BuildPatchServices
{
	/**
	 * A chained chunk source provides a single point of access for a list of sources which are iterated through to find the requested data.
	 */
	class IChainedChunkSource
		: public IChunkSource
	{
	public:
		virtual ~IChainedChunkSource() {}
	};

	/**
	 * A factory for creating an IChainedChunkSource instance.
	 */
	class FChainedChunkSourceFactory
	{
	public:
		/**
		 * This implementation takes the array of other sources and loops through them until the first one returns a valid chunk.
		 * @param ChunkSources   The array of IChunkSource to loop through. Must not be empty, or contain nullptrs.
		 * @return the new IChainedChunkSource instance created.
		 */
		static IChainedChunkSource* Create(TArray<IChunkSource*> ChunkSources);
	};
}