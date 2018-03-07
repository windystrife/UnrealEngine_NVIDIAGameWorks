// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SortedMap.h"

/** Type of tag map that can be used during construction */
typedef TSortedMap<FName, FString, FDefaultAllocator, FNameSortIndexes> FAssetDataTagMap;

/** Wrapper of shared pointer to a map */
class FAssetDataTagMapSharedView
{
	
public:
	/** Default constructor - empty map */
	FAssetDataTagMapSharedView()
	{
	}

	/** Constructor from an existing map pointer */
	FAssetDataTagMapSharedView(TSharedPtr<FAssetDataTagMap> InMap)
		: Map(InMap)
	{
	}

	/** Constructor from an existing map pointer */
	FAssetDataTagMapSharedView(FAssetDataTagMap&& InMap)
	{
		if (InMap.Num())
		{
			Map = MakeShareable(new FAssetDataTagMap(MoveTemp(InMap)));
		}
	}

	/** Find a value by key (nullptr if not found) */
	const FString* Find(FAssetDataTagMap::KeyConstPointerType Key) const
	{
		return GetMap().Find(Key);
	}

	/** Find a value by key (abort if not found) */
	const FString& FindChecked(FAssetDataTagMap::KeyConstPointerType Key) const
	{
		return GetMap().FindChecked(Key);
	}

	/** Find a value by key (default value if not found) */
	FString FindRef(FAssetDataTagMap::KeyConstPointerType Key) const
	{
		return GetMap().FindRef(Key);
	}

	/** Determine whether a key is present in the map */
	bool Contains(FAssetDataTagMap::KeyConstPointerType Key) const
	{
		return GetMap().Contains(Key);
	}

	/** Retrieve size of map */
	int32 Num() const
	{
		return GetMap().Num();
	}

	/** Populate an array with all the map's keys */
	template <class FAllocator>
	int32 GetKeys(TArray<FName, FAllocator>& OutKeys) const
	{
		return GetMap().GetKeys(OutKeys);
	}

	/** Populate an array with all the map's keys */
	template <class FAllocator>
	void GenerateKeyArray(TArray<FName, FAllocator>& OutKeys) const
	{
		GetMap().GenerateKeyArray(OutKeys);
	}

	/** Populate an array with all the map's values */
	template <class FAllocator>
	void GenerateValueArray(TArray<FName, FAllocator>& OutValues) const
	{
		GetMap().GenerateValueArray(OutValues);
	}

	/** Iterate all key-value pairs */
	FAssetDataTagMap::TConstIterator CreateConstIterator() const
	{
		return GetMap().CreateConstIterator();
	}

	/** Const access to the underlying map, mainly for taking a copy */
	const FAssetDataTagMap& GetMap() const
	{
		static FAssetDataTagMap EmptyMap;

		if (Map.IsValid())
		{
			return *Map;
		}

		return EmptyMap;
	}

	/** Returns amount of extra memory used by this structure, including shared ptr overhead */
	uint32 GetAllocatedSize() const
	{
		uint32 AllocatedSize = 0;
		if (Map.IsValid())
		{
			AllocatedSize += sizeof(FAssetDataTagMap); // Map itself
			AllocatedSize += (sizeof(int32) * 2); // SharedPtr overhead
			AllocatedSize += Map->GetAllocatedSize();
		}

		return AllocatedSize;
	}
private:
	friend struct FAssetData;

	FORCEINLINE friend FArchive& operator<<(FArchive& Ar, FAssetDataTagMapSharedView& SharedView)
	{
		if (Ar.IsSaving())
		{
			if (SharedView.Map.IsValid())
			{
				Ar << *SharedView.Map;
			}
			else
			{
				FAssetDataTagMap TempMap;
				Ar << TempMap;
			}
		}
		else
		{
			// Serialize into temporary map, if it isn't empty move memory into new map
			FAssetDataTagMap TempMap;
			Ar << TempMap;

			if (TempMap.Num())
			{
				SharedView.Map = MakeShareable(new FAssetDataTagMap(MoveTemp(TempMap)));
			}
		}

		return Ar;
	}

	/** Range for iterator access - DO NOT USE DIRECTLY */
	friend FAssetDataTagMap::RangedForConstIteratorType begin(const FAssetDataTagMapSharedView& View)
	{
		return begin(View.GetMap());
	}

	/** Range for iterator access - DO NOT USE DIRECTLY */
	friend FAssetDataTagMap::RangedForConstIteratorType end(const FAssetDataTagMapSharedView& View)
	{
		return end(View.GetMap());
	}

	/** Pointer to map being wrapped, it is created on demand */
	TSharedPtr<FAssetDataTagMap> Map;
};

