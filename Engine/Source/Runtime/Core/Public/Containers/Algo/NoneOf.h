// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Templates/Invoke.h"


namespace Algo
{
	/**
	 * Checks if no element in the range is truthy.
	 *
	 * @param  Range  The range to check.
	 *
	 * @return  true if no element is truthy, false otherwise.
	 */
	template <typename RangeType>
	FORCEINLINE bool NoneOf(const RangeType& Range)
	{
		for (const auto& Element : Range)
		{
			if (Element)
			{
				return false;
			}
		}

		return true;
	}

	/**
	 * Checks if no projection of the elements in the range is truthy.
	 *
	 * @param  Range       The range to check.
	 * @param  Projection  The projection to apply to each element.
	 *
	 * @return  true if none of the projections are truthy, false otherwise.
	 */
	template <typename RangeType, typename ProjectionType>
	FORCEINLINE bool NoneOf(const RangeType& Range, ProjectionType Projection)
	{
		for (const auto& Element : Range)
		{
			if (Invoke(Projection, Element))
			{
				return false;
			}
		}

		return true;
	}
}
