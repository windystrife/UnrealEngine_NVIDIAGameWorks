// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/VarArgs.h"
#include "HAL/UnrealMemory.h"
#include "Templates/UnrealTemplate.h"

// Pulled the two FOutputDevice::Logf functions into shared code. Needs to be a #define
// since it uses GET_VARARGS_RESULT which uses the va_list stuff which operates on the
// current function, so we can't easily call a function
#define GROWABLE_LOGF(SerializeFunc) \
	int32	BufferSize	= 1024; \
	TCHAR*	Buffer		= NULL; \
	int32	Result		= -1; \
	/* allocate some stack space to use on the first pass, which matches most strings */ \
	TCHAR	StackBuffer[512]; \
	TCHAR*	AllocatedBuffer = NULL; \
\
	/* first, try using the stack buffer */ \
	Buffer = StackBuffer; \
	GET_VARARGS_RESULT( Buffer, ARRAY_COUNT(StackBuffer), ARRAY_COUNT(StackBuffer) - 1, Fmt, Fmt, Result ); \
\
	/* if that fails, then use heap allocation to make enough space */ \
	while(Result == -1) \
	{ \
		FMemory::SystemFree(AllocatedBuffer); \
		/* We need to use malloc here directly as GMalloc might not be safe. */ \
		Buffer = AllocatedBuffer = (TCHAR*) FMemory::SystemMalloc( BufferSize * sizeof(TCHAR) ); \
		GET_VARARGS_RESULT( Buffer, BufferSize, BufferSize-1, Fmt, Fmt, Result ); \
		BufferSize *= 2; \
	}; \
	Buffer[Result] = 0; \
	; \
\
	SerializeFunc; \
	FMemory::SystemFree(AllocatedBuffer);

