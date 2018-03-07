// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "HAL/UnrealMemory.h"
#include "Templates/IsTriviallyCopyConstructible.h"
#include "Templates/UnrealTypeTraits.h"
#include "Templates/UnrealTemplate.h"
#include "Templates/IsTriviallyDestructible.h"
#include "Containers/ContainerAllocationPolicies.h"
#include "Templates/Less.h"
#include "Containers/Array.h"
#include "Math/UnrealMathUtility.h"
#include "Containers/ScriptArray.h"
#include "Containers/BitArray.h"


#if UE_BUILD_SHIPPING || UE_BUILD_TEST
	#define TSPARSEARRAY_RANGED_FOR_CHECKS 0
#else
	#define TSPARSEARRAY_RANGED_FOR_CHECKS 1
#endif

// Forward declarations.
template<typename ElementType,typename Allocator = FDefaultSparseArrayAllocator >
class TSparseArray;


/** The result of a sparse array allocation. */
struct FSparseArrayAllocationInfo
{
	int32 Index;
	void* Pointer;
};

/** Allocated elements are overlapped with free element info in the element list. */
template<typename ElementType>
union TSparseArrayElementOrFreeListLink
{
	/** If the element is allocated, its value is stored here. */
	ElementType ElementData;

	struct
	{
		/** If the element isn't allocated, this is a link to the previous element in the array's free list. */
		int32 PrevFreeIndex;

		/** If the element isn't allocated, this is a link to the next element in the array's free list. */
		int32 NextFreeIndex;
	};
};

class FScriptSparseArray;

/**
 * A dynamically sized array where element indices aren't necessarily contiguous.  Memory is allocated for all 
 * elements in the array's index range, so it doesn't save memory; but it does allow O(1) element removal that 
 * doesn't invalidate the indices of subsequent elements.  It uses TArray to store the elements, and a TBitArray
 * to store whether each element index is allocated (for fast iteration over allocated elements).
 *
 **/
template<typename InElementType,typename Allocator /*= FDefaultSparseArrayAllocator */>
class TSparseArray
{
	using ElementType = InElementType;

	friend struct TContainerTraits<TSparseArray>;
	friend class  FScriptSparseArray;

public:

	/** Destructor. */
	~TSparseArray()
	{
		// Destruct the elements in the array.
		Empty();
	}

	/** Marks an index as allocated, and returns information about the allocation. */
	FSparseArrayAllocationInfo AllocateIndex(int32 Index)
	{
		check(Index >= 0);
		check(Index < GetMaxIndex());
		check(!AllocationFlags[Index]);

		// Flag the element as allocated.
		AllocationFlags[Index] = true;

		// Set the allocation info.
		FSparseArrayAllocationInfo Result;
		Result.Index = Index;
		Result.Pointer = &GetData(Result.Index).ElementData;

		return Result;
	}

	/**
	 * Allocates space for an element in the array.  The element is not initialized, and you must use the corresponding placement new operator
	 * to construct the element in the allocated memory.
	 */
	FSparseArrayAllocationInfo AddUninitialized()
	{
		int32 Index;
		if(NumFreeIndices)
		{
			// Remove and use the first index from the list of free elements.
			Index = FirstFreeIndex;
			FirstFreeIndex = GetData(FirstFreeIndex).NextFreeIndex;
			--NumFreeIndices;
			if(NumFreeIndices)
			{
				GetData(FirstFreeIndex).PrevFreeIndex = -1;
			}
		}
		else
		{
			// Add a new element.
			Index = Data.AddUninitialized(1);
			AllocationFlags.Add(false);
		}

		return AllocateIndex(Index);
	}

	/** Adds an element to the array. */
	int32 Add(typename TTypeTraits<ElementType>::ConstInitType Element)
	{
		FSparseArrayAllocationInfo Allocation = AddUninitialized();
		new(Allocation) ElementType(Element);
		return Allocation.Index;
	}

	/**
	 * Allocates space for an element in the array at a given index.  The element is not initialized, and you must use the corresponding placement new operator
	 * to construct the element in the allocated memory.
	 */
	FSparseArrayAllocationInfo InsertUninitialized(int32 Index)
	{
		// Enlarge the array to include the given index.
		if(Index >= Data.Num())
		{
			Data.AddUninitialized(Index + 1 - Data.Num());
			while(AllocationFlags.Num() < Data.Num())
			{
				const int32 FreeIndex = AllocationFlags.Num();
				GetData(FreeIndex).PrevFreeIndex = -1;
				GetData(FreeIndex).NextFreeIndex = FirstFreeIndex;
				if(NumFreeIndices)
				{
					GetData(FirstFreeIndex).PrevFreeIndex = FreeIndex;
				}
				FirstFreeIndex = FreeIndex;
				verify(AllocationFlags.Add(false) == FreeIndex);
				++NumFreeIndices;
			};
		}

		// Verify that the specified index is free.
		check(!AllocationFlags[Index]);

		// Remove the index from the list of free elements.
		--NumFreeIndices;
		const int32 PrevFreeIndex = GetData(Index).PrevFreeIndex;
		const int32 NextFreeIndex = GetData(Index).NextFreeIndex;
		if(PrevFreeIndex != -1)
		{
			GetData(PrevFreeIndex).NextFreeIndex = NextFreeIndex;
		}
		else
		{
			FirstFreeIndex = NextFreeIndex;
		}
		if(NextFreeIndex != -1)
		{
			GetData(NextFreeIndex).PrevFreeIndex = PrevFreeIndex;
		}

		return AllocateIndex(Index);
	}

	/**
	 * Inserts an element to the array.
	 */
	void Insert(int32 Index,typename TTypeTraits<ElementType>::ConstInitType Element)
	{
		new(InsertUninitialized(Index)) ElementType(Element);
	}

	/** Removes Count elements from the array, starting from Index. */
	void RemoveAt(int32 Index,int32 Count = 1)
	{
		if (!TIsTriviallyDestructible<ElementType>::Value)
		{
			for (int32 It = Index, ItCount = Count; ItCount; ++It, --ItCount)
			{
				((ElementType&)GetData(It).ElementData).~ElementType();
			}
		}

		RemoveAtUninitialized(Index, Count);
	}

	/** Removes Count elements from the array, starting from Index, without destructing them. */
	void RemoveAtUninitialized(int32 Index,int32 Count = 1)
	{
		for (; Count; --Count)
		{
			check(AllocationFlags[Index]);

			// Mark the element as free and add it to the free element list.
			if(NumFreeIndices)
			{
				GetData(FirstFreeIndex).PrevFreeIndex = Index;
			}
			auto& IndexData = GetData(Index);
			IndexData.PrevFreeIndex = -1;
			IndexData.NextFreeIndex = NumFreeIndices > 0 ? FirstFreeIndex : INDEX_NONE;
			FirstFreeIndex = Index;
			++NumFreeIndices;
			AllocationFlags[Index] = false;

			++Index;
		}
	}

	/**
	 * Removes all elements from the array, potentially leaving space allocated for an expected number of elements about to be added.
	 * @param ExpectedNumElements - The expected number of elements about to be added.
	 */
	void Empty(int32 ExpectedNumElements = 0)
	{
		// Destruct the allocated elements.
		if( !TIsTriviallyDestructible<ElementType>::Value )
		{
			for(TIterator It(*this);It;++It)
			{
				ElementType& Element = *It;
				Element.~ElementType();
			}
		}

		// Free the allocated elements.
		Data.Empty(ExpectedNumElements);
		FirstFreeIndex = -1;
		NumFreeIndices = 0;
		AllocationFlags.Empty(ExpectedNumElements);
	}

	/** Empties the array, but keep its allocated memory as slack. */
	void Reset()
	{
		// Destruct the allocated elements.
		if( !TIsTriviallyDestructible<ElementType>::Value )
		{
			for(TIterator It(*this);It;++It)
			{
				ElementType& Element = *It;
				Element.~ElementType();
			}
		}

		// Free the allocated elements.
		Data.Reset();
		FirstFreeIndex = -1;
		NumFreeIndices = 0;
		AllocationFlags.Reset();
	}

	/**
	 * Preallocates enough memory to contain the specified number of elements.
	 *
	 * @param	ExpectedNumElements		the total number of elements that the array will have
	 */
	void Reserve(int32 ExpectedNumElements)
	{
		if ( ExpectedNumElements > Data.Num() )
		{
			const int32 ElementsToAdd = ExpectedNumElements - Data.Num();

			// allocate memory in the array itself
			int32 ElementIndex = Data.AddUninitialized(ElementsToAdd);

			// now mark the new elements as free
			for ( int32 FreeIndex = ExpectedNumElements - 1; FreeIndex >= ElementIndex; --FreeIndex )
			{
				if(NumFreeIndices)
				{
					GetData(FirstFreeIndex).PrevFreeIndex = FreeIndex;
				}
				GetData(FreeIndex).PrevFreeIndex = -1;
				GetData(FreeIndex).NextFreeIndex = NumFreeIndices > 0 ? FirstFreeIndex : INDEX_NONE;
				FirstFreeIndex = FreeIndex;
				++NumFreeIndices;
			}
			//@fixme - this will have to do until TBitArray has a Reserve method....
			for ( int32 i = 0; i < ElementsToAdd; i++ )
			{
				AllocationFlags.Add(false);
			}
		}
	}

	/** Shrinks the array's storage to avoid slack. */
	void Shrink()
	{
		// Determine the highest allocated index in the data array.
		int32 MaxAllocatedIndex = INDEX_NONE;
		for(TConstSetBitIterator<typename Allocator::BitArrayAllocator> AllocatedIndexIt(AllocationFlags);AllocatedIndexIt;++AllocatedIndexIt)
		{
			MaxAllocatedIndex = FMath::Max(MaxAllocatedIndex,AllocatedIndexIt.GetIndex());
		}

		const int32 FirstIndexToRemove = MaxAllocatedIndex + 1;
		if(FirstIndexToRemove < Data.Num())
		{
			if(NumFreeIndices > 0)
			{
				// Look for elements in the free list that are in the memory to be freed.
				int32 FreeIndex = FirstFreeIndex;
				while(FreeIndex != INDEX_NONE)
				{
					if(FreeIndex >= FirstIndexToRemove)
					{
						const int32 PrevFreeIndex = GetData(FreeIndex).PrevFreeIndex;
						const int32 NextFreeIndex = GetData(FreeIndex).NextFreeIndex;
						if(NextFreeIndex != -1)
						{
							GetData(NextFreeIndex).PrevFreeIndex = PrevFreeIndex;
						}
						if(PrevFreeIndex != -1)
						{
							GetData(PrevFreeIndex).NextFreeIndex = NextFreeIndex;
						}
						else
						{
							FirstFreeIndex = NextFreeIndex;
						}
						--NumFreeIndices;

						FreeIndex = NextFreeIndex;
					}
					else
					{
						FreeIndex = GetData(FreeIndex).NextFreeIndex;
					}
				}
			}

			// Truncate unallocated elements at the end of the data array.
			Data.RemoveAt(FirstIndexToRemove,Data.Num() - FirstIndexToRemove);
			AllocationFlags.RemoveAt(FirstIndexToRemove,AllocationFlags.Num() - FirstIndexToRemove);
		}

		// Shrink the data array.
		Data.Shrink();
	}

	/** Compacts the allocated elements into a contiguous index range. */
	/** Returns true if any elements were relocated, false otherwise. */
	bool Compact()
	{
		int32 NumFree = NumFreeIndices;
		if (NumFree == 0)
		{
			return false;
		}

		bool bResult = false;

		FElementOrFreeListLink* ElementData = Data.GetData();

		int32 EndIndex    = Data.Num();
		int32 TargetIndex = EndIndex - NumFree;
		int32 FreeIndex   = FirstFreeIndex;
		while (FreeIndex != -1)
		{
			int32 NextFreeIndex = GetData(FreeIndex).NextFreeIndex;
			if (FreeIndex < TargetIndex)
			{
				// We need an element here
				do
				{
					--EndIndex;
				}
				while (!AllocationFlags[EndIndex]);

				RelocateConstructItems<FElementOrFreeListLink>(ElementData + FreeIndex, ElementData + EndIndex, 1);
				AllocationFlags[FreeIndex] = true;

				bResult = true;
			}

			FreeIndex = NextFreeIndex;
		}

		Data           .RemoveAt(TargetIndex, NumFree);
		AllocationFlags.RemoveAt(TargetIndex, NumFree);

		NumFreeIndices = 0;
		FirstFreeIndex = -1;

		return bResult;
	}

	/** Compacts the allocated elements into a contiguous index range. Does not change the iteration order of the elements. */
	/** Returns true if any elements were relocated, false otherwise. */
	bool CompactStable()
	{
		if (NumFreeIndices == 0)
		{
			return false;
		}

		// Copy the existing elements to a new array.
		TSparseArray<ElementType,Allocator> CompactedArray;
		CompactedArray.Empty(Num());
		for(TConstIterator It(*this);It;++It)
		{
			new(CompactedArray.AddUninitialized()) ElementType(*It);
		}

		// Replace this array with the compacted array.
		Exchange(*this,CompactedArray);

		return true;
	}

	/** Sorts the elements using the provided comparison class. */
	template<typename PREDICATE_CLASS>
	void Sort( const PREDICATE_CLASS& Predicate )
	{
		if(Num() > 0)
		{
			// Compact the elements array so all the elements are contiguous.
			Compact();

			// Sort the elements according to the provided comparison class.
			::Sort( &GetData(0), Num(), FElementCompareClass< PREDICATE_CLASS >( Predicate ) );
		}
	}

	/** Sorts the elements assuming < operator is defined for ElementType. */
	void Sort()
	{
		Sort( TLess< ElementType >() );
	}

	/** 
	 * Helper function to return the amount of memory allocated by this container 
	 * @return number of bytes allocated by this container
	 */
	uint32 GetAllocatedSize( void ) const
	{
		return	(Data.Num() + Data.GetSlack()) * sizeof(FElementOrFreeListLink) +
				AllocationFlags.GetAllocatedSize();
	}

	/** Tracks the container's memory use through an archive. */
	void CountBytes(FArchive& Ar)
	{
		Data.CountBytes(Ar);
		AllocationFlags.CountBytes(Ar);
	}

	/** Serializer. */
	friend FArchive& operator<<(FArchive& Ar,TSparseArray& Array)
	{
		Array.CountBytes(Ar);
		if( Ar.IsLoading() )
		{
			// Load array.
			int32 NewNumElements = 0;
			Ar << NewNumElements;
			Array.Empty( NewNumElements );
			for(int32 ElementIndex = 0;ElementIndex < NewNumElements;ElementIndex++)
			{
				Ar << *::new(Array.AddUninitialized())ElementType;
			}
		}
		else
		{
			// Save array.
			int32 NewNumElements = Array.Num();
			Ar << NewNumElements;
			for(TIterator It(Array);It;++It)
			{
				Ar << *It;
			}
		}
		return Ar;
	}

	/**
	 * Equality comparison operator.
	 * Checks that both arrays have the same elements and element indices; that means that unallocated elements are signifigant!
	 */
	friend bool operator==(const TSparseArray& A,const TSparseArray& B)
	{
		if(A.GetMaxIndex() != B.GetMaxIndex())
		{
			return false;
		}

		for(int32 ElementIndex = 0;ElementIndex < A.GetMaxIndex();ElementIndex++)
		{
			const bool bIsAllocatedA = A.IsAllocated(ElementIndex);
			const bool bIsAllocatedB = B.IsAllocated(ElementIndex);
			if(bIsAllocatedA != bIsAllocatedB)
			{
				return false;
			}
			else if(bIsAllocatedA)
			{
				if(A[ElementIndex] != B[ElementIndex])
				{
					return false;
				}
			}
		}

		return true;
	}

	/**
	 * Inequality comparison operator.
	 * Checks that both arrays have the same elements and element indices; that means that unallocated elements are signifigant!
	 */
	friend bool operator!=(const TSparseArray& A,const TSparseArray& B)
	{
		return !(A == B);
	}

	/** Default constructor. */
	TSparseArray()
	:	FirstFreeIndex(-1)
	,	NumFreeIndices(0)
	{}

	/** Move constructor. */
	TSparseArray(TSparseArray&& InCopy)
	{
		MoveOrCopy(*this, InCopy);
	}

	/** Copy constructor. */
	TSparseArray(const TSparseArray& InCopy)
	:	FirstFreeIndex(-1)
	,	NumFreeIndices(0)
	{
		*this = InCopy;
	}

	/** Move assignment operator. */
	TSparseArray& operator=(TSparseArray&& InCopy)
	{
		if(this != &InCopy)
		{
			MoveOrCopy(*this, InCopy);
		}
		return *this;
	}

	/** Copy assignment operator. */
	TSparseArray& operator=(const TSparseArray& InCopy)
	{
		if (this != &InCopy)
		{
			int32 SrcMax = InCopy.GetMaxIndex();

			// Reallocate the array.
			Empty(SrcMax);
			Data.AddUninitialized(SrcMax);

			// Copy the other array's element allocation state.
			FirstFreeIndex  = InCopy.FirstFreeIndex;
			NumFreeIndices  = InCopy.NumFreeIndices;
			AllocationFlags = InCopy.AllocationFlags;

			// Determine whether we need per element construction or bulk copy is fine
			if (!TIsTriviallyCopyConstructible<ElementType>::Value)
			{
				      FElementOrFreeListLink* DestData = (FElementOrFreeListLink*)Data.GetData();
				const FElementOrFreeListLink* SrcData  = (FElementOrFreeListLink*)InCopy.Data.GetData();

				// Use the inplace new to copy the element to an array element
				for (int32 Index = 0; Index < SrcMax; ++Index)
				{
					      FElementOrFreeListLink& DestElement = DestData[Index];
					const FElementOrFreeListLink& SrcElement  = SrcData [Index];
					if (InCopy.IsAllocated(Index))
					{
						::new((uint8*)&DestElement.ElementData) ElementType(*(const ElementType*)&SrcElement.ElementData);
					}
					else
					{
						DestElement.PrevFreeIndex = SrcElement.PrevFreeIndex;
						DestElement.NextFreeIndex = SrcElement.NextFreeIndex;
					}
				}
			}
			else
			{
				// Use the much faster path for types that allow it
				FMemory::Memcpy(Data.GetData(), InCopy.Data.GetData(), sizeof(FElementOrFreeListLink) * SrcMax);
			}
		}
		return *this;
	}

private:
	template <typename SparseArrayType>
	FORCEINLINE static typename TEnableIf<TContainerTraits<SparseArrayType>::MoveWillEmptyContainer>::Type MoveOrCopy(SparseArrayType& ToArray, SparseArrayType& FromArray)
	{
		// Destruct the allocated elements.
		if( !TIsTriviallyDestructible<ElementType>::Value )
		{
			for (ElementType& Element : ToArray)
			{
				DestructItem(&Element);
			}
		}

		ToArray.Data            = (DataType&&)FromArray.Data;
		ToArray.AllocationFlags = (AllocationBitArrayType&&)FromArray.AllocationFlags;

		ToArray.FirstFreeIndex = FromArray.FirstFreeIndex;
		ToArray.NumFreeIndices = FromArray.NumFreeIndices;
		FromArray.FirstFreeIndex = -1;
		FromArray.NumFreeIndices = 0;
	}

	template <typename SparseArrayType>
	FORCEINLINE static typename TEnableIf<!TContainerTraits<SparseArrayType>::MoveWillEmptyContainer>::Type MoveOrCopy(SparseArrayType& ToArray, SparseArrayType& FromArray)
	{
		ToArray = FromArray;
	}

public:
	// Accessors.
	ElementType& operator[](int32 Index)
	{
		checkSlow(Index >= 0 && Index < Data.Num() && Index < AllocationFlags.Num());
		//checkSlow(AllocationFlags[Index]); // Disabled to improve loading times -BZ
		return *(ElementType*)&GetData(Index).ElementData;
	}
	const ElementType& operator[](int32 Index) const
	{
		checkSlow(Index >= 0 && Index < Data.Num() && Index < AllocationFlags.Num());
		//checkSlow(AllocationFlags[Index]); // Disabled to improve loading times -BZ
		return *(ElementType*)&GetData(Index).ElementData;
	}

	bool IsAllocated(int32 Index) const { return AllocationFlags[Index]; }
	int32 GetMaxIndex() const { return Data.Num(); }
	int32 Num() const { return Data.Num() - NumFreeIndices; }

	/**
	 * Checks that the specified address is not part of an element within the container.  Used for implementations
	 * to check that reference arguments aren't going to be invalidated by possible reallocation.
	 *
	 * @param Addr The address to check.
	 */
	FORCEINLINE void CheckAddress(const ElementType* Addr) const
	{
		Data.CheckAddress(Addr);
	}

private:

	/** The base class of sparse array iterators. */
	template<bool bConst>
	class TBaseIterator
	{
	public:
		typedef TConstSetBitIterator<typename Allocator::BitArrayAllocator> BitArrayItType;

	private:
		typedef typename TChooseClass<bConst,const TSparseArray,TSparseArray>::Result ArrayType;
		typedef typename TChooseClass<bConst,const ElementType,ElementType>::Result ItElementType;

	public:
		explicit TBaseIterator(ArrayType& InArray, const BitArrayItType& InBitArrayIt)
			: Array     (InArray)
			, BitArrayIt(InBitArrayIt)
		{
		}

		FORCEINLINE TBaseIterator& operator++()
		{
			// Iterate to the next set allocation flag.
			++BitArrayIt;
			return *this;
		}

		FORCEINLINE int32 GetIndex() const { return BitArrayIt.GetIndex(); }

		FORCEINLINE friend bool operator==(const TBaseIterator& Lhs, const TBaseIterator& Rhs) { return Lhs.BitArrayIt == Rhs.BitArrayIt && &Lhs.Array == &Rhs.Array; }
		FORCEINLINE friend bool operator!=(const TBaseIterator& Lhs, const TBaseIterator& Rhs) { return Lhs.BitArrayIt != Rhs.BitArrayIt || &Lhs.Array != &Rhs.Array; }

		/** conversion to "bool" returning true if the iterator is valid. */
		FORCEINLINE explicit operator bool() const
		{ 
			return !!BitArrayIt; 
		}

		/** inverse of the "bool" operator */
		FORCEINLINE bool operator !() const 
		{
			return !(bool)*this;
		}

		FORCEINLINE ItElementType& operator*() const { return Array[GetIndex()]; }
		FORCEINLINE ItElementType* operator->() const { return &Array[GetIndex()]; }
		FORCEINLINE const FRelativeBitReference& GetRelativeBitReference() const { return BitArrayIt; }

	protected:
		ArrayType&     Array;
		BitArrayItType BitArrayIt;
	};

public:

	/** Iterates over all allocated elements in a sparse array. */
	class TIterator : public TBaseIterator<false>
	{
	public:
		TIterator(TSparseArray& InArray)
			: TBaseIterator<false>(InArray, TConstSetBitIterator<typename Allocator::BitArrayAllocator>(InArray.AllocationFlags))
		{
		}

		TIterator(TSparseArray& InArray, const typename TBaseIterator<false>::BitArrayItType& InBitArrayIt)
			: TBaseIterator<false>(InArray, InBitArrayIt)
		{
		}

		/** Safely removes the current element from the array. */
		void RemoveCurrent()
		{
			this->Array.RemoveAt(this->GetIndex());
		}
	};

	/** Iterates over all allocated elements in a const sparse array. */
	class TConstIterator : public TBaseIterator<true>
	{
	public:
		TConstIterator(const TSparseArray& InArray)
			: TBaseIterator<true>(InArray, TConstSetBitIterator<typename Allocator::BitArrayAllocator>(InArray.AllocationFlags))
		{
		}

		TConstIterator(const TSparseArray& InArray, const typename TBaseIterator<true>::BitArrayItType& InBitArrayIt)
			: TBaseIterator<true>(InArray, InBitArrayIt)
		{
		}
	};

	#if TSPARSEARRAY_RANGED_FOR_CHECKS
		class TRangedForIterator : public TIterator
		{
		public:
			TRangedForIterator(TSparseArray& InArray, const typename TBaseIterator<false>::BitArrayItType& InBitArrayIt)
				: TIterator (InArray, InBitArrayIt)
				, InitialNum(InArray.Num())
			{
			}

		private:
			int32 InitialNum;

			friend FORCEINLINE bool operator!=(const TRangedForIterator& Lhs, const TRangedForIterator& Rhs)
			{
				// We only need to do the check in this operator, because no other operator will be
				// called until after this one returns.
				//
				// Also, we should only need to check one side of this comparison - if the other iterator isn't
				// even from the same array then the compiler has generated bad code.
				ensureMsgf(Lhs.Array.Num() == Lhs.InitialNum, TEXT("Container has changed during ranged-for iteration!"));
				return *(TIterator*)&Lhs != *(TIterator*)&Rhs;
			}
		};

		class TRangedForConstIterator : public TConstIterator
		{
		public:
			TRangedForConstIterator(const TSparseArray& InArray, const typename TBaseIterator<true>::BitArrayItType& InBitArrayIt)
				: TConstIterator(InArray, InBitArrayIt)
				, InitialNum    (InArray.Num())
			{
			}

		private:
			int32 InitialNum;

			friend FORCEINLINE bool operator!=(const TRangedForConstIterator& Lhs, const TRangedForConstIterator& Rhs)
			{
				// We only need to do the check in this operator, because no other operator will be
				// called until after this one returns.
				//
				// Also, we should only need to check one side of this comparison - if the other iterator isn't
				// even from the same array then the compiler has generated bad code.
				ensureMsgf(Lhs.Array.Num() == Lhs.InitialNum, TEXT("Container has changed during ranged-for iteration!"));
				return *(TIterator*)&Lhs != *(TIterator*)&Rhs;
			}
		};
	#else
		using TRangedForIterator      = TIterator;
		using TRangedForConstIterator = TConstIterator;
	#endif

	/** Creates an iterator for the contents of this array */
	TIterator CreateIterator()
	{
		return TIterator(*this);
	}

	/** Creates a const iterator for the contents of this array */
	TConstIterator CreateConstIterator() const
	{
		return TConstIterator(*this);
	}

private:
	/**
	 * DO NOT USE DIRECTLY
	 * STL-like iterators to enable range-based for loop support.
	 */
	FORCEINLINE friend TRangedForIterator      begin(      TSparseArray& Array) { return TRangedForIterator     (Array, TConstSetBitIterator<typename Allocator::BitArrayAllocator>(Array.AllocationFlags)); }
	FORCEINLINE friend TRangedForConstIterator begin(const TSparseArray& Array) { return TRangedForConstIterator(Array, TConstSetBitIterator<typename Allocator::BitArrayAllocator>(Array.AllocationFlags)); }
	FORCEINLINE friend TRangedForIterator      end  (      TSparseArray& Array) { return TRangedForIterator     (Array, TConstSetBitIterator<typename Allocator::BitArrayAllocator>(Array.AllocationFlags, Array.AllocationFlags.Num())); }
	FORCEINLINE friend TRangedForConstIterator end  (const TSparseArray& Array) { return TRangedForConstIterator(Array, TConstSetBitIterator<typename Allocator::BitArrayAllocator>(Array.AllocationFlags, Array.AllocationFlags.Num())); }

public:
	/** An iterator which only iterates over the elements of the array which correspond to set bits in a separate bit array. */
	template<typename SubsetAllocator = FDefaultBitArrayAllocator>
	class TConstSubsetIterator
	{
	public:
		TConstSubsetIterator( const TSparseArray& InArray, const TBitArray<SubsetAllocator>& InBitArray ):
			Array(InArray),
			BitArrayIt(InArray.AllocationFlags,InBitArray)
		{}
		FORCEINLINE TConstSubsetIterator& operator++()
		{
			// Iterate to the next element which is both allocated and has its bit set in the other bit array.
			++BitArrayIt;
			return *this;
		}
		FORCEINLINE int32 GetIndex() const { return BitArrayIt.GetIndex(); }
		
		/** conversion to "bool" returning true if the iterator is valid. */
		FORCEINLINE explicit operator bool() const
		{ 
			return !!BitArrayIt; 
		}
		/** inverse of the "bool" operator */
		FORCEINLINE bool operator !() const 
		{
			return !(bool)*this;
		}

		FORCEINLINE const ElementType& operator*() const { return Array(GetIndex()); }
		FORCEINLINE const ElementType* operator->() const { return &Array(GetIndex()); }
		FORCEINLINE const FRelativeBitReference& GetRelativeBitReference() const { return BitArrayIt; }
	private:
		const TSparseArray& Array;
		TConstDualSetBitIterator<typename Allocator::BitArrayAllocator,SubsetAllocator> BitArrayIt;
	};

	/** Concatenation operators */
	TSparseArray& operator+=( const TSparseArray& OtherArray )
	{
		this->Reserve(this->Num() + OtherArray.Num());
		for ( typename TSparseArray::TConstIterator It(OtherArray); It; ++It )
		{
			this->Add(*It);
		}
		return *this;
	}
	TSparseArray& operator+=( const TArray<ElementType>& OtherArray )
	{
		this->Reserve(this->Num() + OtherArray.Num());
		for ( int32 Idx = 0; Idx < OtherArray.Num(); Idx++ )
		{
			this->Add(OtherArray[Idx]);
		}
		return *this;
	}

private:

	/**
	 * The element type stored is only indirectly related to the element type requested, to avoid instantiating TArray redundantly for
	 * compatible types.
	 */
	typedef TSparseArrayElementOrFreeListLink<
		TAlignedBytes<sizeof(ElementType), alignof(ElementType)>
		> FElementOrFreeListLink;

	/** Extracts the element value from the array's element structure and passes it to the user provided comparison class. */
	template <typename PREDICATE_CLASS>
	class FElementCompareClass
	{
		const PREDICATE_CLASS& Predicate;

	public:
		FElementCompareClass( const PREDICATE_CLASS& InPredicate )
			: Predicate( InPredicate )
		{}

		bool operator()( const FElementOrFreeListLink& A,const FElementOrFreeListLink& B ) const
		{
			return Predicate(*(ElementType*)&A.ElementData,*(ElementType*)&B.ElementData);
		}
	};

	/** Accessor for the element or free list data. */
	FElementOrFreeListLink& GetData(int32 Index)
	{
		return ((FElementOrFreeListLink*)Data.GetData())[Index];
	}

	/** Accessor for the element or free list data. */
	const FElementOrFreeListLink& GetData(int32 Index) const
	{
		return ((FElementOrFreeListLink*)Data.GetData())[Index];
	}

	typedef TArray<FElementOrFreeListLink,typename Allocator::ElementAllocator> DataType;
	DataType Data;

	typedef TBitArray<typename Allocator::BitArrayAllocator> AllocationBitArrayType;
	AllocationBitArrayType AllocationFlags;

	/** The index of an unallocated element in the array that currently contains the head of the linked list of free elements. */
	int32 FirstFreeIndex;

	/** The number of elements in the free list. */
	int32 NumFreeIndices;
};

template<typename ElementType, typename Allocator>
struct TContainerTraits<TSparseArray<ElementType, Allocator> > : public TContainerTraitsBase<TSparseArray<ElementType, Allocator> >
{
	enum { MoveWillEmptyContainer =
		TContainerTraits<typename TSparseArray<ElementType, Allocator>::DataType>::MoveWillEmptyContainer &&
		TContainerTraits<typename TSparseArray<ElementType, Allocator>::AllocationBitArrayType>::MoveWillEmptyContainer };
};

struct FScriptSparseArrayLayout
{
	int32 ElementOffset;
	int32 Alignment;
	int32 Size;
};

// Untyped sparse array type for accessing TSparseArray data, like FScriptArray for TArray.
// Must have the same memory representation as a TSet.
class FScriptSparseArray
{
public:
	static FScriptSparseArrayLayout GetScriptLayout(int32 ElementSize, int32 ElementAlignment)
	{
		FScriptSparseArrayLayout Result;
		Result.ElementOffset = 0;
		Result.Alignment     = FMath::Max(ElementAlignment, (int32)alignof(FFreeListLink));
		Result.Size          = FMath::Max(ElementSize,      (int32)sizeof (FFreeListLink));

		return Result;
	}

	FScriptSparseArray()
		: FirstFreeIndex(-1)
		, NumFreeIndices(0)
	{
	}

	bool IsValidIndex(int32 Index) const
	{
		return AllocationFlags.IsValidIndex(Index) && AllocationFlags[Index];
	}

	int32 Num() const
	{
		return Data.Num() - NumFreeIndices;
	}

	int32 GetMaxIndex() const
	{
		return Data.Num();
	}

	void* GetData(int32 Index, const FScriptSparseArrayLayout& Layout)
	{
		return (uint8*)Data.GetData() + Layout.Size * Index;
	}

	const void* GetData(int32 Index, const FScriptSparseArrayLayout& Layout) const
	{
		return (const uint8*)Data.GetData() + Layout.Size * Index;
	}

	void Empty(int32 Slack, const FScriptSparseArrayLayout& Layout)
	{
		// Free the allocated elements.
		Data.Empty(Slack, Layout.Size);
		FirstFreeIndex = -1;
		NumFreeIndices = 0;
		AllocationFlags.Empty(Slack);
	}

	/**
	 * Adds an uninitialized object to the array.
	 *
	 * @return  The index of the added element.
	 */
	int32 AddUninitialized(const FScriptSparseArrayLayout& Layout)
	{
		int32 Index;
		if (NumFreeIndices)
		{
			// Remove and use the first index from the list of free elements.
			Index = FirstFreeIndex;
			FirstFreeIndex = GetFreeListLink(FirstFreeIndex, Layout)->NextFreeIndex;
			--NumFreeIndices;
			if(NumFreeIndices)
			{
				GetFreeListLink(FirstFreeIndex, Layout)->PrevFreeIndex = -1;
			}
		}
		else
		{
			// Add a new element.
			Index = Data.Add(1, Layout.Size);
			AllocationFlags.Add(false);
		}

		AllocationFlags[Index] = true;

		return Index;
	}

	/** Removes Count elements from the array, starting from Index, without destructing them. */
	void RemoveAtUninitialized(const FScriptSparseArrayLayout& Layout, int32 Index, int32 Count = 1)
	{
		for (; Count; --Count)
		{
			check(AllocationFlags[Index]);

			// Mark the element as free and add it to the free element list.
			if(NumFreeIndices)
			{
				GetFreeListLink(FirstFreeIndex, Layout)->PrevFreeIndex = Index;
			}

			auto* IndexData = GetFreeListLink(Index, Layout);
			IndexData->PrevFreeIndex = -1;
			IndexData->NextFreeIndex = NumFreeIndices > 0 ? FirstFreeIndex : INDEX_NONE;
			FirstFreeIndex = Index;
			++NumFreeIndices;
			AllocationFlags[Index] = false;

			++Index;
		}
	}

private:
	FScriptArray    Data;
	FScriptBitArray AllocationFlags;
	int32           FirstFreeIndex;
	int32           NumFreeIndices;

	// This function isn't intended to be called, just to be compiled to validate the correctness of the type.
	static void CheckConstraints()
	{
		typedef FScriptSparseArray  ScriptType;
		typedef TSparseArray<int32> RealType;

		// Check that the class footprint is the same
		static_assert(sizeof (ScriptType) == sizeof (RealType), "FScriptSparseArray's size doesn't match TSparseArray");
		static_assert(alignof(ScriptType) == alignof(RealType), "FScriptSparseArray's alignment doesn't match TSparseArray");

		// Check member sizes
		static_assert(sizeof(DeclVal<ScriptType>().Data)            == sizeof(DeclVal<RealType>().Data),            "FScriptSparseArray's Data member size does not match TSparseArray's");
		static_assert(sizeof(DeclVal<ScriptType>().AllocationFlags) == sizeof(DeclVal<RealType>().AllocationFlags), "FScriptSparseArray's AllocationFlags member size does not match TSparseArray's");
		static_assert(sizeof(DeclVal<ScriptType>().FirstFreeIndex)  == sizeof(DeclVal<RealType>().FirstFreeIndex),  "FScriptSparseArray's FirstFreeIndex member size does not match TSparseArray's");
		static_assert(sizeof(DeclVal<ScriptType>().NumFreeIndices)  == sizeof(DeclVal<RealType>().NumFreeIndices),  "FScriptSparseArray's NumFreeIndices member size does not match TSparseArray's");

		// Check member offsets
		static_assert(STRUCT_OFFSET(ScriptType, Data)            == STRUCT_OFFSET(RealType, Data),            "FScriptSparseArray's Data member offset does not match TSparseArray's");
		static_assert(STRUCT_OFFSET(ScriptType, AllocationFlags) == STRUCT_OFFSET(RealType, AllocationFlags), "FScriptSparseArray's AllocationFlags member offset does not match TSparseArray's");
		static_assert(STRUCT_OFFSET(ScriptType, FirstFreeIndex)  == STRUCT_OFFSET(RealType, FirstFreeIndex),  "FScriptSparseArray's FirstFreeIndex member offset does not match TSparseArray's");
		static_assert(STRUCT_OFFSET(ScriptType, NumFreeIndices)  == STRUCT_OFFSET(RealType, NumFreeIndices),  "FScriptSparseArray's NumFreeIndices member offset does not match TSparseArray's");

		// Check free index offsets
		static_assert(STRUCT_OFFSET(ScriptType::FFreeListLink, PrevFreeIndex) == STRUCT_OFFSET(RealType::FElementOrFreeListLink, PrevFreeIndex), "FScriptSparseArray's FFreeListLink's PrevFreeIndex member offset does not match TSparseArray's");
		static_assert(STRUCT_OFFSET(ScriptType::FFreeListLink, NextFreeIndex) == STRUCT_OFFSET(RealType::FElementOrFreeListLink, NextFreeIndex), "FScriptSparseArray's FFreeListLink's NextFreeIndex member offset does not match TSparseArray's");
	}

	struct FFreeListLink
	{
		/** If the element isn't allocated, this is a link to the previous element in the array's free list. */
		int32 PrevFreeIndex;

		/** If the element isn't allocated, this is a link to the next element in the array's free list. */
		int32 NextFreeIndex;
	};

	/** Accessor for the element or free list data. */
	FORCEINLINE FFreeListLink* GetFreeListLink(int32 Index, const FScriptSparseArrayLayout& Layout)
	{
		return (FFreeListLink*)GetData(Index, Layout);
	}

public:
	// These should really be private, because they shouldn't be called, but there's a bunch of code
	// that needs to be fixed first.
	FScriptSparseArray(const FScriptSparseArray&) { check(false); }
	void operator=(const FScriptSparseArray&) { check(false); }
};

template <>
struct TIsZeroConstructType<FScriptSparseArray>
{
	enum { Value = true };
};

/**
 * A placement new operator which constructs an element in a sparse array allocation.
 */
inline void* operator new(size_t Size,const FSparseArrayAllocationInfo& Allocation)
{
	ASSUME(Allocation.Pointer);
	return Allocation.Pointer;
}
