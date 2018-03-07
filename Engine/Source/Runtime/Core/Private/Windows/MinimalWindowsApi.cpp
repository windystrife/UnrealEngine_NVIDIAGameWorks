// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Windows/MinimalWindowsApi.h"
#include "Windows/WindowsHWrapper.h"

// Check that constants are what we expect them to be
static_assert(WINDOWS_MAX_PATH == MAX_PATH, "Value of WINDOWSAPI_MAX_PATH is not correct");
static_assert(WINDOWS_PF_COMPARE_EXCHANGE128 == PF_COMPARE_EXCHANGE128, "Value of WINDOWS_PF_COMPARE_EXCHANGE128 is not correct");

// Check that AllocTlsSlot() returns INDEX_NONE on failure
static_assert(static_cast<uint32>(INDEX_NONE) == TLS_OUT_OF_INDEXES, "TLS_OUT_OF_INDEXES is different from INDEX_NONE, change FWindowsPlatformTLS::AllocTlsSlot() implementation.");

// Check the size and alignment of the OVERLAPPED mirror
static_assert(sizeof(Windows::OVERLAPPED) == sizeof(OVERLAPPED), "Size of Windows::OVERLAPPED must match definition in Windows.h");
static_assert(__alignof(Windows::OVERLAPPED) == __alignof(OVERLAPPED), "Alignment of Windows::OVERLAPPED must match definition in Windows.h");

// Check the size and alignment of the CRITICAL_SECTION mirror
static_assert(sizeof(Windows::CRITICAL_SECTION) == sizeof(CRITICAL_SECTION), "Size of Windows::CRITICAL_SECTION must match definition in Windows.h");
static_assert(__alignof(Windows::CRITICAL_SECTION) == __alignof(CRITICAL_SECTION), "Alignment of Windows::CRITICAL_SECTION must match definition in Windows.h");

// Check the size and alignment of the LARGE_INTEGER mirror
static_assert(sizeof(Windows::LARGE_INTEGER) == sizeof(LARGE_INTEGER), "Size of Windows::LARGE_INTEGER must match definition in Windows.h");
static_assert(__alignof(Windows::LARGE_INTEGER) == __alignof(LARGE_INTEGER), "Alignment of Windows::LARGE_INTEGER must match definition in Windows.h");
