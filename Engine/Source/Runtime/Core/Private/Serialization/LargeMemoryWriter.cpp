// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Serialization/LargeMemoryWriter.h"
#include "Logging/LogMacros.h"
#include "CoreGlobals.h"

/*----------------------------------------------------------------------------
	FLargeMemoryWriter
----------------------------------------------------------------------------*/

FLargeMemoryWriter::FLargeMemoryWriter(const int64 PreAllocateBytes, bool bIsPersistent, const FName InArchiveName)
	: FMemoryArchive()
	, Data(nullptr)
	, NumBytes(0)
	, MaxBytes(0)
	, ArchiveName(InArchiveName)
{
	ArIsSaving = true;
	ArIsPersistent = bIsPersistent;
	GrowBuffer(PreAllocateBytes);
}

void FLargeMemoryWriter::Serialize(void* InData, int64 Num)
{
	UE_CLOG(!Data, LogSerialization, Fatal, TEXT("Tried to serialize data to an FLargeMemoryWriter that was already released. Archive name: %s."), *ArchiveName.ToString());
	
	const int64 NumBytesToAdd = Offset + Num - NumBytes;
	if (NumBytesToAdd > 0)
	{
		const int64 NewByteCount = NumBytes + NumBytesToAdd;
		if (NewByteCount > MaxBytes)
		{
			GrowBuffer(NewByteCount);
		}

		NumBytes = NewByteCount;
	}

	check((Offset + Num) <= NumBytes);

	if (Num)
	{
		FMemory::Memcpy(&Data[Offset], InData, Num);
		Offset += Num;
	}
}

FString FLargeMemoryWriter::GetArchiveName() const
{
	return ArchiveName.ToString();
}

int64 FLargeMemoryWriter::TotalSize()
{
	return NumBytes;
}

uint8* FLargeMemoryWriter::GetData() const
{
	UE_CLOG(!Data, LogSerialization, Warning, TEXT("Tried to get written data from an FLargeMemoryWriter that was already released. Archive name: %s."), *ArchiveName.ToString());

	return Data;
}

void FLargeMemoryWriter::ReleaseOwnership()
{
	Data = nullptr;
	NumBytes = 0;
	MaxBytes = 0;
}

FLargeMemoryWriter::~FLargeMemoryWriter()
{
	if (Data)
	{
		FMemory::Free(Data);
	}
}

void FLargeMemoryWriter::GrowBuffer(const int64 DesiredBytes)
{
	int64 NewBytes = 64 * 1024; // Initial alloc size

	if (MaxBytes || DesiredBytes > NewBytes)
	{
		// Allocate slack proportional to the buffer size
		NewBytes = FMemory::QuantizeSize(DesiredBytes + 3 * DesiredBytes / 8 + 16);
	}

	if (Data)
	{
		Data = (uint8*)FMemory::Realloc(Data, NewBytes);
	}
	else
	{
		Data = (uint8*)FMemory::Malloc(NewBytes);
	}

	MaxBytes = NewBytes;
}

