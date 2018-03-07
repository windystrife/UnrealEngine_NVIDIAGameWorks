// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "HAL/PlatformFilemanager.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "HAL/ThreadSafeCounter.h"
#include "Misc/Guid.h"
#include "HAL/IConsoleManager.h"
#include "Misc/App.h"
#include "DerivedDataBackendInterface.h"
#include "DerivedDataCacheUsageStats.h"
#include "MemoryDerivedDataBackend.h"
#include "DerivedDataBackendAsyncPutWrapper.h"
#include "PakFileDerivedDataBackend.h"
#include "HierarchicalDerivedDataBackend.h"
#include "DerivedDataLimitKeyLengthWrapper.h"
#include "DerivedDataBackendCorruptionWrapper.h"
#include "DerivedDataBackendVerifyWrapper.h"
#include "DerivedDataUtilsInterface.h"
#include "Misc/EngineBuildSettings.h"
#include "Modules/ModuleManager.h"
#include "Misc/ConfigCacheIni.h"

DEFINE_LOG_CATEGORY(LogDerivedDataCache);

#define MAX_BACKEND_KEY_LENGTH (120)
#define LOCTEXT_NAMESPACE "DerivedDataBackendGraph"

FDerivedDataBackendInterface* CreateFileSystemDerivedDataBackend(const TCHAR* CacheDirectory, bool bForceReadOnly = false, bool bTouchFiles = false, bool bPurgeTransient = false, bool bDeleteOldFiles = false, int32 InDaysToDeleteUnusedFiles = 60, int32 InMaxNumFoldersToCheck = -1, int32 InMaxContinuousFileChecks = -1);

/**
  * This class is used to create a singleton that represents the derived data cache hierarchy and all of the wrappers necessary
  * ideally this would be data driven and the backends would be plugins...
**/
class FDerivedDataBackendGraph : public FDerivedDataBackend
{
public:
	/**
	* constructor, builds the cache tree
	**/
	FDerivedDataBackendGraph()
		: RootCache(NULL)
		, BootCache(NULL)
		, WritePakCache(NULL)
		, AsyncPutWrapper(NULL)
		, KeyLengthWrapper(NULL)
		, HierarchicalWrapper(NULL)
		, MountPakCommand(
		TEXT( "DDC.MountPak" ),
		*LOCTEXT("CommandText_DDCMountPak", "Mounts read-only pak file").ToString(),
		FConsoleCommandWithArgsDelegate::CreateRaw( this, &FDerivedDataBackendGraph::MountPakCommandHandler ) )
		, UnountPakCommand(
		TEXT( "DDC.UnmountPak" ),
		*LOCTEXT("CommandText_DDCUnmountPak", "Unmounts read-only pak file").ToString(),
		FConsoleCommandWithArgsDelegate::CreateRaw( this, &FDerivedDataBackendGraph::UnmountPakCommandHandler ) )
	{
		check(IsInGameThread()); // we pretty much need this to be initialized from the main thread...it uses GConfig, etc
		check(GConfig && GConfig->IsReadyForUse());
		RootCache = NULL;
		TMap<FString, FDerivedDataBackendInterface*> ParsedNodes;		

		// Create the graph using ini settings
		if( FParse::Value( FCommandLine::Get(), TEXT("-DDC="), GraphName ) )
		{
			if( GraphName.Len() > 0 )
			{
				RootCache = ParseNode( TEXT("Root"), GEngineIni, *GraphName, ParsedNodes );				
			}

			if( RootCache == NULL )
			{
				// Remove references to any backend instances that might have been created
				ParsedNodes.Empty();
				DestroyCreatedBackends();
				UE_LOG( LogDerivedDataCache, Warning, TEXT("FDerivedDataBackendGraph:  Unable to create backend graph using the specified graph settings (%s). Reverting to default."), *GraphName );
			}
		}

		if( !RootCache )
		{
			// Use default graph
			GraphName = FApp::IsEngineInstalled() ? TEXT("InstalledDerivedDataBackendGraph") : TEXT("DerivedDataBackendGraph");
			FString Entry;
			if( !GConfig->GetString( *GraphName, TEXT("Root"), Entry, GEngineIni ) || !Entry.Len())
			{
				UE_LOG( LogDerivedDataCache, Fatal, TEXT("Unable to create backend graph using the default graph settings (%s) ini=%s."), *GraphName, *GEngineIni );
			}
			RootCache = ParseNode( TEXT("Root"), GEngineIni, *GraphName, ParsedNodes );
		}
		check(RootCache);

		// Make sure AsyncPutWrapper and KeyLengthWrapper are created
		if( !AsyncPutWrapper )
		{
			AsyncPutWrapper = new FDerivedDataBackendAsyncPutWrapper( RootCache, true );
			check(AsyncPutWrapper);
			CreatedBackends.Add( AsyncPutWrapper );
			RootCache = AsyncPutWrapper;
		}
		if ( !KeyLengthWrapper )
		{
			KeyLengthWrapper = new FDerivedDataLimitKeyLengthWrapper( RootCache, MAX_BACKEND_KEY_LENGTH );
			check(KeyLengthWrapper);
			CreatedBackends.Add( KeyLengthWrapper );
			RootCache = KeyLengthWrapper;
		}
	}

	/**
	 * Helper function to get the value of parsed bool as the return value
	 **/
	bool GetParsedBool( const TCHAR* Stream, const TCHAR* Match ) const
	{
		bool bValue = 0;
		FParse::Bool( Stream, Match, bValue );
		return bValue;
	}

	/**
	 * Parses backend graph node from ini settings
	 *
	 * @param NodeName Name of the node to parse
	 * @param IniFilename Ini filename
	 * @param IniSection Section in the ini file containing the graph definition
	 * @param InParsedNodes Map of parsed nodes and their names to be able to find already parsed nodes
	 * @return Derived data backend interface instance created from ini settings
	 */
	FDerivedDataBackendInterface* ParseNode( const TCHAR* NodeName, const FString& IniFilename, const TCHAR* IniSection, TMap<FString, FDerivedDataBackendInterface*>& InParsedNodes  )
	{
		FDerivedDataBackendInterface* ParsedNode = NULL;
		FString Entry;
		if( GConfig->GetString( IniSection, NodeName, Entry, IniFilename ) )
		{
			// Trim whitespace at the beginning.
			Entry.TrimStartInline();
			// Remove brackets.
			Entry.RemoveFromStart(TEXT("("));
			Entry.RemoveFromEnd(TEXT(")"));

			FString	NodeType;
			if( FParse::Value( *Entry, TEXT("Type="), NodeType ) )
			{
				if( NodeType == TEXT("FileSystem") )
				{
					ParsedNode = ParseDataCache( NodeName, *Entry );
				}
				else if( NodeType == TEXT("Boot") )
				{
					if( BootCache == NULL )
					{
						BootCache = ParseBootCache( NodeName, *Entry, BootCacheFilename );
						ParsedNode = BootCache;
					}
					else
					{
						UE_LOG( LogDerivedDataCache, Warning, TEXT("FDerivedDataBackendGraph:  Unable to create %s Boot cache because only one Boot cache node is supported."), *NodeName );
					}
				}
				else if( NodeType == TEXT("Memory") )
				{
					ParsedNode = ParseMemoryCache( NodeName, *Entry );
				}
				else if( NodeType == TEXT("Hierarchical") )
				{
					ParsedNode = ParseHierarchicalCache( NodeName, *Entry, IniFilename, IniSection, InParsedNodes );
				}
				else if( NodeType == TEXT("KeyLength") )
				{
					if( KeyLengthWrapper == NULL )
					{
						KeyLengthWrapper = ParseKeyLength( NodeName, *Entry, IniFilename, IniSection, InParsedNodes );
						ParsedNode = KeyLengthWrapper;
					}
					else
					{
						UE_LOG( LogDerivedDataCache, Warning, TEXT("FDerivedDataBackendGraph:  Unable to create %s KeyLengthWrapper because only one KeyLength node is supported."), *NodeName );
					}
				}
				else if( NodeType == TEXT("AsyncPut") )
				{
					if( AsyncPutWrapper == NULL )
					{
						AsyncPutWrapper = ParseAsyncPut( NodeName, *Entry, IniFilename, IniSection, InParsedNodes );
						ParsedNode = AsyncPutWrapper;
					}
					else
					{
						UE_LOG( LogDerivedDataCache, Warning, TEXT("FDerivedDataBackendGraph:  Unable to create %s AsyncPutWrapper because only one AsyncPutWrapper node is supported."), *NodeName );
					}
				}
				else if( NodeType == TEXT("Verify") )
				{
					ParsedNode = ParseVerify( NodeName, *Entry, IniFilename, IniSection, InParsedNodes );
				}
				else if( NodeType == TEXT("ReadPak") )
				{
					ParsedNode = ParsePak( NodeName, *Entry, false );
				}
				else if( NodeType == TEXT("WritePak") )
				{
					ParsedNode = ParsePak( NodeName, *Entry, true );
				}
			}
		}

		if( ParsedNode )
		{
			// Store this node so that we don't require any order of adding backend nodes
			InParsedNodes.Add( NodeName, ParsedNode );
			// Keep references to all created nodes.
			CreatedBackends.AddUnique( ParsedNode );
		}

		return ParsedNode;
	}

	/**
	 * Creates Read/write Pak file interface from ini settings
	 *
	 * @param NodeName Node name
	 * @param Entry Node definition
	 * @param bWriting true to create pak interface for writing
	 * @return Pak file data backend interface instance or NULL if unsuccessful
	 */
	FDerivedDataBackendInterface* ParsePak( const TCHAR* NodeName, const TCHAR* Entry, const bool bWriting )
	{
		FDerivedDataBackendInterface* PakNode = NULL;
		FString PakFilename;
		FParse::Value( Entry, TEXT("Filename="), PakFilename );
		bool bCompressed = GetParsedBool(Entry, TEXT("Compressed="));

		if( !PakFilename.Len() )
		{
			UE_LOG( LogDerivedDataCache, Log, TEXT("FDerivedDataBackendGraph:  %s pak cache Filename not found in *engine.ini, will not use a pak cache."), NodeName );
		}
		else
		{
			// now add the pak read cache (if any) to the front of the cache hierarchy
			if ( bWriting )
			{
				FGuid Temp = FGuid::NewGuid();
				ReadPakFilename = PakFilename;
				WritePakFilename = PakFilename + TEXT(".") + Temp.ToString();
				WritePakCache = bCompressed? new FCompressedPakFileDerivedDataBackend( *WritePakFilename, true ) : new FPakFileDerivedDataBackend( *WritePakFilename, true );
				PakNode = WritePakCache;
			}
			else
			{
				bool bReadPak = FPlatformFileManager::Get().GetPlatformFile().FileExists( *PakFilename );
				if( bReadPak )
				{
					FPakFileDerivedDataBackend* ReadPak = bCompressed? new FCompressedPakFileDerivedDataBackend( *PakFilename, false ) : new FPakFileDerivedDataBackend( *PakFilename, false );
					ReadPakFilename = PakFilename;
					PakNode = ReadPak;
					ReadPakCache.Add(ReadPak);
				}
				else
				{
					UE_LOG( LogDerivedDataCache, Log, TEXT("FDerivedDataBackendGraph:  %s pak cache file %s not found, will not use a pak cache."), NodeName, *PakFilename );
				}
			}
		}
		return PakNode;
	}

	/**
	 * Creates Verify wrapper interface from ini settings.
	 *
	 * @param NodeName Node name.
	 * @param Entry Node definition.
	 * @param IniFilename ini filename.
	 * @param IniSection ini section containing graph definition
	 * @param InParsedNodes map of nodes and their names which have already been parsed
	 * @return Verify wrapper backend interface instance or NULL if unsuccessful
	 */
	FDerivedDataBackendInterface* ParseVerify( const TCHAR* NodeName, const TCHAR* Entry, const FString& IniFilename, const TCHAR* IniSection, TMap<FString, FDerivedDataBackendInterface*>& InParsedNodes )
	{
		FDerivedDataBackendInterface* InnerNode = NULL;
		FString InnerName;
		if( FParse::Value( Entry, TEXT("Inner="), InnerName ) )
		{
			FDerivedDataBackendInterface** ParsedInnerNode = InParsedNodes.Find( InnerName );
			if( ParsedInnerNode )
			{
				UE_LOG( LogDerivedDataCache, Warning, TEXT("Inner node %s for Verify node %s already exists. Nodes can only be used once."), *InnerName, NodeName );
				return NULL;
			}
			else
			{
				InnerNode = ParseNode( *InnerName, IniFilename, IniSection, InParsedNodes );
			}
		}

		FDerivedDataBackendInterface* VerifyNode = NULL;
		if( InnerNode )
		{
			IFileManager::Get().DeleteDirectory(*(FPaths::ProjectSavedDir() / TEXT("VerifyDDC/")), false, true);

			const bool bFix = GetParsedBool( Entry, TEXT("Fix=") );
			VerifyNode = new FDerivedDataBackendVerifyWrapper( InnerNode, bFix );
		}
		else
		{
			UE_LOG( LogDerivedDataCache, Warning, TEXT("Unable to find inner node %s for Verify node %s. Verify node will not be created."), *InnerName, NodeName );
		}

		return VerifyNode;
	}

	/**
	 * Creates AsyncPut wrapper interface from ini settings.
	 *
	 * @param NodeName Node name.
	 * @param Entry Node definition.
	 * @param IniFilename ini filename.
	 * @param IniSection ini section containing graph definition
	 * @param InParsedNodes map of nodes and their names which have already been parsed
	 * @return AsyncPut wrapper backend interface instance or NULL if unsuccessfull
	 */
	FDerivedDataBackendInterface* ParseAsyncPut( const TCHAR* NodeName, const TCHAR* Entry, const FString& IniFilename, const TCHAR* IniSection, TMap<FString, FDerivedDataBackendInterface*>& InParsedNodes )
	{
		FDerivedDataBackendInterface* InnerNode = NULL;
		FString InnerName;
		if( FParse::Value( Entry, TEXT("Inner="), InnerName ) )
		{
			FDerivedDataBackendInterface** ParsedInnerNode = InParsedNodes.Find( InnerName );
			if( ParsedInnerNode )
			{
				UE_LOG( LogDerivedDataCache, Warning, TEXT("Inner node %s for AsyncPut node %s already exists. Nodes can only be used once."), *InnerName, NodeName );
				return NULL;
			}
			else
			{
				InnerNode = ParseNode( *InnerName, IniFilename, IniSection, InParsedNodes );
			}
		}

		FDerivedDataBackendInterface* AsyncNode = NULL;
		if( InnerNode )
		{
			AsyncNode = new FDerivedDataBackendAsyncPutWrapper( InnerNode, true );
		}
		else
		{
			UE_LOG( LogDerivedDataCache, Warning, TEXT("Unable to find inner node %s for AsyncPut node %s. AsyncPut node will not be created."), *InnerName, NodeName );
		}

		return AsyncNode;
	}

	/**
	 * Creates KeyLength wrapper interface from ini settings.
	 *
	 * @param NodeName Node name.
	 * @param Entry Node definition.
	 * @param IniFilename ini filename.
	 * @param IniSection ini section containing graph definition
	 * @param InParsedNodes map of nodes and their names which have already been parsed
	 * @return KeyLength wrapper backend interface instance or NULL if unsuccessfull
	 */
	FDerivedDataBackendInterface* ParseKeyLength( const TCHAR* NodeName, const TCHAR* Entry, const FString& IniFilename, const TCHAR* IniSection, TMap<FString, FDerivedDataBackendInterface*>& InParsedNodes )
	{
		FDerivedDataBackendInterface* InnerNode = NULL;
		FString InnerName;
		if( FParse::Value( Entry, TEXT("Inner="), InnerName ) )
		{
			FDerivedDataBackendInterface** ParsedInnerNode = InParsedNodes.Find( InnerName );
			if( ParsedInnerNode )
			{
				UE_LOG( LogDerivedDataCache, Warning, TEXT("Inner node %s for KeyLength node %s already exists. Nodes can only be used once."), *InnerName, NodeName );
				return NULL;
			}
			else
			{
				InnerNode = ParseNode( *InnerName, IniFilename, IniSection, InParsedNodes );
			}
		}

		FDerivedDataBackendInterface* KeyLengthNode = NULL;
		if( InnerNode )
		{
			int32 KeyLength = MAX_BACKEND_KEY_LENGTH;
			FParse::Value( Entry, TEXT("Length="), KeyLength );
			KeyLength = FMath::Clamp( KeyLength, 0, MAX_BACKEND_KEY_LENGTH );

			KeyLengthNode = new FDerivedDataLimitKeyLengthWrapper( InnerNode, KeyLength );
		}
		else
		{
			UE_LOG( LogDerivedDataCache, Warning, TEXT("Unable to find inner node %s for KeyLength node %s. KeyLength node will not be created."), *InnerName, NodeName );
		}

		return KeyLengthNode;
	}

	/**
	 * Creates Hierarchical interface from ini settings.
	 *
	 * @param NodeName Node name.
	 * @param Entry Node definition.
	 * @param IniFilename ini filename.
	 * @param IniSection ini section containing graph definition
	 * @param InParsedNodes map of nodes and their names which have already been parsed
	 * @return Hierarchical backend interface instance or NULL if unsuccessfull
	 */
	FDerivedDataBackendInterface* ParseHierarchicalCache( const TCHAR* NodeName, const TCHAR* Entry, const FString& IniFilename, const TCHAR* IniSection, TMap<FString, FDerivedDataBackendInterface*>& InParsedNodes )
	{
		const TCHAR* InnerMatch = TEXT("Inner=");
		const int32 InnerMatchLength = FCString::Strlen( InnerMatch );

		TArray< FDerivedDataBackendInterface* > InnerNodes;
		FString InnerName;
		while ( FParse::Value( Entry, InnerMatch, InnerName ) )
		{
			// Check if the child has already been parsed
			FDerivedDataBackendInterface** ParsedInnerNode = InParsedNodes.Find( InnerName );
			if( ParsedInnerNode )
			{
				UE_LOG( LogDerivedDataCache, Warning, TEXT("Inner node %s for hierarchical node %s already exists. Nodes can only be used once."), *InnerName, NodeName );
			}
			else
			{
				FDerivedDataBackendInterface* InnerNode = ParseNode( *InnerName, IniFilename, IniSection, InParsedNodes );
				if( InnerNode )
				{
					InnerNodes.Add( InnerNode );
				}
				else
				{
					UE_LOG( LogDerivedDataCache, Log, TEXT("Unable to find inner node %s for hierarchical cache %s."), *InnerName, NodeName );
				}
			}

			// Move the Entry pointer forward so that we can find more children
			Entry = FCString::Strifind( Entry, InnerMatch );
			Entry += InnerMatchLength;
		}

		FDerivedDataBackendInterface* Hierarchy = NULL;
		if( InnerNodes.Num() > 1 )
		{
			FHierarchicalDerivedDataBackend* HierarchyBackend = new FHierarchicalDerivedDataBackend( InnerNodes );
			Hierarchy = HierarchyBackend;
			if (HierarchicalWrapper == NULL)
			{
				HierarchicalWrapper = HierarchyBackend;
			}
		}
		else if ( InnerNodes.Num() == 1 )
		{
			Hierarchy = InnerNodes[ 0 ];
			InnerNodes.Empty();
		}
		else
		{
			UE_LOG( LogDerivedDataCache, Warning, TEXT("Hierarchical cache %s has no inner backends and will not be created."), NodeName );
		}

		return Hierarchy;
	}

	/**
	 * Creates Filesystem data cache interface from ini settings.
	 *
	 * @param NodeName Node name.
	 * @param Entry Node definition.
	 * @return Filesystem data cache backend interface instance or NULL if unsuccessfull
	 */
	FDerivedDataBackendInterface* ParseDataCache( const TCHAR* NodeName, const TCHAR* Entry )
	{
		FDerivedDataBackendInterface* DataCache = NULL;

		// Parse Path by default, it may be overwriten by EnvPathOverride
		FString Path;
		FParse::Value( Entry, TEXT("Path="), Path );

		// Check the EnvPathOverride environment variable to allow persistent overriding of data cache path, eg for offsite workers.
		FString EnvPathOverride;
		if( FParse::Value( Entry, TEXT("EnvPathOverride="), EnvPathOverride ) )
		{
			TCHAR FilesystemCachePathEnv[256];
			FPlatformMisc::GetEnvironmentVariable( *EnvPathOverride, FilesystemCachePathEnv, ARRAY_COUNT(FilesystemCachePathEnv) );
			if( FilesystemCachePathEnv[0] )
			{
				Path = FilesystemCachePathEnv;
				UE_LOG( LogDerivedDataCache, Log, TEXT("Found environment variable %s=%s"), *EnvPathOverride, *Path );
			}
		}
		// Check if the Path is a real path or a special keyword
		if (FEngineBuildSettings::IsInternalBuild())
		{
			auto DDCUtils = FModuleManager::LoadModulePtr< IDDCUtilsModuleInterface >("DDCUtils");
			if (DDCUtils)
			{
				FString PathFromName = DDCUtils->GetSharedCachePath(Path);
				if (!PathFromName.IsEmpty())
				{
					Path = PathFromName;
				}
			}
		}
		else if (Path.StartsWith(TEXT("?")))
		{
			Path = TEXT("");
		}

		// Allow the user to override it from the editor
		FString EditorOverrideSetting;
		if(FParse::Value(Entry, TEXT("EditorOverrideSetting="), EditorOverrideSetting))
		{
			FString Setting = GConfig->GetStr(TEXT("/Script/UnrealEd.EditorSettings"), *EditorOverrideSetting, GEditorSettingsIni);
			if(Setting.Len() > 0)
			{
				FString SettingPath;
				if(FParse::Value(*Setting, TEXT("Path="), SettingPath))
				{
					SettingPath = SettingPath.TrimQuotes();
					if(SettingPath.Len() > 0)
					{
						Path = SettingPath;
					}
				}
			}
		}

		if( !Path.Len() )
		{
			UE_LOG( LogDerivedDataCache, Log, TEXT("%s data cache path not found in *engine.ini, will not use an %s cache."), NodeName, NodeName );
		}
		else if( Path == TEXT("None") )
		{
			UE_LOG( LogDerivedDataCache, Log, TEXT("Disabling %s data cache - path set to 'None'."), NodeName );
		}
		else
		{
			const bool bReadOnly = GetParsedBool( Entry, TEXT("ReadOnly=") );
			const bool bClean = GetParsedBool( Entry, TEXT("Clean=") );
			const bool bFlush = GetParsedBool( Entry, TEXT("Flush=") );
			const bool bTouch = GetParsedBool( Entry, TEXT("Touch=") );
			const bool bPurgeTransient = GetParsedBool( Entry, TEXT("PurgeTransient=") );

			bool bDeleteUnused = true; // On by default
			FParse::Bool( Entry, TEXT("DeleteUnused="), bDeleteUnused );
			int32 UnusedFileAge = 17;
			FParse::Value( Entry, TEXT("UnusedFileAge="), UnusedFileAge );
			int32 MaxFoldersToClean = -1;
			FParse::Value( Entry, TEXT("FoldersToClean="), MaxFoldersToClean );
			int32 MaxFileChecksPerSec = -1;
			FParse::Value( Entry, TEXT("MaxFileChecksPerSec="), MaxFileChecksPerSec );

			if( bFlush )
			{
				IFileManager::Get().DeleteDirectory( *(Path / TEXT("")), false, true );
			}
			else if( bClean )
			{
				DeleteOldFiles( *Path );
			}

			FDerivedDataBackendInterface* InnerFileSystem = NULL;

			// Don't create the file system if shared data cache directory is not mounted
			if( FCString::Strcmp(NodeName, TEXT("Shared")) != 0 || IFileManager::Get().DirectoryExists(*Path) )
			{
				InnerFileSystem = CreateFileSystemDerivedDataBackend( *Path, bReadOnly, bTouch, bPurgeTransient, bDeleteUnused, UnusedFileAge, MaxFoldersToClean, MaxFileChecksPerSec);
			}

			if( InnerFileSystem )
			{
				DataCache = new FDerivedDataBackendCorruptionWrapper( InnerFileSystem );
				UE_LOG( LogDerivedDataCache, Log, TEXT("Using %s data cache path %s: %s"), NodeName, *Path, bReadOnly ? TEXT("ReadOnly") : TEXT("Writable") );
				Directories.AddUnique(Path);
			}
			else
			{
				UE_LOG( LogDerivedDataCache, Warning, TEXT("%s data cache path was not usable, will not use it."), NodeName );
			}
		}

		return DataCache;
	}

	/**
	 * Creates Boot data cache interface from ini settings.
	 *
	 * @param NodeName Node name.
	 * @param Entry Node definition.
	 * @param OutFilename filename specified for the cache
	 * @return Boot data cache backend interface instance or NULL if unsuccessfull
	 */
	FMemoryDerivedDataBackend* ParseBootCache( const TCHAR* NodeName, const TCHAR* Entry, FString& OutFilename )
	{
		FMemoryDerivedDataBackend* Cache = NULL;
		FString Filename;
		int64 MaxCacheSize = -1; // in MB
		const int64 MaxSupportedCacheSize = 2048; // 2GB

		FParse::Value( Entry, TEXT("MaxCacheSize="), MaxCacheSize);
		FParse::Value( Entry, TEXT("Filename="), Filename );
		if ( !Filename.Len() )
		{
			UE_LOG( LogDerivedDataCache, Warning, TEXT("FDerivedDataBackendGraph:  %s filename not found in *engine.ini, will not use %s cache."), NodeName, NodeName );
		}
		else
		{
			// make sure MaxCacheSize does not exceed 2GB
			MaxCacheSize = FMath::Min(MaxCacheSize, MaxSupportedCacheSize);

			UE_LOG( LogDerivedDataCache, Display, TEXT("Max Cache Size: %d MB"), MaxCacheSize);
			Cache = new FMemoryDerivedDataBackend(MaxCacheSize * 1024 * 1024);

			if( Cache && Filename.Len() )
			{
				OutFilename = Filename;

				if (MaxCacheSize > 0 && IFileManager::Get().FileSize(*Filename) >= (MaxCacheSize * 1024 * 1024))
				{
					UE_LOG( LogDerivedDataCache, Warning, TEXT("FDerivedDataBackendGraph:  %s filename exceeds max size."), NodeName );
					return Cache;
				}

				if (IFileManager::Get().FileSize(*Filename) < 0)
				{
					UE_LOG( LogDerivedDataCache, Display, TEXT("Starting with empty %s cache"), NodeName );
				}
				else if( Cache->LoadCache( *Filename ) )
				{
					UE_LOG( LogDerivedDataCache, Display, TEXT("Loaded %s cache: %s"), NodeName, *Filename );
				}
				else
				{
					UE_LOG( LogDerivedDataCache, Warning, TEXT("Could not load %s cache: %s"), NodeName, *Filename );
				}
			}
		}
		return Cache;
	}

	/**
	 * Creates Memory data cache interface from ini settings.
	 *
	 * @param NodeName Node name.
	 * @param Entry Node definition.
	 * @return Memory data cache backend interface instance or NULL if unsuccessfull
	 */
	FMemoryDerivedDataBackend* ParseMemoryCache( const TCHAR* NodeName, const TCHAR* Entry )
	{
		FMemoryDerivedDataBackend* Cache = NULL;
		FString Filename;

		FParse::Value( Entry, TEXT("Filename="), Filename );
		Cache = new FMemoryDerivedDataBackend;
		if( Cache && Filename.Len() )
		{
			if( Cache->LoadCache( *Filename ) )
			{
				UE_LOG( LogDerivedDataCache, Display, TEXT("Loaded %s cache: %s"), NodeName, *Filename );
			}
			else
			{
				UE_LOG( LogDerivedDataCache, Warning, TEXT("Could not load %s cache: %s"), NodeName, *Filename );
			}
		}

		return Cache;
	}

	virtual ~FDerivedDataBackendGraph()
	{
		RootCache = NULL;
		DestroyCreatedBackends();
	}

	FDerivedDataBackendInterface& GetRoot() override
	{
		check(RootCache);
		return *RootCache;
	}

	virtual void NotifyBootComplete() override
	{
		check(RootCache);
		if (BootCache)
		{
			if( !FParse::Param( FCommandLine::Get(), TEXT("DDCNOSAVEBOOT") ) && !FParse::Param( FCommandLine::Get(), TEXT("Multiprocess") ) )
			{
				BootCache->SaveCache(*BootCacheFilename);
			}
			BootCache->Disable();
		}
	}

	virtual void WaitForQuiescence(bool bShutdown) override
	{
		double StartTime = FPlatformTime::Seconds();
		double LastPrint = StartTime;
		while (AsyncCompletionCounter.GetValue())
		{
			check(AsyncCompletionCounter.GetValue() >= 0);
			FPlatformProcess::Sleep(1.0f);
			if (FPlatformTime::Seconds() - LastPrint > 5.0)
			{
				UE_LOG(LogDerivedDataCache, Log, TEXT("Waited %ds for derived data cache to finish..."), int32(FPlatformTime::Seconds() - StartTime));
				LastPrint = FPlatformTime::Seconds();
			}
		}
		if (bShutdown)
		{
			FString MergePaks;
			if(WritePakCache && WritePakCache->IsWritable() && FParse::Value(FCommandLine::Get(), TEXT("MergePaks="), MergePaks))
			{
				TArray<FString> MergePakList;
				MergePaks.FString::ParseIntoArray(MergePakList, TEXT("+"));

				for(const FString& MergePakName : MergePakList)
				{
					FPakFileDerivedDataBackend ReadPak(*FPaths::Combine(*FPaths::GetPath(WritePakFilename), *MergePakName), false);
					WritePakCache->MergeCache(&ReadPak);
				}
			}
			for (int32 ReadPakIndex = 0; ReadPakIndex < ReadPakCache.Num(); ReadPakIndex++)
			{
				ReadPakCache[ReadPakIndex]->Close();
			}
			if (WritePakCache && WritePakCache->IsWritable())
			{
				WritePakCache->Close();
				if (!FPlatformFileManager::Get().GetPlatformFile().FileExists(*WritePakFilename))
				{
					UE_LOG(LogDerivedDataCache, Error, TEXT("Pak file %s was not produced?"), *WritePakFilename);
				}
				else
				{
					if (FPlatformFileManager::Get().GetPlatformFile().FileExists(*ReadPakFilename))
					{
						FPlatformFileManager::Get().GetPlatformFile().SetReadOnly(*ReadPakFilename, false);
						if (!FPlatformFileManager::Get().GetPlatformFile().DeleteFile(*ReadPakFilename))
						{
							UE_LOG(LogDerivedDataCache, Error, TEXT("Could not delete the pak file %s to overwrite it with a new one."), *ReadPakFilename);
						}
					}
					if (!FPakFileDerivedDataBackend::SortAndCopy(WritePakFilename, ReadPakFilename))
					{
						UE_LOG(LogDerivedDataCache, Error, TEXT("Couldn't sort pak file (%s)"), *WritePakFilename);
					}
					else if (!IFileManager::Get().Delete(*WritePakFilename))
					{
						UE_LOG(LogDerivedDataCache, Error, TEXT("Couldn't delete pak file (%s)"), *WritePakFilename);
					}
					else
					{
						UE_LOG(LogDerivedDataCache, Display, TEXT("Sucessfully wrote %s."), *ReadPakFilename);
					}
				}
			}
		}
	}

	virtual void AddToAsyncCompletionCounter(int32 Addend) override
	{
		AsyncCompletionCounter.Add(Addend);
		check(AsyncCompletionCounter.GetValue() >= 0);
	}

	virtual void GetDirectories(TArray<FString>& OutResults) override
	{
		OutResults = Directories;
	}

	static FORCEINLINE FDerivedDataBackendGraph& Get()
	{
		static FDerivedDataBackendGraph SingletonInstance;
		return SingletonInstance;
	}

	virtual FDerivedDataBackendInterface* MountPakFile(const TCHAR* PakFilename) override
	{
		// Assumptions: there's at least one read-only pak backend in the hierarchy
		// and its parent is a hierarchical backend.
		FPakFileDerivedDataBackend* ReadPak = NULL;
		if (HierarchicalWrapper && FPlatformFileManager::Get().GetPlatformFile().FileExists(PakFilename))
		{
			ReadPak = new FPakFileDerivedDataBackend(PakFilename, false);

			HierarchicalWrapper->AddInnerBackend(ReadPak);
			CreatedBackends.Add(ReadPak);
			ReadPakCache.Add(ReadPak);
		}
		else
		{
			UE_LOG(LogDerivedDataCache, Warning, TEXT("Failed to add %s read-only pak DDC backend. Make sure it exists and there's at least one hierarchical backend in the cache tree."), PakFilename);
		}

		return ReadPak;
	}

	virtual bool UnmountPakFile(const TCHAR* PakFilename) override
	{
		for (int PakIndex = 0; PakIndex < ReadPakCache.Num(); ++PakIndex)
		{
			FPakFileDerivedDataBackend* ReadPak = ReadPakCache[PakIndex];
			if (ReadPak->GetFilename() == PakFilename)
			{
				check(HierarchicalWrapper);

				// Wait until all async requests are complete.
				WaitForQuiescence(false);

				HierarchicalWrapper->RemoveInnerBackend(ReadPak);
				ReadPakCache.RemoveAt(PakIndex);
				CreatedBackends.Remove(ReadPak);
				ReadPak->Close();
				delete ReadPak;
				return true;
			}
		}
		return false;
	}

	virtual void GatherUsageStats(TMap<FString, FDerivedDataCacheUsageStats>& UsageStats) override
	{
		if (RootCache)
		{
			RootCache->GatherUsageStats(UsageStats, TEXT(" 0"));
		}
	}

private:

	/** Delete the old files in a directory **/
	void DeleteOldFiles(const TCHAR* Directory)
	{
		float MinimumDaysToKeepFile = 7;
		GConfig->GetFloat( *GraphName, TEXT("MinimumDaysToKeepFile"), MinimumDaysToKeepFile, GEngineIni );
		check(MinimumDaysToKeepFile > 0.0f); // sanity
		//@todo 
	}

	/** Delete all created backends in the reversed order they were created. */
	void DestroyCreatedBackends()
	{
		for (int32 BackendIndex = CreatedBackends.Num() - 1; BackendIndex >= 0; --BackendIndex)
		{
			delete CreatedBackends[BackendIndex];
		}
		CreatedBackends.Empty();
	}

	/** MountPak console command handler. */
	void UnmountPakCommandHandler(const TArray<FString>& Args)
	{
		if (Args.Num() < 1)
		{
			UE_LOG(LogDerivedDataCache, Log, TEXT("Usage: DDC.MountPak PakFilename"));
			return;
		}
		UnmountPakFile(*Args[0]);
	}

	/** UnmountPak console command handler. */
	void MountPakCommandHandler(const TArray<FString>& Args)
	{
		if (Args.Num() < 1)
		{
			UE_LOG(LogDerivedDataCache, Log, TEXT("Usage: DDC.UnmountPak PakFilename"));
			return;
		}
		MountPakFile(*Args[0]);
	}

	FThreadSafeCounter								AsyncCompletionCounter;
	FString											GraphName;
	FString											BootCacheFilename;
	FString											ReadPakFilename;
	FString											WritePakFilename;

	/** Root of the graph */
	FDerivedDataBackendInterface*					RootCache;

	/** References to all created backed interfaces */
	TArray< FDerivedDataBackendInterface* > CreatedBackends;

	/** Instances of backend interfaces which exist in only one copy */
	FMemoryDerivedDataBackend*		BootCache;
	FPakFileDerivedDataBackend*		WritePakCache;
	FDerivedDataBackendInterface*	AsyncPutWrapper;
	FDerivedDataBackendInterface*	KeyLengthWrapper;
	FHierarchicalDerivedDataBackend* HierarchicalWrapper;
	/** Support for multiple read only pak files. */
	TArray<FPakFileDerivedDataBackend*>		ReadPakCache;

	/** List of directories used by the DDC */
	TArray<FString> Directories;

	/** MountPak console command */
	FAutoConsoleCommand MountPakCommand;
	/** UnmountPak console command */
	FAutoConsoleCommand UnountPakCommand;
};

FDerivedDataBackend& FDerivedDataBackend::Get()
{
	return FDerivedDataBackendGraph::Get();
}

#undef LOCTEXT_NAMESPACE
