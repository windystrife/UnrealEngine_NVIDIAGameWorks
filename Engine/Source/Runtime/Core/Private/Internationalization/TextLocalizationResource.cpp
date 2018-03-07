// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Internationalization/TextLocalizationResource.h"
#include "Internationalization/TextLocalizationResourceVersion.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogTextLocalizationResource, Log, All);

const FGuid FTextLocalizationResourceVersion::LocMetaMagic = FGuid(0xA14CEE4F, 0x83554868, 0xBD464C6C, 0x7C50DA70);
const FGuid FTextLocalizationResourceVersion::LocResMagic = FGuid(0x7574140E, 0xFC034A67, 0x9D90154A, 0x1B7F37C3);

bool FTextLocalizationMetaDataResource::LoadFromFile(const FString& FilePath)
{
	TUniquePtr<FArchive> Reader(IFileManager::Get().CreateFileReader(*FilePath));
	if (!Reader)
	{
		UE_LOG(LogTextLocalizationResource, Warning, TEXT("LocMeta '%s' could not be opened for reading!"), *FilePath);
		return false;
	}

	bool Success = LoadFromArchive(*Reader, FilePath);
	Success &= Reader->Close();
	return Success;
}

bool FTextLocalizationMetaDataResource::LoadFromArchive(FArchive& Archive, const FString& LocMetaID)
{
	FTextLocalizationResourceVersion::ELocMetaVersion VersionNumber = FTextLocalizationResourceVersion::ELocMetaVersion::Initial;

	// Verify header
	{
		FGuid MagicNumber;
		Archive << MagicNumber;

		if (MagicNumber != FTextLocalizationResourceVersion::LocMetaMagic)
		{
			UE_LOG(LogTextLocalizationResource, Warning, TEXT("LocMeta '%s' failed the magic number check!"), *LocMetaID);
			return false;
		}

		Archive << VersionNumber;
	}

	Archive << NativeCulture;
	Archive << NativeLocRes;

	return true;
}

bool FTextLocalizationMetaDataResource::SaveToFile(const FString& FilePath)
{
	TUniquePtr<FArchive> Writer(IFileManager::Get().CreateFileWriter(*FilePath));
	if (!Writer)
	{
		UE_LOG(LogTextLocalizationResource, Warning, TEXT("LocMeta '%s' could not be opened for writing!"), *FilePath);
		return false;
	}

	bool bSaved = SaveToArchive(*Writer, FilePath);
	bSaved &= Writer->Close();
	return bSaved;
}

bool FTextLocalizationMetaDataResource::SaveToArchive(FArchive& Archive, const FString& LocMetaID)
{
	// Write the header
	{
		FGuid MagicNumber = FTextLocalizationResourceVersion::LocMetaMagic;
		Archive << MagicNumber;

		uint8 VersionNumber = (uint8)FTextLocalizationResourceVersion::ELocMetaVersion::Latest;
		Archive << VersionNumber;
	}

	// Write the native meta-data
	{
		Archive << NativeCulture;
		Archive << NativeLocRes;
	}

	return true;
}


void FTextLocalizationResource::LoadFromDirectory(const FString& DirectoryPath)
{
	// Find resources in the specified folder.
	TArray<FString> ResourceFileNames;
	IFileManager::Get().FindFiles(ResourceFileNames, *(DirectoryPath / TEXT("*.locres")), true, false);

	for (const FString& ResourceFileName : ResourceFileNames)
	{
		LoadFromFile(FPaths::ConvertRelativePathToFull(DirectoryPath / ResourceFileName));
	}
}

bool FTextLocalizationResource::LoadFromFile(const FString& FilePath)
{
	TUniquePtr<FArchive> Reader(IFileManager::Get().CreateFileReader(*FilePath));
	if (!Reader)
	{
		UE_LOG(LogTextLocalizationResource, Warning, TEXT("LocRes '%s' could not be opened for reading!"), *FilePath);
		return false;
	}

	bool Success = LoadFromArchive(*Reader, FilePath);
	Success &= Reader->Close();
	return Success;
}

bool FTextLocalizationResource::LoadFromArchive(FArchive& Archive, const FString& LocalizationResourceIdentifier)
{
	Archive.SetForceUnicode(true);

	// Read magic number
	FGuid MagicNumber;
	
	if (Archive.TotalSize() >= sizeof(FGuid))
	{
		Archive << MagicNumber;
	}

	FTextLocalizationResourceVersion::ELocResVersion VersionNumber = FTextLocalizationResourceVersion::ELocResVersion::Legacy;
	if (MagicNumber == FTextLocalizationResourceVersion::LocResMagic)
	{
		Archive << VersionNumber;
	}
	else
	{
		// Legacy LocRes files lack the magic number, assume that's what we're dealing with, and seek back to the start of the file
		Archive.Seek(0);
		//UE_LOG(LogTextLocalizationResource, Warning, TEXT("LocRes '%s' failed the magic number check! Assuming this is a legacy resource (please re-generate your localization resources!)"), *LocalizationResourceIdentifier);
		UE_LOG(LogTextLocalizationResource, Log, TEXT("LocRes '%s' failed the magic number check! Assuming this is a legacy resource (please re-generate your localization resources!)"), *LocalizationResourceIdentifier);
	}

	// Read the localized string array
	TArray<FString> LocalizedStringArray;
	if (VersionNumber >= FTextLocalizationResourceVersion::ELocResVersion::Compact)
	{
		int64 LocalizedStringArrayOffset = INDEX_NONE;
		Archive << LocalizedStringArrayOffset;

		if (LocalizedStringArrayOffset != INDEX_NONE)
		{
			const int64 CurrentFileOffset = Archive.Tell();
			Archive.Seek(LocalizedStringArrayOffset);
			Archive << LocalizedStringArray;
			Archive.Seek(CurrentFileOffset);
		}
	}

	// Read namespace count
	uint32 NamespaceCount;
	Archive << NamespaceCount;

	for (uint32 i = 0; i < NamespaceCount; ++i)
	{
		// Read namespace
		FString Namespace;
		Archive << Namespace;

		// Read key count
		uint32 KeyCount;
		Archive << KeyCount;

		FKeysTable& KeyTable = Namespaces.FindOrAdd(*Namespace);

		for (uint32 j = 0; j < KeyCount; ++j)
		{
			// Read key
			FString Key;
			Archive << Key;

			FEntryArray& EntryArray = KeyTable.FindOrAdd(*Key);

			FEntry NewEntry;
			NewEntry.LocResID = LocalizationResourceIdentifier;

			// Read string entry.
			Archive << NewEntry.SourceStringHash;

			if (VersionNumber >= FTextLocalizationResourceVersion::ELocResVersion::Compact)
			{
				int32 LocalizedStringIndex = INDEX_NONE;
				Archive << LocalizedStringIndex;

				if (LocalizedStringArray.IsValidIndex(LocalizedStringIndex))
				{
					NewEntry.LocalizedString = LocalizedStringArray[LocalizedStringIndex];
				}
				else
				{
					UE_LOG(LogTextLocalizationResource, Warning, TEXT("LocRes '%s' has an invalid localized string index for namespace '%s' and key '%s'. This entry will have no translation."), *LocalizationResourceIdentifier, *Namespace, *Key);
				}
			}
			else
			{
				Archive << NewEntry.LocalizedString;
			}

			EntryArray.Add(NewEntry);
		}
	}

	return true;
}

bool FTextLocalizationResource::SaveToFile(const FString& FilePath)
{
	TUniquePtr<FArchive> Writer(IFileManager::Get().CreateFileWriter(*FilePath));
	if (!Writer)
	{
		UE_LOG(LogTextLocalizationResource, Warning, TEXT("LocRes '%s' could not be opened for writing!"), *FilePath);
		return false;
	}

	bool bSaved = SaveToArchive(*Writer, FilePath);
	bSaved &= Writer->Close();
	return bSaved;
}

bool FTextLocalizationResource::SaveToArchive(FArchive& Archive, const FString& LocResID)
{
	Archive.SetForceUnicode(true);

	// Write the header
	{
		FGuid MagicNumber = FTextLocalizationResourceVersion::LocResMagic;
		Archive << MagicNumber;

		uint8 VersionNumber = (uint8)FTextLocalizationResourceVersion::ELocResVersion::Latest;
		Archive << VersionNumber;
	}

	// Write placeholder offsets for the localized string array
	const int64 LocalizedStringArrayOffset = Archive.Tell();
	{
		int64 DummyOffsetValue = INDEX_NONE;
		Archive << DummyOffsetValue;
	}

	// Arrays tracking localized strings, with a map for efficient look-up of array indices from strings
	TArray<FString> LocalizedStringArray;
	TMap<FString, int32, FDefaultSetAllocator, FLocKeyMapFuncs<int32>> LocalizedStringMap;

	auto GetLocalizedStringIndex = [&LocalizedStringArray, &LocalizedStringMap](const FString& InString) -> int32
	{
		if (const int32* FoundIndex = LocalizedStringMap.Find(InString))
		{
			return *FoundIndex;
		}

		const int32 NewIndex = LocalizedStringArray.Num();
		LocalizedStringArray.Add(InString);
		LocalizedStringMap.Add(InString, NewIndex);
		return NewIndex;
	};

	// Write namespace count
	uint32 NamespaceCount = Namespaces.Num();
	Archive << NamespaceCount;

	// Iterate through namespaces
	for (auto& NamespaceEntryPair : Namespaces)
	{
		/*const*/ FString& Namespace = NamespaceEntryPair.Key;
		/*const*/ FKeysTable& KeysTable = NamespaceEntryPair.Value;

		// Write namespace.
		Archive << Namespace;

		// Write a placeholder key count, we'll fill this in once we know how many keys were actually written
		uint32 KeyCount = 0;
		const int64 KeyCountOffset = Archive.Tell();
		Archive << KeyCount;

		// Iterate through keys and values
		for (auto& KeyEntryPair : KeysTable)
		{
			/*const*/ FString& Key = KeyEntryPair.Key;
			/*const*/ FEntryArray& EntryArray = KeyEntryPair.Value;

			// Skip this key if there are no entries.
			if (EntryArray.Num() == 0)
			{
				UE_LOG(LogTextLocalizationResource, Warning, TEXT("LocRes '%s': Archives contained no entries for key (%s)"), *LocResID, *Key);
				continue;
			}

			// Find first valid entry.
			/*const*/ FEntry* Value = nullptr;
			for (auto& PotentialValue : EntryArray)
			{
				if (!PotentialValue.LocalizedString.IsEmpty())
				{
					Value = &PotentialValue;
					break;
				}
			}

			// Skip this key if there is no valid entry.
			if (!Value)
			{
				UE_LOG(LogTextLocalizationResource, Verbose, TEXT("LocRes '%s': Archives contained only blank entries for key (%s)"), *LocResID, *Key);
				continue;
			}

			++KeyCount;

			// Write key.
			Archive << Key;

			// Write string entry.
			Archive << Value->SourceStringHash;

			int32 LocalizedStringIndex = GetLocalizedStringIndex(Value->LocalizedString);
			Archive << LocalizedStringIndex;
		}

		// Re-write the key value now
		{
			const int64 CurrentFileOffset = Archive.Tell();
			Archive.Seek(KeyCountOffset);
			Archive << KeyCount;
			Archive.Seek(CurrentFileOffset);
		}
	}

	// Write the localized strings array now
	{
		int64 CurrentFileOffset = Archive.Tell();
		Archive.Seek(LocalizedStringArrayOffset);
		Archive << CurrentFileOffset;
		Archive.Seek(CurrentFileOffset);
		Archive << LocalizedStringArray;
	}

	return true;
}

void FTextLocalizationResource::DetectAndLogConflicts() const
{
	for (const auto& NamespaceEntry : Namespaces)
	{
		const FString& NamespaceName = NamespaceEntry.Key;
		const FKeysTable& KeyTable = NamespaceEntry.Value;
		for (const auto& KeyEntry : KeyTable)
		{
			const FString& KeyName = KeyEntry.Key;
			const FEntryArray& EntryArray = KeyEntry.Value;

			bool WasConflictDetected = false;
			for (int32 k = 0; k < EntryArray.Num(); ++k)
			{
				const FEntry& LeftEntry = EntryArray[k];
				for (int32 l = k + 1; l < EntryArray.Num(); ++l)
				{
					const FEntry& RightEntry = EntryArray[l];
					const bool bDoesSourceStringHashDiffer = LeftEntry.SourceStringHash != RightEntry.SourceStringHash;
					const bool bDoesLocalizedStringDiffer = !LeftEntry.LocalizedString.Equals(RightEntry.LocalizedString, ESearchCase::CaseSensitive);
					WasConflictDetected = bDoesSourceStringHashDiffer || bDoesLocalizedStringDiffer;
				}
			}

			if (WasConflictDetected)
			{
				FString CollidingEntryListString;
				for (int32 k = 0; k < EntryArray.Num(); ++k)
				{
					const FEntry& Entry = EntryArray[k];

					if (!(CollidingEntryListString.IsEmpty()))
					{
						CollidingEntryListString += TEXT('\n');
					}

					CollidingEntryListString += FString::Printf(TEXT("    Localization Resource: (%s) Source String Hash: (%d) Localized String: (%s)"), *(Entry.LocResID), Entry.SourceStringHash, *(Entry.LocalizedString));
				}

				UE_LOG(LogTextLocalizationResource, Warning, TEXT("Loaded localization resources contain conflicting entries for (Namespace:%s, Key:%s):\n%s"), *NamespaceName, *KeyName, *CollidingEntryListString);
			}
		}
	}
}
