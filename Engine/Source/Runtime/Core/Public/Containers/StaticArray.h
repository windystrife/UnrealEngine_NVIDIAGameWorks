// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "Templates/UnrealTypeTraits.h"
#include "Templates/TypeCompatibleBytes.h"
#include "Templates/MemoryOps.h"

/** An array with a static number of elements. */
template<typename TElement,uint32 NumElements,uint32 Alignment = alignof(TElement)>
class TStaticArray
{
public:

	/** Default constructor. */
	TStaticArray()
	{
		// Call the default constructor for each element using the in-place new operator.
		for(uint32 ElementIndex = 0;ElementIndex < NumElements;++ElementIndex)
		{
			new(&(*this)[ElementIndex]) TElement;
		}
	}

	/** Move constructor. */
	TStaticArray(TStaticArray&& Other)
	{
		MoveConstructItems((void*)Elements, (const TElement*)Other.Elements, NumElements);
	}

	/** Copy constructor. */
	TStaticArray(const TStaticArray& Other)
	{
		ConstructItems<TElement>((void*)Elements, (const TElement*)Other.Elements, NumElements);
	}

	/** Move assignment operator. */
	TStaticArray& operator=(TStaticArray&& Other)
	{
		MoveAssignItems((TElement*)Elements, (const TElement*)Other.Elements, NumElements);
		return *this;
	}
	
	/** Assignment operator. */
	TStaticArray& operator=(const TStaticArray& Other)
	{
		CopyAssignItems((TElement*)Elements, (const TElement*)Other.Elements, NumElements);
		return *this;
	}

	/** Destructor. */
	~TStaticArray()
	{
		DestructItems((TElement*)Elements, NumElements);
	}

	/** Serializer. */
	friend FArchive& operator<<(FArchive& Ar,TStaticArray& StaticArray)
	{
		for(uint32 Index = 0;Index < NumElements;++Index)
		{
			Ar << StaticArray[Index];
		}
		return Ar;
	}

	// Accessors.
	TElement& operator[](uint32 Index)
	{
		check(Index < NumElements);
		return *(TElement*)&Elements[Index];
	}

	const TElement& operator[](uint32 Index) const
	{
		check(Index < NumElements);
		return *(const TElement*)&Elements[Index];
	}

	// Comparisons.
	friend bool operator==(const TStaticArray& A,const TStaticArray& B)
	{
		for(uint32 ElementIndex = 0;ElementIndex < NumElements;++ElementIndex)
		{
			if(!(A[ElementIndex] == B[ElementIndex]))
			{
				return false;
			}
		}
		return true;
	}

	friend bool operator!=(const TStaticArray& A,const TStaticArray& B)
	{
		for(uint32 ElementIndex = 0;ElementIndex < NumElements;++ElementIndex)
		{
			if(!(A[ElementIndex] == B[ElementIndex]))
			{
				return true;
			}
		}
		return false;
	}

	/** The number of elements in the array. */
	int32 Num() const { return NumElements; }
	
	/** Hash function. */
	friend uint32 GetTypeHash(const TStaticArray& Array)
	{
		uint32 Result = 0;
		for(uint32 ElementIndex = 0;ElementIndex < NumElements;++ElementIndex)
		{
			Result ^= GetTypeHash(Array[ElementIndex]);
		}
		return Result;
	}

private:
	TAlignedBytes<sizeof(TElement),Alignment> Elements[NumElements];
};

/** A shortcut for initializing a TStaticArray with 2 elements. */
template<typename TElement>
class TStaticArray2 : public TStaticArray<TElement,2>
{
	typedef TStaticArray<TElement,2> Super;

public:

	TStaticArray2(
		typename TCallTraits<TElement>::ParamType In0,
		typename TCallTraits<TElement>::ParamType In1
		)
	{
		(*this)[0] = In0;
		(*this)[1] = In1;
	}

#if PLATFORM_COMPILER_HAS_DEFAULTED_FUNCTIONS

	TStaticArray2(TStaticArray2&&) = default;
	TStaticArray2(const TStaticArray2&) = default;
	TStaticArray2& operator=(TStaticArray2&&) = default;
	TStaticArray2& operator=(const TStaticArray2&) = default;

#else

	FORCEINLINE TStaticArray2(TStaticArray2&& Other)
		: Super((Super&&)Other)
	{
	}

	FORCEINLINE TStaticArray2(const TStaticArray2& Other)
		: Super((const Super&)Other)
	{
	}

	FORCEINLINE TStaticArray2& operator=(TStaticArray2&& Other)
	{
		(Super&)*this = (Super&&)Other;
		return *this;
	}

	FORCEINLINE TStaticArray2& operator=(const TStaticArray2& Other)
	{
		(Super&)*this = (const Super&)Other;
		return *this;
	}

#endif
};

/** A shortcut for initializing a TStaticArray with 3 elements. */
template<typename TElement>
class TStaticArray3 : public TStaticArray<TElement,3>
{
	typedef TStaticArray<TElement,3> Super;

public:

	TStaticArray3(
		typename TCallTraits<TElement>::ParamType In0,
		typename TCallTraits<TElement>::ParamType In1,
		typename TCallTraits<TElement>::ParamType In2
		)
	{
		(*this)[0] = In0;
		(*this)[1] = In1;
		(*this)[2] = In2;
	}

#if PLATFORM_COMPILER_HAS_DEFAULTED_FUNCTIONS

	TStaticArray3(TStaticArray3&&) = default;
	TStaticArray3(const TStaticArray3&) = default;
	TStaticArray3& operator=(TStaticArray3&&) = default;
	TStaticArray3& operator=(const TStaticArray3&) = default;

#else

	FORCEINLINE TStaticArray3(TStaticArray3&& Other)
		: Super((Super&&)Other)
	{
	}

	FORCEINLINE TStaticArray3(const TStaticArray3& Other)
		: Super((const Super&)Other)
	{
	}

	FORCEINLINE TStaticArray3& operator=(TStaticArray3&& Other)
	{
		(Super&)*this = (Super&&)Other;
		return *this;
	}

	FORCEINLINE TStaticArray3& operator=(const TStaticArray3& Other)
	{
		(Super&)*this = (const Super&)Other;
		return *this;
	}

#endif
};

/** A shortcut for initializing a TStaticArray with 4 elements. */
template<typename TElement>
class TStaticArray4 : public TStaticArray<TElement,4>
{
	typedef TStaticArray<TElement,4> Super;

public:

	TStaticArray4(
		typename TCallTraits<TElement>::ParamType In0,
		typename TCallTraits<TElement>::ParamType In1,
		typename TCallTraits<TElement>::ParamType In2,
		typename TCallTraits<TElement>::ParamType In3
		)
	{
		(*this)[0] = In0;
		(*this)[1] = In1;
		(*this)[2] = In2;
		(*this)[3] = In3;
	}

#if PLATFORM_COMPILER_HAS_DEFAULTED_FUNCTIONS

	TStaticArray4(TStaticArray4&&) = default;
	TStaticArray4(const TStaticArray4&) = default;
	TStaticArray4& operator=(TStaticArray4&&) = default;
	TStaticArray4& operator=(const TStaticArray4&) = default;

#else

	FORCEINLINE TStaticArray4(TStaticArray4&& Other)
		: Super((Super&&)Other)
	{
	}

	FORCEINLINE TStaticArray4(const TStaticArray4& Other)
		: Super((const Super&)Other)
	{
	}

	FORCEINLINE TStaticArray4& operator=(TStaticArray4&& Other)
	{
		(Super&)*this = (Super&&)Other;
		return *this;
	}

	FORCEINLINE TStaticArray4& operator=(const TStaticArray4& Other)
	{
		(Super&)*this = (const Super&)Other;
		return *this;
	}

#endif
};

/** Creates a static array filled with the specified value. */
template<typename TElement,uint32 NumElements>
TStaticArray<TElement,NumElements> MakeUniformStaticArray(typename TCallTraits<TElement>::ParamType InValue)
{
	TStaticArray<TElement,NumElements> Result;
	for(uint32 ElementIndex = 0;ElementIndex < NumElements;++ElementIndex)
	{
		Result[ElementIndex] = InValue;
	}
	return Result;
}

