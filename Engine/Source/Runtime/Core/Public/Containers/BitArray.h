// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "HAL/UnrealMemory.h"
#include "Templates/UnrealTypeTraits.h"
#include "Templates/UnrealTemplate.h"
#include "Containers/ContainerAllocationPolicies.h"
#include "Serialization/Archive.h"
#include "Math/UnrealMathUtility.h"

template<typename Allocator > class TBitArray;

// Functions for manipulating bit sets.
struct FBitSet
{
	/** Clears the next set bit in the mask and returns its index. */
	static FORCEINLINE uint32 GetAndClearNextBit(uint32& Mask)
	{
		const uint32 LowestBitMask = (Mask) & (-(int32)Mask);
		const uint32 BitIndex = FMath::FloorLog2(LowestBitMask);
		Mask ^= LowestBitMask;
		return BitIndex;
	}
};


// Forward declaration.
template<typename Allocator = FDefaultBitArrayAllocator>
class TBitArray;

template<typename Allocator = FDefaultBitArrayAllocator>
class TConstSetBitIterator;

template<typename Allocator = FDefaultBitArrayAllocator,typename OtherAllocator = FDefaultBitArrayAllocator>
class TConstDualSetBitIterator;

class FScriptBitArray;


/**
 * Serializer (predefined for no friend injection in gcc 411)
 */
template<typename Allocator>
FArchive& operator<<(FArchive& Ar, TBitArray<Allocator>& BitArray);

/** Used to read/write a bit in the array as a bool. */
class FBitReference
{
public:

	FORCEINLINE FBitReference(uint32& InData,uint32 InMask)
	:	Data(InData)
	,	Mask(InMask)
	{}

	FORCEINLINE operator bool() const
	{
		 return (Data & Mask) != 0;
	}
	FORCEINLINE void operator=(const bool NewValue)
	{
		if(NewValue)
		{
			Data |= Mask;
		}
		else
		{
			Data &= ~Mask;
		}
	}
	FORCEINLINE void AtomicSet(const bool NewValue)
	{
		if(NewValue)
		{
			if (!(Data & Mask))
			{
				while (1)
				{
					uint32 Current = Data;
					uint32 Desired = Current | Mask;
					if (Current == Desired || FPlatformAtomics::InterlockedCompareExchange((volatile int32*)&Data, (int32)Desired, (int32)Current) == (int32)Current)
					{
						return;
					}
				}
			}
		}
		else
		{
			if (Data & Mask)
			{
				while (1)
				{
					uint32 Current = Data;
					uint32 Desired = Current & ~Mask;
					if (Current == Desired || FPlatformAtomics::InterlockedCompareExchange((volatile int32*)&Data, (int32)Desired, (int32)Current) == (int32)Current)
					{
						return;
					}
				}
			}
		}
	}
	FORCEINLINE FBitReference& operator=(const FBitReference& Copy)
	{
		// As this is emulating a reference, assignment should not rebind,
		// it should write to the referenced bit.
		*this = (bool)Copy;
		return *this;
	}

private:
	uint32& Data;
	uint32 Mask;
};


/** Used to read a bit in the array as a bool. */
class FConstBitReference
{
public:

	FORCEINLINE FConstBitReference(const uint32& InData,uint32 InMask)
	:	Data(InData)
	,	Mask(InMask)
	{}

	FORCEINLINE operator bool() const
	{
		 return (Data & Mask) != 0;
	}

private:
	const uint32& Data;
	uint32 Mask;
};


/** Used to reference a bit in an unspecified bit array. */
class FRelativeBitReference
{
public:
	FORCEINLINE explicit FRelativeBitReference(int32 BitIndex)
		: DWORDIndex(BitIndex >> NumBitsPerDWORDLogTwo)
		, Mask(1 << (BitIndex & (NumBitsPerDWORD - 1)))
	{
	}

	int32  DWORDIndex;
	uint32 Mask;
};


/**
 * A dynamically sized bit array.
 * An array of Booleans.  They stored in one bit/Boolean.  There are iterators that efficiently iterate over only set bits.
 */
template<typename Allocator /*= FDefaultBitArrayAllocator*/>
class TBitArray
{
	friend class FScriptBitArray;

public:

	template<typename>
	friend class TConstSetBitIterator;

	template<typename,typename>
	friend class TConstDualSetBitIterator;

	/**
	 * Minimal initialization constructor.
	 * @param Value - The value to initial the bits to.
	 * @param InNumBits - The initial number of bits in the array.
	 */
	explicit TBitArray( const bool Value = false, const int32 InNumBits = 0 )
	:	NumBits(0)
	,	MaxBits(0)
	{
		Init(Value,InNumBits);
	}

	/**
	 * Move constructor.
	 */
	FORCEINLINE TBitArray(TBitArray&& Other)
	{
		MoveOrCopy(*this, Other);
	}

	/**
	 * Copy constructor.
	 */
	FORCEINLINE TBitArray(const TBitArray& Copy)
	:	NumBits(0)
	,	MaxBits(0)
	{
		*this = Copy;
	}

	/**
	 * Move assignment.
	 */
	FORCEINLINE TBitArray& operator=(TBitArray&& Other)
	{
		if (this != &Other)
		{
			MoveOrCopy(*this, Other);
		}

		return *this;
	}

	/**
	 * Assignment operator.
	 */
	FORCEINLINE TBitArray& operator=(const TBitArray& Copy)
	{
		// check for self assignment since we don't use swap() mechanic
		if( this == &Copy )
		{
			return *this;
		}

		Empty(Copy.Num());
		NumBits = MaxBits = Copy.NumBits;
		if(NumBits)
		{
			const int32 NumDWORDs = FMath::DivideAndRoundUp(MaxBits, NumBitsPerDWORD);
			Realloc(0);
			FMemory::Memcpy(GetData(),Copy.GetData(),NumDWORDs * sizeof(uint32));
		}
		return *this;
	}

private:
	template <typename BitArrayType>
	static FORCEINLINE typename TEnableIf<TContainerTraits<BitArrayType>::MoveWillEmptyContainer>::Type MoveOrCopy(BitArrayType& ToArray, BitArrayType& FromArray)
	{
		ToArray.AllocatorInstance.MoveToEmpty(FromArray.AllocatorInstance);

		ToArray  .NumBits = FromArray.NumBits;
		ToArray  .MaxBits = FromArray.MaxBits;
		FromArray.NumBits = 0;
		FromArray.MaxBits = 0;
	}

	template <typename BitArrayType>
	static FORCEINLINE typename TEnableIf<!TContainerTraits<BitArrayType>::MoveWillEmptyContainer>::Type MoveOrCopy(BitArrayType& ToArray, BitArrayType& FromArray)
	{
		ToArray = FromArray;
	}

public:
	/**
	 * Serializer
	 */
	friend FArchive& operator<<(FArchive& Ar, TBitArray& BitArray)
	{
		// serialize number of bits
		Ar << BitArray.NumBits;

		if (Ar.IsLoading())
		{
			// no need for slop when reading
			BitArray.MaxBits = BitArray.NumBits;

			// allocate room for new bits
			BitArray.Realloc(0);
		}

		// calc the number of dwords for all the bits
		const int32 NumDWORDs = FMath::DivideAndRoundUp(BitArray.NumBits, NumBitsPerDWORD);

		// serialize the data as one big chunk
		Ar.Serialize(BitArray.GetData(), NumDWORDs * sizeof(uint32));

		return Ar;
	}

	/**
	 * Adds a bit to the array with the given value.
	 * @return The index of the added bit.
	 */
	int32 Add(const bool Value)
	{
		const int32 Index = NumBits;
		const bool bReallocate = (NumBits + 1) > MaxBits;

		NumBits++;

		if(bReallocate)
		{
			// Allocate memory for the new bits.
			const uint32 MaxDWORDs = AllocatorInstance.CalculateSlackGrow(
				FMath::DivideAndRoundUp(NumBits, NumBitsPerDWORD),
				FMath::DivideAndRoundUp(MaxBits, NumBitsPerDWORD),
				sizeof(uint32)
				);
			MaxBits = MaxDWORDs * NumBitsPerDWORD;
			Realloc(NumBits - 1);
		}

		(*this)[Index] = Value;

		return Index;
	}

	/**
	 * Removes all bits from the array, potentially leaving space allocated for an expected number of bits about to be added.
	 * @param ExpectedNumBits - The expected number of bits about to be added.
	 */
	void Empty(int32 ExpectedNumBits = 0)
	{
		NumBits = 0;

		ExpectedNumBits = FMath::DivideAndRoundUp(ExpectedNumBits, NumBitsPerDWORD) * NumBitsPerDWORD;
		// If the expected number of bits doesn't match the allocated number of bits, reallocate.
		if(MaxBits != ExpectedNumBits)
		{
			MaxBits = ExpectedNumBits;
			Realloc(0);
		}
	}

	/**
	 * Removes all bits from the array retaining any space already allocated.
	 */
	void Reset()
	{
		// We need this because iterators often use whole DWORDs when masking, which includes off-the-end elements
		FMemory::Memset(GetData(), 0, FMath::DivideAndRoundUp(NumBits, NumBitsPerDWORD) * sizeof(uint32));

		NumBits = 0;
	}

	/**
	 * Resets the array's contents.
	 * @param Value - The value to initial the bits to.
	 * @param NumBits - The number of bits in the array.
	 */
	void Init(bool Value,int32 InNumBits)
	{
		Empty(InNumBits);
		if(InNumBits)
		{
			NumBits = InNumBits;
			FMemory::Memset(GetData(),Value ? 0xff : 0, FMath::DivideAndRoundUp(NumBits, NumBitsPerDWORD) * sizeof(uint32));
		}
	}

	/**
	 * Sets or unsets a range of bits within the array.
	 * @param  Index  The index of the first bit to set.
	 * @param  Num    The number of bits to set.
	 * @param  Value  The value to set the bits to.
	 */
	FORCENOINLINE void SetRange(int32 Index, int32 Num, bool Value)
	{
		check(Index >= 0 && Num >= 0 && Index + Num <= NumBits);

		if (Num == 0)
		{
			return;
		}

		// Work out which uint32 index to set from, and how many
		uint32 StartIndex = Index / 32;
		uint32 Count      = (Index + Num + 31) / 32 - StartIndex;

		// Work out masks for the start/end of the sequence
		uint32 StartMask  = 0xFFFFFFFFu << (Index % 32);
		uint32 EndMask    = 0xFFFFFFFFu >> (32 - (Index + Num) % 32) % 32;

		uint32* Data = GetData() + StartIndex;
		if (Value)
		{
			if (Count == 1)
			{
				*Data |= StartMask & EndMask;
			}
			else
			{
				*Data++ |= StartMask;
				Count -= 2;
				while (Count != 0)
				{
					*Data++ = ~0;
					--Count;
				}
				*Data |= EndMask;
			}
		}
		else
		{
			if (Count == 1)
			{
				*Data &= ~(StartMask & EndMask);
			}
			else
			{
				*Data++ &= ~StartMask;
				Count -= 2;
				while (Count != 0)
				{
					*Data++ = 0;
					--Count;
				}
				*Data &= ~EndMask;
			}
		}
	}

	/**
	 * Removes bits from the array.
	 * @param BaseIndex - The index of the first bit to remove.
	 * @param NumBitsToRemove - The number of consecutive bits to remove.
	 */
	void RemoveAt(int32 BaseIndex,int32 NumBitsToRemove = 1)
	{
		check(BaseIndex >= 0 && NumBitsToRemove >= 0 && BaseIndex + NumBitsToRemove <= NumBits);

		if (BaseIndex + NumBitsToRemove != NumBits)
		{
			// Until otherwise necessary, this is an obviously correct implementation rather than an efficient implementation.
			FIterator WriteIt(*this);
			for(FConstIterator ReadIt(*this);ReadIt;++ReadIt)
			{
				// If this bit isn't being removed, write it back to the array at its potentially new index.
				if(ReadIt.GetIndex() < BaseIndex || ReadIt.GetIndex() >= BaseIndex + NumBitsToRemove)
				{
					if(WriteIt.GetIndex() != ReadIt.GetIndex())
					{
						WriteIt.GetValue() = (bool)ReadIt.GetValue();
					}
					++WriteIt;
				}
			}
		}
		NumBits -= NumBitsToRemove;
	}

	/* Removes bits from the array by swapping them with bits at the end of the array.
	 * This is mainly implemented so that other code using TArray::RemoveSwap will have
	 * matching indices.
 	 * @param BaseIndex - The index of the first bit to remove.
	 * @param NumBitsToRemove - The number of consecutive bits to remove.
	 */
	void RemoveAtSwap( int32 BaseIndex, int32 NumBitsToRemove=1 )
	{
		check(BaseIndex >= 0 && NumBitsToRemove >= 0 && BaseIndex + NumBitsToRemove <= NumBits);
		if( BaseIndex < NumBits - NumBitsToRemove )
		{
			// Copy bits from the end to the region we are removing
			for( int32 Index=0;Index<NumBitsToRemove;Index++ )
			{
#if PLATFORM_MAC || PLATFORM_LINUX
				// Clang compiler doesn't understand the short syntax, so let's be explicit
				int32 FromIndex = NumBits - NumBitsToRemove + Index;
				FConstBitReference From(GetData()[FromIndex / NumBitsPerDWORD],1 << (FromIndex & (NumBitsPerDWORD - 1)));

				int32 ToIndex = BaseIndex + Index;
				FBitReference To(GetData()[ToIndex / NumBitsPerDWORD],1 << (ToIndex & (NumBitsPerDWORD - 1)));

				To = (bool)From;
#else
				(*this)[BaseIndex + Index] = (bool)(*this)[NumBits - NumBitsToRemove + Index];
#endif
			}
		}

		// Remove the bits from the end of the array.
		NumBits -= NumBitsToRemove;
	}
	

	/** 
	 * Helper function to return the amount of memory allocated by this container 
	 * @return number of bytes allocated by this container
	 */
	uint32 GetAllocatedSize( void ) const
	{
		return FMath::DivideAndRoundUp(MaxBits, NumBitsPerDWORD) * sizeof(uint32);
	}

	/** Tracks the container's memory use through an archive. */
	void CountBytes(FArchive& Ar)
	{
		Ar.CountBytes(
			FMath::DivideAndRoundUp(NumBits, NumBitsPerDWORD) * sizeof(uint32),
			FMath::DivideAndRoundUp(MaxBits, NumBitsPerDWORD) * sizeof(uint32)
		);
	}

	/**
	 * Finds the first true/false bit in the array, and returns the bit index.
	 * If there is none, INDEX_NONE is returned.
	 */
	int32 Find(bool bValue) const
	{
		// Iterate over the array until we see a word with a matching bit
		const uint32 Test = bValue ? 0u : (uint32)-1;

		const uint32* RESTRICT DwordArray = GetData();
		const int32 LocalNumBits = NumBits;
		const int32 DwordCount = FMath::DivideAndRoundUp(LocalNumBits, NumBitsPerDWORD);
		int32 DwordIndex = 0;
		while (DwordIndex < DwordCount && DwordArray[DwordIndex] == Test)
		{
			++DwordIndex;
		}

		if (DwordIndex < DwordCount)
		{
			// If we're looking for a false, then we flip the bits - then we only need to find the first one bit
			const uint32 Bits = bValue ? (DwordArray[DwordIndex]) : ~(DwordArray[DwordIndex]);
			ASSUME(Bits != 0);
			const int32 LowestBitIndex = FMath::CountTrailingZeros(Bits) + (DwordIndex << NumBitsPerDWORDLogTwo);
			if (LowestBitIndex < LocalNumBits)
			{
				return LowestBitIndex;
			}
		}

		return INDEX_NONE;
	}

	FORCEINLINE bool Contains(bool bValue) const
	{
		return Find(bValue) != INDEX_NONE;
	}

	/**
	 * Finds the first zero bit in the array, sets it to true, and returns the bit index.
	 * If there is none, INDEX_NONE is returned.
	 */
	int32 FindAndSetFirstZeroBit()
	{
		// Iterate over the array until we see a word with a zero bit.
		uint32* RESTRICT DwordArray = GetData();
		const int32 LocalNumBits = NumBits;
		const int32 DwordCount = FMath::DivideAndRoundUp(LocalNumBits, NumBitsPerDWORD);
		int32 DwordIndex = 0;
		while (DwordIndex < DwordCount && DwordArray[DwordIndex] == (uint32)-1)
		{
			++DwordIndex;
		}

		if (DwordIndex < DwordCount)
		{
			// Flip the bits, then we only need to find the first one bit -- easy.
			const uint32 Bits = ~(DwordArray[DwordIndex]);
			ASSUME(Bits != 0);
			const uint32 LowestBit = (Bits) & (-(int32)Bits);
			const int32 LowestBitIndex = FMath::CountTrailingZeros(Bits) + (DwordIndex << NumBitsPerDWORDLogTwo);
			if (LowestBitIndex < LocalNumBits)
			{
				DwordArray[DwordIndex] |= LowestBit;
				return LowestBitIndex;
			}
		}

		return INDEX_NONE;
	}

	// Accessors.
	FORCEINLINE bool IsValidIndex(int32 InIndex) const
	{
		return InIndex >= 0 && InIndex < NumBits;
	}

	FORCEINLINE int32 Num() const { return NumBits; }
	FORCEINLINE FBitReference operator[](int32 Index)
	{
		check(Index>=0 && Index<NumBits);
		return FBitReference(
			GetData()[Index / NumBitsPerDWORD],
			1 << (Index & (NumBitsPerDWORD - 1))
			);
	}
	FORCEINLINE const FConstBitReference operator[](int32 Index) const
	{
		check(Index>=0 && Index<NumBits);
		return FConstBitReference(
			GetData()[Index / NumBitsPerDWORD],
			1 << (Index & (NumBitsPerDWORD - 1))
			);
	}
	FORCEINLINE FBitReference AccessCorrespondingBit(const FRelativeBitReference& RelativeReference)
	{
		checkSlow(RelativeReference.Mask);
		checkSlow(RelativeReference.DWORDIndex >= 0);
		checkSlow(((uint32)RelativeReference.DWORDIndex + 1) * NumBitsPerDWORD - 1 - FMath::CountLeadingZeros(RelativeReference.Mask) < (uint32)NumBits);
		return FBitReference(
			GetData()[RelativeReference.DWORDIndex],
			RelativeReference.Mask
			);
	}
	FORCEINLINE const FConstBitReference AccessCorrespondingBit(const FRelativeBitReference& RelativeReference) const
	{
		checkSlow(RelativeReference.Mask);
		checkSlow(RelativeReference.DWORDIndex >= 0);
		checkSlow(((uint32)RelativeReference.DWORDIndex + 1) * NumBitsPerDWORD - 1 - FMath::CountLeadingZeros(RelativeReference.Mask) < (uint32)NumBits);
		return FConstBitReference(
			GetData()[RelativeReference.DWORDIndex],
			RelativeReference.Mask
			);
	}

	/** BitArray iterator. */
	class FIterator : public FRelativeBitReference
	{
	public:
		FORCEINLINE FIterator(TBitArray<Allocator>& InArray,int32 StartIndex = 0)
		:	FRelativeBitReference(StartIndex)
		,	Array(InArray)
		,	Index(StartIndex)
		{
		}
		FORCEINLINE FIterator& operator++()
		{
			++Index;
			this->Mask <<= 1;
			if(!this->Mask)
			{
				// Advance to the next uint32.
				this->Mask = 1;
				++this->DWORDIndex;
			}
			return *this;
		}
		/** conversion to "bool" returning true if the iterator is valid. */
		FORCEINLINE explicit operator bool() const
		{ 
			return Index < Array.Num(); 
		}
		/** inverse of the "bool" operator */
		FORCEINLINE bool operator !() const 
		{
			return !(bool)*this;
		}

		FORCEINLINE FBitReference GetValue() const { return FBitReference(Array.GetData()[this->DWORDIndex],this->Mask); }
		FORCEINLINE int32 GetIndex() const { return Index; }
	private:
		TBitArray<Allocator>& Array;
		int32 Index;
	};

	/** Const BitArray iterator. */
	class FConstIterator : public FRelativeBitReference
	{
	public:
		FORCEINLINE FConstIterator(const TBitArray<Allocator>& InArray,int32 StartIndex = 0)
		:	FRelativeBitReference(StartIndex)
		,	Array(InArray)
		,	Index(StartIndex)
		{
		}
		FORCEINLINE FConstIterator& operator++()
		{
			++Index;
			this->Mask <<= 1;
			if(!this->Mask)
			{
				// Advance to the next uint32.
				this->Mask = 1;
				++this->DWORDIndex;
			}
			return *this;
		}

		/** conversion to "bool" returning true if the iterator is valid. */
		FORCEINLINE explicit operator bool() const
		{ 
			return Index < Array.Num(); 
		}
		/** inverse of the "bool" operator */
		FORCEINLINE bool operator !() const 
		{
			return !(bool)*this;
		}

		FORCEINLINE FConstBitReference GetValue() const { return FConstBitReference(Array.GetData()[this->DWORDIndex],this->Mask); }
		FORCEINLINE int32 GetIndex() const { return Index; }
	private:
		const TBitArray<Allocator>& Array;
		int32 Index;
	};

	/** Const reverse iterator. */
	class FConstReverseIterator : public FRelativeBitReference
	{
	public:
		FORCEINLINE FConstReverseIterator(const TBitArray<Allocator>& InArray)
			:	FRelativeBitReference(InArray.Num() - 1)
			,	Array(InArray)
			,	Index(InArray.Num() - 1)
		{
		}
		FORCEINLINE FConstReverseIterator& operator++()
		{
			--Index;
			this->Mask >>= 1;
			if(!this->Mask)
			{
				// Advance to the next uint32.
				this->Mask = (1 << (NumBitsPerDWORD-1));
				--this->DWORDIndex;
			}
			return *this;
		}

		/** conversion to "bool" returning true if the iterator is valid. */
		FORCEINLINE explicit operator bool() const
		{ 
			return Index >= 0; 
		}
		/** inverse of the "bool" operator */
		FORCEINLINE bool operator !() const 
		{
			return !(bool)*this;
		}

		FORCEINLINE FConstBitReference GetValue() const { return FConstBitReference(Array.GetData()[this->DWORDIndex],this->Mask); }
		FORCEINLINE int32 GetIndex() const { return Index; }
	private:
		const TBitArray<Allocator>& Array;
		int32 Index;
	};

	FORCEINLINE const uint32* GetData() const
	{
		return (uint32*)AllocatorInstance.GetAllocation();
	}

	FORCEINLINE uint32* GetData()
	{
		return (uint32*)AllocatorInstance.GetAllocation();
	}

private:
	typedef typename Allocator::template ForElementType<uint32> AllocatorType;

	AllocatorType AllocatorInstance;
	int32         NumBits;
	int32         MaxBits;

	FORCENOINLINE void Realloc(int32 PreviousNumBits)
	{
		const int32 PreviousNumDWORDs = FMath::DivideAndRoundUp(PreviousNumBits, NumBitsPerDWORD);
		const int32 MaxDWORDs = FMath::DivideAndRoundUp(MaxBits, NumBitsPerDWORD);

		AllocatorInstance.ResizeAllocation(PreviousNumDWORDs,MaxDWORDs,sizeof(uint32));

		if(MaxDWORDs)
		{
			// Reset the newly allocated slack DWORDs.
			FMemory::Memzero((uint32*)AllocatorInstance.GetAllocation() + PreviousNumDWORDs,(MaxDWORDs - PreviousNumDWORDs) * sizeof(uint32));
		}
	}
};


template<typename Allocator>
struct TContainerTraits<TBitArray<Allocator> > : public TContainerTraitsBase<TBitArray<Allocator> >
{
	enum { MoveWillEmptyContainer = TAllocatorTraits<Allocator>::SupportsMove };
};


/** An iterator which only iterates over set bits. */
template<typename Allocator>
class TConstSetBitIterator : public FRelativeBitReference
{
public:

	/** Constructor. */
	TConstSetBitIterator(const TBitArray<Allocator>& InArray,int32 StartIndex = 0)
		: FRelativeBitReference(StartIndex)
		, Array                (InArray)
		, UnvisitedBitMask     ((~0U) << (StartIndex & (NumBitsPerDWORD - 1)))
		, CurrentBitIndex      (StartIndex)
		, BaseBitIndex         (StartIndex & ~(NumBitsPerDWORD - 1))
	{
		check(StartIndex >= 0 && StartIndex <= Array.Num());
		if (StartIndex != Array.Num())
		{
			FindFirstSetBit();
		}
	}

	/** Forwards iteration operator. */
	FORCEINLINE TConstSetBitIterator& operator++()
	{
		// Mark the current bit as visited.
		UnvisitedBitMask &= ~this->Mask;

		// Find the first set bit that hasn't been visited yet.
		FindFirstSetBit();

		return *this;
	}

	FORCEINLINE friend bool operator==(const TConstSetBitIterator& Lhs, const TConstSetBitIterator& Rhs) 
	{
		// We only need to compare the bit index and the array... all the rest of the state is unobservable.
		return Lhs.CurrentBitIndex == Rhs.CurrentBitIndex && &Lhs.Array == &Rhs.Array;
	}

	FORCEINLINE friend bool operator!=(const TConstSetBitIterator& Lhs, const TConstSetBitIterator& Rhs)
	{ 
		return !(Lhs == Rhs);
	}

	/** conversion to "bool" returning true if the iterator is valid. */
	FORCEINLINE explicit operator bool() const
	{ 
		return CurrentBitIndex < Array.Num(); 
	}
	/** inverse of the "bool" operator */
	FORCEINLINE bool operator !() const 
	{
		return !(bool)*this;
	}

	/** Index accessor. */
	FORCEINLINE int32 GetIndex() const
	{
		return CurrentBitIndex;
	}

private:

	const TBitArray<Allocator>& Array;

	uint32 UnvisitedBitMask;
	int32 CurrentBitIndex;
	int32 BaseBitIndex;

	/** Find the first set bit starting with the current bit, inclusive. */
	void FindFirstSetBit()
	{
		const uint32* ArrayData      = Array.GetData();
		const int32   ArrayNum       = Array.Num();
		const int32   LastDWORDIndex = (ArrayNum - 1) / NumBitsPerDWORD;

		// Advance to the next non-zero uint32.
		uint32 RemainingBitMask = ArrayData[this->DWORDIndex] & UnvisitedBitMask;
		while (!RemainingBitMask)
		{
			++this->DWORDIndex;
			BaseBitIndex += NumBitsPerDWORD;
			if (this->DWORDIndex > LastDWORDIndex)
			{
				// We've advanced past the end of the array.
				CurrentBitIndex = ArrayNum;
				return;
			}

			RemainingBitMask = ArrayData[this->DWORDIndex];
			UnvisitedBitMask = ~0;
		}

		// This operation has the effect of unsetting the lowest set bit of BitMask
		const uint32 NewRemainingBitMask = RemainingBitMask & (RemainingBitMask - 1);

		// This operation XORs the above mask with the original mask, which has the effect
		// of returning only the bits which differ; specifically, the lowest bit
		this->Mask = NewRemainingBitMask ^ RemainingBitMask;

		// If the Nth bit was the lowest set bit of BitMask, then this gives us N
		CurrentBitIndex = BaseBitIndex + NumBitsPerDWORD - 1 - FMath::CountLeadingZeros(this->Mask);

		// If we've accidentally iterated off the end of an array but still within the same DWORD
		// then set the index to the last index of the array
		if (CurrentBitIndex > ArrayNum)
		{
			CurrentBitIndex = ArrayNum;
		}
	}
};


/** An iterator which only iterates over the bits which are set in both of two bit-arrays. */
template<typename Allocator,typename OtherAllocator>
class TConstDualSetBitIterator : public FRelativeBitReference
{
public:

	/** Constructor. */
	FORCEINLINE TConstDualSetBitIterator(
		const TBitArray<Allocator>& InArrayA,
		const TBitArray<OtherAllocator>& InArrayB,
		int32 StartIndex = 0
		)
	:	FRelativeBitReference(StartIndex)
	,	ArrayA(InArrayA)
	,	ArrayB(InArrayB)
	,	UnvisitedBitMask((~0U) << (StartIndex & (NumBitsPerDWORD - 1)))
	,	CurrentBitIndex(StartIndex)
	,	BaseBitIndex(StartIndex & ~(NumBitsPerDWORD - 1))
	{
		check(ArrayA.Num() == ArrayB.Num());

		FindFirstSetBit();
	}

	/** Advancement operator. */
	FORCEINLINE TConstDualSetBitIterator& operator++()
	{
		checkSlow(ArrayA.Num() == ArrayB.Num());

		// Mark the current bit as visited.
		UnvisitedBitMask &= ~this->Mask;

		// Find the first set bit that hasn't been visited yet.
		FindFirstSetBit();

		return *this;

	}

	/** conversion to "bool" returning true if the iterator is valid. */
	FORCEINLINE explicit operator bool() const
	{ 
		return CurrentBitIndex < ArrayA.Num(); 
	}
	/** inverse of the "bool" operator */
	FORCEINLINE bool operator !() const 
	{
		return !(bool)*this;
	}

	/** Index accessor. */
	FORCEINLINE int32 GetIndex() const
	{
		return CurrentBitIndex;
	}

private:

	const TBitArray<Allocator>& ArrayA;
	const TBitArray<OtherAllocator>& ArrayB;

	uint32 UnvisitedBitMask;
	int32 CurrentBitIndex;
	int32 BaseBitIndex;

	/** Find the first bit that is set in both arrays, starting with the current bit, inclusive. */
	void FindFirstSetBit()
	{
		static const uint32 EmptyArrayData = 0;
		const uint32* ArrayDataA = IfAThenAElseB(ArrayA.GetData(),&EmptyArrayData);
		const uint32* ArrayDataB = IfAThenAElseB(ArrayB.GetData(),&EmptyArrayData);

		// Advance to the next non-zero uint32.
		uint32 RemainingBitMask = ArrayDataA[this->DWORDIndex] & ArrayDataB[this->DWORDIndex] & UnvisitedBitMask;
		while(!RemainingBitMask)
		{
			this->DWORDIndex++;
			BaseBitIndex += NumBitsPerDWORD;
			const int32 LastDWORDIndex = (ArrayA.Num() - 1) / NumBitsPerDWORD;
			if(this->DWORDIndex <= LastDWORDIndex)
			{
				RemainingBitMask = ArrayDataA[this->DWORDIndex] & ArrayDataB[this->DWORDIndex];
				UnvisitedBitMask = ~0;
			}
			else
			{
				// We've advanced past the end of the array.
				CurrentBitIndex = ArrayA.Num();
				return;
			}
		};

		// We can assume that RemainingBitMask!=0 here.
		checkSlow(RemainingBitMask);

		// This operation has the effect of unsetting the lowest set bit of BitMask
		const uint32 NewRemainingBitMask = RemainingBitMask & (RemainingBitMask - 1);

		// This operation XORs the above mask with the original mask, which has the effect
		// of returning only the bits which differ; specifically, the lowest bit
		this->Mask = NewRemainingBitMask ^ RemainingBitMask;

		// If the Nth bit was the lowest set bit of BitMask, then this gives us N
		CurrentBitIndex = BaseBitIndex + NumBitsPerDWORD - 1 - FMath::CountLeadingZeros(this->Mask);
	}
};


// Untyped bit array type for accessing TBitArray data, like FScriptArray for TArray.
// Must have the same memory representation as a TBitArray.
class FScriptBitArray
{
public:
	/**
	 * Minimal initialization constructor.
	 * @param Value - The value to initial the bits to.
	 * @param InNumBits - The initial number of bits in the array.
	 */
	FScriptBitArray()
		: NumBits(0)
		, MaxBits(0)
	{
	}

	bool IsValidIndex(int32 Index) const
	{
		return Index >= 0 && Index < NumBits;
	}

	FBitReference operator[](int32 Index)
	{
		check(IsValidIndex(Index));
		return FBitReference(GetData()[Index / NumBitsPerDWORD], 1 << (Index & (NumBitsPerDWORD - 1)));
	}

	FConstBitReference operator[](int32 Index) const
	{
		check(IsValidIndex(Index));
		return FConstBitReference(GetData()[Index / NumBitsPerDWORD], 1 << (Index & (NumBitsPerDWORD - 1)));
	}

	void Empty(int32 Slack = 0)
	{
		NumBits = 0;

		Slack = FMath::DivideAndRoundUp(Slack, NumBitsPerDWORD) * NumBitsPerDWORD;
		// If the expected number of bits doesn't match the allocated number of bits, reallocate.
		if (MaxBits != Slack)
		{
			MaxBits = Slack;
			Realloc(0);
		}
	}

	int32 Add(const bool Value)
	{
		const int32 Index = NumBits;
		NumBits++;
		if (NumBits > MaxBits)
		{
			ReallocGrow(NumBits - 1);
		}
		(*this)[Index] = Value;
		return Index;
	}

private:
	typedef FDefaultBitArrayAllocator::ForElementType<uint32> AllocatorType;

	AllocatorType AllocatorInstance;
	int32         NumBits;
	int32         MaxBits;

	// This function isn't intended to be called, just to be compiled to validate the correctness of the type.
	static void CheckConstraints()
	{
		typedef FScriptBitArray ScriptType;
		typedef TBitArray<>     RealType;

		// Check that the class footprint is the same
		static_assert(sizeof (ScriptType) == sizeof (RealType), "FScriptBitArray's size doesn't match TBitArray");
		static_assert(alignof(ScriptType) == alignof(RealType), "FScriptBitArray's alignment doesn't match TBitArray");

		// Check member sizes
		static_assert(sizeof(DeclVal<ScriptType>().AllocatorInstance) == sizeof(DeclVal<RealType>().AllocatorInstance), "FScriptBitArray's AllocatorInstance member size does not match TBitArray's");
		static_assert(sizeof(DeclVal<ScriptType>().NumBits)           == sizeof(DeclVal<RealType>().NumBits),           "FScriptBitArray's NumBits member size does not match TBitArray's");
		static_assert(sizeof(DeclVal<ScriptType>().MaxBits)           == sizeof(DeclVal<RealType>().MaxBits),           "FScriptBitArray's MaxBits member size does not match TBitArray's");

		// Check member offsets
		static_assert(STRUCT_OFFSET(ScriptType, AllocatorInstance) == STRUCT_OFFSET(RealType, AllocatorInstance), "FScriptBitArray's AllocatorInstance member offset does not match TBitArray's");
		static_assert(STRUCT_OFFSET(ScriptType, NumBits)           == STRUCT_OFFSET(RealType, NumBits),           "FScriptBitArray's NumBits member offset does not match TBitArray's");
		static_assert(STRUCT_OFFSET(ScriptType, MaxBits)           == STRUCT_OFFSET(RealType, MaxBits),           "FScriptBitArray's MaxBits member offset does not match TBitArray's");
	}

	FORCEINLINE uint32* GetData()
	{
		return (uint32*)AllocatorInstance.GetAllocation();
	}

	FORCEINLINE const uint32* GetData() const
	{
		return (const uint32*)AllocatorInstance.GetAllocation();
	}

	FORCENOINLINE void Realloc(int32 PreviousNumBits)
	{
		const uint32 MaxDWORDs = AllocatorInstance.CalculateSlackReserve(
			FMath::DivideAndRoundUp(MaxBits, NumBitsPerDWORD),
			sizeof(uint32)
			);
		MaxBits = MaxDWORDs * NumBitsPerDWORD;
		const uint32 PreviousNumDWORDs = FMath::DivideAndRoundUp(PreviousNumBits, NumBitsPerDWORD);

		AllocatorInstance.ResizeAllocation(PreviousNumDWORDs, MaxDWORDs, sizeof(uint32));

		if (MaxDWORDs && MaxDWORDs > PreviousNumDWORDs)
		{
			// Reset the newly allocated slack DWORDs.
			FMemory::Memzero((uint32*)AllocatorInstance.GetAllocation() + PreviousNumDWORDs, (MaxDWORDs - PreviousNumDWORDs) * sizeof(uint32));
		}
	}
	FORCENOINLINE void ReallocGrow(int32 PreviousNumBits)
	{
		// Allocate memory for the new bits.
		const uint32 MaxDWORDs = AllocatorInstance.CalculateSlackGrow(
			FMath::DivideAndRoundUp(NumBits, NumBitsPerDWORD),
			FMath::DivideAndRoundUp(MaxBits, NumBitsPerDWORD),
			sizeof(uint32)
			);
		MaxBits = MaxDWORDs * NumBitsPerDWORD;
		const uint32 PreviousNumDWORDs = FMath::DivideAndRoundUp(PreviousNumBits, NumBitsPerDWORD);
		AllocatorInstance.ResizeAllocation(PreviousNumDWORDs, MaxDWORDs, sizeof(uint32));
		if (MaxDWORDs && MaxDWORDs > PreviousNumDWORDs)
		{
			// Reset the newly allocated slack DWORDs.
			FMemory::Memzero((uint32*)AllocatorInstance.GetAllocation() + PreviousNumDWORDs, (MaxDWORDs - PreviousNumDWORDs) * sizeof(uint32));
		}
	}

public:
	// These should really be private, because they shouldn't be called, but there's a bunch of code
	// that needs to be fixed first.
	FScriptBitArray(const FScriptBitArray&) { check(false); }
	void operator=(const FScriptBitArray&) { check(false); }
};

template <>
struct TIsZeroConstructType<FScriptBitArray>
{
	enum { Value = true };
};
