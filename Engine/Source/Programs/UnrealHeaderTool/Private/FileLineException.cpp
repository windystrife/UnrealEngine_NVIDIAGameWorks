// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "FileLineException.h"
#include "UnrealHeaderTool.h"

void VARARGS FFileLineException::ThrowfImpl(FString&& InFilename, int32 InLine, const TCHAR* Fmt, ...)
{
	int32	BufferSize	= 512;
	TCHAR	StartingBuffer[512];
	TCHAR*	Buffer		= StartingBuffer;
	int32	Result		= -1;

	// First try to print to a stack allocated location 
	GET_VARARGS_RESULT( Buffer, BufferSize, BufferSize-1, Fmt, Fmt, Result );

	// If that fails, start allocating regular memory
	if( Result == -1 )
	{
		Buffer = nullptr;
		while(Result == -1)
		{
			BufferSize *= 2;
			Buffer = (TCHAR*) FMemory::Realloc( Buffer, BufferSize * sizeof(TCHAR) );
			GET_VARARGS_RESULT( Buffer, BufferSize, BufferSize-1, Fmt, Fmt, Result );
		}
	}

	Buffer[Result] = 0;

	FString ResultString(Buffer);

	if( BufferSize != 512 )
	{
		FMemory::Free( Buffer );
	}

	throw FFileLineException(MoveTemp(ResultString), MoveTemp(InFilename), InLine);
}

FFileLineException::FFileLineException(FString&& InMessage, FString&& InFilename, int32 InLine)
	: Message (MoveTemp(InMessage))
	, Filename(MoveTemp(InFilename))
	, Line    (InLine)
{
}
