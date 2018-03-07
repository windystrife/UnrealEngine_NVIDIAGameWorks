// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include "HAL/Runnable.h"
#include "Misc/SecureHash.h"
#include "Common/StatsCollector.h"
#include "Data/ChunkData.h"

namespace BuildPatchServices
{
	class IChunkDataAccess;

	/**
	 * Declares threaded chunk writer class for queuing up chunk file saving
	 */
	class FChunkWriter
	{
	private:
		// A private runnable class that writes out data
		class FQueuedChunkWriter : public FRunnable
		{
		public:
			// The directory that we want to save chunks to
			FString ChunkDirectory;

			// The stats collector for stat output
			FStatsCollectorPtr StatsCollector;

			// Default Constructor
			FQueuedChunkWriter();

			// Default Destructor
			~FQueuedChunkWriter();

			// FRunnable interface begin.
			virtual bool Init() override;
			virtual uint32 Run() override;
			// FRunnable interface end.

			/**
			 * Add a complete chunk to the queue.
			 * BLOCKS RETURN until space in the queue
			 * @param	ChunkData		The pointer to the chunk data. Should be of BuildPatchServices::ChunkDataSize length
			 * @param	ChunkGuid		The GUID of this chunk
			 * @param	ChunkHash		The hash value of this chunk
			 */
			void QueueChunk( const uint8* ChunkData, const FGuid& ChunkGuid, const uint64& ChunkHash );

			/**
			 * Call to flag as no more chunks to come (the thread will finish up and then exit).
			 */
			void SetNoMoreChunks();

			/**
			 * Retrieves the list of chunk files sizes
			 * @param	OutChunkFileSizes		The Map is emptied and then filled with the list
			 */
			void GetChunkFilesizes(TMap<FGuid, int64>& OutChunkFileSizes);

		private:
			// The queue of chunks to save
			TArray<IChunkDataAccess*> ChunkFileQueue;

			// A critical section for accessing ChunkFileQueue.
			FCriticalSection ChunkFileQueueCS;

			// Store whether more chunks are coming
			bool bMoreChunks;

			// A critical section for accessing ChunkFileQueue.
			FCriticalSection MoreChunksCS;

			// Store chunk file sizes.
			TMap<FGuid, int64> ChunkFileSizes;

			// A critical section for accessing ChunkFileSizes.
			FCriticalSection ChunkFileSizesCS;

			// Atomic statistics
			volatile int64* StatFileCreateTime;
			volatile int64* StatCheckExistsTime;
			volatile int64* StatSerlialiseTime;
			volatile int64* StatChunksSaved;
			volatile int64* StatCompressTime;
			volatile int64* StatDataWritten;
			volatile int64* StatDataWriteSpeed;
			volatile int64* StatCompressionRatio;

			/**
			 * Called from within run to save out a chunk file.
			 * @param	ChunkFilename	The chunk filename
			 * @param	LockedChunk		File data to be written out. Containing header will be edited.
			 * @param	ChunkGuid		The GUID of this chunk.
			 * @return	true if no file error
			 */
			const bool WriteChunkData(const FString& ChunkFilename, FScopeLockedChunkData& LockedChunk, const FGuid& ChunkGuid);

			/**
			 * Thread safe. Checks to see if there are any chunks in the queue, or if there are more chunks expected.
			 * @return	true if we should keep looping
			 */
			const bool ShouldBeRunning();

			/**
			 * Thread safe. Checks to see if there are any chunks in the queue
			 * @return	true if ChunkFileQueue is not empty
			 */
			const bool HasQueuedChunk();

			/**
			 * Thread safe. Checks to see if there is space to queue a new chunk
			 * @return	true if a chunk can be queued
			 */
			const bool CanQueueChunk();

			/**
			 * Thread safe. Gets the next chunk from the chunk queue
			 * @return	The next chunk file
			 */
			IChunkDataAccess* GetNextChunk();

		} QueuedChunkWriter;

	public:
		/**
		 * Constructor
		 */
		FChunkWriter(const FString& ChunkDirectory, FStatsCollectorRef StatsCollector);

		/**
		 * Default destructor
		 */
		~FChunkWriter();

		/**
		 * Add a complete chunk to the queue. 
		 * BLOCKS RETURN until space in the queue
		 * @param	ChunkData		The pointer to the chunk data. Should be of BuildPatchServices::ChunkDataSize length
		 * @param	ChunkGuid		The GUID of this chunk
		 * @param	ChunkHash		The hash value of this chunk
		 */
		void QueueChunk( const uint8* ChunkData, const FGuid& ChunkGuid, const uint64& ChunkHash );

		/**
		 * Call when there are no more chunks. The thread will be stopped and data cleaned up.
		 */
		void NoMoreChunks();

		/**
		 * Will only return when the writer thread has finished.
		 */
		void WaitForThread();

		/**
		 * Retrieves the list of chunk files sizes from the chunk writer
		 * @param	OutChunkFileSizes		The Map is emptied and then filled with the list
		 */
		void GetChunkFilesizes(TMap<FGuid, int64>& OutChunkFileSizes);

	private:
		/**
		 * Hide the default constructor so that the params must be given
		 */
		FChunkWriter(){};

		// Holds a pointer to our thread for the runnable
		FRunnableThread* WriterThread;
	};
}
