// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/UnrealString.h"
#include "UObject/NameTypes.h"
#include "Serialization/MemoryArchive.h"

/**
* Archive for storing a large amount of arbitrary data to memory
*/
class CORE_API FLargeMemoryWriter : public FMemoryArchive
{
public:
	
	FLargeMemoryWriter(const int64 PreAllocateBytes = 0, bool bIsPersistent = false, const FName InArchiveName = NAME_None);

	virtual void Serialize(void* InData, int64 Num) override;

	/**
	* Returns the name of the Archive.  Useful for getting the name of the package a struct or object
	* is in when a loading error occurs.
	*
	* This is overridden for the specific Archive Types
	**/
	virtual FString GetArchiveName() const override;

	/**
	 * Gets the total size of the data written
	 */
	virtual int64 TotalSize() override;
	
	/**
	 * Returns the written data. To release this archive's ownership of the data, call ReleaseOwnership()
	 */
	uint8* GetData() const;

	/** 
	 * Releases ownership of the written data
	 */
	void ReleaseOwnership();
	
	/**
	 * Destructor
	 */
	~FLargeMemoryWriter();

private:

	/** Non-copyable */
	FLargeMemoryWriter(const FLargeMemoryWriter&) = delete;
	FLargeMemoryWriter& operator=(const FLargeMemoryWriter&) = delete;

	/** Memory owned by this archive. Ownership can be released by calling ReleaseOwnership() */
	uint8* Data;

	/** Number of bytes currently written to our data buffer */
	int64 NumBytes;

	/** Number of bytes currently allocated for our data buffer */
	int64 MaxBytes;

	/** Resizes the data buffer to at least the desired new size with some slack */
	void GrowBuffer(const int64 DesiredBytes);

	/** Archive name, used for debugging, by default set to NAME_None. */
	const FName ArchiveName;
};

