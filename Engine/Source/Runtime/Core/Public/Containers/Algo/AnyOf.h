// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Algo/NoneOf.h"
#include "Templates/UnrealTemplate.h" // for MoveTemp


namespace Algo
{
	/**
	 * Checks if any element in the range is truthy.
	 *
	 * @param  Range  The range to check.
	 *
	 * @return  true if at least one element is truthy, false otherwise.
	 */
	template <typename RangeType>
	FORCEINLINE bool AnyOf(const RangeType& Range)
	{
		return !Algo::NoneOf(Range);
	}

	/**
	 * Checks if any projection of the elements in the range is truthy.
	 *
	 * @param  Range       The range to check.
	 * @param  Projection  The projection to apply to each element.
	 *
	 * @return  true if at least one projection is truthy, false otherwise.
	 */
	template <typename RangeType, typename ProjectionType>
	FORCEINLINE bool AnyOf(const RangeType& Range, ProjectionType Projection)
	{
		return !Algo::NoneOf(Range, MoveTemp(Projection));
	}
}
