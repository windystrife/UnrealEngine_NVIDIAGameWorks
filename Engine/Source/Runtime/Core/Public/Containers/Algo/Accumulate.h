// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"

// TPlus<T> specifically takes const T& and returns T.
// TPlus<> (empty angle brackets) is late-binding, taking whatever is passed and returning the correct result type for (A+B)
template<typename T = void>
struct TPlus
{
	FORCEINLINE T operator()(const T& A, const T& B) { return A + B; }
};
template<>
struct TPlus<void>
{
	template<typename U, typename V>
	FORCEINLINE auto operator()(U&& A, V&& B) -> decltype(A + B) { return A + B; }
};


namespace Algo
{
	/**
	* Sums a range by successively applying Op.
	*
	* @param  Input  Any iterable type
	* @param  Init  Initial value for the summation
	* @param  Op  Summing Operation (the default is TPlus<>)
	*
	* @return the result of summing all the elements of Input
	*/
	template <typename T, typename A, typename OpT>
	FORCEINLINE T Accumulate(const A& Input, T Init, OpT Op)
	{
		T Result = MoveTemp(Init);
		for (const auto& InputElem : Input)
		{
			Result = Op(MoveTemp(Result), InputElem);
		}
		return Result;
	}

	/**
	 * Sums a range.
	 *
	 * @param  Input  Any iterable type
	 * @param  Init  Initial value for the summation
	 *
	 * @return the result of summing all the elements of Input
	 */
	template <typename T, typename A>
	FORCEINLINE T Accumulate(const A& Input, T Init)
	{
		return Accumulate(Input, MoveTemp(Init), TPlus<>());
	}

	/**
	* Sums a range by applying MapOp to each element, and then summing the results.
	*
	* @param  Input  Any iterable type
	* @param  Init  Initial value for the summation
	* @param  MapOp  Mapping Operation
	* @param  Op  Summing Operation (the default is TPlus<>)
	*
	* @return the result of mapping and then summing all the elements of Input
	*/
	template <typename T, typename A, typename MapT, typename OpT>
	FORCEINLINE T TransformAccumulate(const A& Input, MapT MapOp, T Init, OpT Op)
	{
		T Result = MoveTemp(Init);
		for (const auto& InputElem : Input)
		{
			Result = Op(MoveTemp(Result), MapOp(InputElem));
		}
		return Result;
	}

	/**
	* Sums a range by applying MapOp to each element, and then summing the results.
	*
	* @param  Input  Any iterable type
	* @param  Init  Initial value for the summation
	* @param  MapOp  Mapping Operation
	*
	* @return the result of mapping and then summing all the elements of Input
	*/
	template <typename T, typename A, typename MapT>
	FORCEINLINE T TransformAccumulate(const A& Input, MapT MapOp, T Init)
	{
		return TransformAccumulate(Input, MapOp, MoveTemp(Init), TPlus<>());
	}
}
