// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Templates/IdentityFunctor.h"
#include "Templates/Invoke.h"
#include "Templates/UnrealTemplate.h" // For MoveTemp

namespace AlgoImpl
{
	template <typename RangeType>
	struct TRangePointerType
	{
		using Type = decltype(&*begin(DeclVal<RangeType&>()));
	};

	template <typename T, unsigned int N>
	struct TRangePointerType<T[N]>
	{
		using Type = T*;
	};

	template <typename RangeType, typename ValueType, typename ProjectionType>
	typename TRangePointerType<RangeType>::Type FindBy(RangeType& Range, const ValueType& Value, ProjectionType Proj)
	{
		for (auto& Elem : Range)
		{
			if (Invoke(Proj, Elem) == Value)
			{
				return &Elem;
			}
		}

		return nullptr;
	}

	template <typename RangeType, typename PredicateType>
	typename TRangePointerType<RangeType>::Type FindByPredicate(RangeType& Range, PredicateType Pred)
	{
		for (auto& Elem : Range)
		{
			if (Invoke(Pred, Elem))
			{
				return &Elem;
			}
		}

		return nullptr;
	}
}

namespace Algo
{
	/**
	 * Returns a pointer to the first element in the range which is equal to the given value.
	 *
	 * @param  Range  The range to search.
	 * @param  Value  The value to search for.
	 *
	 * @return A pointer to the first element found, or nullptr if none was found.
	 */
	template <typename RangeType, typename ValueType>
	FORCEINLINE auto Find(RangeType& Range, const ValueType& Value)
		-> decltype(AlgoImpl::FindBy(Range, Value, FIdentityFunctor()))
	{
		return AlgoImpl::FindBy(Range, Value, FIdentityFunctor());
	}

	/**
	 * Returns a pointer to the first element in the range whose projection is equal to the given value.
	 *
	 * @param  Range  The range to search.
	 * @param  Value  The value to search for.
	 * @param  Proj   The projection to apply to the element.
	 *
	 * @return A pointer to the first element found, or nullptr if none was found.
	 */
	template <typename RangeType, typename ValueType, typename ProjectionType>
	FORCEINLINE auto FindBy(RangeType& Range, const ValueType& Value, ProjectionType Proj)
		-> decltype(AlgoImpl::FindBy(Range, Value, MoveTemp(Proj)))
	{
		return AlgoImpl::FindBy(Range, Value, MoveTemp(Proj));
	}

	/**
	 * Returns a pointer to the first element in the range which matches the predicate.
	 *
	 * @param  Range  The range to search.
	 * @param  Pred   The predicate to search for.
	 *
	 * @return A pointer to the first element found, or nullptr if none was found.
	 */
	template <typename RangeType, typename PredicateType>
	FORCEINLINE auto FindByPredicate(RangeType& Range, PredicateType Pred)
		-> decltype(AlgoImpl::FindByPredicate(Range, MoveTemp(Pred)))
	{
		return AlgoImpl::FindByPredicate(Range, MoveTemp(Pred));
	}
}
