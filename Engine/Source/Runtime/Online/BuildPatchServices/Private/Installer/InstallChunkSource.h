// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Installer/ChunkSource.h"
#include "Installer/Controllable.h"
#include "BuildPatchManifest.h"

namespace BuildPatchServices
{
	class IChunkStore;
	class IChunkReferenceTracker;
	class IInstallerError;
	class IInstallChunkSourceStat;
	class IFileSystem;

	/**
	 * The interface for an installation chunk source, which provides access to chunk data retrieved from known local installations.
	 */
	class IInstallChunkSource
		: public IChunkSource
		, public IControllable
	{
	public:
		virtual ~IInstallChunkSource() {}

		/**
		 * Get the set of chunks available locally which are relevant to the installation being performed.
		 * @return the set of chunks that should be available locally.
		 */
		virtual const TSet<FGuid>& GetAvailableChunks() const = 0;
	};

	/**
	 * A struct containing the configuration values for an install chunk source.
	 */
	struct FInstallSourceConfig
	{
		// A set of chunks to not retrieve unless specifically asked for.
		TSet<FGuid> ChunkIgnoreSet;
		// The minimum number of chunks to load at a time when one is requested, depending on store slack.
		int32 BatchFetchMinimum;
		// The maximum number of chunks to load at a time when one is requested, depending on store slack.
		int32 BatchFetchMaximum;

		/**
		 * Constructor which sets usual defaults.
		 */
		FInstallSourceConfig()
			: ChunkIgnoreSet()
			, BatchFetchMinimum(10)
			, BatchFetchMaximum(40)
		{
		}
	};

	/**
	 * A factory for creating an IInstallChunkSource instance.
	 */
	class FInstallChunkSourceFactory
	{
	public:
		/**
		 * This implementation will read chunks from provided local installations if they are available.
		 * During initialization the local installations are enumerated to find each available chunk and expected local files
		 * are checked and skipped if missing or incorrect size.
		 * @param Configuration             The configuration struct for this cloud source.
		 * @param FileSystem                The service used to open files on disk.
		 * @param ChunkStore                The chunk store where loaded chunks will be put.
		 * @param ChunkReferenceTracker     The reference tracker for the installation, used to decide which chunks to fetch when loading.
		 * @param InstallerError            Error tracker where fatal errors will be reported.
		 * @param InstallChunkSourceStat    The class to receive statistics and event information.
		 * @param InstallationSources       The map of known local installation directories and the associated manifest.
		 * @param InstallManifest           The manifest that chunks are required for.
		 * @return the new IInstallChunkSource instance created.
		 */
		static IInstallChunkSource* Create(FInstallSourceConfig Configuration, IFileSystem* FileSystem, IChunkStore* ChunkStore, IChunkReferenceTracker* ChunkReferenceTracker, IInstallerError* InstallerError, IInstallChunkSourceStat* InstallChunkSourceStat, const TMap<FString, FBuildPatchAppManifestRef>& InstallationSources, const FBuildPatchAppManifestRef& InstallManifest);
	};

	/**
	 * This interface defines the statistics class required by the install chunk source. It should be implemented in order to collect
	 * desired information which is being broadcast by the system.
	 */
	class IInstallChunkSourceStat
	{
	public:
		/**
		 * Enum which describes success, or the reason for failure when loading a chunk.
		 */
		enum class ELoadResult : uint8
		{
			Success = 0,

			// The hash information was missing.
			MissingHashInfo,

			// Chunk part information was missing.
			MissingPartInfo,

			// Failed to open a source file.
			OpenFileFail,

			// The expected source file size was not matched.
			IncorrectFileSize,

			// The expected data hash for the chunk did not match.
			HashCheckFailed,

			// The process has been aborted.
			Aborted
		};

	public:
		virtual ~IInstallChunkSourceStat() {}

		/**
		 * Called each time a chunk load begins.
		 * @param ChunkId   The id of the chunk.
		 */
		virtual void OnLoadStarted(const FGuid& ChunkId) = 0;

		/**
		 * Called each time a chunk load completes.
		 * @param ChunkId   The id of the chunk.
		 * @param Result    The load result.
		 */
		virtual void OnLoadComplete(const FGuid& ChunkId, ELoadResult Result) = 0;
	};
}