// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * A functor which returns whatever is passed to it.  Mainly used for generic composition.
 */
struct FIdentityFunctor
{
	template <typename T>
	FORCEINLINE T&& operator()(T&& Val) const
	{
		return (T&&)Val;
	}
};
