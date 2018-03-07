// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"

/**
 * Aligns a value to the nearest higher multiple of 'Alignment', which must be a power of two.
 *
 * @param Ptr			Value to align
 * @param Alignment		Alignment, must be a power of two
 * @return				Aligned value
 */
template <typename T>
inline CONSTEXPR T Align( const T Ptr, int32 Alignment )
{
	return (T)(((int64)Ptr + Alignment - 1) & ~(Alignment-1));
}

/**
 * Aligns a value to the nearest lower multiple of 'Alignment', which must be a power of two.
 *
 * @param Ptr			Value to align
 * @param Alignment		Alignment, must be a power of two
 * @return				Aligned value
 */
template <typename T>
inline CONSTEXPR T AlignDown( const T Ptr, int32 Alignment )
{
	return (T)(((int64)Ptr) & ~(Alignment-1));
}

/**
 * Checks if a pointer is aligned to the specified alignment.
 *
 * @param Ptr - The pointer to check.
 *
 * @return true if the pointer is aligned, false otherwise.
 */
static FORCEINLINE bool IsAligned(const volatile void* Ptr, const uint32 Alignment)
{
	return !(UPTRINT(Ptr) & (Alignment - 1));
}

/**
 * Aligns a value to the nearest higher multiple of 'Alignment'.
 *
 * @param Ptr			Value to align
 * @param Alignment		Alignment, can be any arbitrary uint32
 * @return				Aligned value
 */
template< class T > inline T AlignArbitrary( const T Ptr, uint32 Alignment )
{
	return (T) ( ( ((uint64)Ptr + Alignment - 1) / Alignment ) * Alignment );
}

