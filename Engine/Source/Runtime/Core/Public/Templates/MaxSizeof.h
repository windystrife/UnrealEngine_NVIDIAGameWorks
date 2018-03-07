// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"

/** Finds the maximum sizeof the supplied types */
template <typename...>
struct TMaxSizeof;

template <>
struct TMaxSizeof<>
{
	static const uint32 Value = 0;
};

template <typename T, typename... TRest>
struct TMaxSizeof<T, TRest...>
{
	static const uint32 Value = sizeof(T) > TMaxSizeof<TRest...>::Value ? sizeof(T) : TMaxSizeof<TRest...>::Value;
};