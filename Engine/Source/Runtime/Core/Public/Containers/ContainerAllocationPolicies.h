// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "HAL/UnrealMemory.h"
#include "Templates/TypeCompatibleBytes.h"
#include "HAL/PlatformMath.h"
#include "Templates/MemoryOps.h"
#include "Math/NumericLimits.h"

class FDefaultBitArrayAllocator;

/** branchless pointer selection
* return A ? A : B;
**/
template<typename ReferencedType>
ReferencedType* IfAThenAElseB(ReferencedType* A,ReferencedType* B);

/** branchless pointer selection based on predicate
* return PTRINT(Predicate) ? A : B;
**/
template<typename PredicateType,typename ReferencedType>
ReferencedType* IfPThenAElseB(PredicateType Predicate,ReferencedType* A,ReferencedType* B);

FORCEINLINE int32 DefaultCalculateSlackShrink(int32 NumElements, int32 NumAllocatedElements, SIZE_T BytesPerElement, bool bAllowQuantize, uint32 Alignment = DEFAULT_ALIGNMENT)
{
	int32 Retval;
	checkSlow(NumElements < NumAllocatedElements);

	// If the container has too much slack, shrink it to exactly fit the number of elements.
	const uint32 CurrentSlackElements = NumAllocatedElements - NumElements;
	const SIZE_T CurrentSlackBytes = (NumAllocatedElements - NumElements)*BytesPerElement;
	const bool bTooManySlackBytes = CurrentSlackBytes >= 16384;
	const bool bTooManySlackElements = 3 * NumElements < 2 * NumAllocatedElements;
	if ((bTooManySlackBytes || bTooManySlackElements) && (CurrentSlackElements > 64 || !NumElements)) //  hard coded 64 :-(
	{
		Retval = NumElements;
		if (Retval > 0)
		{
			if (bAllowQuantize)
			{
				Retval = FMemory::QuantizeSize(Retval * BytesPerElement, Alignment) / BytesPerElement;
			}
		}
	}
	else
	{
		Retval = NumAllocatedElements;
	}

	return Retval;
}

FORCEINLINE int32 DefaultCalculateSlackGrow(int32 NumElements, int32 NumAllocatedElements, SIZE_T BytesPerElement, bool bAllowQuantize, uint32 Alignment = DEFAULT_ALIGNMENT)
{
	int32 Retval;
	checkSlow(NumElements > NumAllocatedElements && NumElements > 0);

	SIZE_T Grow = 4; // this is the amount for the first alloc
	if (NumAllocatedElements || SIZE_T(NumElements) > Grow)
	{
		// Allocate slack for the array proportional to its size.
		Grow = SIZE_T(NumElements) + 3 * SIZE_T(NumElements) / 8 + 16;
	}
	if (bAllowQuantize)
	{
		Retval = FMemory::QuantizeSize(Grow * BytesPerElement, Alignment) / BytesPerElement;
	}
	else
	{
		Retval = Grow;
	}
	// NumElements and MaxElements are stored in 32 bit signed integers so we must be careful not to overflow here.
	if (NumElements > Retval)
	{
		Retval = MAX_int32;
	}

	return Retval;
}

FORCEINLINE int32 DefaultCalculateSlackReserve(int32 NumElements, SIZE_T BytesPerElement, bool bAllowQuantize, uint32 Alignment = DEFAULT_ALIGNMENT)
{
	int32 Retval = NumElements;
	checkSlow(NumElements > 0);
	if (bAllowQuantize)
	{
		Retval = FMemory::QuantizeSize(SIZE_T(Retval) * SIZE_T(BytesPerElement), Alignment) / BytesPerElement;
		// NumElements and MaxElements are stored in 32 bit signed integers so we must be careful not to overflow here.
		if (NumElements > Retval)
		{
			Retval = MAX_int32;
		}
	}

	return Retval;
}

/** A type which is used to represent a script type that is unknown at compile time. */
struct FScriptContainerElement
{
};

template <typename AllocatorType>
struct TAllocatorTraitsBase
{
	enum { SupportsMove    = false };
	enum { IsZeroConstruct = false };
};

template <typename AllocatorType>
struct TAllocatorTraits : TAllocatorTraitsBase<AllocatorType>
{
};

/** This is the allocation policy interface; it exists purely to document the policy's interface, and should not be used. */
class FContainerAllocatorInterface
{
public:

	/** Determines whether the user of the allocator may use the ForAnyElementType inner class. */
	enum { NeedsElementType = true };
	enum { RequireRangeCheck = true };

	/**
	 * A class that receives both the explicit allocation policy template parameters specified by the user of the container,
	 * but also the implicit ElementType template parameter from the container type.
	 */
	template<typename ElementType>
	class ForElementType
	{
		/**
		 * Moves the state of another allocator into this one.
		 * Assumes that the allocator is currently empty, i.e. memory may be allocated but any existing elements have already been destructed (if necessary).
		 * @param Other - The allocator to move the state from.  This allocator should be left in a valid empty state.
		 */
		void MoveToEmpty(ForElementType& Other);

		/** Accesses the container's current data. */
		ElementType* GetAllocation() const;

		/**
		 * Resizes the container's allocation.
		 * @param PreviousNumElements - The number of elements that were stored in the previous allocation.
		 * @param NumElements - The number of elements to allocate space for.
		 * @param NumBytesPerElement - The number of bytes/element.
		 */
		void ResizeAllocation(
			int32 PreviousNumElements,
			int32 NumElements,
			SIZE_T NumBytesPerElement
			);

		/**
		 * Calculates the amount of slack to allocate for an array that has just grown or shrunk to a given number of elements.
		 * @param NumElements - The number of elements to allocate space for.
		 * @param CurrentNumSlackElements - The current number of elements allocated.
		 * @param NumBytesPerElement - The number of bytes/element.
		 */
		int32 CalculateSlack(
			int32 NumElements,
			int32 CurrentNumSlackElements,
			SIZE_T NumBytesPerElement
			) const;

		/**
		* Calculates the amount of slack to allocate for an array that has just shrunk to a given number of elements.
		* @param NumElements - The number of elements to allocate space for.
		* @param CurrentNumSlackElements - The current number of elements allocated.
		* @param NumBytesPerElement - The number of bytes/element.
		*/
		int32 CalculateSlackShrink(
			int32 NumElements,
			int32 CurrentNumSlackElements,
			SIZE_T NumBytesPerElement
			) const;

		/**
		* Calculates the amount of slack to allocate for an array that has just grown to a given number of elements.
		* @param NumElements - The number of elements to allocate space for.
		* @param CurrentNumSlackElements - The current number of elements allocated.
		* @param NumBytesPerElement - The number of bytes/element.
		*/
		int32 CalculateSlackGrow(
			int32 NumElements,
			int32 CurrentNumSlackElements,
			SIZE_T NumBytesPerElement
			) const;

		SIZE_T GetAllocatedSize(int32 NumAllocatedElements, SIZE_T NumBytesPerElement) const;
	};

	/**
	 * A class that may be used when NeedsElementType=false is specified.
	 * If NeedsElementType=true, then this must be present but will not be used, and so can simply be a typedef to void
	 */
	typedef ForElementType<FScriptContainerElement> ForAnyElementType;
};

/** The indirect allocation policy always allocates the elements indirectly. */
template<uint32 Alignment = DEFAULT_ALIGNMENT>
class TAlignedHeapAllocator
{
public:

	enum { NeedsElementType = false };
	enum { RequireRangeCheck = true };

	class ForAnyElementType
	{
	public:

		/** Default constructor. */
		ForAnyElementType()
			: Data(nullptr)
		{}

		/**
		 * Moves the state of another allocator into this one.
		 * Assumes that the allocator is currently empty, i.e. memory may be allocated but any existing elements have already been destructed (if necessary).
		 * @param Other - The allocator to move the state from.  This allocator should be left in a valid empty state.
		 */
		FORCEINLINE void MoveToEmpty(ForAnyElementType& Other)
		{
			checkSlow(this != &Other);

			if (Data)
			{
				FMemory::Free(Data);
			}

			Data       = Other.Data;
			Other.Data = nullptr;
		}

		/** Destructor. */
		FORCEINLINE ~ForAnyElementType()
		{
			if(Data)
			{
				FMemory::Free(Data);
			}
		}

		// FContainerAllocatorInterface
		FORCEINLINE FScriptContainerElement* GetAllocation() const
		{
			return Data;
		}
		void ResizeAllocation(
			int32 PreviousNumElements,
			int32 NumElements,
			SIZE_T NumBytesPerElement
			)
		{
			// Avoid calling FMemory::Realloc( nullptr, 0 ) as ANSI C mandates returning a valid pointer which is not what we want.
			if (Data || NumElements)
			{
				//checkSlow(((uint64)NumElements*(uint64)ElementTypeInfo.GetSize() < (uint64)INT_MAX));
				Data = (FScriptContainerElement*)FMemory::Realloc( Data, NumElements*NumBytesPerElement, Alignment );
			}
		}
		FORCEINLINE int32 CalculateSlackReserve(int32 NumElements, int32 NumBytesPerElement) const
		{
			return DefaultCalculateSlackReserve(NumElements, NumBytesPerElement, true, Alignment);
		}
		FORCEINLINE int32 CalculateSlackShrink(int32 NumElements, int32 NumAllocatedElements, int32 NumBytesPerElement) const
		{
			return DefaultCalculateSlackShrink(NumElements, NumAllocatedElements, NumBytesPerElement, true, Alignment);
		}
		FORCEINLINE int32 CalculateSlackGrow(int32 NumElements, int32 NumAllocatedElements, int32 NumBytesPerElement) const
		{
			return DefaultCalculateSlackGrow(NumElements, NumAllocatedElements, NumBytesPerElement, true, Alignment);
		}

		SIZE_T GetAllocatedSize(int32 NumAllocatedElements, SIZE_T NumBytesPerElement) const
		{
			return NumAllocatedElements * NumBytesPerElement;
		}

		bool HasAllocation()
		{
			return !!Data;
		}

	private:
		ForAnyElementType(const ForAnyElementType&);
		ForAnyElementType& operator=(const ForAnyElementType&);

		/** A pointer to the container's elements. */
		FScriptContainerElement* Data;
	};

	template<typename ElementType>
	class ForElementType : public ForAnyElementType
	{
	public:

		/** Default constructor. */
		ForElementType()
		{}

		FORCEINLINE ElementType* GetAllocation() const
		{
			return (ElementType*)ForAnyElementType::GetAllocation();
		}
	};
};

template <uint32 Alignment>
struct TAllocatorTraits<TAlignedHeapAllocator<Alignment>> : TAllocatorTraitsBase<TAlignedHeapAllocator<Alignment>>
{
	enum { SupportsMove    = true };
	enum { IsZeroConstruct = true };
};

/** The indirect allocation policy always allocates the elements indirectly. */
class CORE_API FHeapAllocator
{
public:

	enum { NeedsElementType = false };
	enum { RequireRangeCheck = true };

	class CORE_API ForAnyElementType
	{
	public:
		/** Default constructor. */
		ForAnyElementType()
			: Data(nullptr)
		{}

		/**
		 * Moves the state of another allocator into this one.
		 * Assumes that the allocator is currently empty, i.e. memory may be allocated but any existing elements have already been destructed (if necessary).
		 * @param Other - The allocator to move the state from.  This allocator should be left in a valid empty state.
		 */
		FORCEINLINE void MoveToEmpty(ForAnyElementType& Other)
		{
			checkSlow(this != &Other);

			if (Data)
			{
				FMemory::Free(Data);
			}

			Data       = Other.Data;
			Other.Data = nullptr;
		}

		/** Destructor. */
		FORCEINLINE ~ForAnyElementType()
		{
			if(Data)
			{
				FMemory::Free(Data);
			}
		}

		// FContainerAllocatorInterface
		FORCEINLINE FScriptContainerElement* GetAllocation() const
		{
			return Data;
		}
		FORCEINLINE void ResizeAllocation(int32 PreviousNumElements, int32 NumElements, SIZE_T NumBytesPerElement)
		{
			// Avoid calling FMemory::Realloc( nullptr, 0 ) as ANSI C mandates returning a valid pointer which is not what we want.
			if (Data || NumElements)
			{
				//checkSlow(((uint64)NumElements*(uint64)ElementTypeInfo.GetSize() < (uint64)INT_MAX));
				Data = (FScriptContainerElement*)FMemory::Realloc( Data, NumElements*NumBytesPerElement );
			}
		}
		FORCEINLINE int32 CalculateSlackReserve(int32 NumElements, int32 NumBytesPerElement) const
		{
			return DefaultCalculateSlackReserve(NumElements, NumBytesPerElement, true);
		}
		FORCEINLINE int32 CalculateSlackShrink(int32 NumElements, int32 NumAllocatedElements, int32 NumBytesPerElement) const
		{
			return DefaultCalculateSlackShrink(NumElements, NumAllocatedElements, NumBytesPerElement, true);
		}
		FORCEINLINE int32 CalculateSlackGrow(int32 NumElements, int32 NumAllocatedElements, int32 NumBytesPerElement) const
		{
			return DefaultCalculateSlackGrow(NumElements, NumAllocatedElements, NumBytesPerElement, true);
		}

		SIZE_T GetAllocatedSize(int32 NumAllocatedElements, SIZE_T NumBytesPerElement) const
		{
			return NumAllocatedElements * NumBytesPerElement;
		}

		bool HasAllocation()
		{
			return !!Data;
		}

	private:
		ForAnyElementType(const ForAnyElementType&);
		ForAnyElementType& operator=(const ForAnyElementType&);

		/** A pointer to the container's elements. */
		FScriptContainerElement* Data;
	};
	
	template<typename ElementType>
	class ForElementType : public ForAnyElementType
	{
	public:

		/** Default constructor. */
		ForElementType()
		{}

		FORCEINLINE ElementType* GetAllocation() const
		{
			return (ElementType*)ForAnyElementType::GetAllocation();
		}
	};
};

template <>
struct TAllocatorTraits<FHeapAllocator> : TAllocatorTraitsBase<FHeapAllocator>
{
	enum { SupportsMove    = true };
	enum { IsZeroConstruct = true };
};

class FDefaultAllocator;

/**
 * The inline allocation policy allocates up to a specified number of elements in the same allocation as the container.
 * Any allocation needed beyond that causes all data to be moved into an indirect allocation.
 * It always uses DEFAULT_ALIGNMENT.
 */
template <uint32 NumInlineElements, typename SecondaryAllocator = FDefaultAllocator>
class TInlineAllocator
{
public:

	enum { NeedsElementType = true };
	enum { RequireRangeCheck = true };

	template<typename ElementType>
	class ForElementType
	{
	public:

		/** Default constructor. */
		ForElementType()
		{
		}

		/**
		 * Moves the state of another allocator into this one.
		 * Assumes that the allocator is currently empty, i.e. memory may be allocated but any existing elements have already been destructed (if necessary).
		 * @param Other - The allocator to move the state from.  This allocator should be left in a valid empty state.
		 */
		FORCEINLINE void MoveToEmpty(ForElementType& Other)
		{
			checkSlow(this != &Other);

			if (!Other.SecondaryData.GetAllocation())
			{
				// Relocate objects from other inline storage only if it was stored inline in Other
				RelocateConstructItems<ElementType>((void*)InlineData, Other.GetInlineElements(), NumInlineElements);
			}

			// Move secondary storage in any case.
			// This will move secondary storage if it exists but will also handle the case where secondary storage is used in Other but not in *this.
			SecondaryData.MoveToEmpty(Other.SecondaryData);
		}

		// FContainerAllocatorInterface
		FORCEINLINE ElementType* GetAllocation() const
		{
			return IfAThenAElseB<ElementType>(SecondaryData.GetAllocation(),GetInlineElements());
		}

		void ResizeAllocation(int32 PreviousNumElements,int32 NumElements,SIZE_T NumBytesPerElement)
		{
			// Check if the new allocation will fit in the inline data area.
			if(NumElements <= NumInlineElements)
			{
				// If the old allocation wasn't in the inline data area, relocate it into the inline data area.
				if(SecondaryData.GetAllocation())
				{
					RelocateConstructItems<ElementType>((void*)InlineData, (ElementType*)SecondaryData.GetAllocation(), PreviousNumElements);

					// Free the old indirect allocation.
					SecondaryData.ResizeAllocation(0,0,NumBytesPerElement);
				}
			}
			else
			{
				if(!SecondaryData.GetAllocation())
				{
					// Allocate new indirect memory for the data.
					SecondaryData.ResizeAllocation(0,NumElements,NumBytesPerElement);

					// Move the data out of the inline data area into the new allocation.
					RelocateConstructItems<ElementType>((void*)SecondaryData.GetAllocation(), GetInlineElements(), PreviousNumElements);
				}
				else
				{
					// Reallocate the indirect data for the new size.
					SecondaryData.ResizeAllocation(PreviousNumElements,NumElements,NumBytesPerElement);
				}
			}
		}

		FORCEINLINE int32 CalculateSlackReserve(int32 NumElements, SIZE_T NumBytesPerElement) const
		{
			// If the elements use less space than the inline allocation, only use the inline allocation as slack.
			return NumElements <= NumInlineElements ?
				NumInlineElements :
								  SecondaryData.CalculateSlackReserve(NumElements, NumBytesPerElement);
		}
		FORCEINLINE int32 CalculateSlackShrink(int32 NumElements, int32 NumAllocatedElements, int32 NumBytesPerElement) const
		{
			// If the elements use less space than the inline allocation, only use the inline allocation as slack.
			return NumElements <= NumInlineElements ?
			NumInlineElements :
							  SecondaryData.CalculateSlackShrink(NumElements, NumAllocatedElements, NumBytesPerElement);
		}
		FORCEINLINE int32 CalculateSlackGrow(int32 NumElements, int32 NumAllocatedElements, int32 NumBytesPerElement) const
		{
			// If the elements use less space than the inline allocation, only use the inline allocation as slack.
			return NumElements <= NumInlineElements ?
			NumInlineElements :
							  SecondaryData.CalculateSlackGrow(NumElements, NumAllocatedElements, NumBytesPerElement);
		}

		SIZE_T GetAllocatedSize(int32 NumAllocatedElements, SIZE_T NumBytesPerElement) const
		{
			return SecondaryData.GetAllocatedSize(NumAllocatedElements, NumBytesPerElement);
		}

		bool HasAllocation()
		{
			return SecondaryData.HasAllocation();
		}

	private:
		ForElementType(const ForElementType&);
		ForElementType& operator=(const ForElementType&);

		/** The data is stored in this array if less than NumInlineElements is needed. */
		TTypeCompatibleBytes<ElementType> InlineData[NumInlineElements];

		/** The data is allocated through the indirect allocation policy if more than NumInlineElements is needed. */
		typename SecondaryAllocator::template ForElementType<ElementType> SecondaryData;

		/** @return the base of the aligned inline element data */
		ElementType* GetInlineElements() const
		{
			return (ElementType*)InlineData;
		}
	};

	typedef void ForAnyElementType;
};

template <uint32 NumInlineElements, typename SecondaryAllocator>
struct TAllocatorTraits<TInlineAllocator<NumInlineElements, SecondaryAllocator>> : TAllocatorTraitsBase<TInlineAllocator<NumInlineElements, SecondaryAllocator>>
{
	enum { SupportsMove = TAllocatorTraits<SecondaryAllocator>::SupportsMove };
};

/**
 * The fixed allocation policy allocates up to a specified number of elements in the same allocation as the container.
 * It's like the inline allocator, except it doesn't provide secondary storage when the inline storage has been filled.
 */
template <uint32 NumInlineElements>
class TFixedAllocator
{
public:

	enum { NeedsElementType = true };
	enum { RequireRangeCheck = true };

	template<typename ElementType>
	class ForElementType
	{
	public:

		/** Default constructor. */
		ForElementType()
		{
		}

		/**
		 * Moves the state of another allocator into this one.
		 * Assumes that the allocator is currently empty, i.e. memory may be allocated but any existing elements have already been destructed (if necessary).
		 * @param Other - The allocator to move the state from.  This allocator should be left in a valid empty state.
		 */
		FORCEINLINE void MoveToEmpty(ForElementType& Other)
		{
			checkSlow(this != &Other);

			// Relocate objects from other inline storage
			RelocateConstructItems<ElementType>((void*)InlineData, Other.GetInlineElements(), NumInlineElements);
		}

		// FContainerAllocatorInterface
		FORCEINLINE ElementType* GetAllocation() const
		{
			return GetInlineElements();
		}

		void ResizeAllocation(int32 PreviousNumElements,int32 NumElements,SIZE_T NumBytesPerElement)
		{
			// Ensure the requested allocation will fit in the inline data area.
			checkSlow(NumElements <= NumInlineElements);
		}

		FORCEINLINE int32 CalculateSlackReserve(int32 NumElements, SIZE_T NumBytesPerElement) const
		{
			// Ensure the requested allocation will fit in the inline data area.
			checkSlow(NumElements <= NumInlineElements);
			return NumInlineElements;
		}
		FORCEINLINE int32 CalculateSlackShrink(int32 NumElements, int32 NumAllocatedElements, int32 NumBytesPerElement) const
		{
			// Ensure the requested allocation will fit in the inline data area.
			checkSlow(NumAllocatedElements <= NumInlineElements);
			return NumInlineElements;
		}
		FORCEINLINE int32 CalculateSlackGrow(int32 NumElements, int32 NumAllocatedElements, int32 NumBytesPerElement) const
		{
			// Ensure the requested allocation will fit in the inline data area.
			checkSlow(NumElements <= NumInlineElements);
			return NumInlineElements;
		}

		SIZE_T GetAllocatedSize(int32 NumAllocatedElements, SIZE_T NumBytesPerElement) const
		{
			return 0;
		}

		bool HasAllocation()
		{
			return false;
		}


	private:
		ForElementType(const ForElementType&);
		ForElementType& operator=(const ForElementType&);

		/** The data is stored in this array if less than NumInlineElements is needed. */
		TTypeCompatibleBytes<ElementType> InlineData[NumInlineElements];

		/** @return the base of the aligned inline element data */
		ElementType* GetInlineElements() const
		{
			return (ElementType*)InlineData;
		}
	};

	typedef void ForAnyElementType;
};

template <uint32 NumInlineElements>
struct TAllocatorTraits<TFixedAllocator<NumInlineElements>> : TAllocatorTraitsBase<TFixedAllocator<NumInlineElements>>
{
	enum { SupportsMove = true };
};

// We want these to be correctly typed as int32, but we don't want them to have linkage, so we make them macros
#define NumBitsPerDWORD ((int32)32)
#define NumBitsPerDWORDLogTwo ((int32)5)

//
// Sparse array allocation definitions
//

class FDefaultAllocator;
class FDefaultBitArrayAllocator;

/** Encapsulates the allocators used by a sparse array in a single type. */
template<typename InElementAllocator = FDefaultAllocator,typename InBitArrayAllocator = FDefaultBitArrayAllocator>
class TSparseArrayAllocator
{
public:

	typedef InElementAllocator ElementAllocator;
	typedef InBitArrayAllocator BitArrayAllocator;
};

/** An inline sparse array allocator that allows sizing of the inline allocations for a set number of elements. */
template<
	uint32 NumInlineElements,
	typename SecondaryAllocator = TSparseArrayAllocator<FDefaultAllocator,FDefaultAllocator>
	>
class TInlineSparseArrayAllocator
{
private:

	/** The size to allocate inline for the bit array. */
	enum { InlineBitArrayDWORDs = (NumInlineElements + NumBitsPerDWORD - 1) / NumBitsPerDWORD};

public:

	typedef TInlineAllocator<NumInlineElements,typename SecondaryAllocator::ElementAllocator>		ElementAllocator;
	typedef TInlineAllocator<InlineBitArrayDWORDs,typename SecondaryAllocator::BitArrayAllocator>	BitArrayAllocator;
};

//
// Set allocation definitions.
//

#define DEFAULT_NUMBER_OF_ELEMENTS_PER_HASH_BUCKET	2
#define DEFAULT_BASE_NUMBER_OF_HASH_BUCKETS			8
#define DEFAULT_MIN_NUMBER_OF_HASHED_ELEMENTS		4

/** Encapsulates the allocators used by a set in a single type. */
template<
	typename InSparseArrayAllocator               = TSparseArrayAllocator<>,
	typename InHashAllocator                      = TInlineAllocator<1,FDefaultAllocator>,
	uint32   AverageNumberOfElementsPerHashBucket = DEFAULT_NUMBER_OF_ELEMENTS_PER_HASH_BUCKET,
	uint32   BaseNumberOfHashBuckets              = DEFAULT_BASE_NUMBER_OF_HASH_BUCKETS,
	uint32   MinNumberOfHashedElements            = DEFAULT_MIN_NUMBER_OF_HASHED_ELEMENTS
	>
class TSetAllocator
{
public:

	/** Computes the number of hash buckets to use for a given number of elements. */
	static FORCEINLINE uint32 GetNumberOfHashBuckets(uint32 NumHashedElements)
	{
		if(NumHashedElements >= MinNumberOfHashedElements)
		{
			return FPlatformMath::RoundUpToPowerOfTwo(NumHashedElements / AverageNumberOfElementsPerHashBucket + BaseNumberOfHashBuckets);
		}

		return 1;
	}

	typedef InSparseArrayAllocator SparseArrayAllocator;
	typedef InHashAllocator        HashAllocator;
};

class FDefaultAllocator;

/** An inline set allocator that allows sizing of the inline allocations for a set number of elements. */
template<
	uint32   NumInlineElements,
	typename SecondaryAllocator                   = TSetAllocator<TSparseArrayAllocator<FDefaultAllocator,FDefaultAllocator>,FDefaultAllocator>,
	uint32   AverageNumberOfElementsPerHashBucket = DEFAULT_NUMBER_OF_ELEMENTS_PER_HASH_BUCKET,
	uint32   MinNumberOfHashedElements            = DEFAULT_MIN_NUMBER_OF_HASHED_ELEMENTS
	>
class TInlineSetAllocator
{
private:

	enum { NumInlineHashBuckets = (NumInlineElements + AverageNumberOfElementsPerHashBucket - 1) / AverageNumberOfElementsPerHashBucket };

	static_assert(!(NumInlineHashBuckets & (NumInlineHashBuckets - 1)), "Number of inline buckets must be a power of two");

public:

	/** Computes the number of hash buckets to use for a given number of elements. */
	static FORCEINLINE uint32 GetNumberOfHashBuckets(uint32 NumHashedElements)
	{
		const uint32 NumDesiredHashBuckets = FPlatformMath::RoundUpToPowerOfTwo(NumHashedElements / AverageNumberOfElementsPerHashBucket);
		if (NumDesiredHashBuckets < NumInlineHashBuckets)
		{
			return NumInlineHashBuckets;
		}

		if (NumHashedElements < MinNumberOfHashedElements)
		{
			return NumInlineHashBuckets;
		}

		return NumDesiredHashBuckets;
	}

	typedef TInlineSparseArrayAllocator<NumInlineElements,typename SecondaryAllocator::SparseArrayAllocator> SparseArrayAllocator;
	typedef TInlineAllocator<NumInlineHashBuckets,typename SecondaryAllocator::HashAllocator>                HashAllocator;
};


/**
 * 'typedefs' for various allocator defaults.
 *
 * These should be replaced with actual typedefs when Core.h include order is sorted out, as then we won't need to
 * 'forward' these TAllocatorTraits specializations below.
 */

class FDefaultAllocator            : public FHeapAllocator          { public: typedef FHeapAllocator          Typedef; };
class FDefaultSetAllocator         : public TSetAllocator<>         { public: typedef TSetAllocator<>         Typedef; };
class FDefaultBitArrayAllocator    : public TInlineAllocator<4>     { public: typedef TInlineAllocator<4>     Typedef; };
class FDefaultSparseArrayAllocator : public TSparseArrayAllocator<> { public: typedef TSparseArrayAllocator<> Typedef; };

template <> struct TAllocatorTraits<FDefaultAllocator>            : TAllocatorTraits<typename FDefaultAllocator           ::Typedef> {};
template <> struct TAllocatorTraits<FDefaultSetAllocator>         : TAllocatorTraits<typename FDefaultSetAllocator        ::Typedef> {};
template <> struct TAllocatorTraits<FDefaultBitArrayAllocator>    : TAllocatorTraits<typename FDefaultBitArrayAllocator   ::Typedef> {};
template <> struct TAllocatorTraits<FDefaultSparseArrayAllocator> : TAllocatorTraits<typename FDefaultSparseArrayAllocator::Typedef> {};
