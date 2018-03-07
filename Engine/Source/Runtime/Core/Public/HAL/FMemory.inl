// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#if !defined(FMEMORY_INLINE_FUNCTION_DECORATOR)
	#define FMEMORY_INLINE_FUNCTION_DECORATOR 
#endif

#if !defined(FMEMORY_INLINE_GMalloc)
	#define FMEMORY_INLINE_GMalloc GMalloc
#endif

#include "HAL/LowLevelMemTracker.h"

struct FMemory;
struct FScopedMallocTimer;

FMEMORY_INLINE_FUNCTION_DECORATOR void* FMemory::Malloc(SIZE_T Count, uint32 Alignment)
{
	void* Ptr;
	if (!FMEMORY_INLINE_GMalloc)
	{
		Ptr = MallocExternal(Count, Alignment);
	}
	else
	{
		DoGamethreadHook(0);
		FScopedMallocTimer Timer(0);
		Ptr = FMEMORY_INLINE_GMalloc->Malloc(Count, Alignment);
	}
	// optional tracking of every allocation
	LLM(FLowLevelMemTracker::Get().OnLowLevelAlloc(ELLMTracker::Default, Ptr, Count));
	return Ptr;
}

FMEMORY_INLINE_FUNCTION_DECORATOR void* FMemory::Realloc(void* Original, SIZE_T Count, uint32 Alignment)
{
	// optional tracking of every allocation
	LLM(FLowLevelMemTracker::Get().OnLowLevelFree(ELLMTracker::Default, Original, 0));

	void* Ptr;
	if (!FMEMORY_INLINE_GMalloc)
	{
		Ptr = ReallocExternal(Original, Count, Alignment);
	}
	else
	{
		DoGamethreadHook(1);
		FScopedMallocTimer Timer(1);
		Ptr = FMEMORY_INLINE_GMalloc->Realloc(Original, Count, Alignment);
	}

	// optional tracking of every allocation
	LLM(FLowLevelMemTracker::Get().OnLowLevelAlloc(ELLMTracker::Default, Ptr, Count));

	return Ptr;
}

FMEMORY_INLINE_FUNCTION_DECORATOR void FMemory::Free(void* Original)
{
	if (!Original)
	{
		FScopedMallocTimer Timer(3);
		return;
	}

	// optional tracking of every allocation
	LLM(FLowLevelMemTracker::Get().OnLowLevelFree(ELLMTracker::Default, Original, 0));

	if (!FMEMORY_INLINE_GMalloc)
	{
		FreeExternal(Original);
		return;
	}
	DoGamethreadHook(2);
	FScopedMallocTimer Timer(2);
	FMEMORY_INLINE_GMalloc->Free(Original);
}

FMEMORY_INLINE_FUNCTION_DECORATOR SIZE_T FMemory::GetAllocSize(void* Original)
{
	if (!FMEMORY_INLINE_GMalloc)
	{
		return GetAllocSizeExternal(Original);
	}

	SIZE_T Size = 0;
	return FMEMORY_INLINE_GMalloc->GetAllocationSize(Original, Size) ? Size : 0;
}

FMEMORY_INLINE_FUNCTION_DECORATOR SIZE_T FMemory::QuantizeSize(SIZE_T Count, uint32 Alignment)
{
	if (!FMEMORY_INLINE_GMalloc)
	{
		return Count;
	}
	return FMEMORY_INLINE_GMalloc->QuantizeSize(Count, Alignment);
}

