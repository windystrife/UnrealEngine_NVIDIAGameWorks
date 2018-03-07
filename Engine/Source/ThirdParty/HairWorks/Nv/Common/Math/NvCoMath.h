/* Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited. */
#ifndef NV_CO_MATH_H
#define NV_CO_MATH_H

#include <Nv/Core/1.0/NvDefines.h>

// Needed for sqrtf
#include "math.h"
//#include "float.h"

namespace nvidia {
namespace Common {

/* Normally these methods are accessed via qualification as in 
float x = Math::sqrt(y);

This design is desirable as it makes clear in code which implementation of the function is being used. If they were free functions
when sqrt is called - it's not clear if it means the standard libraries version or not. For similar reasons it is implemented in a
class such that it is not possible to make accessible via using namespace. This means that usage must always be qualified. 

Note that in a client owned namespace, a recommended way to access the functions is by doing a typedef such that full qualification is not needed.
As in

#include <Nv/Common/Math/NvCoMath.h>

namespace Something { 

typedef NvCo::Math Math;

float doSomething(float a) { return Math::abs(a); }

} // namespace Something

NOTE: This mechanism is different from the legacy vector/mat library NvCoGfsdkMath.h which is in a namespace. The qualification is not 
needed there though - because the types specified are not built-in types - so there is much less ambiguity.

NOTE: Functions 'calcMin', 'calcMax' are used instead of 'min','max' so as not to clash with windows min and max macros (!)

NOTE: This set of functions must remain a header only library, as it may be included in other headers as only library interfaces.
*/

struct Math 
{
	NV_FORCE_INLINE static float sqrt(float v) { return ::sqrtf(v); }

	NV_FORCE_INLINE static float abs(float v) { return ::fabsf(v); }

	NV_FORCE_INLINE static float lerp(float v1, float v2, float t) { return (1.0f - t) * v1 + t * v2; }

	NV_FORCE_INLINE static float calcMin(float x, float y)  { return (x < y) ? x : y; }
	NV_FORCE_INLINE static float calcMax(float x, float y) { return (x > y) ? x : y; }

	NV_FORCE_INLINE static UInt32 calcMin(UInt32 x, UInt32 y)  { return (x < y) ? x : y; }
	NV_FORCE_INLINE static UInt32 calcMax(UInt32 x, UInt32 y) { return (x > y) ? x : y; }

	NV_FORCE_INLINE static float saturate(float x) { return (x < 0.0f) ? 0.0f : ((x > 1.0f) ? 1.0f : x); }
	
	NV_FORCE_INLINE static float clamp(float x, float a, float b) { return (x < a) ? a : ((x > b) ? b : x); }

	NV_FORCE_INLINE static float sin(float a) { return ::sinf(a); }
	NV_FORCE_INLINE static float cos(float a) { return ::cosf(a); }

	NV_FORCE_INLINE static float recip(float a) {	return 1.0f / a; }
	NV_FORCE_INLINE static float recipFast(float a) { return 1.0f / a;	}

	NV_FORCE_INLINE static float recipSqrt(float a) { return 1.0f / ::sqrtf(a); }
	NV_FORCE_INLINE static float recipSqrtFast(float a) { return 1.0f / ::sqrtf(a); }
	
	NV_FORCE_INLINE static float sign(float a) { return (a >= 0.0f) ? 1.0f : -1.0f; }

	NV_FORCE_INLINE static float fsel(float a, float b, float c) { return (a >= 0.0f) ? b : c;	}
};

} // namespace Common 
} // namespace nvidia 

#endif 
