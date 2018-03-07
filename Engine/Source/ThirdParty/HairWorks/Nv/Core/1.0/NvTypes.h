/* Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited. */

#ifndef NV_CORE_TYPES_H
#define NV_CORE_TYPES_H

#include "NvDefines.h"

#include <stdint.h> // cstdint is in tr1 folder on current XCode

/** \addtogroup core
@{
*/

// These types exist such that in the Nv namespace we can follow the convention 
// that type names are upper camel, even with built in types. 
// This also gives flexibility - to change representations as necessary

//!@{ 
//! C style basic type typedefs. 

#ifdef __cplusplus
/// bool underlying type is only available in C++
/// NOTE! Uses longer named NvBoolean over the preferable NvBool so it doesn't clash with NvApi - which defines NvBool and NvU8.
typedef bool NvBoolean;
#endif

typedef uint8_t NvBool8;		//< C doesn't have bool. Use uint8 to back, and document the type is to be interpreted as a bool.

typedef char NvChar;			///< Char is 8 bit, and text is utf8 encoded. Normally signed. 

typedef uint32_t NvWideChar;	///< Wide char holds a single full unicode character

typedef	uint8_t NvUInt8;
typedef int8_t NvInt8;
typedef	uint16_t NvUInt16;
typedef int16_t NvInt16;
typedef	uint32_t NvUInt32;
typedef int32_t NvInt32;
typedef	uint64_t NvUInt64;
typedef int64_t NvInt64;
typedef	float NvFloat32;
typedef double NvFloat64;

typedef int NvInt;				///< Non sized integer. Must be 32 bits or larger, never larger than memory sized types.
typedef unsigned int NvUInt;	///< Non sized unsigned integer. Must be 32 bits or larger, never larger than memory sized types.
typedef NvFloat32 NvFloat;		///< 'Non sized' float, typically backed by NvFloat32. Must be 32bits or larger.

typedef void NvVoid;			///< Void type for consistency with other types

typedef size_t NvSizeT;			///< Unsigned memory size
typedef ptrdiff_t NvPtrDiffT;	///< Signed memory offset. sizeof(SizeT) == sizeof(NvPtrDiffT)

/*! Type used for indexing. 
This uses a signed type to avoid problems with numerical under/overflow
It is not necessarily the same size as a pointer. This means it might NOT 
be able to address the full address space of bytes. */
typedef NvPtrDiffT NvIndexT;

//!@}

#define NV_CHECK_SIGNED(type, size) NV_COMPILE_TIME_ASSERT(sizeof(type) == size && ((type)~(type)0) < (type)0)
#define NV_CHECK_UNSIGNED(type, size) NV_COMPILE_TIME_ASSERT(sizeof(type) == size && ((type)~(type)0) > (type)0)

NV_CHECK_SIGNED(NvChar, 1);

NV_CHECK_SIGNED(NvInt8, 1);
NV_CHECK_SIGNED(NvInt16, 2);
NV_CHECK_SIGNED(NvInt32, 4);
NV_CHECK_SIGNED(NvInt64, 8);
NV_CHECK_UNSIGNED(NvUInt8, 1);
NV_CHECK_UNSIGNED(NvUInt16, 2);
NV_CHECK_UNSIGNED(NvUInt32, 4);
NV_CHECK_UNSIGNED(NvUInt64, 8);

NV_COMPILE_TIME_ASSERT(sizeof(NvFloat32) == 4);
NV_COMPILE_TIME_ASSERT(sizeof(NvFloat64) == 8);

// Type ranges
#define	NV_MAX_I8			127					//< maximum possible sbyte value, 0x7f
#define	NV_MIN_I8			(-128)				///< minimum possible sbyte value, 0x80
#define	NV_MAX_U8			255U				///< maximum possible ubyte value, 0xff
#define	NV_MIN_U8			0					///< minimum possible ubyte value, 0x00
#define	NV_MAX_I16			32767				///< maximum possible sword value, 0x7fff
#define	NV_MIN_I16			(-32768)			///< minimum possible sword value, 0x8000
#define	NV_MAX_U16			65535U				///< maximum possible uword value, 0xffff
#define	NV_MIN_U16			0					///< minimum possible uword value, 0x0000
#define	NV_MAX_I32			2147483647			///< maximum possible sdword value, 0x7fffffff
#define	NV_MIN_I32			(-2147483647 - 1)	///< minimum possible sdword value, 0x80000000
#define	NV_MAX_U32			4294967295U			///< maximum possible udword value, 0xffffffff
#define	NV_MIN_U32			0					///< minimum possible udword value, 0x00000000
#define	NV_MAX_F32			3.4028234663852885981170418348452e+38F	//< maximum possible float value
#define	NV_MAX_F64			DBL_MAX				//< maximum possible double value

#define NV_EPS_F32			FLT_EPSILON			//< maximum relative error of float rounding
#define NV_EPS_F64			DBL_EPSILON			//< maximum relative error of double rounding

#ifdef __cplusplus

// Define basic types available in nvidia namespace
namespace nvidia {

/// These types exist such that in the nvidia namespace we can follow the convention 
/// that type names are upper camel, even with built in types. 
/// This also gives flexibility to change representations as necessary

typedef ::NvBoolean Boolean;		
typedef ::NvBoolean Bool;

// Just alias to underlying C types
typedef ::NvBool8 Bool8;
typedef ::NvChar Char;
typedef ::NvWideChar WideChar;
typedef	::NvUInt8 UInt8;
typedef ::NvInt8 Int8;
typedef	::NvUInt16 UInt16;
typedef ::NvInt16 Int16;
typedef	::NvUInt32 UInt32;
typedef ::NvInt32 Int32;
typedef	::NvUInt64 UInt64;
typedef ::NvInt64 Int64;
typedef	::NvFloat32 Float32;
typedef ::NvFloat64 Float64;

// Non sized types
typedef ::NvInt Int;
typedef ::NvUInt UInt;
typedef ::NvFloat Float;

// Misc
typedef ::NvVoid Void;

// Memory/pointer types
typedef ::NvSizeT SizeT;
typedef ::NvPtrDiffT PtrDiffT;
typedef ::NvIndexT IndexT;

} // namespace nvidia
#endif // __cplusplus

/** @} */
#endif // #ifndef NV_CORE_TYPES_H
