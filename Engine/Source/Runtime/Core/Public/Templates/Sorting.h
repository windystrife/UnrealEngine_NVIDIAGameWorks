// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/Algo/BinarySearch.h"
#include "Containers/Algo/Sort.h"
#include "HAL/PlatformMath.h"
#include "Templates/Less.h"

/**
 * Helper class for dereferencing pointer types in Sort function
 */
template<typename T, class PREDICATE_CLASS> 
struct TDereferenceWrapper
{
	const PREDICATE_CLASS& Predicate;

	TDereferenceWrapper( const PREDICATE_CLASS& InPredicate )
		: Predicate( InPredicate ) {}
  
	/** Pass through for non-pointer types */
	FORCEINLINE bool operator()( T& A, T& B ) { return Predicate( A, B ); } 
	FORCEINLINE bool operator()( const T& A, const T& B ) const { return Predicate( A, B ); } 
};
/** Partially specialized version of the above class */
template<typename T, class PREDICATE_CLASS> 
struct TDereferenceWrapper<T*, PREDICATE_CLASS>
{
	const PREDICATE_CLASS& Predicate;

	TDereferenceWrapper( const PREDICATE_CLASS& InPredicate )
		: Predicate( InPredicate ) {}
  
	/** Dereference pointers */
	FORCEINLINE bool operator()( T* A, T* B ) const 
	{
		return Predicate( *A, *B ); 
	} 
};

/**
 * Wraps a range into a container like interface to satisfy the GetData and GetNum global functions.
 * We're not using TArrayView since it calls ::Sort creating a circular dependency.
 */
template <typename T>
struct TArrayRange
{
	TArrayRange(T* InPtr, int32 InSize)
		: Begin(InPtr)
		, Size(InSize)
	{
	}

	T* GetData() const { return Begin; }
	int32 Num() const { return Size; }

private:
	T* Begin;
	int32 Size;
};

template <typename T>
struct TIsContiguousContainer< TArrayRange<T> >
{
	enum { Value = true };
};

/**
 * Sort elements using user defined predicate class. The sort is unstable, meaning that the ordering of equal items is not necessarily preserved.
 *
 * @param	First	pointer to the first element to sort
 * @param	Num		the number of items to sort
 * @param Predicate predicate class
 */
template<class T, class PREDICATE_CLASS> 
void Sort( T* First, const int32 Num, const PREDICATE_CLASS& Predicate )
{
	TArrayRange<T> ArrayRange( First, Num );
	Algo::Sort( ArrayRange, TDereferenceWrapper<T, PREDICATE_CLASS>( Predicate ) );
}

/**
 * Specialized version of the above Sort function for pointers to elements.
 *
 * @param	First	pointer to the first element to sort
 * @param	Num		the number of items to sort
 * @param Predicate predicate class
 */
template<class T, class PREDICATE_CLASS> 
void Sort( T** First, const int32 Num, const PREDICATE_CLASS& Predicate )
{
	TArrayRange<T*> ArrayRange( First, Num );
	Algo::Sort( ArrayRange, TDereferenceWrapper<T*, PREDICATE_CLASS>( Predicate ) );
}

/**
 * Sort elements. The sort is unstable, meaning that the ordering of equal items is not necessarily preserved.
 * Assumes < operator is defined for the template type.
 *
 * @param	First	pointer to the first element to sort
 * @param	Num		the number of items to sort
 */
template<class T> 
void Sort( T* First, const int32 Num )
{
	TArrayRange<T> ArrayRange( First, Num );
	Algo::Sort( ArrayRange, TDereferenceWrapper<T, TLess<T> >( TLess<T>() ) );
}

/**
 * Specialized version of the above Sort function for pointers to elements.
 *
 * @param	First	pointer to the first element to sort
 * @param	Num		the number of items to sort
 */
template<class T> 
void Sort( T** First, const int32 Num )
{
	TArrayRange<T*> ArrayRange( First, Num );
	Algo::Sort( ArrayRange, TDereferenceWrapper<T*, TLess<T> >( TLess<T>() ) );
}

/**
 * Stable merge to perform sort below. Stable sort is slower than non-stable
 * algorithm.
 *
 * @param Out Pointer to the first element of output array.
 * @param In Pointer to the first element to sort.
 * @param Mid Middle point of the table, i.e. merge separator.
 * @param Num Number of elements in the whole table.
 * @param Predicate Predicate class.
 */
template<class T, class PREDICATE_CLASS>
void Merge(T* Out, T* In, const int32 Mid, const int32 Num, const PREDICATE_CLASS& Predicate)
{
	int32 Merged = 0;
	int32 Picked;
	int32 A = 0, B = Mid;

	while (Merged < Num)
	{
		if (Merged != B && (B >= Num || !Predicate(In[B], In[A])))
		{
			Picked = A++;
		}
		else
		{
			Picked = B++;
		}

		Out[Merged] = In[Picked];

		++Merged;
	}
}

/**
 * Euclidean algorithm using modulo policy.
 */
class FEuclidDivisionGCD
{
public:
	/**
	 * Calculate GCD.
	 *
	 * @param A First parameter.
	 * @param B Second parameter.
	 *
	 * @returns Greatest common divisor of A and B.
	 */
	static int32 GCD(int32 A, int32 B)
	{
		while (B != 0)
		{
			int32 Temp = B;
			B = A % B;
			A = Temp;
		}

		return A;
	}
};

/**
 * Array rotation using juggling technique.
 *
 * @template_param TGCDPolicy Policy for calculating greatest common divisor.
 */
template <class TGCDPolicy>
class TJugglingRotation
{
public:
	/**
	 * Rotates array.
	 *
	 * @param First Pointer to the array.
	 * @param From Rotation starting point.
	 * @param To Rotation ending point.
	 * @param Amount Amount of steps to rotate.
	 */
	template <class T>
	static void Rotate(T* First, const int32 From, const int32 To, const int32 Amount)
	{
		if (Amount == 0)
		{
			return;
		}

		auto Num = To - From;
		auto GCD = TGCDPolicy::GCD(Num, Amount);
		auto CycleSize = Num / GCD;

		for (int32 Index = 0; Index < GCD; ++Index)
		{
			T BufferObject = MoveTemp(First[From + Index]);
			int32 IndexToFill = Index;

			for (int32 InCycleIndex = 0; InCycleIndex < CycleSize; ++InCycleIndex)
			{
				IndexToFill = (IndexToFill + Amount) % Num;
				Exchange(First[From + IndexToFill], BufferObject);
			}
		}
	}
};

/**
 * Merge policy for merge sort.
 *
 * @template_param TRotationPolicy Policy for array rotation algorithm.
 */
template <class TRotationPolicy>
class TRotationInPlaceMerge
{
public:
	/**
	 * Two sorted arrays merging function.
	 *
	 * @param First Pointer to array.
	 * @param Mid Middle point i.e. separation point of two arrays to merge.
	 * @param Num Number of elements in array.
	 * @param Predicate Predicate for comparison.
	 */
	template <class T, class PREDICATE_CLASS>
	static void Merge(T* First, const int32 Mid, const int32 Num, const PREDICATE_CLASS& Predicate)
	{
		int32 AStart = 0;
		int32 BStart = Mid;

		while (AStart < BStart && BStart < Num)
		{
			// Index after the last value == First[BStart]
			int32 NewAOffset = AlgoImpl::UpperBoundInternal(First + AStart, BStart - AStart, First[BStart], FIdentityFunctor(), Predicate);
			AStart += NewAOffset;

			if (AStart >= BStart) // done
				break;

			// Index of the first value == First[AStart]
			int32 NewBOffset = AlgoImpl::LowerBoundInternal(First + BStart, Num - BStart, First[AStart], FIdentityFunctor(), Predicate);
			TRotationPolicy::Rotate(First, AStart, BStart + NewBOffset, NewBOffset);
			BStart += NewBOffset;
			AStart += NewBOffset + 1;
		}
	}
};

/**
 * Merge sort class.
 *
 * @template_param TMergePolicy Merging policy.
 * @template_param MinMergeSubgroupSize Minimal size of the subgroup that should be merged.
 */
template <class TMergePolicy, int32 MinMergeSubgroupSize = 2>
class TMergeSort
{
public:
	/**
	 * Sort the array.
	 *
	 * @param First Pointer to the array.
	 * @param Num Number of elements in the array.
	 * @param Predicate Predicate for comparison.
	 */
	template<class T, class PREDICATE_CLASS>
	static void Sort(T* First, const int32 Num, const PREDICATE_CLASS& Predicate)
	{
		int32 SubgroupStart = 0;

		if (MinMergeSubgroupSize > 1)
		{
			if (MinMergeSubgroupSize > 2)
			{
				// First pass with simple bubble-sort.
				do
				{
					int32 GroupEnd = FPlatformMath::Min(SubgroupStart + MinMergeSubgroupSize, Num);
					do
					{
						for (int32 It = SubgroupStart; It < GroupEnd - 1; ++It)
						{
							if (Predicate(First[It + 1], First[It]))
							{
								Exchange(First[It], First[It + 1]);
							}
						}
						GroupEnd--;
					} while (GroupEnd - SubgroupStart > 1);

					SubgroupStart += MinMergeSubgroupSize;
				} while (SubgroupStart < Num);
			}
			else
			{
				for (int32 Subgroup = 0; Subgroup < Num; Subgroup += 2)
				{
					if (Subgroup + 1 < Num && Predicate(First[Subgroup + 1], First[Subgroup]))
					{
						Exchange(First[Subgroup], First[Subgroup + 1]);
					}
				}
			}
		}

		int32 SubgroupSize = MinMergeSubgroupSize;
		while (SubgroupSize < Num)
		{
			SubgroupStart = 0;
			do
			{
				TMergePolicy::Merge(
					First + SubgroupStart,
					SubgroupSize,
					FPlatformMath::Min(SubgroupSize << 1, Num - SubgroupStart),
					Predicate);
				SubgroupStart += SubgroupSize << 1;
			} while (SubgroupStart < Num);

			SubgroupSize <<= 1;
		}
	}
};

/**
 * Stable sort elements using user defined predicate class. The sort is stable,
 * meaning that the ordering of equal items is preserved, but it's slower than
 * non-stable algorithm.
 *
 * This is the internal sorting function used by StableSort overrides.
 *
 * @param	First	pointer to the first element to sort
 * @param	Num		the number of items to sort
 * @param Predicate predicate class
 */
template<class T, class PREDICATE_CLASS>
void StableSortInternal(T* First, const int32 Num, const PREDICATE_CLASS& Predicate)
{
	TMergeSort<TRotationInPlaceMerge<TJugglingRotation<FEuclidDivisionGCD> > >::Sort(First, Num, Predicate);
}

/**
 * Stable sort elements using user defined predicate class. The sort is stable,
 * meaning that the ordering of equal items is preserved, but it's slower than
 * non-stable algorithm.
 *
 * @param	First	pointer to the first element to sort
 * @param	Num		the number of items to sort
 * @param Predicate predicate class
 */
template<class T, class PREDICATE_CLASS>
void StableSort(T* First, const int32 Num, const PREDICATE_CLASS& Predicate)
{
	StableSortInternal(First, Num, TDereferenceWrapper<T, PREDICATE_CLASS>(Predicate));
}

/**
 * Specialized version of the above StableSort function for pointers to elements.
 * Stable sort is slower than non-stable algorithm.
 *
 * @param	First	pointer to the first element to sort
 * @param	Num		the number of items to sort
 * @param Predicate predicate class
 */
template<class T, class PREDICATE_CLASS>
void StableSort(T** First, const int32 Num, const PREDICATE_CLASS& Predicate)
{
	StableSortInternal(First, Num, TDereferenceWrapper<T*, PREDICATE_CLASS>(Predicate));
}

/**
 * Stable sort elements. The sort is stable, meaning that the ordering of equal
 * items is preserved, but it's slower than non-stable algorithm.
 *
 * Assumes < operator is defined for the template type.
 *
 * @param	First	pointer to the first element to sort
 * @param	Num		the number of items to sort
 */
template<class T>
void StableSort(T* First, const int32 Num)
{
	StableSortInternal(First, Num, TDereferenceWrapper<T, TLess<T> >(TLess<T>()));
}

/**
 * Specialized version of the above StableSort function for pointers to elements.
 * Stable sort is slower than non-stable algorithm.
 *
 * @param	First	pointer to the first element to sort
 * @param	Num		the number of items to sort
 */
template<class T>
void StableSort(T** First, const int32 Num)
{
	StableSortInternal(First, Num, TDereferenceWrapper<T*, TLess<T> >(TLess<T>()));
}

