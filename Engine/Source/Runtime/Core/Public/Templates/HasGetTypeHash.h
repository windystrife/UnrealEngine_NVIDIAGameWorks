// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Templates/AndOrNot.h"
#include "Templates/IsArithmetic.h"
#include "Templates/IsPointer.h"
#include "Templates/IsEnum.h"

namespace UE4GetTypeHashExists_Private
{
	struct FNotSpecified {};

	template <typename T>
	struct FReturnValueCheck
	{
		static char (&Func())[2];
	};

	template <>
	struct FReturnValueCheck<FNotSpecified>
	{
		static char (&Func())[1];
	};

	template <typename T>
	FNotSpecified GetTypeHash(const T&);

	template <typename T>
	const T& Make();

	template <typename T, bool bIsHashableScalarType = TOr<TIsArithmetic<T>, TIsPointer<T>, TIsEnum<T>>::Value>
	struct GetTypeHashQuery
	{
		// All arithmetic, pointer and enums types are hashable
		enum { Value = true };
	};

	template <typename T>
	struct GetTypeHashQuery<T, false>
	{
		enum { Value = sizeof(FReturnValueCheck<decltype(GetTypeHash(Make<T>()))>::Func()) == sizeof(char[2]) };
	};
}

/**
* Traits class which tests if a type has a GetTypeHash overload.
*/
template <typename T>
struct THasGetTypeHash
{
	enum { Value = UE4GetTypeHashExists_Private::GetTypeHashQuery<T>::Value };
};
