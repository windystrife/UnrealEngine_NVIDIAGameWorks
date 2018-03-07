// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/IdentityFunctor.h"
#include "Templates/Invoke.h"
#include "Templates/Less.h"

namespace AlgoImpl
{
	/**
	 * Performs binary search, resulting in position of the first element >= Value
	 *
	 * @param First Pointer to array
	 * @param Num Number of elements in array
	 * @param Value Value to look for
	 * @param Projection Called on values in array to get type that can be compared to Value
	 * @param SortPredicate Predicate for sort comparison
	 *
	 * @returns Position of the first element >= Value, may be == Num
	 */
	template <typename RangeValueType, typename PredicateValueType, typename ProjectionType, typename SortPredicateType>
	FORCEINLINE SIZE_T LowerBoundInternal(RangeValueType* First, const SIZE_T Num, const PredicateValueType& Value, ProjectionType Projection, SortPredicateType SortPredicate)
	{
		// Current start of sequence to check
		SIZE_T Start = 0;
		// Size of sequence to check
		SIZE_T Size = Num;

		// With this method, if Size is even it will do one more comparison than necessary, but because Size can be predicted by the CPU it is faster in practice
		while (Size > 0)
		{
			const SIZE_T LeftoverSize = Size % 2;
			Size = Size / 2;

			const SIZE_T CheckIndex = Start + Size;
			const SIZE_T StartIfLess = CheckIndex + LeftoverSize;

			auto&& CheckValue = Invoke(Projection, First[CheckIndex]);
			Start = SortPredicate(CheckValue, Value) ? StartIfLess : Start;
		}
		return Start;
	}

	/**
	 * Performs binary search, resulting in position of the first element that is larger than the given value
	 *
	 * @param First Pointer to array
	 * @param Num Number of elements in array
	 * @param Value Value to look for
	 * @param SortPredicate Predicate for sort comparison
	 *
	 * @returns Position of the first element > Value, may be == Num
	 */
	template <typename RangeValueType, typename PredicateValueType, typename ProjectionType, typename SortPredicateType>
	FORCEINLINE SIZE_T UpperBoundInternal(RangeValueType* First, const SIZE_T Num, const PredicateValueType& Value, ProjectionType Projection, SortPredicateType SortPredicate)
	{
		// Current start of sequence to check
		SIZE_T Start = 0;
		// Size of sequence to check
		SIZE_T Size = Num;

		// With this method, if Size is even it will do one more comparison than necessary, but because Size can be predicted by the CPU it is faster in practice
		while (Size > 0)
		{
			const SIZE_T LeftoverSize = Size % 2;
			Size = Size / 2;

			const SIZE_T CheckIndex = Start + Size;
			const SIZE_T StartIfLess = CheckIndex + LeftoverSize;

			auto&& CheckValue = Invoke(Projection, First[CheckIndex]);
			Start = !SortPredicate(Value, CheckValue) ? StartIfLess : Start;
		}

		return Start;
	}
}

namespace Algo
{
	/**
	 * Performs binary search, resulting in position of the first element >= Value using predicate
	 *
	 * @param Range Range to search through, must be already sorted by SortPredicate
	 * @param Value Value to look for
	 * @param SortPredicate Predicate for sort comparison, defaults to <
	 *
	 * @returns Position of the first element >= Value, may be position after last element in range
	 */
	template <typename RangeType, typename ValueType, typename SortPredicateType>
	FORCEINLINE int32 LowerBound(RangeType& Range, const ValueType& Value, SortPredicateType SortPredicate)
	{
		return AlgoImpl::LowerBoundInternal(GetData(Range), GetNum(Range), Value, FIdentityFunctor(), SortPredicate);
	}
	template <typename RangeType, typename ValueType>
	FORCEINLINE int32 LowerBound(RangeType& Range, const ValueType& Value)
	{
		return AlgoImpl::LowerBoundInternal(GetData(Range), GetNum(Range), Value, FIdentityFunctor(), TLess<>());
	}

	/**
	 * Performs binary search, resulting in position of the first element with projected value >= Value using predicate
	 *
	 * @param Range Range to search through, must be already sorted by SortPredicate
	 * @param Value Value to look for
	 * @param Projection Functor or data member pointer, called via Invoke to compare to Value
	 * @param SortPredicate Predicate for sort comparison, defaults to <
	 *
	 * @returns Position of the first element >= Value, may be position after last element in range
	 */
	template <typename RangeType, typename ValueType, typename ProjectionType, typename SortPredicateType>
	FORCEINLINE int32 LowerBoundBy(RangeType& Range, const ValueType& Value, ProjectionType Projection, SortPredicateType SortPredicate)
	{
		return AlgoImpl::LowerBoundInternal(GetData(Range), GetNum(Range), Value, Projection, SortPredicate);
	}
	template <typename RangeType, typename ValueType, typename ProjectionType>
	FORCEINLINE int32 LowerBoundBy(RangeType& Range, const ValueType& Value, ProjectionType Projection)
	{
		return AlgoImpl::LowerBoundInternal(GetData(Range), GetNum(Range), Value, Projection, TLess<>());
	}

	/**
	 * Performs binary search, resulting in position of the first element > Value using predicate
	 *
	 * @param Range Range to search through, must be already sorted by SortPredicate
	 * @param Value Value to look for
	 * @param SortPredicate Predicate for sort comparison, defaults to <
	 *
	 * @returns Position of the first element > Value, may be past end of range
	 */
	template <typename RangeType, typename ValueType, typename SortPredicateType>
	FORCEINLINE int32 UpperBound(RangeType& Range, const ValueType& Value, SortPredicateType SortPredicate)
	{
		return AlgoImpl::UpperBoundInternal(GetData(Range), GetNum(Range), Value, FIdentityFunctor(), SortPredicate);
	}
	template <typename RangeType, typename ValueType>
	FORCEINLINE int32 UpperBound(RangeType& Range, const ValueType& Value)
	{
		return AlgoImpl::UpperBoundInternal(GetData(Range), GetNum(Range), Value, FIdentityFunctor(), TLess<>());
	}

	/**
	 * Performs binary search, resulting in position of the first element with projected value > Value using predicate
	 *
	 * @param Range Range to search through, must be already sorted by SortPredicate
	 * @param Value Value to look for
	 * @param Projection Functor or data member pointer, called via Invoke to compare to Value
	 * @param SortPredicate Predicate for sort comparison, defaults to <
	 *
	 * @returns Position of the first element > Value, may be past end of range
	 */
	template <typename RangeType, typename ValueType, typename ProjectionType, typename SortPredicateType>
	FORCEINLINE int32 UpperBoundBy(RangeType& Range, const ValueType& Value, ProjectionType Projection, SortPredicateType SortPredicate)
	{
		return AlgoImpl::UpperBoundInternal(GetData(Range), GetNum(Range), Value, Projection, SortPredicate);
	}
	template <typename RangeType, typename ValueType, typename ProjectionType>
	FORCEINLINE int32 UpperBoundBy(RangeType& Range, const ValueType& Value, ProjectionType Projection)
	{
		return AlgoImpl::UpperBoundInternal(GetData(Range), GetNum(Range), Value, Projection);
	}

	/**
	 * Returns index to the first found element matching a value in a range, the range must be sorted by <
	 *
	 * @param Range The range to search, must be already sorted by SortPredicate
	 * @param Value The value to search for
	 * @param SortPredicate Predicate for sort comparison, defaults to <
	 * @return Index of found element, or INDEX_NONE
	 */
	template <typename RangeType, typename ValueType, typename SortPredicateType>
	FORCEINLINE int32 BinarySearch(RangeType& Range, const ValueType& Value, SortPredicateType SortPredicate)
	{
		SIZE_T CheckIndex = LowerBound(Range, Value, SortPredicate);
		if (CheckIndex < GetNum(Range))
		{
			auto&& CheckValue = GetData(Range)[CheckIndex];
			// Since we returned lower bound we already know Value <= CheckValue. So if Value is not < CheckValue, they must be equal
			if (!SortPredicate(Value, CheckValue))
			{
				return CheckIndex;
			}
		}
		return INDEX_NONE;
	}
	template <typename RangeType, typename ValueType>
	FORCEINLINE int32 BinarySearch(RangeType& Range, const ValueType& Value)
	{
		return BinarySearch(Range, Value, TLess<>());
	}

	/**
	 * Returns index to the first found element with projected value matching Value in a range, the range must be sorted by predicate
	 *
	 * @param Range The range to search, must be already sorted by SortPredicate
	 * @param Value The value to search for
	 * @param Projection Functor or data member pointer, called via Invoke to compare to Value
	 * @param SortPredicate Predicate for sort comparison, defaults to <
	 * @return Index of found element, or INDEX_NONE
	 */
	template <typename RangeType, typename ValueType, typename ProjectionType, typename SortPredicateType>
	FORCEINLINE int32 BinarySearchBy(RangeType& Range, const ValueType& Value, ProjectionType Projection, SortPredicateType SortPredicate)
	{
		SIZE_T CheckIndex = LowerBoundBy(Range, Value, Projection, SortPredicate);
		if (CheckIndex < GetNum(Range))
		{
			auto&& CheckValue = Invoke(Projection, GetData(Range)[CheckIndex]);
			// Since we returned lower bound we already know Value <= CheckValue. So if Value is not < CheckValue, they must be equal
			if (!SortPredicate(Value, CheckValue))
			{
				return CheckIndex;
			}
		}
		return INDEX_NONE;
	}
	template <typename RangeType, typename ValueType, typename ProjectionType>
	FORCEINLINE int32 BinarySearchBy(RangeType& Range, const ValueType& Value, ProjectionType Projection)
	{
		return BinarySearchBy(Range, Value, Projection, TLess<>());
	}
}
