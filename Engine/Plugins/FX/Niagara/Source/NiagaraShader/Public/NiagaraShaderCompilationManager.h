#pragma once

#include "NiagaraShared.h"


/** Information tracked for each shader compile worker process instance. */
struct FNiagaraShaderCompileWorkerInfo
{
	/** Process handle of the worker app once launched.  Invalid handle means no process. */
	FProcHandle WorkerProcess;

	/** Tracks whether tasks have been issued to the worker. */
	bool bIssuedTasksToWorker;

	/** Whether the worker has been launched for this set of tasks. */
	bool bLaunchedWorker;

	/** Tracks whether all tasks issued to the worker have been received. */
	bool bComplete;

	/** Time at which the worker started the most recent batch of tasks. */
	double StartTime;

	/** Jobs that this worker is responsible for compiling. */
	TArray<class FNiagaraShaderCompileJob*> QueuedJobs;

	FNiagaraShaderCompileWorkerInfo() :
		bIssuedTasksToWorker(false),
		bLaunchedWorker(false),
		bComplete(false),
		StartTime(0)
	{
	}

	// warning: not virtual
	~FNiagaraShaderCompileWorkerInfo()
	{
		if (WorkerProcess.IsValid())
		{
			FPlatformProcess::TerminateProc(WorkerProcess);
			FPlatformProcess::CloseProc(WorkerProcess);
		}
	}
};

/** Stores all of the input and output information used to compile a single shader. */
class FNiagaraShaderCompileJob 
{
public:
	/** Id of the shader map this shader belongs to. */
	uint32 Id;
	/** true if the results of the shader compile have been processed. */
	bool bFinalized;
	/** Output of the shader compile */
	bool bSucceeded;
	bool bOptimizeForLowLatency;
	/** Shader type that this shader belongs to, must be valid */
	FShaderType* ShaderType;
	/** Input for the shader compile */
	FShaderCompilerInput Input;
	FShaderCompilerOutput Output;
	FString Hlsl;
	TArray< TArray<DIGPUBufferParamDescriptor> >DIBufferDescriptors;

	FNiagaraShaderCompileJob(uint32 InId, FShaderType* InShaderType, FString InHlsl) :
		Id(InId),
		bFinalized(false),
		bSucceeded(false),
		bOptimizeForLowLatency(false),
		ShaderType(InShaderType),
		Hlsl(InHlsl)
	{
	}
};



/** Results for a single compiled shader map. */
struct FNiagaraShaderMapCompileResults
{
	FNiagaraShaderMapCompileResults() :
		NumJobsQueued(0),
		bAllJobsSucceeded(true),
		bRecreateComponentRenderStateOnCompletion(false)
	{}

	int32 NumJobsQueued;
	bool bAllJobsSucceeded;
	bool bRecreateComponentRenderStateOnCompletion;
	TArray<FNiagaraShaderCompileJob*> FinishedJobs;
};


/** Results for a single compiled and finalized shader map. */
struct FNiagaraShaderMapFinalizeResults : public FNiagaraShaderMapCompileResults
{
	/** Tracks finalization progress on this shader map. */
	int32 FinalizeJobIndex;

	FNiagaraShaderMapFinalizeResults(const FNiagaraShaderMapCompileResults& InCompileResults) :
		FNiagaraShaderMapCompileResults(InCompileResults),
		FinalizeJobIndex(0)
	{}
};


// handles finished shader compile jobs, applying of the shaders to their scripts, and some error handling
//
class FNiagaraShaderCompilationManager
{
public:
	FNiagaraShaderCompilationManager();

	NIAGARASHADER_API void Tick(float DeltaSeconds = 0.0f);
	NIAGARASHADER_API void AddJobs(TArray<FNiagaraShaderCompileJob*> InNewJobs);
	NIAGARASHADER_API void ProcessAsyncResults();

	void FinishCompilation(const TCHAR* ScriptName, const TArray<int32>& ShaderMapIdsToFinishCompiling);

private:
	void ProcessCompiledNiagaraShaderMaps(TMap<int32, FNiagaraShaderMapFinalizeResults>& CompiledShaderMaps, float TimeBudget);
	void RunCompileJobs();

	TArray<FNiagaraShaderCompileJob*> JobQueue;

	/** Map from shader map Id to the compile results for that map, used to gather compiled results. */
	TMap<int32, FNiagaraShaderMapCompileResults> NiagaraShaderMapJobs;

	/** Map from shader map id to results being finalized.  Used to track shader finalizations over multiple frames. */
	TMap<int32, FNiagaraShaderMapFinalizeResults> PendingFinalizeNiagaraShaderMaps;

	TArray<struct FNiagaraShaderCompileWorkerInfo*> WorkerInfos;
};


extern NIAGARASHADER_API FNiagaraShaderCompilationManager GNiagaraShaderCompilationManager;
