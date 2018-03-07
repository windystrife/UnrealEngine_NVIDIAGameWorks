// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	IPhone.h: Unreal definitions for iPhone.
=============================================================================*/

/*----------------------------------------------------------------------------
	Platform compiler definitions.
----------------------------------------------------------------------------*/

#ifndef __UE3_IPHONE_H__
#define __UE3_IPHONE_H__

#include <string.h>
#include <alloca.h>
#include <stdlib.h>
#include <stdio.h>
#include <float.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdint.h>
#include <limits.h>
#include <sys/time.h>
#include <math.h>
#include <mach/mach_time.h>
#include <libkern/OSAtomic.h>
#include <CommonCrypto/CommonDigest.h>

/*----------------------------------------------------------------------------
	Platform specifics types and defines.
----------------------------------------------------------------------------*/

// Comment this out if you have no need for unicode strings (ie asian languages, etc).
#define UNICODE 1


// Generates GCC version like this:  xxyyzz (eg. 030401)
// xx: major version, yy: minor version, zz: patch level
#ifdef __GNUC__
#	define GCC_VERSION	(__GNUC__*10000 + __GNUC_MINOR__*100 + __GNUC_PATCHLEVEL)
#endif

// Undo any defines.
#undef NULL
#undef BYTE
#undef WORD
#undef DWORD
#undef INT
#undef FLOAT
#undef MAXBYTE
#undef MAXWORD
#undef MAXDWORD
#undef MAXINT
#undef CDECL

// Make sure HANDLE is defined.
#ifndef _WINDOWS_
	#define HANDLE void*
	#define HINSTANCE void*
#endif

#if _DEBUG
	#define STUBBED(x) \
		do { \
			static UBOOL AlreadySeenThisStubbedSection = FALSE; \
			if (!AlreadySeenThisStubbedSection) \
			{ \
				AlreadySeenThisStubbedSection = TRUE; \
				fprintf(stderr, "STUBBED: %s at %s:%d (%s)\n", x, __FILE__, __LINE__, __FUNCTION__); \
			} \
		} while (0)
#else
	#define STUBBED(x)
#endif




// Sizes.
enum {DEFAULT_ALIGNMENT = 8 }; // Default boundary to align memory allocations on.

#define RENDER_DATA_ALIGNMENT 128 // the value to align some renderer bulk data to


// Optimization macros (preceded by #pragma).
#define PRAGMA_DISABLE_OPTIMIZATION _Pragma("optimize(\"\",off)")
#ifdef _DEBUG
	#define PRAGMA_ENABLE_OPTIMIZATION  _Pragma("optimize(\"\",off)")
#else
	#define PRAGMA_ENABLE_OPTIMIZATION  _Pragma("optimize(\"\",on)")
#endif

// Function type macros.
#define VARARGS     					/* Functions with variable arguments */
#define CDECL	    					/* Standard C function */
#define STDCALL							/* Standard calling convention */


#define FORCEINLINE inline __attribute__ ((always_inline))    /* Force code to be inline */

#define FORCENOINLINE __attribute__((noinline))			/* Force code to be NOT inline */
#define ZEROARRAY						/* Zero-length arrays in structs */

// pointer aliasing restricting
#define RESTRICT __restrict

// Compiler name.
#ifdef _DEBUG
	#define COMPILER "Compiled with GCC debug"
#else
	#define COMPILER "Compiled with GCC"
#endif



// Unsigned base types.
typedef uint8_t					BYTE;		// 8-bit  unsigned.
typedef uint16_t				WORD;		// 16-bit unsigned.
typedef uint32_t				UINT;		// 32-bit unsigned.
typedef unsigned long			DWORD;		// 32-bit unsigned.
typedef uint64_t				QWORD;		// 64-bit unsigned.

// Signed base types.
typedef int8_t					SBYTE;		// 8-bit  signed.
typedef int16_t					SWORD;		// 16-bit signed.
typedef int32_t					INT;		// 32-bit signed.
typedef int32_t					LONG;		// 32-bit signed.
typedef int64_t					SQWORD;		// 64-bit unsigned.

// Character types.
typedef char					ANSICHAR;	// An ANSI character. normally a signed type.
typedef int16_t					UNICHAR;	// A unicode character. normally a signed type.
// WCHAR defined below

// Other base types.
typedef UINT					UBOOL;		// Boolean 0 (false) or 1 (true).
typedef float					FLOAT;		// 32-bit IEEE floating point.
typedef double					DOUBLE;		// 64-bit IEEE double.
typedef uintptr_t				SIZE_T;     // Should be size_t, but windows uses this
typedef intptr_t				PTRINT;		// Integer large enough to hold a pointer.
typedef uintptr_t				UPTRINT;	// Unsigned integer large enough to hold a pointer.

// Bitfield type.
typedef unsigned int			BITFIELD;	// For bitfields.

/** Represents a serializable object pointer in UnrealScript.  This is always 64-bits, even on 32-bit platforms. */
typedef	QWORD				ScriptPointerType;

#define DECLARE_UINT64(x)	x##ULL


// Make sure characters are unsigned.
#ifdef _CHAR_UNSIGNED
	#error "Bad VC++ option: Characters must be signed"
#endif

// No asm if not compiling for x86.
#define ASM_X86 0

#define __INTEL_BYTE_ORDER__ 1

#define PLATFORM_64BITS 0
#define PLATFORM_32BITS 1

// DLL file extension.
#define DLLEXT TEXT(".dylib")

// Pathnames.
#define PATH(s) s

// NULL.
#define NULL 0

#define FALSE 0
#define TRUE 1

// Platform support options.
#define FORCE_ANSI_LOG           1

// OS unicode function calling.
typedef wchar_t TCHAR;
#define TCHAR_IS_4_BYTES 1
#define _TCHAR_DEFINED

// defines for the "standard" unicode-safe string functions
#define _tcscpy wcscpy
#define _tcslen wcslen
#define _tcsstr wcsstr
#define _tcschr wcschr
#define _tcsrchr wcsrchr
#define _tcscat wcscat
#define _tcscmp wcscmp
#define _stricmp strcasecmp
#define _tcsicmp wgccstrcasecmp
#define _tcsncmp wcsncmp
#define _tcsupr wgccupr //strupr
#define _tcstoul wcstoul
#define _tcstoui64 wcstoull
#define _tcsnicmp wgccstrncasecmp
#define _tstoi(s) wcstoul(s, 0, 10)
#define _tstoi64(s) wcstoull(s, 0, 10)
#define _tstof(s) wcstod(s, 0)
#define _tcstod wcstod
#define _tcsncpy wcsncpy
#define _stscanf swscanf

#define CP_OEMCP 1
#define CP_ACP 1

#include <wchar.h>
#include <wctype.h>

// Strings.
#define LINE_TERMINATOR TEXT("\n")
#define PATH_SEPARATOR TEXT("\\")
#define appIsPathSeparator( Ch )	((Ch) == TEXT('/') || (Ch) == TEXT('\\'))


// Alignment.
#define GCC_PACK(n) __attribute__((packed,aligned(n)))
#define GCC_ALIGN(n) __attribute__((aligned(n)))
#define MS_ALIGN(n)

// LLVM needs aligned access, and gcc seems maybe even a tiny bit faster with it
#define REQUIRES_ALIGNED_ACCESS 1

// GCC doesn't support __noop, but there is a workaround :)
#define COMPILER_SUPPORTS_NOOP 1
#define __noop(...)


/** @todo appCreateBitmap needs this - what is a valid number for all platforms? */
#define MAX_PATH	128

#define appMalloc malloc
#define appFree free

#define TEXT(s) L##s
#define debugf(...)

#define PHONE_HOME_URL "et.epicgames.com"

FORCEINLINE INT appInterlockedIncrement(volatile INT* Value)
{
	return (INT)OSAtomicIncrement32Barrier((int32_t*)Value);
}

FORCEINLINE INT appInterlockedDecrement(volatile INT* Value)
{
	return (INT)OSAtomicDecrement32Barrier((int32_t*)Value);
}

/** Wrapper around the common crypto SHA-1 hasher */
class FSHA1
{
public:
	// Constructor and Destructor
	FSHA1()
	{
		Reset();
	}
	
	void Reset()
	{
		CC_SHA1_Init(&Context);
	}
	
	// Update the hash value
	void Update(const BYTE* data, DWORD len)
	{
		CC_SHA1_Update(&Context, data, len);
	}
	
	// Finalize hash and report
	void Final()
	{
		CC_SHA1_Final(FinalHash, &Context);
	}
	
	// Report functions: as pre-formatted and raw data
	void GetHash(BYTE *puDest)
	{
		memcpy(puDest, FinalHash, CC_SHA1_DIGEST_LENGTH);
	}
	
	/**
	 * Calculate the hash on a single block and return it
	 *
	 * @param Data Input data to hash
	 * @param DataSize Size of the Data block
	 * @param OutHash Resulting hash value (20 byte buffer)
	 */
	static void HashBuffer(const void* Data, DWORD DataSize, BYTE* OutHash)
	{
		CC_SHA1(Data, DataSize, OutHash);
	}
private:
	CC_SHA1_CTX Context;
	BYTE FinalHash[CC_SHA1_DIGEST_LENGTH];
};

extern INT GEngineVersion;

#endif // __UE3_IPHONE_H__
