// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=====================================================================================================
	AnsiAllocator.h: helper allocator that allocates directly from standard library allocation functions
=======================================================================================================*/

#pragma once

#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "Containers/ContainerAllocationPolicies.h"

/** Allocator that allocates memory using standard library functions. */
class CORE_API FAnsiAllocator
{
public:

	enum { NeedsElementType = false };
	enum { RequireRangeCheck = true };

	typedef FAnsiAllocator ElementAllocator;
	typedef FAnsiAllocator BitArrayAllocator;

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
			check(this != &Other);

			if (Data)
			{
				::free(Data);
			}

			Data = Other.Data;
			Other.Data = nullptr;
		}

		/** Destructor. */
		FORCEINLINE ~ForAnyElementType()
		{
			if (Data)
			{
				::free(Data);
			}
		}

		// FContainerAllocatorInterface
		FORCEINLINE FScriptContainerElement* GetAllocation() const
		{
			return Data;
		}
		void ResizeAllocation(int32 PreviousNumElements, int32 NumElements, SIZE_T NumBytesPerElement)
		{
			// Avoid calling FMemory::Realloc( nullptr, 0 ) as ANSI C mandates returning a valid pointer which is not what we want.
			if (NumElements)
			{
				//checkSlow(((uint64)NumElements*(uint64)ElementTypeInfo.GetSize() < (uint64)INT_MAX));
				void* NewRealloc = ::realloc(Data, NumElements*NumBytesPerElement);
				Data = (FScriptContainerElement*)NewRealloc;
			}
			else
			{
				::free(Data);
				Data = nullptr;
			}
		}
		int32 CalculateSlackReserve(int32 NumElements, int32 NumBytesPerElement) const
		{
			return DefaultCalculateSlackReserve(NumElements, NumBytesPerElement, false);
		}
		int32 CalculateSlackShrink(int32 NumElements, int32 NumAllocatedElements, int32 NumBytesPerElement) const
		{
			return DefaultCalculateSlackShrink(NumElements, NumAllocatedElements, NumBytesPerElement, false);
		}
		int32 CalculateSlackGrow(int32 NumElements, int32 NumAllocatedElements, int32 NumBytesPerElement) const
		{
			return DefaultCalculateSlackGrow(NumElements, NumAllocatedElements, NumBytesPerElement, false);
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
struct TAllocatorTraits<FAnsiAllocator> : TAllocatorTraitsBase<FAnsiAllocator>
{
	enum { SupportsMove = true };
	enum { IsZeroConstruct = true };
};

/** ANSI allocator that can be used with a TSet. */
class FAnsiSetAllocator : public TSetAllocator<FAnsiAllocator, TInlineAllocator<1, FAnsiAllocator>>
{	
};
