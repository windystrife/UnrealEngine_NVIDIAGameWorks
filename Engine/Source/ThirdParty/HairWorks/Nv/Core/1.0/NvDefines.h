/* Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited. */

#ifndef NV_CORE_DEFINES_H
#define NV_CORE_DEFINES_H

// Disable exception signatures (happens on placement new)  
#if defined(__cplusplus) && defined(_MSC_VER) && defined(NV_DISABLE_EXCEPTIONS) && !defined(_HAS_EXCEPTIONS)
#	define _HAS_EXCEPTIONS 0
#endif

#include <stddef.h>
// For memcpy, strlen etc
#include <string.h>

// In C++ compilation unit, also includes <new> for placement new

/** \defgroup core Core Base Library
 @{
*/

/*
The following preprocessor identifiers specify compiler, OS, and architecture.
All definitions have a value of 1 or 0, use '#if' instead of '#ifdef'.
*/

#ifndef NV_COMPILER
#	define NV_COMPILER

/*
Compiler defines, see http://sourceforge.net/p/predef/wiki/Compilers/
NOTE that NV_VC holds the compiler version - not just 1 or 0
*/
#	if defined(_MSC_VER)
#		if _MSC_VER >= 1900
#			define NV_VC 14
#		elif _MSC_VER >= 1800
#			define NV_VC 12
#		elif _MSC_VER >= 1700
#			define NV_VC 11
#		elif _MSC_VER >= 1600
#			define NV_VC 10
#		elif _MSC_VER >= 1500
#			define NV_VC 9
#		else
#			error "Unknown VC version"
#		endif
#	elif defined(__clang__)
#		define NV_CLANG 1
#	elif defined(__SNC__)
#		define NV_SNC 1
#	elif defined(__ghs__)
#		define NV_GHS 1
#	elif defined(__GNUC__) // note: __clang__, __SNC__, or __ghs__ imply __GNUC__
#		define NV_GCC 1
#	else
#		error "Unknown compiler"
#	endif

// Zero unset
#	ifndef NV_VC
#		define NV_VC 0
#	endif
#	ifndef NV_CLANG
#		define NV_CLANG 0
#	endif
#	ifndef NV_SNC
#		define NV_SNC 0
#	endif
#	ifndef NV_GHS
#		define NV_GHS 0
#	endif
#	ifndef NV_GCC
#		define NV_GCC 0
#	endif

#endif // NV_COMPILER

#ifndef NV_PLATFORM
#	define NV_PLATFORM

/**
Operating system defines, see http://sourceforge.net/p/predef/wiki/OperatingSystems/
*/
#	if defined(WINAPI_FAMILY) && WINAPI_FAMILY == WINAPI_PARTITION_APP
#		define NV_WINRT 1 // Windows Runtime, either on Windows RT or Windows 8
#	elif defined(XBOXONE)
#		define NV_XBOXONE 1
#	elif defined(_WIN64) // note: XBOXONE implies _WIN64
#		define NV_WIN64 1
#	elif defined(_M_PPC)
#		define NV_X360 1
#	elif defined(_WIN32) // note: _M_PPC implies _WIN32
#		define NV_WIN32 1
#	elif defined(__ANDROID__)
#		define NV_ANDROID 1
#	elif defined(__linux__) || defined(__CYGWIN__) // note: __ANDROID__ implies __linux__
#		define NV_LINUX 1
#	elif defined(__APPLE__) && (defined(__arm__) || defined(__arm64__))
#		define NV_IOS 1
#	elif defined(__APPLE__)
#		define NV_OSX 1
#	elif defined(__CELLOS_LV2__)
#		define NV_PS3 1
#	elif defined(__ORBIS__)
#		define NV_PS4 1
#	elif defined(__SNC__) && defined(__arm__)
#		define NV_PSP2 1
#	elif defined(__ghs__)
#		define NV_WIIU 1
#	else
#		error "Unknown operating system"
#	endif

// zero unset
#	ifndef NV_WINRT
#		define NV_WINRT 0
#	endif
#	ifndef NV_XBOXONE
#		define NV_XBOXONE 0
#	endif
#	ifndef NV_WIN64
#		define NV_WIN64 0
#	endif
#	ifndef NV_X360
#		define NV_X360 0
#	endif
#	ifndef NV_WIN32
#		define NV_WIN32 0
#	endif
#	ifndef NV_ANDROID
#		define NV_ANDROID 0
#	endif
#	ifndef NV_LINUX
#		define NV_LINUX 0
#	endif
#	ifndef NV_IOS
#		define NV_IOS 0
#	endif
#	ifndef NV_OSX
#		define NV_OSX 0
#	endif
#	ifndef NV_PS3
#		define NV_PS3 0
#	endif
#	ifndef NV_PS4
#		define NV_PS4 0
#	endif
#	ifndef NV_PSP2
#		define NV_PSP2 0
#	endif
#	ifndef NV_WIIU
#		define NV_WIIU 0
#	endif

#endif // NV_PLATFORM

#ifndef NV_PROCESSOR
#	define NV_PROCESSOR

/* Architecture defines, see http://sourceforge.net/p/predef/wiki/Architectures/ */
#	if defined(__x86_64__) || defined(_M_X64) // ps4 compiler defines _M_X64 without value
#		define NV_X64 1
#	elif defined(__i386__) || defined(_M_IX86)
#		define NV_X86 1
#	elif defined(__arm64__) || defined(__aarch64__)
#		define NV_A64 1
#	elif defined(__arm__) || defined(_M_ARM)
#		define NV_ARM 1
#	elif defined(__SPU__)
#		define NV_SPU 1
#	elif defined(__ppc__) || defined(_M_PPC) || defined(__CELLOS_LV2__)
#		define NV_PPC 1
#	else
#		error "Unknown architecture"
#	endif

/**
SIMD defines
*/
#	if defined(__i386__) || defined(_M_IX86) || defined(__x86_64__) || defined(_M_X64)
#		define NV_SSE2 1
#	endif
#	if defined(_M_ARM) || defined(__ARM_NEON__)
#		define NV_NEON 1
#	endif
#	if defined(_M_PPC) || defined(__CELLOS_LV2__)
#		define NV_VMX 1
#	endif

// Zero unset
#	ifndef NV_X64
#		define NV_X64 0
#	endif
#	ifndef NV_X86
#		define NV_X86 0
#	endif
#	ifndef NV_A64
#		define NV_A64 0
#	endif
#	ifndef NV_ARM
#		define NV_ARM 0
#	endif
#	ifndef NV_SPU
#		define NV_SPU 0
#	endif
#	ifndef NV_PPC
#		define NV_PPC 0
#	endif
#	ifndef NV_SSE2
#		define NV_SSE2 0
#	endif
#	ifndef NV_NEON
#		define NV_NEON 0
#	endif
#	ifndef NV_VMX
#		define NV_VMX 0
#	endif

#endif // NV_PROCESSOR

/*
define anything not defined through the command line to 0
*/
#ifndef NV_DEBUG
#	define NV_DEBUG 0
#endif
#ifndef NV_CHECKED
#	define NV_CHECKED 0
#endif
#ifndef NV_PROFILE
#	define NV_PROFILE 0
#endif
#ifndef NV_NVTX
#	define NV_NVTX 0
#endif

// To keep compatible with NvFoundataion/1.0 leave test as defined
#ifndef NV_DOXYGEN
#	define NV_DOXYGEN 0
#endif

/**
family shortcuts
*/
// compiler
#define NV_GCC_FAMILY (NV_CLANG || NV_SNC || NV_GHS || NV_GCC)

// os
#define NV_WINDOWS_FAMILY (NV_WINRT || NV_WIN32 || NV_WIN64)
#define NV_MICROSOFT_FAMILY (NV_XBOXONE || NV_X360 || NV_WINDOWS_FAMILY)
#define NV_LINUX_FAMILY (NV_LINUX || NV_ANDROID)
#define NV_APPLE_FAMILY (NV_IOS || NV_OSX)                  // equivalent to #if __APPLE__
#define NV_UNIX_FAMILY (NV_LINUX_FAMILY || NV_APPLE_FAMILY) // shortcut for unix/posix platforms
// architecture
#define NV_INTEL_FAMILY (NV_X64 || NV_X86)					// Intel x86 family
#define NV_ARM_FAMILY (NV_ARM || NV_A64)
#define NV_PPC_FAMILY (NV_PPC)

#define NV_P64_FAMILY (NV_X64 || NV_A64) // shortcut for 64-bit architectures

// shortcut for PS3 PPU
#define NV_PPU (NV_PS3&& NV_PPC)

#ifndef NV_PROCESSOR_FEATURE
#	define NV_PROCESSOR_FEATURE

#	if NV_X86
#		define NV_INT_IS_32 1
#		define NV_FLOAT_IS_32 1
#		define NV_PTR_IS_32 1
#		define NV_LITTLE_ENDIAN 1
#		define NV_HAS_UNALIGNED_ACCESS 1
#	elif NV_X64
#		define NV_INT_IS_32 1
#		define NV_FLOAT_IS_32 1
#		define NV_PTR_IS_64 1
#		define NV_LITTLE_ENDIAN 1
#		define NV_HAS_UNALIGNED_ACCESS 1
#	elif NV_PPC
#		define NV_INT_IS_32 1
#		define NV_FLOAT_IS_32 1
#		define NV_PTR_IS_32 1
#		define NV_BIG_ENDIAN 1
#	elif NV_ARM
#		define NV_INT_IS_32 1
#		define NV_FLOAT_IS_32 1
#		define NV_PTR_IS_32 1
#		if defined(__ARMEB__) 1
#			define NV_BIG_ENDIAN 1
#		else 
#			define NV_LITTLE_ENDIAN 1
#		endif
#	elif NV_A64
#		define NV_INT_IS_32 1
#		define NV_FLOAT_IS_32 1
#		define NV_PTR_IS_64 1
#		if defined(__ARMEB__)
#			define NV_BIG_ENDIAN 1
#		else
#			define NV_LITTLE_ENDIAN 1
#		endif
#	else
#		error "Unknown processor type"
#	endif

// Set to 0 if not set
#	ifndef NV_LITTLE_ENDIAN
#		define NV_LITTLE_ENDIAN 0
#	endif
#	ifndef NV_BIG_ENDIAN
#		define NV_BIG_ENDIAN 0
#	endif

#	ifndef NV_FLOAT_IS_32
#		define NV_FLOAT_IS_32 0
#	endif
#	ifndef NV_FLOAT_IS_64
#		define NV_FLOAT_IS_64 0
#	endif
#	ifndef NV_INT_IS_32
#		define NV_INT_IS_32 0
#	endif
#	ifndef NV_INT_IS_64
#		define NV_INT_IS_64 0
#	endif
#	ifndef NV_PTR_IS_64
#		define NV_PTR_IS_64 0
#	endif
#	ifndef NV_PTR_IS_32
#		define NV_PTR_IS_32 0
#	endif
#	ifndef NV_HAS_UNALIGNED_ACCESS
#		define NV_HAS_UNALIGNED_ACCESS 0
#	endif

#endif // NV_PROCESSOR_FEATURE

/** Export name mangling control. */
#ifdef __cplusplus
#	define NV_EXTERN_C extern "C" 
#else
#	define NV_EXTERN_C
#endif

#if NV_UNIX_FAMILY&& __GNUC__ >= 4
#	define NV_UNIX_EXPORT __attribute__((visibility("default")))
#else
#	define NV_UNIX_EXPORT
#endif

#if NV_WINDOWS_FAMILY
#	define NV_DLL_EXPORT __declspec(dllexport)
#	define NV_DLL_IMPORT __declspec(dllimport)
#else
#	define NV_DLL_EXPORT NV_UNIX_EXPORT
#	define NV_DLL_IMPORT
#endif

/** Restrict macro */
#if defined(__CUDACC__)
#	define NV_RESTRICT __restrict__
#else
#	define NV_RESTRICT __restrict
#endif

/** Alignment macros
NV_ALIGN_PREFIX and NV_ALIGN_SUFFIX can be used for type alignment instead of aligning individual variables as follows:
NV_ALIGN_PREFIX(16)
struct A {
...
} NV_ALIGN_SUFFIX(16);
This declaration style is parsed correctly by Visual Assist. */
#ifndef NV_ALIGN
#	if NV_MICROSOFT_FAMILY
#		define NV_ALIGN(alignment, decl) __declspec(align(alignment)) decl
#		define NV_ALIGN_PREFIX(alignment) __declspec(align(alignment))
#		define NV_ALIGN_SUFFIX(alignment)
#	elif NV_GCC_FAMILY
#		define NV_ALIGN(alignment, decl) decl __attribute__((aligned(alignment)))
#		define NV_ALIGN_PREFIX(alignment)
#		define NV_ALIGN_SUFFIX(alignment) __attribute__((aligned(alignment)))
#	else
#		define NV_ALIGN(alignment, decl)
#		define NV_ALIGN_PREFIX(alignment)
#		define NV_ALIGN_SUFFIX(alignment)
#	endif
#endif

/** Deprecated macro

To deprecate a function: Place NV_DEPRECATED at the start of the function header (leftmost word).
To deprecate a 'typedef', a 'struct' or a 'class': Place NV_DEPRECATED directly after the keywords ('typedef', 'struct', 'class').
Use these macro definitions to create warnings for deprecated functions
Define NV_ENABLE_DEPRECIATION_WARNINGS to enable warnings */
#ifdef NV_ENABLE_DEPRECIATION_WARNINGS
#	if NV_GCC
#		define NV_DEPRECATED __attribute__((deprecated()))
#	elif defined(NV_VC)
#		define NV_DEPRECATED __declspec(deprecated)
#	endif
#endif
#ifndef NV_DEPRECATED
#	define NV_DEPRECATED
#endif

/// Use for getting the amount of members of a standard C array. 
#define NV_COUNT_OF(x) (sizeof(x)/sizeof(x[0]))
/// NV_INLINE exists to have a way to inline consistant with NV_ALWAYS_INLINE  
#define NV_INLINE inline

// Other defines

#define NV_STRINGIZE_HELPER(X) #X
#define NV_STRINGIZE(X) NV_STRINGIZE_HELPER(X)

#define NV_CONCAT_HELPER(X, Y) X##Y
#define NV_CONCAT(X, Y) NV_CONCAT_HELPER(X, Y)

#ifndef NV_UNUSED
#	define NV_UNUSED(v) (void)v;
#endif

/**
General defines
*/

// GCC Specific 
#if NV_GCC_FAMILY
#	define NV_NO_INLINE __attribute__((noinline))

// This doesn't work on clang - because the typedef is seen as multiply defined, use the line numbered version defined later
#	if !defined(__clang__) && (defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 7)) || defined(__ORBIS__))
#		define NV_COMPILE_TIME_ASSERT(exp) typedef char NvCompileTimeAssert_Dummy[(exp) ? 1 : -1] __attribute__((unused))
#	endif

#	if !NV_SNC && !NV_GHS
#		define NV_OFFSET_OF(X, Y) __builtin_offsetof(X, Y)
#	endif 

#	if !NV_LINUX // Workaround; Fedora Core 3 do not agree with force inline and NvcPool
#		define NV_FORCE_INLINE inline __attribute__((always_inline))
#	endif
// disabled on SPU because they are not supported 
#	if !NV_SPU
#		define NV_PUSH_PACK_DEFAULT _Pragma("pack(push, 8)")
#		define NV_POP_PACK _Pragma("pack(pop)")
#	endif

#   define NV_BREAKPOINT(id) __builtin_trap();

#	if (__cplusplus >= 201103L) && ((__GNUC__ > 4) || (__GNUC__ ==4 && __GNUC_MINOR__ >= 6))
#		define NV_NULL	nullptr
#	else
#		define NV_NULL	__null
#	endif

#	define NV_ALIGN_OF(T)	__alignof__(T)

#	define NV_FUNCTION_SIG	__PRETTY_FUNCTION__

#endif // NV_GCC_FAMILY

// Microsoft VC specific
#if NV_MICROSOFT_FAMILY

#	pragma inline_depth(255)

#	pragma warning(disable : 4324 )	// C4324: structure was padded due to alignment specifier
#	pragma warning(disable : 4514 )	// 'function' : unreferenced inline function has been removed
#	pragma warning(disable : 4710 )	// 'function' : function not inlined
#	pragma warning(disable : 4711 )	// function 'function' selected for inline expansion
#	pragma warning(disable : 4127 )	// C4127: conditional expression is constant

#	define NV_NO_ALIAS __declspec(noalias)
#	define NV_NO_INLINE __declspec(noinline)
#	define NV_FORCE_INLINE __forceinline
#	define NV_PUSH_PACK_DEFAULT __pragma(pack(push, 8))
#	define NV_POP_PACK __pragma(pack(pop))

#	define NV_BREAKPOINT(id) __debugbreak();

#	ifdef __cplusplus
#		define NV_NULL	nullptr
#	endif

#	define NV_ALIGN_OF(T) __alignof(T)

#	define NV_FUNCTION_SIG	__FUNCSIG__

#	define NV_INT64(x) (x##i64)
#	define NV_UINT64(x) (x##ui64)

#	define NV_STDCALL __stdcall
#	define NV_CALL_CONV __cdecl

#endif // NV_MICROSOFT_FAMILY


// Clang specific
#if NV_CLANG
//#	pragma clang diagnostic ignored "-Wunused-local-typedef"
#endif

// Set defaults if not set

// Used for doing constant literals
#ifndef NV_INT64
#	define NV_INT64(x) (x##ll)
#endif
#ifndef NV_UINT64
#	define NV_UINT64(x) (x##ull)
#endif

#ifndef NV_NULL
#	if defined(NULL)
#		define NV_NULL NULL
#	else
#		define NV_NULL 0
#	endif
#endif
#ifndef NV_PUSH_PACK_DEFAULT
#	define NV_PUSH_PACK_DEFAULT
#	define NV_POP_PACK
#endif
#ifndef NV_FORCE_INLINE
#	define NV_FORCE_INLINE inline
#endif
#ifndef NV_NO_INLINE 
#	define NV_NO_INLINE
#endif
#ifndef NV_NO_ALIAS
#	define NV_NO_ALIAS
#endif
#ifndef NV_COMPILE_TIME_ASSERT
#	define NV_COMPILE_TIME_ASSERT(exp) typedef char NV_CONCAT(NvCompileTimeAssert,__LINE__)[(exp) ? 1 : -1]
#endif
#ifndef NV_OFFSET_OF
#	define NV_OFFSET_OF(X, Y) offsetof(X, Y)
#endif
#ifndef NV_BREAKPOINT
// Make it crash with a write to 0!
#   define NV_BREAKPOINT(id) (*((int*)0) = int(id));
#endif

#ifndef NV_FUNCTION_NAME
#	define NV_FUNCTION_NAME	__FUNCTION__
#endif
#ifndef NV_FUNCTION_SIG
#	define NV_FUNCTION_SIG NV_FUNCTION_NAME
#endif

#ifndef NV_STDCALL
#	define NV_STDCALL 
#endif
#ifndef NV_CALL_CONV
#	define NV_CALL_CONV
#endif

//! casting the null ptr takes a special-case code path, which we don't want
#define NV_OFFSETOF_BASE 0x100 
#define NV_OFFSET_OF_RT(Class, Member)                                                                                 \
	(reinterpret_cast<size_t>(&reinterpret_cast<Class*>(NV_OFFSETOF_BASE)->Member) - size_t(NV_OFFSETOF_BASE))

#ifdef __CUDACC__
#	define NV_CUDA_CALLABLE __host__ __device__
#else
#	define NV_CUDA_CALLABLE
#endif

// Ensure that the application hasn't tweaked the pack value to less than 8, which would break
// matching between the API headers and the binaries
// This assert works on win32/win64/360/ps3, but may need further specialization on other platforms.
// Some GCC compilers need the compiler flag -malign-double to be set.
// Apparently the apple-clang-llvm compiler doesn't support malign-double.

#if NV_PS4 || NV_APPLE_FAMILY
typedef long NvCorePackValidateType;
#elif NV_ANDROID
typedef double NvCorePackValidateType;
#else
typedef long long NvCorePackValidateType;
#endif

typedef struct NvCorePackValidate
{
	char _;
	NvCorePackValidateType  a;
} NvCorePackValidate;

#if !NV_APPLE_FAMILY
NV_COMPILE_TIME_ASSERT(NV_OFFSET_OF(NvCorePackValidate, a) == 8);
#endif

// use in a cpp file to suppress LNK4221
#if NV_VC
#	ifdef __cplusplus
#		define NV_DUMMY_SYMBOL namespace { char NvDummySymbol; }
#	else
#		define NV_DUMMY_SYMBOL static char NvDummySymbol;
#	endif
#else
#	define NV_DUMMY_SYMBOL
#endif

#if NV_GCC_FAMILY && !NV_GHS
#	define NV_WEAK_SYMBOL __attribute__((weak)) // this is to support SIMD constant merging in template specialization
#else
#	define NV_WEAK_SYMBOL
#endif

// C++ Symbols
#ifdef __cplusplus

// Macro for avoiding default assignment and copy, because doing this by inheritance can increase class size on some
// platforms.
#	define NV_NO_COPY(cls)        \
protected:                        \
	cls(const cls&);               \
	cls& operator=(const cls&);

/*! \namespace nvidia
\brief Main namespace for nVidia code. 
\details The nvidia namespace holds the basic types that are used across modules developed by nVidia. Generally 
speaking a module will create a child namespace to separately cleanly types, classes and functions between 
modules. The C++ compliant types for the core library are in the nvidia namespace. 

The types, and functions of the nvidia namespace can be accessed via the fully qualified namespace
of nvidia, but it often easier and more readable to use the shortened namespace alias Nv.
*/

namespace nvidia {}

// C++ specific macros

// Gcc
#	if NV_GCC_FAMILY
// Check for C++11
#		if (__cplusplus >= 201103L)
#			if (__GNUC__ * 100 + __GNUC_MINOR__) >= 405
#				define NV_HAS_MOVE_SEMANTICS 1
#			endif
#			if (__GNUC__ * 100 + __GNUC_MINOR__) >= 406
#				define GW_HAS_ENUM_CLASS 1
#			endif
#			if (__GNUC__ * 100 + __GNUC_MINOR__) >= 407
#				define NV_OVERRIDE override 
#			endif
#		endif
#	endif // NV_GCC_FAMILY


// Visual Studio

// Macro for declaring if a method is no throw. Should be set before the return parameter. 
#if NV_WINDOWS_FAMILY && !defined(NV_DISABLE_EXCEPTIONS)
#	define NV_NO_THROW __declspec(nothrow) 
#endif

#	if NV_VC
// C4481: nonstandard extension used: override specifier 'override'
#		if _MSC_VER < 1700
#			pragma warning(disable : 4481)
#		endif
#		define NV_OVERRIDE	override

#		if _MSC_VER >= 1600
#			define NV_HAS_MOVE_SEMANTICS 1
#		endif
#	if _MSC_VER >= 1700
#		define NV_HAS_ENUM_CLASS 1
#   endif

#endif // NV_VC

// Clang specific
#	if NV_CLANG
#	endif // NV_CLANG

// Set non set  

#ifndef NV_NO_THROW
#	define NV_NO_THROW
#endif
#ifndef NV_OVERRIDE
#	define NV_OVERRIDE
#endif
#ifndef NV_HAS_ENUM_CLASS
#	define NV_HAS_ENUM_CLASS 0
#endif
#ifndef NV_HAS_MOVE_SEMANTICS
#	define NV_HAS_MOVE_SEMANTICS 0
#endif

#define NV_MCALL NV_STDCALL

#include <new>		// For placement new

#endif // __cplusplus

// Depreciated

// For compatibility - better to use NV_EXTERN_C (macro name is closer to action)
#define NV_C_EXPORT NV_EXTERN_C

#define NV_NOALIAS NV_NO_ALIAS
#define NV_NOINLINE NV_NO_INLINE
#define NV_NOCOPY NV_NO_COPY

/** @} */
#endif // #ifndef NV_CORE_DEFINES_H
