// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "WindowsHWrapper.h"

#ifndef WINDOWS_PLATFORM_TYPES_GUARD
	#define WINDOWS_PLATFORM_TYPES_GUARD
#else
	#error Nesting AllowWindowsPLatformTypes.h is not allowed!
#endif

#pragma warning( push )
#pragma warning( disable : 4459 )

#define INT ::INT
#define UINT ::UINT
#define DWORD ::DWORD
#define FLOAT ::FLOAT

#define TRUE 1
#define FALSE 0
