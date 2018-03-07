// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "GenericPlatform/GenericPlatformProcess.h"
#include "WindowsSystemIncludes.h"

class FEvent;
class FRunnableThread;

/** Windows implementation of the process handle. */
struct FProcHandle : public TProcHandle<Windows::HANDLE, nullptr>
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
* Windows implementation of the Process OS functions.
**/
struct CORE_API FWindowsPlatformProcess
	: public FGenericPlatformProcess
{
	/**
	 * Windows representation of a interprocess semaphore
	 */
	struct FWindowsSemaphore : public FSemaphore
	{
		virtual void	Lock();
		virtual bool	TryLock(uint64 NanosecondsToWait);
		virtual void	Unlock();

		/** Returns the OS handle */
		Windows::HANDLE GetSemaphore() { return Semaphore; }

		/** Constructor */
		FWindowsSemaphore(const FString& InName, Windows::HANDLE InSemaphore);

		/** Destructor */
		virtual ~FWindowsSemaphore();

	protected:

		/** OS handle */
		Windows::HANDLE Semaphore;
	};

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
		// Process info structure for current process.
		Windows::PROCESSENTRY32* CurrentEntry;

		// Processes state snapshot handle.
		Windows::HANDLE SnapshotHandle;
	};

	/**
	 * Process enumeration info structure.
	 */
	struct CORE_API FProcEnumInfo
	{
		friend FProcEnumInfo FProcEnumerator::GetCurrent() const;
	public:
		// Destructor
		~FProcEnumInfo();

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
		FProcEnumInfo(const Windows::PROCESSENTRY32& InInfo);

		// Process info structure.
		Windows::PROCESSENTRY32* Info;
	};

public:

	// FGenericPlatformProcess interface

	static void* GetDllHandle( const TCHAR* Filename );
	static void FreeDllHandle( void* DllHandle );
	static void* GetDllExport( void* DllHandle, const TCHAR* ProcName );
	static int32 GetDllApiVersion( const TCHAR* Filename );
	static void AddDllDirectory(const TCHAR* Directory);
	static void PushDllDirectory(const TCHAR* Directory);
	static void PopDllDirectory(const TCHAR* Directory);
	static void CleanFileCache();
	static uint32 GetCurrentProcessId();
	static void SetThreadAffinityMask( uint64 AffinityMask );
	static const TCHAR* BaseDir();
	static const TCHAR* UserDir();
	static const TCHAR* UserTempDir();
	static const TCHAR* UserSettingsDir();
	static const TCHAR* ApplicationSettingsDir();
	static const TCHAR* ComputerName();
	static const TCHAR* UserName(bool bOnlyAlphaNumeric = true);
	static void SetCurrentWorkingDirectoryToBaseDir();
	static FString GetCurrentWorkingDirectory();
	static const FString ShaderWorkingDir();
	static const TCHAR* ExecutableName(bool bRemoveExtension = true);
	static FString GenerateApplicationPath( const FString& AppName, EBuildConfigurations::Type BuildConfiguration);
	static const TCHAR* GetModuleExtension();
	static const TCHAR* GetBinariesSubdirectory();
	static const FString GetModulesDirectory();
	static bool CanLaunchURL(const TCHAR* URL);
	static void LaunchURL( const TCHAR* URL, const TCHAR* Parms, FString* Error );
	static FProcHandle CreateProc( const TCHAR* URL, const TCHAR* Parms, bool bLaunchDetached, bool bLaunchHidden, bool bLaunchReallyHidden, uint32* OutProcessID, int32 PriorityModifier, const TCHAR* OptionalWorkingDirectory, void* PipeWriteChild, void * PipeReadChild = nullptr);
	static FProcHandle OpenProcess(uint32 ProcessID);
	static bool IsProcRunning( FProcHandle & ProcessHandle );
	static void WaitForProc( FProcHandle & ProcessHandle );
	static void CloseProc( FProcHandle & ProcessHandle );
	static void TerminateProc( FProcHandle & ProcessHandle, bool KillTree = false );
	static bool GetProcReturnCode( FProcHandle & ProcHandle, int32* ReturnCode );
	static bool GetApplicationMemoryUsage(uint32 ProcessId, SIZE_T* OutMemoryUsage);
	static bool IsApplicationRunning( uint32 ProcessId );
	static bool IsApplicationRunning( const TCHAR* ProcName );
	static FString GetApplicationName( uint32 ProcessId );	
	static bool ExecProcess( const TCHAR* URL, const TCHAR* Params, int32* OutReturnCode, FString* OutStdOut, FString* OutStdErr );
	static bool ExecElevatedProcess(const TCHAR* URL, const TCHAR* Params, int32* OutReturnCode);
	static void LaunchFileInDefaultExternalApplication( const TCHAR* FileName, const TCHAR* Parms = NULL, ELaunchVerb::Type Verb = ELaunchVerb::Open );
	static void ExploreFolder( const TCHAR* FilePath );
	static bool ResolveNetworkPath( FString InUNCPath, FString& OutPath ); 
	static void Sleep(float Seconds);
	static void SleepNoStats(float Seconds);
	static void SleepInfinite();
	static class FEvent* CreateSynchEvent(bool bIsManualReset = false);
	static class FRunnableThread* CreateRunnableThread();
	static void ClosePipe( void* ReadPipe, void* WritePipe );
	static bool CreatePipe( void*& ReadPipe, void*& WritePipe );
	static FString ReadPipe( void* ReadPipe );
	static bool ReadPipeToArray(void* ReadPipe, TArray<uint8> & Output);
	static bool WritePipe(void* WritePipe, const FString& Message, FString* OutWritten = nullptr);
	static FSemaphore* NewInterprocessSynchObject(const FString& Name, bool bCreate, uint32 MaxLocks = 1);
	static bool DeleteInterprocessSynchObject(FSemaphore * Object);
	static bool Daemonize();
protected:

	/**
	 * Reads from a collection of anonymous pipes.
	 *
	 * @param OutStrings Will hold the read data.
	 * @param InPipes The pipes to read from.
	 * @param PipeCount The number of pipes.
	 */
	static void ReadFromPipes(FString* OutStrings[], Windows::HANDLE InPipes[], int32 PipeCount);

private:

	/**
	 * Since Windows can only have one directory at a time,
	 * this stack is used to reset the previous DllDirectory.
	 */
	static TArray<FString> DllDirectoryStack;

	/**
	 * All the DLL directories we want to load from. 
	 */
	static TArray<FString> DllDirectories;

	/**
	 * Replacement implementation of the Win32 LoadLibrary function which searches the given list of directories for dependent imports, and attempts
	 * to load them from the correct location first. The normal LoadLibrary function (pre-Win8) only searches a limited list of locations. 
	 *
	 * @param FileName Path to the library to load
	 * @param SearchPaths Search directories to scan for imports
	 */
	static void* LoadLibraryWithSearchPaths(const FString& FileName, const TArray<FString>& SearchPaths);

	/**
	 * Resolve all the imports for the given library, searching through a set of directories.
	 *
	 * @param FileName Path to the library to load
	 * @param SearchPaths Search directories to scan for imports
	 * @param ImportFileNames Array which is filled with a list of the resolved imports found in the given search directories
	 * @param VisitedImportNames Array which stores a list of imports which have been checked
	 */
	static void ResolveImportsRecursive(const FString& FileName, const TArray<FString>& SearchPaths, TArray<FString>& ImportFileNames, TArray<FString>& VisitedImportNames);

	/**
	 * Resolve an individual import.
	 *
	 * @param ImportName Name of the imported module
	 * @param SearchPaths Search directories to scan for imports
	 * @param OutFileName On success, receives the path to the imported file
	 * @return true if an import was found.
	 */
	static bool ResolveImport(const FString& ImportName, const TArray<FString>& SearchPaths, FString& OutFileName);

	/**
	 * Reads a list of import names from a portable executable file.
	 *
	 * @param FileName Path to the library
	 * @param ImportNames Array to receive the list of imported PE file names
	 */
	static bool ReadLibraryImports(const TCHAR* FileName, TArray<FString>& ImportNames);

	/**
	 * Log diagnostic messages showing missing imports for module.
	 *
	 * @param FileName Path to the library to load
	 * @param SearchPaths Search directories to scan for imports
	 */
	static void LogImportDiagnostics(const FString& FileName, const TArray<FString>& SearchPaths);
};


typedef FWindowsPlatformProcess FPlatformProcess;
