// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"
#include "UnrealAudioTypes.h"
#include "Containers/Queue.h"

#if ENABLE_UNREAL_AUDIO

namespace UAudio
{
	static const uint32 ENTITY_INDEX_BITS = 24;
	static const uint32 ENTITY_INDEX_MASK = (1 << ENTITY_INDEX_BITS) - 1;
	static const uint32 ENTITY_GENERATION_BITS = 8;
	static const uint32 ENTITY_GENERATION_MASK = (1 << ENTITY_GENERATION_BITS) - 1;
	static const uint32 ENTITY_INDEX_INVALID = INDEX_NONE;

	/** An entity handle. */
	struct FEntityHandle
	{

	public:

		/**
		 * Static constructor.
		 *
		 * @param	InId	Identifier for the in.
		 */

		static FEntityHandle Create(uint32 InId = INDEX_NONE)
		{
			FEntityHandle Result;
			Result.Id = InId;
			return Result;
		}

		/**
		 * Query if this object is initialized.
		 *
		 * @return	true if initialized, false if not.
		 */

		bool IsInitialized() const
		{
			return Id != INDEX_NONE;
		}

		/**
		 * Gets the index.
		 *
		 * @return	The index.
		 */

		uint32 GetIndex() const
		{
			return Id & ENTITY_INDEX_MASK;
		}

		/**
		 * Gets the generation.
		 *
		 * @return	The generation.
		 */

		uint32 GetGeneration() const
		{
			return (Id >> ENTITY_GENERATION_BITS) & ENTITY_GENERATION_MASK;
		}

		/** The identifier. */
		uint32 Id;
	};

	/**
	 * Manages entity handles for weak-referencing of resources between threads and processes.
	 */
	class FEntityManager
	{
	public:

		/**
		 * Constructor.
		 *
		 * @param	InMinNumFreeIndices	The minimum number free indices.
		 */

		FEntityManager(uint32 InMinNumFreeIndices)
			: NumFreeEntities(0)
			, MinNumFreeIndices(InMinNumFreeIndices)
		{

		}

		/** Destructor. */
		~FEntityManager()
		{
		}

		/**
		 * Creates the entity.
		 *
		 * @return	The new entity.
		 */

		FEntityHandle CreateEntity()
		{
			uint32 NewEntityIndex = ENTITY_INDEX_INVALID;
			if (NumFreeEntities > MinNumFreeIndices)
			{
				FreeEntityIndices.Dequeue(NewEntityIndex);
				--NumFreeEntities;
			}
			else
			{
				NewEntityIndex = EntityGenerations.Num();
				EntityGenerations.Add(0);
				check(NewEntityIndex < (1 << ENTITY_INDEX_BITS));
			}
			return CreateHandle(NewEntityIndex, EntityGenerations[NewEntityIndex]);
		}

		/**
		 * Releases the entity described by Handle.
		 *
		 * @param	Handle	The handle.
		 */

		void ReleaseEntity(const FEntityHandle& Handle)
		{
			uint32 EntityIndex = Handle.GetIndex();
			uint8 Generation = Handle.GetGeneration();
			EntityGenerations[EntityIndex] = ++Generation;
			++NumFreeEntities;
			FreeEntityIndices.Enqueue(EntityIndex);
		}

		/**
		 * Query if 'Handle' is valid entity.
		 *
		 * @param	Handle	The handle.
		 *
		 * @return	true if valid entity, false if not.
		 */

		bool IsValidEntity(FEntityHandle Handle) const
		{
			if (Handle.Id == ENTITY_INDEX_INVALID)
			{
				return false;
			}

			uint32 EntityIndex = Handle.GetIndex();
			uint8 Generation = Handle.GetGeneration();
			return EntityGenerations[EntityIndex] == Generation;
		}

	private:

		/**
		 * Creates a handle.
		 *
		 * @param	EntityIndex			Zero-based index of the entity.
		 * @param	EntityGeneration	The entity generation.
		 *
		 * @return	The new handle.
		 */

		FEntityHandle CreateHandle(uint32 EntityIndex, uint8 EntityGeneration) const
		{
			return FEntityHandle::Create(EntityIndex | (EntityGeneration << ENTITY_INDEX_BITS));
		}

		/** Number of free entities. */
		uint32 NumFreeEntities;

		/** The minimum number free indices. */
		uint32 MinNumFreeIndices;

		/** The free entity indices. */
		TQueue<uint32> FreeEntityIndices;

		/** The entity generations. */
		TArray<uint8> EntityGenerations;
	};

}

#endif // #if ENABLE_UNREAL_AUDIO


