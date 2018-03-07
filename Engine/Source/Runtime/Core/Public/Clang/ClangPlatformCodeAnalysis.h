// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once


// Code analysis features
#if defined( __clang_analyzer__ )
	#define USING_CODE_ANALYSIS 1
#else
	#define USING_CODE_ANALYSIS 0
#endif

//
// NOTE: To suppress a single occurrence of a code analysis warning:
//
// 		CA_SUPPRESS( <WarningNumber> )
//		...code that triggers warning...
//

//
// NOTE: To disable all code analysis warnings for a section of code (such as include statements
//       for a third party library), you can use the following:
//
// 		#if USING_CODE_ANALYSIS
// 			MSVC_PRAGMA( warning( push ) )
// 			MSVC_PRAGMA( warning( disable : ALL_CODE_ANALYSIS_WARNINGS ) )
// 		#endif	// USING_CODE_ANALYSIS
//
//		<code with warnings>
//
// 		#if USING_CODE_ANALYSIS
// 			MSVC_PRAGMA( warning( pop ) )
// 		#endif	// USING_CODE_ANALYSIS
//

#if USING_CODE_ANALYSIS

	// Input argument
	// Example:  void SetValue( CA_IN bool bReadable );
	#define CA_IN

	// Output argument
	// Example:  void FillValue( CA_OUT bool& bWriteable );
	#define CA_OUT

	// Specifies that a function parameter may only be read from, never written.
	// NOTE: CA_READ_ONLY is inferred automatically if your parameter is has a const qualifier.
	// Example:  void SetValue( CA_READ_ONLY bool bReadable );
	#define CA_READ_ONLY

	// Specifies that a function parameter may only be written to, never read.
	// Example:  void FillValue( CA_WRITE_ONLY bool& bWriteable );
	#define CA_WRITE_ONLY

	// Incoming pointer parameter must not be NULL and must point to a valid location in memory.
	// Place before a function parameter's type name.
	// Example:  void SetPointer( CA_VALID_POINTER void* Pointer );
	#define CA_VALID_POINTER

	// Caller must check the return value.  Place before the return value in a function declaration.
	// Example:  CA_CHECK_RETVAL int32 GetNumber();
	#define CA_CHECK_RETVAL

	// Function is expected to never return
	#define CA_NO_RETURN __attribute__((analyzer_noreturn))

	// Suppresses a warning for a single occurrence.  Should be used only for code analysis warnings on Windows platform!
	#define CA_SUPPRESS( WarningNumber )

	// Tells the code analysis engine to assume the statement to be true.  Useful for suppressing false positive warnings.
	// NOTE: We use a double operator not here to avoid issues with passing certain class objects directly into __analysis_assume (which may cause a bogus compiler warning)
	#define CA_ASSUME( Expr )

	// Does a simple 'if (Condition)', but disables warnings about using constants in the condition.  Helps with some macro expansions.
	#define CA_CONSTANT_IF(Condition) if (Condition)


	//
	// Disable some code analysis warnings that we are NEVER interested in
	//

	// NOTE: Please be conservative about adding new suppressions here!  If you add a suppression, please
	//       add a comment that explains the rationale.

#endif

#if defined(__has_feature) && __has_feature(address_sanitizer)
	#define USING_ADDRESS_SANITISER 1
#else
	#define USING_ADDRESS_SANITISER 0
#endif

#if defined(__has_feature) && __has_feature(thread_sanitizer)
	#define USING_THREAD_SANITISER 1
#else
	#define USING_THREAD_SANITISER 0
#endif

#if USING_THREAD_SANITISER

	// Function attribute to disable thread sanitiser validation on specific functions that assume non-atomic load/stores are implicitly atomic
	// This is only safe for int32/uint32/int64/uint64/uintptr_t/intptr_t/void* types on x86/x64 strongly-consistent memory systems.
	#define TSAN_SAFE __attribute__((no_sanitize("thread")))

	// Thread-sanitiser annotation functions.
	extern "C" void AnnotateHappensBefore(const char *f, int l, void *addr);
	extern "C" void AnnotateHappensAfter(const char *f, int l, void *addr);

	// Annotate that previous load/stores occur before addr 
	#define TSAN_BEFORE(addr) AnnotateHappensBefore(__FILE__, __LINE__, (void*)(addr))

	// Annotate that previous load/stores occur after addr 
	#define TSAN_AFTER(addr) AnnotateHappensAfter(__FILE__, __LINE__, (void*)(addr))

	// Because annotating the global bools is tiresome...
	#include <atomic>
	#define TSAN_ATOMIC(Type) std::atomic<Type>

#endif
