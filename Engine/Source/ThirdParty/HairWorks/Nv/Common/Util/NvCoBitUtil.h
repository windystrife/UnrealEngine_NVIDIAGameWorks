/* Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited. */

#ifndef NV_CO_BIT_UTIL_H
#define NV_CO_BIT_UTIL_H

#include <Nv/Common/NvCoCommon.h>

#if NV_VC
#	include <intrin.h>
#endif

/** \addtogroup common
@{
*/

namespace nvidia {
namespace Common {

/* A set of useful bit operations. */
struct BitUtil
{
		/// True if v is a power of 2 or 0
	static Bool isPowerTwo(UInt32 v) { return ((v - 1) & v) == 0; }
	static Bool isPowerTwo(UInt64 v) { return ((v - 1) & v) == 0; }

		/// Returns the msb (most significant bit) set in v. Result is -1 if v is 0.
	static Int calcMsb(UInt32 v);
	static Int calcMsb(UInt64 v);

		/// Return the msb (most significant bit) set in v. Implementation maybe slow - but useful for testing other implementations. 
		/// Returns -1 if v = 0;
	static Int calcNaiveMsb(UInt32 v);
	static Int calcNaiveMsb(UInt64 v);
};

NV_FORCE_INLINE /* static */Int BitUtil::calcNaiveMsb(UInt32 vIn)
{
	Int32 v(vIn);
	if (v == 0)
	{
		return -1;
	}
	Int count = 31;
	while (v >= 0)
	{
		v += v; 
		count--;
	}
	return count;
}

NV_FORCE_INLINE /* static */Int BitUtil::calcNaiveMsb(UInt64 vIn)
{
	Int64 v(vIn);
	if (v == 0)
	{
		return -1;
	}
	Int count = 63;
	while (v >= 0)
	{
		v += v;
		count--;
	}
	return count;
}

#if NV_VC
#define NV_BIT_UTIL_CALC_MSB_32
NV_FORCE_INLINE /* static */Int BitUtil::calcMsb(UInt32 v)
{
	if (v == 0) return -1;
	unsigned long index;
	_BitScanReverse(&index, v);
	return Int(index);
}
#	if NV_PTR_IS_64
#		define NV_BIT_UTIL_CALC_MSB_64
NV_FORCE_INLINE /* static */Int BitUtil::calcMsb(UInt64 v)
{
	if (v == 0) return -1;
	unsigned long index;
	_BitScanReverse64(&index, v);
	return Int(index);
}
#	endif
#endif

#ifndef NV_BIT_UTIL_CALC_MSB_32
NV_FORCE_INLINE /* static */Int BitUtil::calcMsb(UInt32 v) { return calcNaiveMsb(v); }
#endif

#ifndef NV_BIT_UTIL_CALC_MSB_64
NV_FORCE_INLINE /* static */Int BitUtil::calcMsb(UInt64 v)
{
	const UInt32 high = UInt32(v >> 32);
	const UInt32 low = UInt32(v);
	return high ? (calcMsb(high) + 32) : calcMsb(low);
}

#endif

} // namespace Common 
} // namespace nvidia

/** @} */

#endif // NV_CO_STRING_H