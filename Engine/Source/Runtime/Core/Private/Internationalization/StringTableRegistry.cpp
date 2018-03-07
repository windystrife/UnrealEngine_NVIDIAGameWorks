// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "StringTableRegistry.h"
#include "StringTableCore.h"
#include "Misc/ScopeLock.h"
#include "ModuleManager.h"

#if WITH_EDITOR
#include "IDirectoryWatcher.h"
#include "DirectoryWatcherModule.h"
#endif // WITH_EDITOR

FStringTableRegistry& FStringTableRegistry::Get()
{
	static FStringTableRegistry Instance;
	return Instance;
}

FStringTableRegistry::FStringTableRegistry()
{
#if WITH_EDITOR
	// Commandlets and in-game don't listen for directory changes
	if (!IsRunningCommandlet() && GIsEditor)
	{
		FDirectoryWatcherModule& DirectoryWatcherModule = FModuleManager::LoadModuleChecked<FDirectoryWatcherModule>("DirectoryWatcher");
		IDirectoryWatcher* DirectoryWatcher = DirectoryWatcherModule.Get();

		if (DirectoryWatcher)
		{
			DirectoryWatcher->RegisterDirectoryChangedCallback_Handle(FPaths::EngineContentDir(), IDirectoryWatcher::FDirectoryChanged::CreateRaw(this, &FStringTableRegistry::OnDirectoryChanged), EngineDirectoryWatcherHandle);

			DirectoryWatcher->RegisterDirectoryChangedCallback_Handle(FPaths::ProjectContentDir(), IDirectoryWatcher::FDirectoryChanged::CreateRaw(this, &FStringTableRegistry::OnDirectoryChanged), GameDirectoryWatcherHandle);
		}
	}
#endif // WITH_EDITOR
}

FStringTableRegistry::~FStringTableRegistry()
{
#if WITH_EDITOR
	// Commandlets and in-game don't listen for directory changes
	if (!IsRunningCommandlet() && GIsEditor)
	{
		// If the directory module is still loaded, unregister any delegates
		if (FModuleManager::Get().IsModuleLoaded("DirectoryWatcher"))
		{
			FDirectoryWatcherModule& DirectoryWatcherModule = FModuleManager::GetModuleChecked<FDirectoryWatcherModule>("DirectoryWatcher");
			IDirectoryWatcher* DirectoryWatcher = DirectoryWatcherModule.Get();

			if (DirectoryWatcher)
			{
				DirectoryWatcher->UnregisterDirectoryChangedCallback_Handle(FPaths::EngineContentDir(), EngineDirectoryWatcherHandle);
				EngineDirectoryWatcherHandle.Reset();

				DirectoryWatcher->UnregisterDirectoryChangedCallback_Handle(FPaths::ProjectContentDir(), GameDirectoryWatcherHandle);
				GameDirectoryWatcherHandle.Reset();
			}
		}
	}
#endif // WITH_EDITOR
}

void FStringTableRegistry::RegisterStringTable(const FName InTableId, FStringTableRef InTable)
{
	FScopeLock RegisteredStringTablesLock(&RegisteredStringTablesCS);

	checkf(!InTableId.IsNone(), TEXT("String table ID cannot be 'None'!"));
	checkf(!RegisteredStringTables.Contains(InTableId), TEXT("String table ID '%s' is already in use!"), *InTableId.ToString());

	RegisteredStringTables.Add(InTableId, MoveTemp(InTable));
}

void FStringTableRegistry::UnregisterStringTable(const FName InTableId)
{
	FScopeLock RegisteredStringTablesLock(&RegisteredStringTablesCS);
	RegisteredStringTables.Remove(InTableId);
}

FStringTablePtr FStringTableRegistry::FindMutableStringTable(const FName InTableId) const
{
	FScopeLock RegisteredStringTablesLock(&RegisteredStringTablesCS);
	return RegisteredStringTables.FindRef(InTableId);
}

FStringTableConstPtr FStringTableRegistry::FindStringTable(const FName InTableId) const
{
	FScopeLock RegisteredStringTablesLock(&RegisteredStringTablesCS);
	return RegisteredStringTables.FindRef(InTableId);
}

UStringTable* FStringTableRegistry::FindStringTableAsset(const FName InTableId) const
{
	UStringTable* StringTableAsset = nullptr;
	{
		FStringTableConstPtr StringTable = FindStringTable(InTableId);
		if (StringTable.IsValid())
		{
			StringTableAsset = StringTable->GetOwnerAsset();
		}
	}
	return StringTableAsset;
}

void FStringTableRegistry::EnumerateStringTables(const TFunctionRef<bool(const FName&, const FStringTableConstRef&)>& InEnumerator) const
{
	FScopeLock RegisteredStringTablesLock(&RegisteredStringTablesCS);

	for (const auto& RegisteredStringTablePair : RegisteredStringTables)
	{
		if (!InEnumerator(RegisteredStringTablePair.Key, RegisteredStringTablePair.Value.ToSharedRef()))
		{
			break;
		}
	}
}

bool FStringTableRegistry::FindTableIdAndKey(const FText& InText, FName& OutTableId, FString& OutKey) const
{
	if (InText.IsFromStringTable())
	{
		return FindTableIdAndKey(FTextInspector::GetSharedDisplayString(InText), OutTableId, OutKey) || FTextInspector::GetTableIdAndKey(InText, OutTableId, OutKey);
	}

	return false;
}

bool FStringTableRegistry::FindTableIdAndKey(const FTextDisplayStringRef& InDisplayString, FName& OutTableId, FString& OutKey) const
{
	FScopeLock RegisteredStringTablesLock(&RegisteredStringTablesCS);

	for (const auto& RegisteredStringTablePair : RegisteredStringTables)
	{
		FString FoundKey;
		if (RegisteredStringTablePair.Value->FindKey(InDisplayString, FoundKey))
		{
			OutTableId = RegisteredStringTablePair.Key;
			OutKey = MoveTemp(FoundKey);
			return true;
		}
	}

	return false;
}

void FStringTableRegistry::LogMissingStringTableEntry(const FName InTableId, const FString& InKey)
{
	FScopeLock LoggedMissingEntriesLock(&LoggedMissingEntriesCS);

	if (const FLocKeySet* LoggedMissingKeys = LoggedMissingEntries.Find(InTableId))
	{
		if (LoggedMissingKeys->Contains(InKey))
		{
			return;
		}
	}

	FLocKeySet& LoggedMissingKeys = LoggedMissingEntries.FindOrAdd(InTableId);
	LoggedMissingKeys.Add(InKey);

	UE_LOG(LogStringTable, Warning, TEXT("Failed to find string table entry for '%s' '%s'. Did you forget to add a string table redirector?"), *InTableId.ToString(), *InKey);
}

#if WITH_EDITOR
void FStringTableRegistry::OnDirectoryChanged(const TArray<FFileChangeData>& InFileChanges)
{
	FScopeLock CSVFilesToWatchLock(&CSVFilesToWatchCS);

	for (const FFileChangeData& FileChange : InFileChanges)
	{
		if (FileChange.Action == FFileChangeData::FCA_Added || FileChange.Action == FFileChangeData::FCA_Modified)
		{
			const FName TableId = CSVFilesToWatch.FindRef(FPaths::ConvertRelativePathToFull(FileChange.Filename));
			if (!TableId.IsNone())
			{
				FStringTablePtr StringTable = FindMutableStringTable(TableId);
				if (StringTable.IsValid())
				{
					if (!StringTable->ImportStrings(FileChange.Filename))
					{
						UE_LOG(LogStringTable, Warning, TEXT("Failed to import strings from '%s'"), *FileChange.Filename);
					}
				}
			}
		}
	}
}
#endif // WITH_EDITOR

void FStringTableRegistry::Internal_NewLocTable(const FName InTableId, const FString& InNamespace)
{
	FStringTableRef StringTable = FStringTable::NewStringTable();
	StringTable->SetNamespace(InNamespace);

	RegisterStringTable(InTableId, StringTable);
}

void FStringTableRegistry::Internal_LocTableFromFile(const FName InTableId, const FString& InNamespace, const FString& InFilePath, const FString& InRootPath)
{
	FStringTableRef StringTable = FStringTable::NewStringTable();
	StringTable->SetNamespace(InNamespace);

	const FString FullPath = InRootPath / InFilePath;
	if (!StringTable->ImportStrings(FullPath))
	{
		UE_LOG(LogStringTable, Warning, TEXT("Failed to import strings from '%s'"), *FullPath);
	}

	RegisterStringTable(InTableId, StringTable);

#if WITH_EDITOR
	{
		FScopeLock CSVFilesToWatchLock(&CSVFilesToWatchCS);
		CSVFilesToWatch.Add(FPaths::ConvertRelativePathToFull(FullPath), InTableId);
	}
#endif // WITH_EDITOR
}

void FStringTableRegistry::Internal_SetLocTableEntry(const FName InTableId, const FString& InKey, const FString& InSourceString)
{
	FStringTablePtr StringTable = FindMutableStringTable(InTableId);
	checkf(StringTable.IsValid(), TEXT("Attempting to add a string table entry to the unknown string table '%s'"), *InTableId.ToString());

	StringTable->SetSourceString(InKey, InSourceString);
}

void FStringTableRegistry::Internal_SetLocTableEntryMetaData(const FName InTableId, const FString& InKey, const FName InMetaDataId, const FString& InMetaData)
{
	FStringTablePtr StringTable = FindMutableStringTable(InTableId);
	checkf(StringTable.IsValid(), TEXT("Attempting to add string table entry meta-data to the unknown string table '%s'"), *InTableId.ToString());

	StringTable->SetMetaData(InKey, InMetaDataId, InMetaData);
}

FText FStringTableRegistry::Internal_FindLocTableEntry(const FName InTableId, const FString& InKey, const EStringTableLoadingPolicy InLoadingPolicy) const
{
	// RedirectTableIdAndKey also causes string table assets to be loaded (as it has to do this to process asset redirects)
	FName RedirectedTableId = InTableId;
	FString RedirectedKey = InKey;
	FStringTableRedirects::RedirectTableIdAndKey(RedirectedTableId, RedirectedKey, InLoadingPolicy);

	return FText(RedirectedTableId, RedirectedKey);
}
