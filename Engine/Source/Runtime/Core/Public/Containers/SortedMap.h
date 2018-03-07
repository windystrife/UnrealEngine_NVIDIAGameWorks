// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Map.h"
#include "Containers/Algo/BinarySearch.h"
#include "Containers/Algo/Sort.h"
#include "UObject/NameTypes.h"

/** 
 * A Map of keys to value, implemented as a sorted TArray of TPairs.
 *
 * It has a mostly identical interface to TMap and is designed as a drop in replacement. Keys must be unique,
 * there is no equivalent sorted version of TMultiMap. It uses half as much memory as TMap, but adding and 
 * removing elements is O(n), and finding is O(Log n). In practice it is faster than TMap for low element
 * counts, and slower as n increases, This map is always kept sorted by the key type so cannot be sorted manually.
 */
template <typename KeyType, typename ValueType, typename ArrayAllocator /*= FDefaultAllocator*/, typename SortPredicate /*= TLess<KeyType>*/ >
class TSortedMap
{
	template <typename OtherKeyType, typename OtherValueType, typename OtherArrayAllocator, typename OtherSortPredicate>
	friend class TSortedMap;
	friend struct TContainerTraits<TSortedMap>;

public:
	typedef typename TTypeTraits<KeyType  >::ConstPointerType KeyConstPointerType;
	typedef typename TTypeTraits<KeyType  >::ConstInitType    KeyInitType;
	typedef typename TTypeTraits<ValueType>::ConstInitType    ValueInitType;
	typedef TPair<KeyType, ValueType> ElementType;
	
	TSortedMap() = default;
	TSortedMap(TSortedMap&&) = default;
	TSortedMap(const TSortedMap&) = default;
	TSortedMap& operator=(TSortedMap&&) = default;
	TSortedMap& operator=(const TSortedMap&) = default;

	/** Constructor for moving elements from a TSortedMap with a different ArrayAllocator. */
	template<typename OtherArrayAllocator>
	TSortedMap(TSortedMap<KeyType, ValueType, OtherArrayAllocator, SortPredicate>&& Other)
		: Pairs(MoveTemp(Other.Pairs))
	{
	}

	/** Constructor for copying elements from a TSortedMap with a different ArrayAllocator. */
	template<typename OtherArrayAllocator>
	TSortedMap(const TSortedMap<KeyType, ValueType, OtherArrayAllocator, SortPredicate>& Other)
		: Pairs(Other.Pairs)
	{
	}

	/** Assignment operator for moving elements from a TSortedMap with a different ArrayAllocator. */
	template<typename OtherArrayAllocator>
	TSortedMap& operator=(TSortedMap<KeyType, ValueType, OtherArrayAllocator, SortPredicate>&& Other)
	{
		Pairs = MoveTemp(Other.Pairs);
		return *this;
	}

	/** Assignment operator for copying elements from a TSortedMap with a different ArrayAllocator. */
	template<typename OtherArrayAllocator>
	TSortedMap& operator=(const TSortedMap<KeyType, ValueType, OtherArrayAllocator, SortPredicate>& Other)
	{
		Pairs = Other.Pairs;
		return *this;
	}

	/** Equality operator. This is efficient because pairs are always sorted. */
	FORCEINLINE bool operator==(const TSortedMap& Other) const
	{
		return Pairs == Other.Pairs;
	}

	/** Inequality operator. This is efficient because pairs are always sorted. */
	FORCEINLINE bool operator!=(const TSortedMap& Other) const
	{
		return Pairs != Other.Pairs;
	}

	/**
	 * Removes all elements from the map, potentially leaving space allocated for an expected number of elements about to be added.
	 *
	 * @param ExpectedNumElements The number of elements about to be added to the set.
	 */
	FORCEINLINE void Empty(int32 ExpectedNumElements = 0)
	{
		Pairs.Empty(ExpectedNumElements);
	}

    /** Efficiently empties out the map but preserves all allocations and capacities. */
    FORCEINLINE void Reset()
    {
        Pairs.Reset();
    }

	/** Shrinks the pair set to avoid slack. */
	FORCEINLINE void Shrink()
	{
		Pairs.Shrink();
	}

	/** Preallocates enough memory to contain Number elements. */
	FORCEINLINE void Reserve(int32 Number)
	{
		Pairs.Reserve(Number);
	}

	/** @return The number of elements in the map. */
	FORCEINLINE int32 Num() const
	{
		return Pairs.Num();
	}

	/** 
	 * Helper function to return the amount of memory allocated by this container.
	 *
	 * @return number of bytes allocated by this container.
	 */
	FORCEINLINE uint32 GetAllocatedSize() const
	{
		return Pairs.GetAllocatedSize();
	}

	/** Tracks the container's memory use through an archive. */
	FORCEINLINE void CountBytes(FArchive& Ar)
	{
		Pairs.CountBytes(Ar);
	}

	/**
	 * Sets the value associated with a key.
	 *
	 * @param InKey The key to associate the value with.
	 * @param InValue The value to associate with the key.
	 * @return A reference to the value as stored in the map (only valid until the next change to any key in the map).
	 */
	FORCEINLINE ValueType& Add(const KeyType&  InKey, const ValueType&  InValue) { return Emplace(         InKey ,          InValue ); }
	FORCEINLINE ValueType& Add(const KeyType&  InKey,       ValueType&& InValue) { return Emplace(         InKey , MoveTemp(InValue)); }
	FORCEINLINE ValueType& Add(      KeyType&& InKey, const ValueType&  InValue) { return Emplace(MoveTemp(InKey),          InValue ); }
	FORCEINLINE ValueType& Add(      KeyType&& InKey,       ValueType&& InValue) { return Emplace(MoveTemp(InKey), MoveTemp(InValue)); }

	/**
	 * Sets a default value associated with a key.
	 *
	 * @param InKey The key to associate the value with.
	 * @return A reference to the value as stored in the map.  The reference is only valid until the next change to any key in the map.
	 */
	FORCEINLINE ValueType& Add(const KeyType&  InKey) { return Emplace(         InKey ); }
	FORCEINLINE ValueType& Add(      KeyType&& InKey) { return Emplace(MoveTemp(InKey)); }

	/**
	 * Sets the value associated with a key.
	 *
	 * @param InKey - The key to associate the value with.
	 * @param InValue - The value to associate with the key.
	 * @return A reference to the value as stored in the map (only valid until the next change to any key in the map).
	 */
	template <typename InitKeyType, typename InitValueType>
	ValueType& Emplace(InitKeyType&& InKey, InitValueType&& InValue)
	{
		ElementType* DataPtr = AllocateMemoryForEmplace(InKey);
	
		new(DataPtr) ElementType(TPairInitializer<InitKeyType&&, InitValueType&&>(Forward<InitKeyType>(InKey), Forward<InitValueType>(InValue)));

		return DataPtr->Value;
	}

	/**
	 * Sets a default value associated with a key.
	 *
	 * @param InKey The key to associate the value with.
	 * @return A reference to the value as stored in the map (only valid until the next change to any key in the map).
	 */
	template <typename InitKeyType>
	ValueType& Emplace(InitKeyType&& InKey)
	{
		ElementType* DataPtr = AllocateMemoryForEmplace(InKey);

		new(DataPtr) ElementType(TKeyInitializer<InitKeyType&&>(Forward<InitKeyType>(InKey)));

		return DataPtr->Value;
	}

	/**
	 * Removes all value associations for a key.
	 *
	 * @param InKey The key to remove associated values for.
	 * @return The number of values that were associated with the key.
	 */
	FORCEINLINE int32 Remove(KeyConstPointerType InKey)
	{
		int32 RemoveIndex = FindIndex(InKey);

		if (RemoveIndex == INDEX_NONE)
		{
			return 0;
		}

		Pairs.RemoveAt(RemoveIndex);

		return 1;
	}

	/**
	 * Returns the key associated with the specified value.  The time taken is O(N) in the number of pairs.
	 *
	 * @param Value The value to search for.
	 * @return A pointer to the key associated with the specified value, or nullptr if the value isn't contained in this map (only valid until the next change to any key in the map).
	 */
	const KeyType* FindKey(ValueInitType Value) const
	{
		for(typename ElementArrayType::TConstIterator PairIt(Pairs);PairIt;++PairIt)
		{
			if(PairIt->Value == Value)
			{
				return &PairIt->Key;
			}
		}
		return nullptr;
	}

	/**
	 * Returns the value associated with a specified key.
	 *
	 * @param Key The key to search for.
	 * @return A pointer to the value associated with the specified key, or nullptr if the key isn't contained in this map.  The pointer (only valid until the next change to any key in the map).
	 */
	FORCEINLINE ValueType* Find(KeyConstPointerType Key)
	{
		int32 FoundIndex = FindIndex(Key);

		if (FoundIndex != INDEX_NONE)
		{
			return &Pairs[FoundIndex].Value;
		}

		return nullptr;
	}

	FORCEINLINE const ValueType* Find(KeyConstPointerType Key) const
	{
		return const_cast<TSortedMap*>(this)->Find(Key);
	}

	/**
	 * Returns the value associated with a specified key, or if none exists, 
	 * adds a value using the default constructor.
	 *
	 * @param Key The key to search for.
	 * @return A reference to the value associated with the specified key.
	 */
	FORCEINLINE ValueType& FindOrAdd(const KeyType&  Key) { return FindOrAddImpl(         Key ); }
	FORCEINLINE ValueType& FindOrAdd(      KeyType&& Key) { return FindOrAddImpl(MoveTemp(Key)); }

	/**
	 * Returns a reference to the value associated with a specified key.
	 *
	 * @param Key The key to search for.
	 * @return The value associated with the specified key, or triggers an assertion if the key does not exist.
	 */
	FORCEINLINE ValueType& FindChecked(KeyConstPointerType Key)
	{
		ValueType* Value = Find(Key);
		check(Value != nullptr);
		return *Value;
	}

	FORCEINLINE const ValueType& FindChecked(KeyConstPointerType Key) const
	{
		const ValueType* Value = Find(Key);
		check(Value != nullptr);
		return *Value;
	}

	/**
	 * Returns the value associated with a specified key.
	 *
	 * @param Key The key to search for.
	 * @return The value associated with the specified key, or the default value for the ValueType if the key isn't contained in this map.
	 */
	FORCEINLINE ValueType FindRef(KeyConstPointerType Key) const
	{
		if (const ValueType* Value = Find(Key))
		{
			return *Value;
		}

		return ValueType();
	}

	/**
	 * Checks if map contains the specified key.
	 *
	 * @param Key The key to check for.
	 * @return true if the map contains the key.
	 */
	FORCEINLINE bool Contains(KeyConstPointerType Key) const
	{
		if (Find(Key))
		{
			return true;
		}
		return false;
	}

	/**
	 * Returns the unique keys contained within this map.
	 *
	 * @param OutKeys Upon return, contains the set of unique keys in this map.
	 * @return The number of unique keys in the map.
	 */
	template<typename Allocator> int32 GetKeys(TArray<KeyType, Allocator>& OutKeys) const
	{
		for (typename ElementArrayType::TConstIterator PairIt(Pairs); PairIt; ++PairIt)
		{
			new(OutKeys) KeyType(PairIt->Key);
		}

		return OutKeys.Num();
	}

	/**
	 * Generates an array from the keys in this map.
	 */
	template<typename Allocator> void GenerateKeyArray(TArray<KeyType, Allocator>& OutArray) const
	{
		OutArray.Empty(Pairs.Num());
		for(typename ElementArrayType::TConstIterator PairIt(Pairs);PairIt;++PairIt)
		{
			new(OutArray) KeyType(PairIt->Key);
		}
	}

	/**
	 * Generates an array from the values in this map.
	 */
	template<typename Allocator> void GenerateValueArray(TArray<ValueType, Allocator>& OutArray) const
	{
		OutArray.Empty(Pairs.Num());
		for(typename ElementArrayType::TConstIterator PairIt(Pairs);PairIt;++PairIt)
		{
			new(OutArray) ValueType(PairIt->Value);
		}
	}

	/** Serializer. */
	FORCEINLINE friend FArchive& operator<<(FArchive& Ar, TSortedMap& Map)
	{
		Ar << Map.Pairs;

		if (Ar.IsLoading())
		{
			// We need to resort, in case the sorting is not consistent with what it was before
			Algo::SortBy(Map.Pairs, FKeyForward(), SortPredicate());
		}
		return Ar;
	}

	/**
	 * Describes the map's contents through an output device.
	 *
	 * @param Ar The output device to describe the map's contents through.
	 */
	void Dump(FOutputDevice& Ar)
	{
		Pairs.Dump(Ar);
	}

	/**
	 * Removes the pair with the specified key and copies the value that was removed to the ref parameter.
	 *
	 * @param Key The key to search for
	 * @param OutRemovedValue If found, the value that was removed (not modified if the key was not found)
	 * @return Whether or not the key was found
	 */
	FORCEINLINE bool RemoveAndCopyValue(KeyInitType Key, ValueType& OutRemovedValue)
	{
		int32 FoundIndex = FindIndex(Key);
		if (FoundIndex == INDEX_NONE)
		{
			return false;
		}

		OutRemovedValue = MoveTemp(Pairs[FoundIndex].Value);
		Pairs.RemoveAt(FoundIndex);
		return true;
	}
	
	/**
	 * Finds a pair with the specified key, removes it from the map, and returns the value part of the pair.
	 * If no pair was found, an exception is thrown.
	 *
	 * @param Key The key to search for
	 * @return Whether or not the key was found
	 */
	FORCEINLINE ValueType FindAndRemoveChecked(KeyConstPointerType Key)
	{
		int32 FoundIndex = FindIndex(Key);
		check(FoundIndex != INDEX_NONE);
		
		ValueType OutRemovedValue = MoveTemp(Pairs[FoundIndex].Value);
		Pairs.RemoveAt(FoundIndex);
		return OutRemovedValue;
	}

	/**
	 * Move all items from another map into our map (if any keys are in both, the value from the other map wins) and empty the other map.
	 *
	 * @param OtherMap The other map of items to move the elements from.
	 */
	template<typename OtherArrayAllocator, typename OtherSortPredicate>
	void Append(TSortedMap<KeyType, ValueType, OtherArrayAllocator, OtherSortPredicate>&& OtherMap)
	{
		this->Reserve(this->Num() + OtherMap.Num());
		for (auto& Pair : OtherMap)
		{
			this->Add(MoveTemp(Pair.Key), MoveTemp(Pair.Value));
		}

		OtherMap.Reset();
	}

	/**
	 * Add all items from another map to our map (if any keys are in both, the value from the other map wins).
	 *
	 * @param OtherMap The other map of items to add.
	 */
	template<typename OtherArrayAllocator, typename OtherSortPredicate>
	void Append(const TSortedMap<KeyType, ValueType, OtherArrayAllocator, OtherSortPredicate>& OtherMap)
	{
		this->Reserve(this->Num() + OtherMap.Num());
		for (auto& Pair : OtherMap)
		{
			this->Add(Pair.Key, Pair.Value);
		}
	}

	FORCEINLINE       ValueType& operator[](KeyConstPointerType Key)       { return this->FindChecked(Key); }
	FORCEINLINE const ValueType& operator[](KeyConstPointerType Key) const { return this->FindChecked(Key); }

private:
	typedef TArray<ElementType, ArrayAllocator> ElementArrayType;

	/** Implementation of find and add */
	template <typename ArgType>
	FORCEINLINE ValueType& FindOrAddImpl(ArgType&& Key)
	{
		if (ValueType* Value = Find(Key))
		{
			return *Value;
		}

		return Add(Forward<ArgType>(Key));
	}

	/** Find index of key */
	FORCEINLINE int32 FindIndex(KeyConstPointerType Key)
	{
		return Algo::BinarySearchBy(Pairs, Key, FKeyForward(), SortPredicate());
	}

	/** Allocates raw memory for emplacing */
	template <typename InitKeyType>
	FORCEINLINE ElementType* AllocateMemoryForEmplace(InitKeyType&& InKey)
	{
		int32 InsertIndex = Algo::LowerBoundBy(Pairs, InKey, FKeyForward(), SortPredicate());
		check(InsertIndex >= 0 && InsertIndex <= Pairs.Num());

		ElementType* DataPtr = nullptr;
		// Since we returned lower bound we already know InKey <= InsertIndex key. So if InKey is not < InsertIndex key, they must be equal
		if (Pairs.IsValidIndex(InsertIndex) && !SortPredicate()(InKey, Pairs[InsertIndex].Key))
		{
			// Replacing element, delete old one
			DataPtr = Pairs.GetData() + InsertIndex;
			DestructItems(DataPtr, 1);
		}
		else
		{
			// Adding new one, this may reallocate Pairs
			Pairs.InsertUninitialized(InsertIndex, 1);
			DataPtr = Pairs.GetData() + InsertIndex;
		}

		return DataPtr;
	}

	/** Forwards sorting into Key of pair */
	struct FKeyForward
	{
		FORCEINLINE const KeyType& operator()(const ElementType& Pair) const
		{
			return Pair.Key;
		}
	};

	/** The base of TSortedMap iterators */
	template<bool bConst>
	class TBaseIterator
	{
	public:
		typedef typename TChooseClass<bConst,typename ElementArrayType::TConstIterator,typename ElementArrayType::TIterator>::Result PairItType;
	private:
		typedef typename TChooseClass<bConst,const TSortedMap,TSortedMap>::Result MapType;
		typedef typename TChooseClass<bConst,const KeyType,KeyType>::Result ItKeyType;
		typedef typename TChooseClass<bConst,const ValueType,ValueType>::Result ItValueType;
		typedef typename TChooseClass<bConst,const typename ElementArrayType::ElementType, typename ElementArrayType::ElementType>::Result PairType;

	protected:
		FORCEINLINE TBaseIterator(const PairItType& InElementIt)
			: PairIt(InElementIt)
		{
		}

	public:
		FORCEINLINE TBaseIterator& operator++()
		{
			++PairIt;
			return *this;
		}

		/** conversion to "bool" returning true if the iterator is valid. */
		FORCEINLINE explicit operator bool() const
		{ 
			return !!PairIt; 
		}

		FORCEINLINE friend bool operator==(const TBaseIterator& Lhs, const TBaseIterator& Rhs) { return Lhs.PairIt == Rhs.PairIt; }
		FORCEINLINE friend bool operator!=(const TBaseIterator& Lhs, const TBaseIterator& Rhs) { return Lhs.PairIt != Rhs.PairIt; }

		FORCEINLINE ItKeyType&   Key()   const { return PairIt->Key; }
		FORCEINLINE ItValueType& Value() const { return PairIt->Value; }

		FORCEINLINE PairType& operator* () const { return  *PairIt; }
		FORCEINLINE PairType* operator->() const { return &*PairIt; }

	protected:
		PairItType PairIt;
	};

	/** An array of the key-value pairs in the map */
	ElementArrayType Pairs;

public:

	/** Map iterator */
	class TIterator : public TBaseIterator<false>
	{
	public:

		FORCEINLINE TIterator(TSortedMap& InMap)
			: TBaseIterator<false>(InMap.Pairs.CreateIterator())
		{
		}

		FORCEINLINE TIterator(const typename TBaseIterator<false>::PairItType& InPairIt)
			: TBaseIterator<false>(InPairIt)
		{
		}

		/** Removes the current pair from the map */
		FORCEINLINE void RemoveCurrent()
		{
			TBaseIterator<false>::PairIt.RemoveCurrent();
		}
	};

	/** Const map iterator */
	class TConstIterator : public TBaseIterator<true>
	{
	public:
		FORCEINLINE TConstIterator(const TSortedMap& InMap)
			: TBaseIterator<true>(InMap.Pairs.CreateConstIterator())
		{
		}

		FORCEINLINE TConstIterator(const typename TBaseIterator<true>::PairItType& InPairIt)
			: TBaseIterator<true>(InPairIt)
		{
		}
	};

	/** Iterates over values associated with a specified key in a const map. This will be at most one value because keys must be unique */
	class TConstKeyIterator : public TBaseIterator<true>
	{
	public:
		FORCEINLINE TConstKeyIterator(const TSortedMap& InMap, KeyInitType InKey)
			: TBaseIterator<true>(InMap.Pairs.CreateIterator())
		{
			int32 NewIndex = FindIndex(InKey);
		
			if (NewIndex != INDEX_NONE)
			{
				TBaseIterator<true>::PairIt += NewIndex;
			}
			else
			{
				TBaseIterator<true>::PairIt.SetToEnd();
			}
		}

		FORCEINLINE TConstKeyIterator& operator++()
		{
			TBaseIterator<true>::PairIt.SetToEnd();
			return *this;
		}
	};

	/** Iterates over values associated with a specified key in a map. This will be at most one value because keys must be unique */
	class TKeyIterator : public TBaseIterator<false>
	{
	public:
		FORCEINLINE TKeyIterator(TSortedMap& InMap, KeyInitType InKey)
			: TBaseIterator<false>(InMap.Pairs.CreateConstIterator())
		{
			int32 NewIndex = FindIndex(InKey);

			if (NewIndex != INDEX_NONE)
			{
				TBaseIterator<true>::PairIt += NewIndex;
			}
			else
			{
				TBaseIterator<true>::PairIt.SetToEnd();
			}
		}

		FORCEINLINE TKeyIterator& operator++()
		{
			TBaseIterator<false>::PairIt.SetToEnd();
			return *this;
		}

		/** Removes the current key-value pair from the map. */
		FORCEINLINE void RemoveCurrent()
		{
			TBaseIterator<false>::PairIt.RemoveCurrent();
			TBaseIterator<false>::PairIt.SetToEnd();
		}
	};

	/** Creates an iterator over all the pairs in this map */
	FORCEINLINE TIterator CreateIterator()
	{
		return TIterator(*this);
	}

	/** Creates a const iterator over all the pairs in this map */
	FORCEINLINE TConstIterator CreateConstIterator() const
	{
		return TConstIterator(*this);
	}

	/** Creates an iterator over the values associated with a specified key in a map. This will be at most one value because keys must be unique */
	FORCEINLINE TKeyIterator CreateKeyIterator(KeyInitType InKey)
	{
		return TKeyIterator(*this, InKey);
	}

	/** Creates a const iterator over the values associated with a specified key in a map. This will be at most one value because keys must be unique */
	FORCEINLINE TConstKeyIterator CreateConstKeyIterator(KeyInitType InKey) const
	{
		return TConstKeyIterator(*this, InKey);
	}

	/** Ranged For iterators. Unlike normal TMap these are not the same as the normal iterator for performance reasons */
	typedef typename ElementArrayType::RangedForIteratorType RangedForIteratorType;
	typedef typename ElementArrayType::RangedForConstIteratorType RangedForConstIteratorType;

private:
	/**
	 * DO NOT USE DIRECTLY
	 * STL-like iterators to enable range-based for loop support.
	 */
	FORCEINLINE friend RangedForIteratorType		begin(TSortedMap& MapBase)		{ return begin(MapBase.Pairs); }
	FORCEINLINE friend RangedForConstIteratorType	begin(const TSortedMap& MapBase){ return begin(MapBase.Pairs); }
	FORCEINLINE friend RangedForIteratorType		end(TSortedMap& MapBase)		{ return end(MapBase.Pairs); }
	FORCEINLINE friend RangedForConstIteratorType	end(const TSortedMap& MapBase)	{ return end(MapBase.Pairs); }
};


template <typename KeyType, typename ValueType, typename ArrayAllocator, typename SortPredicate>
struct TContainerTraits<TSortedMap<KeyType, ValueType, ArrayAllocator, SortPredicate>> : public TContainerTraitsBase<TSortedMap<KeyType, ValueType, ArrayAllocator, SortPredicate>>
{
	enum { MoveWillEmptyContainer = TContainerTraits<typename TSortedMap<KeyType, ValueType, ArrayAllocator, SortPredicate>::ElementArrayType>::MoveWillEmptyContainer };
};
