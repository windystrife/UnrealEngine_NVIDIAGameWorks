// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	RenderingThread.h: Rendering thread definitions.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "Async/TaskGraphInterfaces.h"

class FRHICommandListImmediate;

////////////////////////////////////
// Render thread API
////////////////////////////////////

/**
 * Whether the renderer is currently running in a separate thread.
 * If this is false, then all rendering commands will be executed immediately instead of being enqueued in the rendering command buffer.
 */
extern RENDERCORE_API bool GIsThreadedRendering;

/**
 * Whether the rendering thread should be created or not.
 * Currently set by command line parameter and by the ToggleRenderingThread console command.
 */
extern RENDERCORE_API bool GUseThreadedRendering;

extern RENDERCORE_API void SetRHIThreadEnabled(bool bEnableDedicatedThread, bool bEnableRHIOnTaskThreads);

#if (UE_BUILD_SHIPPING || UE_BUILD_TEST)
	static FORCEINLINE void CheckNotBlockedOnRenderThread() {}
#else // #if (UE_BUILD_SHIPPING || UE_BUILD_TEST)
	/** Whether the main thread is currently blocked on the rendering thread, e.g. a call to FlushRenderingCommands. */
	extern RENDERCORE_API bool GMainThreadBlockedOnRenderThread;

	/** Asserts if called from the main thread when the main thread is blocked on the rendering thread. */
	static FORCEINLINE void CheckNotBlockedOnRenderThread() { ensure(!GMainThreadBlockedOnRenderThread || !IsInGameThread()); }
#endif // #if (UE_BUILD_SHIPPING || UE_BUILD_TEST)


/** Starts the rendering thread. */
extern RENDERCORE_API void StartRenderingThread();

/** Stops the rendering thread. */
extern RENDERCORE_API void StopRenderingThread();

/**
 * Checks if the rendering thread is healthy and running.
 * If it has crashed, UE_LOG is called with the exception information.
 */
extern RENDERCORE_API void CheckRenderingThreadHealth();

/** Checks if the rendering thread is healthy and running, without crashing */
extern RENDERCORE_API bool IsRenderingThreadHealthy();

/**
 * Advances stats for the rendering thread. Called from the game thread.
 */
extern RENDERCORE_API void AdvanceRenderingThreadStatsGT( bool bDiscardCallstack, int64 StatsFrame, int32 MasterDisableChangeTagStartFrame );

/**
 * Adds a task that must be completed either before the next scene draw or a flush rendering commands
 * This can be called from any thread though it probably doesn't make sense to call it from the render thread.
 * @param TaskToAdd, task to add as a pending render thread task
 */
extern RENDERCORE_API void AddFrameRenderPrerequisite(const FGraphEventRef& TaskToAdd);

/**
 * Gather the frame render prerequisites and make sure all render commands are at least queued
 */
extern RENDERCORE_API void AdvanceFrameRenderPrerequisite();

/**
 * Waits for the rendering thread to finish executing all pending rendering commands.  Should only be used from the game thread.
 */
extern RENDERCORE_API void FlushRenderingCommands();

extern RENDERCORE_API void FlushPendingDeleteRHIResources_GameThread();
extern RENDERCORE_API void FlushPendingDeleteRHIResources_RenderThread();

extern RENDERCORE_API void TickRenderingTickables();

extern RENDERCORE_API void StartRenderCommandFenceBundler();
extern RENDERCORE_API void StopRenderCommandFenceBundler();

////////////////////////////////////
// Render thread suspension
////////////////////////////////////

/**
 * Encapsulates stopping and starting the renderthread so that other threads can manipulate graphics resources.
 */
class RENDERCORE_API FSuspendRenderingThread
{
public:
	/**
	 *	Constructor that flushes and suspends the renderthread
	 *	@param bRecreateThread	- Whether the rendering thread should be completely destroyed and recreated, or just suspended.
	 */
	FSuspendRenderingThread( bool bRecreateThread );

	/** Destructor that starts the renderthread again */
	~FSuspendRenderingThread();

private:
	/** Whether we should use a rendering thread or not */
	bool bUseRenderingThread;

	/** Whether the rendering thread was currently running or not */
	bool bWasRenderingThreadRunning;

	/** Whether the rendering thread should be completely destroyed and recreated, or just suspended */
	bool bRecreateThread;
};

/** Helper macro for safely flushing and suspending the rendering thread while manipulating graphics resources */
#define SCOPED_SUSPEND_RENDERING_THREAD(bRecreateThread)	FSuspendRenderingThread SuspendRenderingThread(bRecreateThread)

////////////////////////////////////
// Render commands
////////////////////////////////////

/** The parent class of commands stored in the rendering command queue. */
class RENDERCORE_API FRenderCommand
{
public:
	// All render commands run on the render thread
	static ENamedThreads::Type GetDesiredThread()
	{
		check(!GIsThreadedRendering || ENamedThreads::RenderThread != ENamedThreads::GameThread);
		return ENamedThreads::RenderThread;
	}

	static ESubsequentsMode::Type GetSubsequentsMode()
	{
		// Don't support tasks having dependencies on us, reduces task graph overhead tracking and dealing with subsequents
		return ESubsequentsMode::FireAndForget;
	}
};

//
// Macros for using render commands.
//
// ideally this would be inline, however that changes the module dependency situation
extern RENDERCORE_API class FRHICommandListImmediate& GetImmediateCommandList_ForRenderCommand();


DECLARE_STATS_GROUP(TEXT("Render Thread Commands"), STATGROUP_RenderThreadCommands, STATCAT_Advanced);

// Log render commands on server for debugging
#if 0 // UE_SERVER && UE_BUILD_DEBUG
	#define LogRenderCommand(TypeName)				UE_LOG(LogRHI, Warning, TEXT("Render command '%s' is being executed on a dedicated server."), TEXT(#TypeName))
#else
	#define LogRenderCommand(TypeName)
#endif 

// conditions when rendering commands are executed in the thread
#if UE_SERVER
	#define	ShouldExecuteOnRenderThread()			false
#else
	#define	ShouldExecuteOnRenderThread()			(LIKELY(GIsThreadedRendering || !IsInGameThread()))
#endif // UE_SERVER

#define TASK_FUNCTION(Code) \
		void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent) \
		{ \
			FRHICommandListImmediate& RHICmdList = GetImmediateCommandList_ForRenderCommand(); \
			Code; \
		} 

#define TASKNAME_FUNCTION(TypeName) \
		FORCEINLINE TStatId GetStatId() const \
		{ \
			RETURN_QUICK_DECLARE_CYCLE_STAT(TypeName, STATGROUP_RenderThreadCommands); \
		}

template<typename TSTR, typename LAMBDA>
class TEnqueueUniqueRenderCommandType : public FRenderCommand
{
public:
	TEnqueueUniqueRenderCommandType(LAMBDA&& InLambda) : Lambda(Forward<LAMBDA>(InLambda)) {}

	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		FRHICommandListImmediate& RHICmdList = GetImmediateCommandList_ForRenderCommand();
		Lambda(RHICmdList);
	}

	FORCEINLINE_DEBUGGABLE TStatId GetStatId() const
	{
#if STATS
		static struct FThreadSafeStaticStat<FStat_EnqueueUniqueRenderCommandType> StatPtr_EnqueueUniqueRenderCommandType;
		return StatPtr_EnqueueUniqueRenderCommandType.GetStatId();
#else
		return TStatId();
#endif
	}

private:
#if STATS
	struct FStat_EnqueueUniqueRenderCommandType
	{
		typedef FStatGroup_STATGROUP_RenderThreadCommands TGroup;
		static FORCEINLINE const char* GetStatName()
		{
			return TSTR::CStr();
		}
		static FORCEINLINE const TCHAR* GetDescription()
		{
			return TSTR::TStr();
		}
		static FORCEINLINE EStatDataType::Type GetStatType()
		{
			return EStatDataType::ST_int64;
		}
		static FORCEINLINE bool IsClearEveryFrame()
		{
			return true;
		}
		static FORCEINLINE bool IsCycleStat()
		{
			return true;
		}
		static FORCEINLINE FPlatformMemory::EMemoryCounterRegion GetMemoryRegion()
		{
			return FPlatformMemory::MCR_Invalid;
		}
	};
#endif

private:
	LAMBDA Lambda;
};

template<typename TSTR, typename LAMBDA>
FORCEINLINE_DEBUGGABLE void EnqueueUniqueRenderCommand(LAMBDA&& Lambda)
{
	typedef TEnqueueUniqueRenderCommandType<TSTR, LAMBDA> EURCType;

#if 0 // UE_SERVER && UE_BUILD_DEBUG
	UE_LOG(LogRHI, Warning, TEXT("Render command '%s' is being executed on a dedicated server."), TSTR::TStr())
#endif
	if (ShouldExecuteOnRenderThread())
	{
		CheckNotBlockedOnRenderThread();
		TGraphTask<EURCType>::CreateTask().ConstructAndDispatchWhenReady(Forward<LAMBDA>(Lambda));
	}
	else
	{
		EURCType TempCommand(Forward<LAMBDA>(Lambda));
		FScopeCycleCounter EURCMacro_Scope(TempCommand.GetStatId());
		TempCommand.DoTask(ENamedThreads::GameThread, FGraphEventRef());
	}
}

#define ENQUEUE_RENDER_COMMAND(Type) \
	struct Type##Name \
	{  \
		static const char* CStr() { return #Type; } \
		static const TCHAR* TStr() { return TEXT(#Type); } \
	}; \
	EnqueueUniqueRenderCommand<Type##Name>

template<typename LAMBDA>
FORCEINLINE_DEBUGGABLE void EnqueueUniqueRenderCommand(LAMBDA& Lambda)
{
	static_assert(sizeof(LAMBDA) == 0, "EnqueueUniqueRenderCommand enforces use of rvalue and therefore move to avoid an extra copy of the Lambda");
}

/**
 * Declares a rendering command type with 0 parameters.
 */
#define ENQUEUE_UNIQUE_RENDER_COMMAND(TypeName,Code) \
	class EURCMacro_##TypeName : public FRenderCommand \
	{ \
	public: \
		TASKNAME_FUNCTION(TypeName) \
		TASK_FUNCTION(Code) \
	}; \
	{ \
		LogRenderCommand(TypeName); \
		if(ShouldExecuteOnRenderThread()) \
		{ \
			CheckNotBlockedOnRenderThread(); \
			check(ENamedThreads::GameThread != ENamedThreads::RenderThread); \
			TGraphTask<EURCMacro_##TypeName>::CreateTask().ConstructAndDispatchWhenReady(); \
		} \
		else \
		{ \
			EURCMacro_##TypeName TempCommand; \
			FScopeCycleCounter EURCMacro_Scope(TempCommand.GetStatId()); \
			TempCommand.DoTask(ENamedThreads::GameThread, FGraphEventRef() ); \
		} \
	}

/**
 * Declares a rendering command type with 1 parameters.
 */
#define ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER_DECLARE_OPTTYPENAME(TypeName,ParamType1,ParamName1,ParamValue1,OptTypename,Code) \
	class EURCMacro_##TypeName : public FRenderCommand \
	{ \
	public: \
		EURCMacro_##TypeName(OptTypename TCallTraits<ParamType1>::ParamType In##ParamName1): \
		  ParamName1(In##ParamName1) \
		{} \
		TASK_FUNCTION(Code) \
		TASKNAME_FUNCTION(TypeName) \
	private: \
		ParamType1 ParamName1; \
	};
#define ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER_DECLARE(TypeName,ParamType1,ParamName1,ParamValue1,Code) \
	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER_DECLARE_OPTTYPENAME(TypeName,ParamType1,ParamName1,ParamValue1,,Code)

#define ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER_CREATE(TypeName,ParamType1,ParamValue1) \
	{ \
		LogRenderCommand(TypeName); \
		if(ShouldExecuteOnRenderThread()) \
		{ \
			CheckNotBlockedOnRenderThread(); \
			TGraphTask<EURCMacro_##TypeName>::CreateTask().ConstructAndDispatchWhenReady(ParamValue1); \
		} \
		else \
		{ \
			EURCMacro_##TypeName TempCommand(ParamValue1); \
			FScopeCycleCounter EURCMacro_Scope(TempCommand.GetStatId()); \
			TempCommand.DoTask(ENamedThreads::GameThread, FGraphEventRef() ); \
		} \
	}

#define ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(TypeName,ParamType1,ParamName1,ParamValue1,Code) \
	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER_DECLARE(TypeName,ParamType1,ParamName1,ParamValue1,Code) \
	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER_CREATE(TypeName,ParamType1,ParamValue1)

#define ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER_DECLARE_TEMPLATE(TypeName,TemplateParamName,ParamType1,ParamName1,ParamValue1,Code) \
	template <typename TemplateParamName> \
	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER_DECLARE_OPTTYPENAME(TypeName,ParamType1,ParamName1,ParamValue1,typename,Code)

#define ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER_CREATE_TEMPLATE(TypeName,TemplateParamName,ParamType1,ParamValue1) \
	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER_CREATE(TypeName<TemplateParamName>,ParamType1,ParamValue1)

/**
 * Declares a rendering command type with 2 parameters.
 */
#define ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER_DECLARE_OPTTYPENAME(TypeName,ParamType1,ParamName1,ParamValue1,ParamType2,ParamName2,ParamValue2,OptTypename,Code) \
	class EURCMacro_##TypeName : public FRenderCommand \
	{ \
	public: \
		EURCMacro_##TypeName(OptTypename TCallTraits<ParamType1>::ParamType In##ParamName1,OptTypename TCallTraits<ParamType2>::ParamType In##ParamName2): \
		  ParamName1(In##ParamName1), \
		  ParamName2(In##ParamName2) \
		{} \
		TASK_FUNCTION(Code) \
		TASKNAME_FUNCTION(TypeName) \
	private: \
		ParamType1 ParamName1; \
		ParamType2 ParamName2; \
	};
#define ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER_DECLARE(TypeName,ParamType1,ParamName1,ParamValue1,ParamType2,ParamName2,ParamValue2,Code) \
	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER_DECLARE_OPTTYPENAME(TypeName,ParamType1,ParamName1,ParamValue1,ParamType2,ParamName2,ParamValue2,,Code)

#define ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER_CREATE(TypeName,ParamType1,ParamValue1,ParamType2,ParamValue2) \
	{ \
		LogRenderCommand(TypeName); \
		if(ShouldExecuteOnRenderThread()) \
		{ \
			CheckNotBlockedOnRenderThread(); \
			TGraphTask<EURCMacro_##TypeName>::CreateTask().ConstructAndDispatchWhenReady(ParamValue1,ParamValue2); \
		} \
		else \
		{ \
			EURCMacro_##TypeName TempCommand(ParamValue1,ParamValue2); \
			FScopeCycleCounter EURCMacro_Scope(TempCommand.GetStatId()); \
			TempCommand.DoTask(ENamedThreads::GameThread, FGraphEventRef() ); \
		} \
	}

#define ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(TypeName,ParamType1,ParamName1,ParamValue1,ParamType2,ParamName2,ParamValue2,Code) \
	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER_DECLARE(TypeName,ParamType1,ParamName1,ParamValue1,ParamType2,ParamName2,ParamValue2,Code) \
	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER_CREATE(TypeName,ParamType1,ParamValue1,ParamType2,ParamValue2)

#define ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER_DECLARE_TEMPLATE(TypeName,TemplateParamName,ParamType1,ParamName1,ParamValue1,ParamType2,ParamName2,ParamValue2,Code) \
	template <typename TemplateParamName> \
	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER_DECLARE_OPTTYPENAME(TypeName,ParamType1,ParamName1,ParamValue1,ParamType2,ParamName2,ParamValue2,typename,Code)

#define ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER_CREATE_TEMPLATE(TypeName,TemplateParamName,ParamType1,ParamValue1,ParamType2,ParamValue2) \
	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER_CREATE(TypeName<TemplateParamName>,ParamType1,ParamValue1,ParamType2,ParamValue2)

/**
 * Declares a rendering command type with 3 parameters.
 */
#define ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER_DECLARE_OPTTYPENAME(TypeName,ParamType1,ParamName1,ParamValue1,ParamType2,ParamName2,ParamValue2,ParamType3,ParamName3,ParamValue3,OptTypename,Code) \
	class EURCMacro_##TypeName : public FRenderCommand \
	{ \
	public: \
		EURCMacro_##TypeName(OptTypename TCallTraits<ParamType1>::ParamType In##ParamName1,OptTypename TCallTraits<ParamType2>::ParamType In##ParamName2,OptTypename TCallTraits<ParamType3>::ParamType In##ParamName3): \
		  ParamName1(In##ParamName1), \
		  ParamName2(In##ParamName2), \
		  ParamName3(In##ParamName3) \
		{} \
		TASK_FUNCTION(Code) \
		TASKNAME_FUNCTION(TypeName) \
	private: \
		ParamType1 ParamName1; \
		ParamType2 ParamName2; \
		ParamType3 ParamName3; \
	};
#define ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER_DECLARE(TypeName,ParamType1,ParamName1,ParamValue1,ParamType2,ParamName2,ParamValue2,ParamType3,ParamName3,ParamValue3,Code) \
	ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER_DECLARE_OPTTYPENAME(TypeName,ParamType1,ParamName1,ParamValue1,ParamType2,ParamName2,ParamValue2,ParamType3,ParamName3,ParamValue3,,Code)

#define ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER_CREATE(TypeName,ParamType1,ParamValue1,ParamType2,ParamValue2,ParamType3,ParamValue3) \
	{ \
		LogRenderCommand(TypeName); \
		if(ShouldExecuteOnRenderThread()) \
		{ \
			CheckNotBlockedOnRenderThread(); \
			TGraphTask<EURCMacro_##TypeName>::CreateTask().ConstructAndDispatchWhenReady(ParamValue1,ParamValue2,ParamValue3); \
		} \
		else \
		{ \
			EURCMacro_##TypeName TempCommand(ParamValue1,ParamValue2,ParamValue3); \
			FScopeCycleCounter EURCMacro_Scope(TempCommand.GetStatId()); \
			TempCommand.DoTask(ENamedThreads::GameThread, FGraphEventRef() ); \
		} \
	}

#define ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER(TypeName,ParamType1,ParamName1,ParamValue1,ParamType2,ParamName2,ParamValue2,ParamType3,ParamName3,ParamValue3,Code) \
	ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER_DECLARE(TypeName,ParamType1,ParamName1,ParamValue1,ParamType2,ParamName2,ParamValue2,ParamType3,ParamName3,ParamValue3,Code) \
	ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER_CREATE(TypeName,ParamType1,ParamValue1,ParamType2,ParamValue2,ParamType3,ParamValue3)

#define ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER_DECLARE_TEMPLATE(TypeName,TemplateParamName,ParamType1,ParamName1,ParamValue1,ParamType2,ParamName2,ParamValue2,ParamType3,ParamName3,ParamValue3,Code) \
	template <typename TemplateParamName> \
	ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER_DECLARE_OPTTYPENAME(TypeName,ParamType1,ParamName1,ParamValue1,ParamType2,ParamName2,ParamValue2,ParamType3,ParamName3,ParamValue3,typename,Code)

#define ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER_CREATE_TEMPLATE(TypeName,TemplateParamName,ParamType1,ParamValue1,ParamType2,ParamValue2,ParamType3,ParamValue3) \
	ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER_CREATE(TypeName<TemplateParamName>,ParamType1,ParamValue1,ParamType2,ParamValue2,ParamType3,ParamValue3)

/**
 * Declares a rendering command type with 4 parameters.
 */
#define ENQUEUE_UNIQUE_RENDER_COMMAND_FOURPARAMETER_DECLARE_OPTTYPENAME(TypeName,ParamType1,ParamName1,ParamValue1,ParamType2,ParamName2,ParamValue2,ParamType3,ParamName3,ParamValue3,ParamType4,ParamName4,ParamValue4,OptTypename,Code) \
	class EURCMacro_##TypeName : public FRenderCommand \
	{ \
	public: \
		EURCMacro_##TypeName(OptTypename TCallTraits<ParamType1>::ParamType In##ParamName1,OptTypename TCallTraits<ParamType2>::ParamType In##ParamName2,OptTypename TCallTraits<ParamType3>::ParamType In##ParamName3,OptTypename TCallTraits<ParamType4>::ParamType In##ParamName4): \
		  ParamName1(In##ParamName1), \
		  ParamName2(In##ParamName2), \
		  ParamName3(In##ParamName3), \
		  ParamName4(In##ParamName4) \
		{} \
		TASK_FUNCTION(Code) \
		TASKNAME_FUNCTION(TypeName) \
	private: \
		ParamType1 ParamName1; \
		ParamType2 ParamName2; \
		ParamType3 ParamName3; \
		ParamType4 ParamName4; \
	};
#define ENQUEUE_UNIQUE_RENDER_COMMAND_FOURPARAMETER_DECLARE(TypeName,ParamType1,ParamName1,ParamValue1,ParamType2,ParamName2,ParamValue2,ParamType3,ParamName3,ParamValue3,ParamType4,ParamName4,ParamValue4,Code) \
	ENQUEUE_UNIQUE_RENDER_COMMAND_FOURPARAMETER_DECLARE_OPTTYPENAME(TypeName,ParamType1,ParamName1,ParamValue1,ParamType2,ParamName2,ParamValue2,ParamType3,ParamName3,ParamValue3,ParamType4,ParamName4,ParamValue4,,Code)


#define ENQUEUE_UNIQUE_RENDER_COMMAND_FOURPARAMETER_CREATE(TypeName,ParamType1,ParamValue1,ParamType2,ParamValue2,ParamType3,ParamValue3,ParamType4,ParamValue4) \
	{ \
		LogRenderCommand(TypeName); \
		if(ShouldExecuteOnRenderThread()) \
		{ \
			CheckNotBlockedOnRenderThread(); \
			TGraphTask<EURCMacro_##TypeName>::CreateTask().ConstructAndDispatchWhenReady(ParamValue1,ParamValue2,ParamValue3,ParamValue4); \
		} \
		else \
		{ \
			EURCMacro_##TypeName TempCommand(ParamValue1,ParamValue2,ParamValue3,ParamValue4); \
			FScopeCycleCounter EURCMacro_Scope(TempCommand.GetStatId()); \
			TempCommand.DoTask(ENamedThreads::GameThread, FGraphEventRef() ); \
		} \
	}

#define ENQUEUE_UNIQUE_RENDER_COMMAND_FOURPARAMETER(TypeName,ParamType1,ParamName1,ParamValue1,ParamType2,ParamName2,ParamValue2,ParamType3,ParamName3,ParamValue3,ParamType4,ParamName4,ParamValue4,Code) \
	ENQUEUE_UNIQUE_RENDER_COMMAND_FOURPARAMETER_DECLARE(TypeName,ParamType1,ParamName1,ParamValue1,ParamType2,ParamName2,ParamValue2,ParamType3,ParamName3,ParamValue3,ParamType4,ParamName4,ParamValue4,Code) \
	ENQUEUE_UNIQUE_RENDER_COMMAND_FOURPARAMETER_CREATE(TypeName,ParamType1,ParamValue1,ParamType2,ParamValue2,ParamType3,ParamValue3,ParamType4,ParamValue4)

#define ENQUEUE_UNIQUE_RENDER_COMMAND_FOURPARAMETER_DECLARE_TEMPLATE(TypeName,TemplateParamName,ParamType1,ParamName1,ParamValue1,ParamType2,ParamName2,ParamValue2,ParamType3,ParamName3,ParamValue3,ParamType4,ParamName4,ParamValue4,Code) \
	template <typename TemplateParamName> \
	ENQUEUE_UNIQUE_RENDER_COMMAND_FOURPARAMETER_DECLARE_OPTTYPENAME(TypeName,ParamType1,ParamName1,ParamValue1,ParamType2,ParamName2,ParamValue2,ParamType3,ParamName3,ParamValue3,ParamType4,ParamName4,ParamValue4,typename,Code)

#define ENQUEUE_UNIQUE_RENDER_COMMAND_FOURPARAMETER_CREATE_TEMPLATE(TypeName,TemplateParamName,ParamType1,ParamValue1,ParamType2,ParamValue2,ParamType3,ParamValue3,ParamType4,ParamValue4) \
	ENQUEUE_UNIQUE_RENDER_COMMAND_FOURPARAMETER_CREATE(TypeName<TemplateParamName>,ParamType1,ParamValue1,ParamType2,ParamValue2,ParamType3,ParamValue3,ParamType4,ParamValue4)

/**
 * Declares a rendering command type with 5 parameters.
 */
#define ENQUEUE_UNIQUE_RENDER_COMMAND_FIVEPARAMETER_DECLARE_OPTTYPENAME(TypeName,ParamType1,ParamName1,ParamValue1,ParamType2,ParamName2,ParamValue2,ParamType3,ParamName3,ParamValue3,ParamType4,ParamName4,ParamValue4,ParamType5,ParamName5,ParamValue5,OptTypename,Code) \
	class EURCMacro_##TypeName : public FRenderCommand \
	{ \
	public: \
		EURCMacro_##TypeName(OptTypename TCallTraits<ParamType1>::ParamType In##ParamName1,OptTypename TCallTraits<ParamType2>::ParamType In##ParamName2,OptTypename TCallTraits<ParamType3>::ParamType In##ParamName3,OptTypename TCallTraits<ParamType4>::ParamType In##ParamName4,OptTypename TCallTraits<ParamType5>::ParamType In##ParamName5): \
		  ParamName1(In##ParamName1), \
		  ParamName2(In##ParamName2), \
		  ParamName3(In##ParamName3), \
		  ParamName4(In##ParamName4), \
		  ParamName5(In##ParamName5) \
		{} \
		TASK_FUNCTION(Code) \
		TASKNAME_FUNCTION(TypeName) \
	private: \
		ParamType1 ParamName1; \
		ParamType2 ParamName2; \
		ParamType3 ParamName3; \
		ParamType4 ParamName4; \
		ParamType5 ParamName5; \
	};
#define ENQUEUE_UNIQUE_RENDER_COMMAND_FIVEPARAMETER_DECLARE(TypeName,ParamType1,ParamName1,ParamValue1,ParamType2,ParamName2,ParamValue2,ParamType3,ParamName3,ParamValue3,ParamType4,ParamName4,ParamValue4,ParamType5,ParamName5,ParamValue5,Code) \
	ENQUEUE_UNIQUE_RENDER_COMMAND_FIVEPARAMETER_DECLARE_OPTTYPENAME(TypeName,ParamType1,ParamName1,ParamValue1,ParamType2,ParamName2,ParamValue2,ParamType3,ParamName3,ParamValue3,ParamType4,ParamName4,ParamValue4,ParamType5,ParamName5,ParamValue5,,Code)


#define ENQUEUE_UNIQUE_RENDER_COMMAND_FIVEPARAMETER_CREATE(TypeName,ParamType1,ParamValue1,ParamType2,ParamValue2,ParamType3,ParamValue3,ParamType4,ParamValue4,ParamType5,ParamValue5) \
	{ \
		LogRenderCommand(TypeName); \
		if(ShouldExecuteOnRenderThread()) \
		{ \
			CheckNotBlockedOnRenderThread(); \
			TGraphTask<EURCMacro_##TypeName>::CreateTask().ConstructAndDispatchWhenReady(ParamValue1,ParamValue2,ParamValue3,ParamValue4,ParamValue5); \
		} \
		else \
		{ \
			EURCMacro_##TypeName TempCommand(ParamValue1,ParamValue2,ParamValue3,ParamValue4,ParamValue5); \
			FScopeCycleCounter EURCMacro_Scope(TempCommand.GetStatId()); \
			TempCommand.DoTask(ENamedThreads::GameThread, FGraphEventRef() ); \
		} \
	}

#define ENQUEUE_UNIQUE_RENDER_COMMAND_FIVEPARAMETER(TypeName,ParamType1,ParamName1,ParamValue1,ParamType2,ParamName2,ParamValue2,ParamType3,ParamName3,ParamValue3,ParamType4,ParamName4,ParamValue4,ParamType5,ParamName5,ParamValue5,Code) \
	ENQUEUE_UNIQUE_RENDER_COMMAND_FIVEPARAMETER_DECLARE(TypeName,ParamType1,ParamName1,ParamValue1,ParamType2,ParamName2,ParamValue2,ParamType3,ParamName3,ParamValue3,ParamType4,ParamName4,ParamValue4,ParamType5,ParamName5,ParamValue5,Code) \
	ENQUEUE_UNIQUE_RENDER_COMMAND_FIVEPARAMETER_CREATE(TypeName,ParamType1,ParamValue1,ParamType2,ParamValue2,ParamType3,ParamValue3,ParamType4,ParamValue4,ParamType5,ParamValue5)

#define ENQUEUE_UNIQUE_RENDER_COMMAND_FIVEPARAMETER_DECLARE_TEMPLATE(TypeName,TemplateParamName,ParamType1,ParamName1,ParamValue1,ParamType2,ParamName2,ParamValue2,ParamType3,ParamName3,ParamValue3,ParamType4,ParamName4,ParamValue4,ParamType5,ParamName5,ParamValue5,Code) \
	template <typename TemplateParamName> \
	ENQUEUE_UNIQUE_RENDER_COMMAND_FIVEPARAMETER_DECLARE_OPTTYPENAME(TypeName,ParamType1,ParamName1,ParamValue1,ParamType2,ParamName2,ParamValue2,ParamType3,ParamName3,ParamValue3,ParamType4,ParamName4,ParamValue4,ParamType5,ParamName5,ParamValue5,typename,Code)

#define ENQUEUE_UNIQUE_RENDER_COMMAND_FIVEPARAMETER_CREATE_TEMPLATE(TypeName,TemplateParamName,ParamType1,ParamValue1,ParamType2,ParamValue2,ParamType3,ParamValue3,ParamType4,ParamValue4,ParamType5,ParamValue5) \
	ENQUEUE_UNIQUE_RENDER_COMMAND_FIVEPARAMETER_CREATE(TypeName<TemplateParamName>,ParamType1,ParamValue1,ParamType2,ParamValue2,ParamType3,ParamValue3,ParamType4,ParamValue4,ParamType5,ParamValue5)

/**
 * Declares a rendering command type with 6 parameters.
 */
#define ENQUEUE_UNIQUE_RENDER_COMMAND_SIXPARAMETER_DECLARE_OPTTYPENAME(TypeName,ParamType1,ParamName1,ParamValue1,ParamType2,ParamName2,ParamValue2,ParamType3,ParamName3,ParamValue3,ParamType4,ParamName4,ParamValue4,ParamType5,ParamName5,ParamValue5,ParamType6,ParamName6,ParamValue6,OptTypename,Code) \
	class EURCMacro_##TypeName : public FRenderCommand \
	{ \
	public: \
		EURCMacro_##TypeName(OptTypename TCallTraits<ParamType1>::ParamType In##ParamName1,OptTypename TCallTraits<ParamType2>::ParamType In##ParamName2,OptTypename TCallTraits<ParamType3>::ParamType In##ParamName3,OptTypename TCallTraits<ParamType4>::ParamType In##ParamName4,OptTypename TCallTraits<ParamType5>::ParamType In##ParamName5,OptTypename TCallTraits<ParamType6>::ParamType In##ParamName6): \
		  ParamName1(In##ParamName1), \
		  ParamName2(In##ParamName2), \
		  ParamName3(In##ParamName3), \
		  ParamName4(In##ParamName4), \
		  ParamName5(In##ParamName5), \
		  ParamName6(In##ParamName6) \
		{} \
		TASK_FUNCTION(Code) \
		TASKNAME_FUNCTION(TypeName) \
	private: \
		ParamType1 ParamName1; \
		ParamType2 ParamName2; \
		ParamType3 ParamName3; \
		ParamType4 ParamName4; \
		ParamType5 ParamName5; \
		ParamType6 ParamName6; \
	};
#define ENQUEUE_UNIQUE_RENDER_COMMAND_SIXPARAMETER_DECLARE(TypeName,ParamType1,ParamName1,ParamValue1,ParamType2,ParamName2,ParamValue2,ParamType3,ParamName3,ParamValue3,ParamType4,ParamName4,ParamValue4,ParamType5,ParamName5,ParamValue5,ParamType6,ParamName6,ParamValue6,Code) \
	ENQUEUE_UNIQUE_RENDER_COMMAND_SIXPARAMETER_DECLARE_OPTTYPENAME(TypeName,ParamType1,ParamName1,ParamValue1,ParamType2,ParamName2,ParamValue2,ParamType3,ParamName3,ParamValue3,ParamType4,ParamName4,ParamValue4,ParamType5,ParamName5,ParamValue5,ParamType6,ParamName6,ParamValue6,,Code)


#define ENQUEUE_UNIQUE_RENDER_COMMAND_SIXPARAMETER_CREATE(TypeName,ParamType1,ParamValue1,ParamType2,ParamValue2,ParamType3,ParamValue3,ParamType4,ParamValue4,ParamType5,ParamValue5,ParamType6,ParamValue6) \
	{ \
		LogRenderCommand(TypeName); \
		if(ShouldExecuteOnRenderThread()) \
		{ \
			CheckNotBlockedOnRenderThread(); \
			TGraphTask<EURCMacro_##TypeName>::CreateTask().ConstructAndDispatchWhenReady(ParamValue1,ParamValue2,ParamValue3,ParamValue4,ParamValue5,ParamValue6); \
		} \
		else \
		{ \
			EURCMacro_##TypeName TempCommand(ParamValue1,ParamValue2,ParamValue3,ParamValue4,ParamValue5,ParamValue6); \
			FScopeCycleCounter EURCMacro_Scope(TempCommand.GetStatId()); \
			TempCommand.DoTask(ENamedThreads::GameThread, FGraphEventRef() ); \
		} \
	}

#define ENQUEUE_UNIQUE_RENDER_COMMAND_SIXPARAMETER(TypeName,ParamType1,ParamName1,ParamValue1,ParamType2,ParamName2,ParamValue2,ParamType3,ParamName3,ParamValue3,ParamType4,ParamName4,ParamValue4,ParamType5,ParamName5,ParamValue5,ParamType6,ParamName6,ParamValue6,Code) \
	ENQUEUE_UNIQUE_RENDER_COMMAND_SIXPARAMETER_DECLARE(TypeName,ParamType1,ParamName1,ParamValue1,ParamType2,ParamName2,ParamValue2,ParamType3,ParamName3,ParamValue3,ParamType4,ParamName4,ParamValue4,ParamType5,ParamName5,ParamValue5,ParamType6,ParamName6,ParamValue6,Code) \
	ENQUEUE_UNIQUE_RENDER_COMMAND_SIXPARAMETER_CREATE(TypeName,ParamType1,ParamValue1,ParamType2,ParamValue2,ParamType3,ParamValue3,ParamType4,ParamValue4,ParamType5,ParamValue5,ParamType6,ParamValue6)

#define ENQUEUE_UNIQUE_RENDER_COMMAND_SIXPARAMETER_DECLARE_TEMPLATE(TypeName,TemplateParamName,ParamType1,ParamName1,ParamValue1,ParamType2,ParamName2,ParamValue2,ParamType3,ParamName3,ParamValue3,ParamType4,ParamName4,ParamValue4,ParamType5,ParamName5,ParamValue5,ParamType6,ParamName6,ParamValue6,Code) \
	template <typename TemplateParamName> \
	ENQUEUE_UNIQUE_RENDER_COMMAND_SIXPARAMETER_DECLARE_OPTTYPENAME(TypeName,ParamType1,ParamName1,ParamValue1,ParamType2,ParamName2,ParamValue2,ParamType3,ParamName3,ParamValue3,ParamType4,ParamName4,ParamValue4,ParamType5,ParamName5,ParamValue5,ParamType6,ParamName6,ParamValue6,typename,Code)

#define ENQUEUE_UNIQUE_RENDER_COMMAND_SIXPARAMETER_CREATE_TEMPLATE(TypeName,TemplateParamName,ParamType1,ParamValue1,ParamType2,ParamValue2,ParamType3,ParamValue3,ParamType4,ParamValue4,ParamType5,ParamValue5,ParamType6,ParamValue6) \
	ENQUEUE_UNIQUE_RENDER_COMMAND_SIXPARAMETER_CREATE(TypeName<TemplateParamName>,ParamType1,ParamValue1,ParamType2,ParamValue2,ParamType3,ParamValue3,ParamType4,ParamValue4,ParamType5,ParamValue5,ParamType6,ParamValue6)



////////////////////////////////////
// Deferred cleanup
////////////////////////////////////

/**
 * The base class of objects that need to defer deletion until the render command queue has been flushed.
 */
class RENDERCORE_API FDeferredCleanupInterface
{
public:
	virtual void FinishCleanup() = 0;
	virtual ~FDeferredCleanupInterface() {}
};

/**
 * A set of cleanup objects which are pending deletion.
 */
class FPendingCleanupObjects
{
	TArray<FDeferredCleanupInterface*> CleanupArray;
public:
	FPendingCleanupObjects();
	RENDERCORE_API ~FPendingCleanupObjects();
};

/**
 * Adds the specified deferred cleanup object to the current set of pending cleanup objects.
 */
extern RENDERCORE_API void BeginCleanup(FDeferredCleanupInterface* CleanupObject);

/**
 * Transfers ownership of the current set of pending cleanup objects to the caller.  A new set is created for subsequent BeginCleanup calls.
 * @return A pointer to the set of pending cleanup objects.  The called is responsible for deletion.
 */
extern RENDERCORE_API FPendingCleanupObjects* GetPendingCleanupObjects();
