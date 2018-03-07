// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * true we want to force an ansi allocator
 */
#ifndef FORCE_ANSI_ALLOCATOR
	#define FORCE_ANSI_ALLOCATOR 0
#endif

/**
 * true if intelTBB (a.k.a MallocTBB) can be used (different to the platform actually supporting it)
 */
#ifndef TBB_ALLOCATOR_ALLOWED
	#define TBB_ALLOCATOR_ALLOWED 1
#endif

/**
 *	IMPORTANT:	The malloc proxy flags are mutually exclusive.
 *				You can have either none of them set or only one of them.
 */
/** USE_MALLOC_PROFILER				- Define this to use the FMallocProfiler allocator.			*/
/**									  Make sure to enable Stack Frame pointers:					*/
/**									  bOmitFramePointers = false, or /Oy-						*/

#ifndef USE_MALLOC_PROFILER
#define USE_MALLOC_PROFILER				0
#endif

#if USE_MALLOC_PROFILER
#define MALLOC_PROFILER(x)	x	
#else
#define MALLOC_PROFILER(...)
#endif


