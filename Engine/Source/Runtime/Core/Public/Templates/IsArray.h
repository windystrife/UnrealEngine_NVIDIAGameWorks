// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"

/**
 * Traits class which tests if a type is a C++ array.
 */
template <typename T>           struct TIsArray       { enum { Value = false }; };
template <typename T>           struct TIsArray<T[]>  { enum { Value = true  }; };
template <typename T, uint32 N> struct TIsArray<T[N]> { enum { Value = true  }; };

/**
 * Traits class which tests if a type is a bounded C++ array.
 */
template <typename T>           struct TIsBoundedArray       { enum { Value = false }; };
template <typename T, uint32 N> struct TIsBoundedArray<T[N]> { enum { Value = true  }; };

/**
 * Traits class which tests if a type is an unbounded C++ array.
 */
template <typename T> struct TIsUnboundedArray      { enum { Value = false }; };
template <typename T> struct TIsUnboundedArray<T[]> { enum { Value = true  }; };
