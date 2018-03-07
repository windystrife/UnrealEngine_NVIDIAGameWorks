// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CrashDebugHelper.h"
#include "HAL/FileManager.h"
#include "Misc/Parse.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Stats/StatsMisc.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/App.h"
#include "CrashDebugHelperPrivate.h"
#include "CrashDebugPDBCache.h"

#include "Misc/EngineVersion.h"
#if UE_EDITOR || IS_PROGRAM
#include "ISourceControlOperation.h"
#include "SourceControlOperations.h"
#include "ISourceControlRevision.h"
#include "ISourceControlProvider.h"
#include "ISourceControlModule.h"
#include "ISourceControlLabel.h"
#endif

#ifndef MINIDUMPDIAGNOSTICS
	#define MINIDUMPDIAGNOSTICS	0
#endif

/*-----------------------------------------------------------------------------
	FCrashDebugHelperConfig
-----------------------------------------------------------------------------*/

/**
*  Holds FullCrashDump properties from the config.
*
*	PDBCache_0_Branch=UE4-Branch
*	PDBCache_0_ExecutablePathPattern=ue4.net\Builds\UE4-Branch\%ENGINE_VERSION%
*	PDBCache_0_SymbolPathPattern=ue4.net\Builds\UE4-Branch\%ENGINE_VERSION%
*	
*	If PDBCache_0_SymbolPathPattern is missing, the value from PDBCache_0_ExecutablePathPattern will be used
*/
struct FPDBCacheConfigEntry
{
	/** Initialization constructor. */
	FPDBCacheConfigEntry( const FString& InBranch, const FString& InExecutablePathPattern, const FString& InSymbolPathPattern )
		: Branch( InBranch )
		, ExecutablePathPattern( InExecutablePathPattern )
		, SymbolPathPattern( InSymbolPathPattern )
	{}


	/** Branch name. */
	const FString Branch;

	/** Location of the executables */
	const FString ExecutablePathPattern;

	/** Location of the symbols, usually the same as the executables. */
	const FString SymbolPathPattern;
};

/** Helper struct for reading PDB cache configuration. */
struct FCrashDebugHelperConfig
{
	static FCrashDebugHelperConfig& Get()
	{
		static FCrashDebugHelperConfig Instance;
		return Instance;
	}

	bool IsValid() const
	{
		// We need a least one entry to proceed.
		return PDBCacheConfigEntries.Num() > 0;
	}

	/** Reads configuration. */
	void ReadFullCrashDumpConfigurations();

	/** Gets the config for branch. */
	FPDBCacheConfigEntry GetCacheConfigEntryForBranch( const FString& Branch ) const
	{
		const FString BranchFixed = Branch.Replace( TEXT( "/" ), TEXT( "+" ) );
		for (const auto& It : PDBCacheConfigEntries)
		{
			if (BranchFixed.Contains( It.Branch ))
			{
				return It;
			}
		}

		// Invalid entry.
		return FPDBCacheConfigEntry( Branch, FString(), FString() );
	}

protected:
	/** Returns empty string if couldn't read. */
	FString GetKey( const FString& KeyName );

	/** Configuration for PDB Cache. */
	TArray<FPDBCacheConfigEntry> PDBCacheConfigEntries;
};

void FCrashDebugHelperConfig::ReadFullCrashDumpConfigurations()
{
	for (int32 NumEntries = 0;; ++NumEntries)
	{
		const FString Branch = GetKey( FString::Printf( TEXT( "PDBCache_%i_Branch" ), NumEntries ) );
		if (Branch.IsEmpty())
		{
			break;
		}

		const FString ExecutablePathPattern = GetKey( FString::Printf( TEXT( "PDBCache_%i_ExecutablePathPattern" ), NumEntries ) );
		if (ExecutablePathPattern.IsEmpty())
		{
			break;
		}

		FString SymbolPathPattern = GetKey( FString::Printf( TEXT( "PDBCache_%i_SymbolPathPattern" ), NumEntries ) );
		if (SymbolPathPattern.IsEmpty())
		{
			SymbolPathPattern = ExecutablePathPattern;
		}
		
		PDBCacheConfigEntries.Add( FPDBCacheConfigEntry( Branch, ExecutablePathPattern, SymbolPathPattern ) );

		UE_LOG( LogCrashDebugHelper, Log, TEXT( "PDBCacheConfigEntry: Branch:%s ExecutablePathPattern:%s SymbolPathPattern:%s" ), *Branch, *ExecutablePathPattern, *SymbolPathPattern );
	}
}

FString FCrashDebugHelperConfig::GetKey( const FString& KeyName )
{
	const FString SectionName = TEXT( "Engine.CrashDebugHelper" );

	FString Result;
	if (!GConfig->GetString( *SectionName, *KeyName, Result, GEngineIni ))
	{
		return TEXT( "" );
	}
	return Result;
}

/*-----------------------------------------------------------------------------
	ICrashDebugHelper
-----------------------------------------------------------------------------*/

void ICrashDebugHelper::SetDepotIndex( FString& PathToChange )
{
	FString CmdDepotIndex;
	FParse::Value( FCommandLine::Get(), TEXT( "DepotIndex=" ), CmdDepotIndex );
	// %DEPOT_INDEX% - Index of the depot, when multiple processor are used.
	PathToChange.ReplaceInline( TEXT( "%DEPOT_INDEX%" ), *CmdDepotIndex );
}

bool ICrashDebugHelper::Init()
{
	bInitialized = true;

	CrashInfo.bMutexPDBCache = FParse::Param(FCommandLine::Get(), TEXT("MutexPDBCache"));
	FParse::Value(FCommandLine::Get(), TEXT("PDBCacheLock="), CrashInfo.PDBCacheLockName);

	// Check if we have a valid EngineVersion, if so use it.
	FString CmdEngineVersion;
	const bool bHasEngineVersion = FParse::Value( FCommandLine::Get(), TEXT( "EngineVersion=" ), CmdEngineVersion );
	if( bHasEngineVersion )
	{
		FEngineVersion EngineVersion;
		FEngineVersion::Parse( CmdEngineVersion, EngineVersion );

		// Clean branch name.
		CrashInfo.DepotName = EngineVersion.GetBranch();
		CrashInfo.BuiltFromCL = (int32)EngineVersion.GetChangelist();

		CrashInfo.EngineVersion = CmdEngineVersion;
	}
	else
	{
		// Use the current values.
		const FEngineVersion& EngineVersion = FEngineVersion::Current();
		CrashInfo.DepotName = EngineVersion.GetBranch();
		CrashInfo.BuiltFromCL = (int32)EngineVersion.GetChangelist();
		CrashInfo.EngineVersion = EngineVersion.ToString();
	}

	// Check if we have a valid BuildVersion, if so use it.
	FString CmdBuildVersion;
	const bool bHasBuildVersion = FParse::Value(FCommandLine::Get(), TEXT("BuildVersion="), CmdBuildVersion);
	if (bHasBuildVersion)
	{
		CrashInfo.BuildVersion = CmdBuildVersion;
	}
	else
	{
		CrashInfo.BuildVersion = FApp::GetBuildVersion();
	}

	FString PlatformName;
	const bool bHasPlatformName = FParse::Value(FCommandLine::Get(), TEXT("PlatformName="), PlatformName);
	if (bHasPlatformName)
	{
		CrashInfo.PlatformName = PlatformName;
	}
	else
	{
		// Use the current values.
		CrashInfo.PlatformName = FPlatformProperties::PlatformName();
	}

	FString PlatformVariantName;
	const bool bHasPlatformVariantName = FParse::Value(FCommandLine::Get(), TEXT("PlatformVariantName="), PlatformVariantName);
	if (bHasPlatformVariantName)
	{
		CrashInfo.PlatformVariantName = PlatformVariantName;
	}
	else
	{
		// Use the basic platform name.
		CrashInfo.PlatformVariantName = CrashInfo.PlatformName;
	}

	UE_LOG( LogCrashDebugHelper, Log, TEXT( "DepotName: %s" ), *CrashInfo.DepotName );
	UE_LOG( LogCrashDebugHelper, Log, TEXT( "BuiltFromCL: %i" ), CrashInfo.BuiltFromCL );
	UE_LOG( LogCrashDebugHelper, Log, TEXT( "EngineVersion: %s" ), *CrashInfo.EngineVersion );
	UE_LOG( LogCrashDebugHelper, Log, TEXT( "BuildVersion: %s" ), *CrashInfo.BuildVersion );

	GConfig->GetString( TEXT( "Engine.CrashDebugHelper" ), TEXT( "SourceControlBuildLabelPattern" ), SourceControlBuildLabelPattern, GEngineIni );

	FCrashDebugHelperConfig::Get().ReadFullCrashDumpConfigurations();

	const bool bUsePDBCache = FCrashDebugHelperConfig::Get().IsValid();
	UE_CLOG( !bUsePDBCache, LogCrashDebugHelper, Warning, TEXT( "CrashDebugHelperConfig invalid" ) );

	if (bUsePDBCache)
	{
		if (CrashInfo.bMutexPDBCache && !CrashInfo.PDBCacheLockName.IsEmpty())
		{
			// Scoped lock
			FSystemWideCriticalSection PDBCacheLock(CrashInfo.PDBCacheLockName, FTimespan::FromMinutes(10.0));
			if (PDBCacheLock.IsValid())
			{
				FPDBCache::Get().Init();
			}
		}
		else
		{
			FPDBCache::Get().Init();
		}

		if (!FPDBCache::Get().UsePDBCache())
		{
			UE_LOG(LogCrashDebugHelper, Warning, TEXT("PDB Cache failed to initialize"));
		}
	}
	else
	{
		UE_LOG( LogCrashDebugHelper, Warning, TEXT( "PDB Cache disabled" ) );
	}	

	return bInitialized;
}

/** 
 * Initialise the source control interface, and ensure we have a valid connection
 */
bool ICrashDebugHelper::InitSourceControl(bool bShowLogin)
{
	// Ensure we are in a valid state to sync
	if (bInitialized == false)
	{
		UE_LOG(LogCrashDebugHelper, Warning, TEXT("InitSourceControl: CrashDebugHelper is not initialized properly."));
		return false;
	}

#if UE_EDITOR || IS_PROGRAM
	// Initialize the source control if it hasn't already been
	if( !ISourceControlModule::Get().IsEnabled() || !ISourceControlModule::Get().GetProvider().IsAvailable() )
	{
		// make sure our provider is set to Perforce
		ISourceControlModule::Get().SetProvider("Perforce");

		// Attempt to load in a source control module
		ISourceControlModule::Get().GetProvider().Init();

#if !MINIDUMPDIAGNOSTICS
		if ((ISourceControlModule::Get().GetProvider().IsAvailable() == false) || bShowLogin)
		{
			// Unable to connect? Prompt the user for login information
			ISourceControlModule::Get().ShowLoginDialog(FSourceControlLoginClosed(), ELoginWindowMode::Modeless, EOnLoginWindowStartup::PreserveProvider);
		}
#endif
		// If it's still disabled, none was found, so exit
		if( !ISourceControlModule::Get().IsEnabled() || !ISourceControlModule::Get().GetProvider().IsAvailable() )
		{
			UE_LOG(LogCrashDebugHelper, Warning, TEXT("InitSourceControl: Source control unavailable or disabled."));
			return false;
		}
	}
#endif

	return true;
}

/** 
 * Shutdown the connection to source control
 */
void ICrashDebugHelper::ShutdownSourceControl()
{
#if UE_EDITOR || IS_PROGRAM
	ISourceControlModule::Get().GetProvider().Close();
#endif
}


bool ICrashDebugHelper::SyncModules(bool& bOutPDBCacheEntryValid)
{
	bOutPDBCacheEntryValid = false;

	if( !FPDBCache::Get().UsePDBCache() )
	{
		UE_LOG( LogCrashDebugHelper, Warning, TEXT( "The PDB Cache is disabled, cannot proceed, %s" ), *CrashInfo.EngineVersion );
		return false;
	}

	// #CrashReport: 2015-02-23 Obsolete, remove after 4.8
	const TCHAR* UESymbols = TEXT( "Rocket/Symbols/" );
	const bool bHasExecutable = !CrashInfo.ExecutablesPath.IsEmpty();
	const bool bHasSymbols = !CrashInfo.SymbolsPath.IsEmpty();
	
	if( bHasExecutable && bHasSymbols )
	{
		if(FPDBCache::Get().ContainsPDBCacheEntry(CrashInfo.BuildVersion))
		{
			UE_LOG( LogCrashDebugHelper, Warning, TEXT( "Using cached storage: %s" ), *CrashInfo.BuildVersion);
			CrashInfo.PDBCacheEntry = FPDBCache::Get().FindAndTouchPDBCacheEntry( CrashInfo.BuildVersion);
		}
		else
		{
			SCOPE_LOG_TIME_IN_SECONDS( TEXT( "SyncExecutableAndSymbolsFromNetwork" ), nullptr );

			// Find all executables.
			TArray<FString> NetworkExecutables;

			// Don't duplicate work.
			if (CrashInfo.ExecutablesPath != CrashInfo.SymbolsPath)
			{
				IFileManager::Get().FindFilesRecursive( NetworkExecutables, *CrashInfo.ExecutablesPath, TEXT( "*.dll" ), true, false, false );
				IFileManager::Get().FindFilesRecursive( NetworkExecutables, *CrashInfo.ExecutablesPath, TEXT( "*.exe" ), true, false, false );
			}

			// Find all symbols.
			TArray<FString> NetworkSymbols;
			IFileManager::Get().FindFilesRecursive( NetworkSymbols, *CrashInfo.SymbolsPath, TEXT( "*.pdb" ), true, false, false );
			IFileManager::Get().FindFilesRecursive( NetworkSymbols, *CrashInfo.SymbolsPath, TEXT( "*.dll" ), true, false, false );
			IFileManager::Get().FindFilesRecursive( NetworkSymbols, *CrashInfo.SymbolsPath, TEXT( "*.exe" ), true, false, false );

			// From=Full pathname
			// To=Relative pathname
			TMap<FString, FString> FilesToBeCached;

			for( const auto& ExecutablePath : NetworkExecutables )
			{
				const FString NetworkRelativePath = ExecutablePath.Replace( *CrashInfo.ExecutablesPath, TEXT( "" ) );
				FilesToBeCached.Add( ExecutablePath, NetworkRelativePath );
			}

			for( const auto& SymbolPath : NetworkSymbols )
			{
				const FString SymbolRelativePath = SymbolPath.Replace( *CrashInfo.SymbolsPath, TEXT( "" ) );
				FilesToBeCached.Add( SymbolPath, SymbolRelativePath );
			}

			// Initialize and add a new PDB Cache entry to the database.
			CrashInfo.PDBCacheEntry = FPDBCache::Get().CreateAndAddPDBCacheEntryMixed( CrashInfo.BuildVersion, FilesToBeCached );
		}
	}
	// OBSOLETE PATH
	else
	{
		// Command line for blocking obsolete path
#if UE_EDITOR || IS_PROGRAM
		const bool bNoP4Symbols = FParse::Param(FCommandLine::Get(), TEXT("NoP4Symbols"));

		// Check source control
		if (bNoP4Symbols || !ISourceControlModule::Get().IsEnabled())
		{
			return false;
		}

		// Get all labels associated with the crash info's label.
		TArray< TSharedRef<ISourceControlLabel> > Labels = ISourceControlModule::Get().GetProvider().GetLabels(CrashInfo.LabelName);

		if (Labels.Num() >= 1)
		{
			TSharedRef<ISourceControlLabel> Label = Labels[0];
			TSet<FString> FilesToSync;

			if (FPDBCache::Get().ContainsPDBCacheEntry(CrashInfo.EngineVersion))
			{
				UE_LOG(LogCrashDebugHelper, Warning, TEXT("Using cached storage: %s"), *CrashInfo.EngineVersion);
				CrashInfo.PDBCacheEntry = FPDBCache::Get().FindAndTouchPDBCacheEntry(CrashInfo.EngineVersion);
			}
			// Use product version instead of label name to make a distinguish between chosen methods.
			else if (FPDBCache::Get().ContainsPDBCacheEntry(CrashInfo.LabelName))
			{
				UE_LOG(LogCrashDebugHelper, Warning, TEXT("Using cached storage: %s"), *CrashInfo.LabelName);
				CrashInfo.PDBCacheEntry = FPDBCache::Get().FindAndTouchPDBCacheEntry(CrashInfo.LabelName);
			}
			else if (bHasExecutable)
			{
				SCOPE_LOG_TIME_IN_SECONDS(TEXT("SyncModulesAndNetwork"), nullptr);

				// Grab information about symbols.
				TArray< TSharedRef<class ISourceControlRevision, ESPMode::ThreadSafe> > PDBSourceControlRevisions;
				const FString PDBsPath = FString::Printf(TEXT("%s/%s....pdb"), *CrashInfo.DepotName, UESymbols);
				Label->GetFileRevisions(PDBsPath, PDBSourceControlRevisions);

				TSet<FString> PDBPaths;
				for (const auto& PDBSrc : PDBSourceControlRevisions)
				{
					PDBPaths.Add(PDBSrc->GetFilename());
				}

				// Now, sync symbols.
				for (const auto& PDBPath : PDBPaths)
				{
					if (Label->Sync(PDBPath))
					{
						UE_LOG(LogCrashDebugHelper, Warning, TEXT("Synced PDB: %s"), *PDBPath);
					}
				}

				// Find all the executables in the product network path.
				TArray<FString> NetworkExecutables;
				IFileManager::Get().FindFilesRecursive(NetworkExecutables, *CrashInfo.ExecutablesPath, TEXT("*.dll"), true, false, false);
				IFileManager::Get().FindFilesRecursive(NetworkExecutables, *CrashInfo.ExecutablesPath, TEXT("*.exe"), true, false, false);

				// From=Full pathname
				// To=Relative pathname
				TMap<FString, FString> FilesToBeCached;

				// If a symbol matches an executable, add the pair to the list of files that should be cached.
				for (const auto& NetworkExecutableFullpath : NetworkExecutables)
				{
					for (const auto& PDBPath : PDBPaths)
					{
						const FString PDBRelativePath = PDBPath.Replace(*CrashInfo.DepotName, TEXT("")).Replace(UESymbols, TEXT(""));
						const FString PDBFullpath = FPDBCache::Get().GetDepotRoot() / PDBPath;

						const FString PDBMatch = PDBRelativePath.Replace(TEXT("pdb"), TEXT(""));
						const FString NetworkRelativePath = NetworkExecutableFullpath.Replace(*CrashInfo.ExecutablesPath, TEXT(""));
						const bool bMatch = NetworkExecutableFullpath.Contains(PDBMatch);
						if (bMatch)
						{
							// From -> Where
							FilesToBeCached.Add(NetworkExecutableFullpath, NetworkRelativePath);
							FilesToBeCached.Add(PDBFullpath, PDBRelativePath);
							break;
						}
					}
				}

				// Initialize and add a new PDB Cache entry to the database.
				CrashInfo.PDBCacheEntry = FPDBCache::Get().CreateAndAddPDBCacheEntryMixed(CrashInfo.EngineVersion, FilesToBeCached);
			}
			else
			{
				TArray<FString> FilesToBeCached;

				//@TODO: MAC: Excluding labels for Mac since we are only syncing windows binaries here...
				if (Label->GetName().Contains(TEXT("Mac")))
				{
					UE_LOG(LogCrashDebugHelper, Log, TEXT("Skipping Mac label: %s"), *Label->GetName());
				}
				else
				{
					// Sync all the dll, exes, and related symbol files
					UE_LOG(LogCrashDebugHelper, Log, TEXT("Syncing modules with label: %s"), *Label->GetName());

					SCOPE_LOG_TIME_IN_SECONDS(TEXT("SyncModules"), nullptr);

					// Grab all dll and pdb files for the specified label.
					TArray< TSharedRef<class ISourceControlRevision, ESPMode::ThreadSafe> > DLLSourceControlRevisions;
					const FString DLLsPath = FString::Printf(TEXT("%s/....dll"), *CrashInfo.DepotName);
					Label->GetFileRevisions(DLLsPath, DLLSourceControlRevisions);

					TArray< TSharedRef<class ISourceControlRevision, ESPMode::ThreadSafe> > EXESourceControlRevisions;
					const FString EXEsPath = FString::Printf(TEXT("%s/....exe"), *CrashInfo.DepotName);
					Label->GetFileRevisions(EXEsPath, EXESourceControlRevisions);

					TArray< TSharedRef<class ISourceControlRevision, ESPMode::ThreadSafe> > PDBSourceControlRevisions;
					const FString PDBsPath = FString::Printf(TEXT("%s/....pdb"), *CrashInfo.DepotName);
					Label->GetFileRevisions(PDBsPath, PDBSourceControlRevisions);

					TSet<FString> ModulesPaths;
					for (const auto& DLLSrc : DLLSourceControlRevisions)
					{
						ModulesPaths.Add(DLLSrc->GetFilename().Replace(*CrashInfo.DepotName, TEXT("")));
					}
					for (const auto& EXESrc : EXESourceControlRevisions)
					{
						ModulesPaths.Add(EXESrc->GetFilename().Replace(*CrashInfo.DepotName, TEXT("")));
					}

					TSet<FString> PDBPaths;
					for (const auto& PDBSrc : PDBSourceControlRevisions)
					{
						PDBPaths.Add(PDBSrc->GetFilename().Replace(*CrashInfo.DepotName, TEXT("")));
					}

					// Iterate through all module and see if we have dll and pdb associated with the module, if so add it to the files to sync.
					for (const auto& ModuleName : CrashInfo.ModuleNames)
					{
						const FString ModuleNamePDB = ModuleName.Replace(TEXT(".dll"), TEXT(".pdb")).Replace(TEXT(".exe"), TEXT(".pdb"));

						for (const auto& ModulePath : ModulesPaths)
						{
							const bool bContainsModule = ModulePath.Contains(ModuleName);
							if (bContainsModule)
							{
								FilesToSync.Add(ModulePath);
							}
						}

						for (const auto& PDBPath : PDBPaths)
						{
							const bool bContainsPDB = PDBPath.Contains(ModuleNamePDB);
							if (bContainsPDB)
							{
								FilesToSync.Add(PDBPath);
							}
						}
					}

					// Now, sync all files.
					for (const auto& Filename : FilesToSync)
					{
						const FString DepotPath = CrashInfo.DepotName + Filename;
						if (Label->Sync(DepotPath))
						{
							UE_LOG(LogCrashDebugHelper, Warning, TEXT("Synced binary: %s"), *DepotPath);
						}
						FilesToBeCached.Add(DepotPath);
					}
				}

				// Initialize and add a new PDB Cache entry to the database.
				CrashInfo.PDBCacheEntry = FPDBCache::Get().CreateAndAddPDBCacheEntry(CrashInfo.LabelName, CrashInfo.DepotName, FilesToBeCached);
			}
		}
		else
		{
			UE_LOG(LogCrashDebugHelper, Error, TEXT("Could not find label: %s"), *CrashInfo.LabelName);
			return false;
		}
#endif
	}

	bOutPDBCacheEntryValid = CrashInfo.PDBCacheEntry.IsValid() && CrashInfo.PDBCacheEntry->Files.Num() > 0;
	return true;
}

bool ICrashDebugHelper::SyncSourceFile()
{
#if UE_EDITOR || IS_PROGRAM
	// Check source control
	if( !ISourceControlModule::Get().IsEnabled() )
	{
		return false;
	}

	// Sync a single source file to requested CL.
	FString DepotPath = CrashInfo.DepotName / CrashInfo.SourceFile + TEXT( "@" ) + TTypeToString<int32>::ToString( CrashInfo.BuiltFromCL );
	ISourceControlModule::Get().GetProvider().Execute(ISourceControlOperation::Create<FSync>(), DepotPath);

	UE_LOG( LogCrashDebugHelper, Warning, TEXT( "Syncing a single source file: %s"), *DepotPath );
#endif

	return true;
}


bool ICrashDebugHelper::ReadSourceFile( TArray<FString>& OutStrings )
{
	const bool bUsePDBCache = FPDBCache::Get().UsePDBCache();

	FString FilePath;
	if (bUsePDBCache)
	{
		// We assume a special folder for syncing all streams and //depot/ is not used in the view mapping.
		/*
			//depot/... //machine/... 
			//UE4/...	//machine/Stream/UE4/...
		*/
		const TCHAR* DepotDelimiter = TEXT( "//depot/" );
		const bool bIsDepot = CrashInfo.DepotName.Contains( DepotDelimiter );

		if (bIsDepot)
		{
			FilePath = FPDBCache::Get().GetDepotRoot() / CrashInfo.DepotName.Replace( DepotDelimiter, TEXT( "" ) ) / CrashInfo.SourceFile;
		}
		else
		{
			FilePath = FPDBCache::Get().GetDepotRoot() / TEXT( "Stream" ) / CrashInfo.DepotName / CrashInfo.SourceFile;
			FilePath.ReplaceInline( TEXT( "//" ), TEXT( "/" ) );
		}
	}
	else
	{
		FilePath = FPaths::RootDir() / CrashInfo.SourceFile;
	}

	FString Line;
	if (FFileHelper::LoadFileToString( Line, *FilePath ))
	{
		Line = Line.Replace( TEXT( "\r" ), TEXT( "" ) );
		Line.ParseIntoArray( OutStrings, TEXT( "\n" ), false );
		UE_LOG( LogCrashDebugHelper, Log, TEXT( "Reading a single source file: %s" ), *FilePath );
		return true;
	}
	else
	{
		UE_LOG( LogCrashDebugHelper, Warning, TEXT( "Failed to open source file: %s" ), *FilePath );
		return false;
	}
}

void ICrashDebugHelper::AddSourceToReport()
{
	if( CrashInfo.SourceFile.Len() > 0 && CrashInfo.SourceLineNumber != 0 )
	{
		TArray<FString> Lines;
		ReadSourceFile( Lines );

		const uint32 MinLine = FMath::Clamp( CrashInfo.SourceLineNumber - 15, (uint32)1, (uint32)Lines.Num() );
		const uint32 MaxLine = FMath::Clamp( CrashInfo.SourceLineNumber + 15, (uint32)1, (uint32)Lines.Num() );

		for( uint32 Line = MinLine; Line < MaxLine; Line++ )
		{
			if( Line == CrashInfo.SourceLineNumber - 1 )
			{
				CrashInfo.SourceContext.Add( FString::Printf( TEXT( "%5u ***** %s" ), Line, *Lines[Line] ) );
			}
			else
			{
				CrashInfo.SourceContext.Add( FString::Printf( TEXT( "%5u       %s" ), Line, *Lines[Line] ) );
			}
		}
	}
}

bool ICrashDebugHelper::AddAnnotatedSourceToReport()
{
	// Make sure we have a source file to interrogate
	if( CrashInfo.SourceFile.Len() > 0 && CrashInfo.SourceLineNumber != 0 && !CrashInfo.LabelName.IsEmpty() )
	{
#if UE_EDITOR || IS_PROGRAM
		// Check source control
		if( !ISourceControlModule::Get().IsEnabled() )
		{
			return false;
		}

		// Ask source control to annotate the file for us
		FString DepotPath = CrashInfo.DepotName / CrashInfo.SourceFile;

		TArray<FAnnotationLine> Lines;
		SourceControlHelpers::AnnotateFile( ISourceControlModule::Get().GetProvider(), CrashInfo.BuiltFromCL, DepotPath, Lines );

		uint32 MinLine = FMath::Clamp( CrashInfo.SourceLineNumber - 15, (uint32)1, (uint32)Lines.Num() );
		uint32 MaxLine = FMath::Clamp( CrashInfo.SourceLineNumber + 15, (uint32)1, (uint32)Lines.Num() );

		// Display a source context in the report, and decorate each line with the last editor of the line
		for( uint32 Line = MinLine; Line < MaxLine; Line++ )
		{			
			if( Line == CrashInfo.SourceLineNumber )
			{
				CrashInfo.SourceContext.Add( FString::Printf( TEXT( "%5u ***** %20s: %s" ), Line, *Lines[Line].UserName, *Lines[Line].Line ) );
			}
			else
			{
				CrashInfo.SourceContext.Add( FString::Printf( TEXT( "%5u       %20s: %s" ), Line, *Lines[Line].UserName, *Lines[Line].Line ) );
			}
		}
#endif
		return true;
	}

	return false;
}

void FCrashInfo::Log( FString Line )
{
	UE_LOG( LogCrashDebugHelper, Warning, TEXT("%s"), *Line );
	Report += Line + LINE_TERMINATOR;
}


const TCHAR* FCrashInfo::GetProcessorArchitecture( EProcessorArchitecture PA )
{
	switch( PA )
	{
	case PA_X86:
		return TEXT( "x86" );
	case PA_X64:
		return TEXT( "x64" );
	case PA_ARM:
		return TEXT( "ARM" );
	}

	return TEXT( "Unknown" );
}


int64 FCrashInfo::StringSize( const ANSICHAR* Line )
{
	int64 Size = 0;
	if( Line != nullptr )
	{
		while( *Line++ != 0 )
		{
			Size++;
		}
	}
	return Size;
}


void FCrashInfo::WriteLine( FArchive* ReportFile, const ANSICHAR* Line )
{
	if( Line != NULL )
	{
		int64 StringBytes = StringSize( Line );
		ReportFile->Serialize( ( void* )Line, StringBytes );
	}

	ReportFile->Serialize( TCHAR_TO_UTF8( LINE_TERMINATOR ), FCStringWide::Strlen(LINE_TERMINATOR) );
}


void FCrashInfo::GenerateReport( const FString& DiagnosticsPath )
{
	FArchive* ReportFile = IFileManager::Get().CreateFileWriter( *DiagnosticsPath );
	if( ReportFile != NULL )
	{
		FString Line;

		WriteLine( ReportFile, TCHAR_TO_UTF8( TEXT( "Generating report for minidump" ) ) );
		WriteLine( ReportFile );

		if ( EngineVersion.Len() > 0 )
		{
			Line = FString::Printf( TEXT( "Application version %s" ), *EngineVersion );
			WriteLine( ReportFile, TCHAR_TO_UTF8( *Line ) );
		}
		else if( Modules.Num() > 0 )
		{
			Line = FString::Printf( TEXT( "Application version %d.%d.%d" ), Modules[0].Major, Modules[0].Minor, Modules[0].Patch );
			WriteLine( ReportFile, TCHAR_TO_UTF8( *Line ) );
		}

		Line = FString::Printf( TEXT( " ... built from changelist %d" ), BuiltFromCL );
		WriteLine( ReportFile, TCHAR_TO_UTF8( *Line ) );
		if( LabelName.Len() > 0 )
		{
			Line = FString::Printf( TEXT( " ... based on label %s" ), *LabelName );
			WriteLine( ReportFile, TCHAR_TO_UTF8( *Line ) );
		}
		WriteLine( ReportFile );

		Line = FString::Printf( TEXT( "OS version %d.%d.%d.%d" ), SystemInfo.OSMajor, SystemInfo.OSMinor, SystemInfo.OSBuild, SystemInfo.OSRevision );
		WriteLine( ReportFile, TCHAR_TO_UTF8( *Line ) );

		Line = FString::Printf( TEXT( "Running %d %s processors" ), SystemInfo.ProcessorCount, GetProcessorArchitecture( SystemInfo.ProcessorArchitecture ) );
		WriteLine( ReportFile, TCHAR_TO_UTF8( *Line ) );

		Line = FString::Printf( TEXT( "Exception was \"%s\"" ), *Exception.ExceptionString );
		WriteLine( ReportFile, TCHAR_TO_UTF8( *Line ) );
		WriteLine( ReportFile );

		Line = FString::Printf( TEXT( "Source context from \"%s\"" ), *SourceFile );
		WriteLine( ReportFile, TCHAR_TO_UTF8( *Line ) );
		WriteLine( ReportFile );

		Line = FString::Printf( TEXT( "<SOURCE START>" ) );
		WriteLine( ReportFile, TCHAR_TO_UTF8( *Line ) );
		for( int32 LineIndex = 0; LineIndex < SourceContext.Num(); LineIndex++ )
		{
			Line = FString::Printf( TEXT( "%s" ), *SourceContext[LineIndex] );
			WriteLine( ReportFile, TCHAR_TO_UTF8( *Line ) );
		}
		Line = FString::Printf( TEXT( "<SOURCE END>" ) );
		WriteLine( ReportFile, TCHAR_TO_UTF8( *Line ) );
		WriteLine( ReportFile );

		Line = FString::Printf( TEXT( "<CALLSTACK START>" ) );
		WriteLine( ReportFile, TCHAR_TO_UTF8( *Line ) );

		for( int32 StackIndex = 0; StackIndex < Exception.CallStackString.Num(); StackIndex++ )
		{
			Line = FString::Printf( TEXT( "%s" ), *Exception.CallStackString[StackIndex] );
			WriteLine( ReportFile, TCHAR_TO_UTF8( *Line ) );
		}

		Line = FString::Printf( TEXT( "<CALLSTACK END>" ) );
		WriteLine( ReportFile, TCHAR_TO_UTF8( *Line ) );
		WriteLine( ReportFile );

		Line = FString::Printf( TEXT( "%d loaded modules" ), Modules.Num() );
		WriteLine( ReportFile, TCHAR_TO_UTF8( *Line ) );

		for( int32 ModuleIndex = 0; ModuleIndex < Modules.Num(); ModuleIndex++ )
		{
			FCrashModuleInfo& Module = Modules[ModuleIndex];

			FString ModuleDirectory = FPaths::GetPath(Module.Name);
			FString ModuleName = FPaths::GetBaseFilename( Module.Name, true ) + FPaths::GetExtension( Module.Name, true );

			FString ModuleDetail = FString::Printf( TEXT( "%40s" ), *ModuleName );
			FString Version = FString::Printf( TEXT( " (%d.%d.%d.%d)" ), Module.Major, Module.Minor, Module.Patch, Module.Revision );
			ModuleDetail += FString::Printf( TEXT( " %22s" ), *Version );
			ModuleDetail += FString::Printf( TEXT( " 0x%016x 0x%08x" ), Module.BaseOfImage, Module.SizeOfImage );
			ModuleDetail += FString::Printf( TEXT( " %s" ), *ModuleDirectory );

			WriteLine( ReportFile, TCHAR_TO_UTF8( *ModuleDetail ) );
		}

		WriteLine( ReportFile );

		// Write out the processor debugging log
		WriteLine( ReportFile, TCHAR_TO_UTF8( *Report ) );

		Line = FString::Printf( TEXT( "Report end!" ) );
		WriteLine( ReportFile, TCHAR_TO_UTF8( *Line )  );

		ReportFile->Close();
		delete ReportFile;
	}
}

void ICrashDebugHelper::FindSymbolsAndBinariesStorage()
{
	CrashInfo.ExecutablesPath.Empty();
	CrashInfo.SymbolsPath.Empty();
	CrashInfo.LabelName.Empty();

	if( CrashInfo.BuiltFromCL == FCrashInfo::INVALID_CHANGELIST )
	{
		UE_LOG( LogCrashDebugHelper, Warning, TEXT( "Invalid parameters" ) );
		return;
	}

	UE_LOG( LogCrashDebugHelper, Log, TEXT( "Engine version: %s" ), *CrashInfo.EngineVersion );

	const FPDBCacheConfigEntry ConfigEntry = FCrashDebugHelperConfig::Get().GetCacheConfigEntryForBranch( CrashInfo.DepotName );
	FString ExecutablePathPattern = ConfigEntry.ExecutablePathPattern;
	FString SymbolPathPattern = ConfigEntry.SymbolPathPattern;

	if (!ExecutablePathPattern.IsEmpty() || !SymbolPathPattern.IsEmpty())
	{
		UE_LOG( LogCrashDebugHelper, Log, TEXT( "Using branch: %s" ), *CrashInfo.DepotName );
	}
	else
	{
		UE_LOG( LogCrashDebugHelper, Log, TEXT( "Branch not found: %s" ), *CrashInfo.DepotName );
		return;
	}
	
	const FString StrENGINE_VERSION = CrashInfo.EngineVersion;
	const FString StrPLATFORM_NAME = CrashInfo.PlatformName;
	const FString StrPLATFORM_VARIANT = CrashInfo.PlatformVariantName;
	const FString StrOLD_ENGINE_VERSION = FString::Printf( TEXT( "%s-CL-%i" ), *CrashInfo.DepotName.Replace( TEXT( "+" ), TEXT( "/" ) ), CrashInfo.BuiltFromCL )
		.Replace( TEXT("/"), TEXT("+") );
	const FString StrBUILD_VERSION = CrashInfo.BuildVersion;

	const FString TestExecutablesPath = ExecutablePathPattern
		.Replace( TEXT( "%ENGINE_VERSION%" ), *StrENGINE_VERSION )
		.Replace( TEXT( "%PLATFORM_NAME%" ), *StrPLATFORM_NAME )
		.Replace( TEXT( "%PLATFORM_VARIANT%" ), *StrPLATFORM_VARIANT )
		.Replace( TEXT( "%OLD_ENGINE_VERSION%" ), *StrOLD_ENGINE_VERSION )
		.Replace( TEXT( "%BUILD_VERSION%" ), *StrBUILD_VERSION );

	const FString TestSymbolsPath = SymbolPathPattern
		.Replace( TEXT( "%ENGINE_VERSION%" ), *StrENGINE_VERSION )
		.Replace( TEXT( "%PLATFORM_NAME%" ), *StrPLATFORM_NAME )
		.Replace( TEXT( "%PLATFORM_VARIANT%" ), *StrPLATFORM_VARIANT )
		.Replace( TEXT( "%OLD_ENGINE_VERSION%" ), *StrOLD_ENGINE_VERSION )
		.Replace( TEXT( "%BUILD_VERSION%" ), *StrBUILD_VERSION );

	// Try to find the network path by using the pattern supplied via ini.
	// If this step successes, we will grab the executable from the network path instead of P4.
	bool bFoundDirectory = false;
	
	bool bHasExecutables = false;
	if (!TestExecutablesPath.IsEmpty())
	{
		bHasExecutables = IFileManager::Get().DirectoryExists(*TestExecutablesPath);
	}
	bool bHasSymbols = false;
	if (!TestSymbolsPath.IsEmpty())
	{
		bHasSymbols = IFileManager::Get().DirectoryExists(*TestSymbolsPath);
	}

	if( bHasExecutables && bHasSymbols )
	{
		CrashInfo.ExecutablesPath = TestExecutablesPath;
		CrashInfo.SymbolsPath = TestSymbolsPath;
		bFoundDirectory = true;
		UE_LOG( LogCrashDebugHelper, Log, TEXT( "Using path for executables and symbols: %s" ), *CrashInfo.ExecutablesPath );
	}
	else if( bHasExecutables )
	{
		CrashInfo.ExecutablesPath = TestExecutablesPath;
		UE_LOG( LogCrashDebugHelper, Log, TEXT( "Using path for executables: %s" ), *CrashInfo.ExecutablesPath );
	}
	else
	{
		UE_LOG( LogCrashDebugHelper, Log, TEXT( "Path for executables not found: %s" ), *TestExecutablesPath );
	}
	

	// Try to find the label directly in source control by using the pattern supplied via ini.
	if( !bFoundDirectory && !SourceControlBuildLabelPattern.IsEmpty() )
	{	
		const FString ChangelistString = FString::Printf( TEXT( "%d" ), CrashInfo.BuiltFromCL );
		const FString LabelWithCL = SourceControlBuildLabelPattern.Replace( TEXT( "%CHANGELISTNUMBER%" ), *ChangelistString, ESearchCase::CaseSensitive );
		UE_LOG( LogCrashDebugHelper, Log, TEXT( "Label matching pattern: %s" ), *LabelWithCL );

#if UE_EDITOR || IS_PROGRAM
		TArray< TSharedRef<ISourceControlLabel> > Labels = ISourceControlModule::Get().GetProvider().GetLabels( LabelWithCL );
		if( Labels.Num() > 0 )
		{
			const int32 LabelIndex = 0;
			CrashInfo.LabelName = Labels[LabelIndex]->GetName();;
		
			// If we found more than one label, warn about it and just use the first one
			if( Labels.Num() > 1 )
			{
				UE_LOG( LogCrashDebugHelper, Warning, TEXT( "More than one build label found, using label: %s" ), *LabelWithCL, *CrashInfo.LabelName );
			}
			else
			{
				UE_LOG( LogCrashDebugHelper, Log, TEXT( "Using label: %s" ), *CrashInfo.LabelName );
			}		
		}
#endif
	}
}
