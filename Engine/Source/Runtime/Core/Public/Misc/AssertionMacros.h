// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "HAL/PlatformMisc.h"
#include "VarArgs.h"

namespace ELogVerbosity
{
	enum Type : uint8;
}
/**
 * C Exposed function to print the callstack to ease debugging needs.  In an 
 * editor build you can call this in the Immediate Window by doing, {,,UE4Editor-Core}::PrintScriptCallstack()
 */
extern "C" DLLEXPORT void PrintScriptCallstack();

/**
 * FDebug
 * These functions offer debugging and diagnostic functionality and its presence 
 * depends on compiler switches.
 **/
struct CORE_API FDebug
{
	/** Logs final assert message and exits the program. */
	static void VARARGS AssertFailed(const ANSICHAR* Expr, const ANSICHAR* File, int32 Line, const TCHAR* Format = TEXT(""), ...);

	/** Records the calling of AssertFailed() */
	static bool bHasAsserted;

	/** Dumps the stack trace into the log, meant to be used for debugging purposes. */
	static void DumpStackTraceToLog();

#if DO_CHECK || DO_GUARD_SLOW
	/** Failed assertion handler.  Warning: May be called at library startup time. */
	VARARG_DECL(static void, static void, VARARG_NONE, LogAssertFailedMessage, VARARG_NONE, const TCHAR*, VARARG_EXTRA(const ANSICHAR* Expr, const ANSICHAR* File, int32 Line), VARARG_EXTRA(Expr, File, Line));
	
	/**
	 * Called when an 'ensure' assertion fails; gathers stack data and generates and error report.
	 *
	 * @param	Expr	Code expression ANSI string (#code)
	 * @param	File	File name ANSI string (__FILE__)
	 * @param	Line	Line number (__LINE__)
	 * @param	Msg		Informative error message text
	 * 
	 * Don't change the name of this function, it's used to detect ensures by the crash reporter.
	 */
	static void EnsureFailed( const ANSICHAR* Expr, const ANSICHAR* File, int32 Line, const TCHAR* Msg );

	/**
	 * Logs an error if bLog is true, and returns false.  Takes a formatted string.
	 *
	 * @param	bLog	Log if true.
	 * @param	Expr	Code expression ANSI string (#code)
	 * @param	File	File name ANSI string (__FILE__)
	 * @param	Line	Line number (__LINE__)
	 * @param	FormattedMsg	Informative error message text with variable args
	 *
	 * @return false in all cases.
	 *
	 * Note: this crazy name is to ensure that the crash reporter recognizes it, which checks for functions in the callstack starting with 'EnsureNotFalse'.
	 */
	static bool VARARGS OptionallyLogFormattedEnsureMessageReturningFalse(bool bLog, const ANSICHAR* Expr, const ANSICHAR* File, int32 Line, const TCHAR* FormattedMsg, ...);
#endif // DO_CHECK || DO_GUARD_SLOW

	/**
	* Logs an a message to the provided log channel. If a callstack is included (detected by lines starting with 0x) if will be logged in the standard Unreal 
	* format of [Callstack] Address FunctionInfo [File]
	*
	* @param	LogName		Log channel. If NAME_None then LowLevelOutputDebugStringf is used
	* @param	File		File name ANSI string (__FILE__)
	* @param	Line		Line number (__LINE__)
	* @param	Heading		Informative heading displayed above the message callstack
	* @param	Message		Multi-line message with a callstack
	*
	*/
	static void LogFormattedMessageWithCallstack(const FName& LogName, const ANSICHAR* File, int32 Line, const TCHAR* Heading, const TCHAR* Message, ELogVerbosity::Type Verbosity);
};

/*----------------------------------------------------------------------------
	Check, verify, etc macros
----------------------------------------------------------------------------*/

//
// "check" expressions are only evaluated if enabled.
// "verify" expressions are always evaluated, but only cause an error if enabled.
//

#if !UE_BUILD_SHIPPING
#define _DebugBreakAndPromptForRemote() \
	if (!FPlatformMisc::IsDebuggerPresent()) { FPlatformMisc::PromptForRemoteDebugging(false); } FPlatformMisc::DebugBreak();
#else
	#define _DebugBreakAndPromptForRemote()
#endif // !UE_BUILD_SHIPPING
#if DO_CHECK
	#define checkCode( Code )		do { Code; } while ( false );
	#define verify(expr)			{ if(UNLIKELY(!(expr))) { FDebug::LogAssertFailedMessage( #expr, __FILE__, __LINE__, TEXT("") ); _DebugBreakAndPromptForRemote(); FDebug::AssertFailed( #expr, __FILE__, __LINE__ ); CA_ASSUME(false); } }
	#define check(expr)				{ if(UNLIKELY(!(expr))) { FDebug::LogAssertFailedMessage( #expr, __FILE__, __LINE__, TEXT("") ); _DebugBreakAndPromptForRemote(); FDebug::AssertFailed( #expr, __FILE__, __LINE__ ); CA_ASSUME(false); } }
	
	/**
	 * verifyf, checkf: Same as verify, check but with printf style additional parameters
	 * Read about __VA_ARGS__ (variadic macros) on http://gcc.gnu.org/onlinedocs/gcc-3.4.4/cpp.pdf.
	 */
	#define verifyf(expr, format,  ...)		{ if(UNLIKELY(!(expr))) { FDebug::LogAssertFailedMessage( #expr, __FILE__, __LINE__, format, ##__VA_ARGS__ ); _DebugBreakAndPromptForRemote(); FDebug::AssertFailed(#expr, __FILE__, __LINE__, format, ##__VA_ARGS__); CA_ASSUME(false); } }
	#define checkf(expr, format,  ...)		{ if(UNLIKELY(!(expr))) { FDebug::LogAssertFailedMessage( #expr, __FILE__, __LINE__, format, ##__VA_ARGS__ ); _DebugBreakAndPromptForRemote(); FDebug::AssertFailed(#expr, __FILE__, __LINE__, format, ##__VA_ARGS__); CA_ASSUME(false); } }
	/**
	 * Denotes code paths that should never be reached.
	 */
	#define checkNoEntry()       { FDebug::LogAssertFailedMessage( "Enclosing block should never be called", __FILE__, __LINE__, TEXT("") ); _DebugBreakAndPromptForRemote(); FDebug::AssertFailed("Enclosing block should never be called", __FILE__, __LINE__ ); CA_ASSUME(false); }

	/**
	 * Denotes code paths that should not be executed more than once.
	 */
	#define checkNoReentry()     { static bool s_beenHere##__LINE__ = false;                                         \
	                               checkf( !s_beenHere##__LINE__, TEXT("Enclosing block was called more than once") );   \
								   s_beenHere##__LINE__ = true; }

	class FRecursionScopeMarker
	{
	public: 
		FRecursionScopeMarker(uint16 &InCounter) : Counter( InCounter ) { ++Counter; }
		~FRecursionScopeMarker() { --Counter; }
	private:
		uint16& Counter;
	};

	/**
	 * Denotes code paths that should never be called recursively.
	 */
	#define checkNoRecursion()  static uint16 RecursionCounter##__LINE__ = 0;                                            \
	                            checkf( RecursionCounter##__LINE__ == 0, TEXT("Enclosing block was entered recursively") );  \
	                            const FRecursionScopeMarker ScopeMarker##__LINE__( RecursionCounter##__LINE__ )

	#define unimplemented()       { FDebug::LogAssertFailedMessage( "Unimplemented function called", __FILE__, __LINE__, TEXT("") ); _DebugBreakAndPromptForRemote(); FDebug::AssertFailed("Unimplemented function called", __FILE__, __LINE__); CA_ASSUME(false); }

#else
	#define checkCode(...)
	#define check(expr)					{ CA_ASSUME(expr); }
	#define checkf(expr, format, ...)	{ CA_ASSUME(expr); }
	#define checkNoEntry()
	#define checkNoReentry()
	#define checkNoRecursion()
	#define verify(expr)				{ if(UNLIKELY(!(expr))){ CA_ASSUME(false); } }
	#define verifyf(expr, format, ...)	{ if(UNLIKELY(!(expr))){ CA_ASSUME(false); } }
	#define unimplemented()				{ CA_ASSUME(false); }
#endif

//
// Check for development only.
//
#if DO_GUARD_SLOW
	#define checkSlow(expr)					{ if(UNLIKELY(!(expr))) { FDebug::LogAssertFailedMessage( #expr, __FILE__, __LINE__, TEXT("") ); _DebugBreakAndPromptForRemote(); FDebug::AssertFailed(#expr, __FILE__, __LINE__); CA_ASSUME(false); } }
	#define checkfSlow(expr, format, ...)	{ if(UNLIKELY(!(expr))) { FDebug::LogAssertFailedMessage( #expr, __FILE__, __LINE__, format, ##__VA_ARGS__ ); _DebugBreakAndPromptForRemote(); FDebug::AssertFailed( #expr, __FILE__, __LINE__, format, ##__VA_ARGS__ ); CA_ASSUME(false); } }
	#define verifySlow(expr)				{ if(UNLIKELY(!(expr))) { FDebug::LogAssertFailedMessage( #expr, __FILE__, __LINE__, TEXT("") ); _DebugBreakAndPromptForRemote(); FDebug::AssertFailed(#expr, __FILE__, __LINE__); CA_ASSUME(false); } }
#else
	#define checkSlow(expr)					{ CA_ASSUME(expr); }
	#define checkfSlow(expr, format, ...)	{ CA_ASSUME(expr); }
	#define verifySlow(expr)				{ if(UNLIKELY(!(expr))) { CA_ASSUME(false); } }
#endif

/**
 * ensure() can be used to test for *non-fatal* errors at runtime
 *
 * Rather than crashing, an error report (with a full call stack) will be logged and submitted to the crash server. 
 * This is useful when you want runtime code verification but you're handling the error case anyway.
 *
 * Note: ensure() can be nested within conditionals!
 *
 * Example:
 *
 *		if (ensure(InObject != nullptr))
 *		{
 *			InObject->Modify();
 *		}
 *
 * This code is safe to execute as the pointer dereference is wrapped in a non-nullptr conditional block, but
 * you still want to find out if this ever happens so you can avoid side effects.  Using ensure() here will
 * force a crash report to be generated without crashing the application (and potentially causing editor
 * users to lose unsaved work.)
 *
 * ensure() resolves to just evaluate the expression when DO_CHECK is 0 (typically shipping or test builds).
 *
 * By default a given call site will only print the callstack and submit the 'crash report' the first time an
 * ensure is hit in a session; ensureAlways can be used instead if you want to handle every failure
 */

#if DO_CHECK && !USING_CODE_ANALYSIS // The Visual Studio 2013 analyzer doesn't understand these complex conditionals

	namespace UE4Asserts_Private
	{
		// This is used by ensure to generate a bool per instance
		// by passing a lambda which will uniquely instantiate the template.
		template <typename Type>
		bool TrueOnFirstCallOnly(const Type&)
		{
			static bool bValue = true;
			bool Result = bValue;
			bValue = false;
			return Result;
		}

		FORCEINLINE bool OptionallyDebugBreakAndPromptForRemoteReturningFalse(bool bBreak, bool bIsEnsure = false)
		{
			if (bBreak)
			{
				FPlatformMisc::DebugBreakAndPromptForRemoteReturningFalse(bIsEnsure);
			}
			return false;
		}
	}

	#define ensure(           InExpression                ) (LIKELY(!!(InExpression)) || FDebug::OptionallyLogFormattedEnsureMessageReturningFalse(UE4Asserts_Private::TrueOnFirstCallOnly([]{}), #InExpression, __FILE__, __LINE__, TEXT("")               ) || UE4Asserts_Private::OptionallyDebugBreakAndPromptForRemoteReturningFalse(UE4Asserts_Private::TrueOnFirstCallOnly([]{}), true))
	#define ensureMsgf(       InExpression, InFormat, ... ) (LIKELY(!!(InExpression)) || FDebug::OptionallyLogFormattedEnsureMessageReturningFalse(UE4Asserts_Private::TrueOnFirstCallOnly([]{}), #InExpression, __FILE__, __LINE__, InFormat, ##__VA_ARGS__) || UE4Asserts_Private::OptionallyDebugBreakAndPromptForRemoteReturningFalse(UE4Asserts_Private::TrueOnFirstCallOnly([]{}), true))
	#define ensureAlways(     InExpression                ) (LIKELY(!!(InExpression)) || FDebug::OptionallyLogFormattedEnsureMessageReturningFalse(true,                                          #InExpression, __FILE__, __LINE__, TEXT("")               ) || FPlatformMisc::DebugBreakAndPromptForRemoteReturningFalse(true))
	#define ensureAlwaysMsgf( InExpression, InFormat, ... ) (LIKELY(!!(InExpression)) || FDebug::OptionallyLogFormattedEnsureMessageReturningFalse(true,                                          #InExpression, __FILE__, __LINE__, InFormat, ##__VA_ARGS__) || FPlatformMisc::DebugBreakAndPromptForRemoteReturningFalse(true))

#else	// DO_CHECK

	#define ensure(           InExpression                ) (!!(InExpression))
	#define ensureMsgf(       InExpression, InFormat, ... ) (!!(InExpression))
	#define ensureAlways(     InExpression                ) (!!(InExpression))
	#define ensureAlwaysMsgf( InExpression, InFormat, ... ) (!!(InExpression))

#endif	// DO_CHECK

namespace UE4Asserts_Private
{
	// A junk function to allow us to use sizeof on a member variable which is potentially a bitfield
	template <typename T>
	bool GetMemberNameCheckedJunk(const T&);
	template <typename T>
	bool GetMemberNameCheckedJunk(const volatile T&);
}

// Returns FName(TEXT("EnumeratorName")), while statically verifying that the enumerator exists in the enum
#define GET_ENUMERATOR_NAME_CHECKED(EnumName, EnumeratorName) \
	((void)sizeof(UE4Asserts_Private::GetMemberNameCheckedJunk(EnumName::EnumeratorName)), FName(TEXT(#EnumeratorName)))

// Returns FName(TEXT("MemberName")), while statically verifying that the member exists in ClassName
#define GET_MEMBER_NAME_CHECKED(ClassName, MemberName) \
	((void)sizeof(UE4Asserts_Private::GetMemberNameCheckedJunk(((ClassName*)0)->MemberName)), FName(TEXT(#MemberName)))

#define GET_MEMBER_NAME_STRING_CHECKED(ClassName, MemberName) \
	((void)sizeof(UE4Asserts_Private::GetMemberNameCheckedJunk(((ClassName*)0)->MemberName)), TEXT(#MemberName))

// Returns FName(TEXT("FunctionName")), while statically verifying that the function exists in ClassName
#define GET_FUNCTION_NAME_CHECKED(ClassName, FunctionName) \
	((void)sizeof(&ClassName::FunctionName), FName(TEXT(#FunctionName)))

#define GET_FUNCTION_NAME_STRING_CHECKED(ClassName, FunctionName) \
	((void)sizeof(&ClassName::FunctionName), TEXT(#FunctionName))

/*----------------------------------------------------------------------------
	Low level error macros
----------------------------------------------------------------------------*/

struct FTCharArrayTester
{
	template <uint32 N>
	static char (&Func(const TCHAR(&)[N]))[2];
	static char (&Func(...))[1];
};

#define IS_TCHAR_ARRAY(expr) (sizeof(FTCharArrayTester::Func(expr)) == 2)

/** low level fatal error handler. */
CORE_API void VARARGS LowLevelFatalErrorHandler(const ANSICHAR* File, int32 Line, const TCHAR* Format=TEXT(""), ... );

#define LowLevelFatalError(Format, ...) \
	{ \
		static_assert(IS_TCHAR_ARRAY(Format), "Formatting string must be a TCHAR array."); \
		LowLevelFatalErrorHandler(__FILE__, __LINE__, Format, ##__VA_ARGS__); \
		_DebugBreakAndPromptForRemote(); \
		FDebug::AssertFailed("", __FILE__, __LINE__, Format, ##__VA_ARGS__); \
	}

