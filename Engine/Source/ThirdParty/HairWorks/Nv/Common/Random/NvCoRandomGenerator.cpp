/* Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited. */

#include "NvCoRandomGenerator.h"

#include "NvCoFogRandomGenerator.h"
#include <Nv/Common/Util/NvCoBitUtil.h>

namespace nvidia {
namespace Common {

/* static */RandomGenerator* RandomGenerator::s_instance = NV_NULL;

/* static */RandomGenerator* RandomGenerator::create(Int32 seed)
{
	return new FogRandomGenerator(seed);
}

Float RandomGenerator::nextFloat()
{
	return (nextInt32() & 0x3fffffff) * (1.0f / 0x40000000);
}

Float RandomGenerator::nextFloatModOne()
{
	const Int32 n = nextInt32();
	Float v = (n & 0x3fffffff) * (1.0f / 0x40000000);
	return (n < 0) ? -v : v;
}

Int32 RandomGenerator::nextInt32InRange(Int32 min, Int32 max)
{
	const Int range = max - min;
	NV_CORE_ASSERT(range >= 0);
	if (range <= 1)
	{
		return min;
	}
	Int32 v = nextInt32();

	// If the range is a power of 2, we avoid the divide and get true uniform answer (assuming underlying generator is)
	if (BitUtil::isPowerTwo(UInt32(range)))
	{
		return v & Int32(range - 1) + min;
	}

	return ((v & 0x7fffffff) % range) + min;
}

Void RandomGenerator::nextFloats(Float* out, IndexT size)
{
	NV_COMPILE_TIME_ASSERT(sizeof(Float) == sizeof(Int32));

	// We'll use the destination as workspace
	Int32* work = (Int32*)out;
	nextInt32s(work, size);

	for (IndexT i = 0; i < size; i++)
	{
		out[i] = (work[i] & 0x3fffffff) * (1.0f / 0x40000000);
	}
}

Void RandomGenerator::nextFloatsModOne(Float* out, IndexT size)
{
	NV_COMPILE_TIME_ASSERT(sizeof(Float) == sizeof(Int32));

	// We'll use the destination as workspace
	Int32* work = (Int32*)out;
	nextInt32s(work, size);

	for (IndexT i = 0; i < size; i++)
	{
		const Int32 n = work[i];
		Float v = (n & 0x3fffffff) * (1.0f / 0x40000000);
		out[i] = (n < 0) ? -v : v;
	}
}

} // namespace Common 
} // namespace nvidia

