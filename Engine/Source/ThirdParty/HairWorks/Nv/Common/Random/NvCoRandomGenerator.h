/* Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited. */

#ifndef NV_CO_RANDOM_GENERATOR_H
#define NV_CO_RANDOM_GENERATOR_H

#include <Nv/Common/NvCoCommon.h>
#include <Nv/Common/NvCoTypeMacros.h>

/** \addtogroup common
@{
*/

namespace nvidia {
namespace Common {


/* Random Generator interface */
class RandomGenerator
{
	NV_CO_DECLARE_POLYMORPHIC_CLASS_BASE(RandomGenerator)

	virtual ~RandomGenerator() {}

		/*! Reset the state of the generator with the seed. The seed value should always initial the generator to the same initial state.
		@param seed The seeding value */
   	virtual void reset(Int32 seed) = 0;
		/*! Generate the next Int32 */
	virtual Int32 nextInt32() = 0;
		/*! Generate an array of int32s 
		@param out The array to write the numbers to
		@param size The amount of Int32s to produce */
	virtual Void nextInt32s(Int32* out, IndexT size) = 0;
		/*! The next float in range [0 - 1) */
	virtual Float nextFloat();
		/*! The next float in rand (-1 - 1) ie |v| < 1 */
	virtual Float nextFloatModOne();

		/*! Get an array of random floats [0 - 1)
		@param out The array to write the floats to
		@param size The amount of floats wanted */
	virtual Void nextFloats(Float* out, IndexT size);
		/*! Get an array of random floats (-1 to - 1)
		@param out The array to write the floats to
		@param size The amount of floats wanted */
	virtual Void nextFloatsModOne(Float* out, IndexT size);

		/* The calculate the next integer in the range min to max (not including max) 
		@param min minimum value
		@param max maximum value */
	virtual Int32 nextInt32InRange(Int32 min, Int32 max);

		/// Create a new random generator
	static RandomGenerator* create(Int32 seed);

		/// Get the instance
	NV_FORCE_INLINE static RandomGenerator* getInstance() { return s_instance;  }
		/// Set the instance
	NV_FORCE_INLINE static void setInstance(RandomGenerator* alloc) { s_instance = alloc;  }

private:
	static RandomGenerator* s_instance;
};

} // namespace Common 
} // namespace nvidia

/** @} */

#endif // NV_CO_MEMORY_ALLOCATOR_H