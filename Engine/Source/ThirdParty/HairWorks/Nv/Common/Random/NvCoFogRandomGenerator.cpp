/* Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited. */

#include "NvCoFogRandomGenerator.h"

namespace nvidia {
namespace Common {

template <SizeT R>
NV_FORCE_INLINE static UInt32 rotl(UInt32 x) { return (x << R) | (x >> (32 - R)); } 

FogRandomGenerator::FogRandomGenerator(Int32 seed)
{
	reset(seed);
}

void FogRandomGenerator::reset(Int32 seedIn) 
{
	UInt32 s = UInt32(seedIn);
	for (Int i = 0; i < KK; i++)
	{
		s = s * UInt32(2891336453u) + 1;
		m_buf[i] = s;
	}
	m_p1 = 0;
	m_p2 = JJ;

	// Randomize a bit more
	Int32 work[9];
	nextInt32s(work, NV_COUNT_OF(work));
}

Int32 FogRandomGenerator::nextInt32()
{
	Int p1 = m_p1, p2 = m_p2;
	const Int wrapPos = KK - 1;
	const UInt32 x = rotl<R1>(m_buf[p2]) + rotl<R2>(m_buf[p1]);
	m_buf[p1] = x;
	--p1;
	--p2;
	p1 = (p1 < 0) ? wrapPos : p1;
	p2 = (p2 < 0) ? wrapPos : p2;
	m_p1 = p1;
	m_p2 = p2;

	return x;
}

Void FogRandomGenerator::nextInt32s(Int32* out, IndexT size)
{
	Int p1 = m_p1, p2 = m_p2;
	const Int wrapPos = KK - 1;

	for (IndexT i = 0; i < size; i++)
	{
		const UInt32 x = rotl<R1>(m_buf[p2]) + rotl<R2>(m_buf[p1]);
		m_buf[p1] = x;
		--p1;
		--p2;
		p1 = (p1 < 0) ? wrapPos : p1;
		p2 = (p2 < 0) ? wrapPos : p2;

		out[i] = Int32(x);
	}
	m_p1 = p1;
	m_p2 = p2;
}

} // namespace Common 
} // namespace nvidia

