// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Templates/RemoveCV.h"
#include "Templates/RemoveReference.h"

namespace UE4Decay_Private
{
	template <typename T>
	struct TDecayNonReference
	{
		typedef typename TRemoveCV<T>::Type Type;
	};

	template <typename T>
	struct TDecayNonReference<T[]>
	{
		typedef T* Type;
	};

	template <typename T, uint32 N>
	struct TDecayNonReference<T[N]>
	{
		typedef T* Type;
	};

	template <typename RetType, typename... Params>
	struct TDecayNonReference<RetType(Params...)>
	{
		typedef RetType (*Type)(Params...);
	};
}

/**
 * Returns the decayed type of T, meaning it removes all references, qualifiers and
 * applies array-to-pointer and function-to-pointer conversions.
 *
 * http://en.cppreference.com/w/cpp/types/decay
 */
template <typename T>
struct TDecay
{
	typedef typename UE4Decay_Private::TDecayNonReference<typename TRemoveReference<T>::Type>::Type Type;
};
