/* Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited. */

#ifndef NV_CO_FOG_RANDOM_GENERATOR_H
#define NV_CO_FOG_RANDOM_GENERATOR_H

#include "NvCoRandomGenerator.h"

/** \addtogroup common
@{
*/

namespace nvidia {
namespace Common {


/* Random Generator implementation.
RANROT type B by Agner Fog

http://agner.org/random/discuss/read.php?i=138
*/
class FogRandomGenerator: public RandomGenerator
{
	NV_CO_DECLARE_POLYMORPHIC_CLASS(FogRandomGenerator, RandomGenerator)

	virtual void reset(Int32 seed) NV_OVERRIDE;
	virtual Int32 nextInt32() NV_OVERRIDE;
	virtual Void nextInt32s(Int32* out, IndexT size) NV_OVERRIDE;
	
		/// Ctor
	FogRandomGenerator(Int32 seed = 223442);

	protected:
	enum 
	{
		KK = 17,
		JJ = 10,
		R1 = 13,
		R2 = 9,
	};

	UInt32 m_buf[KK];		///< History buffer
	Int m_p1, m_p2;			///< Indices into the history buffer
};

} // namespace Common 
} // namespace nvidia

/** @} */

#endif // NV_CO_MEMORY_ALLOCATOR_H