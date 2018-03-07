// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

// Re-enable warnings
THIRD_PARTY_INCLUDES_END

// Undo any Windows defines.
#undef uint8
#undef uint16
#undef uint32
#undef int32
#undef float
#undef CDECL
#undef PF_MAX
#undef PlaySound
#undef DrawText
#undef CaptureStackBackTrace
#undef MemoryBarrier
#undef DeleteFile
#undef MoveFile
#undef CopyFile
#undef CreateDirectory
#undef GetCurrentTime
#undef SendMessage
#undef LoadString
#undef UpdateResource
#undef FindWindow
#undef GetObject
#undef GetEnvironmentVariable
#undef CreateFont
#undef CreateDesktop
#undef GetMessage
#undef GetCommandLine

// Undefine all the atomics. AllowWindowsPlatformAtomics/HideWindowsPlatformAtomics temporarily defining these macros.
#if !PLATFORM_XBOXONE
	#undef InterlockedIncrement
	#undef InterlockedDecrement
	#undef InterlockedAdd
	#undef InterlockedExchange
	#undef InterlockedExchangeAdd
	#undef InterlockedCompareExchange
	#undef InterlockedCompareExchangePointer
	#undef InterlockedExchange64
	#undef InterlockedExchangeAdd64
	#undef InterlockedCompareExchange64
	#undef InterlockedIncrement64
	#undef InterlockedDecrement64
#endif

// Restore any previously defined macros
#pragma pop_macro("MAX_uint8")
#pragma pop_macro("MAX_uint16")
#pragma pop_macro("MAX_uint32")
#pragma pop_macro("MAX_int32")
#pragma pop_macro("TEXT")

// Redefine CDECL to our version of the #define.  <AJS> Is this really necessary?
#define CDECL	    __cdecl					/* Standard C function */

// Make sure version is high enough for API to be defined. For CRITICAL_SECTION
#if !defined(_XTL_) && (_WIN32_WINNT < 0x0403)
	#error SetCriticalSectionSpinCount requires _WIN32_WINNT >= 0x0403
#endif
