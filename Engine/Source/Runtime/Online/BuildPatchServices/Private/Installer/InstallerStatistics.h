// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "EnumRange.h"
#include "Array.h"

enum class EBuildPatchDownloadHealth;

namespace BuildPatchServices
{
	class IMemoryChunkStoreStat;
	class IDiskChunkStoreStat;
	class IChunkDbChunkSourceStat;
	class IInstallChunkSourceStat;
	class IDownloadServiceStat;
	class ICloudChunkSourceStat;
	class IFileConstructorStat;
	class IVerifierStat;
	class IInstallerAnalytics;
	struct FBuildPatchProgress;

	/**
	 * An enum defining the use for a store instance.
	 */
	enum class EMemoryChunkStore : uint8
	{
		// Chunk store created for the cloud source.
		CloudSource = 0,
		// Chunk store created for the local installations source.
		InstallSource,

		MaxValue
	};

	/**
	 * ...
	 */
	class IInstallerStatistics
	{
	public:
		virtual ~IInstallerStatistics() {}

		/**
		 * @return the total number of bytes downloaded.
		 */
		virtual int64 GetBytesDownloaded() const = 0;

		/**
		 * @return the number of successfully downloaded chunks.
		 */
		virtual int32 GetNumSuccessfulChunkDownloads() const = 0;

		/**
		 * @return the number of chunk requests that failed.
		 */
		virtual int32 GetNumFailedChunkDownloads() const = 0;

		/**
		 * @return the number of successful chunk downloads which had invalid data.
		 */
		virtual int32 GetNumCorruptChunkDownloads() const = 0;

		/**
		 * @return the number of chunk downloads which were aborted, having been determined as lagging.
		 */
		virtual int32 GetNumAbortedChunkDownloads() const = 0;

		/**
		 * @return the number of chunks which were successfully loaded from local installations.
		 */
		virtual int32 GetNumSuccessfulChunkRecycles() const = 0;

		/**
		 * @return the number of chunks which failed to load from local installations.
		 */
		virtual int32 GetNumFailedChunkRecycles() const = 0;

		/**
		 * @return the number of chunks successfully read from chunkdbs.
		 */
		virtual int32 GetNumSuccessfulChunkDbLoads() const = 0;

		/**
		 * @return the number of chunks which failed to load from provided chunkdbs.
		 */
		virtual int32 GetNumFailedChunkDbLoads() const = 0;

		/**
		 * @return the number of chunks which were booted from memory stores.
		 */
		virtual int32 GetNumStoreBootedChunks() const = 0;

		/**
		 * @return the number of chunks which were loaded from the overflow disk store.
		 */
		virtual int32 GetNumSuccessfulChunkDiskCacheLoads() const = 0;

		/**
		 * @return the number of chunks which failed to load from the overflow disk store.
		 */
		virtual int32 GetNumFailedChunkDiskCacheLoads() const = 0;

		/**
		 * @return the number of bytes that the installation required from cloud sources.
		 */
		virtual int64 GetRequiredDownloadSize() const = 0;

		/**
		 * Get the current average download speed achieved from the last X seconds.
		 * @param Seconds       The time in seconds to take the reading over.
		 * @return the average speed of downloads over the past given seconds.
		 */
		virtual double GetDownloadSpeed(float Seconds) const = 0;

		/**
		 * @return the rate of success for chunks download requests, 1.0 being 100%.
		 */
		virtual float GetDownloadSuccessRate() const = 0;

		/**
		 * @return the EBuildPatchDownloadHealth value which the success rate applies to according to the configured ranges.
		 */
		virtual EBuildPatchDownloadHealth GetDownloadHealth() const = 0;

		/**
		 * @return an array of seconds spent in each download health range, indexable by EBuildPatchDownloadHealth.
		 */
		virtual TArray<float> GetDownloadHealthTimers() const = 0;

		/**
		 * Get the stats interface for a particular memory store.
		 * @param Instance      The instance that the store is for.
		 * @return the stats interface for the memory store.
		 */
		virtual IMemoryChunkStoreStat* GetMemoryChunkStoreStat(EMemoryChunkStore Instance) = 0;

		/**
		 * @return the stats interface for the disk store.
		 */
		virtual IDiskChunkStoreStat* GetDiskChunkStoreStat() = 0;

		/**
		 * @return the stats interface for the chunkdb source.
		 */
		virtual IChunkDbChunkSourceStat* GetChunkDbChunkSourceStat() = 0;

		/**
		 * @return the stats interface for the installation source.
		 */
		virtual IInstallChunkSourceStat* GetInstallChunkSourceStat() = 0;

		/**
		 * @return the stats interface for the download service.
		 */
		virtual IDownloadServiceStat* GetDownloadServiceStat() = 0;

		/**
		 * @return the stats interface for the cloud source.
		 */
		virtual ICloudChunkSourceStat* GetCloudChunkSourceStat() = 0;

		/**
		 * @return the stats interface for the file constructor.
		 */
		virtual IFileConstructorStat* GetFileConstructorStat() = 0;

		/**
		 * @return the stats interface for the verifier service.
		 */
		virtual IVerifierStat* GetVerifierStat() = 0;
	};

	/**
	 * A factory for creating an IInstallerStatistics instance.
	 */
	class FInstallerStatisticsFactory
	{
	public:

		/**
		 * Creates an implementation with provides access to interfaces for each system of the installer, and exposes statistics
		 * values collected from the systems' reports.
		 * This will also send the analytics events which are generated from the various system behaviors.
		 * State progress is also handled for various systems until the BuildPatchProgress class is refactored.
		 * @param InstallerAnalytics  The analytics implementation for reporting the installer events.
		 * @param BuildProgress       The legacy progress implementation to bridge the system stats to.
		 * @return the new IInstallerStatistics instance created.
		 */
		static IInstallerStatistics* Create(IInstallerAnalytics* InstallerAnalytics, FBuildPatchProgress* BuildProgress);
	};
}

/**
 * Allows the BuildPatchServices::EMemoryChunkStore to be used in loops.
 */
ENUM_RANGE_BY_COUNT(BuildPatchServices::EMemoryChunkStore, BuildPatchServices::EMemoryChunkStore::MaxValue);
