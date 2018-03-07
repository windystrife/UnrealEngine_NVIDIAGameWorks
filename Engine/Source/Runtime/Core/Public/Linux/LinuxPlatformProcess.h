// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


/*=============================================================================================
	LinuxPlatformProcess.h: Linux platform Process functions
==============================================================================================*/

#pragma once

#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "Containers/UnrealString.h"
#include "GenericPlatform/GenericPlatformProcess.h"

class Error;

/** Wrapper around Linux pid_t. Should not be copyable as changes in the process state won't be properly propagated to all copies. */
struct FProcState
{
	/** Default constructor. */
	FORCEINLINE FProcState()
		:	ProcessId(0)
		,	bIsRunning(false)
		,	bHasBeenWaitedFor(false)
		,	ReturnCode(-1)
		,	bFireAndForget(false)
	{
	}

	/** Initialization constructor. */
	explicit FProcState(pid_t InProcessId, bool bInFireAndForget);

	/** Destructor. */
	~FProcState();

	/** Getter for process id */
	FORCEINLINE pid_t GetProcessId() const
	{
		return ProcessId;
	}

	/**
	 * Returns whether this process is running.
	 *
	 * @return true if running
	 */
	bool	IsRunning();

	/**
	 * Returns child's return code (only valid to call if not running)
	 *
	 * @param ReturnCode set to return code if not NULL (use the value only if method returned true)
	 *
	 * @return return whether we have the return code (we don't if it crashed)
	 */
	bool	GetReturnCode(int32* ReturnCodePtr);

	/**
	 * Waits for the process to end.
	 * Has a side effect (stores child's return code).
	 */
	void	Wait();

protected:  // the below is not a public API!

	// FProcState should not be copyable since it breeds problems (e.g. one copy could have wait()ed, but another would not know it)

	/** Copy constructor - should not be publicly accessible */
	FORCEINLINE FProcState(const FProcState& Other)
		:	ProcessId(Other.ProcessId)
		,	bIsRunning(Other.bIsRunning)  // assume it is
		,	bHasBeenWaitedFor(Other.bHasBeenWaitedFor)
		,	ReturnCode(Other.ReturnCode)
		,	bFireAndForget(Other.bFireAndForget)
	{
		checkf(false, TEXT("FProcState should not be copied"));
	}

	/** Assignment operator - should not be publicly accessible */
	FORCEINLINE FProcState& operator=(const FProcState& Other)
	{
		checkf(false, TEXT("FProcState should not be copied"));
		return *this;
	}

	friend struct FLinuxPlatformProcess;

	// -------------------------

	/** Process id */
	pid_t	ProcessId;

	/** Whether the process has finished or not (cached) */
	bool	bIsRunning;

	/** Whether the process's return code has been collected */
	bool	bHasBeenWaitedFor;

	/** Return code of the process (if negative, means that process did not finish gracefully but was killed/crashed*/
	int32	ReturnCode;

	/** Whether this child is fire-and-forget */
	bool	bFireAndForget;
};

/** FProcHandle can be copied (and thus passed by value). */
struct FProcHandle
{
	FProcState* 		ProcInfo;

	FProcHandle()
	:	ProcInfo(nullptr)
	{
	}

	FProcHandle(FProcState* InHandle)
	:	ProcInfo(InHandle)
	{
	}

	/** Accessors. */
	FORCEINLINE pid_t Get() const
	{
		return ProcInfo ? ProcInfo->GetProcessId() : -1;
	}

	/** Resets the handle to invalid */
	FORCEINLINE void Reset()
	{
		ProcInfo = nullptr;
	}

	/** Checks the validity of handle */
	FORCEINLINE bool IsValid() const
	{
		return ProcInfo != nullptr;
	}

	// the below is not part of FProcHandle API and is specific to Linux implementation
	FORCEINLINE FProcState* GetProcessInfo() const
	{
		return ProcInfo;
	}
};

/** Wrapper around Linux file descriptors */
struct FPipeHandle
{
	FPipeHandle(int Fd)
		:	PipeDesc(Fd)
	{
	}

	~FPipeHandle();

	/**
	 * Reads until EOF.
	 */
	FString Read();

	/**
	 * Reads until EOF.
	 */
	bool ReadToArray(TArray<uint8> & Output);

	/**
	 * Returns raw file handle.
	 */
	int GetHandle() const
	{
		return PipeDesc;
	}

protected:

	int	PipeDesc;
};

/**
 * Linux implementation of the Process OS functions
 */
struct CORE_API FLinuxPlatformProcess : public FGenericPlatformProcess
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
		// Private implementation data.
		struct FProcEnumData* Data;
	};

	/**
	 * Process enumeration info structure.
	 */
	struct CORE_API FProcEnumInfo
	{
		friend FLinuxPlatformProcess::FProcEnumerator::FProcEnumerator();

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
		FProcEnumInfo(uint32 InPID);

		// Current process PID.
		uint32 PID;
	};

	static void* GetDllHandle( const TCHAR* Filename );
	static void FreeDllHandle( void* DllHandle );
	static void* GetDllExport( void* DllHandle, const TCHAR* ProcName );
	static int32 GetDllApiVersion( const TCHAR* Filename );
	static const TCHAR* ComputerName();
	static void CleanFileCache();
	static const TCHAR* BaseDir();
	static const TCHAR* UserName(bool bOnlyAlphaNumeric = true);
	static const TCHAR* UserDir();
	static const TCHAR* UserSettingsDir();
	static const TCHAR* ApplicationSettingsDir();
	static FString GetCurrentWorkingDirectory();
	static FString GenerateApplicationPath(const FString& AppName, EBuildConfigurations::Type BuildConfiguration);
	static FString GetApplicationName( uint32 ProcessId );
	static bool SetProcessLimits(EProcessResource::Type Resource, uint64 Limit);
	static const TCHAR* ExecutableName(bool bRemoveExtension = true);
	static const TCHAR* GetModulePrefix();
	static const TCHAR* GetModuleExtension();
	static const TCHAR* GetBinariesSubdirectory();
	static void ClosePipe( void* ReadPipe, void* WritePipe );
	static bool CreatePipe( void*& ReadPipe, void*& WritePipe );
	static FString ReadPipe( void* ReadPipe );
	static bool ReadPipeToArray(void* ReadPipe, TArray<uint8> & Output);
	static bool WritePipe(void* WritePipe, const FString& Message, FString* OutWritten = nullptr);
	static class FRunnableThread* CreateRunnableThread();
	static bool CanLaunchURL(const TCHAR* URL);
	static void LaunchURL(const TCHAR* URL, const TCHAR* Parms, FString* Error);
	static FProcHandle CreateProc(const TCHAR* URL, const TCHAR* Parms, bool bLaunchDetached, bool bLaunchHidden, bool bLaunchReallyHidden, uint32* OutProcessID, int32 PriorityModifier, const TCHAR* OptionalWorkingDirectory, void* PipeWriteChild, void* PipeReadChild = nullptr);
	static bool IsProcRunning( FProcHandle & ProcessHandle );
	static void WaitForProc( FProcHandle & ProcessHandle );
	static void CloseProc( FProcHandle & ProcessHandle );
	static void TerminateProc( FProcHandle & ProcessHandle, bool KillTree = false );
	static EWaitAndForkResult WaitAndFork();
	static uint32 GetCurrentProcessId();
	static bool GetProcReturnCode( FProcHandle & ProcHandle, int32* ReturnCode );
	static bool Daemonize();
	static bool IsApplicationRunning( uint32 ProcessId );
	static bool IsApplicationRunning( const TCHAR* ProcName );
	static bool ExecProcess( const TCHAR* URL, const TCHAR* Params, int32* OutReturnCode, FString* OutStdOut, FString* OutStdErr );
	static void ExploreFolder( const TCHAR* FilePath );
	static void LaunchFileInDefaultExternalApplication( const TCHAR* FileName, const TCHAR* Parms = NULL, ELaunchVerb::Type Verb = ELaunchVerb::Open );
	static bool IsFirstInstance();

	/**
	 * @brief Releases locks that we held for IsFirstInstance check
	 */
	static void CeaseBeingFirstInstance();

	/**
	 * @brief Returns user home directory (i.e. $HOME).
	 *
	 * Like other directory functions, cannot return nullptr!
	 */
	static const TCHAR* UserHomeDir();
};

typedef FLinuxPlatformProcess FPlatformProcess;
