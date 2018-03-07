// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Linux/DesktopPlatformLinux.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Misc/Guid.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FeedbackContext.h"
#include "DesktopPlatformPrivate.h"
#include "Modules/ModuleManager.h"
#include "Linux/LinuxApplication.h"
#include "Misc/FeedbackContextMarkup.h"
#include "HAL/ThreadHeartBeat.h"

//#include "LinuxNativeFeedbackContext.h"
#include "ISlateFileDialogModule.h"

#define LOCTEXT_NAMESPACE "DesktopPlatform"
#define MAX_FILETYPES_STR 4096
#define MAX_FILENAME_STR 65536

FDesktopPlatformLinux::FDesktopPlatformLinux()
	:	FDesktopPlatformBase()
{
}

FDesktopPlatformLinux::~FDesktopPlatformLinux()
{
}

bool FDesktopPlatformLinux::OpenFileDialog(const void* ParentWindowHandle, const FString& DialogTitle, const FString& DefaultPath, const FString& DefaultFile, const FString& FileTypes, uint32 Flags, TArray<FString>& OutFilenames, int32& OutFilterIndex)
{
	if (!FModuleManager::Get().IsModuleLoaded("SlateFileDialogs"))
	{
		FModuleManager::Get().LoadModule("SlateFileDialogs");
	}

	ISlateFileDialogsModule *FileDialog = FModuleManager::GetModulePtr<ISlateFileDialogsModule>("SlateFileDialogs");

	if (FileDialog)
	{
		return FileDialog->OpenFileDialog(ParentWindowHandle, DialogTitle, DefaultPath, DefaultFile, FileTypes, Flags, OutFilenames, OutFilterIndex);
	}

	return false;
}

bool FDesktopPlatformLinux::OpenFileDialog(const void* ParentWindowHandle, const FString& DialogTitle, const FString& DefaultPath, const FString& DefaultFile, const FString& FileTypes, uint32 Flags, TArray<FString>& OutFilenames)
{
	if (!FModuleManager::Get().IsModuleLoaded("SlateFileDialogs"))
	{
		FModuleManager::Get().LoadModule("SlateFileDialogs");
	}

	ISlateFileDialogsModule *FileDialog = FModuleManager::GetModulePtr<ISlateFileDialogsModule>("SlateFileDialogs");

	if (FileDialog)
	{
		return FileDialog->OpenFileDialog(ParentWindowHandle, DialogTitle, DefaultPath, DefaultFile, FileTypes, Flags, OutFilenames);
	}

	return false;
}

bool FDesktopPlatformLinux::SaveFileDialog(const void* ParentWindowHandle, const FString& DialogTitle, const FString& DefaultPath, const FString& DefaultFile, const FString& FileTypes, uint32 Flags, TArray<FString>& OutFilenames)
{
	if (!FModuleManager::Get().IsModuleLoaded("SlateFileDialogs"))
	{
		FModuleManager::Get().LoadModule("SlateFileDialogs");
	}

	ISlateFileDialogsModule *FileDialog = FModuleManager::GetModulePtr<ISlateFileDialogsModule>("SlateFileDialogs");

	if (FileDialog)
	{
		return FileDialog->SaveFileDialog(ParentWindowHandle, DialogTitle, DefaultPath, DefaultFile, FileTypes, Flags, OutFilenames);
	}

	return false;
}

bool FDesktopPlatformLinux::OpenDirectoryDialog(const void* ParentWindowHandle, const FString& DialogTitle, const FString& DefaultPath, FString& OutFolderName)
{
	if (!FModuleManager::Get().IsModuleLoaded("SlateFileDialogs"))
	{
		FModuleManager::Get().LoadModule("SlateFileDialogs");
	}

	ISlateFileDialogsModule *FileDialog = FModuleManager::GetModulePtr<ISlateFileDialogsModule>("SlateFileDialogs");

	if (FileDialog)
	{
		return FileDialog->OpenDirectoryDialog(ParentWindowHandle, DialogTitle, DefaultPath, OutFolderName);
	}

	return false;
}

bool FDesktopPlatformLinux::OpenFontDialog(const void* ParentWindowHandle, FString& OutFontName, float& OutHeight, EFontImportFlags& OutFlags)
{
	unimplemented();
	return false;
}

bool FDesktopPlatformLinux::FileDialogShared(bool bSave, const void* ParentWindowHandle, const FString& DialogTitle, const FString& DefaultPath, const FString& DefaultFile, const FString& FileTypes, uint32 Flags, TArray<FString>& OutFilenames, int32& OutFilterIndex)
{
	return false;
}

bool FDesktopPlatformLinux::RegisterEngineInstallation(const FString &RootDir, FString &OutIdentifier)
{
	bool bRes = false;
	if (IsValidRootDirectory(RootDir))
	{
		FConfigFile ConfigFile;
		FString ConfigPath = FString(FPlatformProcess::ApplicationSettingsDir()) / FString(TEXT("UnrealEngine")) / FString(TEXT("Install.ini"));
		ConfigFile.Read(ConfigPath);

		FConfigSection &Section = ConfigFile.FindOrAdd(TEXT("Installations"));
		OutIdentifier = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensInBraces);
		Section.AddUnique(*OutIdentifier, RootDir);

		ConfigFile.Dirty = true;
		ConfigFile.Write(ConfigPath);
	}
	return bRes;
}

void FDesktopPlatformLinux::EnumerateEngineInstallations(TMap<FString, FString> &OutInstallations)
{
	EnumerateLauncherEngineInstallations(OutInstallations);

	FString UProjectPath = FString(FPlatformProcess::ApplicationSettingsDir()) / "Unreal.uproject";
	FArchive* File = IFileManager::Get().CreateFileWriter(*UProjectPath, FILEWRITE_EvenIfReadOnly);
	if (File)
	{
		File->Close();
		delete File;
	}
	else
	{
		FSlowHeartBeatScope SuspendHeartBeat;
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Unable to write to Settings Directory", TCHAR_TO_UTF8(*UProjectPath), NULL);
	}

	FConfigFile ConfigFile;
	FString ConfigPath = FString(FPlatformProcess::ApplicationSettingsDir()) / FString(TEXT("UnrealEngine")) / FString(TEXT("Install.ini"));
	ConfigFile.Read(ConfigPath);

	FConfigSection &Section = ConfigFile.FindOrAdd(TEXT("Installations"));
	// Remove invalid entries
	// @todo The installations list might contain multiple keys for the same value. Do we have to remove them?
	TArray<FName> KeysToRemove;
	for (auto It : Section)
	{
		const FString& EngineDir = It.Value.GetValue();
		// We remove entries pointing to a folder that doesn't exist or was using the wrong path.
		if (EngineDir.Contains(FPaths::EngineDir()) || !IFileManager::Get().DirectoryExists(*EngineDir))
		{
			KeysToRemove.Add(It.Key);
			ConfigFile.Dirty = true;
		}
	}
	for (auto Key : KeysToRemove)
	{
		Section.Remove(Key);
	}

	// @todo: currently we can enumerate only this installation
	FString EngineDir = FPaths::RootDir();
	FPaths::NormalizeDirectoryName(EngineDir);
	FPaths::CollapseRelativeDirectories(EngineDir);

	FString EngineId;
	const FName* Key = Section.FindKey(EngineDir);
	if (Key)
	{
		FGuid IdGuid;
		FGuid::Parse(Key->ToString(), IdGuid);
		EngineId = IdGuid.ToString(EGuidFormats::DigitsWithHyphensInBraces);;
	}
	else
	{
		if (!OutInstallations.FindKey(EngineDir))
		{
			EngineId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensInBraces);
			Section.AddUnique(*EngineId, EngineDir);
			ConfigFile.Dirty = true;
		}
	}
	if (!EngineId.IsEmpty() && !OutInstallations.Find(EngineId))
	{
		OutInstallations.Add(EngineId, EngineDir);
	}

	ConfigFile.Write(ConfigPath);

	IFileManager::Get().Delete(*UProjectPath);
}

bool FDesktopPlatformLinux::IsSourceDistribution(const FString &RootDir)
{
	// Check for the existence of a GenerateProjectFiles.sh file. This allows compatibility with the GitHub 4.0 release.
	FString GenerateProjectFilesPath = RootDir / TEXT("GenerateProjectFiles.sh");
	if (IFileManager::Get().FileSize(*GenerateProjectFilesPath) >= 0)
	{
		return true;
	}

	// Otherwise use the default test
	return FDesktopPlatformBase::IsSourceDistribution(RootDir);
}

bool FDesktopPlatformLinux::VerifyFileAssociations()
{
	STUBBED("FDesktopPlatformLinux::VerifyFileAssociationsg");
	return true; // for now we are associated
}

bool FDesktopPlatformLinux::UpdateFileAssociations()
{
	//unimplemented();
	STUBBED("FDesktopPlatformLinux::UpdateFileAssociations");
	return false;
}

bool FDesktopPlatformLinux::OpenProject(const FString &ProjectFileName)
{
	// Get the project filename in a native format
	FString PlatformProjectFileName = ProjectFileName;
	FPaths::MakePlatformFilename(PlatformProjectFileName);

	STUBBED("FDesktopPlatformLinux::OpenProject");
	return false;
}

bool FDesktopPlatformLinux::RunUnrealBuildTool(const FText& Description, const FString& RootDir, const FString& Arguments, FFeedbackContext* Warn)
{
	// Get the path to UBT
	FString UnrealBuildToolPath = RootDir / TEXT("Engine/Binaries/DotNET/UnrealBuildTool.exe");
	if(IFileManager::Get().FileSize(*UnrealBuildToolPath) < 0)
	{
		Warn->Logf(ELogVerbosity::Error, TEXT("Couldn't find UnrealBuildTool at '%s'"), *UnrealBuildToolPath);
		return false;
	}

	// Write the output
	Warn->Logf(TEXT("Running %s %s"), *UnrealBuildToolPath, *Arguments);

	// launch UBT with Mono
	FString ScriptPath = FPaths::ConvertRelativePathToFull(RootDir / TEXT("Engine/Build/BatchFiles/Linux/RunMono.sh"));
	FString CmdLineParams = FString::Printf(TEXT("\"%s\" \"%s\" %s"), *ScriptPath, *UnrealBuildToolPath, *Arguments);

	// Spawn it with bash (and not sh) because of pushd
	int32 ExitCode = 0;
	return FFeedbackContextMarkup::PipeProcessOutput(Description, TEXT("/bin/bash"), CmdLineParams, Warn, &ExitCode) && ExitCode == 0;
}

bool FDesktopPlatformLinux::IsUnrealBuildToolRunning()
{
	// For now assume that if mono application is running, we're running UBT
	// @todo: we need to get the commandline for the mono process and check if UBT.exe is in there.
	return FPlatformProcess::IsApplicationRunning(TEXT("mono"));
}

FFeedbackContext* FDesktopPlatformLinux::GetNativeFeedbackContext()
{
	//unimplemented();
	STUBBED("FDesktopPlatformLinux::GetNativeFeedbackContext");
	return GWarn;
}

FString FDesktopPlatformLinux::GetUserTempPath()
{
	return FString(FPlatformProcess::UserTempDir());
}

#undef LOCTEXT_NAMESPACE
