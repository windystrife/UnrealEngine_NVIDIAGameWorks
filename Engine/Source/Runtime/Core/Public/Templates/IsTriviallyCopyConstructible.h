// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Templates/AndOrNot.h"
#include "Templates/IsPODType.h"

/**
 * Traits class which tests if a type has a trivial copy constructor.
 */
template <typename T>
struct TIsTriviallyCopyConstructible
{
	enum { Value = TOrValue<__has_trivial_copy(T), TIsPODType<T>>::Value };
};
