// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================================
	MacPlatformProcess.h: Mac platform Process functions
==============================================================================================*/

#pragma once

#include "GenericPlatform/GenericPlatformProcess.h"
#include <sys/sysctl.h>

/** Mac implementation of the process handle. */
struct FProcHandle : public TProcHandle<void*, nullptr>
{
public:
	/** Default constructor. */
	FORCEINLINE FProcHandle()
		: TProcHandle()
	{}

	/** Initialization constructor. */
	FORCEINLINE explicit FProcHandle( HandleType Other )
		: TProcHandle( Other )
	{}
};

/**
 * Mac implementation of the Process OS functions
 **/
struct CORE_API FMacPlatformProcess : public FGenericPlatformProcess
{
	struct FProcEnumInfo;

	/**
	 * Process enumerator.
	 */
	class CORE_API FProcEnumerator
	{
	public:
		// Constructor
		FProcEnumerator();
		FProcEnumerator(const FProcEnumerator&) = delete;
		FProcEnumerator& operator=(const FProcEnumerator&) = delete;

		// Destructor
		~FProcEnumerator();

		// Gets current process enumerator info.
		FProcEnumInfo GetCurrent() const;

		/**
		 * Moves current to the next process.
		 *
		 * @returns True if succeeded. False otherwise.
		 */
		bool MoveNext();

	private:
		// Array of process info structures.
		struct kinfo_proc* Processes;

		// Count of processes.
		uint32 ProcCount;

		// Current process index.
		uint32 CurrentProcIndex;
	};

	/**
	 * Process enumeration info structure.
	 */
	struct CORE_API FProcEnumInfo
	{
		friend FProcEnumInfo FMacPlatformProcess::FProcEnumerator::GetCurrent() const;

		// Gets process PID.
		uint32 GetPID() const;

		// Gets parent process PID.
		uint32 GetParentPID() const;

		// Gets process name. I.e. exec name.
		FString GetName() const;

		// Gets process full image path. I.e. full path of the exec file.
		FString GetFullPath() const;

	private:
		// Private constructor.
		FProcEnumInfo(struct kinfo_proc InProcInfo);

		// Process info struct.
		struct kinfo_proc ProcInfo;
	};

	static void* GetDllHandle( const TCHAR* Filename );
	static void FreeDllHandle( void* DllHandle );
	static void* GetDllExport( void* DllHandle, const TCHAR* ProcName );
	static int32 GetDllApiVersion( const TCHAR* Filename );
	static void CleanFileCache();
	static uint32 GetCurrentProcessId();
	static const TCHAR* BaseDir();
	static const TCHAR* UserDir();
	static const TCHAR* UserTempDir();
	static const TCHAR* UserSettingsDir();
	static const TCHAR* ApplicationSettingsDir();
	static const TCHAR* ComputerName();
	static const TCHAR* UserName(bool bOnlyAlphaNumeric = true);
	static void SetCurrentWorkingDirectoryToBaseDir();
	static FString GetCurrentWorkingDirectory();
	static const TCHAR* ExecutableName(bool bRemoveExtension = true);
	static FString GenerateApplicationPath( const FString& AppName, EBuildConfigurations::Type BuildConfiguration);
	static const TCHAR* GetModuleExtension();
	static const TCHAR* GetBinariesSubdirectory();
	static const FString GetModulesDirectory();
	static bool CanLaunchURL(const TCHAR* URL);
	static void LaunchURL(const TCHAR* URL, const TCHAR* Parms, FString* Error);
	static FString GetGameBundleId();
	static FProcHandle CreateProc( const TCHAR* URL, const TCHAR* Parms, bool bLaunchDetached, bool bLaunchHidden, bool bLaunchReallyHidden, uint32* OutProcessID, int32 PriorityModifier, const TCHAR* OptionalWorkingDirectory, void* PipeWriteChild, void * PipeReadChild = nullptr);
	static FProcHandle OpenProcess(uint32 ProcessID);
	static bool IsProcRunning( FProcHandle & ProcessHandle );
	static void WaitForProc( FProcHandle & ProcessHandle );
	static void CloseProc( FProcHandle & ProcessHandle );
	static void TerminateProc( FProcHandle & ProcessHandle, bool KillTree = false );
	static bool GetProcReturnCode( FProcHandle & ProcHandle, int32* ReturnCode );
	static bool IsApplicationRunning( const TCHAR* ProcName );
	static bool IsApplicationRunning( uint32 ProcessId );
	static FString GetApplicationName( uint32 ProcessId );
	static bool IsSandboxedApplication();
	static void LaunchFileInDefaultExternalApplication( const TCHAR* FileName, const TCHAR* Parms = NULL, ELaunchVerb::Type Verb = ELaunchVerb::Open );
	static void ExploreFolder( const TCHAR* FilePath );
	static FRunnableThread* CreateRunnableThread();
	static void ClosePipe( void* ReadPipe, void* WritePipe );
	static bool CreatePipe( void*& ReadPipe, void*& WritePipe );
	static FString ReadPipe( void* ReadPipe );
	static bool ReadPipeToArray(void* ReadPipe, TArray<uint8> & Output);
	static bool WritePipe(void* WritePipe, const FString& Message, FString* OutWritten = nullptr);
	static bool ExecProcess(const TCHAR* URL, const TCHAR* Params, int32* OutReturnCode, FString* OutStdOut, FString* OutStdErr);
	static void SetThreadAffinityMask(uint64 AffinityMask);
	
	// Mac specific
	static const TCHAR* UserPreferencesDir();
	static const TCHAR* UserLogsDir();
};

typedef FMacPlatformProcess FPlatformProcess;