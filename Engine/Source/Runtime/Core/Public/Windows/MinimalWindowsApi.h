// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

//=====================================================================================================================
// Implementation of a minimal subset of the Windows API required for inline function definitions and platform-specific
// interfaces in Core. Use of this file allows avoiding including large platform headers in the public engine API.
//
// Win32 API functions are declared in the "Windows" namespace to avoid conflicts if the real Windows.h is included 
// later, but are mapped to the same imported symbols by the linker due to C-style linkage.
//=====================================================================================================================

#pragma once

#include "CoreTypes.h"

// Use strongly typed handles
#ifndef STRICT
#define STRICT
#endif

// With STRICT enabled, handles are implemented as opaque struct pointers. We can prototype these structs here and 
// typedef them under the Windows namespace below. Since typedefs are only aliases rather than types in their own 
// right, this allows using handles from the Windows:: namespace interchangably with their native definition.
struct HINSTANCE__;
struct HWND__;
struct HKEY__;
struct HDC__;
struct HICON__;

// Other types
struct tagPROCESSENTRY32W;
struct _EXCEPTION_POINTERS;
struct _OVERLAPPED;
struct _RTL_CRITICAL_SECTION;
union _LARGE_INTEGER;

// Global constants
#define WINDOWS_MAX_PATH 260
#define WINDOWS_PF_COMPARE_EXCHANGE128	14

// Standard calling convention for Win32 functions
#define WINAPI __stdcall

// Minimal subset of the Windows API required for interfaces and inline functions
namespace Windows
{
	// Typedefs for basic Windows types
	typedef int32 BOOL;
	typedef unsigned long DWORD;
	typedef DWORD* LPDWORD;
	typedef long LONG;
	typedef long* LPLONG;
	typedef int64 LONGLONG;
	typedef LONGLONG* LPLONGLONG;
	typedef void* LPVOID;
	typedef const void* LPCVOID;
	typedef const wchar_t* LPCTSTR;

	// Typedefs for standard handles
	typedef void* HANDLE;
	typedef HINSTANCE__* HINSTANCE;
	typedef HINSTANCE HMODULE;
	typedef HWND__* HWND;
	typedef HKEY__* HKEY;
	typedef HDC__* HDC;
	typedef HICON__* HICON;
	typedef HICON__* HCURSOR;

	// Other typedefs
	typedef tagPROCESSENTRY32W PROCESSENTRY32;
	typedef _EXCEPTION_POINTERS* LPEXCEPTION_POINTERS;
	typedef _RTL_CRITICAL_SECTION* LPCRITICAL_SECTION;
	typedef _OVERLAPPED* LPOVERLAPPED;
	typedef _LARGE_INTEGER* LPLARGE_INTEGER;

	typedef struct _RTL_SRWLOCK
	{
		void* Ptr;
	} RTL_SRWLOCK, *PRTL_SRWLOCK;

	typedef RTL_SRWLOCK SRWLOCK, *PSRWLOCK;

	// Modules
	extern "C" __declspec(dllimport) HMODULE WINAPI LoadLibraryW(LPCTSTR lpFileName);
	extern "C" __declspec(dllimport) BOOL WINAPI FreeLibrary(HMODULE hModule);

	// Critical sections
	extern "C" __declspec(dllimport) void WINAPI InitializeCriticalSection(LPCRITICAL_SECTION lpCriticalSection);
	extern "C" __declspec(dllimport) BOOL WINAPI InitializeCriticalSectionAndSpinCount(LPCRITICAL_SECTION lpCriticalSection, DWORD dwSpinCount);
	extern "C" __declspec(dllimport) DWORD WINAPI SetCriticalSectionSpinCount(LPCRITICAL_SECTION lpCriticalSection, DWORD dwSpinCount);
	extern "C" __declspec(dllimport) BOOL WINAPI TryEnterCriticalSection(LPCRITICAL_SECTION lpCriticalSection);
	extern "C" __declspec(dllimport) void WINAPI EnterCriticalSection(LPCRITICAL_SECTION lpCriticalSection);
	extern "C" __declspec(dllimport) void WINAPI LeaveCriticalSection(LPCRITICAL_SECTION lpCriticalSection);
	extern "C" __declspec(dllimport) void WINAPI DeleteCriticalSection(LPCRITICAL_SECTION lpCriticalSection);

	extern "C" __declspec(dllimport) void WINAPI InitializeSRWLock(PSRWLOCK SRWLock);
	extern "C" __declspec(dllimport) void WINAPI AcquireSRWLockShared(PSRWLOCK SRWLock);
	extern "C" __declspec(dllimport) void WINAPI ReleaseSRWLockShared(PSRWLOCK SRWLock);
	extern "C" __declspec(dllimport) void WINAPI AcquireSRWLockExclusive(PSRWLOCK SRWLock);
	extern "C" __declspec(dllimport) void WINAPI ReleaseSRWLockExclusive(PSRWLOCK SRWLock);

	// I/O
	extern "C" __declspec(dllimport) BOOL WINAPI ConnectNamedPipe(HANDLE hNamedPipe, LPOVERLAPPED lpOverlapped);
	extern "C" __declspec(dllimport) BOOL WINAPI GetOverlappedResult(HANDLE hFile, LPOVERLAPPED lpOverlapped, LPDWORD lpNumberOfBytesTransferred, BOOL bWait);
	extern "C" __declspec(dllimport) BOOL WINAPI WriteFile(HANDLE hFile, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite, LPDWORD lpNumberOfBytesWritten, LPOVERLAPPED lpOverlapped);
	extern "C" __declspec(dllimport) BOOL WINAPI ReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead, LPDWORD lpNumberOfBytesRead, LPOVERLAPPED lpOverlapped);

	// Timing
	extern "C" __declspec(dllimport) BOOL WINAPI QueryPerformanceCounter(LPLARGE_INTEGER Cycles);

	// Thread-local storage functions
	extern "C" __declspec(dllimport) DWORD WINAPI GetCurrentThreadId();
	extern "C" __declspec(dllimport) DWORD WINAPI TlsAlloc();
	extern "C" __declspec(dllimport) LPVOID WINAPI TlsGetValue(DWORD dwTlsIndex);
	extern "C" __declspec(dllimport) BOOL WINAPI TlsSetValue(DWORD dwTlsIndex, LPVOID lpTlsValue);
	extern "C" __declspec(dllimport) BOOL WINAPI TlsFree(DWORD dwTlsIndex);

	// System
	extern "C" __declspec(dllimport) BOOL WINAPI IsProcessorFeaturePresent(DWORD ProcessorFeature);

	// For structures which are opaque
	struct CRITICAL_SECTION { void* Opaque1[1]; long Opaque2[2]; void* Opaque3[3]; };
	struct OVERLAPPED { void *Opaque[3]; unsigned long Opaque2[2]; };
	union LARGE_INTEGER { struct { DWORD LowPart; LONG HighPart; }; LONGLONG QuadPart; };

	FORCEINLINE BOOL WINAPI ConnectNamedPipe(HANDLE hNamedPipe, OVERLAPPED* lpOverlapped)
	{
		return ConnectNamedPipe(hNamedPipe, (LPOVERLAPPED)lpOverlapped);
	}

	FORCEINLINE BOOL WINAPI GetOverlappedResult(HANDLE hFile, OVERLAPPED* lpOverlapped, LPDWORD lpNumberOfBytesTransferred, BOOL bWait)
	{
		return GetOverlappedResult(hFile, (LPOVERLAPPED)lpOverlapped, lpNumberOfBytesTransferred, bWait);
	}

	FORCEINLINE BOOL WINAPI WriteFile(HANDLE hFile, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite, LPDWORD lpNumberOfBytesWritten, OVERLAPPED* lpOverlapped)
	{
		return WriteFile(hFile, lpBuffer, nNumberOfBytesToWrite, lpNumberOfBytesWritten, (LPOVERLAPPED)lpOverlapped);
	}

	FORCEINLINE BOOL WINAPI ReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead, LPDWORD lpNumberOfBytesRead, OVERLAPPED* lpOverlapped)
	{
		return ReadFile(hFile, lpBuffer, nNumberOfBytesToRead, lpNumberOfBytesRead, (LPOVERLAPPED)lpOverlapped);
	}

	FORCEINLINE void WINAPI InitializeCriticalSection(CRITICAL_SECTION* lpCriticalSection)
	{
		InitializeCriticalSection((LPCRITICAL_SECTION)lpCriticalSection);
	}

	FORCEINLINE BOOL WINAPI InitializeCriticalSectionAndSpinCount(CRITICAL_SECTION* lpCriticalSection, DWORD dwSpinCount)
	{
		return InitializeCriticalSectionAndSpinCount((LPCRITICAL_SECTION)lpCriticalSection, dwSpinCount);
	}

	FORCEINLINE DWORD WINAPI SetCriticalSectionSpinCount(CRITICAL_SECTION* lpCriticalSection, DWORD dwSpinCount)
	{
		return SetCriticalSectionSpinCount((LPCRITICAL_SECTION)lpCriticalSection, dwSpinCount);
	}

	FORCEINLINE BOOL WINAPI TryEnterCriticalSection(CRITICAL_SECTION* lpCriticalSection)
	{
		return TryEnterCriticalSection((LPCRITICAL_SECTION)lpCriticalSection);
	}

	FORCEINLINE void WINAPI EnterCriticalSection(CRITICAL_SECTION* lpCriticalSection)
	{
		EnterCriticalSection((LPCRITICAL_SECTION)lpCriticalSection);
	}

	FORCEINLINE void WINAPI LeaveCriticalSection(CRITICAL_SECTION* lpCriticalSection)
	{
		LeaveCriticalSection((LPCRITICAL_SECTION)lpCriticalSection);
	}

	FORCEINLINE void WINAPI DeleteCriticalSection(CRITICAL_SECTION* lpCriticalSection)
	{
		DeleteCriticalSection((LPCRITICAL_SECTION)lpCriticalSection);
	}

	FORCEINLINE BOOL WINAPI QueryPerformanceCounter(LARGE_INTEGER* Cycles)
	{
		return QueryPerformanceCounter((LPLARGE_INTEGER)Cycles);
	}
}
