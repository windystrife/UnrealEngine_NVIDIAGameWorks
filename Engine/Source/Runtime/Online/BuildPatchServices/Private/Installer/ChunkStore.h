// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Data/ChunkData.h"
#include "Misc/Guid.h"
#include "UniquePtr.h"

namespace BuildPatchServices
{
	/**
	 * An interface providing access to storage of chunk data instances.
	 */
	class IChunkStore
	{
	public:
		virtual ~IChunkStore() {}

		/**
		 * Put chunk data into this store. Chunk data unique ptr must be moved in, the store becomes the owner
		 * of the memory and its lifetime.
		 * Whether or not the call involves actually storing the data provided is implementation specific. It is possible to implement
		 * readonly/null IChunkStore.
		 * @param DataId    The GUID for the data.
		 * @param ChunkData The instance of the data. This must be moved in.
		 */
		virtual void Put(const FGuid& DataId, TUniquePtr<IChunkDataAccess> ChunkData) = 0;

		/**
		 * Get access to chunk data contained in this store.
		 * The returned data ptr is only valid until the next Get call, or a remove call for the same data.
		 * @param DataId    The GUID for the data.
		 * @return ptr to the data instance, or nullptr if this GUID was not in the store.
		 */
		virtual IChunkDataAccess* Get(const FGuid& DataId) = 0;

		/**
		 * Remove chunk data from this store. The data access is returned, this will cause destruction once out of scope.
		 * Whether or not the call involves actual data destruction is implementation specific. It is possible to implement
		 * readonly/null IChunkStore.
		 * @param DataId    The GUID for the data.
		 * @return the data instance referred to, or invalid if this GUID was not in the store.
		 */
		virtual TUniquePtr<IChunkDataAccess> Remove(const FGuid& DataId) = 0;

		/**
		 * Gets the slack space for the store. If the store is configured with a max size, this represents how much space is available.
		 * The value can be negative, indicating an eviction policy which allowed the store to grow.
		 * For unsized stores, it is expected to return max int32 value.
		 * @return the slack space for the store.
		 */
		virtual int32 GetSlack() const = 0;
	};
}
