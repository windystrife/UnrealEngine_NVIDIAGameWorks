// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Installer/ChunkStore.h"

namespace BuildPatchServices
{
	class IChunkDataSerialization;
	class IDiskChunkStoreStat;
	enum class EChunkSaveResult : uint8;
	enum class EChunkLoadResult : uint8;

	/**
	 * An interface providing access to chunk data instances which are stored on disk.
	 */
	class IDiskChunkStore
		: public IChunkStore
	{
	public:
		virtual ~IDiskChunkStore() {}
	};

	/**
	 * A factory for creating an IDiskChunkStore instance.
	 */
	class FDiskChunkStoreFactory
	{
	public:
		/**
		 * Creates an instance of a chunk store class that stores chunks on disk in the provided location.
		 * As per the IChunkStore contract, the ptr returned by Get() will be valid at least until another Get() call is made.
		 * A Remove() call will not actually delete the data from disk.
		 * A Get(), Put(), or Remove() call may block on the file IO.
		 * @param Serializer            Required ptr to the serialization implementation to use. If existing chunks used a different serializer,
		 *                              then Get() and Remove() calls for those could fail.
		 * @param DiskChunkStoreStat    Pointer to the statistics receiver.
		 * @param StoreRootPath         The directory path for where to store chunk files.
		 * @return the new IDiskChunkStore instance created.
		 */
		static IDiskChunkStore* Create(IChunkDataSerialization* Serializer, IDiskChunkStoreStat* DiskChunkStoreStat, FString StoreRootPath);
	};

	/**
	 * This interface defines the statistics class required by the disk chunk store. It should be implemented in order to collect
	 * desired information which is being broadcast by the system.
	 */
	class IDiskChunkStoreStat
	{
	public:
		virtual ~IDiskChunkStoreStat() {}

		/**
		 * Called whenever a new chunk has been put into the store.
		 * @param ChunkId           The id of the chunk.
		 * @param ChunkFilename     The filename this chunk was saved to.
		 * @param SaveResult        The serialization result, including whether the operation was successful.
		 */
		virtual void OnChunkStored(const FGuid& ChunkId, const FString& ChunkFilename, EChunkSaveResult SaveResult) = 0;

		/**
		 * Called whenever a new chunk has been loaded from the store.
		 * @param ChunkId           The id of the chunk.
		 * @param ChunkFilename     The filename this chunk was loaded from.
		 * @param SaveResult        The serialization result, including whether the operation was successful.
		 */
		virtual void OnChunkLoaded(const FGuid& ChunkId, const FString& ChunkFilename, EChunkLoadResult LoadResult) = 0;

		/**
		 * Called whenever the number of chunks in the store has updated.
		 * @param ChunkCount        The number of chunks now held by the store.
		 */
		virtual void OnCacheUseUpdated(int32 ChunkCount) = 0;
	};
}