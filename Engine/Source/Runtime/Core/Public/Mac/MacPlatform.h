// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*================================================================================
	MacPlatform.h: Setup for the Mac platform
==================================================================================*/

#pragma once

/**
* Mac specific types
**/
struct FMacPlatformTypes : public FGenericPlatformTypes
{
	typedef unsigned int		DWORD;
	typedef size_t				SIZE_T;
	typedef decltype(NULL)		TYPE_OF_NULL;
	typedef char16_t			CHAR16;
};

typedef FMacPlatformTypes FPlatformTypes;

// Base defines, must define these for the platform, there are no defaults
#define PLATFORM_DESKTOP				1
#define PLATFORM_64BITS					1
// Technically the underlying platform has 128bit atomics, but clang might not issue optimal code
#define PLATFORM_HAS_128BIT_ATOMICS		0
#define PLATFORM_CAN_SUPPORT_EDITORONLY_DATA	1

// Base defines, defaults are commented out

#define PLATFORM_LITTLE_ENDIAN						1
//#define PLATFORM_EXCEPTIONS_DISABLED				!PLATFORM_DESKTOP
#define PLATFORM_SUPPORTS_PRAGMA_PACK				1
#define PLATFORM_ENABLE_VECTORINTRINSICS			1
//#define PLATFORM_USE_LS_SPEC_FOR_WIDECHAR			1
#define PLATFORM_USE_SYSTEM_VSWPRINTF				0
#define PLATFORM_COMPILER_DISTINGUISHES_INT_AND_LONG			1
#define PLATFORM_TCHAR_IS_4_BYTES					1
#define PLATFORM_HAS_BSD_TIME						1
#define PLATFORM_HAS_BSD_IPV6_SOCKETS				1
//#define PLATFORM_USE_PTHREADS						1
#define PLATFORM_MAX_FILEPATH_LENGTH				MAX_PATH
#define PLATFORM_SUPPORTS_TBB						1
#define PLATFORM_SUPPORTS_STACK_SYMBOLS				1
#define PLATFORM_HAS_BSD_SOCKET_FEATURE_MSG_DONTWAIT	1

#define PLATFORM_RHITHREAD_DEFAULT_BYPASS			WITH_EDITOR

#define PLATFORM_ENABLE_POPCNT_INTRINSIC 1

// Function type macros.
#define VARARGS															/* Functions with variable arguments */
#define CDECL															/* Standard C function */
#define STDCALL															/* Standard calling convention */
#if UE_BUILD_DEBUG
#define FORCEINLINE inline 												/* Don't force code to be inline */
#else
#define FORCEINLINE inline __attribute__ ((always_inline))				/* Force code to be inline */
#endif

#define FORCENOINLINE __attribute__((noinline))							/* Force code to NOT be inline */
#define FUNCTION_CHECK_RETURN_END __attribute__ ((warn_unused_result))	/* Warn that callers should not ignore the return value. */
#define FUNCTION_NO_RETURN_END __attribute__ ((noreturn))				/* Indicate that the function never returns. */

#define ABSTRACT abstract

// Alignment.
#define GCC_PACK(n) __attribute__((packed,aligned(n)))
#define GCC_ALIGN(n) __attribute__((aligned(n)))

// operator new/delete operators
// As of 10.9 we need to use _NOEXCEPT & cxx_noexcept compatible definitions
#if __has_feature(cxx_noexcept)
#define OPERATOR_NEW_THROW_SPEC
#else
#define OPERATOR_NEW_THROW_SPEC throw (std::bad_alloc)
#endif
#define OPERATOR_DELETE_THROW_SPEC _NOEXCEPT
#define OPERATOR_NEW_NOTHROW_SPEC  _NOEXCEPT
#define OPERATOR_DELETE_NOTHROW_SPEC  _NOEXCEPT

// DLL export and import definitions
#define DLLEXPORT
#define DLLIMPORT

#define MAX_PATH 1024
