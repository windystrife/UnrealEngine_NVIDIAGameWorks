// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

namespace BuildPatchServices
{
	class IChunkSource;
	class IFileSystem;
	class IInstallerError;
	class IChunkReferenceTracker;
	class IChunkDataSerialization;

	struct FChunkDatabaseFile
	{
		FString DatabaseFilename;
		TArray<FGuid> DataList;
	};

	/**
	 * An interface providing providing access to a system which writes chunk database files, given a chunk source and details of the chunks to put
	 * into databases.
	 */
	class IChunkDatabaseWriter
	{
	public:
		virtual ~IChunkDatabaseWriter() {}
	};

	/**
	 * A factory for creating an IChunkDatabaseWriter instance.
	 */
	class FChunkDatabaseWriterFactory
	{
	public:
		/**
		 * This implementation returns a chunk database writer that immediately kicks off the work and calls a provided callback when complete.
		 * @param ChunkSource               A chunk source for pulling required chunks from.
		 * @param FileSystem                A files system interface for writing out the chunkdb files.
		 * @param InstallerError            The error interface for aborting on other errors or registering our own.
		 * @param ChunkReferenceTracker     Chunk reference tracker to keep up to date.
		 * @param ChunkDataSerialization    Chunk data serialization implementation.
		 * @param ChunkDatabaseList         The array of chunk database files to create and the chunks to place in them.
		 * @param OnComplete                Function to call when the database files have been created. Called on main thread.
		 * @return the new IChunkDatabaseWriter.
		 */
		static IChunkDatabaseWriter* Create(
			IChunkSource* ChunkSource,
			IFileSystem* FileSystem,
			IInstallerError* InstallerError,
			IChunkReferenceTracker* ChunkReferenceTracker,
			IChunkDataSerialization* ChunkDataSerialization,
			TArray<FChunkDatabaseFile> ChunkDatabaseList,
			TFunction<void(bool)> OnComplete);
	};
}
