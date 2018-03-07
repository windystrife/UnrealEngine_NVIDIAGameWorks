// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/UnrealString.h"
#include "Templates/SharedPointer.h"

class FArchive;
struct FPDBCacheEntry;

enum EProcessorArchitecture
{
	PA_UNKNOWN,
	PA_ARM,
	PA_X86,
	PA_X64,
};

/** 
 * Details of a module from a crash dump
 */
class FCrashModuleInfo
{
public:
	FString Report;

	FString Name;
	FString Extension;
	uint64 BaseOfImage;
	uint32 SizeOfImage;
	uint16 Major;
	uint16 Minor;
	uint16 Patch;
	uint16 Revision;

	FCrashModuleInfo()
		: BaseOfImage( 0 )
		, SizeOfImage( 0 )
		, Major( 0 )
		, Minor( 0 )
		, Patch( 0 )
		, Revision( 0 )
	{
	}
};

/** 
 * Details about a thread from a crash dump
 */
class FCrashThreadInfo
{
public:
	FString Report;

	uint32 ThreadId;
	uint32 SuspendCount;

	TArray<uint64> CallStack;

	FCrashThreadInfo()
		: ThreadId( 0 )
		, SuspendCount( 0 )
	{
	}

	~FCrashThreadInfo()
	{
	}
};

/** 
 * Details about the exception in the crash dump
 */
class FCrashExceptionInfo
{
public:
	FString Report;

	uint32 ProcessId;
	uint32 ThreadId;
	uint32 Code;
	FString ExceptionString;

	TArray<FString> CallStackString;

	FCrashExceptionInfo()
		: ProcessId( 0 )
		, ThreadId( 0 )
		, Code( 0 )
	{
	}

	~FCrashExceptionInfo()
	{
	}
};

/** 
 * Details about the system the crash dump occurred on
 */
class FCrashSystemInfo
{
public:
	FString Report;

	EProcessorArchitecture ProcessorArchitecture;
	uint32 ProcessorCount;

	uint16 OSMajor;
	uint16 OSMinor;
	uint16 OSBuild;
	uint16 OSRevision;

	FCrashSystemInfo()
		: ProcessorArchitecture( PA_UNKNOWN )
		, ProcessorCount( 0 )
		, OSMajor( 0 )
		, OSMinor( 0 )
		, OSBuild( 0 )
		, OSRevision( 0 )
	{
	}
};

// #TODO 2015-07-24 Refactor
/** A platform independent representation of a crash */
class CRASHDEBUGHELPER_API FCrashInfo
{
public:
	enum
	{
		/** An invalid changelist, something went wrong. */
		INVALID_CHANGELIST = -1,
	};

	/** Report log. */
	FString Report;

	/** The depot name, indicate where the executables and symbols are stored. */
	FString DepotName;

	/** Product version, based on FEngineVersion. */
	FString EngineVersion;

	/** Build version string. */
	FString BuildVersion;

	/** CL built from. */
	int32 BuiltFromCL;

	/** The label the describes the executables and symbols. */
	FString LabelName;

	/** The network path where the executables are stored. */
	FString ExecutablesPath;

	/** The network path where the symbols are stored. */
	FString SymbolsPath;

	FString SourceFile;
	uint32 SourceLineNumber;
	TArray<FString> SourceContext;

	/** Only modules names, retrieved from the minidump file. */
	TArray<FString> ModuleNames;

	FCrashSystemInfo SystemInfo;
	FCrashExceptionInfo Exception;
	TArray<FCrashThreadInfo> Threads;
	TArray<FCrashModuleInfo> Modules;

	/** Shared pointer to the PDB Cache entry, if valid contains all information about synced PDBs. */
	TSharedPtr<FPDBCacheEntry> PDBCacheEntry;

	FString PlatformName;
	FString PlatformVariantName;

	/** If we are using a PDBCache, this is whether we should use a system-wide lock to access it. */
	bool bMutexPDBCache;

	/** If we are using a PDBCache, this is the name of the system-wide lock we should use to access it. */
	FString PDBCacheLockName;

	FCrashInfo()
		: BuiltFromCL( INVALID_CHANGELIST )
		, SourceLineNumber( 0 )
	{
	}

	~FCrashInfo()
	{
	}

	/** 
	 * Generate a report for the crash in the requested path
	 */
	void GenerateReport( const FString& DiagnosticsPath );

	/** 
	 * Handle logging
	 */
	void Log( FString Line );

private:
	/** 
	 * Convert the processor architecture to a human readable string
	 */
	const TCHAR* GetProcessorArchitecture( EProcessorArchitecture PA );

	/**
	 * Calculate the byte size of a UTF-8 string
	 */
	int64 StringSize( const ANSICHAR* Line );

	/** 
	* Write a line of UTF-8 to a file
	*/
	void WriteLine( FArchive* ReportFile, const ANSICHAR* Line = NULL );
};


/** Helper structure for tracking crash debug information */
struct FCrashDebugInfo
{
	/** The name of the crash dump file */
	FString CrashDumpName;
	/** The engine version of the crash dump build */
	int32 EngineVersion;
	/** The platform of the crash dump build */
	FString PlatformName;
	/** The source control label of the crash dump build */
	FString SourceControlLabel;
};

/** The public interface for the crash dump handler singleton. */
class CRASHDEBUGHELPER_API ICrashDebugHelper
{
public:
	/** Replaces %DEPOT_INDEX% with the command line DepotIndex in the specified path. */
	static void SetDepotIndex( FString& PathToChange );

protected:
	/**
	 *	Pattern to search in source control for the label.
	 *	This somehow works for older crashes, before 4.2 and for the newest one,
	 *	bur for the newest we also need to look for the executables on the network drive.
	 *	This may change in future.
	 */
	FString SourceControlBuildLabelPattern;
	
	/** Indicates that the crash handler is ready to do work */
	bool bInitialized;

public:
	/** A platform independent representation of a crash */
	FCrashInfo CrashInfo;
	
	/** Virtual destructor */
	virtual ~ICrashDebugHelper()
	{}

	/**
	 *	Initialize the helper
	 *
	 *	@return	bool		true if successful, false if not
	 */
	virtual bool Init();

	/**
	 *	Parse the given crash dump, determining EngineVersion of the build that produced it - if possible. 
	 *
	 *	@param	InCrashDumpName		The crash dump file to process
	 *	@param	OutCrashDebugInfo	The crash dump info extracted from the file
	 *
	 *	@return	bool				true if successful, false if not
	 *	
	 *	Only used by Mac, to be removed.
	 */
	virtual bool ParseCrashDump(const FString& InCrashDumpName, FCrashDebugInfo& OutCrashDebugInfo)
	{
		return false;
	}

	/**
	 *	Parse the given crash dump, and generate a report. 
	 *
	 *	@param	InCrashDumpName		The crash dump file to process
	 *
	 *	@return	bool				true if successful, false if not
	 */
	virtual bool CreateMinidumpDiagnosticReport( const FString& InCrashDumpName )
	{
		return false;
	}

	/**
	 * Sync the branch root relative file names to the requested label
	 *
	 *	@param	bOutPDBCacheEntryValid		Returns whether the PDB cache entry was found or created and whether it contains files.
	 *
	 *	@return	bool						true if successful, false if not
	 */
	virtual bool SyncModules(bool& bOutPDBCacheEntryValid);

	/**
	 *	Sync a single source file to the requested CL.
	 */
	virtual bool SyncSourceFile();

	/**
	 *	Extract lines from a source file, and add to the crash report.
	 */
	virtual void AddSourceToReport();

	/**
	 *	Extract annotated lines from a source file stored in Perforce, and add to the crash report.
	 */
	virtual bool AddAnnotatedSourceToReport();

protected:

	/** Finds the storage of the symbols and the executables for the specified changelist and the depot name, it can be Perforce, network drive or stored locally. */
	void FindSymbolsAndBinariesStorage();

	/**
	 *	Load the given ANSI text file to an array of strings - one FString per line of the file.
	 *	Intended for use in simple text parsing actions
	 *
	 *	@param	OutStrings			The array of FStrings to fill in
	 *
	 *	@return	bool				true if successful, false if not
	 */
	bool ReadSourceFile( TArray<FString>& OutStrings );

public:
	bool InitSourceControl(bool bShowLogin);
	void ShutdownSourceControl();
};
