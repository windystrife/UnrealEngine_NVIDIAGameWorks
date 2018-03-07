// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*================================================================================
	HTML5Platform.h: Setup for the HTML5 platform
==================================================================================*/

#pragma once




/**
 * HTML5 specific types
 **/
struct FHTML5Types : public FGenericPlatformTypes
{
	typedef unsigned long SIZE_T;
	typedef long SSIZE_T;
	// Emscripten uses the musl libc implementation, where NULL is defined as '0L', which is of type long.
	typedef long					TYPE_OF_NULL;
};
typedef FHTML5Types FPlatformTypes;

// Platform defines, overrides from the default (except first 3)
#define PLATFORM_DESKTOP							0
#define PLATFORM_64BITS								0
#define PLATFORM_LITTLE_ENDIAN						1
#define PLATFORM_SUPPORTS_PRAGMA_PACK				1
#define PLATFORM_USE_LS_SPEC_FOR_WIDECHAR			1
#define PLATFORM_HAS_BSD_TIME						1
#define PLATFORM_MAX_FILEPATH_LENGTH				PATH_MAX
#define PLATFORM_TCHAR_IS_4_BYTES					1
#define PLATFORM_HAS_BSD_SOCKETS					1
#define PLATFORM_HAS_BSD_SOCKET_FEATURE_MSG_DONTWAIT	1
#define PLATFORM_HAS_NO_EPROCLIM					1
#define PLATFORM_USE_PTHREADS						0
#define PLATFORM_SUPPORTS_TEXTURE_STREAMING			1
#define PLATFORM_REQUIRES_FILESERVER				1
#define PLATFORM_SUPPORTS_TBB						0
#define PLATFORM_ENABLE_VECTORINTRINSICS			0
#define PLATFORM_USES_ES2							1
#define PLATFORM_BUILTIN_VERTEX_HALF_FLOAT			0
#define PLATFORM_SUPPORTS_STACK_SYMBOLS				1

// Function type macros.
#define FORCEINLINE		inline __attribute__((__always_inline__))					/* Force code to be inline */
#define FORCENOINLINE	__attribute__((noinline))									/* Force code to NOT be inline */
#define FUNCTION_CHECK_RETURN_END __attribute__ ((warn_unused_result))	/* Warn that callers should not ignore the return value. */
#define FUNCTION_NO_RETURN_END __attribute__ ((noreturn))				/* Indicate that the function never returns. */

// Optimization macros
#define PRAGMA_DISABLE_OPTIMIZATION_ACTUAL _Pragma("clang optimize off")
#define PRAGMA_ENABLE_OPTIMIZATION_ACTUAL  _Pragma("clang optimize on")

#define ABSTRACT			abstract

// Alignment.
#define GCC_PACK(n)			__attribute__((packed,aligned(n)))
#define GCC_ALIGN(n)		__attribute__((aligned(n)))
#define REQUIRES_ALIGNED_ACCESS 1

// Operator new/delete handling.
// operator new/delete operators
// As of 10.9 we need to use _NOEXCEPT & cxx_noexcept compatible definitions
#if __has_feature(cxx_noexcept)
#define OPERATOR_NEW_THROW_SPEC
#else
#define OPERATOR_NEW_THROW_SPEC throw (std::bad_alloc)
#endif
#define OPERATOR_DELETE_THROW_SPEC noexcept
#define OPERATOR_NEW_NOTHROW_SPEC  noexcept
#define OPERATOR_DELETE_NOTHROW_SPEC  noexcept


#define MAXUINT8    ((uint8)~((uint8)0))
#define MAX_PATH 1024
