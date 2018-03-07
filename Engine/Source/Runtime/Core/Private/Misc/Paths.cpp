// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

// Core includes.
#include "Misc/Paths.h"
#include "UObject/NameTypes.h"
#include "Logging/LogMacros.h"
#include "HAL/FileManager.h"
#include "Misc/Parse.h"
#include "Misc/ScopeLock.h"
#include "Misc/CommandLine.h"
#include "Internationalization/Text.h"
#include "Internationalization/Internationalization.h"
#include "Misc/Guid.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/App.h"
#include "Misc/EngineVersion.h"

DEFINE_LOG_CATEGORY_STATIC(LogPaths, Log, All);

FString FPaths::GameProjectFilePath;

/*-----------------------------------------------------------------------------
	Path helpers for retrieving game dir, engine dir, etc.
-----------------------------------------------------------------------------*/

namespace UE4Paths_Private
{
	auto IsSlashOrBackslash    = [](TCHAR C) { return C == TEXT('/') || C == TEXT('\\'); };
	auto IsNotSlashOrBackslash = [](TCHAR C) { return C != TEXT('/') && C != TEXT('\\'); };

	FString GameSavedDir()
	{
		FString Result = FPaths::ProjectUserDir();

		FString NonDefaultSavedDirSuffix;
		if (FParse::Value(FCommandLine::Get(), TEXT("-saveddirsuffix="), NonDefaultSavedDirSuffix))
		{
			for (int32 CharIdx = 0; CharIdx < NonDefaultSavedDirSuffix.Len(); ++CharIdx)
			{
				if (!FCString::Strchr(VALID_SAVEDDIRSUFFIX_CHARACTERS, NonDefaultSavedDirSuffix[CharIdx]))
				{
					NonDefaultSavedDirSuffix.RemoveAt(CharIdx, 1, false);
					--CharIdx;
				}
			}

			if (!NonDefaultSavedDirSuffix.IsEmpty())
			{
				Result += TEXT("Saved_") + NonDefaultSavedDirSuffix + TEXT("/");
			}
		}
		else
		{
			Result += TEXT("Saved/");
		}

		return Result;
	}

	FString ConvertRelativePathToFullInternal(FString&& BasePath, FString&& InPath)
	{
		FString FullyPathed;
		if ( FPaths::IsRelative(InPath) )
		{
			FullyPathed  = MoveTemp(BasePath);
			FullyPathed /= MoveTemp(InPath);
		}
		else
		{
			FullyPathed = MoveTemp(InPath);
		}

		FPaths::NormalizeFilename(FullyPathed);
		FPaths::CollapseRelativeDirectories(FullyPathed);

		if (FullyPathed.Len() == 0)
		{
			// Empty path is not absolute, and '/' is the best guess across all the platforms.
			// This substituion is not valid for Windows of course; however CollapseRelativeDirectories() will not produce an empty
			// absolute path on Windows as it takes care not to remove the drive letter.
			FullyPathed = TEXT("/");
		}

		return FullyPathed;
	}
}

bool FPaths::ShouldSaveToUserDir()
{
	static bool bShouldSaveToUserDir = FApp::IsInstalled() || FParse::Param(FCommandLine::Get(), TEXT("SaveToUserDir")) || FPlatformProcess::ShouldSaveToUserDir();
	return bShouldSaveToUserDir;
}

FString FPaths::LaunchDir()
{
	return FString(FPlatformMisc::LaunchDir());
}

FString FPaths::EngineDir()
{
	return FString(FPlatformMisc::EngineDir());
}

FString FPaths::EngineUserDir()
{
	if (ShouldSaveToUserDir() || FApp::IsEngineInstalled())
	{
		return FPaths::Combine(FPlatformProcess::UserSettingsDir(), *FApp::GetEpicProductIdentifier(), *FEngineVersion::Current().ToString(EVersionComponent::Minor)) + TEXT("/");
	}
	else
	{
		return FPaths::EngineDir();
	}
}

FString FPaths::EngineVersionAgnosticUserDir()
{
	if (ShouldSaveToUserDir() || FApp::IsEngineInstalled())
	{
		return FPaths::Combine(FPlatformProcess::UserSettingsDir(), *FApp::GetEpicProductIdentifier(), TEXT("Common")) + TEXT("/");
	}
	else
	{
		return FPaths::EngineDir();
	}
}

FString FPaths::EngineContentDir()
{
	return FPaths::EngineDir() + TEXT("Content/");
}

FString FPaths::EngineConfigDir()
{
	return FPaths::EngineDir() + TEXT("Config/");
}

FString FPaths::EngineIntermediateDir()
{
	return FPaths::EngineDir() + TEXT("Intermediate/");
}

FString FPaths::EngineSavedDir()
{
	return EngineUserDir() + TEXT("Saved/");
}

FString FPaths::EnginePluginsDir()
{
	return FPaths::EngineDir() + TEXT("Plugins/");
}

FString FPaths::EnterpriseDir()
{
	return FPaths::RootDir() + TEXT("Enterprise/");
}

FString FPaths::EnterprisePluginsDir()
{
	return EnterpriseDir() + TEXT("Plugins/");
}

FString FPaths::RootDir()
{
	return FString(FPlatformMisc::RootDir());
}

FString FPaths::ProjectDir()
{
	return FString(FPlatformMisc::ProjectDir());
}

FString FPaths::ProjectUserDir()
{
	if (ShouldSaveToUserDir())
	{
		return FPaths::Combine(FPlatformProcess::UserSettingsDir(), FApp::GetProjectName()) + TEXT("/");
	}
	else
	{
		FString UserDir;
		if (FParse::Value(FCommandLine::Get(), TEXT("UserDir="), UserDir))
		{
			if (FPaths::IsRelative(UserDir))
			{
				return FPaths::Combine(*FPaths::ProjectDir(), *UserDir) + TEXT("/");
			}
			FPaths::NormalizeDirectoryName(UserDir);
			return UserDir + TEXT("/");
		}

		return FPaths::ProjectDir();
	}
}

FString FPaths::ProjectContentDir()
{
	return FPaths::ProjectDir() + TEXT("Content/");
}

FString FPaths::ProjectConfigDir()
{
	return FPaths::ProjectDir() + TEXT("Config/");
}

FString FPaths::ProjectSavedDir()
{
	static FString Result = UE4Paths_Private::GameSavedDir();
	return Result;
}

FString FPaths::ProjectIntermediateDir()
{
	return ProjectUserDir() + TEXT("Intermediate/");
}

FString FPaths::ProjectPluginsDir()
{
	return FPaths::ProjectDir() + TEXT("Plugins/");
}

FString FPaths::ProjectModsDir()
{
	return FPaths::ProjectDir() + TEXT("Mods/");
}

FString FPaths::ProjectPersistentDownloadDir()
{
	return FPlatformMisc::GamePersistentDownloadDir();
}

FString FPaths::SourceConfigDir()
{
	return FPaths::ProjectDir() + TEXT("Config/");
}

FString FPaths::GeneratedConfigDir()
{
#if PLATFORM_MAC
	return FPlatformProcess::UserPreferencesDir();
#else
	return FPaths::ProjectSavedDir() + TEXT("Config/");
#endif
}

FString FPaths::SandboxesDir()
{
	return FPaths::ProjectDir() + TEXT("Saved/Sandboxes");
}

FString FPaths::ProfilingDir()
{
	return FPaths::ProjectSavedDir() + TEXT("Profiling/");
}

FString FPaths::ScreenShotDir()
{
	return FPaths::ProjectSavedDir() + TEXT("Screenshots/") + FPlatformProperties::PlatformName() + TEXT("/");
}

FString FPaths::BugItDir()
{
	return FPaths::ProjectSavedDir() + TEXT("BugIt/") + FPlatformProperties::PlatformName() + TEXT("/");
}

FString FPaths::VideoCaptureDir()
{
	return FPaths::ProjectSavedDir() + TEXT("VideoCaptures/");
}

FString FPaths::ProjectLogDir()
{
#if PLATFORM_MAC || PLATFORM_XBOXONE
	return FPlatformProcess::UserLogsDir();
#else
	return FPaths::ProjectSavedDir() + TEXT("Logs/");
#endif
}

FString FPaths::AutomationDir()
{
	return FPaths::ProjectSavedDir() + TEXT("Automation/");
}

FString FPaths::AutomationTransientDir()
{
	return FPaths::AutomationDir() + TEXT("Transient/");
}

FString FPaths::AutomationLogDir()
{
	return FPaths::AutomationDir() + TEXT("Logs/");
}

FString FPaths::CloudDir()
{
	return FPlatformMisc::CloudDir();
}

FString FPaths::GameDevelopersDir()
{
	return FPaths::ProjectContentDir() + TEXT("Developers/");
}

FString FPaths::GameUserDeveloperDir()
{
	static FString UserFolder;

	if ( UserFolder.Len() == 0 )
	{
		// The user folder is the user name without any invalid characters
		const FString InvalidChars = INVALID_LONGPACKAGE_CHARACTERS;
		const FString& UserName = FPlatformProcess::UserName();
		
		UserFolder = UserName;
		
		for (int32 CharIdx = 0; CharIdx < InvalidChars.Len(); ++CharIdx)
		{
			const FString Char = InvalidChars.Mid(CharIdx, 1);
			UserFolder = UserFolder.Replace(*Char, TEXT("_"), ESearchCase::CaseSensitive);
		}
	}

	return FPaths::GameDevelopersDir() + UserFolder + TEXT("/");
}

FString FPaths::DiffDir()
{
	return FPaths::ProjectSavedDir() + TEXT("Diff/");
}

const TArray<FString>& FPaths::GetEngineLocalizationPaths()
{
	static TArray<FString> Results;
	static bool HasInitialized = false;

	if(!HasInitialized)
	{
		if(GConfig && GConfig->IsReadyForUse())
		{
			GConfig->GetArray( TEXT("Internationalization"), TEXT("LocalizationPaths"), Results, GEngineIni );
			if(!Results.Num())
			{
				UE_LOG(LogInit, Warning, TEXT("No paths for engine localization data were specifed in the engine configuration."));
			}
			HasInitialized = true;
		}
		else
		{
			Results.AddUnique(TEXT("../../../Engine/Content/Localization/Engine")); // Hardcoded convention.
		}
	}

	return Results;
}

const TArray<FString>& FPaths::GetEditorLocalizationPaths()
{
	static TArray<FString> Results;
	static bool HasInitialized = false;

	if(!HasInitialized)
	{
		if(GConfig && GConfig->IsReadyForUse())
		{
			GConfig->GetArray( TEXT("Internationalization"), TEXT("LocalizationPaths"), Results, GEditorIni );
			if(!Results.Num())
			{
				UE_LOG(LogInit, Warning, TEXT("No paths for editor localization data were specifed in the editor configuration."));
			}
			HasInitialized = true;
		}
		else
		{
			Results.AddUnique(TEXT("../../../Engine/Content/Localization/Editor")); // Hardcoded convention.
		}
	}

	return Results;
}

const TArray<FString>& FPaths::GetPropertyNameLocalizationPaths()
{
	static TArray<FString> Results;
	static bool HasInitialized = false;

	if(!HasInitialized)
	{
		if(GConfig && GConfig->IsReadyForUse())
		{
			GConfig->GetArray( TEXT("Internationalization"), TEXT("PropertyNameLocalizationPaths"), Results, GEditorIni );
			if(!Results.Num())
			{
				UE_LOG(LogInit, Warning, TEXT("No paths for property name localization data were specifed in the editor configuration."));
			}
			HasInitialized = true;
		}
		else
		{
			Results.AddUnique(TEXT("../../../Engine/Content/Localization/PropertyNames")); // Hardcoded convention.
		}
	}

	return Results;
}

const TArray<FString>& FPaths::GetToolTipLocalizationPaths() 
{
	static TArray<FString> Results;
	static bool HasInitialized = false;

	if(!HasInitialized)
	{
		if(GConfig && GConfig->IsReadyForUse())
		{
			GConfig->GetArray( TEXT("Internationalization"), TEXT("ToolTipLocalizationPaths"), Results, GEditorIni );
			if(!Results.Num())
			{
				UE_LOG(LogInit, Warning, TEXT("No paths for tooltips localization data were specifed in the editor configuration."));
			}
			HasInitialized = true;
		}
		else
		{
			Results.AddUnique(TEXT("../../../Engine/Content/Localization/ToolTips")); // Hardcoded convention.
		}
	}

	return Results;
}

const TArray<FString>& FPaths::GetGameLocalizationPaths()
{
	static TArray<FString> Results;
	static bool HasInitialized = false;

	if(!HasInitialized)
	{
		if(GConfig && GConfig->IsReadyForUse())
		{
			GConfig->GetArray( TEXT("Internationalization"), TEXT("LocalizationPaths"), Results, GGameIni );
			if(!Results.Num()) // Failed to find localization path.
			{
				UE_LOG(LogPaths, Warning, TEXT("No paths for game localization data were specifed in the game configuration."));
			}
			HasInitialized = true;
		}
	}


	return Results;
}

FString FPaths::GameAgnosticSavedDir()
{
	return EngineSavedDir();
}

FString FPaths::EngineSourceDir()
{
	return FPaths::EngineDir() + TEXT("Source/");
}

FString FPaths::GameSourceDir()
{
	return FPaths::ProjectDir() + TEXT("Source/");
}

FString FPaths::FeaturePackDir()
{
	return FPaths::RootDir() + TEXT("FeaturePacks/");
}

bool FPaths::IsProjectFilePathSet()
{
	FScopeLock Lock(GameProjectFilePathLock());
	return !GameProjectFilePath.IsEmpty();
}

FString FPaths::GetProjectFilePath()
{
	FScopeLock Lock(GameProjectFilePathLock());
	return GameProjectFilePath;
}

void FPaths::SetProjectFilePath( const FString& NewGameProjectFilePath )
{
	FScopeLock Lock(GameProjectFilePathLock());
	GameProjectFilePath = NewGameProjectFilePath;
	FPaths::NormalizeFilename(GameProjectFilePath);
}

FString FPaths::GetExtension( const FString& InPath, bool bIncludeDot )
{
	const FString Filename = GetCleanFilename(InPath);
	int32 DotPos = Filename.Find(TEXT("."), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
	if (DotPos != INDEX_NONE)
	{
		return Filename.Mid(DotPos + (bIncludeDot ? 0 : 1));
	}

	return TEXT("");
}

FString FPaths::GetCleanFilename(const FString& InPath)
{
	static_assert(INDEX_NONE == -1, "INDEX_NONE assumed to be -1");

	int32 EndPos   = InPath.FindLastCharByPredicate(UE4Paths_Private::IsNotSlashOrBackslash) + 1;
	int32 StartPos = InPath.FindLastCharByPredicate(UE4Paths_Private::IsSlashOrBackslash, EndPos) + 1;

	FString Result = InPath.Mid(StartPos, EndPos - StartPos);
	return Result;
}

FString FPaths::GetCleanFilename(FString&& InPath)
{
	static_assert(INDEX_NONE == -1, "INDEX_NONE assumed to be -1");

	int32 EndPos   = InPath.FindLastCharByPredicate(UE4Paths_Private::IsNotSlashOrBackslash) + 1;
	int32 StartPos = InPath.FindLastCharByPredicate(UE4Paths_Private::IsSlashOrBackslash, EndPos) + 1;

	InPath.RemoveAt(EndPos, InPath.Len() - EndPos, false);
	InPath.RemoveAt(0, StartPos, false);

	return MoveTemp(InPath);
}

FString FPaths::GetBaseFilename( const FString& InPath, bool bRemovePath )
{
	FString Wk = bRemovePath ? GetCleanFilename(InPath) : InPath;

	// remove the extension
	int32 ExtPos = Wk.Find(TEXT("."), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
	
	// determine the position of the path/leaf separator
	int32 LeafPos = INDEX_NONE;
	if (!bRemovePath)
	{
		LeafPos = Wk.FindLastCharByPredicate(UE4Paths_Private::IsSlashOrBackslash);
	}

	if (ExtPos != INDEX_NONE && (LeafPos == INDEX_NONE || ExtPos > LeafPos))
	{
		Wk = Wk.Left(ExtPos);
	}

	return Wk;
}

FString FPaths::GetPath(const FString& InPath)
{
	int32 Pos = InPath.FindLastCharByPredicate(UE4Paths_Private::IsSlashOrBackslash);

	FString Result;
	if (Pos != INDEX_NONE)
	{
		Result = InPath.Left(Pos);
	}

	return Result;
}

FString FPaths::GetPath(FString&& InPath)
{
	int32 Pos = InPath.FindLastCharByPredicate(UE4Paths_Private::IsSlashOrBackslash);

	FString Result;
	if (Pos != INDEX_NONE)
	{
		InPath.RemoveAt(Pos, InPath.Len() - Pos, false);
		Result = MoveTemp(InPath);
	}

	return Result;
}

FString FPaths::ChangeExtension(const FString& InPath, const FString& InNewExtension)
{
	int32 Pos = INDEX_NONE;
	if (InPath.FindLastChar('.', Pos))
	{
		const int32 PathEndPos = InPath.FindLastCharByPredicate(UE4Paths_Private::IsSlashOrBackslash);
		if (PathEndPos != INDEX_NONE && PathEndPos > Pos)
		{
			// The dot found was part of the path rather than the name
			Pos = INDEX_NONE;
		}
	}

	if (Pos != INDEX_NONE)
	{
		FString Result = InPath.Left(Pos);

		if (InNewExtension.Len() && InNewExtension[0] != '.')
		{
			Result += '.';
		}

		Result += InNewExtension;

		return Result;
	}

	return InPath;
}

FString FPaths::SetExtension(const FString& InPath, const FString& InNewExtension)
{
	int32 Pos = INDEX_NONE;
	if (InPath.FindLastChar('.', Pos))
	{
		const int32 PathEndPos = InPath.FindLastCharByPredicate(UE4Paths_Private::IsSlashOrBackslash);
		if (PathEndPos != INDEX_NONE && PathEndPos > Pos)
		{
			// The dot found was part of the path rather than the name
			Pos = INDEX_NONE;
		}
	}

	FString Result = Pos == INDEX_NONE ? InPath : InPath.Left(Pos);

	if (InNewExtension.Len() && InNewExtension[0] != '.')
	{
		Result += '.';
	}

	Result += InNewExtension;

	return Result;
}

bool FPaths::FileExists(const FString& InPath)
{
	return IFileManager::Get().FileExists(*InPath);
}

bool FPaths::DirectoryExists(const FString& InPath)
{
	return IFileManager::Get().DirectoryExists(*InPath);
}

bool FPaths::IsDrive(const FString& InPath)
{
	FString ConvertedPathString = InPath;

	ConvertedPathString = ConvertedPathString.Replace(TEXT("/"), TEXT("\\"), ESearchCase::CaseSensitive);
	const TCHAR* ConvertedPath= *ConvertedPathString;

	// Does Path refer to a drive letter or BNC path?
	if (ConvertedPath[0] == TCHAR(0))
	{
		return true;
	}
	else if (FChar::ToUpper(ConvertedPath[0])!=FChar::ToLower(ConvertedPath[0]) && ConvertedPath[1]==TEXT(':') && ConvertedPath[2]==0)
	{
		return true;
	}
	else if (FCString::Strcmp(ConvertedPath,TEXT("\\"))==0)
	{
		return true;
	}
	else if (FCString::Strcmp(ConvertedPath,TEXT("\\\\"))==0)
	{
		return true;
	}
	else if (ConvertedPath[0]==TEXT('\\') && ConvertedPath[1]==TEXT('\\') && !FCString::Strchr(ConvertedPath+2,TEXT('\\')))
	{
		return true;
	}
	else
	{
		// Need to handle cases such as X:\A\B\..\..\C\..
		// This assumes there is no actual filename in the path (ie, not c:\DIR\File.ext)!
		FString TempPath(ConvertedPath);
		// Make sure there is a '\' at the end of the path
		if (TempPath.Find(TEXT("\\"), ESearchCase::CaseSensitive, ESearchDir::FromEnd) != (TempPath.Len() - 1))
		{
			TempPath += TEXT("\\");
		}

		FString CheckPath = TEXT("");
		int32 ColonSlashIndex = TempPath.Find(TEXT(":\\"), ESearchCase::CaseSensitive);
		if (ColonSlashIndex != INDEX_NONE)
		{
			// Remove the 'X:\' from the start
			CheckPath = TempPath.Right(TempPath.Len() - ColonSlashIndex - 2);
		}
		else
		{
			// See if the first two characters are '\\' to handle \\Server\Foo\Bar cases
			if (TempPath.StartsWith(TEXT("\\\\"), ESearchCase::CaseSensitive) == true)
			{
				CheckPath = TempPath.Right(TempPath.Len() - 2);
				// Find the next slash
				int32 SlashIndex = CheckPath.Find(TEXT("\\"), ESearchCase::CaseSensitive);
				if (SlashIndex != INDEX_NONE)
				{
					CheckPath = CheckPath.Right(CheckPath.Len() - SlashIndex  - 1);
				}
				else
				{
					CheckPath = TEXT("");
				}
			}
		}

		if (CheckPath.Len() > 0)
		{
			// Replace any remaining '\\' instances with '\'
			CheckPath.Replace(TEXT("\\\\"), TEXT("\\"), ESearchCase::CaseSensitive);

			int32 CheckCount = 0;
			int32 SlashIndex = CheckPath.Find(TEXT("\\"), ESearchCase::CaseSensitive);
			while (SlashIndex != INDEX_NONE)
			{
				FString FolderName = CheckPath.Left(SlashIndex);
				if (FolderName == TEXT(".."))
				{
					// It's a relative path, so subtract one from the count
					CheckCount--;
				}
				else
				{
					// It's a real folder, so add one to the count
					CheckCount++;
				}
				CheckPath = CheckPath.Right(CheckPath.Len() - SlashIndex  - 1);
				SlashIndex = CheckPath.Find(TEXT("\\"), ESearchCase::CaseSensitive);
			}

			if (CheckCount <= 0)
			{
				// If there were the same number or greater relative to real folders, it's the root dir
				return true;
			}
		}
	}

	// It's not a drive...
	return false;
}

bool FPaths::IsRelative(const FString& InPath)
{
	// The previous implementation of this function seemed to handle normalized and unnormalized paths, so this one does too for legacy reasons.

	const bool IsRooted = InPath.StartsWith(TEXT("\\"), ESearchCase::CaseSensitive)	||					// Root of the current directory on Windows. Also covers "\\" for UNC or "network" paths.
						  InPath.StartsWith(TEXT("/"), ESearchCase::CaseSensitive)	||					// Root of the current directory on Windows, root on UNIX-likes.  Also covers "\\", considering normalization replaces "\\" with "//".						
						  InPath.StartsWith(TEXT("root:/"), ESearchCase::IgnoreCase) ||					// Feature packs use this
						  (InPath.Len() >= 2 && FChar::IsAlpha(InPath[0]) && InPath[1] == TEXT(':'));	// Starts with "<DriveLetter>:"

	return !IsRooted;
}

void FPaths::NormalizeFilename(FString& InPath)
{
	InPath.ReplaceInline(TEXT("\\"), TEXT("/"), ESearchCase::CaseSensitive);

	FPlatformMisc::NormalizePath(InPath);
}

void FPaths::NormalizeDirectoryName(FString& InPath)
{
	InPath.ReplaceInline(TEXT("\\"), TEXT("/"), ESearchCase::CaseSensitive);
	if (InPath.EndsWith(TEXT("/"), ESearchCase::CaseSensitive) && !InPath.EndsWith(TEXT("//"), ESearchCase::CaseSensitive) && !InPath.EndsWith(TEXT(":/"), ESearchCase::CaseSensitive))
	{
		// overwrite trailing slash with terminator
		InPath.GetCharArray()[InPath.Len() - 1] = 0;
		// shrink down
		InPath.TrimToNullTerminator();
	}

	FPlatformMisc::NormalizePath(InPath);
}

bool FPaths::CollapseRelativeDirectories(FString& InPath)
{
	const TCHAR ParentDir[] = TEXT("/..");
	const int32 ParentDirLength = ARRAY_COUNT( ParentDir ) - 1; // To avoid hardcoded values

	for (;;)
	{
		// An empty path is finished
		if (InPath.IsEmpty())
			break;

		// Consider empty paths or paths which start with .. or /.. as invalid
		if (InPath.StartsWith(TEXT(".."), ESearchCase::CaseSensitive) || InPath.StartsWith(ParentDir))
			return false;

		// If there are no "/.."s left then we're done
		const int32 Index = InPath.Find(ParentDir, ESearchCase::CaseSensitive);
		if (Index == -1)
			break;

		int32 PreviousSeparatorIndex = Index;
		for (;;)
		{
			// Find the previous slash
			PreviousSeparatorIndex = FMath::Max(0, InPath.Find( TEXT("/"), ESearchCase::CaseSensitive, ESearchDir::FromEnd, PreviousSeparatorIndex - 1));

			// Stop if we've hit the start of the string
			if (PreviousSeparatorIndex == 0)
				break;

			// Stop if we've found a directory that isn't "/./"
			if ((Index - PreviousSeparatorIndex) > 1 && (InPath[PreviousSeparatorIndex + 1] != TEXT('.') || InPath[PreviousSeparatorIndex + 2] != TEXT('/')))
				break;
		}

		// If we're attempting to remove the drive letter, that's illegal
		int32 Colon = InPath.Find(TEXT(":"), ESearchCase::CaseSensitive, ESearchDir::FromStart, PreviousSeparatorIndex);
		if (Colon >= 0 && Colon < Index)
			return false;

		InPath.RemoveAt(PreviousSeparatorIndex, Index - PreviousSeparatorIndex + ParentDirLength, false);
	}

	InPath.ReplaceInline(TEXT("./"), TEXT(""), ESearchCase::CaseSensitive);

	InPath.TrimToNullTerminator();

	return true;
}

void FPaths::RemoveDuplicateSlashes(FString& InPath)
{
	while (InPath.Contains(TEXT("//"), ESearchCase::CaseSensitive))
	{
		InPath = InPath.Replace(TEXT("//"), TEXT("/"), ESearchCase::CaseSensitive);
	}
}

void FPaths::MakeStandardFilename(FString& InPath)
{
	// if this is an empty path, use the relative base dir
	if (InPath.Len() == 0)
	{
#if !PLATFORM_HTML5
		InPath = FPlatformProcess::BaseDir();
		// if the base directory is nothing then this function will recurse infinitely instead of returning nothing. 
		if (InPath.Len() == 0)
			return;
		FPaths::MakeStandardFilename(InPath);
#else
		// @todo: revisit this as needed
//		InPath = TEXT("/");
#endif
		return;
	}

	FString WithSlashes = InPath.Replace(TEXT("\\"), TEXT("/"), ESearchCase::CaseSensitive);

	FString RootDirectory = FPaths::ConvertRelativePathToFull(FPaths::RootDir());

	// look for paths that cannot be made relative, and are therefore left alone
	// UNC (windows) network path
	bool bCannotBeStandardized = InPath.StartsWith(TEXT("\\\\"), ESearchCase::CaseSensitive);
	// windows drive letter path that doesn't start with base dir
	bCannotBeStandardized |= ((InPath.Len() > 1) && (InPath[1] == ':') && !WithSlashes.StartsWith(RootDirectory));
	// Unix style absolute path that doesn't start with base dir
	bCannotBeStandardized |= (WithSlashes.GetCharArray()[0] == '/' && !WithSlashes.StartsWith(RootDirectory));

	// if it can't be standardized, just return itself
	if (bCannotBeStandardized)
	{
		return;
	}

	// make an absolute path
	
	FString Standardized = FPaths::ConvertRelativePathToFull(InPath);

	// remove duplicate slashes
	FPaths::RemoveDuplicateSlashes(Standardized);

	// make it relative to Engine\Binaries\Platform
	InPath = Standardized.Replace(*RootDirectory, *FPaths::GetRelativePathToRoot());
}

void FPaths::MakePlatformFilename( FString& InPath )
{
	InPath.ReplaceInline(TEXT( "\\" ), FPlatformMisc::GetDefaultPathSeparator(), ESearchCase::CaseSensitive);
	InPath.ReplaceInline(TEXT( "/" ), FPlatformMisc::GetDefaultPathSeparator(), ESearchCase::CaseSensitive);
}

bool FPaths::MakePathRelativeTo( FString& InPath, const TCHAR* InRelativeTo )
{
	FString Target = FPaths::ConvertRelativePathToFull(InPath);
	FString Source = FPaths::ConvertRelativePathToFull(InRelativeTo);
	
	Source = FPaths::GetPath(Source);
	Source.ReplaceInline(TEXT("\\"), TEXT("/"), ESearchCase::CaseSensitive);
	Target.ReplaceInline(TEXT("\\"), TEXT("/"), ESearchCase::CaseSensitive);

	TArray<FString> TargetArray;
	Target.ParseIntoArray(TargetArray, TEXT("/"), true);
	TArray<FString> SourceArray;
	Source.ParseIntoArray(SourceArray, TEXT("/"), true);

	if (TargetArray.Num() && SourceArray.Num())
	{
		// Check for being on different drives
		if ((TargetArray[0][1] == TEXT(':')) && (SourceArray[0][1] == TEXT(':')))
		{
			if (FChar::ToUpper(TargetArray[0][0]) != FChar::ToUpper(SourceArray[0][0]))
			{
				// The Target and Source are on different drives... No relative path available.
				return false;
			}
		}
	}

	while (TargetArray.Num() && SourceArray.Num() && TargetArray[0] == SourceArray[0])
	{
		TargetArray.RemoveAt(0);
		SourceArray.RemoveAt(0);
	}
	FString Result;
	for (int32 Index = 0; Index < SourceArray.Num(); Index++)
	{
		Result += TEXT("../");
	}
	for (int32 Index = 0; Index < TargetArray.Num(); Index++)
	{
		Result += TargetArray[Index];
		if (Index + 1 < TargetArray.Num())
		{
			Result += TEXT("/");
		}
	}
	
	InPath = Result;
	return true;
}

FString FPaths::ConvertRelativePathToFull(const FString& InPath)
{
	return UE4Paths_Private::ConvertRelativePathToFullInternal(FString(FPlatformProcess::BaseDir()), FString(InPath));
}

FString FPaths::ConvertRelativePathToFull(FString&& InPath)
{
	return UE4Paths_Private::ConvertRelativePathToFullInternal(FString(FPlatformProcess::BaseDir()), MoveTemp(InPath));
}

FString FPaths::ConvertRelativePathToFull(const FString& BasePath, const FString& InPath)
{
	return UE4Paths_Private::ConvertRelativePathToFullInternal(CopyTemp(BasePath), CopyTemp(InPath));
}

FString FPaths::ConvertRelativePathToFull(const FString& BasePath, FString&& InPath)
{
	return UE4Paths_Private::ConvertRelativePathToFullInternal(CopyTemp(BasePath), MoveTemp(InPath));
}

FString FPaths::ConvertRelativePathToFull(FString&& BasePath, const FString& InPath)
{
	return UE4Paths_Private::ConvertRelativePathToFullInternal(MoveTemp(BasePath), CopyTemp(InPath));
}

FString FPaths::ConvertRelativePathToFull(FString&& BasePath, FString&& InPath)
{
	return UE4Paths_Private::ConvertRelativePathToFullInternal(MoveTemp(BasePath), MoveTemp(InPath));
}

FString FPaths::ConvertToSandboxPath( const FString& InPath, const TCHAR* InSandboxName )
{
	FString SandboxDirectory = FPaths::SandboxesDir() / InSandboxName;
	FPaths::NormalizeFilename(SandboxDirectory);
	
	FString RootDirectory = FPaths::RootDir();
	FPaths::CollapseRelativeDirectories(RootDirectory);
	FPaths::NormalizeFilename(RootDirectory);

	FString SandboxPath = FPaths::ConvertRelativePathToFull(InPath);
	if (!SandboxPath.StartsWith(RootDirectory))
	{
		UE_LOG(LogInit, Fatal, TEXT("%s does not start with %s so this is not a valid sandbox path."), *SandboxPath, *RootDirectory);
	}
	check(SandboxPath.StartsWith(RootDirectory));
	SandboxPath.ReplaceInline(*RootDirectory, *SandboxDirectory);

	return SandboxPath;
}

FString FPaths::ConvertFromSandboxPath( const FString& InPath, const TCHAR* InSandboxName )
{
	FString SandboxDirectory =  FPaths::SandboxesDir() / InSandboxName;
	FPaths::NormalizeFilename(SandboxDirectory);
	FString RootDirectory = FPaths::RootDir();
	
	FString SandboxPath(InPath);
	check(SandboxPath.StartsWith(SandboxDirectory));
	SandboxPath.ReplaceInline(*SandboxDirectory, *RootDirectory);
	return SandboxPath;
}

FString FPaths::CreateTempFilename( const TCHAR* Path, const TCHAR* Prefix, const TCHAR* Extension )
{
	FString UniqueFilename;
	do
	{
		UniqueFilename = FPaths::Combine(Path, *FString::Printf(TEXT("%s%s%s"), Prefix, *FGuid::NewGuid().ToString(), Extension));
	}
	while (IFileManager::Get().FileSize(*UniqueFilename) >= 0);
	
	return UniqueFilename;
}

bool FPaths::ValidatePath( const FString& InPath, FText* OutReason )
{
	// Windows has the most restricted file system, and since we're cross platform, we have to respect the limitations of the lowest common denominator
	// # isn't legal. Used for revision specifiers in P4/SVN, and also not allowed on Windows anyway
	// @ isn't legal. Used for revision/label specifiers in P4/SVN
	// ^ isn't legal. While the file-system won't complain about this character, Visual Studio will				
	static const FString RestrictedChars = "/?:&\\*\"<>|%#@^";
	static const FString RestrictedNames[] = {	"CON", "PRN", "AUX", "CLOCK$", "NUL", 
												"COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8", "COM9", 
												"LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9" };

	FString Standardized = InPath;
	NormalizeFilename(Standardized);
	CollapseRelativeDirectories(Standardized);
	RemoveDuplicateSlashes(Standardized);

	// The loop below requires that the path not end with a /
	if(Standardized.EndsWith(TEXT("/"), ESearchCase::CaseSensitive))
	{
		Standardized = Standardized.LeftChop(1);
	}

	// Walk each part of the path looking for name errors
	for(int32 StartPos = 0, EndPos = Standardized.Find(TEXT("/"), ESearchCase::CaseSensitive); ; 
		StartPos = EndPos + 1, EndPos = Standardized.Find(TEXT("/"), ESearchCase::CaseSensitive, ESearchDir::FromStart, StartPos)
		)
	{
		const bool bIsLastPart = EndPos == INDEX_NONE;
		const FString PathPart = Standardized.Mid(StartPos, (bIsLastPart) ? MAX_int32 : EndPos - StartPos);

		// If this is the first part of the path, it's possible for it to be a drive name and is allowed to contain a colon
		if(StartPos == 0 && IsDrive(PathPart))
		{
			if(bIsLastPart)
			{
				break;
			}
			continue;
		}

		// Check for invalid characters
		TCHAR CharString[] = { '\0', '\0' };
		FString MatchedInvalidChars;
		for(const TCHAR* InvalidCharacters = *RestrictedChars; *InvalidCharacters; ++InvalidCharacters)
		{
			CharString[0] = *InvalidCharacters;
			if(PathPart.Contains(CharString))
			{
				MatchedInvalidChars += *InvalidCharacters;
			}
		}
		if(MatchedInvalidChars.Len())
		{
			if(OutReason)
			{
				FFormatNamedArguments Args;
				Args.Add(TEXT("IllegalPathCharacters"), FText::FromString(MatchedInvalidChars));
				*OutReason = FText::Format(NSLOCTEXT("Core", "PathContainsInvalidCharacters", "Path may not contain the following characters: {IllegalPathCharacters}"), Args);
			}
			return false;
		}

		// Check for reserved names
		for(const FString& RestrictedName : RestrictedNames)
		{
			if(PathPart.Equals(RestrictedName, ESearchCase::IgnoreCase))
			{
				if(OutReason)
				{
					FFormatNamedArguments Args;
					Args.Add(TEXT("RestrictedName"), FText::FromString(RestrictedName));
					*OutReason = FText::Format(NSLOCTEXT("Core", "PathContainsRestrictedName", "Path may not contain a restricted name: {RestrictedName}"), Args);
				}
				return false;
			}
		}

		if(bIsLastPart)
		{
			break;
		}
	}

	return true;
}

void FPaths::Split( const FString& InPath, FString& PathPart, FString& FilenamePart, FString& ExtensionPart )
{
	PathPart = GetPath(InPath);
	FilenamePart = GetBaseFilename(InPath);
	ExtensionPart = GetExtension(InPath);
}

const FString& FPaths::GetRelativePathToRoot()
{
	struct FRelativePathInitializer
	{
		FString RelativePathToRoot;

		FRelativePathInitializer()
		{
			FString RootDirectory = FPaths::RootDir();
			FString BaseDirectory = FPlatformProcess::BaseDir();

			// this is how to go from the base dir back to the root
			RelativePathToRoot = RootDirectory;
			FPaths::MakePathRelativeTo(RelativePathToRoot, *BaseDirectory);

			// Ensure that the path ends w/ '/'
			if ((RelativePathToRoot.Len() > 0) && (RelativePathToRoot.EndsWith(TEXT("/"), ESearchCase::CaseSensitive) == false) && (RelativePathToRoot.EndsWith(TEXT("\\"), ESearchCase::CaseSensitive) == false))
			{
				RelativePathToRoot += TEXT("/");
			}
		}
	};

	static FRelativePathInitializer StaticInstance;
	return StaticInstance.RelativePathToRoot;
}

void FPaths::CombineInternal(FString& OutPath, const TCHAR** Pathes, int32 NumPathes)
{
	check(Pathes != NULL && NumPathes > 0);

	int32 OutStringSize = 0;

	for (int32 i=0; i < NumPathes; ++i)
	{
		OutStringSize += FCString::Strlen(Pathes[i]) + 1;
	}

	OutPath.Empty(OutStringSize);
	OutPath += Pathes[0];
	
	for (int32 i=1; i < NumPathes; ++i)
	{
		OutPath /= Pathes[i];
	}
}

bool FPaths::IsSamePath(const FString& PathA, const FString& PathB)
{
	FString TmpA = PathA;
	FString TmpB = PathB;

	MakeStandardFilename(TmpA);
	MakeStandardFilename(TmpB);

#if PLATFORM_WINDOWS || PLATFORM_XBOXONE
	return FCString::Stricmp(*TmpA, *TmpB) == 0;
#else
	return FCString::Strcmp(*TmpA, *TmpB) == 0;
#endif
}

