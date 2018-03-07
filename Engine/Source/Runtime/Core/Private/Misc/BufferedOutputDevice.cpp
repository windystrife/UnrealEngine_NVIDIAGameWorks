// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Misc/BufferedOutputDevice.h"
#include "Misc/ScopeLock.h"

void FBufferedOutputDevice::Serialize(const TCHAR* InData, ELogVerbosity::Type Verbosity, const class FName& Category)
{
	if (Verbosity > FilterLevel)
		return;

	FScopeLock ScopeLock(&SynchronizationObject);
	new(BufferedLines)FBufferedLine(InData, Category, Verbosity);
}

void FBufferedOutputDevice::GetContents(TArray<FBufferedLine>& DestBuffer, bool ClearDevice/*=true*/)
{
	FScopeLock ScopeLock(&SynchronizationObject);
	DestBuffer = BufferedLines;

	if (ClearDevice)
	{
		BufferedLines.Empty();
	}
}
