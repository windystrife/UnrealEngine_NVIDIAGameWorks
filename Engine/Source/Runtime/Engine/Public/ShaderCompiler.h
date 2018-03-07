// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ShaderCompiler.h: Platform independent shader compilation definitions.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Templates/RefCounting.h"
#include "Templates/ScopedPointer.h"
#include "HAL/PlatformProcess.h"
#include "ShaderCore.h"
#include "Shader.h"
#include "HAL/RunnableThread.h"
#include "HAL/Runnable.h"
#include "UniquePtr.h"

class FShaderCompileJob;
class FShaderPipelineCompileJob;
class FVertexFactoryType;

DECLARE_LOG_CATEGORY_EXTERN(LogShaderCompilers, Log, All);

class FShaderCompileJob;
class FShaderPipelineCompileJob;

#define DEBUG_INFINITESHADERCOMPILE 0


/** Stores all of the common information used to compile a shader or pipeline. */
class FShaderCommonCompileJob : public FRefCountedObject
{
public:
	/** Id of the shader map this shader belongs to. */
	uint32 Id;
	/** true if the results of the shader compile have been processed. */
	bool bFinalized;
	/** Output of the shader compile */
	bool bSucceeded;
	bool bOptimizeForLowLatency;

	FShaderCommonCompileJob(uint32 InId) :
		Id(InId),
		bFinalized(false),
		bSucceeded(false),
		bOptimizeForLowLatency(false)
	{
	}

	virtual FShaderCompileJob* GetSingleShaderJob() { return nullptr; }
	virtual const FShaderCompileJob* GetSingleShaderJob() const { return nullptr; }
	virtual FShaderPipelineCompileJob* GetShaderPipelineJob() { return nullptr; }
	virtual const FShaderPipelineCompileJob* GetShaderPipelineJob() const { return nullptr; }
};


/** Stores all of the input and output information used to compile a single shader. */
class FShaderCompileJob : public FShaderCommonCompileJob
{
public:
	/** Vertex factory type that this shader belongs to, may be NULL */
	FVertexFactoryType* VFType;
	/** Shader type that this shader belongs to, must be valid */
	FShaderType* ShaderType;
	/** Input for the shader compile */
	FShaderCompilerInput Input;
	FShaderCompilerOutput Output;

	// List of pipelines that are sharing this job.
	TMap<const FVertexFactoryType*, TArray<const FShaderPipelineType*>> SharingPipelines;

	FShaderCompileJob(uint32 InId, FVertexFactoryType* InVFType, FShaderType* InShaderType) :
		FShaderCommonCompileJob(InId),
		VFType(InVFType),
		ShaderType(InShaderType)
	{
	}

	virtual FShaderCompileJob* GetSingleShaderJob() override { return this; }
	virtual const FShaderCompileJob* GetSingleShaderJob() const override { return this; }
};

class FShaderPipelineCompileJob : public FShaderCommonCompileJob
{
public:
	TArray<FShaderCommonCompileJob*> StageJobs;
	bool bFailedRemovingUnused;

	/** Shader pipeline that this shader belongs to, may (currently) be NULL */
	const FShaderPipelineType* ShaderPipeline;

	FShaderPipelineCompileJob(uint32 InId, const FShaderPipelineType* InShaderPipeline, int32 NumStages) :
		FShaderCommonCompileJob(InId),
		bFailedRemovingUnused(false),
		ShaderPipeline(InShaderPipeline)
	{
		check(InShaderPipeline && InShaderPipeline->GetName());
		check(NumStages > 0);
		StageJobs.Empty(NumStages);
	}

	~FShaderPipelineCompileJob()
	{
		for (int32 Index = 0; Index < StageJobs.Num(); ++Index)
		{
			delete StageJobs[Index];
		}
		StageJobs.Reset();
	}

	virtual FShaderPipelineCompileJob* GetShaderPipelineJob() override { return this; }
	virtual const FShaderPipelineCompileJob* GetShaderPipelineJob() const override { return this; }
};

class FGlobalShaderTypeCompiler
{
public:
	/**
	* Enqueues compilation of a shader of this type.
	*/
	ENGINE_API static class FShaderCompileJob* BeginCompileShader(FGlobalShaderType* ShaderType, EShaderPlatform Platform, const FShaderPipelineType* ShaderPipeline, TArray<FShaderCommonCompileJob*>& NewJobs);

	/**
	* Enqueues compilation of a shader pipeline of this type.
	*/
	ENGINE_API static void BeginCompileShaderPipeline(EShaderPlatform Platform, const FShaderPipelineType* ShaderPipeline, const TArray<FGlobalShaderType*>& ShaderStages, TArray<FShaderCommonCompileJob*>& NewJobs);

	/** Either returns an equivalent existing shader of this type, or constructs a new instance. */
	static FShader* FinishCompileShader(FGlobalShaderType* ShaderType, const FShaderCompileJob& CompileJob, const FShaderPipelineType* ShaderPipelineType);
};

class FShaderCompileThreadRunnableBase : public FRunnable
{
	friend class FShaderCompilingManager;

protected:
	/** The manager for this thread */
	class FShaderCompilingManager* Manager;
	/** The runnable thread */
	FRunnableThread* Thread;
	
	/** If the thread has been terminated by an unhandled exception, this contains the error message. */
	FString ErrorMessage;
	/** true if the thread has been terminated by an unhandled exception. */
	bool bTerminatedByError;

	volatile bool bForceFinish;

public:
	FShaderCompileThreadRunnableBase(class FShaderCompilingManager* InManager);
	virtual ~FShaderCompileThreadRunnableBase()
	{}

	void StartThread();

	// FRunnable interface.
	virtual void Stop() { bForceFinish = true; }
	virtual uint32 Run();
	inline void WaitForCompletion() const
	{
		if( Thread )
		{
			Thread->WaitForCompletion();
		}
	}

	/** Checks the thread's health, and passes on any errors that have occured.  Called by the main thread. */
	void CheckHealth() const;

	/** Main work loop. */
	virtual int32 CompilingLoop() = 0;
};

/** 
 * Shader compiling thread
 * This runs in the background while UE4 is running, launches shader compile worker processes when necessary, and feeds them inputs and reads back the outputs.
 */
class FShaderCompileThreadRunnable : public FShaderCompileThreadRunnableBase
{
	friend class FShaderCompilingManager;
private:

	/** Information about the active workers that this thread is tracking. */
	TArray<struct FShaderCompileWorkerInfo*> WorkerInfos;
	/** Tracks the last time that this thread checked if the workers were still active. */
	double LastCheckForWorkersTime;

public:
	/** Initialization constructor. */
	FShaderCompileThreadRunnable(class FShaderCompilingManager* InManager);
	virtual ~FShaderCompileThreadRunnable();

private:

	/** 
	 * Grabs tasks from Manager->CompileQueue in a thread safe way and puts them into QueuedJobs of available workers. 
	 * Also writes completed jobs to Manager->ShaderMapJobs.
	 */
	int32 PullTasksFromQueue();

	/** Used when compiling through workers, writes out the worker inputs for any new tasks in WorkerInfos.QueuedJobs. */
	void WriteNewTasks();

	/** Used when compiling through workers, launches worker processes if needed. */
	bool LaunchWorkersIfNeeded();

	/** Used when compiling through workers, attempts to open the worker output file if the worker is done and read the results. */
	void ReadAvailableResults();

	/** Used when compiling directly through the console tools dll. */
	void CompileDirectlyThroughDll();

	/** Main work loop. */
	virtual int32 CompilingLoop() override;
};

namespace FShaderCompileUtilities
{
	bool DoWriteTasks(const TArray<FShaderCommonCompileJob*>& QueuedJobs, FArchive& TransferFile);
	void DoReadTaskResults(const TArray<FShaderCommonCompileJob*>& QueuedJobs, FArchive& OutputFile);
}

#if PLATFORM_WINDOWS // XGE shader compilation is only supported on Windows.

class FShaderCompileXGEThreadRunnable_XmlInterface : public FShaderCompileThreadRunnableBase
{
private:
	/** The handle referring to the XGE console process, if a build is in progress. */
	FProcHandle BuildProcessHandle;
	
	/** Process ID of the XGE console, if a build is in progress. */
	uint32 BuildProcessID;

	/**
	 * A map of directory paths to shader jobs contained within that directory.
	 * One entry per XGE task.
	 */
	class FShaderBatch
	{
		TArray<FShaderCommonCompileJob*> Jobs;
		bool bTransferFileWritten;

	public:
		const FString& DirectoryBase;
		const FString& InputFileName;
		const FString& SuccessFileName;
		const FString& OutputFileName;

		int32 BatchIndex;
		int32 DirectoryIndex;

		FString WorkingDirectory;
		FString OutputFileNameAndPath;
		FString SuccessFileNameAndPath;
		FString InputFileNameAndPath;
		
		FShaderBatch(const FString& InDirectoryBase, const FString& InInputFileName, const FString& InSuccessFileName, const FString& InOutputFileName, int32 InDirectoryIndex, int32 InBatchIndex)
			: bTransferFileWritten(false)
			, DirectoryBase(InDirectoryBase)
			, InputFileName(InInputFileName)
			, SuccessFileName(InSuccessFileName)
			, OutputFileName(InOutputFileName)
		{
			SetIndices(InDirectoryIndex, InBatchIndex);
		}

		void SetIndices(int32 InDirectoryIndex, int32 InBatchIndex);

		void CleanUpFiles(bool keepInputFile);

		inline int32 NumJobs()
		{
			return Jobs.Num();
		}
		inline const TArray<FShaderCommonCompileJob*>& GetJobs() const
		{
			return Jobs;
		}

		void AddJob(FShaderCommonCompileJob* Job);
		
		void WriteTransferFile();
	};
	TArray<FShaderBatch*> ShaderBatchesInFlight;
	TArray<FShaderBatch*> ShaderBatchesFull;
	TSparseArray<FShaderBatch*> ShaderBatchesIncomplete;

	/** The full path to the two working directories for XGE shader builds. */
	const FString XGEWorkingDirectory;
	uint32 XGEDirectoryIndex;

	uint64 LastAddTime;
	uint64 StartTime;
	int32 BatchIndexToCreate;
	int32 BatchIndexToFill;

	FDateTime ScriptFileCreationTime;

	void PostCompletedJobsForBatch(FShaderBatch* Batch);

	void GatherResultsFromXGE();

public:
	/** Initialization constructor. */
	FShaderCompileXGEThreadRunnable_XmlInterface(class FShaderCompilingManager* InManager);
	virtual ~FShaderCompileXGEThreadRunnable_XmlInterface();

	/** Main work loop. */
	virtual int32 CompilingLoop() override;

	static bool IsSupported();
};

class FShaderCompileXGEThreadRunnable_InterceptionInterface : public FShaderCompileThreadRunnableBase
{
	uint32 NumDispatchedJobs;

	TSparseArray<class FXGEShaderCompilerTask*> DispatchedTasks;

public:
	/** Initialization constructor. */
	FShaderCompileXGEThreadRunnable_InterceptionInterface(class FShaderCompilingManager* InManager);
	virtual ~FShaderCompileXGEThreadRunnable_InterceptionInterface();

	/** Main work loop. */
	virtual int32 CompilingLoop() override;

	static bool IsSupported();
};

#endif // PLATFORM_WINDOWS

/** Results for a single compiled shader map. */
struct FShaderMapCompileResults
{
	FShaderMapCompileResults() :
		NumJobsQueued(0),
		bAllJobsSucceeded(true),
		bApplyCompletedShaderMapForRendering(true),
		bRecreateComponentRenderStateOnCompletion(false)
	{}

	int32 NumJobsQueued;
	bool bAllJobsSucceeded;
	bool bApplyCompletedShaderMapForRendering;
	bool bRecreateComponentRenderStateOnCompletion;
	TArray<FShaderCommonCompileJob*> FinishedJobs;
};

/** Results for a single compiled and finalized shader map. */
struct FShaderMapFinalizeResults : public FShaderMapCompileResults
{
	/** Tracks finalization progress on this shader map. */
	int32 FinalizeJobIndex;

	// List of pipelines with shared shaders; nullptr for non mesh pipelines
	TMap<const FVertexFactoryType*, TArray<const FShaderPipelineType*> > SharedPipelines;

	FShaderMapFinalizeResults(const FShaderMapCompileResults& InCompileResults) :
		FShaderMapCompileResults(InCompileResults),
		FinalizeJobIndex(0)
	{}
};

/**  
 * Manager of asynchronous and parallel shader compilation.
 * This class contains an interface to enqueue and retreive asynchronous shader jobs, and manages a FShaderCompileThreadRunnable.
 */
class FShaderCompilingManager
{
	friend class FShaderCompileThreadRunnableBase;
	friend class FShaderCompileThreadRunnable;

#if PLATFORM_WINDOWS
	friend class FShaderCompileXGEThreadRunnable_XmlInterface;
	friend class FShaderCompileXGEThreadRunnable_InterceptionInterface;
#endif // PLATFORM_WINDOWS

private:

	//////////////////////////////////////////////////////
	// Thread shared properties: These variables can only be read from or written to when a lock on CompileQueueSection is obtained, since they are used by both threads.

	/** Tracks whether we are compiling while the game is running.  If true, we need to throttle down shader compiling CPU usage to avoid starving the runtime threads. */
	bool bCompilingDuringGame;
	/** Queue of tasks that haven't been assigned to a worker yet. */
	TArray<FShaderCommonCompileJob*> CompileQueue;
	/** Map from shader map Id to the compile results for that map, used to gather compiled results. */
	TMap<int32, FShaderMapCompileResults> ShaderMapJobs;

	/** Number of jobs currently being compiled.  This includes CompileQueue and any jobs that have been assigned to workers but aren't complete yet. */
	int32 NumOutstandingJobs;

	/** Critical section used to gain access to the variables above that are shared by both the main thread and the FShaderCompileThreadRunnable. */
	FCriticalSection CompileQueueSection;

	//////////////////////////////////////////////////////
	// Main thread state - These are only accessed on the main thread and used to track progress

	/** Map from shader map id to results being finalized.  Used to track shader finalizations over multiple frames. */
	TMap<int32, FShaderMapFinalizeResults> PendingFinalizeShaderMaps;

	/** The threads spawned for shader compiling. */
	TUniquePtr<FShaderCompileThreadRunnableBase> Thread;

	//////////////////////////////////////////////////////
	// Configuration properties - these are set only on initialization and can be read from either thread

	/** Number of busy threads to use for shader compiling while loading. */
	uint32 NumShaderCompilingThreads;
	/** Number of busy threads to use for shader compiling while in game. */
	uint32 NumShaderCompilingThreadsDuringGame;
	/** Largest number of jobs that can be put in the same batch. */
	int32 MaxShaderJobBatchSize;
	/** Process Id of UE4. */
	uint32 ProcessId;
	/** Whether to allow compiling shaders through the worker application, which allows multiple cores to be used. */
	bool bAllowCompilingThroughWorkers;
	/** Whether to allow shaders to compile in the background or to block after each material. */
	bool bAllowAsynchronousShaderCompiling;
	/** Whether to ask to retry a failed shader compile error. */
	bool bPromptToRetryFailedShaderCompiles;
	/** Whether to log out shader job completion times on the worker thread.  Useful for tracking down which global shader is taking a long time. */
	bool bLogJobCompletionTimes;
	/** Target execution time for ProcessAsyncResults.  Larger values speed up async shader map processing but cause more hitchiness while async compiling is happening. */
	float ProcessGameThreadTargetTime;
	/** Base directory where temporary files are written out during multi core shader compiling. */
	FString ShaderBaseWorkingDirectory;
	/** Absolute version of ShaderBaseWorkingDirectory. */
	FString AbsoluteShaderBaseWorkingDirectory;
	/** Absolute path to the directory to dump shader debug info to. */
	FString AbsoluteShaderDebugInfoDirectory;
	/** Name of the shader worker application. */
	FString ShaderCompileWorkerName;
	/** Whether the SCW has crashed and we should fall back to calling the compiler dll's directly. */
	bool bFallBackToDirectCompiles;

	/** 
	 * Tracks the total time that shader compile workers have been busy since startup.  
	 * Useful for profiling the shader compile worker thread time.
	 */
	double WorkersBusyTime;

	/** 
	 * Tracks which opt-in shader platforms have their warnings suppressed.
	 */
	uint64 SuppressedShaderPlatforms;

	/** Launches the worker, returns the launched process handle. */
	FProcHandle LaunchWorker(const FString& WorkingDirectory, uint32 ProcessId, uint32 ThreadId, const FString& WorkerInputFile, const FString& WorkerOutputFile);

	/** Blocks on completion of the given shader maps. */
	void BlockOnShaderMapCompletion(const TArray<int32>& ShaderMapIdsToFinishCompiling, TMap<int32, FShaderMapFinalizeResults>& CompiledShaderMaps);

	/** Blocks on completion of all shader maps. */
	void BlockOnAllShaderMapCompletion(TMap<int32, FShaderMapFinalizeResults>& CompiledShaderMaps);

	/** Finalizes the given shader map results and optionally assigns the affected shader maps to materials, while attempting to stay within an execution time budget. */
	void ProcessCompiledShaderMaps(TMap<int32, FShaderMapFinalizeResults>& CompiledShaderMaps, float TimeBudget);

	/** Finalizes the given Niagara shader map results and assigns the affected shader maps to Niagara scripts, while attempting to stay within an execution time budget. */
	void ProcessCompiledNiagaraShaderMaps(TMap<int32, FShaderMapFinalizeResults>& CompiledShaderMaps, float TimeBudget);

	/** Propagate the completed compile to primitives that might be using the materials compiled. */
	void PropagateMaterialChangesToPrimitives(const TMap<FMaterial*, class FMaterialShaderMap*>& MaterialsToUpdate);

	/** Recompiles shader jobs with errors if requested, and returns true if a retry was needed. */
	bool HandlePotentialRetryOnError(TMap<int32, FShaderMapFinalizeResults>& CompletedShaderMaps);

public:
	
	ENGINE_API FShaderCompilingManager();

	/** 
	 * Returns whether to display a notification that shader compiling is happening in the background. 
	 * Note: This is dependent on NumOutstandingJobs which is updated from another thread, so the results are non-deterministic.
	 */
	bool ShouldDisplayCompilingNotification() const 
	{ 
		// Heuristic based on the number of jobs outstanding
		return NumOutstandingJobs > 80; 
	}

	bool AllowAsynchronousShaderCompiling() const 
	{
		return bAllowAsynchronousShaderCompiling;
	}

	/** 
	 * Returns whether async compiling is happening.
	 * Note: This is dependent on NumOutstandingJobs which is updated from another thread, so the results are non-deterministic.
	 */
	bool IsCompiling() const
	{
		return NumOutstandingJobs > 0 || PendingFinalizeShaderMaps.Num() > 0;
	}

	/**
	 * return true if we have shader jobs in any state
	 * shader jobs are removed when they are applied to the gamethreadshadermap
	 * accessable from gamethread
	 */
	bool HasShaderJobs() const
	{
		return ShaderMapJobs.Num() > 0 || PendingFinalizeShaderMaps.Num() > 0;
	}

	/** 
	 * Returns the number of outstanding compile jobs.
	 * Note: This is dependent on NumOutstandingJobs which is updated from another thread, so the results are non-deterministic.
	 */
	int32 GetNumRemainingJobs() const
	{
		return NumOutstandingJobs;
	}

	const FString& GetAbsoluteShaderDebugInfoDirectory() const
	{
		return AbsoluteShaderDebugInfoDirectory;
	}

	bool AreWarningsSuppressed(const EShaderPlatform Platform) const
	{
		return (SuppressedShaderPlatforms & (static_cast<uint64>(1) << Platform)) != 0;
	}

	void SuppressWarnings(const EShaderPlatform Platform)
	{
		SuppressedShaderPlatforms |= static_cast<uint64>(1) << Platform;
	}

	/** 
	 * Adds shader jobs to be asynchronously compiled. 
	 * FinishCompilation or ProcessAsyncResults must be used to get the results.
	 */
	ENGINE_API void AddJobs(TArray<FShaderCommonCompileJob*>& NewJobs, bool bApplyCompletedShaderMapForRendering, bool bOptimizeForLowLatency, bool bRecreateComponentRenderStateOnCompletion);

	/**
	* Removes all outstanding compile jobs for the passed shader maps.
	*/
	ENGINE_API void CancelCompilation(const TCHAR* MaterialName, const TArray<int32>& ShaderMapIdsToCancel);

	/** 
	 * Blocks until completion of the requested shader maps.  
	 * This will not assign the shader map to any materials, the caller is responsible for that.
	 */
	ENGINE_API void FinishCompilation(const TCHAR* MaterialName, const TArray<int32>& ShaderMapIdsToFinishCompiling);

	/** 
	 * Blocks until completion of all async shader compiling, and assigns shader maps to relevant materials.
	 * This should be called before exit if the DDC needs to be made up to date. 
	 */
	ENGINE_API void FinishAllCompilation();


	/** 
	 * Shutdown the shader compiler manager, this will shutdown immediately and not process any more shader compile requests. 
	 */
	ENGINE_API void Shutdown();


	/** 
	 * Processes completed asynchronous shader maps, and assigns them to relevant materials.
	 * @param bLimitExecutionTime - When enabled, ProcessAsyncResults will be bandwidth throttled by ProcessGameThreadTargetTime, to limit hitching.
	 *		ProcessAsyncResults will then have to be called often to finish all shader maps (eg from Tick).  Otherwise, all compiled shader maps will be processed.
	 * @param bBlockOnGlobalShaderCompletion - When enabled, ProcessAsyncResults will block until global shader maps are complete.
	 *		This must be done before using global shaders for rendering.
	 */
	ENGINE_API void ProcessAsyncResults(bool bLimitExecutionTime, bool bBlockOnGlobalShaderCompletion);

	/**
	 * Returns true if the given shader compile worker is still running.
	 */
	static bool IsShaderCompilerWorkerRunning(FProcHandle & WorkerHandle);
};

/** The global shader compiling thread manager. */
extern ENGINE_API FShaderCompilingManager* GShaderCompilingManager;

/** The shader precompilers for each platform.  These are only set during the console shader compilation while cooking or in the PrecompileShaders commandlet. */
extern class FConsoleShaderPrecompiler* GConsoleShaderPrecompilers[SP_NumPlatforms];

/** Enqueues a shader compile job with GShaderCompilingManager. */
extern ENGINE_API void GlobalBeginCompileShader(
	const FString& DebugGroupName,
	class FVertexFactoryType* VFType,
	class FShaderType* ShaderType,
	const class FShaderPipelineType* ShaderPipelineType,
	const TCHAR* SourceFilename,
	const TCHAR* FunctionName,
	FShaderTarget Target,
	FShaderCompileJob* NewJob,
	TArray<FShaderCommonCompileJob*>& NewJobs,
	bool bAllowDevelopmentShaderCompile = true
	);

/** Implementation of the 'recompileshaders' console command.  Recompiles shaders at runtime based on various criteria. */
extern bool RecompileShaders(const TCHAR* Cmd, FOutputDevice& Ar);

/** Returns whether all global shader types containing the substring are complete and ready for rendering. if type name is null, check everything */
extern ENGINE_API bool IsGlobalShaderMapComplete(const TCHAR* TypeNameSubstring = nullptr);

/**
* Makes sure all global shaders are loaded and/or compiled for the passed in platform.
* Note: if compilation is needed, this only kicks off the compile.
*
* @param	Platform	Platform to verify global shaders for
*/
extern ENGINE_API void VerifyGlobalShaders(EShaderPlatform Platform, bool bLoadedFromCacheFile);

/**
* Forces a recompile of the global shaders.
*/
extern ENGINE_API void RecompileGlobalShaders();

/**
* Recompiles global shaders and material shaders
* rebuilds global shaders and also
* clears the cooked platform data for all materials if there is a global shader change detected
* can be slow
*/
extern ENGINE_API bool RecompileChangedShadersForPlatform(const FString& PlatformName);

/**
* Begins recompiling the specified global shader types, and flushes their bound shader states.
* FinishRecompileGlobalShaders must be called after this and before using the global shaders for anything.
*/
extern ENGINE_API void BeginRecompileGlobalShaders(const TArray<FShaderType*>& OutdatedShaderTypes, const TArray<const FShaderPipelineType*>& OutdatedShaderPipelineTypes, EShaderPlatform ShaderPlatform);

/** Finishes recompiling global shaders.  Must be called after BeginRecompileGlobalShaders. */
extern ENGINE_API void FinishRecompileGlobalShaders();

/** Called by the shader compiler to process completed global shader jobs. */
extern ENGINE_API void ProcessCompiledGlobalShaders(const TArray<FShaderCommonCompileJob*>& CompilationResults);

/**
* Saves the global shader map as a file for the target platform.
* @return the name of the file written
*/
extern ENGINE_API FString SaveGlobalShaderFile(EShaderPlatform Platform, FString SavePath, class ITargetPlatform* TargetPlatform = nullptr);

/**
* Recompiles global shaders
*
* @param PlatformName					Name of the Platform the shaders are compiled for
* @param OutputDirectory				The directory the compiled data will be stored to
* @param MaterialsToLoad				List of Materials that need to be loaded and compiled
* @param SerializedShaderResources		Serialized shader resources
* @param MeshMaterialMaps				Mesh material maps
* @param ModifiedFiles					Returns the list of modified files if not NULL
* @param bCompileChangedShaders		Whether to compile all changed shaders or the specific material that is passed
**/
extern ENGINE_API void RecompileShadersForRemote(
	const FString& PlatformName,
	EShaderPlatform ShaderPlatform,
	const FString& OutputDirectory,
	const TArray<FString>& MaterialsToLoad,
	const TArray<uint8>& SerializedShaderResources,
	TArray<uint8>* MeshMaterialMaps,
	TArray<FString>* ModifiedFiles,
	bool bCompileChangedShaders = true);

extern ENGINE_API void CompileGlobalShaderMap(bool bRefreshShaderMap=false);
extern ENGINE_API void CompileGlobalShaderMap(EShaderPlatform Platform, bool bRefreshShaderMap = false);
extern ENGINE_API void CompileGlobalShaderMap(ERHIFeatureLevel::Type InFeatureLevel, bool bRefreshShaderMap=false);

extern ENGINE_API FString GetGlobalShaderMapDDCKey();

extern ENGINE_API FString GetMaterialShaderMapDDCKey();