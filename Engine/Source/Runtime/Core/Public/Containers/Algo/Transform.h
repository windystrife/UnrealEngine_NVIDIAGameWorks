// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"

namespace Algo
{
	/**
	 * Conditionally applies a transform to a range and stores the results into a container
	 *
	 * @param  Input      Any iterable type
	 * @param  Output     Container to hold the output
	 * @param  Predicate  Condition which returns true for elements that should be transformed and false for elements that should be skipped
	 * @param  Trans      Transformation operation
	 */
	template <typename InT, typename OutT, typename PredicateT, typename TransformT>
	FORCEINLINE void TransformIf(const InT& Input, OutT& Output, PredicateT Predicate, TransformT Trans)
	{
		for (const auto& Value : Input)
		{
			if (Invoke(Predicate, Value))
			{
				Output.Add(Invoke(Trans, Value));
			}
		}
	}

	/**
	 * Applies a transform to a range and stores the results into a container
	 *
	 * @param  Input   Any iterable type
	 * @param  Output  Container to hold the output
	 * @param  Trans   Transformation operation
	 */
	template <typename InT, typename OutT, typename TransformT>
	FORCEINLINE void Transform(const InT& Input, OutT& Output, TransformT Trans)
	{
		for (const auto& Value : Input)
		{
			Output.Add(Invoke(Trans, Value));
		}
	}
}
