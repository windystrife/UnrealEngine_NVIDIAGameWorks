// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"

/**
 * Traits class which tests if a type is integral.
 */
template <typename T>
struct TIsIntegral
{
	enum { Value = false };
};

template <> struct TIsIntegral<uint8>    { enum { Value = true }; };
template <> struct TIsIntegral<uint16>   { enum { Value = true }; };
template <> struct TIsIntegral<uint32>   { enum { Value = true }; };
template <> struct TIsIntegral<uint64>   { enum { Value = true }; };
template <> struct TIsIntegral<int8>     { enum { Value = true }; };
template <> struct TIsIntegral<int16>    { enum { Value = true }; };
template <> struct TIsIntegral<int32>    { enum { Value = true }; };
template <> struct TIsIntegral<int64>    { enum { Value = true }; };
template <> struct TIsIntegral<bool>     { enum { Value = true }; };
template <> struct TIsIntegral<WIDECHAR> { enum { Value = true }; };
template <> struct TIsIntegral<ANSICHAR> { enum { Value = true }; };

template <typename T> struct TIsIntegral<const          T> { enum { Value = TIsIntegral<T>::Value }; };
template <typename T> struct TIsIntegral<      volatile T> { enum { Value = TIsIntegral<T>::Value }; };
template <typename T> struct TIsIntegral<const volatile T> { enum { Value = TIsIntegral<T>::Value }; };
