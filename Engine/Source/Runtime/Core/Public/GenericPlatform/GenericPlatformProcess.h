// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/UnrealString.h"
#include "Templates/Function.h"

class Error;
struct FProcHandle;

template <typename FuncType> class TFunctionRef;

namespace EProcessResource
{
	enum Type
	{
		/** 
		 * Limits address space - basically limits the largest address the process can get. Affects mmap() (won't be able to map files larger than that) among others.
		 * May also limit automatic stack expansion, depending on platform (e.g. Linux)
		 */
		VirtualMemory
	};
}


/** Not all platforms have different opening semantics, but Windows allows you to specify a specific verb when opening a file. */
namespace ELaunchVerb
{
	enum Type
	{
		/** Launch the application associated with opening file to 'view' */
		Open,

		/** Launch the application associated with opening file to 'edit' */
		Edit,
	};
}


/** Generic implementation for the process handle. */
template< typename T, T InvalidHandleValue >
struct TProcHandle
{
	typedef T HandleType;
public:

	/** Default constructor. */
	FORCEINLINE TProcHandle()
		: Handle( InvalidHandleValue )
	{ }

	/** Initialization constructor. */
	FORCEINLINE explicit TProcHandle( T Other )
		: Handle( Other )
	{ }

	/** Assignment operator. */
	FORCEINLINE TProcHandle& operator=( const TProcHandle& Other )
	{
		if( this != &Other )
		{
			Handle = Other.Handle;
		}
		return *this;
	}

	/** Accessors. */
	FORCEINLINE T Get() const
	{
		return Handle;
	}

	FORCEINLINE void Reset()
	{
		Handle = InvalidHandleValue;
	}

	FORCEINLINE bool IsValid() const
	{
		return Handle != InvalidHandleValue;
	}

protected:
	/** Platform specific handle. */
	T Handle;
};


struct FProcHandle;

/**
* Generic implementation for most platforms, these tend to be unused and unimplemented
**/
struct CORE_API FGenericPlatformProcess
{
	/**
	 * Generic representation of a interprocess semaphore
	 */
	struct FSemaphore
	{
		/** Returns the name of the object */
		const TCHAR* GetName() const
		{
			return Name;
		}

		/** Acquires an exclusive access (also known as Wait()) */
		virtual void Lock() = 0;

		/** 
		 * Tries to acquire and exclusive access for a specified amount of nanoseconds (also known as TryWait()).
		 *
		 * @param Nanoseconds (10^-9 seconds) to wait for, 
		 * @return false if was not able to lock within given time
		 */
		virtual bool TryLock(uint64 NanosecondsToWait) = 0;

		/** Relinquishes an exclusive access (also known as Release())*/
		virtual void Unlock() = 0;

		/** 
		 * Creates and initializes a new instance with the specified name.
		 *
		 * @param InName name of the semaphore (all processes should use the same)
		 */
		FSemaphore(const FString& InName);

		/** Virtual destructor. */
		virtual ~FSemaphore() { };

	protected:

		enum Limits
		{
			MaxSemaphoreName = 128
		};

		/** Name of the region */
		TCHAR			Name[MaxSemaphoreName];
	};

	/** Load a DLL. **/
	static void* GetDllHandle( const TCHAR* Filename );

	/** Free a DLL. **/
	static void FreeDllHandle( void* DllHandle );

	/** Lookup the address of a DLL function. **/
	static void* GetDllExport( void* DllHandle, const TCHAR* ProcName );

	/** Gets the API version from the specified DLL **/
	static int32 GetDllApiVersion( const TCHAR* Filename );

	/** Adds a directory to search in when resolving implicitly loaded or filename-only DLLs. **/
	FORCEINLINE static void AddDllDirectory(const TCHAR* Directory)
	{

	}

	/** Set a directory to look for DLL files. NEEDS to have a Pop call when complete */
	FORCEINLINE static void PushDllDirectory(const TCHAR* Directory)
	{
	
	}

	/** Unsets a directory to look for DLL files. The same directory must be passed in as the Push call to validate */
	FORCEINLINE static void PopDllDirectory(const TCHAR* Directory)
	{

	}

	/** Deletes 1) all temporary files; 2) all cache files that are no longer wanted. **/
	FORCEINLINE static void CleanFileCache()
	{

	}

	/**
	 * Retrieves the ProcessId of this process.
	 *
	 * @return the ProcessId of this process.
	 */
	static uint32 GetCurrentProcessId();
	
	/**	 
	 * Change the thread processor affinity
	 *
	 * @param AffinityMask A bitfield indicating what processors the thread is allowed to run on.
	 */
	static void SetThreadAffinityMask( uint64 AffinityMask );

	/** Allow the platform to do anything it needs for game thread */
	static void SetupGameThread() { }

	/** Allow the platform to do anything it needs for render thread */
	static void SetupRenderThread() { }

	/** Allow the platform to do anything it needs for audio thread */
	static void SetupAudioThread() { }

	/** Content saved to the game or engine directories should be rerouted to user directories instead **/
	static bool ShouldSaveToUserDir();

	/** Get startup directory.  NOTE: Only one return value is valid at a time! **/
	static const TCHAR* BaseDir();

	/** Get user directory.  NOTE: Only one return value is valid at a time! **/
	static const TCHAR* UserDir();

	/** Get the user settings directory.  NOTE: Only one return value is valid at a time! **/
	static const TCHAR *UserSettingsDir();

	/** Get the user temporary directory.  NOTE: Only one return value is valid at a time! **/
	static const TCHAR *UserTempDir();

	/** Get application settings directory.  NOTE: Only one return value is valid at a time! **/
	static const TCHAR* ApplicationSettingsDir();

	/** Get computer name.  NOTE: Only one return value is valid at a time! **/
	static const TCHAR* ComputerName();

	/** Get user name.  NOTE: Only one return value is valid at a time! **/
	static const TCHAR* UserName(bool bOnlyAlphaNumeric = true);
	static const TCHAR* ShaderDir();
	static void SetShaderDir(const TCHAR*Where);
	static void SetCurrentWorkingDirectoryToBaseDir();
	
	/** Get the current working directory (only really makes sense on desktop platforms) */
	static FString GetCurrentWorkingDirectory();

	/**
	 * Sets the process limits.
	 *
	 * @param Resource one of process resources.
	 * @param Limit the maximum amount of the resource (for some OS, this means both hard and soft limits).
	 * @return true on success, false otherwise.
	 */
	static bool SetProcessLimits(EProcessResource::Type Resource, uint64 Limit)
	{
		return true;	// return fake success by default, that way the game won't early quit on platforms that don't implement this
	}

	/**
	 * Get the shader working directory.
	 *
	 * @return The path to the directory.
	 */
	static const FString ShaderWorkingDir();

	/**	Clean the shader working directory. */
	static void CleanShaderWorkingDir();

	/**
	 * Return the name of the currently running executable
	 *
	 * @param	bRemoveExtension	true to remove the extension of the executable name, false to leave it intact
	 * @return 	Name of the currently running executable
	 */
	static const TCHAR* ExecutableName(bool bRemoveExtension = true);

	/**
	 * Generates the path to the specified application or game.
	 *
	 * The application must reside in the Engine's binaries directory. The returned path is relative to this
	 * executable's directory.For example, calling this method with "UE4" and EBuildConfigurations::Debug
	 * on Windows 64-bit will generate the path "../Win64/UE4Editor-Win64-Debug.exe"
	 *
	 * @param AppName The name of the application or game.
	 * @param BuildConfiguration The build configuration of the game.
	 * @return The generated application path.
	 */
	static FString GenerateApplicationPath( const FString& AppName, EBuildConfigurations::Type BuildConfiguration);

	/**
	 * Return the prefix of dynamic library (e.g. lib)
	 *
	 * @return The prefix string.
	 * @see GetModuleExtension, GetModulesDirectory
	 */
	static const TCHAR* GetModulePrefix();

	/**
	 * Return the extension of dynamic library
	 *
	 * @return Extension of dynamic library.
	 * @see GetModulePrefix, GetModulesDirectory
	 */
	static const TCHAR* GetModuleExtension();

	/**
	 * Used only by platforms with DLLs, this gives the subdirectory from binaries to find the executables
	 */
	static const TCHAR* GetBinariesSubdirectory();

	/**
	 * Used only by platforms with DLLs, this gives the full path to the main directory containing modules
	 *
	 * @return The path to the directory.
	 * @see GetModulePrefix, GetModuleExtension
	 */
	static const FString GetModulesDirectory();
	
	/**
	 * Launch a uniform resource locator (i.e. http://www.epicgames.com/unreal).
	 * This is expected to return immediately as the URL is launched by another
	 * task. The URL param must already be a valid URL. If you're looking for code 
	 * to properly escape a URL fragment, use FGenericPlatformHttp::UrlEncode.
	 */
	static void LaunchURL( const TCHAR* URL, const TCHAR* Parms, FString* Error );

	/**
	 * Checks if the platform can launch a uniform resource locator (i.e. http://www.epicgames.com/unreal).
	 **/
	static bool CanLaunchURL(const TCHAR* URL);
	
	/**
	 * Retrieves the platform-specific bundle identifier or package name of the game
	 *
	 * @return The game's bundle identifier or package name.
	 */
	static FString GetGameBundleId();
	
	/**
	 * Creates a new process and its primary thread. The new process runs the
	 * specified executable file in the security context of the calling process.
	 * @param URL					executable name
	 * @param Parms					command line arguments
	 * @param bLaunchDetached		if true, the new process will have its own window
	 * @param bLaunchHidded			if true, the new process will be minimized in the task bar
	 * @param bLaunchReallyHidden	if true, the new process will not have a window or be in the task bar
	 * @param OutProcessId			if non-NULL, this will be filled in with the ProcessId
	 * @param PriorityModifier		-2 idle, -1 low, 0 normal, 1 high, 2 higher
	 * @param OptionalWorkingDirectory		Directory to start in when running the program, or NULL to use the current working directory
	 * @param PipeWrite				Optional HANDLE to pipe for redirecting output
	 * @return	The process handle for use in other process functions
	 */
	static FProcHandle CreateProc( const TCHAR* URL, const TCHAR* Parms, bool bLaunchDetached, bool bLaunchHidden, bool bLaunchReallyHidden, uint32* OutProcessID, int32 PriorityModifier, const TCHAR* OptionalWorkingDirectory, void* PipeWriteChild, void * PipeReadChild = nullptr);

	/**
	 * Opens an existing process. 
	 *
	 * @param ProcessID				The process id of the process for which we want to obtain a handle.
	 * @return The process handle for use in other process functions
	 */
	static FProcHandle OpenProcess(uint32 ProcessID);

	/**
	 * Returns true if the specified process is running 
	 *
	 * @param ProcessHandle handle returned from FPlatformProcess::CreateProc
	 * @return true if the process is still running
	 */
	static bool IsProcRunning( FProcHandle & ProcessHandle );
	
	/**
	 * Waits for a process to stop
	 *
	 * @param ProcessHandle handle returned from FPlatformProcess::CreateProc
	 */
	static void WaitForProc( FProcHandle & ProcessHandle );

	/**
	 * Cleans up FProcHandle after we're done with it.
	 *
	 * @param ProcessHandle handle returned from FPlatformProcess::CreateProc.
	 */
	static void CloseProc( FProcHandle & ProcessHandle );

	/** Terminates a process
	 *
	 * @param ProcessHandle handle returned from FPlatformProcess::CreateProc
	 * @param KillTree Whether the entire process tree should be terminated.
	 */
	static void TerminateProc( FProcHandle & ProcessHandle, bool KillTree = false );

	enum class EWaitAndForkResult : uint8
	{
		Error,
		Parent,
		Child
	};

	/**
	 * Waits for process signals and forks child processes.
	 *
	 * WaitAndFork stalls the invoking process and forks child processes when signals are sent to it from an external source.
	 * Forked child processes will provide a return value of EWaitAndForkResult::Child, while the parent process
	 * will not return until GIsRequestingExit is true (EWaitAndForkResult::Parent) or there was an error (EWaitAndForkResult::Error)
	 * The signal the parent process expects is platform-specific (i.e. SIGRTMIN+1 on Linux). 
	 */
	static EWaitAndForkResult WaitAndFork();

	/** Retrieves the termination status of the specified process. **/
	static bool GetProcReturnCode( FProcHandle & ProcHandle, int32* ReturnCode );

	/** Returns true if the specified application is running */
	static bool IsApplicationRunning( uint32 ProcessId );

	/** Returns true if the specified application is running */
	static bool IsApplicationRunning( const TCHAR* ProcName );

	/** Returns the Name of process given by the PID.  Returns Empty string "" if PID not found. */
	static FString GetApplicationName( uint32 ProcessId );

	/** Outputs the virtual memory usage, of the process with the specified PID */
	static bool GetApplicationMemoryUsage(uint32 ProcessId, SIZE_T* OutMemoryUsage);

	/**
	 * Executes a process, returning the return code, stdout, and stderr. This
	 * call blocks until the process has returned.
	 * @param OutReturnCode may be 0
	 * @param OutStdOut may be 0
	 * @param OutStdErr may be 0
	 */
	static bool ExecProcess( const TCHAR* URL, const TCHAR* Params, int32* OutReturnCode, FString* OutStdOut, FString* OutStdErr );

	/**
	 * Executes a process as administrator, requesting elevation as necessary. This
	 * call blocks until the process has returned.
	 */
	static bool ExecElevatedProcess(const TCHAR* URL, const TCHAR* Params, int32* OutReturnCode);

	/**
	 * Attempt to launch the provided file name in its default external application. Similar to FPlatformProcess::LaunchURL,
	 * with the exception that if a default application isn't found for the file, the user will be prompted with
	 * an "Open With..." dialog.
	 *
	 * @param	FileName	Name of the file to attempt to launch in its default external application
	 * @param	Parms		Optional parameters to the default application
	 * @param	Verb		Optional verb to use when opening the file, if it applies for the platform.
	 */
	static void LaunchFileInDefaultExternalApplication( const TCHAR* FileName, const TCHAR* Parms = NULL, ELaunchVerb::Type Verb = ELaunchVerb::Open );

	/**
	 * Attempt to "explore" the folder specified by the provided file path
	 *
	 * @param	FilePath	File path specifying a folder to explore
	 */
	static void ExploreFolder( const TCHAR* FilePath );

#if PLATFORM_HAS_BSD_TIME 

	/** Sleep this thread for Seconds.  0.0 means release the current time slice to let other threads get some attention. Uses stats.*/
	static void Sleep( float Seconds );
	/** Sleep this thread for Seconds.  0.0 means release the current time slice to let other threads get some attention. */
	static void SleepNoStats( float Seconds );
	/** Sleep this thread infinitely. */
	static void SleepInfinite();

#endif // PLATFORM_HAS_BSD_TIME

	/**
	* Sleep thread until condition is satisfied.
	*
	* @param	Condition	Condition to evaluate.
	* @param	SleepTime	Time to sleep
	*/
	static void ConditionalSleep(TFunctionRef<bool()> Condition, float SleepTime = 0.0f);

	/**
	 * Creates a new event.
	 *
	 * @param bIsManualReset Whether the event requires manual reseting or not.
	 * @return A new event, or nullptr none could be created.
	 * @see GetSynchEventFromPool, ReturnSynchEventToPool
	 */
	// Message to others in the future, don't try to delete this function as it isn't exactly deprecated, but it should only ever be called from FEventPool::GetEventFromPool()
	DEPRECATED(4.8, "Please use GetSynchEventFromPool to create a new event, and ReturnSynchEventToPool to release the event.")
	static class FEvent* CreateSynchEvent(bool bIsManualReset = false);

	/**
	 * Gets an event from the pool or creates a new one if necessary.
	 *
	 * @param bIsManualReset Whether the event requires manual reseting or not.
	 * @return An event, or nullptr none could be created.
	 * @see CreateSynchEvent, ReturnSynchEventToPool
	 */
	static class FEvent* GetSynchEventFromPool(bool bIsManualReset = false);

	/**
	 * Returns an event to the pool.
	 *
	 * @param Event The event to return.
	 * @see CreateSynchEvent, GetSynchEventFromPool
	 */
	static void ReturnSynchEventToPool(FEvent* Event);

	/**
	 * Creates the platform-specific runnable thread. This should only be called from FRunnableThread::Create.
	 *
	 * @return The newly created thread
	 */
	static class FRunnableThread* CreateRunnableThread();

	/**
	 * Closes an anonymous pipe.
	 *
	 * @param ReadPipe The handle to the read end of the pipe.
	 * @param WritePipe The handle to the write end of the pipe.
	 * @see CreatePipe, ReadPipe
	 */
	static void ClosePipe( void* ReadPipe, void* WritePipe );

	/**
	 * Creates a writable anonymous pipe.
	 *
	 * Anonymous pipes can be used to capture and/or redirect STDOUT and STDERROR of a process.
	 * The pipe created by this method can be passed into CreateProc as Write
	 *
	 * @param ReadPipe Will hold the handle to the read end of the pipe.
	 * @param WritePipe Will hold the handle to the write end of the pipe.
	 * @return true on success, false otherwise.
	 * @see ClosePipe, ReadPipe
	 */
	static bool CreatePipe( void*& ReadPipe, void*& WritePipe );

	/**
	 * Reads all pending data from an anonymous pipe, such as STDOUT or STDERROR of a process.
	 *
	 * @param Pipe The handle to the pipe to read from.
	 * @return A string containing the read data.
	 * @see ClosePipe, CreatePipe
	 */
	static FString ReadPipe( void* ReadPipe );

	/**
	 * Reads all pending data from an anonymous pipe, such as STDOUT or STDERROR of a process.
	 *
	 * @param Pipe The handle to the pipe to read from.
	 * @param Output The data read.
	 * @return true if successful (i.e. any data was read)
	 * @see ClosePipe, CreatePipe
	 */
	static bool ReadPipeToArray(void* ReadPipe, TArray<uint8> & Output);

	/**
	* Sends the message to process through pipe
	*
	* @param WritePipe Pipe for writing.
	* @param Message The message to be written.
	* @param OutWritten Optional parameter to know how much of the string written.
	* @return True if all bytes written successfully.
	* @see CreatePipe, ClosePipe, ReadPipe
	*/
	static bool WritePipe(void* WritePipe, const FString& Message, FString* OutWritten = nullptr);

	/**
	 * Gets whether this platform can use multiple threads.
	 *
	 * @return true if the platform can use multiple threads, false otherwise.
	 */
	static bool SupportsMultithreading();
	
	/** Enables Real Time Mode on the current thread. */
	static void SetRealTimeMode() { }

	/**
	 * Creates or opens an interprocess synchronization object.
	 *
	 * @param Name name (so we can use it across processes).
	 * @param bCreate If true, the function will try to create, otherwise will try to open existing.
	 * @param MaxLocks Maximum amount of locks that the semaphore can have (pass 1 to make it act as mutex).
	 */
	static FSemaphore* NewInterprocessSynchObject(const FString& Name, bool bCreate, uint32 MaxLocks = 1);

	/**
	 * Deletes an interprocess synchronization object.
	 *
	 * @param Object object to destroy.
	 */
	static bool DeleteInterprocessSynchObject(FSemaphore * Object);

	/**
	 * Makes process run as a system service (daemon), i.e. detaches it from whatever user session it was initially run from.
	 *
	 * @return true if successful, false otherwise.
	 */
	static bool Daemonize();

	/**
	 * Checks if we're the first instance. An instance can become first if the previous first instance quits before it.
	 */
	static bool IsFirstInstance();

	/**
	 * Returns the map virtual shader directory path -> real shader directory path.
	 */
	static const TMap<FString, FString>& AllShaderSourceDirectoryMappings();
	
	/**
	 * Clears all shader source directory mappings.
	 */
	static void ResetAllShaderSourceDirectoryMappings();

	/**
	 * Maps a real shader directory existing on disk to a virtual shader directory.
	 * @param VirtualShaderDirectory Unique absolute path of the virtual shader directory (ex: /Project).
	 * @param RealShaderDirectory FPlatformProcess::BaseDir() relative path of the directory map.
	 */
	static void AddShaderSourceDirectoryMapping(const FString& VirtualShaderDirectory, const FString& RealShaderDirectory);
};


