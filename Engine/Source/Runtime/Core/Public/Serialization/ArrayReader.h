// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "HAL/UnrealMemory.h"
#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "Serialization/MemoryArchive.h"

class FArrayReader final : public FMemoryArchive, public TArray<uint8>
{
public:
	FArrayReader( bool bIsPersistent=false )
	{
		ArIsLoading = true;
		ArIsPersistent = bIsPersistent;
	}

	/**
	* Returns the name of the Archive.  Useful for getting the name of the package a struct or object
	* is in when a loading error occurs.
	*
	* This is overridden for the specific Archive Types
	**/
	virtual FString GetArchiveName() const { return TEXT("FArrayReader"); }

	int64 TotalSize()
	{
		return (int64)Num();
	}

	void Serialize(void* Data, int64 Count)
	{
		if (Count && !ArIsError)
		{
			// Only serialize if we have the requested amount of data
			if (Offset + Count <= Num())
			{
				FMemory::Memcpy(Data, &((*this)[Offset]), Count);
				Offset += Count;
			}
			else
			{
				ArIsError = true;
			}
		}
	}
};
