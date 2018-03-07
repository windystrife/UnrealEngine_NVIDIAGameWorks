// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*================================================================================
	IOSPlatform.h: Setup for the iOS platform
==================================================================================*/

#pragma once

/**
* iOS specific types
**/
struct FIOSPlatformTypes : public FGenericPlatformTypes
{
	typedef size_t				SIZE_T;
	typedef decltype(NULL)		TYPE_OF_NULL;
	typedef char16_t			CHAR16;
};

typedef FIOSPlatformTypes FPlatformTypes;

// Base defines, must define these for the platform, there are no defaults
#define PLATFORM_DESKTOP				0

#ifdef __LP64__
#define PLATFORM_64BITS					1
// Technically the underlying platform has 128bit atomics, but clang might not issue optimal code
#define PLATFORM_HAS_128BIT_ATOMICS		0
#else
#define PLATFORM_64BITS					0
#define PLATFORM_HAS_128BIT_ATOMICS		0
#endif

// Base defines, defaults are commented out
#define PLATFORM_LITTLE_ENDIAN							1
#define PLATFORM_SUPPORTS_PRAGMA_PACK					1
#define PLATFORM_COMPILER_DISTINGUISHES_INT_AND_LONG	1
#define PLATFORM_TCHAR_IS_4_BYTES						1
#define PLATFORM_USE_SYSTEM_VSWPRINTF					0
#define PLATFORM_HAS_BSD_TIME							1
#define PLATFORM_HAS_BSD_IPV6_SOCKETS					1
#define PLATFORM_HAS_BSD_SOCKET_FEATURE_MSG_DONTWAIT	1
#define PLATFORM_MAX_FILEPATH_LENGTH					MAX_PATH
#define PLATFORM_SUPPORTS_TEXTURE_STREAMING				1
#define PLATFORM_BUILTIN_VERTEX_HALF_FLOAT				0
#define PLATFORM_SUPPORTS_MULTIPLE_NATIVE_WINDOWS		0
#define PLATFORM_ALLOW_NULL_RHI							1
#define PLATFORM_ENABLE_VECTORINTRINSICS_NEON			PLATFORM_64BITS // disable vector intrinsics to make it compatible with 32-bit in Xcode 8.3
#define PLATFORM_SUPPORTS_STACK_SYMBOLS					1
#define PLATFORM_NUM_AUDIODECOMPRESSION_PRECACHE_BUFFERS		0
#if PLATFORM_TVOS
#define PLATFORM_USES_ES2								0
#define PLATFORM_HAS_TOUCH_MAIN_SCREEN					0
#else
#define PLATFORM_USES_ES2								1
#define PLATFORM_HAS_TOUCH_MAIN_SCREEN					1
#endif
#define PLATFORM_UI_HAS_MOBILE_SCROLLBARS				1
#define PLATFORM_UI_NEEDS_TOOLTIPS						0
#define PLATFORM_UI_NEEDS_FOCUS_OUTLINES				0

//mallocpoison not safe with aligned ansi allocator.  returns the larger unaligned size during Free() which causes writes off the end of the allocation.
#define UE_USE_MALLOC_FILL_BYTES 0 

#define PLATFORM_RHITHREAD_DEFAULT_BYPASS				1

// Function type macros.
#define VARARGS															/* Functions with variable arguments */
#define CDECL															/* Standard C function */
#define STDCALL															/* Standard calling convention */
#if UE_BUILD_DEBUG || UE_DISABLE_FORCE_INLINE
#define FORCEINLINE inline 												/* Don't force code to be inline */
#else
#define FORCEINLINE inline __attribute__ ((always_inline))				/* Force code to be inline */
#endif
#define FORCENOINLINE __attribute__((noinline))							/* Force code to NOT be inline */
#define FUNCTION_CHECK_RETURN_END __attribute__ ((warn_unused_result))	/* Warn that callers should not ignore the return value. */
#define FUNCTION_NO_RETURN_END __attribute__ ((noreturn))				/* Indicate that the function never returns. */

#define ABSTRACT abstract

// Strings.
#define LINE_TERMINATOR TEXT("\n")
#define LINE_TERMINATOR_ANSI "\n"

// Alignment.
#define GCC_PACK(n) __attribute__((packed,aligned(n)))
#define GCC_ALIGN(n) __attribute__((aligned(n)))
#define REQUIRES_ALIGNED_ACCESS 1

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

// DLL export and import definitions
#define DLLEXPORT
#define DLLIMPORT

#define MAX_PATH 1024
