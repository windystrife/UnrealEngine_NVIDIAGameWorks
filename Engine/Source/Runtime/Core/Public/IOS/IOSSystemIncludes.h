// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

// Set up compiler pragmas, etc
#include "CoreTypes.h"
#include "IOS/IOSPlatformCompilerSetup.h"

#define FVector FVectorWorkaround
#ifdef __OBJC__
#import <UIKit/UIKit.h>
#import <CoreData/CoreData.h>
#else //@todo - zombie - iOS should always have Obj-C to function. This definition guard may be unnecessary. -astarkey
#error "Objective C is undefined."
#endif
#undef check
#undef verify
#undef FVector

#undef TYPE_BOOL

#include <CoreFoundation/CoreFoundation.h>

#include <string.h>
#include <alloca.h>
#include <stdlib.h>
#include <stdio.h>
#include <float.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdint.h>
#include <limits.h>
#include <sys/time.h>
#include <math.h>
#include <mach/mach_time.h>
#include <wchar.h>
#include <wctype.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <libkern/OSAtomic.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>
#include <fcntl.h>
#include <dirent.h>
#include <dlfcn.h>
#include <copyfile.h>
#include <utime.h>
#include <mach/mach.h>
#include <mach/mach_host.h>
#include <execinfo.h>
#include <signal.h>
#include <sys/sysctl.h>
#include <malloc/malloc.h>

// SIMD intrinsics
#if WITH_SIMULATOR
#include <xmmintrin.h>
#else
#include <arm_neon.h>
#endif

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

/*----------------------------------------------------------------------------
 Memory. On Mac OS X malloc allocates memory aligned to 16 bytes.
 ----------------------------------------------------------------------------*/
#define _aligned_malloc(Size,Align) malloc(Size)
#define _aligned_realloc(Ptr,Size,Align) realloc(Ptr,Size)
#define _aligned_free(Ptr) free(Ptr)
#define _aligned_msize(Ptr,Align,Offset) malloc_size(Ptr)
