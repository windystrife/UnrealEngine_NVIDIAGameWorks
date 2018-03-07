// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once


// This file is included in some resource files, which issue a warning:
//
// warning RC4011: identifier truncated to 'PLATFORM_CAN_SUPPORT_EDITORONLY'
//
// due to limitations of resource compiler. The only thing needed from this file
// for resource compilation is PREPROCESSOR_TO_STRING macro at the end, so we take
// rest of code out for resource compilation.
#ifndef RC_INVOKED

#define LOCALIZED_SEEKFREE_SUFFIX	TEXT("_LOC")
#define PLAYWORLD_PACKAGE_PREFIX TEXT("UEDPIE")

#ifndef WITH_EDITORONLY_DATA
	#if !PLATFORM_CAN_SUPPORT_EDITORONLY_DATA || UE_SERVER || PLATFORM_IOS
		#define WITH_EDITORONLY_DATA	0
	#else
		#define WITH_EDITORONLY_DATA	1
	#endif
#endif

/** This controls if metadata for compiled in classes is unpacked and setup at boot time. Meta data is not normally used except by the editor. **/
#define WITH_METADATA (WITH_EDITORONLY_DATA && WITH_EDITOR)

// Set up optimization control macros, now that we have both the build settings and the platform macros
#define PRAGMA_DISABLE_OPTIMIZATION		PRAGMA_DISABLE_OPTIMIZATION_ACTUAL
#if UE_BUILD_DEBUG
	#define PRAGMA_ENABLE_OPTIMIZATION  PRAGMA_DISABLE_OPTIMIZATION_ACTUAL
#else
	#define PRAGMA_ENABLE_OPTIMIZATION  PRAGMA_ENABLE_OPTIMIZATION_ACTUAL
#endif

#if UE_BUILD_DEBUG
	#define FORCEINLINE_DEBUGGABLE FORCEINLINE_DEBUGGABLE_ACTUAL
#else
	#define FORCEINLINE_DEBUGGABLE FORCEINLINE
#endif


#if STATS
	#define CLOCK_CYCLES(Timer)   {Timer -= FPlatformTime::Cycles();}
	#define UNCLOCK_CYCLES(Timer) {Timer += FPlatformTime::Cycles();}
#else
	#define CLOCK_CYCLES(Timer)
	#define UNCLOCK_CYCLES(Timer)
#endif

#define SHUTDOWN_IF_EXIT_REQUESTED
#define RETURN_IF_EXIT_REQUESTED
#define RETURN_VAL_IF_EXIT_REQUESTED(x)

#if CHECK_PUREVIRTUALS
#define PURE_VIRTUAL(func,extra) =0;
#else
#define PURE_VIRTUAL(func,extra) { LowLevelFatalError(TEXT("Pure virtual not implemented (%s)"), TEXT(#func)); extra }
#endif


// Code analysis features
#ifndef USING_CODE_ANALYSIS
	#define USING_CODE_ANALYSIS 0
#endif

#if USING_CODE_ANALYSIS
	#if !defined( CA_IN ) || !defined( CA_OUT ) || !defined( CA_READ_ONLY ) || !defined( CA_WRITE_ONLY ) || !defined( CA_VALID_POINTER ) || !defined( CA_CHECK_RETVAL ) || !defined( CA_NO_RETURN ) || !defined( CA_SUPPRESS ) || !defined( CA_ASSUME )
		#error Code analysis macros are not configured correctly for this platform
	#endif
#else
	// Just to be safe, define all of the code analysis macros to empty macros
	#define CA_IN 
	#define CA_OUT
	#define CA_READ_ONLY
	#define CA_WRITE_ONLY
	#define CA_VALID_POINTER
	#define CA_CHECK_RETVAL
	#define CA_NO_RETURN
	#define CA_SUPPRESS( WarningNumber )
	#define CA_ASSUME( Expr )
	#define CA_CONSTANT_IF(Condition) if (Condition)
#endif

#ifndef USING_THREAD_SANITISER
	#define USING_THREAD_SANITISER 0
#endif

#if USING_THREAD_SANITISER
	#if !defined( TSAN_SAFE ) || !defined( TSAN_BEFORE ) || !defined( TSAN_AFTER ) || !defined( TSAN_ATOMIC )
		#error Thread Sanitiser macros are not configured correctly for this platform
	#endif
#else
	// Define TSAN macros to empty when not enabled
	#define TSAN_SAFE
	#define TSAN_BEFORE(Addr)
	#define TSAN_AFTER(Addr)
	#define TSAN_ATOMIC(Type) Type
#endif

enum {INDEX_NONE	= -1				};
enum {UNICODE_BOM   = 0xfeff			};

enum EForceInit 
{
	ForceInit,
	ForceInitToZero
};
enum ENoInit {NoInit};

// Handle type to stably track users on a specific platform
typedef int32 FPlatformUserId;
const FPlatformUserId PLATFORMUSERID_NONE = INDEX_NONE;
#endif // RC_INVOKED

// Turns an preprocessor token into a real string (see UBT_COMPILED_PLATFORM)
#define PREPROCESSOR_TO_STRING(x) PREPROCESSOR_TO_STRING_INNER(x)
#define PREPROCESSOR_TO_STRING_INNER(x) #x

// Concatenates two preprocessor tokens, performing macro expansion on them first
#define PREPROCESSOR_JOIN(x, y) PREPROCESSOR_JOIN_INNER(x, y)
#define PREPROCESSOR_JOIN_INNER(x, y) x##y

// Expands to the second argument or the third argument if the first argument is 1 or 0 respectively
#define PREPROCESSOR_IF(cond, x, y) PREPROCESSOR_JOIN(PREPROCESSOR_IF_INNER_, cond)(x, y)
#define PREPROCESSOR_IF_INNER_1(x, y) x
#define PREPROCESSOR_IF_INNER_0(x, y) y

// Expands to the parameter list of the macro - used for when you need to pass a comma-separated identifier to another macro as a single parameter
#define PREPROCESSOR_COMMA_SEPARATED(first, second, ...) first, second, ##__VA_ARGS__

// Expands to nothing - used as a placeholder
#define PREPROCESSOR_NOTHING

// When passed to pragma message will result in clickable warning in VS
#define WARNING_LOCATION(Line) __FILE__ "(" PREPROCESSOR_TO_STRING(Line) ")"

// Push and pop macro definitions
#ifdef __clang__
	#define PUSH_MACRO(name) _Pragma(PREPROCESSOR_TO_STRING(push_macro(PREPROCESSOR_TO_STRING(name))))
	#define POP_MACRO(name) _Pragma(PREPROCESSOR_TO_STRING(pop_macro(PREPROCESSOR_TO_STRING(name))))
#else
	#define PUSH_MACRO(name) __pragma(push_macro(PREPROCESSOR_TO_STRING(name)))
	#define POP_MACRO(name) __pragma(pop_macro(PREPROCESSOR_TO_STRING(name)))
#endif


#ifdef __COUNTER__
	// Created a variable with a unique name
	#define ANONYMOUS_VARIABLE( Name ) PREPROCESSOR_JOIN(Name, __COUNTER__)
#else
	// Created a variable with a unique name.
	// Less reliable than the __COUNTER__ version.
	#define ANONYMOUS_VARIABLE( Name ) PREPROCESSOR_JOIN(Name, __LINE__)
#endif
