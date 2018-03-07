// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * Parses a semantic in to its base name and index.
 * @param MemContext - Memory context with which allocations can be made.
 * @param InSemantic - The semantic to parse.
 * @param OutSemantic - Upon return contains the base semantic.
 * @param OutIndex - Upon return contains the semantic index.
 */
void ParseSemanticAndIndex(void* MemContext,const char* InSemantic,const char** OutSemantic,int* OutIndex);

/**
* Moves any instructions in the global instruction stream to the beginning
* of main. This can happen due to conversions and initializers of global
* variables. Note however that instructions can be moved iff main() is the
* only function in the program!
*/
bool MoveGlobalInstructionsToMain(struct exec_list* Instructions);

/**
 * For debug output in visual studio.
 */
#if defined(_MSC_VER)
void dprintf(const char* Format, ...);
#else
#ifndef __APPLE__
// Make sure we include stdio.h before defined dprintf away, otherwise
// the dprintf() declaration in glibc's stdio.h ends up declaring a
// conflicting version of printf.
#define dprintf printf
#endif
#endif

#ifdef WIN32
#ifdef ENABLE_TIMING
#include <windows.h>
inline long long GetTimeInMiliseconds()
{
	static LARGE_INTEGER s_frequency;
	static BOOL s_use_qpc = QueryPerformanceFrequency(&s_frequency);
	if (s_use_qpc)
	{
		LARGE_INTEGER now;
		QueryPerformanceCounter(&now);
		return (1000LL * now.QuadPart) / s_frequency.QuadPart;
	}
	else
	{
		return GetTickCount();
	}
}
#endif	// WIN32
#endif	// ENABLE_TIMING

