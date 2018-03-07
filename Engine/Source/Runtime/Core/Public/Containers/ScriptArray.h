// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "HAL/UnrealMemory.h"
#include "Containers/ContainerAllocationPolicies.h"
#include "Containers/Array.h"
#include <initializer_list>


/**
 * Base dynamic array.
 * An untyped data array; mirrors a TArray's members, but doesn't need an exact C++ type for its elements.
 **/
class FScriptArray
	: protected FHeapAllocator::ForAnyElementType
{
public:

	FORCEINLINE void* GetData()
	{
		return this->GetAllocation();
	}
	FORCEINLINE const void* GetData() const
	{
		return this->GetAllocation();
	}
	FORCEINLINE bool IsValidIndex(int32 i) const
	{
		return i>=0 && i<ArrayNum;
	}
	FORCEINLINE int32 Num() const
	{
		checkSlow(ArrayNum>=0);
		checkSlow(ArrayMax>=ArrayNum);
		return ArrayNum;
	}
	void InsertZeroed( int32 Index, int32 Count, int32 NumBytesPerElement )
	{
		Insert( Index, Count, NumBytesPerElement );
		FMemory::Memzero( (uint8*)this->GetAllocation()+Index*NumBytesPerElement, Count*NumBytesPerElement );
	}
	void Insert( int32 Index, int32 Count, int32 NumBytesPerElement )
	{
		check(Count>=0);
		check(ArrayNum>=0);
		check(ArrayMax>=ArrayNum);
		check(Index>=0);
		check(Index<=ArrayNum);

		const int32 OldNum = ArrayNum;
		if( (ArrayNum+=Count)>ArrayMax )
		{
			ResizeGrow(OldNum, NumBytesPerElement);
		}
		FMemory::Memmove
		(
			(uint8*)this->GetAllocation() + (Index+Count )*NumBytesPerElement,
			(uint8*)this->GetAllocation() + (Index       )*NumBytesPerElement,
			                                               (OldNum-Index)*NumBytesPerElement
		);
	}
	int32 Add( int32 Count, int32 NumBytesPerElement )
	{
		check(Count>=0);
		checkSlow(ArrayNum>=0);
		checkSlow(ArrayMax>=ArrayNum);

		const int32 OldNum = ArrayNum;
		if( (ArrayNum+=Count)>ArrayMax )
		{
			ResizeGrow(OldNum, NumBytesPerElement);
		}

		return OldNum;
	}
	int32 AddZeroed( int32 Count, int32 NumBytesPerElement )
	{
		const int32 Index = Add( Count, NumBytesPerElement );
		FMemory::Memzero( (uint8*)this->GetAllocation()+Index*NumBytesPerElement, Count*NumBytesPerElement );
		return Index;
	}
	void Shrink( int32 NumBytesPerElement )
	{
		checkSlow(ArrayNum>=0);
		checkSlow(ArrayMax>=ArrayNum);
		if (ArrayNum != ArrayMax)
		{
			ResizeTo(ArrayNum, NumBytesPerElement);
		}
	}
	void Empty( int32 Slack, int32 NumBytesPerElement )
	{
		checkSlow(Slack>=0);
		ArrayNum = 0;
		if (Slack != ArrayMax)
		{
			ResizeTo(Slack, NumBytesPerElement);
		}
	}
	void SwapMemory(int32 A, int32 B, int32 NumBytesPerElement )
	{
		FMemory::Memswap(
			(uint8*)this->GetAllocation()+(NumBytesPerElement*A),
			(uint8*)this->GetAllocation()+(NumBytesPerElement*B),
			NumBytesPerElement
			);
	}
	FScriptArray()
	:   ArrayNum( 0 )
	,	ArrayMax( 0 )
	{
	}
	void CountBytes( FArchive& Ar, int32 NumBytesPerElement  )
	{
		Ar.CountBytes( ArrayNum*NumBytesPerElement, ArrayMax*NumBytesPerElement );
	}
	/**
	 * Returns the amount of slack in this array in elements.
	 */
	FORCEINLINE int32 GetSlack() const
	{
		return ArrayMax - ArrayNum;
	}
		
	void Remove( int32 Index, int32 Count, int32 NumBytesPerElement  )
	{
		if (Count)
		{
			checkSlow(Count >= 0);
			checkSlow(Index >= 0);
			checkSlow(Index <= ArrayNum);
			checkSlow(Index + Count <= ArrayNum);

			// Skip memmove in the common case that there is nothing to move.
			int32 NumToMove = ArrayNum - Index - Count;
			if (NumToMove)
			{
				FMemory::Memmove
					(
					(uint8*)this->GetAllocation() + (Index)* NumBytesPerElement,
					(uint8*)this->GetAllocation() + (Index + Count) * NumBytesPerElement,
					NumToMove * NumBytesPerElement
					);
			}
			ArrayNum -= Count;

			ResizeShrink(NumBytesPerElement);
			checkSlow(ArrayNum >= 0);
			checkSlow(ArrayMax >= ArrayNum);
		}
	}

protected:

	FScriptArray( int32 InNum, int32 NumBytesPerElement  )
	:   ArrayNum( 0 )
	,	ArrayMax( InNum )

	{
		if (ArrayMax)
		{
			ResizeInit(NumBytesPerElement);
		}
		ArrayNum = InNum;
	}
	int32	  ArrayNum;
	int32	  ArrayMax;

	FORCENOINLINE void ResizeInit(int32 NumBytesPerElement)
	{
		ArrayMax = this->CalculateSlackReserve(ArrayMax, NumBytesPerElement);
		this->ResizeAllocation(ArrayNum, ArrayMax, NumBytesPerElement);
	}
	FORCENOINLINE void ResizeGrow(int32 OldNum, int32 NumBytesPerElement)
	{
		ArrayMax = this->CalculateSlackGrow(ArrayNum, ArrayMax, NumBytesPerElement);
		this->ResizeAllocation(OldNum, ArrayMax, NumBytesPerElement);
	}
	FORCENOINLINE void ResizeShrink(int32 NumBytesPerElement)
	{
		const int32 NewArrayMax = this->CalculateSlackShrink(ArrayNum, ArrayMax, NumBytesPerElement);
		if (NewArrayMax != ArrayMax)
		{
			ArrayMax = NewArrayMax;
			this->ResizeAllocation(ArrayNum, ArrayMax, NumBytesPerElement);
		}
	}
	FORCENOINLINE void ResizeTo(int32 NewMax, int32 NumBytesPerElement)
	{
		if (NewMax)
		{
			NewMax = this->CalculateSlackReserve(NewMax, NumBytesPerElement);
		}
		if (NewMax != ArrayMax)
		{
			ArrayMax = NewMax;
			this->ResizeAllocation(ArrayNum, ArrayMax, NumBytesPerElement);
		}
	}
public:
	// These should really be private, because they shouldn't be called, but there's a bunch of code
	// that needs to be fixed first.
	FScriptArray(const FScriptArray&) { check(false); }
	void operator=(const FScriptArray&) { check(false); }
};


template<> struct TIsZeroConstructType<FScriptArray> { enum { Value = true }; };
