// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "BuildPatchManifest.h"

namespace BuildPatchServices
{
	/**
	 * An interface for tracking references to chunks used throughout an installation. It is used to share across systems the
	 * chunks that are still required and when.
	 */
	class IChunkReferenceTracker
	{
	public:
		enum class ESortDirection : uint8
		{
			Ascending = 0,
			Descending
		};

	public:
		virtual ~IChunkReferenceTracker() {}

		/**
		 * Gets a set of all chunks referenced by the installation this tracker refers to.
		 * @return set of GUID containing every chunk used.
		 */
		virtual TSet<FGuid> GetReferencedChunks() const = 0;

		/**
		 * Gets the number of times a specific chunk is still referenced for the associated installation.
		 * @param ChunkId           The id for the chunk in question.
		 * @return the number of references remaining.
		 */
		virtual int32 GetReferenceCount(const FGuid& ChunkId) const = 0;

		/**
		 * Sorts a given array of chunk ids by the order in which they are required for the installation.
		 * @param ChunkList         The array to be sorted.
		 * @param Direction         The direction of sort. Ascending places soonest required chunk first.
		 */
		virtual void SortByUseOrder(TArray<FGuid>& ChunkList, ESortDirection Direction) const = 0;

		/**
		 * Retrieve the array of next chunk references, using a predicate to select whether each chunk is considered.
		 * @param Count             The number of chunk entries that are desired.
		 * @param SelectPredicate   The predicate used to determine whether to count or ignore the given chunk.
		 * @return an array of unique chunk id entries, in the order in which they are required.
		 */
		virtual TArray<FGuid> GetNextReferences(int32 Count, const TFunction<bool(const FGuid&)>& SelectPredicate) const = 0;

		/**
		 * Pop the top reference from the tracker, indicating that operation has been performed.
		 * It is not valid to pop anything but the top guid, so it must be provided for verification of behavior.
		 * @param ChunkId           The id of the top chunk, this is used to verify behavior.
		 * @return true if the correct guid was provided and the reference was popped, false if the wrong guid
		 *         was provided and thus no change was made.
		 */
		virtual bool PopReference(const FGuid& ChunkId) = 0;
	};

	/**
	 * A factory for creating an IChunkReferenceTracker instance.
	 */
	class FChunkReferenceTrackerFactory
	{
	public:
		/**
		 * This implementation takes the install manifest and generates the internal data and chunk reference tracking based
		 * off of a set of files that will be constructed.
		 * @param InstallManifest   The install manifest to enumerate references from.
		 * @param FilesToConstruct  The set of files to be installed, other files will not be considered.
		 * @return the new IChunkReferenceTracker instance created.
		 */
		static IChunkReferenceTracker* Create(const FBuildPatchAppManifestRef& InstallManifest, const TSet<FString>& FilesToConstruct);

		/**
		 * This implementation takes the install manifest and generates the internal data and chunk reference tracking based
		 * on caching data and so using each chunk once in the order that would be required to install the build.
		 * @param InstallManifest   The install manifest to enumerate references from.
		 * @return the new IChunkReferenceTracker instance created.
		 */
		static IChunkReferenceTracker* Create(const FBuildPatchAppManifestRef& InstallManifest);
	};
}