// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"

template <typename T, T... Indices>
struct TIntegerSequence
{
};

// Doxygen can't parse recursive template definitions; just skip it.
#if !UE_BUILD_DOCS

template <typename T, bool bTerminateRecursion, T ToAdd, T... Values>
struct TMakeIntegerSequenceImpl
{
	typedef typename TMakeIntegerSequenceImpl<T, (ToAdd - 1 == (T)0), ToAdd - 1, Values..., sizeof...(Values)>::Type Type;
};

template <typename T, T ToAdd, T... Values>
struct TMakeIntegerSequenceImpl<T, true, ToAdd, Values...>
{
	typedef TIntegerSequence<T, Values...> Type;
};

#endif

template <typename T, T ToAdd>
using TMakeIntegerSequence = typename TMakeIntegerSequenceImpl<T, (ToAdd == (T)0), ToAdd>::Type;
