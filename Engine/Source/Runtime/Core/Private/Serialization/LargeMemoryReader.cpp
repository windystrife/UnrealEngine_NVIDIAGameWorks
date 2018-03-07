// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Serialization/LargeMemoryReader.h"
#include "Logging/LogMacros.h"
#include "CoreGlobals.h"

/*----------------------------------------------------------------------------
	FLargeMemoryReader
----------------------------------------------------------------------------*/

FLargeMemoryReader::FLargeMemoryReader(const uint8* InData, const int64 Num, ELargeMemoryReaderFlags InFlags, const FName InArchiveName)
	: FMemoryArchive()
	, bFreeOnClose((InFlags & ELargeMemoryReaderFlags::TakeOwnership) != ELargeMemoryReaderFlags::None)
	, Data(InData)
	, NumBytes(Num)
	, ArchiveName(InArchiveName)
{
	UE_CLOG(!(InData && Num > 0), LogSerialization, Fatal, TEXT("Tried to initialize an FLargeMemoryReader with a null or empty buffer. Archive name: %s."), *ArchiveName.ToString());
	ArIsLoading = true;
	ArIsPersistent = (InFlags & ELargeMemoryReaderFlags::Persistent) != ELargeMemoryReaderFlags::None;
}

void FLargeMemoryReader::Serialize(void* OutData, int64 Num)
{
	if (Num && !ArIsError)
	{
		// Only serialize if we have the requested amount of data
		if (Offset + Num <= NumBytes)
		{
			FMemory::Memcpy(OutData, &Data[Offset], Num);
			Offset += Num;
		}
		else
		{
			ArIsError = true;
		}
	}
}

int64 FLargeMemoryReader::TotalSize()
{
	return NumBytes;
}

FString FLargeMemoryReader::GetArchiveName() const
{
	return ArchiveName.ToString();
}

FLargeMemoryReader::~FLargeMemoryReader()
{
	if (bFreeOnClose)
	{
		FMemory::Free((void*)Data);
	}
}
