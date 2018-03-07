// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================================
	LinuxPlatformMath.h: Linux platform Math functions
==============================================================================================*/

#pragma once

#include "CoreTypes.h"
#include "GenericPlatform/GenericPlatformMath.h"
#include "Linux/LinuxSystemIncludes.h"
#if PLATFORM_ENABLE_VECTORINTRINSICS
	#include "Math/UnrealPlatformMathSSE.h"
#endif // PLATFORM_ENABLE_VECTORINTRINSICS

/**
* Linux implementation of the Math OS functions
**/
struct FLinuxPlatformMath : public FGenericPlatformMath
{
#if PLATFORM_ENABLE_VECTORINTRINSICS
	static FORCEINLINE int32 TruncToInt(float F)
	{
		return _mm_cvtt_ss2si(_mm_set_ss(F));
	}

	static FORCEINLINE float TruncToFloat(float F)
	{
		return (float)TruncToInt(F); // same as generic implementation, but this will call the faster trunc
	}

	static FORCEINLINE int32 RoundToInt(float F)
	{
		// Note: the x2 is to workaround the rounding-to-nearest-even-number issue when the fraction is .5
		return _mm_cvt_ss2si(_mm_set_ss(F + F + 0.5f)) >> 1;
	}

	static FORCEINLINE float RoundToFloat(float F)
	{
		return (float)RoundToInt(F);
	}

	static FORCEINLINE int32 FloorToInt(float F)
	{
		return _mm_cvt_ss2si(_mm_set_ss(F + F - 0.5f)) >> 1;
	}

	static FORCEINLINE float FloorToFloat(float F)
	{
		return (float)FloorToInt(F);
	}

	static FORCEINLINE int32 CeilToInt(float F)
	{
		// Note: the x2 is to workaround the rounding-to-nearest-even-number issue when the fraction is .5
		return -(_mm_cvt_ss2si(_mm_set_ss(-0.5f - (F + F))) >> 1);
	}

	static FORCEINLINE float CeilToFloat(float F)
	{
		// Note: the x2 is to workaround the rounding-to-nearest-even-number issue when the fraction is .5
		return (float)CeilToInt(F);
	}

	static FORCEINLINE bool IsNaN( float A ) { return isnan(A) != 0; }
	static FORCEINLINE bool IsFinite( float A ) { return isfinite(A); }

	static FORCEINLINE float InvSqrt( float F )
	{
		return UnrealPlatformMathSSE::InvSqrt(F);
	}

	static FORCEINLINE float InvSqrtEst( float F )
	{
		return UnrealPlatformMathSSE::InvSqrtEst(F);
	}

#if PLATFORM_ENABLE_POPCNT_INTRINSIC
	/**
	 * Use the SSE instruction to count bits
	 */
	static FORCEINLINE int32 CountBits(uint64 Bits)
	{
		return __builtin_popcountll(Bits);
	}
#endif
#endif

	/**
	 * Counts the number of leading zeros in the bit representation of the value
	 *
	 * @param Value the value to determine the number of leading zeros for
	 *
	 * @return the number of zeros before the first "on" bit
	 */
	static FORCEINLINE uint32 CountLeadingZeros(uint32 Value)
	{
		if (Value == 0)
		{
			return 32;
		}

		return __builtin_clz(Value);
	}

	/**
	 * Counts the number of trailing zeros in the bit representation of the value
	 *
	 * @param Value the value to determine the number of trailing zeros for
	 *
	 * @return the number of zeros after the last "on" bit
	 */
	static FORCEINLINE uint32 CountTrailingZeros(uint32 Value)
	{
		if (Value == 0)
		{
			return 32;
		}

		return __builtin_ctz(Value);
	}
};

typedef FLinuxPlatformMath FPlatformMath;

