// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Serialization/MemoryWriter.h"
#include "Internationalization/TextPackageNamespaceUtil.h"
#include "UObject/PropertyPortFlags.h"

struct FLazyObjectPtr;
struct FSoftObjectPtr;
struct FSoftObjectPath;
struct FWeakObjectPtr;

/**
 * UObject Memory Writer Archive.
 */
class FObjectWriter : public FMemoryWriter
{

public:
	FObjectWriter(UObject* Obj, TArray<uint8>& InBytes, bool bIgnoreClassRef = false, bool bIgnoreArchetypeRef = false, bool bDoDelta = true, uint32 AdditionalPortFlags = 0)
		: FMemoryWriter(InBytes)
	{
		ArIgnoreClassRef = bIgnoreClassRef;
		ArIgnoreArchetypeRef = bIgnoreArchetypeRef;
		ArNoDelta = !bDoDelta;
		ArPortFlags |= AdditionalPortFlags;

#if USE_STABLE_LOCALIZATION_KEYS
		if (GIsEditor && !(ArPortFlags & (PPF_DuplicateVerbatim | PPF_DuplicateForPIE)))
		{
			SetLocalizationNamespace(TextNamespaceUtil::EnsurePackageNamespace(Obj));
		}
#endif // USE_STABLE_LOCALIZATION_KEYS

		Obj->Serialize(*this);
	}

	//~ Begin FArchive Interface
	COREUOBJECT_API virtual FArchive& operator<<(FName& N) override;
	COREUOBJECT_API virtual FArchive& operator<<(UObject*& Res) override;
	COREUOBJECT_API virtual FArchive& operator<<(FLazyObjectPtr& Value) override;
	COREUOBJECT_API virtual FArchive& operator<<(FSoftObjectPtr& Value) override;
	COREUOBJECT_API virtual FArchive& operator<<(FSoftObjectPath& Value) override;
	COREUOBJECT_API virtual FArchive& operator<<(FWeakObjectPtr& Value) override;
	COREUOBJECT_API virtual FString GetArchiveName() const override;
	//~ End FArchive Interface

	FObjectWriter(TArray<uint8>& InBytes)
		: FMemoryWriter(InBytes)
	{
		ArIgnoreClassRef = false;
		ArIgnoreArchetypeRef = false;
	}
};
