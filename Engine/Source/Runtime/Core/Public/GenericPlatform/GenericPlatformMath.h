// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


/*=============================================================================================
	GenericPlatformMath.h: Generic platform Math classes, mostly implemented with ANSI C++
==============================================================================================*/

#pragma once

#include "CoreTypes.h"
#include "Containers/ContainersFwd.h"
#include "HAL/PlatformCrt.h"

/**
 * Generic implementation for most platforms
 */
struct FGenericPlatformMath
{
	/**
	 * Converts a float to an integer with truncation towards zero.
	 * @param F		Floating point value to convert
	 * @return		Truncated integer.
	 */
	static CONSTEXPR FORCEINLINE int32 TruncToInt(float F)
	{
		return (int32)F;
	}

	/**
	 * Converts a float to an integer value with truncation towards zero.
	 * @param F		Floating point value to convert
	 * @return		Truncated integer value.
	 */
	static CONSTEXPR FORCEINLINE float TruncToFloat(float F)
	{
		return (float)TruncToInt(F);
	}

	/**
	 * Converts a float to a nearest less or equal integer.
	 * @param F		Floating point value to convert
	 * @return		An integer less or equal to 'F'.
	 */
	static FORCEINLINE int32 FloorToInt(float F)
	{
		return TruncToInt(floorf(F));
	}

	/**
	* Converts a float to the nearest less or equal integer.
	* @param F		Floating point value to convert
	* @return		An integer less or equal to 'F'.
	*/
	static FORCEINLINE float FloorToFloat(float F)
	{
		return floorf(F);
	}

	/**
	* Converts a double to a less or equal integer.
	* @param F		Floating point value to convert
	* @return		The nearest integer value to 'F'.
	*/
	static FORCEINLINE double FloorToDouble(double F)
	{
		return floor(F);
	}

	/**
	 * Converts a float to the nearest integer. Rounds up when the fraction is .5
	 * @param F		Floating point value to convert
	 * @return		The nearest integer to 'F'.
	 */
	static FORCEINLINE int32 RoundToInt(float F)
	{
		return FloorToInt(F + 0.5f);
	}

	/**
	* Converts a float to the nearest integer. Rounds up when the fraction is .5
	* @param F		Floating point value to convert
	* @return		The nearest integer to 'F'.
	*/
	static FORCEINLINE float RoundToFloat(float F)
	{
		return FloorToFloat(F + 0.5f);
	}

	/**
	* Converts a double to the nearest integer. Rounds up when the fraction is .5
	* @param F		Floating point value to convert
	* @return		The nearest integer to 'F'.
	*/
	static FORCEINLINE double RoundToDouble(double F)
	{
		return FloorToDouble(F + 0.5);
	}

	/**
	* Converts a float to the nearest greater or equal integer.
	* @param F		Floating point value to convert
	* @return		An integer greater or equal to 'F'.
	*/
	static FORCEINLINE int32 CeilToInt(float F)
	{
		return TruncToInt(ceilf(F));
	}

	/**
	* Converts a float to the nearest greater or equal integer.
	* @param F		Floating point value to convert
	* @return		An integer greater or equal to 'F'.
	*/
	static FORCEINLINE float CeilToFloat(float F)
	{
		return ceilf(F);
	}

	/**
	* Converts a double to the nearest greater or equal integer.
	* @param F		Floating point value to convert
	* @return		An integer greater or equal to 'F'.
	*/
	static FORCEINLINE double CeilToDouble(double F)
	{
		return ceil(F);
	}

	/**
	* Returns signed fractional part of a float.
	* @param Value	Floating point value to convert
	* @return		A float between >=0 and < 1 for nonnegative input. A float between >= -1 and < 0 for negative input.
	*/
	static FORCEINLINE float Fractional(float Value)
	{
		return Value - TruncToFloat(Value);
	}

	/**
	* Returns the fractional part of a float.
	* @param Value	Floating point value to convert
	* @return		A float between >=0 and < 1.
	*/
	static FORCEINLINE float Frac(float Value)
	{
		return Value - FloorToFloat(Value);
	}

	/**
	* Breaks the given value into an integral and a fractional part.
	* @param InValue	Floating point value to convert
	* @param OutIntPart Floating point value that receives the integral part of the number.
	* @return			The fractional part of the number.
	*/
	static FORCEINLINE float Modf(const float InValue, float* OutIntPart)
	{
		return modff(InValue, OutIntPart);
	}

	/**
	* Breaks the given value into an integral and a fractional part.
	* @param InValue	Floating point value to convert
	* @param OutIntPart Floating point value that receives the integral part of the number.
	* @return			The fractional part of the number.
	*/
	static FORCEINLINE double Modf(const double InValue, double* OutIntPart)
	{
		return modf(InValue, OutIntPart);
	}

	// Returns e^Value
	static FORCEINLINE float Exp( float Value ) { return expf(Value); }
	// Returns 2^Value
	static FORCEINLINE float Exp2( float Value ) { return powf(2.f, Value); /*exp2f(Value);*/ }
	static FORCEINLINE float Loge( float Value ) {	return logf(Value); }
	static FORCEINLINE float LogX( float Base, float Value ) { return Loge(Value) / Loge(Base); }
	// 1.0 / Loge(2) = 1.4426950f
	static FORCEINLINE float Log2( float Value ) { return Loge(Value) * 1.4426950f; }	

	/** 
	* Returns the floating-point remainder of X / Y
	* Warning: Always returns remainder toward 0, not toward the smaller multiple of Y.
	*			So for example Fmod(2.8f, 2) gives .8f as you would expect, however, Fmod(-2.8f, 2) gives -.8f, NOT 1.2f 
	* Use Floor instead when snapping positions that can be negative to a grid
	*/
	static FORCEINLINE float Fmod(float X, float Y)
	{
		if (fabsf(Y) <= 1.e-8f)
		{
			FmodReportError(X, Y);
			return 0.f;
		}
		const float Quotient = TruncToFloat(X / Y);
		float IntPortion = Y * Quotient;

		// Rounding and imprecision could cause IntPortion to exceed X and cause the result to be outside the expected range.
		// For example Fmod(55.8, 9.3) would result in a very small negative value!
		if (fabsf(IntPortion) > fabsf(X))
		{
			IntPortion = X;
		}

		const float Result = X - IntPortion;
		return Result;
	}

	static FORCEINLINE float Sin( float Value ) { return sinf(Value); }
	static FORCEINLINE float Asin( float Value ) { return asinf( (Value<-1.f) ? -1.f : ((Value<1.f) ? Value : 1.f) ); }
	static FORCEINLINE float Sinh(float Value) { return sinhf(Value); }
	static FORCEINLINE float Cos( float Value ) { return cosf(Value); }
	static FORCEINLINE float Acos( float Value ) { return acosf( (Value<-1.f) ? -1.f : ((Value<1.f) ? Value : 1.f) ); }
	static FORCEINLINE float Tan( float Value ) { return tanf(Value); }
	static FORCEINLINE float Atan( float Value ) { return atanf(Value); }
	static CORE_API float Atan2( float Y, float X );
	static FORCEINLINE float Sqrt( float Value ) { return sqrtf(Value); }
	static FORCEINLINE float Pow( float A, float B ) { return powf(A,B); }

	/** Computes a fully accurate inverse square root */
	static FORCEINLINE float InvSqrt( float F )
	{
		return 1.0f / sqrtf( F );
	}

	/** Computes a faster but less accurate inverse square root */
	static FORCEINLINE float InvSqrtEst( float F )
	{
		return InvSqrt( F );
	}

	/** Return true if value is NaN (not a number). */
	static FORCEINLINE bool IsNaN( float A ) 
	{
		return ((*(uint32*)&A) & 0x7FFFFFFF) > 0x7F800000;
	}
	/** Return true if value is finite (not NaN and not Infinity). */
	static FORCEINLINE bool IsFinite( float A )
	{
		return ((*(uint32*)&A) & 0x7F800000) != 0x7F800000;
	}
	static FORCEINLINE bool IsNegativeFloat(const float& A)
	{
		return ( (*(uint32*)&A) >= (uint32)0x80000000 ); // Detects sign bit.
	}

	static FORCEINLINE bool IsNegativeDouble(const double& A)
	{
		return ( (*(uint64*)&A) >= (uint64)0x8000000000000000 ); // Detects sign bit.
	}

	/** Returns a random integer between 0 and RAND_MAX, inclusive */
	static FORCEINLINE int32 Rand() { return rand(); }

	/** Seeds global random number functions Rand() and FRand() */
	static FORCEINLINE void RandInit(int32 Seed) { srand( Seed ); }

	/** Returns a random float between 0 and 1, inclusive. */
	static FORCEINLINE float FRand() { return Rand() / (float)RAND_MAX; }

	/** Seeds future calls to SRand() */
	static CORE_API void SRandInit( int32 Seed );

	/** Returns the current seed for SRand(). */
	static CORE_API int32 GetRandSeed();

	/** Returns a seeded random float in the range [0,1), using the seed from SRandInit(). */
	static CORE_API float SRand();

	/**
	 * Computes the base 2 logarithm for an integer value that is greater than 0.
	 * The result is rounded down to the nearest integer.
	 *
	 * @param Value		The value to compute the log of
	 * @return			Log2 of Value. 0 if Value is 0.
	 */	
	static FORCEINLINE uint32 FloorLog2(uint32 Value) 
	{
/*		// reference implementation 
		// 1500ms on test data
		uint32 Bit = 32;
		for (; Bit > 0;)
		{
			Bit--;
			if (Value & (1<<Bit))
			{
				break;
			}
		}
		return Bit;
*/
		// same output as reference

		// see http://codinggorilla.domemtech.com/?p=81 or http://en.wikipedia.org/wiki/Binary_logarithm but modified to return 0 for a input value of 0
		// 686ms on test data
		uint32 pos = 0;
		if (Value >= 1<<16) { Value >>= 16; pos += 16; }
		if (Value >= 1<< 8) { Value >>=  8; pos +=  8; }
		if (Value >= 1<< 4) { Value >>=  4; pos +=  4; }
		if (Value >= 1<< 2) { Value >>=  2; pos +=  2; }
		if (Value >= 1<< 1) {				pos +=  1; }
		return (Value == 0) ? 0 : pos;

		// even faster would be method3 but it can introduce more cache misses and it would need to store the table somewhere
		// 304ms in test data
		/*int LogTable256[256];

		void prep()
		{
			LogTable256[0] = LogTable256[1] = 0;
			for (int i = 2; i < 256; i++)
			{
				LogTable256[i] = 1 + LogTable256[i / 2];
			}
			LogTable256[0] = -1; // if you want log(0) to return -1
		}

		int _forceinline method3(uint32 v)
		{
			int r;     // r will be lg(v)
			uint32 tt; // temporaries

			if ((tt = v >> 24) != 0)
			{
				r = (24 + LogTable256[tt]);
			}
			else if ((tt = v >> 16) != 0)
			{
				r = (16 + LogTable256[tt]);
			}
			else if ((tt = v >> 8 ) != 0)
			{
				r = (8 + LogTable256[tt]);
			}
			else
			{
				r = LogTable256[v];
			}
			return r;
		}*/
	}

	/**
	 * Computes the base 2 logarithm for a 64-bit value that is greater than 0.
	 * The result is rounded down to the nearest integer.
	 *
	 * @param Value		The value to compute the log of
	 * @return			Log2 of Value. 0 if Value is 0.
	 */	
	static FORCEINLINE uint64 FloorLog2_64(uint64 Value) 
	{
		uint64 pos = 0;
		if (Value >= 1ull<<32) { Value >>= 32; pos += 32; }
		if (Value >= 1ull<<16) { Value >>= 16; pos += 16; }
		if (Value >= 1ull<< 8) { Value >>=  8; pos +=  8; }
		if (Value >= 1ull<< 4) { Value >>=  4; pos +=  4; }
		if (Value >= 1ull<< 2) { Value >>=  2; pos +=  2; }
		if (Value >= 1ull<< 1) {				pos +=  1; }
		return (Value == 0) ? 0 : pos;
	}

	/**
	 * Counts the number of leading zeros in the bit representation of the value
	 *
	 * @param Value the value to determine the number of leading zeros for
	 *
	 * @return the number of zeros before the first "on" bit
	 */
	static FORCEINLINE uint32 CountLeadingZeros(uint32 Value)
	{
		if (Value == 0) return 32;
		return 31 - FloorLog2(Value);
	}

	/**
	 * Counts the number of leading zeros in the bit representation of the 64-bit value
	 *
	 * @param Value the value to determine the number of leading zeros for
	 *
	 * @return the number of zeros before the first "on" bit
	 */
	static FORCEINLINE uint64 CountLeadingZeros64(uint64 Value)
	{
		if (Value == 0) return 64;
		return 63 - FloorLog2_64(Value);
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
		uint32 Result = 0;
		while ((Value & 1) == 0)
		{
			Value >>= 1;
			++Result;
		}
		return Result;
	}

	/**
	 * Returns smallest N such that (1<<N)>=Arg.
	 * Note: CeilLogTwo(0)=0 because (1<<0)=1 >= 0.
	 */
	static FORCEINLINE uint32 CeilLogTwo( uint32 Arg )
	{
		int32 Bitmask = ((int32)(CountLeadingZeros(Arg) << 26)) >> 31;
		return (32 - CountLeadingZeros(Arg - 1)) & (~Bitmask);
	}

	static FORCEINLINE uint64 CeilLogTwo64( uint64 Arg )
	{
		int64 Bitmask = ((int64)(CountLeadingZeros64(Arg) << 57)) >> 63;
		return (64 - CountLeadingZeros64(Arg - 1)) & (~Bitmask);
	}

	/** @return Rounds the given number up to the next highest power of two. */
	static FORCEINLINE uint32 RoundUpToPowerOfTwo(uint32 Arg)
	{
		return 1 << CeilLogTwo(Arg);
	}

	/** Spreads bits to every other. */
	static FORCEINLINE uint32 MortonCode2( uint32 x )
	{
		x &= 0x0000ffff;
		x = (x ^ (x << 8)) & 0x00ff00ff;
		x = (x ^ (x << 4)) & 0x0f0f0f0f;
		x = (x ^ (x << 2)) & 0x33333333;
		x = (x ^ (x << 1)) & 0x55555555;
		return x;
	}

	/** Reverses MortonCode2. Compacts every other bit to the right. */
	static FORCEINLINE uint32 ReverseMortonCode2( uint32 x )
	{
		x &= 0x55555555;
		x = (x ^ (x >> 1)) & 0x33333333;
		x = (x ^ (x >> 2)) & 0x0f0f0f0f;
		x = (x ^ (x >> 4)) & 0x00ff00ff;
		x = (x ^ (x >> 8)) & 0x0000ffff;
		return x;
	}

	/** Spreads bits to every 3rd. */
	static FORCEINLINE uint32 MortonCode3( uint32 x )
	{
		x &= 0x000003ff;
		x = (x ^ (x << 16)) & 0xff0000ff;
		x = (x ^ (x <<  8)) & 0x0300f00f;
		x = (x ^ (x <<  4)) & 0x030c30c3;
		x = (x ^ (x <<  2)) & 0x09249249;
		return x;
	}

	/** Reverses MortonCode3. Compacts every 3rd bit to the right. */
	static FORCEINLINE uint32 ReverseMortonCode3( uint32 x )
	{
		x &= 0x09249249;
		x = (x ^ (x >>  2)) & 0x030c30c3;
		x = (x ^ (x >>  4)) & 0x0300f00f;
		x = (x ^ (x >>  8)) & 0xff0000ff;
		x = (x ^ (x >> 16)) & 0x000003ff;
		return x;
	}

	/**
	 * Returns value based on comparand. The main purpose of this function is to avoid
	 * branching based on floating point comparison which can be avoided via compiler
	 * intrinsics.
	 *
	 * Please note that we don't define what happens in the case of NaNs as there might
	 * be platform specific differences.
	 *
	 * @param	Comparand		Comparand the results are based on
	 * @param	ValueGEZero		Return value if Comparand >= 0
	 * @param	ValueLTZero		Return value if Comparand < 0
	 *
	 * @return	ValueGEZero if Comparand >= 0, ValueLTZero otherwise
	 */
	static CONSTEXPR FORCEINLINE float FloatSelect( float Comparand, float ValueGEZero, float ValueLTZero )
	{
		return Comparand >= 0.f ? ValueGEZero : ValueLTZero;
	}

	/**
	 * Returns value based on comparand. The main purpose of this function is to avoid
	 * branching based on floating point comparison which can be avoided via compiler
	 * intrinsics.
	 *
	 * Please note that we don't define what happens in the case of NaNs as there might
	 * be platform specific differences.
	 *
	 * @param	Comparand		Comparand the results are based on
	 * @param	ValueGEZero		Return value if Comparand >= 0
	 * @param	ValueLTZero		Return value if Comparand < 0
	 *
	 * @return	ValueGEZero if Comparand >= 0, ValueLTZero otherwise
	 */
	static CONSTEXPR FORCEINLINE double FloatSelect( double Comparand, double ValueGEZero, double ValueLTZero )
	{
		return Comparand >= 0.f ? ValueGEZero : ValueLTZero;
	}

	/** Computes absolute value in a generic way */
	template< class T > 
	static CONSTEXPR FORCEINLINE T Abs( const T A )
	{
		return (A>=(T)0) ? A : -A;
	}

	/** Returns 1, 0, or -1 depending on relation of T to 0 */
	template< class T > 
	static CONSTEXPR FORCEINLINE T Sign( const T A )
	{
		return (A > (T)0) ? (T)1 : ((A < (T)0) ? (T)-1 : (T)0);
	}

	/** Returns higher value in a generic way */
	template< class T > 
	static CONSTEXPR FORCEINLINE T Max( const T A, const T B )
	{
		return (A>=B) ? A : B;
	}

	/** Returns lower value in a generic way */
	template< class T > 
	static CONSTEXPR FORCEINLINE T Min( const T A, const T B )
	{
		return (A<=B) ? A : B;
	}

	/**
	* Min of Array
	* @param	Array of templated type
	* @param	Optional pointer for returning the index of the minimum element, if multiple minimum elements the first index is returned
	* @return	The min value found in the array or default value if the array was empty
	*/
	template< class T >
	static FORCEINLINE T Min(const TArray<T>& Values, int32* MinIndex = NULL)
	{
		if (Values.Num() == 0)
		{
			if (MinIndex)
			{
				*MinIndex = INDEX_NONE;
			}
			return T();
		}

		T CurMin = Values[0];
		int32 CurMinIndex = 0;
		for (int32 v = 1; v < Values.Num(); ++v)
		{
			const T Value = Values[v];
			if (Value < CurMin)
			{
				CurMin = Value;
				CurMinIndex = v;
			}
		}

		if (MinIndex)
		{
			*MinIndex = CurMinIndex;
		}
		return CurMin;
	}

	/**
	* Max of Array
	* @param	Array of templated type
	* @param	Optional pointer for returning the index of the maximum element, if multiple maximum elements the first index is returned
	* @return	The max value found in the array or default value if the array was empty
	*/
	template< class T >
	static FORCEINLINE T Max(const TArray<T>& Values, int32* MaxIndex = NULL)
	{
		if (Values.Num() == 0)
		{
			if (MaxIndex)
			{
				*MaxIndex = INDEX_NONE;
			}
			return T();
		}

		T CurMax = Values[0];
		int32 CurMaxIndex = 0;
		for (int32 v = 1; v < Values.Num(); ++v)
		{
			const T Value = Values[v];
			if (CurMax < Value)
			{
				CurMax = Value;
				CurMaxIndex = v;
			}
		}

		if (MaxIndex)
		{
			*MaxIndex = CurMaxIndex;
		}
		return CurMax;
	}

	static FORCEINLINE int32 CountBits(uint64 Bits)
	{
		// https://en.wikipedia.org/wiki/Hamming_weight
		Bits -= (Bits >> 1) & 0x5555555555555555ull;
		Bits = (Bits & 0x3333333333333333ull) + ((Bits >> 2) & 0x3333333333333333ull);
		Bits = (Bits + (Bits >> 4)) & 0x0f0f0f0f0f0f0f0full;
		return (Bits * 0x0101010101010101) >> 56;
	}

#if WITH_DEV_AUTOMATION_TESTS
	/** Test some of the tricky functions above **/
	static void AutoTest();
#endif

private:

	/** Error reporting for Fmod. Not inlined to avoid compilation issues and avoid all the checks and error reporting at all callsites. */
	static CORE_API void FmodReportError(float X, float Y);
};

/** Float specialization */
template<>
FORCEINLINE float FGenericPlatformMath::Abs( const float A )
{
	return fabsf( A );
}

