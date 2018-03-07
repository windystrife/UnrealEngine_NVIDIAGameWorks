// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Templates/AndOrNot.h"

namespace UE4IsEnumClass_Private
{
	template <typename T>
	struct TIsEnumConvertibleToInt
	{
		static char (&Resolve(int))[2];
		static char Resolve(...);

		enum { Value = sizeof(Resolve(T())) - 1 };
	};
}

/**
 * Traits class which tests if a type is arithmetic.
 */
template <typename T>
struct TIsEnumClass
{ 
	enum { Value = TAndValue<__is_enum(T), TNot<UE4IsEnumClass_Private::TIsEnumConvertibleToInt<T>>>::Value };
};
