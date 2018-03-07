// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "OnlineHotfixManager.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Internationalization/Culture.h"
#include "Misc/CoreDelegates.h"
#include "Misc/App.h"
#include "UObject/UObjectIterator.h"
#include "UObject/Package.h"
#include "Misc/PackageName.h"
#include "OnlineSubsystemUtils.h"
#include "Logging/LogSuppressionInterface.h"

DEFINE_LOG_CATEGORY(LogHotfixManager);

FName NAME_HotfixManager(TEXT("HotfixManager"));

class FPakFileVisitor : public IPlatformFile::FDirectoryVisitor
{
public:
	virtual bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory) override
	{
		if (!bIsDirectory)
		{
			Files.Add(FilenameOrDirectory);
		}
		return true;
	}

	TArray<FString> Files;
};

UOnlineHotfixManager::UOnlineHotfixManager() :
	Super(),
	TotalFiles(0),
	NumDownloaded(0),
	TotalBytes(0),
	NumBytes(0),
	bHotfixingInProgress(false),
	bHotfixNeedsMapReload(false),
	ChangedOrRemovedPakCount(0)
{
	OnEnumerateFilesCompleteDelegate = FOnEnumerateFilesCompleteDelegate::CreateUObject(this, &UOnlineHotfixManager::OnEnumerateFilesComplete);
	OnReadFileProgressDelegate = FOnReadFileProgressDelegate::CreateUObject(this, &UOnlineHotfixManager::OnReadFileProgress);
	OnReadFileCompleteDelegate = FOnReadFileCompleteDelegate::CreateUObject(this, &UOnlineHotfixManager::OnReadFileComplete);
#if !UE_BUILD_SHIPPING
	bLogMountedPakContents = FParse::Param(FCommandLine::Get(), TEXT("LogHotfixPakContents"));
#endif
	GameContentPath = FString() / FApp::GetProjectName() / TEXT("Content");
}

UOnlineHotfixManager* UOnlineHotfixManager::Get(UWorld* World)
{
	UOnlineHotfixManager* DefaultObject = UOnlineHotfixManager::StaticClass()->GetDefaultObject<UOnlineHotfixManager>();
	IOnlineSubsystem* OnlineSub = Online::GetSubsystem(World, DefaultObject->OSSName.Len() > 0 ? FName(*DefaultObject->OSSName) : NAME_None);
	if (OnlineSub != nullptr)
	{
		UOnlineHotfixManager* HotfixManager = Cast<UOnlineHotfixManager>(OnlineSub->GetNamedInterface(NAME_HotfixManager));
		if (HotfixManager == nullptr)
		{
			FString HotfixManagerClassName = DefaultObject->HotfixManagerClassName;
			UClass* HotfixManagerClass = LoadClass<UOnlineHotfixManager>(nullptr, *HotfixManagerClassName, nullptr, LOAD_None, nullptr);
			if (HotfixManagerClass == nullptr)
			{
				// Just use the default class if it couldn't load what was specified
				HotfixManagerClass = UOnlineHotfixManager::StaticClass();
			}
			// Create it and store it
			HotfixManager = NewObject<UOnlineHotfixManager>(GetTransientPackage(), HotfixManagerClass);
			OnlineSub->SetNamedInterface(NAME_HotfixManager, HotfixManager);
		}
		return HotfixManager;
	}
	return nullptr;
}

void UOnlineHotfixManager::PostInitProperties()
{
#if !UE_BUILD_SHIPPING
	FParse::Value(FCommandLine::Get(), TEXT("HOTFIXPREFIX="), DebugPrefix);
#endif
	// So we only try to apply files for this platform
	PlatformPrefix = DebugPrefix + ANSI_TO_TCHAR(FPlatformProperties::PlatformName());
	PlatformPrefix += TEXT("_");
	// Server prefix
	ServerPrefix = DebugPrefix + TEXT("DedicatedServer");
	// Build the default prefix too
	DefaultPrefix = DebugPrefix + TEXT("Default");

	Super::PostInitProperties();
}

void UOnlineHotfixManager::Init()
{
	bHotfixingInProgress = true;
	bHotfixNeedsMapReload = false;
	TotalFiles = 0;
	NumDownloaded = 0;
	TotalBytes = 0;
	NumBytes = 0;
	ChangedOrRemovedPakCount = 0;
	// Build the name of the loc file that we'll care about
	// It can change at runtime so build it just before fetching the data
	GameLocName = DebugPrefix + FInternationalization::Get().GetCurrentCulture()->GetTwoLetterISOLanguageName() + TEXT("_Game.locres");
	OnlineTitleFile = Online::GetTitleFileInterface(OSSName.Len() ? FName(*OSSName, FNAME_Find) : NAME_None);
	if (OnlineTitleFile.IsValid())
	{
		OnEnumerateFilesCompleteDelegateHandle = OnlineTitleFile->AddOnEnumerateFilesCompleteDelegate_Handle(OnEnumerateFilesCompleteDelegate);
		OnReadFileProgressDelegateHandle = OnlineTitleFile->AddOnReadFileProgressDelegate_Handle(OnReadFileProgressDelegate);
		OnReadFileCompleteDelegateHandle = OnlineTitleFile->AddOnReadFileCompleteDelegate_Handle(OnReadFileCompleteDelegate);
	}
}

void UOnlineHotfixManager::Cleanup()
{
	PendingHotfixFiles.Empty();
	if (OnlineTitleFile.IsValid())
	{
		// Make sure to give back the memory used when reading the hotfix files
		OnlineTitleFile->ClearFiles();
		OnlineTitleFile->ClearOnEnumerateFilesCompleteDelegate_Handle(OnEnumerateFilesCompleteDelegateHandle);
		OnlineTitleFile->ClearOnReadFileProgressDelegate_Handle(OnReadFileProgressDelegateHandle);
		OnlineTitleFile->ClearOnReadFileCompleteDelegate_Handle(OnReadFileCompleteDelegateHandle);
	}
	OnlineTitleFile = nullptr;
	bHotfixingInProgress = false;
}

void UOnlineHotfixManager::StartHotfixProcess()
{
	// Patching the editor this way seems like a bad idea
	bool bShouldHotfix = IsRunningGame() || IsRunningDedicatedServer() || IsRunningClientOnly();
	if (!bShouldHotfix)
	{
		UE_LOG(LogHotfixManager, Warning, TEXT("Hotfixing skipped when not running game/server"));
		TriggerHotfixComplete(EHotfixResult::SuccessNoChange);
		return;
	}

	if (bHotfixingInProgress)
	{
		UE_LOG(LogHotfixManager, Warning, TEXT("Hotfixing already in progress"));
		return;
	}

	Init();
	// Kick off an enumeration of the files that are available to download
	if (OnlineTitleFile.IsValid())
	{
		OnlineTitleFile->EnumerateFiles();
	}
	else
	{
		UE_LOG(LogHotfixManager, Error, TEXT("Failed to start the hotfixing process due to no OnlineTitleInterface present for OSS(%s)"), *OSSName);
		TriggerHotfixComplete(EHotfixResult::Failed);
	}
}

struct FHotfixFileSortPredicate
{
	struct FHotfixFileNameSortPredicate
	{
		const FString PlatformPrefix;
		const FString ServerPrefix;
		const FString DefaultPrefix;

		FHotfixFileNameSortPredicate(const FString& InPlatformPrefix, const FString& InServerPrefix, const FString& InDefaultPrefix) :
			PlatformPrefix(InPlatformPrefix),
			ServerPrefix(InServerPrefix),
			DefaultPrefix(InDefaultPrefix)
		{
		}

		int32 GetPriorityForCompare(const FString& HotfixName) const
		{
			// Non-ini files are applied last
			int32 Priority = 5;
			
			if (HotfixName.EndsWith(TEXT("INI")))
			{
				// Defaults are applied first
				if (HotfixName.StartsWith(DefaultPrefix))
				{
					Priority = 1;
				}
				// Server trumps default
				else if (HotfixName.StartsWith(ServerPrefix))
				{
					Priority = 2;
				}
				// Platform trumps server
				else if (HotfixName.StartsWith(PlatformPrefix))
				{
					Priority = 3;
				}
				// Other INIs whitelisted in game override of WantsHotfixProcessing will trump all other INIs
				else
				{
					Priority = 4;
				}
			}

			return Priority;
		}

		bool Compare(const FString& A, const FString& B) const
		{
			int32 APriority = GetPriorityForCompare(A);
			int32 BPriority = GetPriorityForCompare(B);
			if (APriority != BPriority)
			{
				return APriority < BPriority;
			}
			else
			{
				// Fall back to sort by the string order if both have same priority
				return A < B;
			}
		}
	};
	FHotfixFileNameSortPredicate FileNameSorter;

	FHotfixFileSortPredicate(const FString& InPlatformPrefix, const FString& InServerPrefix, const FString& InDefaultPrefix) :
		FileNameSorter(InPlatformPrefix, InServerPrefix, InDefaultPrefix)
	{
	}

	bool operator()(const FCloudFileHeader &A, const FCloudFileHeader &B) const
	{
		return FileNameSorter.Compare(A.FileName, B.FileName);
	}

	bool operator()(const FString& A, const FString& B) const
	{
		return FileNameSorter.Compare(FPaths::GetCleanFilename(A), FPaths::GetCleanFilename(B));
	}
};

void UOnlineHotfixManager::OnEnumerateFilesComplete(bool bWasSuccessful, const FString& ErrorStr)
{
	if (bWasSuccessful)
	{
		check(OnlineTitleFile.IsValid());
		// Cache our current set so we can compare for differences
		LastHotfixFileList = HotfixFileList;
		HotfixFileList.Empty();
		// Get the new header data
		OnlineTitleFile->GetFileList(HotfixFileList);
		FilterHotfixFiles();
		// Reduce the set of work to just the files that changed since last run
		BuildHotfixFileListDeltas();
		// Sort after filtering so that the comparison below doesn't fail to different order from the server
		ChangedHotfixFileList.Sort<FHotfixFileSortPredicate>(FHotfixFileSortPredicate(PlatformPrefix, ServerPrefix, DefaultPrefix));
		// Perform any undo operations needed
		if (ChangedHotfixFileList.Num() > 0 || RemovedHotfixFileList.Num() > 0)
		{
			RestoreBackupIniFiles();
			UnmountHotfixFiles();
		}
		// Read any changed files
		if (ChangedHotfixFileList.Num() > 0)
		{
			// Update our totals for our progress delegates
			TotalFiles = ChangedHotfixFileList.Num();
			for (auto& FileHeader : ChangedHotfixFileList)
			{
				TotalBytes += FileHeader.FileSize;
			}
			ReadHotfixFiles();
		}
		else
		{
			UE_LOG(LogHotfixManager, Display, TEXT("Returned hotfix data is the same as last application, skipping the apply phase"));
			TriggerHotfixComplete(EHotfixResult::SuccessNoChange);
		}
	}
	else
	{
		UE_LOG(LogHotfixManager, Error, TEXT("Enumeration of hotfix files failed"));
		TriggerHotfixComplete(EHotfixResult::Failed);
	}
}

void UOnlineHotfixManager::CheckAvailability(FOnHotfixAvailableComplete& InCompletionDelegate)
{
	// Checking for hotfixes in editor is not supported
	bool bShouldHotfix = IsRunningGame() || IsRunningDedicatedServer() || IsRunningClientOnly();
	if (!bShouldHotfix)
	{
		UE_LOG(LogHotfixManager, Warning, TEXT("Hotfixing availability skipped when not running game/server"));
		InCompletionDelegate.ExecuteIfBound(EHotfixResult::SuccessNoChange);
		return;
	}

	if (bHotfixingInProgress)
	{
		UE_LOG(LogHotfixManager, Warning, TEXT("Hotfixing availability skipped because hotfix in progress"));
		InCompletionDelegate.ExecuteIfBound(EHotfixResult::Failed);
		return;
	}

	OnlineTitleFile = Online::GetTitleFileInterface(OSSName.Len() ? FName(*OSSName, FNAME_Find) : NAME_None);

	FOnEnumerateFilesCompleteDelegate OnEnumerateFilesForAvailabilityCompleteDelegate;
	OnEnumerateFilesForAvailabilityCompleteDelegate.BindUObject(this, &UOnlineHotfixManager::OnEnumerateFilesForAvailabilityComplete, InCompletionDelegate);
	OnEnumerateFilesForAvailabilityCompleteDelegateHandle = OnlineTitleFile->AddOnEnumerateFilesCompleteDelegate_Handle(OnEnumerateFilesForAvailabilityCompleteDelegate);

	bHotfixingInProgress = true;

	// Kick off an enumeration of the files that are available to download
	if (OnlineTitleFile.IsValid())
	{
		OnlineTitleFile->EnumerateFiles();
	}
	else
	{
		UE_LOG(LogHotfixManager, Error, TEXT("Failed to start the hotfix check process due to no OnlineTitleInterface present for OSS(%s)"), *OSSName);
		TriggerHotfixComplete(EHotfixResult::Failed);
	}
}

void UOnlineHotfixManager::OnEnumerateFilesForAvailabilityComplete(bool bWasSuccessful, const FString& ErrorStr, FOnHotfixAvailableComplete InCompletionDelegate)
{
	if (OnlineTitleFile.IsValid())
	{
		OnlineTitleFile->ClearOnEnumerateFilesCompleteDelegate_Handle(OnEnumerateFilesForAvailabilityCompleteDelegateHandle);
	}
	
	EHotfixResult Result = EHotfixResult::Failed;
	if (bWasSuccessful)
	{
		TArray<FCloudFileHeader> TmpHotfixFileList;
		TArray<FCloudFileHeader> TmpLastHotfixFileList;

		TmpHotfixFileList = HotfixFileList;
		TmpLastHotfixFileList = LastHotfixFileList;

		// Cache our current set so we can compare for differences
		LastHotfixFileList = HotfixFileList;
		HotfixFileList.Empty();
		// Get the new header data
		OnlineTitleFile->GetFileList(HotfixFileList);
		FilterHotfixFiles();
		// Reduce the set of work to just the files that changed since last run
		BuildHotfixFileListDeltas();

		// Read any changed files
		if (ChangedHotfixFileList.Num() > 0 || RemovedHotfixFileList.Num() > 0)
		{
			UE_LOG(LogHotfixManager, Display, TEXT("Hotfix files available"));
			Result = EHotfixResult::Success;
		}
		else
		{
			UE_LOG(LogHotfixManager, Display, TEXT("Returned hotfix data is the same as last application, returning nothing to do"));
			Result = EHotfixResult::SuccessNoChange;
		}

		// Restore state to before the check
		RemovedHotfixFileList.Empty();
		ChangedHotfixFileList.Empty();
		HotfixFileList = TmpHotfixFileList;
		LastHotfixFileList = TmpLastHotfixFileList;
	}
	else
	{
		UE_LOG(LogHotfixManager, Error, TEXT("Enumeration of hotfix files failed"));
	}

	OnlineTitleFile = nullptr;
	bHotfixingInProgress = false;
	InCompletionDelegate.ExecuteIfBound(Result);
}

void UOnlineHotfixManager::BuildHotfixFileListDeltas()
{
	RemovedHotfixFileList.Empty();
	ChangedHotfixFileList.Empty();
	// Go through the current list and see if it's changed from the previous attempt
	for (auto& CurrentHeader : HotfixFileList)
	{
		bool bFoundMatch = false;
		for (auto& LastHeader : LastHotfixFileList)
		{
			if (LastHeader == CurrentHeader)
			{
				bFoundMatch = true;
				break;
			}
		}
		if (!bFoundMatch)
		{
			// We're different so add to the process list
			ChangedHotfixFileList.Add(CurrentHeader);
		}
	}
	// Find any files that have been removed from the set of hotfix files
	for (auto& LastHeader : LastHotfixFileList)
	{
		bool bFoundMatch = false;
		for (auto& CurrentHeader : HotfixFileList)
		{
			if (LastHeader.FileName == CurrentHeader.FileName)
			{
				bFoundMatch = true;
				break;
			}
		}
		if (!bFoundMatch)
		{
			// We've been removed so add to the removed list
			RemovedHotfixFileList.Add(LastHeader);
		}
	}
}

void UOnlineHotfixManager::FilterHotfixFiles()
{
	for (int32 Idx = 0; Idx < HotfixFileList.Num(); Idx++)
	{
		if (!WantsHotfixProcessing(HotfixFileList[Idx]))
		{
			HotfixFileList.RemoveAt(Idx, 1, false);
			Idx--;
		}
	}
}

void UOnlineHotfixManager::ReadHotfixFiles()
{
	if (ChangedHotfixFileList.Num())
	{
		check(OnlineTitleFile.IsValid());
		// Kick off a read for each file
		// Do this in two passes so already cached files don't trigger completion
		for (auto& FileHeader : ChangedHotfixFileList)
		{
			UE_LOG(LogOnline, VeryVerbose, TEXT("HF: %s %s %d "), *FileHeader.DLName, *FileHeader.FileName, FileHeader.FileSize);
			PendingHotfixFiles.Add(FileHeader.DLName, FPendingFileDLProgress());
		}
		for (auto& FileHeader : ChangedHotfixFileList)
		{
			OnlineTitleFile->ReadFile(FileHeader.DLName);
		}
	}
	else
	{
		UE_LOG(LogHotfixManager, Display, TEXT("No hotfix files need to be downloaded"));
		TriggerHotfixComplete(EHotfixResult::Success);
	}
}

void UOnlineHotfixManager::OnReadFileComplete(bool bWasSuccessful, const FString& FileName)
{
	if (PendingHotfixFiles.Contains(FileName))
	{
		if (bWasSuccessful)
		{
			FCloudFileHeader* Header = GetFileHeaderFromDLName(FileName);
			check(Header != nullptr);
			UE_LOG(LogHotfixManager, Log, TEXT("Hotfix file (%s) downloaded. Size was (%d)"), *GetFriendlyNameFromDLName(FileName), Header->FileSize);
			// Completion updates the file count and progress updates the byte count
			UpdateProgress(1, 0);
			PendingHotfixFiles.Remove(FileName);
			if (PendingHotfixFiles.Num() == 0)
			{
				// All files have been downloaded so now apply the files
				ApplyHotfix();
			}
		}
		else
		{
			UE_LOG(LogHotfixManager, Error, TEXT("Hotfix file (%s) failed to download"), *GetFriendlyNameFromDLName(FileName));
			TriggerHotfixComplete(EHotfixResult::Failed);
		}
	}
}

void UOnlineHotfixManager::UpdateProgress(uint32 FileCount, uint64 UpdateSize)
{
	NumDownloaded += FileCount;
	NumBytes += UpdateSize;
	// Update our progress
	TriggerOnHotfixProgressDelegates(NumDownloaded, TotalFiles, NumBytes, TotalBytes);
}

void UOnlineHotfixManager::ApplyHotfix()
{
	for (auto& FileHeader : ChangedHotfixFileList)
	{
		if (!ApplyHotfixProcessing(FileHeader))
		{
			UE_LOG(LogHotfixManager, Error, TEXT("Couldn't apply hotfix file (%s)"), *FileHeader.FileName);
			TriggerHotfixComplete(EHotfixResult::Failed);
			return;
		}
		// Let anyone listening know we just processed this file
		TriggerOnHotfixProcessedFileDelegates(FileHeader.FileName, GetCachedDirectory() / FileHeader.DLName);
	}
	UE_LOG(LogHotfixManager, Display, TEXT("Hotfix data has been successfully applied"));
	EHotfixResult Result = EHotfixResult::Success;
	if (ChangedOrRemovedPakCount > 0)
	{
		UE_LOG(LogHotfixManager, Display, TEXT("Hotfix has changed or removed PAK files so a relaunch of the app is needed"));
		Result = EHotfixResult::SuccessNeedsRelaunch;
	}
	else if (bHotfixNeedsMapReload)
	{
		UE_LOG(LogHotfixManager, Display, TEXT("Hotfix has detected PAK files containing currently loaded maps, so a level load is needed"));
		Result = EHotfixResult::SuccessNeedsReload;
	}
	TriggerHotfixComplete(Result);
}

void UOnlineHotfixManager::TriggerHotfixComplete(EHotfixResult HotfixResult)
{
	TriggerOnHotfixCompleteDelegates(HotfixResult);
	if (HotfixResult == EHotfixResult::Failed)
	{
		HotfixFileList.Empty();
		UnmountHotfixFiles();
	}
	Cleanup();
}

bool UOnlineHotfixManager::WantsHotfixProcessing(const FCloudFileHeader& FileHeader)
{
	const FString Extension = FPaths::GetExtension(FileHeader.FileName);
	if (Extension == TEXT("INI"))
	{
		bool bIsServerHotfix = FileHeader.FileName.StartsWith(ServerPrefix);
		bool bWantsServerHotfix = IsRunningDedicatedServer() && bIsServerHotfix;
		bool bWantsDefaultHotfix = FileHeader.FileName.StartsWith(DefaultPrefix);
		bool bWantsPlatformHotfix = FileHeader.FileName.StartsWith(PlatformPrefix);

		if (bWantsPlatformHotfix)
		{
			UE_LOG(LogHotfixManager, Verbose, TEXT("Using platform hotfix %s"), * FileHeader.FileName);
		}
		else if (bWantsServerHotfix)
		{
			UE_LOG(LogHotfixManager, Verbose, TEXT("Using server hotfix %s"), * FileHeader.FileName);
		}
		else if (bWantsDefaultHotfix)
		{
			UE_LOG(LogHotfixManager, Verbose, TEXT("Using default hotfix %s"), * FileHeader.FileName);
		}

		return bWantsPlatformHotfix || bWantsServerHotfix || bWantsDefaultHotfix;
	}
	else if (Extension == TEXT("PAK"))
	{
		return FileHeader.FileName.Find(PlatformPrefix) != -1;
	}
	return FileHeader.FileName == GameLocName;
}

bool UOnlineHotfixManager::ApplyHotfixProcessing(const FCloudFileHeader& FileHeader)
{
	bool bSuccess = false;
	const FString Extension = FPaths::GetExtension(FileHeader.FileName);
	if (Extension == TEXT("INI"))
	{
		TArray<uint8> FileData;
		if (OnlineTitleFile->GetFileContents(FileHeader.DLName, FileData))
		{
			// Convert to a FString
			FileData.Add(0);
			FString HotfixStr;
			FFileHelper::BufferToString(HotfixStr, FileData.GetData(), FileData.Num());
			bSuccess = HotfixIniFile(FileHeader.FileName, HotfixStr);
		}
	}
	else if (Extension == TEXT("LOCRES"))
	{
		HotfixLocFile(FileHeader);
		// Currently no failure case for this
		bSuccess = true;
	}
	else if (Extension == TEXT("PAK"))
	{
		bSuccess = HotfixPakFile(FileHeader);
	}
	OnlineTitleFile->ClearFile(FileHeader.FileName);
	return bSuccess;
}

FString UOnlineHotfixManager::GetStrippedConfigFileName(const FString& IniName)
{
	FString StrippedIniName(IniName);
	if (StrippedIniName.StartsWith(PlatformPrefix))
	{
		StrippedIniName = IniName.Right(StrippedIniName.Len() - PlatformPrefix.Len());
	}
	else if (StrippedIniName.StartsWith(DefaultPrefix))
	{
		StrippedIniName = IniName.Right(StrippedIniName.Len() - DefaultPrefix.Len());
	}
	else if (StrippedIniName.StartsWith(DebugPrefix))
	{
		StrippedIniName = IniName.Right(StrippedIniName.Len() - DebugPrefix.Len());
	}
	return StrippedIniName;
}

FString UOnlineHotfixManager::GetConfigFileNamePath(const FString& IniName)
{
	return FPaths::GeneratedConfigDir() + ANSI_TO_TCHAR(FPlatformProperties::PlatformName()) / IniName;
}

FConfigFile* UOnlineHotfixManager::GetConfigFile(const FString& IniName)
{
	FString StrippedIniName(GetStrippedConfigFileName(IniName));
	FConfigFile* ConfigFile = nullptr;
	// Look for the first matching INI file entry
	for (TMap<FString, FConfigFile>::TIterator It(*GConfig); It; ++It)
	{
		if (It.Key().EndsWith(StrippedIniName))
		{
			ConfigFile = &It.Value();
			break;
		}
	}
	// If not found, add this file to the config cache
	if (ConfigFile == nullptr)
	{
		const FString IniNameWithPath = GetConfigFileNamePath(StrippedIniName);
		FConfigFile Empty;
		GConfig->SetFile(IniNameWithPath, &Empty);
		ConfigFile = GConfig->Find(IniNameWithPath, false);
	}
	check(ConfigFile);
	// We never want to save these merged files
	ConfigFile->NoSave = true;
	return ConfigFile;
}

bool UOnlineHotfixManager::HotfixIniFile(const FString& FileName, const FString& IniData)
{
	FConfigFile* ConfigFile = GetConfigFile(FileName);
	// Store the original file so we can undo this later
	FConfigFileBackup& BackupFile = BackupIniFile(FileName, ConfigFile);
	// Merge the string into the config file
	ConfigFile->CombineFromBuffer(IniData);
	TArray<UClass*> Classes;
	TArray<UObject*> PerObjectConfigObjects;
	int32 StartIndex = 0;
	int32 EndIndex = 0;
	bool bUpdateLogSuppression = false;
	// Find the set of object classes that were affected
	while (StartIndex >= 0 && StartIndex < IniData.Len() && EndIndex >= StartIndex)
	{
		// Find the next section header
		StartIndex = IniData.Find(TEXT("["), ESearchCase::IgnoreCase, ESearchDir::FromStart, StartIndex);
		if (StartIndex > -1)
		{
			// Find the ending section identifier
			EndIndex = IniData.Find(TEXT("]"), ESearchCase::IgnoreCase, ESearchDir::FromStart, StartIndex);
			if (EndIndex > StartIndex)
			{
				int32 PerObjectNameIndex = IniData.Find(TEXT(" "), ESearchCase::IgnoreCase, ESearchDir::FromStart, StartIndex);
				// Per object config entries will have a space in the name, but classes won't
				if (PerObjectNameIndex == -1 || PerObjectNameIndex > EndIndex)
				{
					const TCHAR* ScriptHeader = TEXT("[/Script/");
					if (FCString::Strnicmp(*IniData + StartIndex, ScriptHeader, FCString::Strlen(ScriptHeader)) == 0)
					{
						const int32 ScriptSectionTag = 9;
						// Snip the text out and try to find the class for that
						const FString PackageClassName = IniData.Mid(StartIndex + ScriptSectionTag, EndIndex - StartIndex - ScriptSectionTag);
						// Find the class for this so we know what to update
						UClass* Class = FindObject<UClass>(nullptr, *PackageClassName, true);
						if (Class)
						{
							// Add this to the list to check against
							Classes.Add(Class);
							BackupFile.ClassesReloaded.AddUnique(Class->GetPathName());
						}
					}
					else if (FileName.Contains(TEXT("Engine.ini")))
					{
						const TCHAR* LogConfigSection = TEXT("[Core.Log]");
						if (FCString::Strnicmp(*IniData + StartIndex, LogConfigSection, FCString::Strlen(LogConfigSection)) == 0)
						{
							bUpdateLogSuppression = true;
						}
					}
				}
				// Handle the per object config case by finding the object for reload
				else
				{
					const int32 ClassNameStart = PerObjectNameIndex + 1;
					const FString ClassName = IniData.Mid(ClassNameStart, EndIndex - ClassNameStart);

					// Look up the class to search for
					UClass* ObjectClass = FindObject<UClass>(ANY_PACKAGE, *ClassName);

					if (ObjectClass)
					{
						const int32 Count = PerObjectNameIndex - StartIndex - 1;
						const FString PerObjectName = IniData.Mid(StartIndex + 1, Count);

						// Explicitly search the transient package (won't update non-transient objects)
						UObject* PerObject = StaticFindObject(ObjectClass, ANY_PACKAGE, *PerObjectName, false);
						if (PerObject != nullptr)
						{
							PerObjectConfigObjects.Add(PerObject);
							BackupFile.ClassesReloaded.AddUnique(ObjectClass->GetPathName());
						}
					}
					else
					{
						UE_LOG(LogHotfixManager, Warning, TEXT("Specified per-object class %s was not found"), *ClassName);
					}
				}
				StartIndex = EndIndex;
			}
		}
	}

	int32 NumObjectsReloaded = 0;
	const double StartTime = FPlatformTime::Seconds();
	if (Classes.Num())
	{
		// Now that we have a list of classes to update, we can iterate objects and reload
		for (FObjectIterator It; It; ++It)
		{
			UClass* Class = It->GetClass();
			if (Class->HasAnyClassFlags(CLASS_Config))
			{
				// Check to see if this class is in our list (yes, potentially n^2, but not in practice)
				for (int32 ClassIndex = 0; ClassIndex < Classes.Num(); ClassIndex++)
				{
					if (It->IsA(Classes[ClassIndex]))
					{
						// Force a reload of the config vars
						UE_LOG(LogHotfixManager, Verbose, TEXT("Reloading %s"), *It->GetPathName());
						It->ReloadConfig();
						NumObjectsReloaded++;
						break;
					}
				}
			}
		}
	}
	// Reload any PerObjectConfig objects that were affected
	for (auto ReloadObject : PerObjectConfigObjects)
	{
		UE_LOG(LogHotfixManager, Verbose, TEXT("Reloading %s"), *ReloadObject->GetPathName());
		ReloadObject->ReloadConfig();
		NumObjectsReloaded++;
	}

	// Reload log suppression if configs changed
	if (bUpdateLogSuppression)
	{
		FLogSuppressionInterface::Get().ProcessConfigAndCommandLine();
	}

	UE_LOG(LogHotfixManager, Log, TEXT("Updating config from %s took %f seconds and reloaded %d objects"),
		*FileName, FPlatformTime::Seconds() - StartTime, NumObjectsReloaded);
	return true;
}

void UOnlineHotfixManager::HotfixLocFile(const FCloudFileHeader& FileHeader)
{
	const double StartTime = FPlatformTime::Seconds();
	FString LocFilePath = FString::Printf(TEXT("%s/%s"), *GetCachedDirectory(), *FileHeader.DLName);
	FTextLocalizationManager::Get().UpdateFromLocalizationResource(LocFilePath);
	UE_LOG(LogHotfixManager, Log, TEXT("Updating loc from %s took %f seconds"), *FileHeader.FileName, FPlatformTime::Seconds() - StartTime);
}

bool UOnlineHotfixManager::HotfixPakFile(const FCloudFileHeader& FileHeader)
{
	if (!FCoreDelegates::OnMountPak.IsBound())
	{
		UE_LOG(LogHotfixManager, Error, TEXT("PAK file (%s) could not be mounted because OnMountPak is not bound"), *FileHeader.FileName);
		return false;
	}
	FString PakLocation = FString::Printf(TEXT("%s/%s"), *GetCachedDirectory(), *FileHeader.DLName);
	FPakFileVisitor Visitor;
	if (FCoreDelegates::OnMountPak.Execute(PakLocation, 0, &Visitor))
	{
		MountedPakFiles.Add(FileHeader.DLName);
		UE_LOG(LogHotfixManager, Log, TEXT("Hotfix mounted PAK file (%s)"), *FileHeader.FileName);
		int32 NumInisReloaded = 0;
		const double StartTime = FPlatformTime::Seconds();
		TArray<FString> IniList;
		// Iterate through the pak file's contents for INI and asset reloading
		for (auto& InternalPakFileName : Visitor.Files)
		{
			if (InternalPakFileName.EndsWith(TEXT(".ini")))
			{
				IniList.Add(InternalPakFileName);
			}
			else if (!bHotfixNeedsMapReload && InternalPakFileName.EndsWith(FPackageName::GetMapPackageExtension()))
			{
				bHotfixNeedsMapReload = IsMapLoaded(InternalPakFileName);
			}
		}
		// Sort the INIs so they are processed consistently
		IniList.Sort<FHotfixFileSortPredicate>(FHotfixFileSortPredicate(PlatformPrefix, ServerPrefix, DefaultPrefix));
		// Now process the INIs in sorted order
		for (auto& IniName : IniList)
		{
			HotfixPakIniFile(IniName);
			NumInisReloaded++;
		}
		UE_LOG(LogHotfixManager, Log, TEXT("Processing pak file (%s) took %f seconds and resulted in (%d) INIs being reloaded"),
			*FileHeader.FileName, FPlatformTime::Seconds() - StartTime, NumInisReloaded);
#if !UE_BUILD_SHIPPING
		if (bLogMountedPakContents)
		{
			UE_LOG(LogHotfixManager, Log, TEXT("Files in pak file (%s):"), *FileHeader.FileName);
			for (auto& FileName : Visitor.Files)
			{
				UE_LOG(LogHotfixManager, Log, TEXT("\t\t%s"), *FileName);
			}
		}
#endif
		return true;
	}
	return false;
}

bool UOnlineHotfixManager::IsMapLoaded(const FString& MapName)
{
	FString MapPackageName(MapName.Left(MapName.Len() - 5));
	MapPackageName = MapPackageName.Replace(*GameContentPath, TEXT("/Game"));
	// If this map's UPackage exists, it is currently loaded
	UPackage* MapPackage = FindObject<UPackage>(ANY_PACKAGE, *MapPackageName, true);
	return MapPackage != nullptr;
}

bool UOnlineHotfixManager::HotfixPakIniFile(const FString& FileName)
{
	FString StrippedName;
	const double StartTime = FPlatformTime::Seconds();
	// Need to strip off the PAK path
	FileName.Split(TEXT("/"), nullptr, &StrippedName, ESearchCase::IgnoreCase, ESearchDir::FromEnd);
	FConfigFile* ConfigFile = GetConfigFile(StrippedName);
	if (!ConfigFile->Combine(FString(TEXT("../../../")) + FileName.Replace(*GameContentPath, TEXT("/Game"))))
	{
		UE_LOG(LogHotfixManager, Log, TEXT("Hotfix failed to merge INI (%s) found in a PAK file"), *FileName);
		return false;
	}
	UE_LOG(LogHotfixManager, Log, TEXT("Hotfix merged INI (%s) found in a PAK file"), *FileName);
	int32 NumObjectsReloaded = 0;
	// Now that we have a list of classes to update, we can iterate objects and
	// reload if they match the INI file that was changed
	for (FObjectIterator It; It; ++It)
	{
		UClass* Class = It->GetClass();
		if (Class->HasAnyClassFlags(CLASS_Config) &&
			Class->ClassConfigName == ConfigFile->Name)
		{
			// Force a reload of the config vars
			It->ReloadConfig();
			NumObjectsReloaded++;
		}
	}
	UE_LOG(LogHotfixManager, Log, TEXT("Updating config from %s took %f seconds reloading %d objects"),
		*FileName, FPlatformTime::Seconds() - StartTime, NumObjectsReloaded);
	return true;
}

const FString UOnlineHotfixManager::GetFriendlyNameFromDLName(const FString& DLName) const
{
	for (auto& Header : HotfixFileList)
	{
		if (Header.DLName == DLName)
		{
			return Header.FileName;
		}
	}
	return FString();
}

void UOnlineHotfixManager::UnmountHotfixFiles()
{
	if (MountedPakFiles.Num() == 0)
	{
		return;
	}
	// Unmount any changed hotfix files since we need to download them again
	for (auto& FileHeader : ChangedHotfixFileList)
	{
		for (int32 Index = 0; Index < MountedPakFiles.Num(); Index++)
		{
			if (MountedPakFiles[Index] == FileHeader.DLName)
			{
				FCoreDelegates::OnUnmountPak.Execute(MountedPakFiles[Index]);
				MountedPakFiles.RemoveAt(Index);
				ChangedOrRemovedPakCount++;
				UE_LOG(LogHotfixManager, Log, TEXT("Hotfix unmounted PAK file (%s) so it can be redownloaded"), *FileHeader.FileName);
				break;
			}
		}
	}
	// Unmount any removed hotfix files
	for (auto& FileHeader : RemovedHotfixFileList)
	{
		for (int32 Index = 0; Index < MountedPakFiles.Num(); Index++)
		{
			if (MountedPakFiles[Index] == FileHeader.DLName)
			{
				FCoreDelegates::OnUnmountPak.Execute(MountedPakFiles[Index]);
				MountedPakFiles.RemoveAt(Index);
				ChangedOrRemovedPakCount++;
				UE_LOG(LogHotfixManager, Log, TEXT("Hotfix unmounted PAK file (%s) since it was removed from the hotfix set"), *FileHeader.FileName);
				break;
			}
		}
	}
}

FCloudFileHeader* UOnlineHotfixManager::GetFileHeaderFromDLName(const FString& FileName)
{
	for (int32 Index = 0; Index < HotfixFileList.Num(); Index++)
	{
		if (HotfixFileList[Index].DLName == FileName)
		{
			return &HotfixFileList[Index];
		}
	}
	return nullptr;
}

void UOnlineHotfixManager::OnReadFileProgress(const FString& FileName, uint64 BytesRead)
{
	if (PendingHotfixFiles.Contains(FileName))
	{
		// Since the title file is reporting absolute numbers subtract out the last update so we can add a delta
		uint64 Delta = BytesRead - PendingHotfixFiles[FileName].Progress;
		PendingHotfixFiles[FileName].Progress = BytesRead;
		// Completion updates the file count and progress updates the byte count
		UpdateProgress(0, Delta);
	}
}

UOnlineHotfixManager::FConfigFileBackup& UOnlineHotfixManager::BackupIniFile(const FString& IniName, const FConfigFile* ConfigFile)
{
	int32 AddAt = IniBackups.AddDefaulted();
	FConfigFileBackup& NewBackup = IniBackups[AddAt];
	NewBackup.IniName = GetConfigFileNamePath(GetStrippedConfigFileName(IniName));
	NewBackup.ConfigData = *ConfigFile;
	// There's a lack of deep copy related to the SourceConfigFile so null it out
	NewBackup.ConfigData.SourceConfigFile = nullptr;
	return NewBackup;
}

void UOnlineHotfixManager::RestoreBackupIniFiles()
{
	if (IniBackups.Num() == 0)
	{
		return;
	}
	const double StartTime = FPlatformTime::Seconds();
	TArray<FString> ClassesToRestore;

	// Restore any changed INI files and build a list of which ones changed for UObject reloading below
	for (auto& FileHeader : ChangedHotfixFileList)
	{
		if (FileHeader.FileName.EndsWith(TEXT(".INI")))
		{
			const FString ProcessedName = GetConfigFileNamePath(GetStrippedConfigFileName(FileHeader.FileName));
			for (int32 Index = 0; Index < IniBackups.Num(); Index++)
			{
				const FConfigFileBackup& BackupFile = IniBackups[Index];
				if (IniBackups[Index].IniName == ProcessedName)
				{
					ClassesToRestore.Append(BackupFile.ClassesReloaded);

					GConfig->SetFile(BackupFile.IniName, &BackupFile.ConfigData);
					IniBackups.RemoveAt(Index);
					break;
				}
			}
		}
	}

	// Also restore any files that were previously part of the hotfix and now are not
	for (auto& FileHeader : RemovedHotfixFileList)
	{
		if (FileHeader.FileName.EndsWith(TEXT(".INI")))
		{
			const FString ProcessedName = GetConfigFileNamePath(GetStrippedConfigFileName(FileHeader.FileName));
			for (int32 Index = 0; Index < IniBackups.Num(); Index++)
			{
				const FConfigFileBackup& BackupFile = IniBackups[Index];
				if (BackupFile.IniName == ProcessedName)
				{
					ClassesToRestore.Append(BackupFile.ClassesReloaded);

					GConfig->SetFile(BackupFile.IniName, &BackupFile.ConfigData);
					IniBackups.RemoveAt(Index);
					break;
				}
			}
		}
	}

	uint32 NumObjectsReloaded = 0;
	if (ClassesToRestore.Num() > 0)
	{
		TArray<UClass*> RestoredClasses;
		RestoredClasses.Reserve(ClassesToRestore.Num());
		for (int32 Index = 0; Index < ClassesToRestore.Num(); Index++)
		{
			UClass* Class = FindObject<UClass>(nullptr, *ClassesToRestore[Index], true);
			if (Class != nullptr)
			{
				// Add this to the list to check against
				RestoredClasses.Add(Class);
			}
		}

		for (FObjectIterator It; It; ++It)
		{
			UClass* Class = It->GetClass();
			if (Class->HasAnyClassFlags(CLASS_Config))
			{
				// Check to see if this class is in our list (yes, potentially n^2, but not in practice)
				for (int32 ClassIndex = 0; ClassIndex < RestoredClasses.Num(); ClassIndex++)
				{
					if (It->IsA(RestoredClasses[ClassIndex]))
					{
						UE_LOG(LogHotfixManager, Verbose, TEXT("Restoring %s"), *It->GetPathName());
						It->ReloadConfig();
						NumObjectsReloaded++;
						break;
					}
				}
			}
		}
	}
	UE_LOG(LogHotfixManager, Log, TEXT("Restoring config for %d changed classes took %f seconds reloading %d objects"),
		ClassesToRestore.Num(), FPlatformTime::Seconds() - StartTime, NumObjectsReloaded);
}

struct FHotfixManagerExec :
	public FSelfRegisteringExec
{
	virtual bool Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override
	{
		if (FParse::Command(&Cmd, TEXT("HOTFIX")))
		{
			UOnlineHotfixManager* HotfixManager = UOnlineHotfixManager::Get(InWorld);
			if (HotfixManager != nullptr)
			{
				HotfixManager->StartHotfixProcess();
			}
			return true;
		}
		else if (FParse::Command(&Cmd, TEXT("TESTHOTFIXSORT")))
		{
			TArray<FCloudFileHeader> TestList;
			FCloudFileHeader Header;
			Header.FileName = TEXT("SomeRandom.ini");
			TestList.Add(Header);
			Header.FileName = TEXT("DedicatedServerGame.ini");
			TestList.Add(Header);
			Header.FileName = TEXT("pakchunk1-PS4_P.pak");
			TestList.Add(Header);
			Header.FileName = TEXT("EN_Game.locres");
			TestList.Add(Header);
			Header.FileName = TEXT("DefaultGame.ini");
			TestList.Add(Header);
			Header.FileName = TEXT("PS4_DefaultEngine.ini");
			TestList.Add(Header);
			Header.FileName = TEXT("DefaultEngine.ini");
			TestList.Add(Header);
			Header.FileName = TEXT("pakchunk0-PS4_P.pak");
			TestList.Add(Header);
			Header.FileName = TEXT("PS4_DefaultGame.ini");
			TestList.Add(Header);
			Header.FileName = TEXT("AnotherRandom.ini");
			TestList.Add(Header);
			Header.FileName = TEXT("DedicatedServerEngine.ini");
			TestList.Add(Header);
			TestList.Sort<FHotfixFileSortPredicate>(FHotfixFileSortPredicate(TEXT("PS4_"), TEXT("DedicatedServer"), TEXT("Default")));

			UE_LOG(LogHotfixManager, Log, TEXT("Hotfixing sort is:"));
			for (auto& FileHeader : TestList)
			{
				UE_LOG(LogHotfixManager, Log, TEXT("\t%s"), *FileHeader.FileName);
			}

			TArray<FString> TestList2;
			TestList2.Add(TEXT("SomeRandom.ini"));
			TestList2.Add(TEXT("DefaultGame.ini"));
			TestList2.Add(TEXT("PS4_DefaultEngine.ini"));
			TestList2.Add(TEXT("DedicatedServerEngine.ini"));
			TestList2.Add(TEXT("DedicatedServerGame.ini"));
			TestList2.Add(TEXT("DefaultEngine.ini"));
			TestList2.Add(TEXT("PS4_DefaultGame.ini"));
			TestList2.Add(TEXT("AnotherRandom.ini"));
			TestList2.Sort<FHotfixFileSortPredicate>(FHotfixFileSortPredicate(TEXT("PS4_"), TEXT("DedicatedServer"), TEXT("Default")));

			UE_LOG(LogHotfixManager, Log, TEXT("Hotfixing PAK INI file sort is:"));
			for (auto& IniName : TestList2)
			{
				UE_LOG(LogHotfixManager, Log, TEXT("\t%s"), *IniName);
			}
			return true;
		}
		return false;
	}
};
static FHotfixManagerExec HotfixManagerExec;
