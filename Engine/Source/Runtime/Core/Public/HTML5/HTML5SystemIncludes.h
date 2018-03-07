// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#include <wctype.h>
#include <limits.h>

// Set up compiler pragmas, etc
#include "HTML5/HTML5PlatformCompilerSetup.h"

#define _alloca alloca

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