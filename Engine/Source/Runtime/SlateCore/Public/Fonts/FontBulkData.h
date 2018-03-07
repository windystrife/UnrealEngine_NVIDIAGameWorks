// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Serialization/BulkData.h"
#include "FontBulkData.generated.h"

UCLASS()
class SLATECORE_API UFontBulkData : public UObject
{
	GENERATED_BODY()

public:
	/** Default constructor */
	UFontBulkData();

	/** Construct the bulk font data from the given file */
	void Initialize(const FString& InFontFilename);

	/** Construct the bulk font data from the given data */
	void Initialize(const void* const InFontData, const int32 InFontDataSizeBytes);

	/** Locks the bulk font data and returns a read-only pointer to it */
	const void* Lock(int32& OutFontDataSizeBytes) const;

	/** Unlock the bulk font data, after which point the pointer returned by Lock no longer is valid */
	void Unlock() const;

	/** Returns the size of the bulk data in bytes */
	int32 GetBulkDataSize() const;

	/** Returns the size of the bulk data on disk. This can differ from GetBulkDataSize if BULKDATA_SerializeCompressed is set */
	int32 GetBulkDataSizeOnDisk() const;

	// UObject interface
	virtual void Serialize(FArchive& Ar) override;

private:
	/** Internal bulk data */
	FByteBulkData BulkData;

	/** Critical section to prevent concurrent access when locking the internal bulk data */
	mutable FCriticalSection CriticalSection;
};
