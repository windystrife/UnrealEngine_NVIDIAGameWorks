// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "HAL/UnrealMemory.h"
#include "Math/UnrealMathUtility.h"
#include "Containers/UnrealString.h"

/** Used to read/write a bit in the static array as a bool. */
template<typename T>
class TStaticBitReference
{
public:

	TStaticBitReference(T& InData,T InMask)
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

private:

	T& Data;
	T Mask;
};


/** Used to read a bit in the static array as a bool. */
template<typename T>
class TConstStaticBitReference
{
public:

	TConstStaticBitReference(const T& InData,T InMask)
		: Data(InData)
		, Mask(InMask)
	{ }

	FORCEINLINE operator bool() const
	{
		 return (Data & Mask) != 0;
	}

private:

	const T& Data;
	T Mask;
};


/**
 * A statically sized bit array.
 */
template<uint32 NumBits>
class TStaticBitArray
{
	typedef uint64 WordType;

	struct FBoolType;
	typedef int32* FBoolType::* UnspecifiedBoolType;
	typedef float* FBoolType::* UnspecifiedZeroType;

public:

	/** Minimal initialization constructor */
	FORCEINLINE TStaticBitArray()
	{
		Clear_();
	}

	/** Constructor that allows initializing by assignment from 0 */
	FORCEINLINE TStaticBitArray(UnspecifiedZeroType)
	{
		Clear_();
	}

	/**
	 * Constructor to initialize to a single bit
	 */
	FORCEINLINE TStaticBitArray(bool, uint32 InBitIndex)
	{
		/***********************************************************************

		 If this line fails to compile you are attempting to construct a bit
		 array with an out-of bounds bit index. Follow the compiler errors to
		 the initialization point

		***********************************************************************/
//		static_assert(InBitIndex >= 0 && InBitIndex < NumBits, "Invalid bit value.");

		check((NumBits > 0) ? (InBitIndex<NumBits):1);

		uint32 DestWordIndex = InBitIndex / NumBitsPerWord;
		WordType Word = (WordType)1 << (InBitIndex & (NumBitsPerWord - 1));

		for(int32 WordIndex = 0; WordIndex < NumWords; ++WordIndex)
		{
			Words[WordIndex] = WordIndex == DestWordIndex ? Word : (WordType)0;
		}
	}

	/**
	 * Constructor to initialize from string
	 */
	explicit TStaticBitArray(const FString& Str)
	{
		int32 Length = Str.Len();

		// Trim count to length of bit array
		if(NumBits < Length)
		{
			Length = NumBits;
		}
		Clear_();

		int32 Pos = Length;
		for(int32 Index = 0; Index < Length; ++Index)
		{
			const TCHAR ch = Str[--Pos];
			if(ch == TEXT('1'))
			{
				operator[](Index) = true;
			}
			else if(ch != TEXT('0'))
			{
				ErrorInvalid_();
			}
		}
	}

	// Conversion to bool
	FORCEINLINE operator UnspecifiedBoolType() const
	{
		WordType And = 0;
		for(int32 Index = 0; Index < NumWords; ++Index)
		{
			And |= Words[Index];
		}

		return And ? &FBoolType::Valid : NULL;
	}

	// Accessors.
	FORCEINLINE static int32 Num() { return NumBits; }
	FORCEINLINE TStaticBitReference<WordType> operator[](int32 Index)
	{
		check(Index>=0 && Index<NumBits);
		return TStaticBitReference<WordType>(
			Words[Index / NumBitsPerWord],
			(WordType)1 << (Index & (NumBitsPerWord - 1))
			);
	}
	FORCEINLINE const TConstStaticBitReference<WordType> operator[](int32 Index) const
	{
		check(Index>=0 && Index<NumBits);
		return TConstStaticBitReference<WordType>(
			Words[Index / NumBitsPerWord],
			(WordType)1 << (Index & (NumBitsPerWord - 1))
			);
	}

	// Modifiers.
	FORCEINLINE TStaticBitArray& operator|=(const TStaticBitArray& Other)
	{
		for(int32 Index = 0; Index < NumWords; ++Index)
		{
			Words[Index] |= Other.Words[Index];
		}
		return *this;
	}
	FORCEINLINE TStaticBitArray& operator&=(const TStaticBitArray& Other)
	{
		for(int32 Index = 0; Index < NumWords; ++Index)
		{
			Words[Index] &= Other.Words[Index];
		}
		return *this;
	}
	FORCEINLINE TStaticBitArray& operator^=(const TStaticBitArray& Other )
	{
		for(int32 Index = 0; Index < NumWords; ++Index)
		{
			Words[Index] ^= Other.Words[Index];
		}
		return *this;
	}
	friend FORCEINLINE TStaticBitArray<NumBits> operator~(const TStaticBitArray<NumBits>& A)
	{
		TStaticBitArray Result;
		for(int32 Index = 0; Index < NumWords; ++Index)
		{
			Result.Words[Index] = ~A.Words[Index];
		}
		Result.Trim_();
		return Result;
	}
	friend FORCEINLINE TStaticBitArray<NumBits> operator|(const TStaticBitArray<NumBits>& A, const TStaticBitArray<NumBits>& B)
	{
		// is not calling |= because doing it in here has less LoadHitStores and is therefore faster.
		TStaticBitArray Results(0);

		const WordType* RESTRICT APtr = (const WordType* RESTRICT)A.Words;
		const WordType* RESTRICT BPtr = (const WordType* RESTRICT)B.Words;
		WordType* RESTRICT ResultsPtr = (WordType* RESTRICT)Results.Words;
		for(int32 Index = 0; Index < NumWords; ++Index)
		{
			ResultsPtr[Index] = APtr[Index] | BPtr[Index];
		}

		return Results;
	}
	friend FORCEINLINE TStaticBitArray<NumBits> operator&(const TStaticBitArray<NumBits>& A, const TStaticBitArray<NumBits>& B)
	{
		// is not calling &= because doing it in here has less LoadHitStores and is therefore faster.
		TStaticBitArray Results(0);

		const WordType* RESTRICT APtr = (const WordType* RESTRICT)A.Words;
		const WordType* RESTRICT BPtr = (const WordType* RESTRICT)B.Words;
		WordType* RESTRICT ResultsPtr = (WordType* RESTRICT)Results.Words;
		for(int32 Index = 0; Index < NumWords; ++Index)
		{
			ResultsPtr[Index] = APtr[Index] & BPtr[Index];
		}

		return Results;
	}
	friend FORCEINLINE TStaticBitArray<NumBits> operator^(const TStaticBitArray<NumBits>& A, const TStaticBitArray<NumBits>& B)
	{
		TStaticBitArray Results(A);
		Results ^= B;
		return Results;
	}
	friend FORCEINLINE bool operator==(const TStaticBitArray<NumBits>& A, const TStaticBitArray<NumBits>& B)
	{
		for(int32 Index = 0; Index < A.NumWords; ++Index)
		{
			if(A.Words[Index] != B.Words[Index])
			{
				return false;
			}
		}
		return true;
	}
	/** This operator only exists to disambiguate == in statements of the form (flags == 0) */
	friend FORCEINLINE bool operator==(const TStaticBitArray<NumBits>& A, UnspecifiedBoolType Value)
	{
		return (UnspecifiedBoolType)A == Value;
	}
	/** != simple maps to == */
	friend FORCEINLINE bool operator!=(const TStaticBitArray<NumBits>& A, const TStaticBitArray<NumBits>& B)
	{
		return !(A == B);
	}
	/** != simple maps to == */
	friend FORCEINLINE bool operator!=(const TStaticBitArray<NumBits>& A, UnspecifiedBoolType Value)
	{
		return !(A == Value);
	}
	
	/**
	 * Finds the first clear bit in the array and returns the bit index.
	 * If there isn't one, INDEX_NONE is returned.
	 */
	int32 FindFirstClearBit() const
	{
		static const int32 NumBitsPerWordLog2 = FMath::FloorLog2(NumBitsPerWord);

		const int32 LocalNumBits = NumBits;

		int32 WordIndex = 0;
		// Iterate over the array until we see a word with a unset bit.
		while (WordIndex < NumWords && Words[WordIndex] == WordType(-1))
		{
			++WordIndex;
		}

		if (WordIndex < NumWords)
		{
			// Flip the bits, then we only need to find the first one bit -- easy.
			const WordType Bits = ~(Words[WordIndex]);
			ASSUME(Bits != 0);

			const uint32 LowestBit = (Bits) & (-WordType(Bits));
			const int32 LowestBitIndex = FMath::CountTrailingZeros(Bits) + (WordIndex << NumBitsPerWordLog2);
			if (LowestBitIndex < LocalNumBits)
			{
				return LowestBitIndex;
			}
		}

		return INDEX_NONE;
	}

	/**
	 * Finds the first set bit in the array and returns it's index.
	 * If there isn't one, INDEX_NONE is returned.
	 */
	int32 FindFirstSetBit() const
	{
		static const int32 NumBitsPerWordLog2 = FMath::FloorLog2(NumBitsPerWord);

		const int32 LocalNumBits = NumBits;

		int32 WordIndex = 0;
		// Iterate over the array until we see a word with a set bit.
		while (WordIndex < NumWords && Words[WordIndex] == WordType(0))
		{
			++WordIndex;
		}

		if (WordIndex < NumWords)
		{
			const WordType Bits = Words[WordIndex];
			ASSUME(Bits != 0);

			const uint32 LowestBit = (Bits) & (-WordType(Bits));
			const int32 LowestBitIndex = FMath::CountTrailingZeros(Bits) + (WordIndex << NumBitsPerWordLog2);
			if (LowestBitIndex < LocalNumBits)
			{
				return LowestBitIndex;
			}
		}

		return INDEX_NONE;
	}

	/**
	 * Serializer.
	 */
	friend FArchive& operator<<(FArchive& Ar, TStaticBitArray& BitArray)
	{
		uint32 ArchivedNumWords = BitArray.NumWords;
		Ar << ArchivedNumWords;

		if(Ar.IsLoading())
		{
			FMemory::Memset(BitArray.Words, 0, sizeof(BitArray.Words));
			ArchivedNumWords = FMath::Min(BitArray.NumWords, ArchivedNumWords);
		}

		Ar.Serialize(BitArray.Words, ArchivedNumWords * sizeof(BitArray.Words[0]));
		return Ar;
	}

	/**
	 * Converts the bitarray to a string representing the binary representation of the array
	 */
	FString ToString() const
	{
		FString Str;
		Str.Empty(NumBits);

		for(int32 Index = NumBits - 1; Index >= 0; --Index)
		{
			Str += operator[](Index) ? TEXT('1') : TEXT('0');
		}

		return Str;
	}

	static const uint32 NumOfBits = NumBits;

private:

//	static_assert(NumBits > 0, "Must have at least 1 bit.");
	static const uint32 NumBitsPerWord = sizeof(WordType) * 8;
	static const uint32 NumWords = ((NumBits + NumBitsPerWord - 1) & ~(NumBitsPerWord - 1)) / NumBitsPerWord;
	WordType Words[NumWords];

	// Helper class for bool conversion
	struct FBoolType
	{
		int32* Valid;
	};

	/**
	 * Resets the bit array to a 0 value
	 */
	FORCEINLINE void Clear_()
	{
		for(int32 Index = 0; Index < NumWords; ++Index)
		{
			Words[Index] = 0;
		}
	}

	/**
	 * Clears any trailing bits in the last word
	 */
	void Trim_()
	{
		if(NumBits % NumBitsPerWord != 0)
		{
			Words[NumWords-1] &= (WordType(1) << (NumBits % NumBitsPerWord)) - 1;
		}
	}

	/**
	 * Reports an invalid string element in the bitset conversion
	 */
	void ErrorInvalid_() const
	{
		LowLevelFatalError(TEXT("invalid TStaticBitArray<NumBits> character"));
	}
};
