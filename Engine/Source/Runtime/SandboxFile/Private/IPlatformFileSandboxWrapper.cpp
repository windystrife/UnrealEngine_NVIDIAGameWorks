// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "IPlatformFileSandboxWrapper.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/CommandLine.h"
#include "Misc/Guid.h"
#include "Stats/Stats.h"
#include "Misc/App.h"
#include "Modules/ModuleManager.h"
#include "HAL/IPlatformFileModule.h"
#include "UniquePtr.h"

DEFINE_LOG_CATEGORY(SandboxFile);

FSandboxPlatformFile::FSandboxPlatformFile(bool bInEntireEngineWillUseThisSandbox)
	: LowerLevel(NULL)
	, bEntireEngineWillUseThisSandbox(bInEntireEngineWillUseThisSandbox)
	, bSandboxEnabled(true)
{
}

static FString GetCookedSandboxDir()
{
	return FPaths::Combine(*(FPaths::ProjectSavedDir()), TEXT("Cooked"), ANSI_TO_TCHAR(FPlatformProperties::PlatformName()));
}

bool FSandboxPlatformFile::ShouldBeUsed(IPlatformFile* Inner, const TCHAR* CmdLine) const
{
	FString SandboxDir;
	bool bResult = FParse::Value( CmdLine, TEXT("-Sandbox="), SandboxDir );
#if PLATFORM_DESKTOP && (UE_GAME || UE_SERVER)
	if (FPlatformProperties::RequiresCookedData() && SandboxDir.IsEmpty() && Inner == &FPlatformFileManager::Get().GetPlatformFile() && bEntireEngineWillUseThisSandbox)
	{
		SandboxDir = GetCookedSandboxDir();
		bResult = FPlatformFileManager::Get().GetPlatformFile().DirectoryExists(*SandboxDir);
	}
#endif
	return bResult;
}

bool FSandboxPlatformFile::Initialize(IPlatformFile* Inner, const TCHAR* CmdLine)
{
	FString CommandLineDirectory;
	FParse::Value( CmdLine, TEXT("-Sandbox="), CommandLineDirectory);
#if PLATFORM_DESKTOP && (UE_GAME || UE_SERVER)
	if (CommandLineDirectory.IsEmpty() && bEntireEngineWillUseThisSandbox)
	{
		CommandLineDirectory = GetCookedSandboxDir();
		UE_LOG(LogInit, Display, TEXT("No sandbox specified, assuming %s"), *CommandLineDirectory);

		// Don't allow the default cooked sandbox to fallback to non-cooked assets
		FileExclusionWildcards.AddUnique(TEXT("*.uasset"));
		FileExclusionWildcards.AddUnique(TEXT("*.umap"));
	}
#endif

	LowerLevel = Inner;
	if (LowerLevel != NULL && !CommandLineDirectory.IsEmpty())
	{
		// Cache root directory
		RelativeRootDirectory = FPaths::GetRelativePathToRoot();
		AbsoluteRootDirectory = FPaths::ConvertRelativePathToFull(RelativeRootDirectory);

		// Commandline syntax
		bool bWipeSandbox = false;
		FPaths::NormalizeFilename(CommandLineDirectory);
		int32 CommandIndex = CommandLineDirectory.Find(TEXT(":"), ESearchCase::CaseSensitive);
		if( CommandIndex != INDEX_NONE )
		{
			// Check if absolute path was specified and the ':' refers to drive name
			FString DriveCheck( CommandLineDirectory.Mid(0, CommandIndex + 1) );
			if( FPaths::IsDrive(DriveCheck) == false )
			{
				FString Command( CommandLineDirectory.Mid( CommandIndex + 1 ) );
				CommandLineDirectory = CommandLineDirectory.Left( CommandIndex );
		
				if( Command == TEXT("wipe") )
				{
					bWipeSandbox = true;
				}
				// Add new commands here
			}
		}

		bool bSandboxIsAbsolute = false;
		if( CommandLineDirectory == TEXT("User") )
		{
			// Special case - platform defined user directory will be used
			SandboxDirectory = FPlatformProcess::UserDir();
			SandboxDirectory += TEXT("My Games/");
			SandboxDirectory += TEXT( "UE4/" );
			bSandboxIsAbsolute = true;
		}
		else if( CommandLineDirectory == TEXT("Unique") )
		{
			const FString Path = FPaths::GetRelativePathToRoot() / TEXT("");
			SandboxDirectory = FPaths::ConvertToSandboxPath( Path, *FGuid::NewGuid().ToString() );
		}
		else if (CommandLineDirectory.StartsWith(TEXT("..")))
		{
			// for relative-specified directories, just use it directly, and don't put into FPaths::ProjectSavedDir()
			SandboxDirectory = CommandLineDirectory;
		}
		else if( FPaths::IsDrive( CommandLineDirectory.Mid( 0, CommandLineDirectory.Find(TEXT("/"), ESearchCase::CaseSensitive) ) ) == false ) 
		{
			const FString Path = FPaths::GetRelativePathToRoot() / TEXT("");
			SandboxDirectory = FPaths::ConvertToSandboxPath( Path, *CommandLineDirectory );
		}
		else
		{
			SandboxDirectory = CommandLineDirectory;
			bSandboxIsAbsolute = true;
		}
	
		if( !bSandboxIsAbsolute )
		{
			// Make sure all path separators are correct with TEXT("/")
			FPaths::MakeStandardFilename(SandboxDirectory);

			// SandboxDirectory should be absolute and have no relative paths in it
			FPaths::ConvertRelativePathToFull(SandboxDirectory);
		}

		if( bWipeSandbox )
		{
			WipeSandboxFolder( *SandboxDirectory );
		}

		if (SandboxDirectory.EndsWith(TEXT("/")) == false)
		{
			SandboxDirectory += TEXT("/");
		}

		if (bEntireEngineWillUseThisSandbox)
		{
			FCommandLine::AddToSubprocessCommandline( *FString::Printf( TEXT("-sandbox=%s"), *SandboxDirectory ) );
		}
	}
	return !!LowerLevel;
}

const FString& FSandboxPlatformFile::GetGameSandboxDirectoryName()
{
	if (GameSandboxDirectoryName.IsEmpty())
	{
		GameSandboxDirectoryName = FString::Printf(TEXT("%s/"), FApp::GetProjectName());
	}
	return GameSandboxDirectoryName;
}

FString FSandboxPlatformFile::ConvertToSandboxPath( const TCHAR* Filename ) const
{
	// Mostly for the malloc profiler to flush the data.
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FSandboxPlatformFile::ConvertToSandboxPath"), STAT_SandboxPlatformFile_ConvertToSandboxPath, STATGROUP_LoadTimeVerbose);

	// convert to a standardized path (relative)
	FString SandboxPath = Filename;
	FPaths::MakeStandardFilename(SandboxPath);

	if ((bSandboxEnabled == true) && (SandboxDirectory.Len() > 0))
	{
		// See whether Filename is relative to root directory.
		// if it's not inside the root, then just use it
		FString FullSandboxPath = FPaths::ConvertRelativePathToFull(SandboxPath);
		FString FullGameDir;
#if IS_PROGRAM
		if (FPaths::IsProjectFilePathSet())
		{
			FullGameDir = FPaths::ConvertRelativePathToFull(FPaths::GetPath(FPaths::GetProjectFilePath()) + TEXT("/"));
		}
		else
#endif
		{
			FullGameDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
		}
		if(FullSandboxPath.StartsWith(FullGameDir))
		{
#if IS_PROGRAM
			SandboxPath = FPaths::Combine(*SandboxDirectory, *FPaths::GetBaseFilename(FPaths::GetProjectFilePath()), *FullSandboxPath + FullGameDir.Len());
#else
			SandboxPath = FPaths::Combine(*SandboxDirectory, FApp::GetProjectName(), *FullSandboxPath + FullGameDir.Len());
#endif
		}
		else if (FullSandboxPath.StartsWith(*AbsoluteRootDirectory))
		{
			SandboxPath = FPaths::Combine(*SandboxDirectory, *FullSandboxPath + AbsoluteRootDirectory.Len());
		}
		else
		{
			int32 SeparatorIndex = SandboxPath.Find(TEXT("/"), ESearchCase::CaseSensitive);
			int32 SeparatorIndex2 = SandboxPath.Find(TEXT("\\"), ESearchCase::CaseSensitive);
			if (SeparatorIndex < 0 || (SeparatorIndex2 >= 0 && SeparatorIndex2 < SeparatorIndex))
			{
				SeparatorIndex = SeparatorIndex2;
			}
			FString DrivePath = SandboxPath;
			if (SeparatorIndex != INDEX_NONE)
			{
				DrivePath = SandboxPath.Mid( 0, SeparatorIndex );
			}
			if( FPaths::IsDrive( DrivePath ) == false )
			{
				FString Dir = FPlatformProcess::BaseDir();
				FPaths::MakeStandardFilename(Dir);
				SandboxPath = Dir / SandboxPath;
				SandboxPath = SandboxPath.Replace( *RelativeRootDirectory, *SandboxDirectory, ESearchCase::IgnoreCase );
			}
		}
	}

	return SandboxPath;
}

FString FSandboxPlatformFile::ConvertFromSandboxPath(const TCHAR* Filename) const
{
	// Mostly for the malloc profiler to flush the data.
	//DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FSandboxPlatformFile::ConvertFromSandboxPath"), STAT_SandboxPlatformFile_ConvertToSandboxPath, STATGROUP_LoadTimeVerbose);

	FString FullSandboxPath = FPaths::ConvertRelativePathToFull(Filename);

	FString SandboxGameDirectory = FPaths::Combine(*SandboxDirectory, FApp::GetProjectName());
	FString SandboxRootDirectory = SandboxDirectory;

	FString OriginalPath;

	if (FullSandboxPath.StartsWith(SandboxGameDirectory))
	{
		OriginalPath = FullSandboxPath.Replace(*SandboxGameDirectory, *FPaths::ProjectDir());
	}
	else if (FullSandboxPath.StartsWith(SandboxRootDirectory))
	{
		OriginalPath = FullSandboxPath.Replace(*SandboxRootDirectory, *FPaths::RootDir());
	}

	OriginalPath.ReplaceInline(TEXT("//"), TEXT("/"));

	FString FullOriginalPath = FPaths::ConvertRelativePathToFull(OriginalPath);

	return FullOriginalPath;
}

bool FSandboxPlatformFile::WipeSandboxFolder( const TCHAR* AbsolutePath )
{
	return DeleteDirectory( AbsolutePath, true );
}

bool FSandboxPlatformFile::DeleteDirectory( const TCHAR* Path, bool Tree )
{
	if( Tree )
	{
		// Support code for removing a directory tree.
		bool Result = true;
		// Delete all files in the directory first
		FString Spec = FString( Path ) / TEXT( "*" );
		TArray<FString> List;
		FindFiles( List, *Spec, true, false );
		for( int32 FileIndex = 0; FileIndex < List.Num(); FileIndex++ )
		{
			FString Filename( FString( Path ) / List[ FileIndex ] );
			// Delete the file even if it's read-only
			if( LowerLevel->FileExists( *Filename ) )
			{
				LowerLevel->SetReadOnly( *Filename, false );
				if( !LowerLevel->DeleteFile( *Filename ) )
				{
					Result = false;
				}				
			}
			else
			{
				Result = false;
			}
		}
		// Clear out the list of found files and look for directories this time
		List.Empty();
		FindFiles( List, *Spec, false, true );
		for( int32 DirectoryIndex = 0; DirectoryIndex < List.Num(); DirectoryIndex++ )
		{
			if( !DeleteDirectory( *( FString( Path ) / List[ DirectoryIndex ] ), true ) )
			{
				Result = false;
			}
		}
		// The directory is empty now so it can be deleted
		return DeleteDirectory( Path, false ) && Result;
	}
	else
	{
		return LowerLevel->DeleteDirectory( Path ) || ( !LowerLevel->DirectoryExists( Path ) );
	}
}

void FSandboxPlatformFile::FindFiles( TArray<FString>& Result, const TCHAR* InFilename, bool Files, bool Directories )
{
	class FFileMatch : public IPlatformFile::FDirectoryVisitor
	{
	public:
		TArray<FString>& Result;
		FString WildCard;
		bool bFiles;
		bool bDirectories;
		FFileMatch( TArray<FString>& InResult, const FString& InWildCard, bool bInFiles, bool bInDirectories )
			: Result( InResult )
			, WildCard( InWildCard )
			, bFiles( bInFiles )
			, bDirectories( bInDirectories )
		{
		}
		virtual bool Visit( const TCHAR* FilenameOrDirectory, bool bIsDirectory )
		{
			if( ( bIsDirectory && bDirectories ) ||
				( !bIsDirectory && bFiles && FString( FilenameOrDirectory ).MatchesWildcard( WildCard ) ) )
			{
				new( Result ) FString( FPaths::GetCleanFilename( FilenameOrDirectory ) );
			}
			return true;
		}
	};
	
	FFileMatch FileMatch( Result, FPaths::GetCleanFilename(InFilename), Files, Directories );
	LowerLevel->IterateDirectory( *FPaths::GetPath(InFilename), FileMatch );
}

FString FSandboxPlatformFile::ConvertToAbsolutePathForExternalAppForRead( const TCHAR* Filename )
{
	FString SandboxPath( *ConvertToSandboxPath( Filename ) );
	if ( LowerLevel->FileExists( *SandboxPath ) || !OkForInnerAccess(Filename))
	{
		return SandboxPath;
	}
	else
	{
		return FPaths::ConvertRelativePathToFull(Filename);
	}
}

FString FSandboxPlatformFile::ConvertToAbsolutePathForExternalAppForWrite( const TCHAR* Filename )
{
	return ConvertToSandboxPath( Filename );
}

const FString& FSandboxPlatformFile::GetAbsolutePathToGameDirectory()
{
	if (AbsolutePathToGameDirectory.IsEmpty())
	{
		// Strip game directory, keep just to path to the game directory which could simply be the root dir (but not always).
		AbsolutePathToGameDirectory = FPaths::GetPath(GetAbsoluteGameDirectory());
	}
	return AbsolutePathToGameDirectory;
}

const FString& FSandboxPlatformFile::GetAbsoluteGameDirectory()
{
	if (AbsoluteGameDirectory.IsEmpty())
	{
		AbsoluteGameDirectory = FPaths::GetProjectFilePath();
		UE_CLOG(AbsoluteGameDirectory.IsEmpty(), SandboxFile, Fatal, TEXT("SandboxFileWrapper tried to access project path before it was set."));
		AbsoluteGameDirectory = FPaths::ConvertRelativePathToFull(AbsoluteGameDirectory);
		// Strip .uproject filename
		AbsoluteGameDirectory = FPaths::GetPath(AbsoluteGameDirectory);
	}
	return AbsoluteGameDirectory;
}


/**
 * Module for the sandbox file
 */
class FSandboxFileModule : public IPlatformFileModule
{
public:
	virtual IPlatformFile* GetPlatformFile() override
	{
		static TUniquePtr<IPlatformFile> AutoDestroySingleton = MakeUnique<FSandboxPlatformFile>(true);
		return AutoDestroySingleton.Get();
	}
};
IMPLEMENT_MODULE(FSandboxFileModule, SandboxFile);



