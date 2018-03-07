// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "StringTableCore.h"
#include "Misc/ScopeLock.h"
#include "Misc/FileHelper.h"
#include "Misc/ConfigCacheIni.h"
#include "Serialization/Csv/CsvParser.h"

DEFINE_LOG_CATEGORY(LogStringTable);


IStringTableEngineBridge* IStringTableEngineBridge::InstancePtr = nullptr;


namespace StringTableRedirects
{
	static TMap<FName, FName> TableIdRedirects;
	static TMap<FName, TMap<FString, FString, FDefaultSetAllocator, FLocKeyMapFuncs<FString>>> TableKeyRedirects;
}


FStringTableEntry::FStringTableEntry()
{
}

FStringTableEntry::FStringTableEntry(FStringTableConstRef InOwnerTable, FString InSourceString, FTextDisplayStringPtr InDisplayString)
	: OwnerTable(MoveTemp(InOwnerTable))
	, SourceString(MoveTemp(InSourceString))
	, DisplayString(MoveTemp(InDisplayString))
{
}

bool FStringTableEntry::IsOwned() const
{
	return OwnerTable.IsValid();
}

void FStringTableEntry::Disown()
{
	OwnerTable.Reset();
}

const FString& FStringTableEntry::GetSourceString() const
{
	return SourceString;
}

FTextDisplayStringPtr FStringTableEntry::GetDisplayString() const
{
	return DisplayString;
}


FStringTable::FStringTable()
	: OwnerAsset(nullptr)
	, bIsLoaded(true)
{
}

FStringTable::~FStringTable()
{
	// Make sure our entries are disowned correctly
	ClearSourceStrings();
}

UStringTable* FStringTable::GetOwnerAsset() const
{
	return OwnerAsset;
}

void FStringTable::SetOwnerAsset(UStringTable* InOwnerAsset)
{
	OwnerAsset = InOwnerAsset;
}

bool FStringTable::IsLoaded() const
{
	return bIsLoaded;
}

void FStringTable::IsLoaded(const bool bInIsLoaded)
{
	bIsLoaded = bInIsLoaded;
}

FString FStringTable::GetNamespace() const
{
	return TableNamespace;
}

void FStringTable::SetNamespace(const FString& InNamespace)
{
	FScopeLock KeyMappingLock(&KeyMappingCS);

	if (!TableNamespace.Equals(InNamespace, ESearchCase::CaseSensitive))
	{
		TableNamespace = InNamespace;

		// Changing the namespace affects the display string pointers, so update those now
		for (auto& KeyToEntryPair : KeysToEntries)
		{
			FStringTableEntryPtr OldEntry = KeyToEntryPair.Value;
			OldEntry->Disown();

			DisplayStringsToKeys.Remove(OldEntry->GetDisplayString());
			KeyToEntryPair.Value = FStringTableEntry::NewStringTableEntry(AsShared(), OldEntry->GetSourceString(), FTextLocalizationManager::Get().GetDisplayString(TableNamespace, KeyToEntryPair.Key, &OldEntry->GetSourceString()));
			DisplayStringsToKeys.Emplace(KeyToEntryPair.Value->GetDisplayString(), KeyToEntryPair.Key);
		}
	}
}

bool FStringTable::GetSourceString(const FString& InKey, FString& OutSourceString) const
{
	FScopeLock KeyMappingLock(&KeyMappingCS);

	FStringTableEntryPtr TableEntry = KeysToEntries.FindRef(InKey);
	if (TableEntry.IsValid())
	{
		OutSourceString = TableEntry->GetSourceString();
		return true;
	}
	return false;
}

void FStringTable::SetSourceString(const FString& InKey, const FString& InSourceString)
{
	checkf(!InKey.IsEmpty(), TEXT("String table key cannot be empty!"));

	FScopeLock KeyMappingLock(&KeyMappingCS);
	
	FStringTableEntryPtr TableEntry = KeysToEntries.FindRef(InKey);
	if (TableEntry.IsValid())
	{
		TableEntry->Disown();
		DisplayStringsToKeys.Remove(TableEntry->GetDisplayString());
	}
	
	TableEntry = FStringTableEntry::NewStringTableEntry(AsShared(), InSourceString, FTextLocalizationManager::Get().GetDisplayString(TableNamespace, InKey, &InSourceString));
	KeysToEntries.Emplace(InKey, TableEntry);
	DisplayStringsToKeys.Emplace(TableEntry->GetDisplayString(), InKey);
}

void FStringTable::RemoveSourceString(const FString& InKey)
{
	FScopeLock KeyMappingLock(&KeyMappingCS);

	FStringTableEntryPtr TableEntry = KeysToEntries.FindRef(InKey);
	if (TableEntry.IsValid())
	{
		TableEntry->Disown();
		KeysToEntries.Remove(InKey);
		DisplayStringsToKeys.Remove(TableEntry->GetDisplayString());
		ClearMetaData(InKey);
	}
}

void FStringTable::EnumerateSourceStrings(const TFunctionRef<bool(const FString&, const FString&)>& InEnumerator) const
{
	FScopeLock KeyMappingLock(&KeyMappingCS);

	for (const auto& KeyToEntryPair : KeysToEntries)
	{
		if (!InEnumerator(KeyToEntryPair.Key, KeyToEntryPair.Value->GetSourceString()))
		{
			break;
		}
	}
}

void FStringTable::ClearSourceStrings(const int32 InSlack)
{
	FScopeLock KeyMappingLock(&KeyMappingCS);

	for (const auto& KeyToEntryPair : KeysToEntries)
	{
		KeyToEntryPair.Value->Disown();
	}

	KeysToEntries.Empty(InSlack);
	DisplayStringsToKeys.Empty(InSlack);

	ClearMetaData(InSlack);
}

FStringTableEntryConstPtr FStringTable::FindEntry(const FString& InKey) const
{
	FScopeLock KeyMappingLock(&KeyMappingCS);
	return KeysToEntries.FindRef(InKey);
}

bool FStringTable::FindKey(const FStringTableEntryConstRef& InEntry, FString& OutKey) const
{
	return FindKey(InEntry->GetDisplayString().ToSharedRef(), OutKey);
}

bool FStringTable::FindKey(const FTextDisplayStringRef& InDisplayString, FString& OutKey) const
{
	FScopeLock KeyMappingLock(&KeyMappingCS);

	const FString* FoundKey = DisplayStringsToKeys.Find(InDisplayString);
	if (FoundKey)
	{
		OutKey = *FoundKey;
		return true;
	}
	return false;
}

FString FStringTable::GetMetaData(const FString& InKey, const FName InMetaDataId) const
{
	FScopeLock MetaDataLock(&KeysToMetaDataCS);

	const FMetaDataMap* MetaDataMap = KeysToMetaData.Find(InKey);
	if (MetaDataMap)
	{
		return MetaDataMap->FindRef(InMetaDataId);
	}
	return FString();
}

void FStringTable::SetMetaData(const FString& InKey, const FName InMetaDataId, const FString& InMetaDataValue)
{
	FScopeLock MetaDataLock(&KeysToMetaDataCS);

	FMetaDataMap& MetaDataMap = KeysToMetaData.FindOrAdd(InKey);
	MetaDataMap.Add(InMetaDataId, InMetaDataValue);
}

void FStringTable::RemoveMetaData(const FString& InKey, const FName InMetaDataId)
{
	FScopeLock MetaDataLock(&KeysToMetaDataCS);

	FMetaDataMap* MetaDataMap = KeysToMetaData.Find(InKey);
	if (MetaDataMap)
	{
		MetaDataMap->Remove(InMetaDataId);
		if (MetaDataMap->Num() == 0)
		{
			KeysToMetaData.Remove(InKey);
		}
	}
}

void FStringTable::EnumerateMetaData(const FString& InKey, const TFunctionRef<bool(FName, const FString&)>& InEnumerator) const
{
	FScopeLock MetaDataLock(&KeysToMetaDataCS);

	const FMetaDataMap* MetaDataMap = KeysToMetaData.Find(InKey);
	if (MetaDataMap)
	{
		for (const auto& IdToMetaDataPair : *MetaDataMap)
		{
			if (!InEnumerator(IdToMetaDataPair.Key, IdToMetaDataPair.Value))
			{
				break;
			}
		}
	}
}

void FStringTable::ClearMetaData(const FString& InKey)
{
	FScopeLock MetaDataLock(&KeysToMetaDataCS);
	KeysToMetaData.Remove(InKey);
}

void FStringTable::ClearMetaData(const int32 InSlack)
{
	FScopeLock MetaDataLock(&KeysToMetaDataCS);
	KeysToMetaData.Empty(InSlack);
}

void FStringTable::Serialize(FArchive& Ar)
{
	FScopeLock KeyMappingLock(&KeyMappingCS);
	FScopeLock MetaDataLock(&KeysToMetaDataCS);

	Ar << TableNamespace;

	if (Ar.IsSaving())
	{
		// Save entries
		{
			int32 NumEntries = KeysToEntries.Num();
			Ar << NumEntries;

			for (const auto& KeyToEntryPair : KeysToEntries)
			{
				FString Key = KeyToEntryPair.Key;
				Ar << Key;

				FString SourceString = KeyToEntryPair.Value->GetSourceString();
				Ar << SourceString;
			}
		}

		// Save meta-data
		{
			Ar << KeysToMetaData;
		}
	}
	else if (Ar.IsLoading())
	{
		// Load entries
		{
			int32 NumEntries = 0;
			Ar << NumEntries;

			ClearSourceStrings(NumEntries);
			for (int32 EntryIndex = 0; EntryIndex < NumEntries; ++EntryIndex)
			{
				FString Key;
				Ar << Key;

				FString SourceString;
				Ar << SourceString;

				FStringTableEntryRef TableEntry = FStringTableEntry::NewStringTableEntry(AsShared(), SourceString, FTextLocalizationManager::Get().GetDisplayString(TableNamespace, Key, &SourceString));
				KeysToEntries.Emplace(Key, TableEntry);
				DisplayStringsToKeys.Emplace(TableEntry->GetDisplayString(), Key);
			}
		}

		// Load meta-data
		{
			Ar << KeysToMetaData;
		}
	}
}

bool FStringTable::ExportStrings(const FString& InFilename) const
{
	FString ExportedStrings;

	{
		FScopeLock KeyMappingLock(&KeyMappingCS);
		FScopeLock MetaDataLock(&KeysToMetaDataCS);

		// Collect meta-data column names
		TSet<FName> MetaDataColumnNames;
		for (const auto& KeyToMetaDataPair : KeysToMetaData)
		{
			for (const auto& IdToMetaDataPair : KeyToMetaDataPair.Value)
			{
				MetaDataColumnNames.Add(IdToMetaDataPair.Key);
			}
		}

		// Write header
		ExportedStrings += TEXT("Key,SourceString");
		for (const FName MetaDataColumnName : MetaDataColumnNames)
		{
			ExportedStrings += TEXT(",");
			ExportedStrings += MetaDataColumnName.ToString();
		}
		ExportedStrings += TEXT("\n");

		// Write entries
		for (const auto& KeyToEntryPair : KeysToEntries)
		{
			FString ExportedKey = KeyToEntryPair.Key.ReplaceCharWithEscapedChar();
			ExportedKey.ReplaceInline(TEXT("\""), TEXT("\"\""));

			FString ExportedSourceString = KeyToEntryPair.Value->GetSourceString().ReplaceCharWithEscapedChar();
			ExportedSourceString.ReplaceInline(TEXT("\""), TEXT("\"\""));

			ExportedStrings += TEXT("\"");
			ExportedStrings += ExportedKey;
			ExportedStrings += TEXT("\"");

			ExportedStrings += TEXT(",");

			ExportedStrings += TEXT("\"");
			ExportedStrings += ExportedSourceString;
			ExportedStrings += TEXT("\"");

			for (const FName MetaDataColumnName : MetaDataColumnNames)
			{
				FString ExportedMetaData = GetMetaData(KeyToEntryPair.Key, MetaDataColumnName);
				ExportedMetaData.ReplaceInline(TEXT("\""), TEXT("\"\""));

				ExportedStrings += TEXT(",");

				ExportedStrings += TEXT("\"");
				ExportedStrings += ExportedMetaData;
				ExportedStrings += TEXT("\"");
			}

			ExportedStrings += TEXT("\n");
		}
	}

	return FFileHelper::SaveStringToFile(ExportedStrings, *InFilename);
}

bool FStringTable::ImportStrings(const FString& InFilename)
{
	FString ImportedStrings;
	if (!FFileHelper::LoadFileToString(ImportedStrings, *InFilename))
	{
		UE_LOG(LogStringTable, Warning, TEXT("Failed to import string table from '%s'. Could not open file."), *InFilename);
		return false;
	}

	const FCsvParser ImportedStringsParser(ImportedStrings);
	const FCsvParser::FRows& Rows = ImportedStringsParser.GetRows();

	// Must have at least 2 rows (header and content)
	if (Rows.Num() <= 1)
	{
		UE_LOG(LogStringTable, Warning, TEXT("Failed to import string table from '%s'. Incorrect number of rows (must be at least 2)."), *InFilename);
		return false;
	}

	int32 KeyColumn = INDEX_NONE;
	int32 SourceStringColumn = INDEX_NONE;
	TMap<FName, int32> MetaDataColumns;

	// Validate header
	{
		const TArray<const TCHAR*>& Cells = Rows[0];

		for (int32 CellIdx = 0; CellIdx < Cells.Num(); ++CellIdx)
		{
			const TCHAR* Cell = Cells[CellIdx];
			if (FCString::Stricmp(Cell, TEXT("Key")) == 0 && KeyColumn == INDEX_NONE)
			{
				KeyColumn = CellIdx;
			}
			else if(FCString::Stricmp(Cell, TEXT("SourceString")) == 0 && SourceStringColumn == INDEX_NONE)
			{
				SourceStringColumn = CellIdx;
			}
			else
			{
				const FName MetaDataName = Cell;
				if (!MetaDataName.IsNone())
				{
					MetaDataColumns.Add(MetaDataName, CellIdx);
				}
			}
		}

		bool bValidHeader = true;
		if (KeyColumn == INDEX_NONE)
		{
			bValidHeader = false;
			UE_LOG(LogStringTable, Warning, TEXT("Failed to import string table from '%s'. Failed to find required column 'Key'."), *InFilename);
		}
		if (SourceStringColumn == INDEX_NONE)
		{
			bValidHeader = false;
			UE_LOG(LogStringTable, Warning, TEXT("Failed to import string table from '%s'. Failed to find required column 'SourceString'."), *InFilename);
		}
		if (!bValidHeader)
		{
			return false;
		}
	}

	// Import rows
	{
		FScopeLock KeyMappingLock(&KeyMappingCS);
		FScopeLock MetaDataLock(&KeysToMetaDataCS);

		ClearSourceStrings(Rows.Num() - 1);
		for (int32 RowIdx = 1; RowIdx < Rows.Num(); ++RowIdx)
		{
			const TArray<const TCHAR*>& Cells = Rows[RowIdx];

			// Must have at least an entry for the Key and SourceString columns
			if (Cells.IsValidIndex(KeyColumn) && Cells.IsValidIndex(SourceStringColumn))
			{
				FString Key = Cells[KeyColumn];
				Key = Key.ReplaceEscapedCharWithChar();

				FString SourceString = Cells[SourceStringColumn];
				SourceString = SourceString.ReplaceEscapedCharWithChar();

				FStringTableEntryRef TableEntry = FStringTableEntry::NewStringTableEntry(AsShared(), SourceString, FTextLocalizationManager::Get().GetDisplayString(TableNamespace, Key, &SourceString));
				KeysToEntries.Emplace(Key, TableEntry);
				DisplayStringsToKeys.Emplace(TableEntry->GetDisplayString(), Key);

				for (const auto& MetaDataColumnPair : MetaDataColumns)
				{
					if (Cells.IsValidIndex(MetaDataColumnPair.Value))
					{
						FString MetaData = Cells[MetaDataColumnPair.Value];
						MetaData = MetaData.ReplaceEscapedCharWithChar();

						if (!MetaData.IsEmpty())
						{
							FMetaDataMap& MetaDataMap = KeysToMetaData.FindOrAdd(Key);
							MetaDataMap.Add(MetaDataColumnPair.Key, MetaData);
						}
					}
				}
			}
		}
	}

	return true;
}


void FStringTableRedirects::InitStringTableRedirects()
{
	check(GConfig);

	FConfigSection* CoreStringTableSection = GConfig->GetSectionPrivate(TEXT("Core.StringTable"), false, true, GEngineIni);
	if (CoreStringTableSection)
	{
		for (FConfigSection::TIterator It(*CoreStringTableSection); It; ++It)
		{
			if (It.Key() == TEXT("StringTableRedirects"))
			{
				const FString& ConfigValue = It.Value().GetValue();

				FName OldStringTable;
				FName NewStringTable;

				FString OldKey;
				FString NewKey;

				if (FParse::Value(*ConfigValue, TEXT("OldStringTable="), OldStringTable))
				{
					FParse::Value(*ConfigValue, TEXT("NewStringTable="), NewStringTable);
					UE_CLOG(NewStringTable.IsNone(), LogStringTable, Warning, TEXT("Failed to parse string table redirect '%s'. Missing or empty 'NewStringTable'."), *ConfigValue);

					if (!NewStringTable.IsNone())
					{
						StringTableRedirects::TableIdRedirects.Add(OldStringTable, NewStringTable);
					}
				}
				else if (FParse::Value(*ConfigValue, TEXT("StringTable="), OldStringTable))
				{
					FParse::Value(*ConfigValue, TEXT("OldKey="), OldKey);
					UE_CLOG(OldKey.IsEmpty(), LogStringTable, Warning, TEXT("Failed to parse string table redirect '%s'. Missing or empty 'OldKey'."), *ConfigValue);

					FParse::Value(*ConfigValue, TEXT("NewKey="), NewKey);
					UE_CLOG(NewKey.IsEmpty(), LogStringTable, Warning, TEXT("Failed to parse string table redirect '%s'. Missing or empty 'NewKey'."), *ConfigValue);

					if (!OldKey.IsEmpty() && !NewKey.IsEmpty())
					{
						StringTableRedirects::TableKeyRedirects.FindOrAdd(OldStringTable).Add(OldKey, NewKey);
					}
				}
				else
				{
					UE_LOG(LogStringTable, Warning, TEXT("Failed to parse string table redirect '%s'. Expected 'OldStringTable' and 'NewStringTable' for a table ID redirect, or 'StringTable', 'OldKey', 'NewKey' for a key redirect."), *ConfigValue);
				}
			}
		}
	}
}

void FStringTableRedirects::RedirectTableId(FName& InOutTableId, const EStringTableLoadingPolicy InLoadingPolicy)
{
	// Process the static redirect
	const FName* RedirectedTableId = StringTableRedirects::TableIdRedirects.Find(InOutTableId);
	if (RedirectedTableId)
	{
		InOutTableId = *RedirectedTableId;
	}

	// Process the asset redirect
	IStringTableEngineBridge::RedirectAndLoadStringTableAsset(InOutTableId, InLoadingPolicy);
}

void FStringTableRedirects::RedirectKey(const FName InTableId, FString& InOutKey)
{
	const auto* RedirectedKeyMap = StringTableRedirects::TableKeyRedirects.Find(InTableId);
	if (RedirectedKeyMap)
	{
		const FString* RedirectedKey = RedirectedKeyMap->Find(InOutKey);
		if (RedirectedKey)
		{
			InOutKey = *RedirectedKey;
		}
	}
}

void FStringTableRedirects::RedirectTableIdAndKey(FName& InOutTableId, FString& InOutKey, const EStringTableLoadingPolicy InLoadingPolicy)
{
	RedirectTableId(InOutTableId, InLoadingPolicy);
	RedirectKey(InOutTableId, InOutKey);
}


void FStringTableReferenceCollection::CollectAssetReferences(const FName InTableId, FArchive& InAr)
{
	if (InAr.IsObjectReferenceCollector())
	{
		IStringTableEngineBridge::CollectStringTableAssetReferences(InTableId, InAr);
	}
}
