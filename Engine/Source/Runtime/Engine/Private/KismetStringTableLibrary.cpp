// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Kismet/KismetStringTableLibrary.h"
#include "Internationalization/StringTableCore.h"
#include "Internationalization/StringTableRegistry.h"

#define LOCTEXT_NAMESPACE "Kismet"

UKismetStringTableLibrary::UKismetStringTableLibrary(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}


bool UKismetStringTableLibrary::IsRegisteredTableId(const FName TableId)
{
	FStringTableConstPtr StringTable = FStringTableRegistry::Get().FindStringTable(TableId);
	return StringTable.IsValid();
}


bool UKismetStringTableLibrary::IsRegisteredTableEntry(const FName TableId, const FString& Key)
{
	FStringTableConstPtr StringTable = FStringTableRegistry::Get().FindStringTable(TableId);
	return StringTable.IsValid() && StringTable->FindEntry(Key).IsValid();
}


FString UKismetStringTableLibrary::GetTableNamespace(const FName TableId)
{
	FStringTableConstPtr StringTable = FStringTableRegistry::Get().FindStringTable(TableId);
	if (StringTable.IsValid())
	{
		return StringTable->GetNamespace();
	}

	return FString();
}


FString UKismetStringTableLibrary::GetTableEntrySourceString(const FName TableId, const FString& Key)
{
	FString SourceString;

	FStringTableConstPtr StringTable = FStringTableRegistry::Get().FindStringTable(TableId);
	if (StringTable.IsValid())
	{
		StringTable->GetSourceString(Key, SourceString);
	}

	return SourceString;
}


FString UKismetStringTableLibrary::GetTableEntryMetaData(const FName TableId, const FString& Key, const FName MetaDataId)
{
	FStringTableConstPtr StringTable = FStringTableRegistry::Get().FindStringTable(TableId);
	if (StringTable.IsValid())
	{
		return StringTable->GetMetaData(Key, MetaDataId);
	}

	return FString();
}


TArray<FName> UKismetStringTableLibrary::GetRegisteredStringTables()
{
	TArray<FName> RegisteredStringTableIds;
	
	FStringTableRegistry::Get().EnumerateStringTables([&](const FName& InTableId, const FStringTableConstRef& InStringTable) -> bool
	{
		RegisteredStringTableIds.Add(InTableId);
		return true; // continue enumeration
	});
	
	return RegisteredStringTableIds;
}


TArray<FString> UKismetStringTableLibrary::GetKeysFromStringTable(const FName TableId)
{
	TArray<FString> KeysFromStringTable;

	FStringTableConstPtr StringTable = FStringTableRegistry::Get().FindStringTable(TableId);
	if (StringTable.IsValid())
	{
		StringTable->EnumerateSourceStrings([&](const FString& InKey, const FString& InSourceString) -> bool
		{
			KeysFromStringTable.Add(InKey);
			return true; // continue enumeration
		});
	}

	return KeysFromStringTable;
}


TArray<FName> UKismetStringTableLibrary::GetMetaDataIdsFromStringTableEntry(const FName TableId, const FString& Key)
{
	TArray<FName> MetaDataIdsFromStringTable;

	FStringTableConstPtr StringTable = FStringTableRegistry::Get().FindStringTable(TableId);
	if (StringTable.IsValid())
	{
		StringTable->EnumerateMetaData(Key, [&](FName InMetaDataId, const FString& InMetaData) -> bool
		{
			MetaDataIdsFromStringTable.Add(InMetaDataId);
			return true; // continue enumeration
		});
	}

	return MetaDataIdsFromStringTable;
}

#undef LOCTEXT_NAMESPACE
