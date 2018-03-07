// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/ContainerAllocationPolicies.h"
#include "Misc/Crc.h"
#include "Containers/UnrealString.h"
#include "Containers/Set.h"
#include "Containers/Map.h"
#include "Templates/SharedPointer.h"
#include "Internationalization/InternationalizationManifest.h"
#include "LocKeyFuncs.h"

class FLocMetadataObject;

class CORE_API FArchiveEntry
{
public:
	FArchiveEntry(const FString& InNamespace, const FString& InKey, const FLocItem& InSource, const FLocItem& InTranslation, TSharedPtr<FLocMetadataObject> InKeyMetadataObj = nullptr, bool IsOptional = false);

	const FString Namespace;
	const FString Key;
	const FLocItem Source;
	FLocItem Translation;
	bool bIsOptional;
	TSharedPtr<FLocMetadataObject> KeyMetadataObj;
};

typedef TMultiMap< FString, TSharedRef< FArchiveEntry >, FDefaultSetAllocator, FLocKeyMultiMapFuncs< TSharedRef< FArchiveEntry > > > FArchiveEntryByStringContainer;

class CORE_API FInternationalizationArchive 
{
public:
	enum class EFormatVersion : uint8
	{
		Initial = 0,
		EscapeFixes,
		AddedKeys,

		LatestPlusOne,
		Latest = LatestPlusOne - 1
	};

	FInternationalizationArchive()
		: FormatVersion(EFormatVersion::Latest)
	{
	}

	bool AddEntry(const FString& Namespace, const FString& Key, const FLocItem& Source, const FLocItem& Translation, const TSharedPtr<FLocMetadataObject> KeyMetadataObj, const bool bOptional);
	bool AddEntry(const TSharedRef<FArchiveEntry>& InEntry);

	void UpdateEntry(const TSharedRef<FArchiveEntry>& OldEntry, const TSharedRef<FArchiveEntry>& NewEntry);

	bool SetTranslation(const FString& Namespace, const FString& Key, const FLocItem& Source, const FLocItem& Translation, const TSharedPtr<FLocMetadataObject> KeyMetadataObj);

	TSharedPtr<FArchiveEntry> FindEntryByKey(const FString& Namespace, const FString& Key, const TSharedPtr<FLocMetadataObject> KeyMetadataObj) const;

	FArchiveEntryByStringContainer::TConstIterator GetEntriesByKeyIterator() const
	{
		return EntriesByKey.CreateConstIterator();
	}

	int32 GetNumEntriesByKey() const
	{
		return EntriesByKey.Num();
	}

	FArchiveEntryByStringContainer::TConstIterator GetEntriesBySourceTextIterator() const
	{
		return EntriesBySourceText.CreateConstIterator();
	}

	int32 GetNumEntriesBySourceText() const
	{
		return EntriesBySourceText.Num();
	}

	void SetFormatVersion(const EFormatVersion Version)
	{
		FormatVersion = Version;
	}

	EFormatVersion GetFormatVersion() const
	{
		return FormatVersion;
	}

private:
	EFormatVersion FormatVersion;
	FArchiveEntryByStringContainer EntriesBySourceText;
	FArchiveEntryByStringContainer EntriesByKey;
};
