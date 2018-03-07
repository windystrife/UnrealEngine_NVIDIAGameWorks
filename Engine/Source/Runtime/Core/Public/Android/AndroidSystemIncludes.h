// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#include <wctype.h>
#include <pthread.h>
#include <limits.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <utime.h>

// Set up compiler pragmas, etc
#include "Android/AndroidCompilerSetup.h"

//@todo android: verify malloc alignment or change?
#define _aligned_malloc(Size,Align) malloc(Size)
#define _aligned_realloc(Ptr,Size,Align) realloc(Ptr,Size)
#define _aligned_free(Ptr) free(Ptr)

int vswprintf(TCHAR *dst, int count, const TCHAR *fmt, va_list arg);

/*
// Define TEXT early	//@todo android:
#define TEXT(x) L##x

// map the Windows functions (that UE4 unfortunately uses be default) to normal functions
#define _alloca alloca
*/

struct tagRECT
{
	int32 left;
	int32 top;
	int32 right;
	int32 bottom;
};
typedef struct tagRECT RECT;

#define OUT
#define IN