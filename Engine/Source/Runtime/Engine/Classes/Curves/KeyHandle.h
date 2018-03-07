// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Class.h"
#include "KeyHandle.generated.h"

/**
 * Key handles are used to keep a handle to a key. They are completely transient.
 */
struct FKeyHandle
{
	ENGINE_API FKeyHandle();

	bool operator ==(const FKeyHandle& Other) const
	{
		return Index == Other.Index;
	}
	
	bool operator !=(const FKeyHandle& Other) const
	{
		return Index != Other.Index;
	}
	
	friend uint32 GetTypeHash(const FKeyHandle& Handle)
	{
		return GetTypeHash(Handle.Index);
	}

	friend FArchive& operator<<(FArchive& Ar,FKeyHandle& Handle)
	{
		Ar << Handle.Index;
		return Ar;
	}

private:

	uint32 Index;
};


/**
 * Represents a mapping of key handles to key index that may be serialized 
 */
USTRUCT()
struct FKeyHandleMap
{
	GENERATED_USTRUCT_BODY()
public:
	FKeyHandleMap() {}

	// This struct is not copyable.  This must be public or because derived classes are allowed to be copied
	FKeyHandleMap( const FKeyHandleMap& Other ) {}
	void operator=(const FKeyHandleMap& Other) {}

	/** TMap functionality */
	void Add( const FKeyHandle& InHandle, int32 InIndex );
	void Empty();
	void Remove( const FKeyHandle& InHandle );
	const int32* Find( const FKeyHandle& InHandle ) const;
	const FKeyHandle* FindKey( int32 KeyIndex ) const;
	int32 Num() const;
	TMap<FKeyHandle, int32>::TConstIterator CreateConstIterator() const;
	TMap<FKeyHandle, int32>::TIterator CreateIterator();

	/** ICPPStructOps implementation */
	bool Serialize(FArchive& Ar);
	bool operator==(const FKeyHandleMap& Other) const;
	bool operator!=(const FKeyHandleMap& Other) const;

	friend FArchive& operator<<(FArchive& Ar,FKeyHandleMap& P)
	{
		P.Serialize(Ar);
		return Ar;
	}

	// Ensures that all indices have a valid handle and that there are no handles left to invalid indices
	void EnsureAllIndicesHaveHandles(int32 NumIndices);
	
	// Ensures a handle exists for the specified Index
	void EnsureIndexHasAHandle(int32 KeyIndex);

private:

	TMap<FKeyHandle, int32> KeyHandlesToIndices;
};


template<>
struct TStructOpsTypeTraits<FKeyHandleMap>
	: public TStructOpsTypeTraitsBase2<FKeyHandleMap>
{
	enum 
	{
		WithSerializer = true,
		WithCopy = false,
		WithIdenticalViaEquality = true,
	};
};

/**
 * Lookup table that maps key handles to indices in an external data structure
 * Maintains a map of key handle to last known index,
 * and an array of optional key handles that's used to validate map entries.
 */
struct FKeyHandleLookupTable
{
	/**
	 * Get the index that corresponds to the specified key handle
	 *
	 * @param KeyHandle		The key handle to retrieve an up to date index for
	 * @retrun The index that corresponds to this key handle, or INDEX_NONE if the key handle is invalid
	 */
	ENGINE_API int32 GetIndex(FKeyHandle KeyHandle);

	/**
	 * Attempt to find the handle for the specified index, or allocate a new one if it doesn't have one
	 *
	 * @param Index			The index to find or allocate a handle for
	 * @retrun A valid key handle for the specified index
	 */
	ENGINE_API FKeyHandle FindOrAddKeyHandle(int32 Index);

	/**
	 * Move the specified handle from its previous index, to its new index
	 *
	 * @param OldIndex		The previous index of the handle
	 * @param NewIndex		The new index of the handle
	 */
	ENGINE_API void MoveHandle(int32 OldIndex, int32 NewIndex);

	/**
	 * Allocate a new handle for the specified index
	 *
	 * @param Index			The index to associate a handle with
	 * @return A new key handle that corresponds to the specified index
	 */
	ENGINE_API FKeyHandle AllocateHandle(int32 Index);

	/**
	 * Deallocate the specified index
	 *
	 * @param Index			The index to invalidate a handle for
	 */
	ENGINE_API void DeallocateHandle(int32 Index);

	/**
	 * Reset this lookup table, forgetting all key handles and indices
	 */
	ENGINE_API void Reset();

private:

	/** Array of optional key handles that reside in corresponding indices for an externally owned serial data structure */
	TArray<TOptional<FKeyHandle>> KeyHandles;

	/** Map of which key handles go to which indices. Indices may point to incorrect indices. Check against entry in KeyHandles array to verify. */
	TMap<FKeyHandle, int32> KeyHandlesToIndices;
};
