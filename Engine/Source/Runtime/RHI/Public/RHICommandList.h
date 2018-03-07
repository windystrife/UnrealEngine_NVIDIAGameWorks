// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	RHICommandList.h: RHI Command List definitions for queueing up & executing later.
=============================================================================*/

#pragma once
#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "HAL/UnrealMemory.h"
#include "Templates/UnrealTemplate.h"
#include "Math/Color.h"
#include "Math/IntPoint.h"
#include "Math/IntRect.h"
#include "Math/Box2D.h"
#include "Math/PerspectiveMatrix.h"
#include "Math/TranslationMatrix.h"
#include "Math/ScaleMatrix.h"
#include "Math/Float16Color.h"
#include "HAL/ThreadSafeCounter.h"
#include "GenericPlatform/GenericPlatformProcess.h"
#include "Misc/MemStack.h"
#include "Misc/App.h"
#include "Stats/Stats.h"
#include "HAL/IConsoleManager.h"
#include "Async/TaskGraphInterfaces.h"
#include "HAL/LowLevelMemTracker.h"

class FApp;
class FBlendStateInitializerRHI;
class FGraphicsPipelineStateInitializer;
class FLastRenderTimeContainer;
class FReadSurfaceDataFlags;
class FRHICommandListBase;
class FRHIComputeShader;
class FRHIDepthRenderTargetView;
class FRHIRenderTargetView;
class FRHISetRenderTargetsInfo;
class IRHICommandContext;
class IRHIComputeContext;
struct FBoundShaderStateInput;
struct FDepthStencilStateInitializerRHI;
struct FRasterizerStateInitializerRHI;
struct FResolveParams;
struct FRHIResourceCreateInfo;
struct FRHIResourceInfo;
struct FRHIUniformBufferLayout;
struct FSamplerStateInitializerRHI;
struct FTextureMemoryStats;
class FRHIRenderPassCommandList;
class FRHIParallelRenderPassCommandList;
class FRHIRenderSubPassCommandList;
enum class EAsyncComputeBudget;
enum class EClearDepthStencil;
enum class EResourceTransitionAccess;
enum class EResourceTransitionPipeline;
class FComputePipelineState;
class FGraphicsPipelineState;

// NVCHANGE_BEGIN: Nvidia Volumetric Lighting
#if WITH_NVVOLUMETRICLIGHTING
#include "NVVolumetricLightingRHI.h"
#endif
// NVCHANGE_END: Nvidia Volumetric Lighting

DECLARE_STATS_GROUP(TEXT("RHICmdList"), STATGROUP_RHICMDLIST, STATCAT_Advanced);


// set this one to get a stat for each RHI command 
#define RHI_STATS 0

#if RHI_STATS
DECLARE_STATS_GROUP(TEXT("RHICommands"),STATGROUP_RHI_COMMANDS, STATCAT_Advanced);
#define RHISTAT(Method)	DECLARE_SCOPE_CYCLE_COUNTER(TEXT(#Method), STAT_RHI##Method, STATGROUP_RHI_COMMANDS)
#else
#define RHISTAT(Method)
#endif

extern RHI_API bool GUseRHIThread_InternalUseOnly;
extern RHI_API bool GUseRHITaskThreads_InternalUseOnly;
extern RHI_API bool GIsRunningRHIInSeparateThread_InternalUseOnly;
extern RHI_API bool GIsRunningRHIInDedicatedThread_InternalUseOnly;
extern RHI_API bool GIsRunningRHIInTaskThread_InternalUseOnly;

/**
* Whether the RHI commands are being run in a thread other than the render thread
*/
bool FORCEINLINE IsRunningRHIInSeparateThread()
{
	return GIsRunningRHIInSeparateThread_InternalUseOnly;
}

/**
* Whether the RHI commands are being run on a dedicated thread other than the render thread
*/
bool FORCEINLINE IsRunningRHIInDedicatedThread()
{
	return GIsRunningRHIInDedicatedThread_InternalUseOnly;
}

/**
* Whether the RHI commands are being run on a dedicated thread other than the render thread
*/
bool FORCEINLINE IsRunningRHIInTaskThread()
{
	return GIsRunningRHIInTaskThread_InternalUseOnly;
}




extern RHI_API bool GEnableAsyncCompute;
extern RHI_API TAutoConsoleVariable<int32> CVarRHICmdWidth;
extern RHI_API TAutoConsoleVariable<int32> CVarRHICmdFlushRenderThreadTasks;
class FRHICommandListBase;



enum class ECmdList
{
	EGfx,
	ECompute,	
};

class IRHICommandContextContainer
{
public:
	virtual ~IRHICommandContextContainer()
	{
	}

	virtual IRHICommandContext* GetContext()
	{
		return nullptr;
	}
	virtual void FinishContext()
	{
		check(0);
	}
	virtual void SubmitAndFreeContextContainer(int32 Index, int32 Num)
	{
		check(0);
	}
};

struct FRHICommandBase
{
	FRHICommandBase* Next;
	void(*ExecuteAndDestructPtr)(FRHICommandListBase& CmdList, FRHICommandBase *Cmd);
	FORCEINLINE FRHICommandBase(void(*InExecuteAndDestructPtr)(FRHICommandListBase& CmdList, FRHICommandBase *Cmd))
		: Next(nullptr)
		, ExecuteAndDestructPtr(InExecuteAndDestructPtr)
	{
	}
	FORCEINLINE void CallExecuteAndDestruct(FRHICommandListBase& CmdList)
	{
		ExecuteAndDestructPtr(CmdList, this);
	}
};

// Thread-safe allocator for GPU fences used in deferred command list execution
// Fences are stored in a ringbuffer
class RHI_API FRHICommandListFenceAllocator
{
public:
	static const int MAX_FENCE_INDICES		= 4096;
	FRHICommandListFenceAllocator()
	{
		CurrentFenceIndex = 0;
		for ( int i=0; i<MAX_FENCE_INDICES; i++)
		{
			FenceIDs[i] = 0xffffffffffffffffull;
			FenceFrameNumber[i] = 0xffffffff;
		}
	}

	uint32 AllocFenceIndex()
	{
		check(IsInRenderingThread());
		uint32 FenceIndex = ( FPlatformAtomics::InterlockedIncrement(&CurrentFenceIndex)-1 ) % MAX_FENCE_INDICES;
		check(FenceFrameNumber[FenceIndex] != GFrameNumberRenderThread);
		FenceFrameNumber[FenceIndex] = GFrameNumberRenderThread;

		return FenceIndex;
	}

	volatile uint64& GetFenceID( int32 FenceIndex )
	{
		check( FenceIndex < MAX_FENCE_INDICES );
		return FenceIDs[ FenceIndex ];
	}

private:
	volatile int32 CurrentFenceIndex;
	uint64 FenceIDs[MAX_FENCE_INDICES];
	uint32 FenceFrameNumber[MAX_FENCE_INDICES];
};

extern RHI_API FRHICommandListFenceAllocator GRHIFenceAllocator;

class RHI_API FRHICommandListBase : public FNoncopyable
{
public:
	FRHICommandListBase();
	~FRHICommandListBase();

	/** Custom new/delete with recycling */
	void* operator new(size_t Size);
	void operator delete(void *RawMemory);

	inline void Flush();
	inline bool IsImmediate();
	inline bool IsImmediateAsyncCompute();

	const int32 GetUsedMemory() const;
	void QueueAsyncCommandListSubmit(FGraphEventRef& AnyThreadCompletionEvent, class FRHICommandList* CmdList);
	void QueueAsyncCommandListSubmit(FGraphEventRef& AnyThreadCompletionEvent, class FRHIRenderSubPassCommandList* CmdList);
	void QueueParallelAsyncCommandListSubmit(FGraphEventRef* AnyThreadCompletionEvents, bool bIsPrepass, class FRHICommandList** CmdLists, int32* NumDrawsIfKnown, int32 Num, int32 MinDrawsPerTranslate, bool bSpewMerge);
	void QueueParallelAsyncCommandListSubmit(FGraphEventRef* AnyThreadCompletionEvents, bool bIsPrepass, class FRHIRenderSubPassCommandList** CmdLists, int32* NumDrawsIfKnown, int32 Num, int32 MinDrawsPerTranslate, bool bSpewMerge);
	void QueueRenderThreadCommandListSubmit(FGraphEventRef& RenderThreadCompletionEvent, class FRHICommandList* CmdList);
	void QueueAsyncPipelineStateCompile(FGraphEventRef& AsyncCompileCompletionEvent);
	void QueueCommandListSubmit(class FRHICommandList* CmdList);
	void WaitForTasks(bool bKnownToBeComplete = false);
	void WaitForDispatch();
	void WaitForRHIThreadTasks();
	void HandleRTThreadTaskCompletion(const FGraphEventRef& MyCompletionGraphEvent);

	FORCEINLINE_DEBUGGABLE void* Alloc(int32 AllocSize, int32 Alignment)
	{
		return MemManager.Alloc(AllocSize, Alignment);
	}

	template <typename T>
	FORCEINLINE_DEBUGGABLE void* Alloc()
	{
		return Alloc(sizeof(T), alignof(T));
	}


	FORCEINLINE_DEBUGGABLE TCHAR* AllocString(const TCHAR* Name)
	{
		int32 Len = FCString::Strlen(Name) + 1;
		TCHAR* NameCopy  = (TCHAR*)Alloc(Len * (int32)sizeof(TCHAR), (int32)sizeof(TCHAR));
		FCString::Strcpy(NameCopy, Len, Name);
		return NameCopy;
	}

	template <typename TCmd>
	FORCEINLINE_DEBUGGABLE void* AllocCommand()
	{
		checkSlow(!IsExecuting());
		TCmd* Result = (TCmd*)Alloc<TCmd>();
		++NumCommands;
		*CommandLink = Result;
		CommandLink = &Result->Next;
		return Result;
	}
	FORCEINLINE uint32 GetUID()
	{
		return UID;
	}
	FORCEINLINE bool HasCommands() const
	{
		return (NumCommands > 0);
	}
	FORCEINLINE bool IsExecuting() const
	{
		return bExecuting;
	}

	bool Bypass();

	FORCEINLINE void ExchangeCmdList(FRHICommandListBase& Other)
	{
		check(!RTTasks.Num() && !Other.RTTasks.Num());
		FMemory::Memswap(this, &Other, sizeof(FRHICommandListBase));
		if (CommandLink == &Other.Root)
		{
			CommandLink = &Root;
		}
		if (Other.CommandLink == &Root)
		{
			Other.CommandLink = &Other.Root;
		}
	}

	void SetContext(IRHICommandContext* InContext)
	{
		check(InContext);
		Context = InContext;
	}

	FORCEINLINE IRHICommandContext& GetContext()
	{
		checkSlow(Context);
		return *Context;
	}

	void SetComputeContext(IRHIComputeContext* InContext)
	{
		check(InContext);
		ComputeContext = InContext;
	}

	IRHIComputeContext& GetComputeContext()
	{
		checkSlow(ComputeContext);
		return *ComputeContext;
	}

	void CopyContext(FRHICommandListBase& ParentCommandList)
	{
		Context = ParentCommandList.Context;
		ComputeContext = ParentCommandList.ComputeContext;
	}

	struct FDrawUpData
	{

		uint32 PrimitiveType;
		uint32 NumPrimitives;
		uint32 NumVertices;
		uint32 VertexDataStride;
		void* OutVertexData;
		uint32 MinVertexIndex;
		uint32 NumIndices;
		uint32 IndexDataStride;
		void* OutIndexData;

		FDrawUpData()
			: PrimitiveType(PT_Num)
			, OutVertexData(nullptr)
			, OutIndexData(nullptr)
		{
		}
	};

private:
	FRHICommandBase* Root;
	FRHICommandBase** CommandLink;
	bool bExecuting;
	uint32 NumCommands;
	uint32 UID;
	IRHICommandContext* Context;
	IRHIComputeContext* ComputeContext;

	FMemStackBase MemManager; 
	FGraphEventArray RTTasks;

	void Reset();

	friend class FRHICommandListExecutor;
	friend class FRHICommandListIterator;

public:
	TStatId	ExecuteStat;
	enum class ERenderThreadContext
	{
		SceneRenderTargets,
		Num
	};
	void *RenderThreadContexts[(int32)ERenderThreadContext::Num];

protected:
	//the values of this struct must be copied when the commandlist is split 
	struct FPSOContext
	{
		uint32 CachedNumSimultanousRenderTargets = 0;
		TStaticArray<FRHIRenderTargetView, MaxSimultaneousRenderTargets> CachedRenderTargets;
		FRHIDepthRenderTargetView CachedDepthStencilTarget;
	} PSOContext;

	void CacheActiveRenderTargets(
		uint32 NewNumSimultaneousRenderTargets,
		const FRHIRenderTargetView* NewRenderTargetsRHI,
		const FRHIDepthRenderTargetView* NewDepthStencilTargetRHI
		)
	{
		PSOContext.CachedNumSimultanousRenderTargets = NewNumSimultaneousRenderTargets;

		for (uint32 RTIdx = 0; RTIdx < PSOContext.CachedNumSimultanousRenderTargets; ++RTIdx)
		{
			PSOContext.CachedRenderTargets[RTIdx] = NewRenderTargetsRHI[RTIdx];
		}

		PSOContext.CachedDepthStencilTarget = (NewDepthStencilTargetRHI) ? *NewDepthStencilTargetRHI : FRHIDepthRenderTargetView();
	}

	void CacheActiveRenderTargets(const FRHIRenderPassInfo& Info)
	{
		FRHISetRenderTargetsInfo RTInfo;
		Info.ConvertToRenderTargetsInfo(RTInfo);
		CacheActiveRenderTargets(RTInfo.NumColorRenderTargets, RTInfo.ColorRenderTarget, &RTInfo.DepthStencilRenderTarget);
	}

public:
	void CopyRenderThreadContexts(const FRHICommandListBase& ParentCommandList)
	{
		for (int32 Index = 0; ERenderThreadContext(Index) < ERenderThreadContext::Num; Index++)
		{
			RenderThreadContexts[Index] = ParentCommandList.RenderThreadContexts[Index];
		}
	}
	void SetRenderThreadContext(void* InContext, ERenderThreadContext Slot)
	{
		RenderThreadContexts[int32(Slot)] = InContext;
	}
	FORCEINLINE void* GetRenderThreadContext(ERenderThreadContext Slot)
	{
		return RenderThreadContexts[int32(Slot)];
	}

	FDrawUpData DrawUPData; 

	struct FCommonData
	{
		class FRHICommandListBase* Parent = nullptr;
		struct FLocalCmdListRenderPass* LocalRHIRenderPass = nullptr;
		struct FLocalCmdListParallelRenderPass* LocalRHIParallelRenderPass = nullptr;
		struct FLocalCmdListRenderSubPass* LocalRHIRenderSubPass = nullptr;

		enum class ECmdListType
		{
			Immediate = 1,
			RenderSubPass,
			Regular,
		};
		ECmdListType Type = ECmdListType::Regular;
	};

	FCommonData Data;
};

template<typename TCmd>
struct FRHICommand : public FRHICommandBase
{
	FORCEINLINE FRHICommand()
		: FRHICommandBase(&ExecuteAndDestruct)
	{

	}
	static FORCEINLINE void ExecuteAndDestruct(FRHICommandListBase& CmdList, FRHICommandBase *Cmd)
	{
		TCmd *ThisCmd = (TCmd*)Cmd;
		ThisCmd->Execute(CmdList);
		ThisCmd->~TCmd();
	}
};

struct  FRHICommandBeginUpdateMultiFrameResource : public FRHICommand<FRHICommandBeginUpdateMultiFrameResource>
{
	FTextureRHIParamRef Texture;
	FORCEINLINE_DEBUGGABLE FRHICommandBeginUpdateMultiFrameResource(FTextureRHIParamRef InTexture)
		: Texture(InTexture)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct  FRHICommandEndUpdateMultiFrameResource : public FRHICommand<FRHICommandEndUpdateMultiFrameResource>
{
	FTextureRHIParamRef Texture;
	FORCEINLINE_DEBUGGABLE FRHICommandEndUpdateMultiFrameResource(FTextureRHIParamRef InTexture)
		: Texture(InTexture)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct  FRHICommandBeginUpdateMultiFrameUAV : public FRHICommand<FRHICommandBeginUpdateMultiFrameResource>
{
	FUnorderedAccessViewRHIParamRef UAV;
	FORCEINLINE_DEBUGGABLE FRHICommandBeginUpdateMultiFrameUAV(FUnorderedAccessViewRHIParamRef InUAV)
		: UAV(InUAV)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct  FRHICommandEndUpdateMultiFrameUAV : public FRHICommand<FRHICommandEndUpdateMultiFrameResource>
{
	FUnorderedAccessViewRHIParamRef UAV;
	FORCEINLINE_DEBUGGABLE FRHICommandEndUpdateMultiFrameUAV(FUnorderedAccessViewRHIParamRef InUAV)
		: UAV(InUAV)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandSetRasterizerState : public FRHICommand<FRHICommandSetRasterizerState>
{
	FRasterizerStateRHIParamRef State;
	FORCEINLINE_DEBUGGABLE FRHICommandSetRasterizerState(FRasterizerStateRHIParamRef InState)
		: State(InState)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandSetDepthStencilState : public FRHICommand<FRHICommandSetDepthStencilState>
{
	FDepthStencilStateRHIParamRef State;
	uint32 StencilRef;
	FORCEINLINE_DEBUGGABLE FRHICommandSetDepthStencilState(FDepthStencilStateRHIParamRef InState, uint32 InStencilRef)
		: State(InState)
		, StencilRef(InStencilRef)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandSetStencilRef : public FRHICommand<FRHICommandSetStencilRef>
{
	uint32 StencilRef;
	FORCEINLINE_DEBUGGABLE FRHICommandSetStencilRef(uint32 InStencilRef)
		: StencilRef(InStencilRef)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

template <typename TShaderRHIParamRef, ECmdList CmdListType>
struct FRHICommandSetShaderParameter : public FRHICommand<FRHICommandSetShaderParameter<TShaderRHIParamRef, CmdListType> >
{
	TShaderRHIParamRef Shader;
	const void* NewValue;
	uint32 BufferIndex;
	uint32 BaseIndex;
	uint32 NumBytes;
	FORCEINLINE_DEBUGGABLE FRHICommandSetShaderParameter(TShaderRHIParamRef InShader, uint32 InBufferIndex, uint32 InBaseIndex, uint32 InNumBytes, const void* InNewValue)
		: Shader(InShader)
		, NewValue(InNewValue)
		, BufferIndex(InBufferIndex)
		, BaseIndex(InBaseIndex)
		, NumBytes(InNumBytes)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

template <typename TShaderRHIParamRef, ECmdList CmdListType>
struct FRHICommandSetShaderUniformBuffer : public FRHICommand<FRHICommandSetShaderUniformBuffer<TShaderRHIParamRef, CmdListType> >
{
	TShaderRHIParamRef Shader;
	uint32 BaseIndex;
	FUniformBufferRHIParamRef UniformBuffer;
	FORCEINLINE_DEBUGGABLE FRHICommandSetShaderUniformBuffer(TShaderRHIParamRef InShader, uint32 InBaseIndex, FUniformBufferRHIParamRef InUniformBuffer)
		: Shader(InShader)
		, BaseIndex(InBaseIndex)
		, UniformBuffer(InUniformBuffer)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

template <typename TShaderRHIParamRef, ECmdList CmdListType>
struct FRHICommandSetShaderTexture : public FRHICommand<FRHICommandSetShaderTexture<TShaderRHIParamRef, CmdListType> >
{
	TShaderRHIParamRef Shader;
	uint32 TextureIndex;
	FTextureRHIParamRef Texture;
	FORCEINLINE_DEBUGGABLE FRHICommandSetShaderTexture(TShaderRHIParamRef InShader, uint32 InTextureIndex, FTextureRHIParamRef InTexture)
		: Shader(InShader)
		, TextureIndex(InTextureIndex)
		, Texture(InTexture)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

template <typename TShaderRHIParamRef, ECmdList CmdListType>
struct FRHICommandSetShaderResourceViewParameter : public FRHICommand<FRHICommandSetShaderResourceViewParameter<TShaderRHIParamRef, CmdListType> >
{
	TShaderRHIParamRef Shader;
	uint32 SamplerIndex;
	FShaderResourceViewRHIParamRef SRV;
	FORCEINLINE_DEBUGGABLE FRHICommandSetShaderResourceViewParameter(TShaderRHIParamRef InShader, uint32 InSamplerIndex, FShaderResourceViewRHIParamRef InSRV)
		: Shader(InShader)
		, SamplerIndex(InSamplerIndex)
		, SRV(InSRV)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

template <typename TShaderRHIParamRef, ECmdList CmdListType>
struct FRHICommandSetUAVParameter : public FRHICommand<FRHICommandSetUAVParameter<TShaderRHIParamRef, CmdListType> >
{
	TShaderRHIParamRef Shader;
	uint32 UAVIndex;
	FUnorderedAccessViewRHIParamRef UAV;
	FORCEINLINE_DEBUGGABLE FRHICommandSetUAVParameter(TShaderRHIParamRef InShader, uint32 InUAVIndex, FUnorderedAccessViewRHIParamRef InUAV)
		: Shader(InShader)
		, UAVIndex(InUAVIndex)
		, UAV(InUAV)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

template <typename TShaderRHIParamRef, ECmdList CmdListType>
struct FRHICommandSetUAVParameter_IntialCount : public FRHICommand<FRHICommandSetUAVParameter_IntialCount<TShaderRHIParamRef, CmdListType> >
{
	TShaderRHIParamRef Shader;
	uint32 UAVIndex;
	FUnorderedAccessViewRHIParamRef UAV;
	uint32 InitialCount;
	FORCEINLINE_DEBUGGABLE FRHICommandSetUAVParameter_IntialCount(TShaderRHIParamRef InShader, uint32 InUAVIndex, FUnorderedAccessViewRHIParamRef InUAV, uint32 InInitialCount)
		: Shader(InShader)
		, UAVIndex(InUAVIndex)
		, UAV(InUAV)
		, InitialCount(InInitialCount)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

template <typename TShaderRHIParamRef, ECmdList CmdListType>
struct FRHICommandSetShaderSampler : public FRHICommand<FRHICommandSetShaderSampler<TShaderRHIParamRef, CmdListType> >
{
	TShaderRHIParamRef Shader;
	uint32 SamplerIndex;
	FSamplerStateRHIParamRef Sampler;
	FORCEINLINE_DEBUGGABLE FRHICommandSetShaderSampler(TShaderRHIParamRef InShader, uint32 InSamplerIndex, FSamplerStateRHIParamRef InSampler)
		: Shader(InShader)
		, SamplerIndex(InSamplerIndex)
		, Sampler(InSampler)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

// WaveWorks Start
struct FRHICommandSetWaveWorksState : public FRHICommand < FRHICommandSetWaveWorksState >
{
	FWaveWorksRHIParamRef State;
	FMatrix ViewMatrix;
	TArray<uint32> ShaderInputMappings;
	FORCEINLINE_DEBUGGABLE FRHICommandSetWaveWorksState(FWaveWorksRHIParamRef InState, const FMatrix& InViewMatrix, const TArray<uint32>& InShaderInputMappings)
		: State(InState)
		, ViewMatrix(InViewMatrix)
		, ShaderInputMappings(InShaderInputMappings)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};
// WaveWorks End

struct FRHICommandDrawPrimitive : public FRHICommand<FRHICommandDrawPrimitive>
{
	uint32 PrimitiveType;
	uint32 BaseVertexIndex;
	uint32 NumPrimitives;
	uint32 NumInstances;
	FORCEINLINE_DEBUGGABLE FRHICommandDrawPrimitive(uint32 InPrimitiveType, uint32 InBaseVertexIndex, uint32 InNumPrimitives, uint32 InNumInstances)
		: PrimitiveType(InPrimitiveType)
		, BaseVertexIndex(InBaseVertexIndex)
		, NumPrimitives(InNumPrimitives)
		, NumInstances(InNumInstances)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandDrawIndexedPrimitive : public FRHICommand<FRHICommandDrawIndexedPrimitive>
{
	FIndexBufferRHIParamRef IndexBuffer;
	uint32 PrimitiveType;
	int32 BaseVertexIndex;
	uint32 FirstInstance;
	uint32 NumVertices;
	uint32 StartIndex;
	uint32 NumPrimitives;
	uint32 NumInstances;
	FORCEINLINE_DEBUGGABLE FRHICommandDrawIndexedPrimitive(FIndexBufferRHIParamRef InIndexBuffer, uint32 InPrimitiveType, int32 InBaseVertexIndex, uint32 InFirstInstance, uint32 InNumVertices, uint32 InStartIndex, uint32 InNumPrimitives, uint32 InNumInstances)
		: IndexBuffer(InIndexBuffer)
		, PrimitiveType(InPrimitiveType)
		, BaseVertexIndex(InBaseVertexIndex)
		, FirstInstance(InFirstInstance)
		, NumVertices(InNumVertices)
		, StartIndex(InStartIndex)
		, NumPrimitives(InNumPrimitives)
		, NumInstances(InNumInstances)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandSetBoundShaderState : public FRHICommand<FRHICommandSetBoundShaderState>
{
	FBoundShaderStateRHIParamRef BoundShaderState;
	FORCEINLINE_DEBUGGABLE FRHICommandSetBoundShaderState(FBoundShaderStateRHIParamRef InBoundShaderState)
		: BoundShaderState(InBoundShaderState)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandSetBlendState : public FRHICommand<FRHICommandSetBlendState>
{
	FBlendStateRHIParamRef State;
	FLinearColor BlendFactor;
	FORCEINLINE_DEBUGGABLE FRHICommandSetBlendState(FBlendStateRHIParamRef InState, const FLinearColor& InBlendFactor)
		: State(InState)
		, BlendFactor(InBlendFactor)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandSetBlendFactor : public FRHICommand<FRHICommandSetBlendFactor>
{
	FLinearColor BlendFactor;
	FORCEINLINE_DEBUGGABLE FRHICommandSetBlendFactor(const FLinearColor& InBlendFactor)
		: BlendFactor(InBlendFactor)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandSetStreamSourceDEPRECATED : public FRHICommand<FRHICommandSetStreamSourceDEPRECATED>
{
	uint32 StreamIndex;
	FVertexBufferRHIParamRef VertexBuffer;
	uint32 Stride;
	uint32 Offset;
	FORCEINLINE_DEBUGGABLE FRHICommandSetStreamSourceDEPRECATED(uint32 InStreamIndex, FVertexBufferRHIParamRef InVertexBuffer, uint32 InStride, uint32 InOffset)
		: StreamIndex(InStreamIndex)
		, VertexBuffer(InVertexBuffer)
		, Stride(InStride)
		, Offset(InOffset)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandSetStreamSource : public FRHICommand<FRHICommandSetStreamSource>
{
	uint32 StreamIndex;
	FVertexBufferRHIParamRef VertexBuffer;
	uint32 Offset;
	FORCEINLINE_DEBUGGABLE FRHICommandSetStreamSource(uint32 InStreamIndex, FVertexBufferRHIParamRef InVertexBuffer, uint32 InOffset)
		: StreamIndex(InStreamIndex)
		, VertexBuffer(InVertexBuffer)
		, Offset(InOffset)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandSetViewport : public FRHICommand<FRHICommandSetViewport>
{
	uint32 MinX;
	uint32 MinY;
	float MinZ;
	uint32 MaxX;
	uint32 MaxY;
	float MaxZ;
	FORCEINLINE_DEBUGGABLE FRHICommandSetViewport(uint32 InMinX, uint32 InMinY, float InMinZ, uint32 InMaxX, uint32 InMaxY, float InMaxZ)
		: MinX(InMinX)
		, MinY(InMinY)
		, MinZ(InMinZ)
		, MaxX(InMaxX)
		, MaxY(InMaxY)
		, MaxZ(InMaxZ)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandSetStereoViewport : public FRHICommand<FRHICommandSetStereoViewport>
{
	uint32 LeftMinX;
	uint32 RightMinX;
	uint32 LeftMinY;
	uint32 RightMinY;
	float MinZ;
	uint32 LeftMaxX;
	uint32 RightMaxX;
	uint32 LeftMaxY;
	uint32 RightMaxY;
	float MaxZ;
	FORCEINLINE_DEBUGGABLE FRHICommandSetStereoViewport(uint32 InLeftMinX, uint32 InRightMinX, uint32 InLeftMinY, uint32 InRightMinY, float InMinZ, uint32 InLeftMaxX, uint32 InRightMaxX, uint32 InLeftMaxY, uint32 InRightMaxY, float InMaxZ)
		: LeftMinX(InLeftMinX)
		, RightMinX(InRightMinX)
		, LeftMinY(InLeftMinY)
		, RightMinY(InRightMinY)
		, MinZ(InMinZ)
		, LeftMaxX(InLeftMaxX)
		, RightMaxX(InRightMaxX)
		, LeftMaxY(InLeftMaxY)
		, RightMaxY(InRightMaxY)
		, MaxZ(InMaxZ)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandSetScissorRect : public FRHICommand<FRHICommandSetScissorRect>
{
	bool bEnable;
	uint32 MinX;
	uint32 MinY;
	uint32 MaxX;
	uint32 MaxY;
	FORCEINLINE_DEBUGGABLE FRHICommandSetScissorRect(bool InbEnable, uint32 InMinX, uint32 InMinY, uint32 InMaxX, uint32 InMaxY)
		: bEnable(InbEnable)
		, MinX(InMinX)
		, MinY(InMinY)
		, MaxX(InMaxX)
		, MaxY(InMaxY)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandSetRenderTargets : public FRHICommand<FRHICommandSetRenderTargets>
{
	uint32 NewNumSimultaneousRenderTargets;
	FRHIRenderTargetView NewRenderTargetsRHI[MaxSimultaneousRenderTargets];
	FRHIDepthRenderTargetView NewDepthStencilTarget;
	uint32 NewNumUAVs;
	FUnorderedAccessViewRHIParamRef UAVs[MaxSimultaneousUAVs];

	FORCEINLINE_DEBUGGABLE FRHICommandSetRenderTargets(
		uint32 InNewNumSimultaneousRenderTargets,
		const FRHIRenderTargetView* InNewRenderTargetsRHI,
		const FRHIDepthRenderTargetView* InNewDepthStencilTargetRHI,
		uint32 InNewNumUAVs,
		const FUnorderedAccessViewRHIParamRef* InUAVs
		)
		: NewNumSimultaneousRenderTargets(InNewNumSimultaneousRenderTargets)
		, NewNumUAVs(InNewNumUAVs)

	{
		check(InNewNumSimultaneousRenderTargets <= MaxSimultaneousRenderTargets && InNewNumUAVs <= MaxSimultaneousUAVs);
		for (uint32 Index = 0; Index < NewNumSimultaneousRenderTargets; Index++)
		{
			NewRenderTargetsRHI[Index] = InNewRenderTargetsRHI[Index];
		}
		for (uint32 Index = 0; Index < NewNumUAVs; Index++)
		{
			UAVs[Index] = InUAVs[Index];
		}
		if (InNewDepthStencilTargetRHI)
		{
			NewDepthStencilTarget = *InNewDepthStencilTargetRHI;
		}		
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FLocalCmdListRenderPass
{
	TRefCountPtr<FRHIRenderPass> RenderPass;

	//~FLocalCmdListRenderPass()
	//{
	//	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("~FLocalCmdListRenderPass %p, RP %p\n"), this, RenderPass.GetReference());
	//}
};

struct FRHICommandBeginRenderPass : public FRHICommand<FRHICommandBeginRenderPass>
{
	FRHIRenderPassInfo Info;
	FLocalCmdListRenderPass* LocalRenderPass;
	const TCHAR* Name;

	FRHICommandBeginRenderPass(const FRHIRenderPassInfo& InInfo, FLocalCmdListRenderPass* InLocalRenderPass, const TCHAR* InName)
		: Info(InInfo)
		, LocalRenderPass(InLocalRenderPass)
		, Name(InName)
	{
	}

	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandEndRenderPass : public FRHICommand<FRHICommandEndRenderPass>
{
	FLocalCmdListRenderPass* LocalRenderPass;

	FRHICommandEndRenderPass(FLocalCmdListRenderPass* InLocalRenderPass)
		: LocalRenderPass(InLocalRenderPass)
	{
	}

	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FLocalCmdListParallelRenderPass
{
	TRefCountPtr<FRHIParallelRenderPass> RenderPass;
};

struct FRHICommandBeginParallelRenderPass : public FRHICommand<FRHICommandBeginParallelRenderPass>
{
	FRHIRenderPassInfo Info;
	FLocalCmdListParallelRenderPass* LocalRenderPass;
	const TCHAR* Name;

	FRHICommandBeginParallelRenderPass(const FRHIRenderPassInfo& InInfo, FLocalCmdListParallelRenderPass* InLocalRenderPass, const TCHAR* InName)
		: Info(InInfo)
		, LocalRenderPass(InLocalRenderPass)
		, Name(InName)
	{
	}

	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandEndParallelRenderPass : public FRHICommand<FRHICommandEndParallelRenderPass>
{
	FLocalCmdListParallelRenderPass* LocalRenderPass;

	FRHICommandEndParallelRenderPass(FLocalCmdListParallelRenderPass* InLocalRenderPass)
		: LocalRenderPass(InLocalRenderPass)
	{
	}

	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FLocalCmdListRenderSubPass
{
	TRefCountPtr<FRHIRenderSubPass> RenderSubPass;
};

struct FRHICommandBeginRenderSubPass : public FRHICommand<FRHICommandBeginRenderSubPass>
{
	FLocalCmdListParallelRenderPass* LocalRenderPass;
	FLocalCmdListRenderSubPass* LocalRenderSubPass;

	FRHICommandBeginRenderSubPass(FLocalCmdListParallelRenderPass* InLocalRenderPass, FLocalCmdListRenderSubPass* InLocalRenderSubPass)
		: LocalRenderPass(InLocalRenderPass)
		, LocalRenderSubPass(InLocalRenderSubPass)
	{
	}

	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandEndRenderSubPass : public FRHICommand<FRHICommandEndRenderSubPass>
{
	FLocalCmdListParallelRenderPass* LocalRenderPass;
	FLocalCmdListRenderSubPass* LocalRenderSubPass;

	FRHICommandEndRenderSubPass(FLocalCmdListParallelRenderPass* InLocalRenderPass, FLocalCmdListRenderSubPass* InLocalRenderSubPass)
		: LocalRenderPass(InLocalRenderPass)
		, LocalRenderSubPass(InLocalRenderSubPass)
	{
	}

	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandSetRenderTargetsAndClear : public FRHICommand<FRHICommandSetRenderTargetsAndClear>
{
	FRHISetRenderTargetsInfo RenderTargetsInfo;

	FORCEINLINE_DEBUGGABLE FRHICommandSetRenderTargetsAndClear(const FRHISetRenderTargetsInfo& InRenderTargetsInfo) :
		RenderTargetsInfo(InRenderTargetsInfo)
	{
	}

	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandBindClearMRTValues : public FRHICommand<FRHICommandBindClearMRTValues>
{
	bool bClearColor;
	bool bClearDepth;
	bool bClearStencil;

	FORCEINLINE_DEBUGGABLE FRHICommandBindClearMRTValues(
		bool InbClearColor,
		bool InbClearDepth,
		bool InbClearStencil
		) 
		: bClearColor(InbClearColor)
		, bClearDepth(InbClearDepth)
		, bClearStencil(InbClearStencil)
	{
	}	

	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandEndDrawPrimitiveUP : public FRHICommand<FRHICommandEndDrawPrimitiveUP>
{
	uint32 PrimitiveType;
	uint32 NumPrimitives;
	uint32 NumVertices;
	uint32 VertexDataStride;
	void* OutVertexData;

	FORCEINLINE_DEBUGGABLE FRHICommandEndDrawPrimitiveUP(uint32 InPrimitiveType, uint32 InNumPrimitives, uint32 InNumVertices, uint32 InVertexDataStride, void* InOutVertexData)
		: PrimitiveType(InPrimitiveType)
		, NumPrimitives(InNumPrimitives)
		, NumVertices(InNumVertices)
		, VertexDataStride(InVertexDataStride)
		, OutVertexData(InOutVertexData)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};


struct FRHICommandEndDrawIndexedPrimitiveUP : public FRHICommand<FRHICommandEndDrawIndexedPrimitiveUP>
{
	uint32 PrimitiveType;
	uint32 NumPrimitives;
	uint32 NumVertices;
	uint32 VertexDataStride;
	void* OutVertexData;
	uint32 MinVertexIndex;
	uint32 NumIndices;
	uint32 IndexDataStride;
	void* OutIndexData;

	FORCEINLINE_DEBUGGABLE FRHICommandEndDrawIndexedPrimitiveUP(uint32 InPrimitiveType, uint32 InNumPrimitives, uint32 InNumVertices, uint32 InVertexDataStride, void* InOutVertexData, uint32 InMinVertexIndex, uint32 InNumIndices, uint32 InIndexDataStride, void* InOutIndexData)
		: PrimitiveType(InPrimitiveType)
		, NumPrimitives(InNumPrimitives)
		, NumVertices(InNumVertices)
		, VertexDataStride(InVertexDataStride)
		, OutVertexData(InOutVertexData)
		, MinVertexIndex(InMinVertexIndex)
		, NumIndices(InNumIndices)
		, IndexDataStride(InIndexDataStride)
		, OutIndexData(InOutIndexData)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

template<ECmdList CmdListType>
struct FRHICommandSetComputeShader : public FRHICommand<FRHICommandSetComputeShader<CmdListType>>
{
	FComputeShaderRHIParamRef ComputeShader;
	FORCEINLINE_DEBUGGABLE FRHICommandSetComputeShader(FComputeShaderRHIParamRef InComputeShader)
		: ComputeShader(InComputeShader)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

template<ECmdList CmdListType>
struct FRHICommandSetComputePipelineState : public FRHICommand<FRHICommandSetComputePipelineState<CmdListType>>
{
	FComputePipelineState* ComputePipelineState;
	FORCEINLINE_DEBUGGABLE FRHICommandSetComputePipelineState(FComputePipelineState* InComputePipelineState)
		: ComputePipelineState(InComputePipelineState)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandSetGraphicsPipelineState : public FRHICommand<FRHICommandSetGraphicsPipelineState>
{
	FGraphicsPipelineState* GraphicsPipelineState;
	FORCEINLINE_DEBUGGABLE FRHICommandSetGraphicsPipelineState(FGraphicsPipelineState* InGraphicsPipelineState)
		: GraphicsPipelineState(InGraphicsPipelineState)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

template<ECmdList CmdListType>
struct FRHICommandDispatchComputeShader : public FRHICommand<FRHICommandDispatchComputeShader<CmdListType>>
{
	uint32 ThreadGroupCountX;
	uint32 ThreadGroupCountY;
	uint32 ThreadGroupCountZ;
	FORCEINLINE_DEBUGGABLE FRHICommandDispatchComputeShader(uint32 InThreadGroupCountX, uint32 InThreadGroupCountY, uint32 InThreadGroupCountZ)
		: ThreadGroupCountX(InThreadGroupCountX)
		, ThreadGroupCountY(InThreadGroupCountY)
		, ThreadGroupCountZ(InThreadGroupCountZ)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

template<ECmdList CmdListType>
struct FRHICommandDispatchIndirectComputeShader : public FRHICommand<FRHICommandDispatchIndirectComputeShader<CmdListType>>
{
	FVertexBufferRHIParamRef ArgumentBuffer;
	uint32 ArgumentOffset;
	FORCEINLINE_DEBUGGABLE FRHICommandDispatchIndirectComputeShader(FVertexBufferRHIParamRef InArgumentBuffer, uint32 InArgumentOffset)
		: ArgumentBuffer(InArgumentBuffer)
		, ArgumentOffset(InArgumentOffset)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandAutomaticCacheFlushAfterComputeShader : public FRHICommand<FRHICommandAutomaticCacheFlushAfterComputeShader>
{
	bool bEnable;
	FORCEINLINE_DEBUGGABLE FRHICommandAutomaticCacheFlushAfterComputeShader(bool InbEnable)
		: bEnable(InbEnable)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandFlushComputeShaderCache : public FRHICommand<FRHICommandFlushComputeShaderCache>
{
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandDrawPrimitiveIndirect : public FRHICommand<FRHICommandDrawPrimitiveIndirect>
{
	FVertexBufferRHIParamRef ArgumentBuffer;
	uint32 PrimitiveType;
	uint32 ArgumentOffset;
	FORCEINLINE_DEBUGGABLE FRHICommandDrawPrimitiveIndirect(uint32 InPrimitiveType, FVertexBufferRHIParamRef InArgumentBuffer, uint32 InArgumentOffset)
		: ArgumentBuffer(InArgumentBuffer)
		, PrimitiveType(InPrimitiveType)
		, ArgumentOffset(InArgumentOffset)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandDrawIndexedIndirect : public FRHICommand<FRHICommandDrawIndexedIndirect>
{
	FIndexBufferRHIParamRef IndexBufferRHI;
	uint32 PrimitiveType;
	FStructuredBufferRHIParamRef ArgumentsBufferRHI;
	uint32 DrawArgumentsIndex;
	uint32 NumInstances;

	FORCEINLINE_DEBUGGABLE FRHICommandDrawIndexedIndirect(FIndexBufferRHIParamRef InIndexBufferRHI, uint32 InPrimitiveType, FStructuredBufferRHIParamRef InArgumentsBufferRHI, uint32 InDrawArgumentsIndex, uint32 InNumInstances)
		: IndexBufferRHI(InIndexBufferRHI)
		, PrimitiveType(InPrimitiveType)
		, ArgumentsBufferRHI(InArgumentsBufferRHI)
		, DrawArgumentsIndex(InDrawArgumentsIndex)
		, NumInstances(InNumInstances)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandDrawIndexedPrimitiveIndirect : public FRHICommand<FRHICommandDrawIndexedPrimitiveIndirect>
{
	FIndexBufferRHIParamRef IndexBuffer;
	FVertexBufferRHIParamRef ArgumentsBuffer;
	uint32 PrimitiveType;
	uint32 ArgumentOffset;

	FORCEINLINE_DEBUGGABLE FRHICommandDrawIndexedPrimitiveIndirect(uint32 InPrimitiveType, FIndexBufferRHIParamRef InIndexBuffer, FVertexBufferRHIParamRef InArgumentsBuffer, uint32 InArgumentOffset)
		: IndexBuffer(InIndexBuffer)
		, ArgumentsBuffer(InArgumentsBuffer)
		, PrimitiveType(InPrimitiveType)
		, ArgumentOffset(InArgumentOffset)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandEnableDepthBoundsTest : public FRHICommand<FRHICommandEnableDepthBoundsTest>
{
	bool bEnable;
	float MinDepth;
	float MaxDepth;

	FORCEINLINE_DEBUGGABLE FRHICommandEnableDepthBoundsTest(bool InbEnable, float InMinDepth, float InMaxDepth)
		: bEnable(InbEnable)
		, MinDepth(InMinDepth)
		, MaxDepth(InMaxDepth)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandClearTinyUAV : public FRHICommand<FRHICommandClearTinyUAV>
{
	FUnorderedAccessViewRHIParamRef UnorderedAccessViewRHI;
	uint32 Values[4];

	FORCEINLINE_DEBUGGABLE FRHICommandClearTinyUAV(FUnorderedAccessViewRHIParamRef InUnorderedAccessViewRHI, const uint32* InValues)
		: UnorderedAccessViewRHI(InUnorderedAccessViewRHI)
	{
		Values[0] = InValues[0];
		Values[1] = InValues[1];
		Values[2] = InValues[2];
		Values[3] = InValues[3];
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandCopyToResolveTarget : public FRHICommand<FRHICommandCopyToResolveTarget>
{
	FResolveParams ResolveParams;
	FTextureRHIParamRef SourceTexture;
	FTextureRHIParamRef DestTexture;
	bool bKeepOriginalSurface;

	FORCEINLINE_DEBUGGABLE FRHICommandCopyToResolveTarget(FTextureRHIParamRef InSourceTexture, FTextureRHIParamRef InDestTexture, bool InbKeepOriginalSurface, const FResolveParams& InResolveParams)
		: ResolveParams(InResolveParams)
		, SourceTexture(InSourceTexture)
		, DestTexture(InDestTexture)
		, bKeepOriginalSurface(InbKeepOriginalSurface)
	{
		ensure(SourceTexture);
		ensure(DestTexture);
		ensure(SourceTexture->GetTexture2D() || SourceTexture->GetTexture3D() || SourceTexture->GetTextureCube());
		ensure(DestTexture->GetTexture2D() || DestTexture->GetTexture3D() || DestTexture->GetTextureCube());
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandCopyTexture : public FRHICommand<FRHICommandCopyTexture>
{
	FResolveParams ResolveParams;
	FTextureRHIParamRef SourceTexture;
	FTextureRHIParamRef DestTexture;

	FORCEINLINE_DEBUGGABLE FRHICommandCopyTexture(FTextureRHIParamRef InSourceTexture, FTextureRHIParamRef InDestTexture, const FResolveParams& InResolveParams)
		: ResolveParams(InResolveParams)
		, SourceTexture(InSourceTexture)
		, DestTexture(InDestTexture)
	{
		ensure(SourceTexture);
		ensure(DestTexture);
		ensure(SourceTexture->GetTexture2D() || SourceTexture->GetTexture3D() || SourceTexture->GetTextureCube());
		ensure(DestTexture->GetTexture2D() || DestTexture->GetTexture3D() || DestTexture->GetTextureCube());
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandTransitionTextures : public FRHICommand<FRHICommandTransitionTextures>
{
	int32 NumTextures;
	FTextureRHIParamRef* Textures; // Pointer to an array of textures, allocated inline with the command list
	EResourceTransitionAccess TransitionType;
	FORCEINLINE_DEBUGGABLE FRHICommandTransitionTextures(EResourceTransitionAccess InTransitionType, FTextureRHIParamRef* InTextures, int32 InNumTextures)
		: NumTextures(InNumTextures)
		, Textures(InTextures)
		, TransitionType(InTransitionType)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandTransitionTexturesArray : public FRHICommand<FRHICommandTransitionTexturesArray>
{	
	TArray<FTextureRHIParamRef>& Textures;
	EResourceTransitionAccess TransitionType;
	FORCEINLINE_DEBUGGABLE FRHICommandTransitionTexturesArray(EResourceTransitionAccess InTransitionType, TArray<FTextureRHIParamRef>& InTextures)
		: Textures(InTextures)
		, TransitionType(InTransitionType)
	{		
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

template<ECmdList CmdListType>
struct FRHICommandTransitionUAVs : public FRHICommand<FRHICommandTransitionUAVs<CmdListType>>
{
	int32 NumUAVs;
	FUnorderedAccessViewRHIParamRef* UAVs; // Pointer to an array of UAVs, allocated inline with the command list
	EResourceTransitionAccess TransitionType;
	EResourceTransitionPipeline TransitionPipeline;
	FComputeFenceRHIParamRef WriteFence;

	FORCEINLINE_DEBUGGABLE FRHICommandTransitionUAVs(EResourceTransitionAccess InTransitionType, EResourceTransitionPipeline InTransitionPipeline, FUnorderedAccessViewRHIParamRef* InUAVs, int32 InNumUAVs, FComputeFenceRHIParamRef InWriteFence)
		: NumUAVs(InNumUAVs)
		, UAVs(InUAVs)
		, TransitionType(InTransitionType)
		, TransitionPipeline(InTransitionPipeline)
		, WriteFence(InWriteFence)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

template<ECmdList CmdListType>
struct FRHICommandSetAsyncComputeBudget : public FRHICommand<FRHICommandSetAsyncComputeBudget<CmdListType>>
{
	EAsyncComputeBudget Budget;

	FORCEINLINE_DEBUGGABLE FRHICommandSetAsyncComputeBudget(EAsyncComputeBudget InBudget)
		: Budget(InBudget)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

template<ECmdList CmdListType>
struct FRHICommandWaitComputeFence : public FRHICommand<FRHICommandWaitComputeFence<CmdListType>>
{
	FComputeFenceRHIParamRef WaitFence;

	FORCEINLINE_DEBUGGABLE FRHICommandWaitComputeFence(FComputeFenceRHIParamRef InWaitFence)
		: WaitFence(InWaitFence)
	{		
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandClearColorTexture : public FRHICommand<FRHICommandClearColorTexture>
{
	FTextureRHIParamRef Texture;
	FLinearColor Color;

	FORCEINLINE_DEBUGGABLE FRHICommandClearColorTexture(
		FTextureRHIParamRef InTexture,
		const FLinearColor& InColor
		)
		: Texture(InTexture)
		, Color(InColor)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandClearDepthStencilTexture : public FRHICommand<FRHICommandClearDepthStencilTexture>
{
	FTextureRHIParamRef Texture;
	float Depth;
	uint32 Stencil;
	EClearDepthStencil ClearDepthStencil;

	FORCEINLINE_DEBUGGABLE FRHICommandClearDepthStencilTexture(
		FTextureRHIParamRef InTexture,
		EClearDepthStencil InClearDepthStencil,
		float InDepth,
		uint32 InStencil
	)
		: Texture(InTexture)
		, Depth(InDepth)
		, Stencil(InStencil)
		, ClearDepthStencil(InClearDepthStencil)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandClearColorTextures : public FRHICommand<FRHICommandClearColorTextures>
{
	FLinearColor ColorArray[MaxSimultaneousRenderTargets];
	FTextureRHIParamRef Textures[MaxSimultaneousRenderTargets];
	int32 NumClearColors;

	FORCEINLINE_DEBUGGABLE FRHICommandClearColorTextures(
		int32 InNumClearColors,
		FTextureRHIParamRef* InTextures,
		const FLinearColor* InColorArray
		)
		: NumClearColors(InNumClearColors)
	{
		check(InNumClearColors <= MaxSimultaneousRenderTargets);
		for (int32 Index = 0; Index < InNumClearColors; Index++)
		{
			ColorArray[Index] = InColorArray[Index];
			Textures[Index] = InTextures[Index];
		}
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

// WaveWorks Begin
struct FQuadTreeWaveWorksArgsWorkArea
{
#if DO_CHECK // the below variables are used in check(), which can be enabled in Shipping builds (see Build.h)
	FRHICommandListBase* CheckCmdList;
	int32 UID;
#endif

	FWaveWorksRHIRef WaveWorks;
	FMatrix ViewMatrix;
	FMatrix ProjMatrix;
	TArray<uint32> ShaderInputMappings;
	struct GFSDK_WaveWorks_Quadtree* QuadTreeHandle;

	FORCEINLINE_DEBUGGABLE FQuadTreeWaveWorksArgsWorkArea(
		FRHICommandListBase* InCheckCmdList,
		FWaveWorksRHIRef _WaveWorks,
		struct GFSDK_WaveWorks_Quadtree* _QuadTreeHandle,
		FMatrix _ViewMatrix,
		FMatrix _ProjMatrix,
		TArray<uint32> _ShaderInputMappings
		)
		: WaveWorks(_WaveWorks)
		, QuadTreeHandle(_QuadTreeHandle)
		, ViewMatrix(_ViewMatrix)
		, ProjMatrix(_ProjMatrix)
		, ShaderInputMappings(_ShaderInputMappings)
#if DO_CHECK
		, CheckCmdList(InCheckCmdList)
		, UID(InCheckCmdList->GetUID())
#endif
	{
	}
};
// WaveWorks End

// WaveWorks Begin
struct FRHICommandBuildDrawQuadTreeWaveWorks : public FRHICommand<FRHICommandBuildDrawQuadTreeWaveWorks>
{
	FQuadTreeWaveWorksArgsWorkArea WorkArea;

	FORCEINLINE_DEBUGGABLE FRHICommandBuildDrawQuadTreeWaveWorks(
		FRHICommandListBase* CheckCmdList,
		FWaveWorksRHIRef WaveWorks,
		struct GFSDK_WaveWorks_Quadtree* QuadTreeHandle,
		FMatrix ViewMatrix,
		FMatrix ProjMatrix,
		TArray<uint32> ShaderInputMappings
		)
		: WorkArea(CheckCmdList, WaveWorks, QuadTreeHandle, ViewMatrix, ProjMatrix, ShaderInputMappings)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};
// WaveWorks End

struct FComputedGraphicsPipelineState
{
	FGraphicsPipelineStateRHIRef GraphicsPipelineState;
	int32 UseCount;
	FComputedGraphicsPipelineState()
		: UseCount(0)
	{
	}
};

struct FLocalGraphicsPipelineStateWorkArea
{
	FGraphicsPipelineStateInitializer Args;
	FComputedGraphicsPipelineState* ComputedGraphicsPipelineState;
#if DO_CHECK // the below variables are used in check(), which can be enabled in Shipping builds (see Build.h)
	FRHICommandListBase* CheckCmdList;
	int32 UID;
#endif

	FORCEINLINE_DEBUGGABLE FLocalGraphicsPipelineStateWorkArea(
		FRHICommandListBase* InCheckCmdList,
		const FGraphicsPipelineStateInitializer& Initializer
		)
		: Args(Initializer)
#if DO_CHECK
		, CheckCmdList(InCheckCmdList)
		, UID(InCheckCmdList->GetUID())
#endif
	{
		ComputedGraphicsPipelineState = new (InCheckCmdList->Alloc<FComputedGraphicsPipelineState>()) FComputedGraphicsPipelineState;
	}
};

struct FLocalGraphicsPipelineState
{
	FLocalGraphicsPipelineStateWorkArea* WorkArea;
	FGraphicsPipelineStateRHIRef BypassGraphicsPipelineState; // this is only used in the case of Bypass, should eventually be deleted

	FLocalGraphicsPipelineState()
		: WorkArea(nullptr)
	{
	}

	FLocalGraphicsPipelineState(const FLocalGraphicsPipelineState& Other)
		: WorkArea(Other.WorkArea)
		, BypassGraphicsPipelineState(Other.BypassGraphicsPipelineState)
	{
	}
};

struct FRHICommandBuildLocalGraphicsPipelineState : public FRHICommand<FRHICommandBuildLocalGraphicsPipelineState>
{
	FLocalGraphicsPipelineStateWorkArea WorkArea;

	FORCEINLINE_DEBUGGABLE FRHICommandBuildLocalGraphicsPipelineState(
		FRHICommandListBase* CheckCmdList,
		const FGraphicsPipelineStateInitializer& Initializer
		)
		: WorkArea(CheckCmdList, Initializer)
	{
	}

	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandSetLocalGraphicsPipelineState : public FRHICommand<FRHICommandSetLocalGraphicsPipelineState>
{
	FLocalGraphicsPipelineState LocalGraphicsPipelineState;
	FORCEINLINE_DEBUGGABLE FRHICommandSetLocalGraphicsPipelineState(FRHICommandListBase* CheckCmdList, FLocalGraphicsPipelineState& InLocalGraphicsPipelineState)
		: LocalGraphicsPipelineState(InLocalGraphicsPipelineState)
	{
		check(CheckCmdList == LocalGraphicsPipelineState.WorkArea->CheckCmdList && CheckCmdList->GetUID() == LocalGraphicsPipelineState.WorkArea->UID); // this PSO was not built for this particular commandlist
		LocalGraphicsPipelineState.WorkArea->ComputedGraphicsPipelineState->UseCount++;
	}

	RHI_API void Execute(FRHICommandListBase& CmdList);
};


// NVCHANGE_BEGIN: Nvidia Volumetric Lighting
#if WITH_NVVOLUMETRICLIGHTING
struct FRHICommandBeginAccumulation : public FRHICommand<FRHICommandBeginAccumulation>
{
	FTextureRHIParamRef SceneDepthTextureRHI;
	TArray<NvVl::ViewerDesc> ViewerDescs;
	NvVl::MediumDesc MediumDesc;
	NvVl::DebugFlags DebugFlags;

	FORCEINLINE_DEBUGGABLE FRHICommandBeginAccumulation(FTextureRHIParamRef InSceneDepthTextureRHI, const TArray<NvVl::ViewerDesc>& InViewerDescs, const NvVl::MediumDesc& InMediumDesc, NvVl::DebugFlags InDebugFlags)
		: SceneDepthTextureRHI(InSceneDepthTextureRHI)
		, ViewerDescs(InViewerDescs)
		, MediumDesc(InMediumDesc)
		, DebugFlags(InDebugFlags)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandRenderVolume : public FRHICommand<FRHICommandRenderVolume>
{
	TArray<FTextureRHIParamRef> ShadowMapTextures;
	NvVl::ShadowMapDesc ShadowMapDesc;
	NvVl::LightDesc LightDesc;
	NvVl::VolumeDesc VolumeDesc;

	FORCEINLINE_DEBUGGABLE FRHICommandRenderVolume(const TArray<FTextureRHIParamRef>& InShadowMapTextures, const NvVl::ShadowMapDesc& InShadowMapDesc, const NvVl::LightDesc& InLightDesc, const NvVl::VolumeDesc& InVolumeDesc)
		: ShadowMapTextures(InShadowMapTextures)
		, ShadowMapDesc(InShadowMapDesc)
		, LightDesc(InLightDesc)
		, VolumeDesc(InVolumeDesc)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandEndAccumulation : public FRHICommand<FRHICommandEndAccumulation>
{
	FORCEINLINE_DEBUGGABLE FRHICommandEndAccumulation()
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandApplyLighting : public FRHICommand<FRHICommandApplyLighting>
{
	FTextureRHIParamRef SceneColorSurfaceRHI;
	NvVl::PostprocessDesc PostprocessDesc;

	FORCEINLINE_DEBUGGABLE FRHICommandApplyLighting(FTextureRHIParamRef InSceneColorSurfaceRHI, const NvVl::PostprocessDesc& InPostprocessDesc)
		: SceneColorSurfaceRHI(InSceneColorSurfaceRHI)
		, PostprocessDesc(InPostprocessDesc)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};
#endif
// NVCHANGE_END: Nvidia Volumetric Lighting

struct FComputedUniformBuffer
{
	FUniformBufferRHIRef UniformBuffer;
	mutable int32 UseCount;
	FComputedUniformBuffer()
		: UseCount(0)
	{
	}
};

struct FLocalUniformBufferWorkArea
{
	void* Contents;
	const FRHIUniformBufferLayout* Layout;
	FComputedUniformBuffer* ComputedUniformBuffer;
#if DO_CHECK // the below variables are used in check(), which can be enabled in Shipping builds (see Build.h)
	FRHICommandListBase* CheckCmdList;
	int32 UID;
#endif

	FLocalUniformBufferWorkArea(FRHICommandListBase* InCheckCmdList, const void* InContents, uint32 ContentsSize, const FRHIUniformBufferLayout* InLayout)
		: Layout(InLayout)
#if DO_CHECK
		, CheckCmdList(InCheckCmdList)
		, UID(InCheckCmdList->GetUID())
#endif
	{
		check(ContentsSize);
		Contents = InCheckCmdList->Alloc(ContentsSize, UNIFORM_BUFFER_STRUCT_ALIGNMENT);
		FMemory::Memcpy(Contents, InContents, ContentsSize);
		ComputedUniformBuffer = new (InCheckCmdList->Alloc<FComputedUniformBuffer>()) FComputedUniformBuffer;
	}
};

struct FLocalUniformBuffer
{
	FLocalUniformBufferWorkArea* WorkArea;
	FUniformBufferRHIRef BypassUniform; // this is only used in the case of Bypass, should eventually be deleted
	FLocalUniformBuffer()
		: WorkArea(nullptr)
	{
	}
	FLocalUniformBuffer(const FLocalUniformBuffer& Other)
		: WorkArea(Other.WorkArea)
		, BypassUniform(Other.BypassUniform)
	{
	}
	FORCEINLINE_DEBUGGABLE bool IsValid() const
	{
		return WorkArea || IsValidRef(BypassUniform);
	}
};

struct FRHICommandBuildLocalUniformBuffer : public FRHICommand<FRHICommandBuildLocalUniformBuffer>
{
	FLocalUniformBufferWorkArea WorkArea;
	FORCEINLINE_DEBUGGABLE FRHICommandBuildLocalUniformBuffer(
		FRHICommandListBase* CheckCmdList,
		const void* Contents,
		uint32 ContentsSize,
		const FRHIUniformBufferLayout& Layout
		)
		: WorkArea(CheckCmdList, Contents, ContentsSize, &Layout)

	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

template <typename TShaderRHIParamRef>
struct FRHICommandSetLocalUniformBuffer : public FRHICommand<FRHICommandSetLocalUniformBuffer<TShaderRHIParamRef> >
{
	TShaderRHIParamRef Shader;
	uint32 BaseIndex;
	FLocalUniformBuffer LocalUniformBuffer;
	FORCEINLINE_DEBUGGABLE FRHICommandSetLocalUniformBuffer(FRHICommandListBase* CheckCmdList, TShaderRHIParamRef InShader, uint32 InBaseIndex, const FLocalUniformBuffer& InLocalUniformBuffer)
		: Shader(InShader)
		, BaseIndex(InBaseIndex)
		, LocalUniformBuffer(InLocalUniformBuffer)

	{
		check(CheckCmdList == LocalUniformBuffer.WorkArea->CheckCmdList && CheckCmdList->GetUID() == LocalUniformBuffer.WorkArea->UID); // this uniform buffer was not built for this particular commandlist
		LocalUniformBuffer.WorkArea->ComputedUniformBuffer->UseCount++;
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandBeginRenderQuery : public FRHICommand<FRHICommandBeginRenderQuery>
{
	FRenderQueryRHIParamRef RenderQuery;

	FORCEINLINE_DEBUGGABLE FRHICommandBeginRenderQuery(FRenderQueryRHIParamRef InRenderQuery)
		: RenderQuery(InRenderQuery)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandEndRenderQuery : public FRHICommand<FRHICommandEndRenderQuery>
{
	FRenderQueryRHIParamRef RenderQuery;

	FORCEINLINE_DEBUGGABLE FRHICommandEndRenderQuery(FRenderQueryRHIParamRef InRenderQuery)
		: RenderQuery(InRenderQuery)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandBeginOcclusionQueryBatch : public FRHICommand<FRHICommandBeginOcclusionQueryBatch>
{
	FORCEINLINE_DEBUGGABLE FRHICommandBeginOcclusionQueryBatch()
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandEndOcclusionQueryBatch : public FRHICommand<FRHICommandEndOcclusionQueryBatch>
{
	FORCEINLINE_DEBUGGABLE FRHICommandEndOcclusionQueryBatch()
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

template<ECmdList CmdListType>
struct FRHICommandSubmitCommandsHint : public FRHICommand<FRHICommandSubmitCommandsHint<CmdListType>>
{
	FORCEINLINE_DEBUGGABLE FRHICommandSubmitCommandsHint()
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandBeginScene : public FRHICommand<FRHICommandBeginScene>
{
	FORCEINLINE_DEBUGGABLE FRHICommandBeginScene()
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandEndScene : public FRHICommand<FRHICommandEndScene>
{
	FORCEINLINE_DEBUGGABLE FRHICommandEndScene()
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandBeginFrame : public FRHICommand<FRHICommandBeginFrame>
{
	FORCEINLINE_DEBUGGABLE FRHICommandBeginFrame()
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandEndFrame : public FRHICommand<FRHICommandEndFrame>
{
	FORCEINLINE_DEBUGGABLE FRHICommandEndFrame()
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandBeginDrawingViewport : public FRHICommand<FRHICommandBeginDrawingViewport>
{
	FViewportRHIParamRef Viewport;
	FTextureRHIParamRef RenderTargetRHI;

	FORCEINLINE_DEBUGGABLE FRHICommandBeginDrawingViewport(FViewportRHIParamRef InViewport, FTextureRHIParamRef InRenderTargetRHI)
		: Viewport(InViewport)
		, RenderTargetRHI(InRenderTargetRHI)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandEndDrawingViewport : public FRHICommand<FRHICommandEndDrawingViewport>
{
	FViewportRHIParamRef Viewport;
	bool bPresent;
	bool bLockToVsync;

	FORCEINLINE_DEBUGGABLE FRHICommandEndDrawingViewport(FViewportRHIParamRef InViewport, bool InbPresent, bool InbLockToVsync)
		: Viewport(InViewport)
		, bPresent(InbPresent)
		, bLockToVsync(InbLockToVsync)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

template<ECmdList CmdListType>
struct FRHICommandPushEvent : public FRHICommand<FRHICommandPushEvent<CmdListType>>
{
	const TCHAR *Name;
	FColor Color;

	FORCEINLINE_DEBUGGABLE FRHICommandPushEvent(const TCHAR *InName, FColor InColor)
		: Name(InName)
		, Color(InColor)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

template<ECmdList CmdListType>
struct FRHICommandPopEvent : public FRHICommand<FRHICommandPopEvent<CmdListType>>
{
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandInvalidateCachedState : public FRHICommand<FRHICommandInvalidateCachedState>
{
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandDebugBreak : public FRHICommand<FRHICommandDebugBreak>
{
	void Execute(FRHICommandListBase& CmdList)
	{
		if (FPlatformMisc::IsDebuggerPresent())
		{
			FPlatformMisc::DebugBreak();
		}
	}
};

struct FRHICommandUpdateTextureReference : public FRHICommand<FRHICommandUpdateTextureReference>
{
	FTextureReferenceRHIParamRef TextureRef;
	FTextureRHIParamRef NewTexture;
	FORCEINLINE_DEBUGGABLE FRHICommandUpdateTextureReference(FTextureReferenceRHIParamRef InTextureRef, FTextureRHIParamRef InNewTexture)
		: TextureRef(InTextureRef)
		, NewTexture(InNewTexture)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

// NvFlow begin
struct FRHICommandNvFlowWork : public FRHICommand<FRHICommandNvFlowWork>
{
	void(*WorkFunc)(void*,SIZE_T,IRHICommandContext*);
	void* ParamData;
	SIZE_T NumBytes;
	FORCEINLINE_DEBUGGABLE FRHICommandNvFlowWork(void(*WorkFunc)(void*,SIZE_T,IRHICommandContext*), void* ParamData, SIZE_T NumBytes)
		: WorkFunc(WorkFunc)
		, ParamData(ParamData)
		, NumBytes(NumBytes)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};
// NvFlow end

#define CMD_CONTEXT(Method) GetContext().Method
#define COMPUTE_CONTEXT(Method) GetComputeContext().Method

// NVCHANGE_BEGIN: Add HBAO+
#if WITH_GFSDK_SSAO

struct FRHICommandRenderHBAO : public FRHICommand<FRHICommandRenderHBAO>
{
	FTextureRHIParamRef SceneDepthTextureRHI;
	FMatrix ProjectionMatrix;
	FTextureRHIParamRef SceneNormalTextureRHI;
	FMatrix ViewMatrix;
	FTextureRHIParamRef SceneColorTextureRHI;
	GFSDK_SSAO_Parameters AOParams;

	FORCEINLINE_DEBUGGABLE FRHICommandRenderHBAO(
		const FTextureRHIParamRef InSceneDepthTextureRHI,
		const FMatrix& InProjectionMatrix,
		const FTextureRHIParamRef InSceneNormalTextureRHI,
		const FMatrix& InViewMatrix,
		const FTextureRHIParamRef InSceneColorTextureRHI,
		const GFSDK_SSAO_Parameters& InAOParams
	)
		: SceneDepthTextureRHI(InSceneDepthTextureRHI)
		, ProjectionMatrix(InProjectionMatrix)
		, SceneNormalTextureRHI(InSceneNormalTextureRHI)
		, ViewMatrix(InViewMatrix)
		, SceneColorTextureRHI(InSceneColorTextureRHI)
		, AOParams(InAOParams)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

#endif
// NVCHANGE_END: Add HBAO+

// NVCHANGE_BEGIN: Add VXGI
#if WITH_GFSDK_VXGI

struct FRHIVXGICleanupAfterVoxelization : public FRHICommand<FRHIVXGICleanupAfterVoxelization>
{
	FORCEINLINE_DEBUGGABLE FRHIVXGICleanupAfterVoxelization()
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHISetViewportsAndScissorRects : public FRHICommand<FRHISetViewportsAndScissorRects>
{
	uint32 Count;
	TArray<FViewportBounds> Viewports;
	TArray<FScissorRect> ScissorRects;

	FORCEINLINE_DEBUGGABLE FRHISetViewportsAndScissorRects(uint32 InCount, const FViewportBounds* InViewports, const FScissorRect* InScissorRects)
	{
		Count = InCount;
		Viewports.SetNum(Count);
		ScissorRects.SetNum(Count);
		FMemory::Memmove(Viewports.GetData(), InViewports, Count * sizeof(FViewportBounds));
		FMemory::Memmove(ScissorRects.GetData(), InScissorRects, Count * sizeof(FScissorRect));
	}

	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHIDispatchIndirectComputeShaderStructured : public FRHICommand<FRHIDispatchIndirectComputeShaderStructured>
{
	FStructuredBufferRHIRef ArgumentBuffer;
	uint32 ArgumentOffset;

	FORCEINLINE_DEBUGGABLE FRHIDispatchIndirectComputeShaderStructured(FStructuredBufferRHIParamRef InArgumentBuffer, uint32 InArgumentOffset)
		: ArgumentBuffer(InArgumentBuffer)
		, ArgumentOffset(InArgumentOffset)
	{
	}

	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICopyStructuredBufferData : public FRHICommand<FRHICopyStructuredBufferData>
{
	FStructuredBufferRHIRef DestBuffer;
	uint32 DestOffset;
	FStructuredBufferRHIRef SrcBuffer;
	uint32 SrcOffset;
	uint32 DataSize;

	FORCEINLINE_DEBUGGABLE FRHICopyStructuredBufferData(
		FStructuredBufferRHIParamRef InDestBuffer,
		uint32 InDestOffset,
		FStructuredBufferRHIParamRef InSrcBuffer,
		uint32 InSrcOffset,
		uint32 InDataSize
	)
		: DestBuffer(InDestBuffer)
		, DestOffset(InDestOffset)
		, SrcBuffer(InSrcBuffer)
		, SrcOffset(InSrcOffset)
		, DataSize(InDataSize)
	{
	}

	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHIExecuteVxgiRenderingCommand : public FRHICommand<FRHIExecuteVxgiRenderingCommand>
{
	NVRHI::IRenderThreadCommand* Command;

	FORCEINLINE_DEBUGGABLE FRHIExecuteVxgiRenderingCommand(
		NVRHI::IRenderThreadCommand* InCommand
	)
		: Command(InCommand)
	{
	}

	RHI_API void Execute(FRHICommandListBase& CmdList);
};
#endif
// NVCHANGE_END: Add VXGI

template<> void FRHICommandSetShaderParameter<FComputeShaderRHIParamRef, ECmdList::ECompute>::Execute(FRHICommandListBase& CmdList);
template<> void FRHICommandSetShaderUniformBuffer<FComputeShaderRHIParamRef, ECmdList::ECompute>::Execute(FRHICommandListBase& CmdList);
template<> void FRHICommandSetShaderTexture<FComputeShaderRHIParamRef, ECmdList::ECompute>::Execute(FRHICommandListBase& CmdList);
template<> void FRHICommandSetShaderResourceViewParameter<FComputeShaderRHIParamRef, ECmdList::ECompute>::Execute(FRHICommandListBase& CmdList);
template<> void FRHICommandSetShaderSampler<FComputeShaderRHIParamRef, ECmdList::ECompute>::Execute(FRHICommandListBase& CmdList);


class RHI_API FRHIRenderPassCommandList : public FRHICommandListBase
{
public:
	/** Custom new/delete with recycling */
	void* operator new(size_t Size);
	void operator delete(void *RawMemory);

	FRHICommandList& GetParent()
	{
		return *(FRHICommandList*)this;
	}

	FORCEINLINE_DEBUGGABLE void BeginUpdateMultiFrameResource(FTextureRHIParamRef Texture)
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHIBeginUpdateMultiFrameResource)(Texture);
			return;
		}
		new (AllocCommand<FRHICommandBeginUpdateMultiFrameResource>()) FRHICommandBeginUpdateMultiFrameResource(Texture);
	}

	FORCEINLINE_DEBUGGABLE void EndUpdateMultiFrameResource(FTextureRHIParamRef Texture)
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHIEndUpdateMultiFrameResource)(Texture);
			return;
		}
		new (AllocCommand<FRHICommandEndUpdateMultiFrameResource>()) FRHICommandEndUpdateMultiFrameResource(Texture);
	}

	FORCEINLINE_DEBUGGABLE void BeginUpdateMultiFrameResource(FUnorderedAccessViewRHIParamRef UAV)
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHIBeginUpdateMultiFrameResource)(UAV);
			return;
		}
		new (AllocCommand<FRHICommandBeginUpdateMultiFrameUAV>()) FRHICommandBeginUpdateMultiFrameUAV(UAV);
	}

	FORCEINLINE_DEBUGGABLE void EndUpdateMultiFrameResource(FUnorderedAccessViewRHIParamRef UAV)
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHIEndUpdateMultiFrameResource)(UAV);
			return;
		}
		new (AllocCommand<FRHICommandEndUpdateMultiFrameUAV>()) FRHICommandEndUpdateMultiFrameUAV(UAV);
	}

	FORCEINLINE_DEBUGGABLE FLocalGraphicsPipelineState BuildLocalGraphicsPipelineState(
		const FGraphicsPipelineStateInitializer& Initializer
	)
	{
		FLocalGraphicsPipelineState Result;
		if (Bypass())
		{
			Result.BypassGraphicsPipelineState = RHICreateGraphicsPipelineState(Initializer);
		}
		else
		{
			auto* Cmd = new (AllocCommand<FRHICommandBuildLocalGraphicsPipelineState>()) FRHICommandBuildLocalGraphicsPipelineState(this, Initializer);
			Result.WorkArea = &Cmd->WorkArea;
		}
		return Result;
	}

	FORCEINLINE_DEBUGGABLE void SetLocalGraphicsPipelineState(FLocalGraphicsPipelineState LocalGraphicsPipelineState)
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHISetGraphicsPipelineState)(LocalGraphicsPipelineState.BypassGraphicsPipelineState);
			return;
		}
		new (AllocCommand<FRHICommandSetLocalGraphicsPipelineState>()) FRHICommandSetLocalGraphicsPipelineState(this, LocalGraphicsPipelineState);
	}

	FORCEINLINE_DEBUGGABLE FLocalUniformBuffer BuildLocalUniformBuffer(const void* Contents, uint32 ContentsSize, const FRHIUniformBufferLayout& Layout)
	{
		FLocalUniformBuffer Result;
		if (Bypass())
		{
			Result.BypassUniform = RHICreateUniformBuffer(Contents, Layout, UniformBuffer_SingleFrame);
		}
		else
		{
			check(Contents && ContentsSize && (&Layout != nullptr));
			auto* Cmd = new (AllocCommand<FRHICommandBuildLocalUniformBuffer>()) FRHICommandBuildLocalUniformBuffer(this, Contents, ContentsSize, Layout);
			Result.WorkArea = &Cmd->WorkArea;
		}
		return Result;
	}

	template <typename TShaderRHIParamRef>
	FORCEINLINE_DEBUGGABLE void SetLocalShaderUniformBuffer(TShaderRHIParamRef Shader, uint32 BaseIndex, const FLocalUniformBuffer& UniformBuffer)
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHISetShaderUniformBuffer)(Shader, BaseIndex, UniformBuffer.BypassUniform);
			return;
		}
		new (AllocCommand<FRHICommandSetLocalUniformBuffer<TShaderRHIParamRef> >()) FRHICommandSetLocalUniformBuffer<TShaderRHIParamRef>(this, Shader, BaseIndex, UniformBuffer);
	}

	template <typename TShaderRHI>
	FORCEINLINE_DEBUGGABLE void SetLocalShaderUniformBuffer(TRefCountPtr<TShaderRHI>& Shader, uint32 BaseIndex, const FLocalUniformBuffer& UniformBuffer)
	{
		SetLocalShaderUniformBuffer(Shader.GetReference(), BaseIndex, UniformBuffer);
	}

	template <typename TShaderRHI>
	FORCEINLINE_DEBUGGABLE void SetShaderUniformBuffer(TShaderRHI* Shader, uint32 BaseIndex, FUniformBufferRHIParamRef UniformBuffer)
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHISetShaderUniformBuffer)(Shader, BaseIndex, UniformBuffer);
			return;
		}
		new (AllocCommand<FRHICommandSetShaderUniformBuffer<TShaderRHI*, ECmdList::EGfx> >()) FRHICommandSetShaderUniformBuffer<TShaderRHI*, ECmdList::EGfx>(Shader, BaseIndex, UniformBuffer);
	}
	template <typename TShaderRHI>
	FORCEINLINE void SetShaderUniformBuffer(TRefCountPtr<TShaderRHI>& Shader, uint32 BaseIndex, FUniformBufferRHIParamRef UniformBuffer)
	{
		SetShaderUniformBuffer(Shader.GetReference(), BaseIndex, UniformBuffer);
	}

	template <typename TShaderRHI>
	FORCEINLINE_DEBUGGABLE void SetShaderParameter(TShaderRHI* Shader, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue)
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHISetShaderParameter)(Shader, BufferIndex, BaseIndex, NumBytes, NewValue);
			return;
		}
		void* UseValue = Alloc(NumBytes, 16);
		FMemory::Memcpy(UseValue, NewValue, NumBytes);
		new (AllocCommand<FRHICommandSetShaderParameter<TShaderRHI*, ECmdList::EGfx> >()) FRHICommandSetShaderParameter<TShaderRHI*, ECmdList::EGfx>(Shader, BufferIndex, BaseIndex, NumBytes, UseValue);
	}
	template <typename TShaderRHI>
	FORCEINLINE void SetShaderParameter(TRefCountPtr<TShaderRHI>& Shader, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue)
	{
		SetShaderParameter(Shader.GetReference(), BufferIndex, BaseIndex, NumBytes, NewValue);
	}

	template <typename TShaderRHIParamRef>
	FORCEINLINE_DEBUGGABLE void SetShaderTexture(TShaderRHIParamRef Shader, uint32 TextureIndex, FTextureRHIParamRef Texture)
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHISetShaderTexture)(Shader, TextureIndex, Texture);
			return;
		}
		new (AllocCommand<FRHICommandSetShaderTexture<TShaderRHIParamRef, ECmdList::EGfx> >()) FRHICommandSetShaderTexture<TShaderRHIParamRef, ECmdList::EGfx>(Shader, TextureIndex, Texture);
	}

	template <typename TShaderRHI>
	FORCEINLINE_DEBUGGABLE void SetShaderTexture(TRefCountPtr<TShaderRHI>& Shader, uint32 TextureIndex, FTextureRHIParamRef Texture)
	{
		SetShaderTexture(Shader.GetReference(), TextureIndex, Texture);
	}

	template <typename TShaderRHIParamRef>
	FORCEINLINE_DEBUGGABLE void SetShaderResourceViewParameter(TShaderRHIParamRef Shader, uint32 SamplerIndex, FShaderResourceViewRHIParamRef SRV)
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHISetShaderResourceViewParameter)(Shader, SamplerIndex, SRV);
			return;
		}
		new (AllocCommand<FRHICommandSetShaderResourceViewParameter<TShaderRHIParamRef, ECmdList::EGfx> >()) FRHICommandSetShaderResourceViewParameter<TShaderRHIParamRef, ECmdList::EGfx>(Shader, SamplerIndex, SRV);
	}

	template <typename TShaderRHI>
	FORCEINLINE_DEBUGGABLE void SetShaderResourceViewParameter(TRefCountPtr<TShaderRHI>& Shader, uint32 SamplerIndex, FShaderResourceViewRHIParamRef SRV)
	{
		SetShaderResourceViewParameter(Shader.GetReference(), SamplerIndex, SRV);
	}

	template <typename TShaderRHIParamRef>
	FORCEINLINE_DEBUGGABLE void SetShaderSampler(TShaderRHIParamRef Shader, uint32 SamplerIndex, FSamplerStateRHIParamRef State)
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHISetShaderSampler)(Shader, SamplerIndex, State);
			return;
		}
		new (AllocCommand<FRHICommandSetShaderSampler<TShaderRHIParamRef, ECmdList::EGfx> >()) FRHICommandSetShaderSampler<TShaderRHIParamRef, ECmdList::EGfx>(Shader, SamplerIndex, State);
	}

	template <typename TShaderRHI>
	FORCEINLINE_DEBUGGABLE void SetShaderSampler(TRefCountPtr<TShaderRHI>& Shader, uint32 SamplerIndex, FSamplerStateRHIParamRef State)
	{
		SetShaderSampler(Shader.GetReference(), SamplerIndex, State);
	}

	FORCEINLINE_DEBUGGABLE void SetUAVParameter(FComputeShaderRHIParamRef Shader, uint32 UAVIndex, FUnorderedAccessViewRHIParamRef UAV)
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHISetUAVParameter)(Shader, UAVIndex, UAV);
			return;
		}
		new (AllocCommand<FRHICommandSetUAVParameter<FComputeShaderRHIParamRef, ECmdList::EGfx> >()) FRHICommandSetUAVParameter<FComputeShaderRHIParamRef, ECmdList::EGfx>(Shader, UAVIndex, UAV);
	}

	FORCEINLINE_DEBUGGABLE void SetUAVParameter(TRefCountPtr<FRHIComputeShader>& Shader, uint32 UAVIndex, FUnorderedAccessViewRHIParamRef UAV)
	{
		SetUAVParameter(Shader.GetReference(), UAVIndex, UAV);
	}

	FORCEINLINE_DEBUGGABLE void SetUAVParameter(FComputeShaderRHIParamRef Shader, uint32 UAVIndex, FUnorderedAccessViewRHIParamRef UAV, uint32 InitialCount)
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHISetUAVParameter)(Shader, UAVIndex, UAV, InitialCount);
			return;
		}
		new (AllocCommand<FRHICommandSetUAVParameter_IntialCount<FComputeShaderRHIParamRef, ECmdList::EGfx> >()) FRHICommandSetUAVParameter_IntialCount<FComputeShaderRHIParamRef, ECmdList::EGfx>(Shader, UAVIndex, UAV, InitialCount);
	}

	FORCEINLINE_DEBUGGABLE void SetUAVParameter(TRefCountPtr<FRHIComputeShader>& Shader, uint32 UAVIndex, FUnorderedAccessViewRHIParamRef UAV, uint32 InitialCount)
	{
		SetUAVParameter(Shader.GetReference(), UAVIndex, UAV, InitialCount);
	}


	FORCEINLINE_DEBUGGABLE void SetBlendFactor(const FLinearColor& BlendFactor = FLinearColor::White)
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHISetBlendFactor)(BlendFactor);
			return;
		}
		new (AllocCommand<FRHICommandSetBlendFactor>()) FRHICommandSetBlendFactor(BlendFactor);
	}

	FORCEINLINE_DEBUGGABLE void DrawPrimitive(uint32 PrimitiveType, uint32 BaseVertexIndex, uint32 NumPrimitives, uint32 NumInstances)
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHIDrawPrimitive)(PrimitiveType, BaseVertexIndex, NumPrimitives, NumInstances);
			return;
		}
		new (AllocCommand<FRHICommandDrawPrimitive>()) FRHICommandDrawPrimitive(PrimitiveType, BaseVertexIndex, NumPrimitives, NumInstances);
	}

	FORCEINLINE_DEBUGGABLE void DrawIndexedPrimitive(FIndexBufferRHIParamRef IndexBuffer, uint32 PrimitiveType, int32 BaseVertexIndex, uint32 FirstInstance, uint32 NumVertices, uint32 StartIndex, uint32 NumPrimitives, uint32 NumInstances)
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHIDrawIndexedPrimitive)(IndexBuffer, PrimitiveType, BaseVertexIndex, FirstInstance, NumVertices, StartIndex, NumPrimitives, NumInstances);
			return;
		}
		new (AllocCommand<FRHICommandDrawIndexedPrimitive>()) FRHICommandDrawIndexedPrimitive(IndexBuffer, PrimitiveType, BaseVertexIndex, FirstInstance, NumVertices, StartIndex, NumPrimitives, NumInstances);
	}

	FORCEINLINE_DEBUGGABLE void SetStreamSource(uint32 StreamIndex, FVertexBufferRHIParamRef VertexBuffer, uint32 Offset)
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHISetStreamSource)(StreamIndex, VertexBuffer, Offset);
			return;
		}
		new (AllocCommand<FRHICommandSetStreamSource>()) FRHICommandSetStreamSource(StreamIndex, VertexBuffer, Offset);
	}

	FORCEINLINE_DEBUGGABLE void SetStencilRef(uint32 StencilRef)
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHISetStencilRef)(StencilRef);
			return;
		}

		new(AllocCommand<FRHICommandSetStencilRef>()) FRHICommandSetStencilRef(StencilRef);
	}

	FORCEINLINE_DEBUGGABLE void SetViewport(uint32 MinX, uint32 MinY, float MinZ, uint32 MaxX, uint32 MaxY, float MaxZ)
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHISetViewport)(MinX, MinY, MinZ, MaxX, MaxY, MaxZ);
			return;
		}
		new (AllocCommand<FRHICommandSetViewport>()) FRHICommandSetViewport(MinX, MinY, MinZ, MaxX, MaxY, MaxZ);
	}

	FORCEINLINE_DEBUGGABLE void SetStereoViewport(uint32 LeftMinX, uint32 RightMinX, uint32 LeftMinY, uint32 RightMinY, float MinZ, uint32 LeftMaxX, uint32 RightMaxX, uint32 LeftMaxY, uint32 RightMaxY, float MaxZ)
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHISetStereoViewport)(LeftMinX, RightMinX, LeftMinY, RightMinY, MinZ, LeftMaxX, RightMaxX, LeftMaxY, RightMaxY, MaxZ);
			return;
		}
		new (AllocCommand<FRHICommandSetStereoViewport>()) FRHICommandSetStereoViewport(LeftMinX, RightMinX, LeftMinY, RightMinY, MinZ, LeftMaxX, RightMaxX, LeftMaxY, RightMaxY, MaxZ);
	}

	FORCEINLINE_DEBUGGABLE void SetScissorRect(bool bEnable, uint32 MinX, uint32 MinY, uint32 MaxX, uint32 MaxY)
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHISetScissorRect)(bEnable, MinX, MinY, MaxX, MaxY);
			return;
		}
		new (AllocCommand<FRHICommandSetScissorRect>()) FRHICommandSetScissorRect(bEnable, MinX, MinY, MaxX, MaxY);
	}

	FORCEINLINE_DEBUGGABLE void BeginDrawPrimitiveUP(uint32 PrimitiveType, uint32 NumPrimitives, uint32 NumVertices, uint32 VertexDataStride, void*& OutVertexData)
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHIBeginDrawPrimitiveUP)(PrimitiveType, NumPrimitives, NumVertices, VertexDataStride, OutVertexData);
			return;
		}
		check(!DrawUPData.OutVertexData && NumVertices * VertexDataStride > 0);

		OutVertexData = Alloc(NumVertices * VertexDataStride, 16);

		DrawUPData.PrimitiveType = PrimitiveType;
		DrawUPData.NumPrimitives = NumPrimitives;
		DrawUPData.NumVertices = NumVertices;
		DrawUPData.VertexDataStride = VertexDataStride;
		DrawUPData.OutVertexData = OutVertexData;
	}

	FORCEINLINE_DEBUGGABLE void EndDrawPrimitiveUP()
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHIEndDrawPrimitiveUP)();
			return;
		}
		check(DrawUPData.OutVertexData && DrawUPData.NumVertices);
		new (AllocCommand<FRHICommandEndDrawPrimitiveUP>()) FRHICommandEndDrawPrimitiveUP(DrawUPData.PrimitiveType, DrawUPData.NumPrimitives, DrawUPData.NumVertices, DrawUPData.VertexDataStride, DrawUPData.OutVertexData);

		DrawUPData.OutVertexData = nullptr;
		DrawUPData.NumVertices = 0;
	}

	FORCEINLINE_DEBUGGABLE void BeginDrawIndexedPrimitiveUP(uint32 PrimitiveType, uint32 NumPrimitives, uint32 NumVertices, uint32 VertexDataStride, void*& OutVertexData, uint32 MinVertexIndex, uint32 NumIndices, uint32 IndexDataStride, void*& OutIndexData)
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHIBeginDrawIndexedPrimitiveUP)(PrimitiveType, NumPrimitives, NumVertices, VertexDataStride, OutVertexData, MinVertexIndex, NumIndices, IndexDataStride, OutIndexData);
			return;
		}
		check(!DrawUPData.OutVertexData && !DrawUPData.OutIndexData && NumVertices * VertexDataStride > 0 && NumIndices * IndexDataStride > 0);

		OutVertexData = Alloc(NumVertices * VertexDataStride, 16);
		OutIndexData = Alloc(NumIndices * IndexDataStride, 16);

		DrawUPData.PrimitiveType = PrimitiveType;
		DrawUPData.NumPrimitives = NumPrimitives;
		DrawUPData.NumVertices = NumVertices;
		DrawUPData.VertexDataStride = VertexDataStride;
		DrawUPData.OutVertexData = OutVertexData;
		DrawUPData.MinVertexIndex = MinVertexIndex;
		DrawUPData.NumIndices = NumIndices;
		DrawUPData.IndexDataStride = IndexDataStride;
		DrawUPData.OutIndexData = OutIndexData;

	}

	FORCEINLINE_DEBUGGABLE void EndDrawIndexedPrimitiveUP()
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHIEndDrawIndexedPrimitiveUP)();
			return;
		}
		check(DrawUPData.OutVertexData && DrawUPData.OutIndexData && DrawUPData.NumIndices && DrawUPData.NumVertices);

		new (AllocCommand<FRHICommandEndDrawIndexedPrimitiveUP>()) FRHICommandEndDrawIndexedPrimitiveUP(DrawUPData.PrimitiveType, DrawUPData.NumPrimitives, DrawUPData.NumVertices, DrawUPData.VertexDataStride, DrawUPData.OutVertexData, DrawUPData.MinVertexIndex, DrawUPData.NumIndices, DrawUPData.IndexDataStride, DrawUPData.OutIndexData);

		DrawUPData.OutVertexData = nullptr;
		DrawUPData.OutIndexData = nullptr;
		DrawUPData.NumIndices = 0;
		DrawUPData.NumVertices = 0;
	}

	FORCEINLINE_DEBUGGABLE void SetGraphicsPipelineState(class FGraphicsPipelineState* GraphicsPipelineState)
	{
		if (Bypass())
		{
			extern RHI_API FRHIGraphicsPipelineState* ExecuteSetGraphicsPipelineState(class FGraphicsPipelineState* GraphicsPipelineState);
			FRHIGraphicsPipelineState* RHIGraphicsPipelineState = ExecuteSetGraphicsPipelineState(GraphicsPipelineState);
			CMD_CONTEXT(RHISetGraphicsPipelineState)(RHIGraphicsPipelineState);
			return;
		}
		new (AllocCommand<FRHICommandSetGraphicsPipelineState>()) FRHICommandSetGraphicsPipelineState(GraphicsPipelineState);
	}

	FORCEINLINE_DEBUGGABLE void DrawPrimitiveIndirect(uint32 PrimitiveType, FVertexBufferRHIParamRef ArgumentBuffer, uint32 ArgumentOffset)
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHIDrawPrimitiveIndirect)(PrimitiveType, ArgumentBuffer, ArgumentOffset);
			return;
		}
		new (AllocCommand<FRHICommandDrawPrimitiveIndirect>()) FRHICommandDrawPrimitiveIndirect(PrimitiveType, ArgumentBuffer, ArgumentOffset);
	}

	FORCEINLINE_DEBUGGABLE void DrawIndexedIndirect(FIndexBufferRHIParamRef IndexBufferRHI, uint32 PrimitiveType, FStructuredBufferRHIParamRef ArgumentsBufferRHI, uint32 DrawArgumentsIndex, uint32 NumInstances)
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHIDrawIndexedIndirect)(IndexBufferRHI, PrimitiveType, ArgumentsBufferRHI, DrawArgumentsIndex, NumInstances);
			return;
		}
		new (AllocCommand<FRHICommandDrawIndexedIndirect>()) FRHICommandDrawIndexedIndirect(IndexBufferRHI, PrimitiveType, ArgumentsBufferRHI, DrawArgumentsIndex, NumInstances);
	}

	FORCEINLINE_DEBUGGABLE void DrawIndexedPrimitiveIndirect(uint32 PrimitiveType, FIndexBufferRHIParamRef IndexBuffer, FVertexBufferRHIParamRef ArgumentsBuffer, uint32 ArgumentOffset)
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHIDrawIndexedPrimitiveIndirect)(PrimitiveType, IndexBuffer, ArgumentsBuffer, ArgumentOffset);
			return;
		}
		new (AllocCommand<FRHICommandDrawIndexedPrimitiveIndirect>()) FRHICommandDrawIndexedPrimitiveIndirect(PrimitiveType, IndexBuffer, ArgumentsBuffer, ArgumentOffset);
	}

	FORCEINLINE_DEBUGGABLE void EnableDepthBoundsTest(bool bEnable, float MinDepth, float MaxDepth)
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHIEnableDepthBoundsTest)(bEnable, MinDepth, MaxDepth);
			return;
		}
		new (AllocCommand<FRHICommandEnableDepthBoundsTest>()) FRHICommandEnableDepthBoundsTest(bEnable, MinDepth, MaxDepth);
	}

	FORCEINLINE_DEBUGGABLE void PushEvent(const TCHAR* Name, FColor Color)
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHIPushEvent)(Name, Color);
			return;
		}
		TCHAR* NameCopy  = AllocString(Name);
		new (AllocCommand<FRHICommandPushEvent<ECmdList::EGfx>>()) FRHICommandPushEvent<ECmdList::EGfx>(NameCopy, Color);
	}

	FORCEINLINE_DEBUGGABLE void PopEvent()
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHIPopEvent)();
			return;
		}
		new (AllocCommand<FRHICommandPopEvent<ECmdList::EGfx>>()) FRHICommandPopEvent<ECmdList::EGfx>();
	}

	FORCEINLINE_DEBUGGABLE void BeginRenderQuery(FRenderQueryRHIParamRef RenderQuery)
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHIBeginRenderQuery)(RenderQuery);
			return;
		}
		new (AllocCommand<FRHICommandBeginRenderQuery>()) FRHICommandBeginRenderQuery(RenderQuery);
	}

	FORCEINLINE_DEBUGGABLE void EndRenderQuery(FRenderQueryRHIParamRef RenderQuery)
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHIEndRenderQuery)(RenderQuery);
			return;
		}
		new (AllocCommand<FRHICommandEndRenderQuery>()) FRHICommandEndRenderQuery(RenderQuery);
	}

	FORCEINLINE_DEBUGGABLE void BeginOcclusionQueryBatch()
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHIBeginOcclusionQueryBatch)();
			return;
		}
		new (AllocCommand<FRHICommandBeginOcclusionQueryBatch>()) FRHICommandBeginOcclusionQueryBatch();
	}

	FORCEINLINE_DEBUGGABLE void EndOcclusionQueryBatch()
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHIEndOcclusionQueryBatch)();
			return;
		}
		new (AllocCommand<FRHICommandEndOcclusionQueryBatch>()) FRHICommandEndOcclusionQueryBatch();
	}

	FORCEINLINE_DEBUGGABLE void BreakPoint()
	{
#if !UE_BUILD_SHIPPING
		if (Bypass())
		{
			if (FPlatformMisc::IsDebuggerPresent())
			{
				FPlatformMisc::DebugBreak();
			}
			return;
		}
		new (AllocCommand<FRHICommandDebugBreak>()) FRHICommandDebugBreak();
#endif
	}

	void ApplyCachedRenderTargets(FGraphicsPipelineStateInitializer& GraphicsPSOInit);

	FRHIRenderPassCommandList(FRHICommandList& InParent)
	{
		// Separate constructor since we want to call the base one
		Data.Parent = (FRHICommandListBase*)&InParent;
	}

	friend class FRHICommandList;
};


class RHI_API FRHIRenderSubPassCommandList : public FRHIRenderPassCommandList
{
public:
	FRHIRenderSubPassCommandList(FRHICommandList& InParent, FRHIParallelRenderPassCommandList* InRenderPass)
		: FRHIRenderPassCommandList(InParent)
	{
		Data.LocalRHIRenderSubPass = new(Alloc<FLocalCmdListRenderSubPass>()) FLocalCmdListRenderSubPass;
		Data.Type = FRHICommandListBase::FCommonData::ECmdListType::RenderSubPass;
	}

	FRHIParallelRenderPassCommandList& GetParent()
	{
		return *(FRHIParallelRenderPassCommandList*)Data.Parent;
	}

	~FRHIRenderSubPassCommandList()
	{
		//check(bEnded);
		FRHIRenderPassCommandList::~FRHIRenderPassCommandList();
	}

	/** Custom new/delete with recycling */
	void* operator new(size_t Size);
	void operator delete(void *RawMemory);
};

class RHI_API FRHIParallelRenderPassCommandList : public FRHIRenderPassCommandList
{
public:
	FRHIParallelRenderPassCommandList(FRHICommandList& InParent)
		: FRHIRenderPassCommandList(InParent)
	{
		Data.LocalRHIParallelRenderPass = new(Alloc<FLocalCmdListParallelRenderPass>()) FLocalCmdListParallelRenderPass;
	}

	/** Custom new/delete with recycling */
	void* operator new(size_t Size);
	void operator delete(void *RawMemory);

	FRHIRenderSubPassCommandList& BeginSubpass()
	{
		FRHIRenderSubPassCommandList* SubPass = new FRHIRenderSubPassCommandList(GetParent(), this);
		//SubRenderPasses.Add(SubPass);

		if (Bypass())
		{
			SubPass->Data.LocalRHIRenderSubPass->RenderSubPass = CMD_CONTEXT(RHIBeginRenderSubPass)(Data.LocalRHIParallelRenderPass->RenderPass);
		}
		else
		{
			new (AllocCommand<FRHICommandBeginRenderSubPass>()) FRHICommandBeginRenderSubPass(Data.LocalRHIParallelRenderPass, SubPass->Data.LocalRHIRenderSubPass);
		}

		return *SubPass;
	}

	void EndSubpass(FRHIRenderSubPassCommandList& SubPass)
	{
		//int32 Index = SubRenderPasses.Find(&SubPass);
		//ensure(Index != INDEX_NONE);
		//check(!SubPass.bEnded);

		if (Bypass())
		{
			CMD_CONTEXT(RHIEndRenderSubPass)(Data.LocalRHIParallelRenderPass->RenderPass, SubPass.Data.LocalRHIRenderSubPass->RenderSubPass);
			delete &SubPass;
		}
		else
		{
			new (AllocCommand<FRHICommandEndRenderSubPass>()) FRHICommandEndRenderSubPass(Data.LocalRHIParallelRenderPass, SubPass.Data.LocalRHIRenderSubPass);
		}

		//SubPass.bEnded = true;
	}
};

class RHI_API FRHICommandList : public FRHICommandListBase
{
public:

	/** Custom new/delete with recycling */
	void* operator new(size_t Size);
	void operator delete(void *RawMemory);
	
	inline bool IsOutsideRenderPass() const
	{
		return Data.LocalRHIRenderPass == nullptr && Data.LocalRHIParallelRenderPass == nullptr;
	}

	// WaveWorks Begin
	FORCEINLINE_DEBUGGABLE void DrawQuadTreeWaveWorks(FWaveWorksRHIRef waveWorks, struct GFSDK_WaveWorks_Quadtree* QuadTreeHandle, FMatrix ViewMatrix, FMatrix ProjMatrix, const TArray<uint32>& ShaderInputMappings)
	{
		if (Bypass())
		{
			waveWorks->DrawQuadTree(
				QuadTreeHandle,
				ViewMatrix,
				ProjMatrix,
				ShaderInputMappings
				);
		}
		else
		{
			new (AllocCommand<FRHICommandBuildDrawQuadTreeWaveWorks>()) FRHICommandBuildDrawQuadTreeWaveWorks(this, waveWorks, QuadTreeHandle, ViewMatrix, ProjMatrix, ShaderInputMappings);
		}
	}
	// WaveWorks End

	FORCEINLINE_DEBUGGABLE void BeginUpdateMultiFrameResource( FTextureRHIParamRef Texture)
	{
		check(IsOutsideRenderPass());
		if (Bypass())
		{
			CMD_CONTEXT(RHIBeginUpdateMultiFrameResource)( Texture);
			return;
		}
		new (AllocCommand<FRHICommandBeginUpdateMultiFrameResource>()) FRHICommandBeginUpdateMultiFrameResource( Texture);
	}

	FORCEINLINE_DEBUGGABLE void EndUpdateMultiFrameResource(FTextureRHIParamRef Texture)
	{
		check(IsOutsideRenderPass());
		if (Bypass())
		{
			CMD_CONTEXT(RHIEndUpdateMultiFrameResource)(Texture);
			return;
		}
		new (AllocCommand<FRHICommandEndUpdateMultiFrameResource>()) FRHICommandEndUpdateMultiFrameResource(Texture);
	}

	FORCEINLINE_DEBUGGABLE void BeginUpdateMultiFrameResource(FUnorderedAccessViewRHIParamRef UAV)
	{
		check(IsOutsideRenderPass());
		if (Bypass())
		{
			CMD_CONTEXT(RHIBeginUpdateMultiFrameResource)(UAV);
			return;
		}
		new (AllocCommand<FRHICommandBeginUpdateMultiFrameUAV>()) FRHICommandBeginUpdateMultiFrameUAV(UAV);
	}

	FORCEINLINE_DEBUGGABLE void EndUpdateMultiFrameResource(FUnorderedAccessViewRHIParamRef UAV)
	{
		check(IsOutsideRenderPass());
		if (Bypass())
		{
			CMD_CONTEXT(RHIEndUpdateMultiFrameResource)(UAV);
			return;
		}
		new (AllocCommand<FRHICommandEndUpdateMultiFrameUAV>()) FRHICommandEndUpdateMultiFrameUAV(UAV);
	}

	FORCEINLINE_DEBUGGABLE FLocalGraphicsPipelineState BuildLocalGraphicsPipelineState(
		const FGraphicsPipelineStateInitializer& Initializer
		)
	{
		check(IsOutsideRenderPass());
		FLocalGraphicsPipelineState Result;
		if (Bypass())
		{
			Result.BypassGraphicsPipelineState = RHICreateGraphicsPipelineState(Initializer);
		}
		else
		{
			auto* Cmd = new (AllocCommand<FRHICommandBuildLocalGraphicsPipelineState>()) FRHICommandBuildLocalGraphicsPipelineState(this, Initializer);
			Result.WorkArea = &Cmd->WorkArea;
		}
		return Result;
	}

	FORCEINLINE_DEBUGGABLE void SetLocalGraphicsPipelineState(FLocalGraphicsPipelineState LocalGraphicsPipelineState)
	{
		check(IsOutsideRenderPass());
		if (Bypass())
		{
			CMD_CONTEXT(RHISetGraphicsPipelineState)(LocalGraphicsPipelineState.BypassGraphicsPipelineState);
			return;
		}
		new (AllocCommand<FRHICommandSetLocalGraphicsPipelineState>()) FRHICommandSetLocalGraphicsPipelineState(this, LocalGraphicsPipelineState);
	}
	
	FORCEINLINE_DEBUGGABLE FLocalUniformBuffer BuildLocalUniformBuffer(const void* Contents, uint32 ContentsSize, const FRHIUniformBufferLayout& Layout)
	{
		check(IsOutsideRenderPass());
		FLocalUniformBuffer Result;
		if (Bypass())
		{
			Result.BypassUniform = RHICreateUniformBuffer(Contents, Layout, UniformBuffer_SingleFrame);
		}
		else
		{
			check(Contents && ContentsSize && (&Layout != nullptr));
			auto* Cmd = new (AllocCommand<FRHICommandBuildLocalUniformBuffer>()) FRHICommandBuildLocalUniformBuffer(this, Contents, ContentsSize, Layout);
			Result.WorkArea = &Cmd->WorkArea;
		}
		return Result;
	}

	template <typename TShaderRHIParamRef>
	FORCEINLINE_DEBUGGABLE void SetLocalShaderUniformBuffer(TShaderRHIParamRef Shader, uint32 BaseIndex, const FLocalUniformBuffer& UniformBuffer)
	{
		check(IsOutsideRenderPass());
		if (Bypass())
		{
			CMD_CONTEXT(RHISetShaderUniformBuffer)(Shader, BaseIndex, UniformBuffer.BypassUniform);
			return;
		}
		new (AllocCommand<FRHICommandSetLocalUniformBuffer<TShaderRHIParamRef> >()) FRHICommandSetLocalUniformBuffer<TShaderRHIParamRef>(this, Shader, BaseIndex, UniformBuffer);
	}

	template <typename TShaderRHI>
	FORCEINLINE_DEBUGGABLE void SetLocalShaderUniformBuffer(TRefCountPtr<TShaderRHI>& Shader, uint32 BaseIndex, const FLocalUniformBuffer& UniformBuffer)
	{
		SetLocalShaderUniformBuffer(Shader.GetReference(), BaseIndex, UniformBuffer);
	}

	template <typename TShaderRHI>
	FORCEINLINE_DEBUGGABLE void SetShaderUniformBuffer(TShaderRHI* Shader, uint32 BaseIndex, FUniformBufferRHIParamRef UniformBuffer)
	{
		check(IsOutsideRenderPass());
		if (Bypass())
		{
			CMD_CONTEXT(RHISetShaderUniformBuffer)(Shader, BaseIndex, UniformBuffer);
			return;
		}
		new (AllocCommand<FRHICommandSetShaderUniformBuffer<TShaderRHI*, ECmdList::EGfx> >()) FRHICommandSetShaderUniformBuffer<TShaderRHI*, ECmdList::EGfx>(Shader, BaseIndex, UniformBuffer);
	}
	template <typename TShaderRHI>
	FORCEINLINE void SetShaderUniformBuffer(TRefCountPtr<TShaderRHI>& Shader, uint32 BaseIndex, FUniformBufferRHIParamRef UniformBuffer)
	{
		SetShaderUniformBuffer(Shader.GetReference(), BaseIndex, UniformBuffer);
	}

	template <typename TShaderRHI>
	FORCEINLINE_DEBUGGABLE void SetShaderParameter(TShaderRHI* Shader, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue)
	{
		check(IsOutsideRenderPass());
		if (Bypass())
		{
			CMD_CONTEXT(RHISetShaderParameter)(Shader, BufferIndex, BaseIndex, NumBytes, NewValue);
			return;
		}
		void* UseValue = Alloc(NumBytes, 16);
		FMemory::Memcpy(UseValue, NewValue, NumBytes);
		new (AllocCommand<FRHICommandSetShaderParameter<TShaderRHI*, ECmdList::EGfx> >()) FRHICommandSetShaderParameter<TShaderRHI*, ECmdList::EGfx>(Shader, BufferIndex, BaseIndex, NumBytes, UseValue);
	}
	template <typename TShaderRHI>
	FORCEINLINE void SetShaderParameter(TRefCountPtr<TShaderRHI>& Shader, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue)
	{
		SetShaderParameter(Shader.GetReference(), BufferIndex, BaseIndex, NumBytes, NewValue);
	}

	template <typename TShaderRHIParamRef>
	FORCEINLINE_DEBUGGABLE void SetShaderTexture(TShaderRHIParamRef Shader, uint32 TextureIndex, FTextureRHIParamRef Texture)
	{
		check(IsOutsideRenderPass());
		if (Bypass())
		{
			CMD_CONTEXT(RHISetShaderTexture)(Shader, TextureIndex, Texture);
			return;
		}
		new (AllocCommand<FRHICommandSetShaderTexture<TShaderRHIParamRef, ECmdList::EGfx> >()) FRHICommandSetShaderTexture<TShaderRHIParamRef, ECmdList::EGfx>(Shader, TextureIndex, Texture);
	}

	template <typename TShaderRHI>
	FORCEINLINE_DEBUGGABLE void SetShaderTexture(TRefCountPtr<TShaderRHI>& Shader, uint32 TextureIndex, FTextureRHIParamRef Texture)
	{
		SetShaderTexture(Shader.GetReference(), TextureIndex, Texture);
	}

	template <typename TShaderRHIParamRef>
	FORCEINLINE_DEBUGGABLE void SetShaderResourceViewParameter(TShaderRHIParamRef Shader, uint32 SamplerIndex, FShaderResourceViewRHIParamRef SRV)
	{
		check(IsOutsideRenderPass());
		if (Bypass())
		{
			CMD_CONTEXT(RHISetShaderResourceViewParameter)(Shader, SamplerIndex, SRV);
			return;
		}
		new (AllocCommand<FRHICommandSetShaderResourceViewParameter<TShaderRHIParamRef, ECmdList::EGfx> >()) FRHICommandSetShaderResourceViewParameter<TShaderRHIParamRef, ECmdList::EGfx>(Shader, SamplerIndex, SRV);
	}

	template <typename TShaderRHI>
	FORCEINLINE_DEBUGGABLE void SetShaderResourceViewParameter(TRefCountPtr<TShaderRHI>& Shader, uint32 SamplerIndex, FShaderResourceViewRHIParamRef SRV)
	{
		SetShaderResourceViewParameter(Shader.GetReference(), SamplerIndex, SRV);
	}

	template <typename TShaderRHIParamRef>
	FORCEINLINE_DEBUGGABLE void SetShaderSampler(TShaderRHIParamRef Shader, uint32 SamplerIndex, FSamplerStateRHIParamRef State)
	{
		check(IsOutsideRenderPass());
		if (Bypass())
		{
			CMD_CONTEXT(RHISetShaderSampler)(Shader, SamplerIndex, State);
			return;
		}
		new (AllocCommand<FRHICommandSetShaderSampler<TShaderRHIParamRef, ECmdList::EGfx> >()) FRHICommandSetShaderSampler<TShaderRHIParamRef, ECmdList::EGfx>(Shader, SamplerIndex, State);
	}

	template <typename TShaderRHI>
	FORCEINLINE_DEBUGGABLE void SetShaderSampler(TRefCountPtr<TShaderRHI>& Shader, uint32 SamplerIndex, FSamplerStateRHIParamRef State)
	{
		SetShaderSampler(Shader.GetReference(), SamplerIndex, State);
	}

	FORCEINLINE_DEBUGGABLE void SetUAVParameter(FComputeShaderRHIParamRef Shader, uint32 UAVIndex, FUnorderedAccessViewRHIParamRef UAV)
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHISetUAVParameter)(Shader, UAVIndex, UAV);
			return;
		}
		new (AllocCommand<FRHICommandSetUAVParameter<FComputeShaderRHIParamRef, ECmdList::EGfx> >()) FRHICommandSetUAVParameter<FComputeShaderRHIParamRef, ECmdList::EGfx>(Shader, UAVIndex, UAV);
	}

	FORCEINLINE_DEBUGGABLE void SetUAVParameter(TRefCountPtr<FRHIComputeShader>& Shader, uint32 UAVIndex, FUnorderedAccessViewRHIParamRef UAV)
	{
		SetUAVParameter(Shader.GetReference(), UAVIndex, UAV);
	}

	FORCEINLINE_DEBUGGABLE void SetUAVParameter(FComputeShaderRHIParamRef Shader, uint32 UAVIndex, FUnorderedAccessViewRHIParamRef UAV, uint32 InitialCount)
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHISetUAVParameter)(Shader, UAVIndex, UAV, InitialCount);
			return;
		}
		new (AllocCommand<FRHICommandSetUAVParameter_IntialCount<FComputeShaderRHIParamRef, ECmdList::EGfx> >()) FRHICommandSetUAVParameter_IntialCount<FComputeShaderRHIParamRef, ECmdList::EGfx>(Shader, UAVIndex, UAV, InitialCount);
	}

	FORCEINLINE_DEBUGGABLE void SetUAVParameter(TRefCountPtr<FRHIComputeShader>& Shader, uint32 UAVIndex, FUnorderedAccessViewRHIParamRef UAV, uint32 InitialCount)
	{
		SetUAVParameter(Shader.GetReference(), UAVIndex, UAV, InitialCount);
	}

	FORCEINLINE_DEBUGGABLE void SetBlendFactor(const FLinearColor& BlendFactor = FLinearColor::White)
	{
		check(IsOutsideRenderPass());
		if (Bypass())
		{
			CMD_CONTEXT(RHISetBlendFactor)(BlendFactor);
			return;
		}
		new (AllocCommand<FRHICommandSetBlendFactor>()) FRHICommandSetBlendFactor(BlendFactor);
	}

	// WaveWorks Start
	FORCEINLINE_DEBUGGABLE void SetWaveWorksState(FWaveWorksRHIParamRef State, const FMatrix ViewMatrix, const TArray<uint32>& ShaderInputMappings)
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHISetWaveWorksState)(State, ViewMatrix, ShaderInputMappings);
			return;
		}
		new (AllocCommand<FRHICommandSetWaveWorksState>()) FRHICommandSetWaveWorksState(State, ViewMatrix, ShaderInputMappings);
	}
	// WaveWorks End

	FORCEINLINE_DEBUGGABLE void DrawPrimitive(uint32 PrimitiveType, uint32 BaseVertexIndex, uint32 NumPrimitives, uint32 NumInstances)
	{
		check(IsOutsideRenderPass());
		if (Bypass())
		{
			CMD_CONTEXT(RHIDrawPrimitive)(PrimitiveType, BaseVertexIndex, NumPrimitives, NumInstances);
			return;
		}
		new (AllocCommand<FRHICommandDrawPrimitive>()) FRHICommandDrawPrimitive(PrimitiveType, BaseVertexIndex, NumPrimitives, NumInstances);
	}

	FORCEINLINE_DEBUGGABLE void DrawIndexedPrimitive(FIndexBufferRHIParamRef IndexBuffer, uint32 PrimitiveType, int32 BaseVertexIndex, uint32 FirstInstance, uint32 NumVertices, uint32 StartIndex, uint32 NumPrimitives, uint32 NumInstances)
	{
		check(IsOutsideRenderPass());
		if (Bypass())
		{
			CMD_CONTEXT(RHIDrawIndexedPrimitive)(IndexBuffer, PrimitiveType, BaseVertexIndex, FirstInstance, NumVertices, StartIndex, NumPrimitives, NumInstances);
			return;
		}
		new (AllocCommand<FRHICommandDrawIndexedPrimitive>()) FRHICommandDrawIndexedPrimitive(IndexBuffer, PrimitiveType, BaseVertexIndex, FirstInstance, NumVertices, StartIndex, NumPrimitives, NumInstances);
	}

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	DEPRECATED(4.18, "Use alternate SetStreamSource() call with no Stride parameter.")
	FORCEINLINE_DEBUGGABLE void SetStreamSource(uint32 StreamIndex, FVertexBufferRHIParamRef VertexBuffer, uint32 Stride, uint32 Offset)
	{
		check(IsOutsideRenderPass());
		if (Bypass())
		{
			CMD_CONTEXT(RHISetStreamSource)(StreamIndex, VertexBuffer, Stride, Offset);
			return;
		}
		new (AllocCommand<FRHICommandSetStreamSource>()) FRHICommandSetStreamSourceDEPRECATED(StreamIndex, VertexBuffer, Stride, Offset);
	}
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	FORCEINLINE_DEBUGGABLE void SetStreamSource(uint32 StreamIndex, FVertexBufferRHIParamRef VertexBuffer, uint32 Offset)
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHISetStreamSource)(StreamIndex, VertexBuffer, Offset);
			return;
		}
		new (AllocCommand<FRHICommandSetStreamSource>()) FRHICommandSetStreamSource(StreamIndex, VertexBuffer, Offset);
	}

	FORCEINLINE_DEBUGGABLE void SetStencilRef(uint32 StencilRef)
	{
		check(IsOutsideRenderPass());
		if (Bypass())
		{
			CMD_CONTEXT(RHISetStencilRef)(StencilRef);
			return;
		}

		new(AllocCommand<FRHICommandSetStencilRef>()) FRHICommandSetStencilRef(StencilRef);
	}

	FORCEINLINE_DEBUGGABLE void SetViewport(uint32 MinX, uint32 MinY, float MinZ, uint32 MaxX, uint32 MaxY, float MaxZ)
	{
		check(IsOutsideRenderPass());
		if (Bypass())
		{
			CMD_CONTEXT(RHISetViewport)(MinX, MinY, MinZ, MaxX, MaxY, MaxZ);
			return;
		}
		new (AllocCommand<FRHICommandSetViewport>()) FRHICommandSetViewport(MinX, MinY, MinZ, MaxX, MaxY, MaxZ);
	}

	FORCEINLINE_DEBUGGABLE void SetStereoViewport(uint32 LeftMinX, uint32 RightMinX, uint32 LeftMinY, uint32 RightMinY, float MinZ, uint32 LeftMaxX, uint32 RightMaxX, uint32 LeftMaxY, uint32 RightMaxY, float MaxZ)
	{
		check(IsOutsideRenderPass());
		if (Bypass())
		{
			CMD_CONTEXT(RHISetStereoViewport)(LeftMinX, RightMinX, LeftMinY, RightMinY, MinZ, LeftMaxX, RightMaxX, LeftMaxY, RightMaxY, MaxZ);
			return;
		}
		new (AllocCommand<FRHICommandSetStereoViewport>()) FRHICommandSetStereoViewport(LeftMinX, RightMinX, LeftMinY, RightMinY, MinZ, LeftMaxX, RightMaxX, LeftMaxY, RightMaxY, MaxZ);
	}

	FORCEINLINE_DEBUGGABLE void SetScissorRect(bool bEnable, uint32 MinX, uint32 MinY, uint32 MaxX, uint32 MaxY)
	{
		check(IsOutsideRenderPass());
		if (Bypass())
		{
			CMD_CONTEXT(RHISetScissorRect)(bEnable, MinX, MinY, MaxX, MaxY);
			return;
		}
		new (AllocCommand<FRHICommandSetScissorRect>()) FRHICommandSetScissorRect(bEnable, MinX, MinY, MaxX, MaxY);
	}

	void ApplyCachedRenderTargets(
		FGraphicsPipelineStateInitializer& GraphicsPSOInit
		)
	{
		GraphicsPSOInit.RenderTargetsEnabled = PSOContext.CachedNumSimultanousRenderTargets;

		for (uint32 i = 0; i < GraphicsPSOInit.RenderTargetsEnabled; ++i)
		{
			if (PSOContext.CachedRenderTargets[i].Texture)
			{
				GraphicsPSOInit.RenderTargetFormats[i] = PSOContext.CachedRenderTargets[i].Texture->GetFormat();
				GraphicsPSOInit.RenderTargetFlags[i] = PSOContext.CachedRenderTargets[i].Texture->GetFlags();
			}
			else
			{
				GraphicsPSOInit.RenderTargetFormats[i] = PF_Unknown;
			}

			GraphicsPSOInit.RenderTargetLoadActions[i] = PSOContext.CachedRenderTargets[i].LoadAction;
			GraphicsPSOInit.RenderTargetStoreActions[i] = PSOContext.CachedRenderTargets[i].StoreAction;

			if (GraphicsPSOInit.RenderTargetFormats[i] != PF_Unknown)
			{
				GraphicsPSOInit.NumSamples = PSOContext.CachedRenderTargets[i].Texture->GetNumSamples();
			}
		}

		if (PSOContext.CachedDepthStencilTarget.Texture)
		{
			GraphicsPSOInit.DepthStencilTargetFormat = PSOContext.CachedDepthStencilTarget.Texture->GetFormat();
			GraphicsPSOInit.DepthStencilTargetFlag = PSOContext.CachedDepthStencilTarget.Texture->GetFlags();
		}
		else
		{
			GraphicsPSOInit.DepthStencilTargetFormat = PF_Unknown;
		}

		GraphicsPSOInit.DepthTargetLoadAction = PSOContext.CachedDepthStencilTarget.DepthLoadAction;
		GraphicsPSOInit.DepthTargetStoreAction = PSOContext.CachedDepthStencilTarget.DepthStoreAction;
		GraphicsPSOInit.StencilTargetLoadAction = PSOContext.CachedDepthStencilTarget.StencilLoadAction;
		GraphicsPSOInit.StencilTargetStoreAction = PSOContext.CachedDepthStencilTarget.GetStencilStoreAction();

		if (GraphicsPSOInit.DepthStencilTargetFormat != PF_Unknown)
		{
			GraphicsPSOInit.NumSamples = PSOContext.CachedDepthStencilTarget.Texture->GetNumSamples();
		}
	}

	FORCEINLINE_DEBUGGABLE void SetRenderTargets(
		uint32 NewNumSimultaneousRenderTargets,
		const FRHIRenderTargetView* NewRenderTargetsRHI,
		const FRHIDepthRenderTargetView* NewDepthStencilTargetRHI,
		uint32 NewNumUAVs,
		const FUnorderedAccessViewRHIParamRef* UAVs
		)
	{
		check(IsOutsideRenderPass());
		CacheActiveRenderTargets(
			NewNumSimultaneousRenderTargets, 
			NewRenderTargetsRHI, 
			NewDepthStencilTargetRHI
			);

		if (Bypass())
		{
			CMD_CONTEXT(RHISetRenderTargets)(
				NewNumSimultaneousRenderTargets,
				NewRenderTargetsRHI,
				NewDepthStencilTargetRHI,
				NewNumUAVs,
				UAVs);
			return;
		}
		new (AllocCommand<FRHICommandSetRenderTargets>()) FRHICommandSetRenderTargets(
			NewNumSimultaneousRenderTargets,
			NewRenderTargetsRHI,
			NewDepthStencilTargetRHI,
			NewNumUAVs,
			UAVs);
	}

	FORCEINLINE_DEBUGGABLE void SetRenderTargetsAndClear(const FRHISetRenderTargetsInfo& RenderTargetsInfo)
	{
		check(IsOutsideRenderPass());
		CacheActiveRenderTargets(
			RenderTargetsInfo.NumColorRenderTargets, 
			RenderTargetsInfo.ColorRenderTarget, 
			&RenderTargetsInfo.DepthStencilRenderTarget
			);

		if (Bypass())
		{
			CMD_CONTEXT(RHISetRenderTargetsAndClear)(RenderTargetsInfo);
			return;
		}
		new (AllocCommand<FRHICommandSetRenderTargetsAndClear>()) FRHICommandSetRenderTargetsAndClear(RenderTargetsInfo);
	}	

	FORCEINLINE_DEBUGGABLE void BindClearMRTValues(bool bClearColor, bool bClearDepth, bool bClearStencil)
	{
		check(IsOutsideRenderPass());
		if (Bypass())
		{
			CMD_CONTEXT(RHIBindClearMRTValues)(bClearColor, bClearDepth, bClearStencil);
			return;
		}
		new (AllocCommand<FRHICommandBindClearMRTValues>()) FRHICommandBindClearMRTValues(bClearColor, bClearDepth, bClearStencil);
	}	

	FORCEINLINE_DEBUGGABLE void BeginDrawPrimitiveUP(uint32 PrimitiveType, uint32 NumPrimitives, uint32 NumVertices, uint32 VertexDataStride, void*& OutVertexData)
	{
		check(IsOutsideRenderPass());
		if (Bypass())
		{
			CMD_CONTEXT(RHIBeginDrawPrimitiveUP)(PrimitiveType, NumPrimitives, NumVertices, VertexDataStride, OutVertexData);
			return;
		}
		check(!DrawUPData.OutVertexData && NumVertices * VertexDataStride > 0);

		OutVertexData = Alloc(NumVertices * VertexDataStride, 16);

		DrawUPData.PrimitiveType = PrimitiveType;
		DrawUPData.NumPrimitives = NumPrimitives;
		DrawUPData.NumVertices = NumVertices;
		DrawUPData.VertexDataStride = VertexDataStride;
		DrawUPData.OutVertexData = OutVertexData;
	}

	FORCEINLINE_DEBUGGABLE void EndDrawPrimitiveUP()
	{
		check(IsOutsideRenderPass());
		if (Bypass())
		{
			CMD_CONTEXT(RHIEndDrawPrimitiveUP)();
			return;
		}
		check(DrawUPData.OutVertexData && DrawUPData.NumVertices);
		new (AllocCommand<FRHICommandEndDrawPrimitiveUP>()) FRHICommandEndDrawPrimitiveUP(DrawUPData.PrimitiveType, DrawUPData.NumPrimitives, DrawUPData.NumVertices, DrawUPData.VertexDataStride, DrawUPData.OutVertexData);

		DrawUPData.OutVertexData = nullptr;
		DrawUPData.NumVertices = 0;
	}

	FORCEINLINE_DEBUGGABLE void BeginDrawIndexedPrimitiveUP(uint32 PrimitiveType, uint32 NumPrimitives, uint32 NumVertices, uint32 VertexDataStride, void*& OutVertexData, uint32 MinVertexIndex, uint32 NumIndices, uint32 IndexDataStride, void*& OutIndexData)
	{
		check(IsOutsideRenderPass());
		if (Bypass())
		{
			CMD_CONTEXT(RHIBeginDrawIndexedPrimitiveUP)(PrimitiveType, NumPrimitives, NumVertices, VertexDataStride, OutVertexData, MinVertexIndex, NumIndices, IndexDataStride, OutIndexData);
			return;
		}
		check(!DrawUPData.OutVertexData && !DrawUPData.OutIndexData && NumVertices * VertexDataStride > 0 && NumIndices * IndexDataStride > 0);

		OutVertexData = Alloc(NumVertices * VertexDataStride, 16);
		OutIndexData = Alloc(NumIndices * IndexDataStride, 16);

		DrawUPData.PrimitiveType = PrimitiveType;
		DrawUPData.NumPrimitives = NumPrimitives;
		DrawUPData.NumVertices = NumVertices;
		DrawUPData.VertexDataStride = VertexDataStride;
		DrawUPData.OutVertexData = OutVertexData;
		DrawUPData.MinVertexIndex = MinVertexIndex;
		DrawUPData.NumIndices = NumIndices;
		DrawUPData.IndexDataStride = IndexDataStride;
		DrawUPData.OutIndexData = OutIndexData;

	}

	FORCEINLINE_DEBUGGABLE void EndDrawIndexedPrimitiveUP()
	{
		check(IsOutsideRenderPass());
		if (Bypass())
		{
			CMD_CONTEXT(RHIEndDrawIndexedPrimitiveUP)();
			return;
		}
		check(DrawUPData.OutVertexData && DrawUPData.OutIndexData && DrawUPData.NumIndices && DrawUPData.NumVertices);

		new (AllocCommand<FRHICommandEndDrawIndexedPrimitiveUP>()) FRHICommandEndDrawIndexedPrimitiveUP(DrawUPData.PrimitiveType, DrawUPData.NumPrimitives, DrawUPData.NumVertices, DrawUPData.VertexDataStride, DrawUPData.OutVertexData, DrawUPData.MinVertexIndex, DrawUPData.NumIndices, DrawUPData.IndexDataStride, DrawUPData.OutIndexData);

		DrawUPData.OutVertexData = nullptr;
		DrawUPData.OutIndexData = nullptr;
		DrawUPData.NumIndices = 0;
		DrawUPData.NumVertices = 0;
	}

	FORCEINLINE_DEBUGGABLE void SetComputeShader(FComputeShaderRHIParamRef ComputeShader)
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHISetComputeShader)(ComputeShader);
			return;
		}
		new (AllocCommand<FRHICommandSetComputeShader<ECmdList::EGfx>>()) FRHICommandSetComputeShader<ECmdList::EGfx>(ComputeShader);
	}

	FORCEINLINE_DEBUGGABLE void SetComputePipelineState(class FComputePipelineState* ComputePipelineState)
	{
		if (Bypass())
		{
			extern RHI_API FRHIComputePipelineState* ExecuteSetComputePipelineState(class FComputePipelineState* ComputePipelineState);
			FRHIComputePipelineState* RHIComputePipelineState = ExecuteSetComputePipelineState(ComputePipelineState);
			CMD_CONTEXT(RHISetComputePipelineState)(RHIComputePipelineState);
			return;
		}
		new (AllocCommand<FRHICommandSetComputePipelineState<ECmdList::EGfx>>()) FRHICommandSetComputePipelineState<ECmdList::EGfx>(ComputePipelineState);
	}

	FORCEINLINE_DEBUGGABLE void SetGraphicsPipelineState(class FGraphicsPipelineState* GraphicsPipelineState)
	{
		check(IsOutsideRenderPass());
		if (Bypass())
		{
			extern RHI_API FRHIGraphicsPipelineState* ExecuteSetGraphicsPipelineState(class FGraphicsPipelineState* GraphicsPipelineState);
			FRHIGraphicsPipelineState* RHIGraphicsPipelineState = ExecuteSetGraphicsPipelineState(GraphicsPipelineState);
			CMD_CONTEXT(RHISetGraphicsPipelineState)(RHIGraphicsPipelineState);
			return;
		}
		new (AllocCommand<FRHICommandSetGraphicsPipelineState>()) FRHICommandSetGraphicsPipelineState(GraphicsPipelineState);
	}

	FORCEINLINE_DEBUGGABLE void DispatchComputeShader(uint32 ThreadGroupCountX, uint32 ThreadGroupCountY, uint32 ThreadGroupCountZ)
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHIDispatchComputeShader)(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);
			return;
		}
		new (AllocCommand<FRHICommandDispatchComputeShader<ECmdList::EGfx>>()) FRHICommandDispatchComputeShader<ECmdList::EGfx>(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);
	}

	FORCEINLINE_DEBUGGABLE void DispatchIndirectComputeShader(FVertexBufferRHIParamRef ArgumentBuffer, uint32 ArgumentOffset)
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHIDispatchIndirectComputeShader)(ArgumentBuffer, ArgumentOffset);
			return;
		}
		new (AllocCommand<FRHICommandDispatchIndirectComputeShader<ECmdList::EGfx>>()) FRHICommandDispatchIndirectComputeShader<ECmdList::EGfx>(ArgumentBuffer, ArgumentOffset);
	}

	FORCEINLINE_DEBUGGABLE void AutomaticCacheFlushAfterComputeShader(bool bEnable)
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHIAutomaticCacheFlushAfterComputeShader)(bEnable);
			return;
		}
		new (AllocCommand<FRHICommandAutomaticCacheFlushAfterComputeShader>()) FRHICommandAutomaticCacheFlushAfterComputeShader(bEnable);
	}

	FORCEINLINE_DEBUGGABLE void FlushComputeShaderCache()
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHIFlushComputeShaderCache)();
			return;
		}
		new (AllocCommand<FRHICommandFlushComputeShaderCache>()) FRHICommandFlushComputeShaderCache();
	}

	FORCEINLINE_DEBUGGABLE void DrawPrimitiveIndirect(uint32 PrimitiveType, FVertexBufferRHIParamRef ArgumentBuffer, uint32 ArgumentOffset)
	{
		check(IsOutsideRenderPass());
		if (Bypass())
		{
			CMD_CONTEXT(RHIDrawPrimitiveIndirect)(PrimitiveType, ArgumentBuffer, ArgumentOffset);
			return;
		}
		new (AllocCommand<FRHICommandDrawPrimitiveIndirect>()) FRHICommandDrawPrimitiveIndirect(PrimitiveType, ArgumentBuffer, ArgumentOffset);
	}

	FORCEINLINE_DEBUGGABLE void DrawIndexedIndirect(FIndexBufferRHIParamRef IndexBufferRHI, uint32 PrimitiveType, FStructuredBufferRHIParamRef ArgumentsBufferRHI, uint32 DrawArgumentsIndex, uint32 NumInstances)
	{
		check(IsOutsideRenderPass());
		if (Bypass())
		{
			CMD_CONTEXT(RHIDrawIndexedIndirect)(IndexBufferRHI, PrimitiveType, ArgumentsBufferRHI, DrawArgumentsIndex, NumInstances);
			return;
		}
		new (AllocCommand<FRHICommandDrawIndexedIndirect>()) FRHICommandDrawIndexedIndirect(IndexBufferRHI, PrimitiveType, ArgumentsBufferRHI, DrawArgumentsIndex, NumInstances);
	}

	FORCEINLINE_DEBUGGABLE void DrawIndexedPrimitiveIndirect(uint32 PrimitiveType, FIndexBufferRHIParamRef IndexBuffer, FVertexBufferRHIParamRef ArgumentsBuffer, uint32 ArgumentOffset)
	{
		check(IsOutsideRenderPass());
		if (Bypass())
		{
			CMD_CONTEXT(RHIDrawIndexedPrimitiveIndirect)(PrimitiveType, IndexBuffer, ArgumentsBuffer, ArgumentOffset);
			return;
		}
		new (AllocCommand<FRHICommandDrawIndexedPrimitiveIndirect>()) FRHICommandDrawIndexedPrimitiveIndirect(PrimitiveType, IndexBuffer, ArgumentsBuffer, ArgumentOffset);
	}

	FORCEINLINE_DEBUGGABLE void EnableDepthBoundsTest(bool bEnable, float MinDepth, float MaxDepth)
	{
		check(IsOutsideRenderPass());
		if (Bypass())
		{
			CMD_CONTEXT(RHIEnableDepthBoundsTest)(bEnable, MinDepth, MaxDepth);
			return;
		}
		new (AllocCommand<FRHICommandEnableDepthBoundsTest>()) FRHICommandEnableDepthBoundsTest(bEnable, MinDepth, MaxDepth);
	}

	FORCEINLINE_DEBUGGABLE void CopyToResolveTarget(FTextureRHIParamRef SourceTextureRHI, FTextureRHIParamRef DestTextureRHI, bool bKeepOriginalSurface, const FResolveParams& ResolveParams)
	{
		check(IsOutsideRenderPass());
		if (Bypass())
		{
			CMD_CONTEXT(RHICopyToResolveTarget)(SourceTextureRHI, DestTextureRHI, bKeepOriginalSurface, ResolveParams);
			return;
		}
		new (AllocCommand<FRHICommandCopyToResolveTarget>()) FRHICommandCopyToResolveTarget(SourceTextureRHI, DestTextureRHI, bKeepOriginalSurface, ResolveParams);
	}

	FORCEINLINE_DEBUGGABLE void CopyTexture(FTextureRHIParamRef SourceTextureRHI, FTextureRHIParamRef DestTextureRHI, const FResolveParams& ResolveParams)
	{
		check(IsOutsideRenderPass());
		if (Bypass())
		{
			CMD_CONTEXT(RHICopyTexture)(SourceTextureRHI, DestTextureRHI, ResolveParams);
			return;
		}
		new (AllocCommand<FRHICommandCopyTexture>()) FRHICommandCopyTexture(SourceTextureRHI, DestTextureRHI, ResolveParams);
	}

	FORCEINLINE_DEBUGGABLE void ClearTinyUAV(FUnorderedAccessViewRHIParamRef UnorderedAccessViewRHI, const uint32(&Values)[4])
	{
		check(IsOutsideRenderPass());
		if (Bypass())
		{
			CMD_CONTEXT(RHIClearTinyUAV)(UnorderedAccessViewRHI, Values);
			return;
		}
		new (AllocCommand<FRHICommandClearTinyUAV>()) FRHICommandClearTinyUAV(UnorderedAccessViewRHI, Values);
	}

	// NVCHANGE_BEGIN: Add HBAO+
#if WITH_GFSDK_SSAO

	FORCEINLINE_DEBUGGABLE void RenderHBAO(
		const FTextureRHIParamRef SceneDepthTextureRHI,
		const FMatrix& ProjectionMatrix,
		const FTextureRHIParamRef SceneNormalTextureRHI,
		const FMatrix& ViewMatrix,
		const FTextureRHIParamRef SceneColorTextureRHI,
		const GFSDK_SSAO_Parameters& AOParams
	)
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHIRenderHBAO)(
				SceneDepthTextureRHI,
				ProjectionMatrix,
				SceneNormalTextureRHI,
				ViewMatrix,
				SceneColorTextureRHI,
				AOParams);
			return;
		}
		new (AllocCommand<FRHICommandRenderHBAO>()) FRHICommandRenderHBAO(
			SceneDepthTextureRHI,
			ProjectionMatrix,
			SceneNormalTextureRHI,
			ViewMatrix,
			SceneColorTextureRHI,
			AOParams);
	}

#endif
	// NVCHANGE_END: Add HBAO+

	FORCEINLINE_DEBUGGABLE void BeginRenderQuery(FRenderQueryRHIParamRef RenderQuery)
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHIBeginRenderQuery)(RenderQuery);
			return;
		}
		new (AllocCommand<FRHICommandBeginRenderQuery>()) FRHICommandBeginRenderQuery(RenderQuery);
	}
	FORCEINLINE_DEBUGGABLE void EndRenderQuery(FRenderQueryRHIParamRef RenderQuery)
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHIEndRenderQuery)(RenderQuery);
			return;
		}
		new (AllocCommand<FRHICommandEndRenderQuery>()) FRHICommandEndRenderQuery(RenderQuery);
	}

	FORCEINLINE_DEBUGGABLE void BeginOcclusionQueryBatch()
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHIBeginOcclusionQueryBatch)();
			return;
		}
		new (AllocCommand<FRHICommandBeginOcclusionQueryBatch>()) FRHICommandBeginOcclusionQueryBatch();
	}

	FORCEINLINE_DEBUGGABLE void EndOcclusionQueryBatch()
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHIEndOcclusionQueryBatch)();
			return;
		}
		new (AllocCommand<FRHICommandEndOcclusionQueryBatch>()) FRHICommandEndOcclusionQueryBatch();
	}

	FORCEINLINE_DEBUGGABLE void SubmitCommandsHint()
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHISubmitCommandsHint)();
			return;
		}
		new (AllocCommand<FRHICommandSubmitCommandsHint<ECmdList::EGfx>>()) FRHICommandSubmitCommandsHint<ECmdList::EGfx>();
	}

	FORCEINLINE_DEBUGGABLE void TransitionResource(EResourceTransitionAccess TransitionType, FTextureRHIParamRef InTexture)
	{
		FTextureRHIParamRef Texture = InTexture;
		check(Texture == nullptr || Texture->IsCommitted());
		if (Bypass())
		{
			CMD_CONTEXT(RHITransitionResources)(TransitionType, &Texture, 1);
			return;
		}

		// Allocate space to hold the single texture pointer inline in the command list itself.
		FTextureRHIParamRef* TextureArray = (FTextureRHIParamRef*)Alloc(sizeof(FTextureRHIParamRef), alignof(FTextureRHIParamRef));
		TextureArray[0] = Texture;
		new (AllocCommand<FRHICommandTransitionTextures>()) FRHICommandTransitionTextures(TransitionType, TextureArray, 1);
	}

	FORCEINLINE_DEBUGGABLE void TransitionResources(EResourceTransitionAccess TransitionType, FTextureRHIParamRef* InTextures, int32 NumTextures)
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHITransitionResources)(TransitionType, InTextures, NumTextures);
			return;
		}

		// Allocate space to hold the list of textures inline in the command list itself.
		FTextureRHIParamRef* InlineTextureArray = (FTextureRHIParamRef*)Alloc(sizeof(FTextureRHIParamRef) * NumTextures, alignof(FTextureRHIParamRef));
		for (int32 Index = 0; Index < NumTextures; ++Index)
		{
			InlineTextureArray[Index] = InTextures[Index];
		}

		new (AllocCommand<FRHICommandTransitionTextures>()) FRHICommandTransitionTextures(TransitionType, InlineTextureArray, NumTextures);
	}

	FORCEINLINE_DEBUGGABLE void TransitionResourceArrayNoCopy(EResourceTransitionAccess TransitionType, TArray<FTextureRHIParamRef>& InTextures)
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHITransitionResources)(TransitionType, &InTextures[0], InTextures.Num());
			return;
		}
		new (AllocCommand<FRHICommandTransitionTexturesArray>()) FRHICommandTransitionTexturesArray(TransitionType, InTextures);
	}

	FORCEINLINE_DEBUGGABLE void TransitionResource(EResourceTransitionAccess TransitionType, EResourceTransitionPipeline TransitionPipeline, FUnorderedAccessViewRHIParamRef InUAV, FComputeFenceRHIParamRef WriteFence)
	{
		FUnorderedAccessViewRHIParamRef UAV = InUAV;
		check(InUAV == nullptr || InUAV->IsCommitted());
		if (Bypass())
		{
			CMD_CONTEXT(RHITransitionResources)(TransitionType, TransitionPipeline, &UAV, 1, WriteFence);
			return;
		}

		// Allocate space to hold the single UAV pointer inline in the command list itself.
		FUnorderedAccessViewRHIParamRef* UAVArray = (FUnorderedAccessViewRHIParamRef*)Alloc(sizeof(FUnorderedAccessViewRHIParamRef), alignof(FUnorderedAccessViewRHIParamRef));
		UAVArray[0] = UAV;
		new (AllocCommand<FRHICommandTransitionUAVs<ECmdList::EGfx>>()) FRHICommandTransitionUAVs<ECmdList::EGfx>(TransitionType, TransitionPipeline, UAVArray, 1, WriteFence);
	}

	FORCEINLINE_DEBUGGABLE void TransitionResource(EResourceTransitionAccess TransitionType, EResourceTransitionPipeline TransitionPipeline, FUnorderedAccessViewRHIParamRef InUAV)
	{
		check(InUAV == nullptr || InUAV->IsCommitted());
		TransitionResource(TransitionType, TransitionPipeline, InUAV, nullptr);
	}

	FORCEINLINE_DEBUGGABLE void TransitionResources(EResourceTransitionAccess TransitionType, EResourceTransitionPipeline TransitionPipeline, FUnorderedAccessViewRHIParamRef* InUAVs, int32 NumUAVs, FComputeFenceRHIParamRef WriteFence)
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHITransitionResources)(TransitionType, TransitionPipeline, InUAVs, NumUAVs, WriteFence);
			return;
		}

		// Allocate space to hold the list UAV pointers inline in the command list itself.
		FUnorderedAccessViewRHIParamRef* UAVArray = (FUnorderedAccessViewRHIParamRef*)Alloc(sizeof(FUnorderedAccessViewRHIParamRef) * NumUAVs, alignof(FUnorderedAccessViewRHIParamRef));
		for (int32 Index = 0; Index < NumUAVs; ++Index)
		{
			UAVArray[Index] = InUAVs[Index];
		}

		new (AllocCommand<FRHICommandTransitionUAVs<ECmdList::EGfx>>()) FRHICommandTransitionUAVs<ECmdList::EGfx>(TransitionType, TransitionPipeline, UAVArray, NumUAVs, WriteFence);
	}

	FORCEINLINE_DEBUGGABLE void TransitionResources(EResourceTransitionAccess TransitionType, EResourceTransitionPipeline TransitionPipeline, FUnorderedAccessViewRHIParamRef* InUAVs, int32 NumUAVs)
	{
		TransitionResources(TransitionType, TransitionPipeline, InUAVs, NumUAVs, nullptr);
	}

	FORCEINLINE_DEBUGGABLE void WaitComputeFence(FComputeFenceRHIParamRef WaitFence)
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHIWaitComputeFence)(WaitFence);
			return;
		}
		new (AllocCommand<FRHICommandWaitComputeFence<ECmdList::EGfx>>()) FRHICommandWaitComputeFence<ECmdList::EGfx>(WaitFence);
	}

	FRHIRenderPassCommandList& BeginRenderPass(const FRHIRenderPassInfo& InInfo, const TCHAR* Name)
	{
		check(IsOutsideRenderPass());
		FRHIRenderPassCommandList& RenderPassCmdList = *(FRHIRenderPassCommandList*)this;
		Data.LocalRHIRenderPass = new(Alloc<FLocalCmdListRenderPass>()) FLocalCmdListRenderPass;

		CacheActiveRenderTargets(InInfo);

		if (Bypass())
		{
			check(!Data.LocalRHIRenderPass->RenderPass.GetReference());
			Data.LocalRHIRenderPass->RenderPass = CMD_CONTEXT(RHIBeginRenderPass)(InInfo, Name);
		}
		else
		{
			TCHAR* NameCopy  = AllocString(Name);
			new (AllocCommand<FRHICommandBeginRenderPass>()) FRHICommandBeginRenderPass(InInfo, Data.LocalRHIRenderPass, NameCopy);
		}
		return RenderPassCmdList;
	}

	void EndRenderPass(FRHIRenderPassCommandList& RenderPass)
	{
		check(Data.LocalRHIRenderPass);
		if (Bypass())
		{
			CMD_CONTEXT(RHIEndRenderPass)(Data.LocalRHIRenderPass->RenderPass);
		}
		else
		{
			new (AllocCommand<FRHICommandEndRenderPass>()) FRHICommandEndRenderPass(Data.LocalRHIRenderPass);
		}

		Data.LocalRHIRenderPass = nullptr;
	}

	FRHIParallelRenderPassCommandList& BeginParallelRenderPass(const FRHIRenderPassInfo& InInfo, const TCHAR* InName)
	{
		check(IsOutsideRenderPass());
		FRHIParallelRenderPassCommandList& ParallelRenderPassCmdList = *(FRHIParallelRenderPassCommandList*)this;
		Data.LocalRHIParallelRenderPass = new(Alloc<FLocalCmdListParallelRenderPass>()) FLocalCmdListParallelRenderPass;

		CacheActiveRenderTargets(InInfo);

		if (Bypass())
		{
			Data.LocalRHIParallelRenderPass->RenderPass = CMD_CONTEXT(RHIBeginParallelRenderPass)(InInfo, InName);
		}
		else
		{
			TCHAR* NameCopy  = AllocString(InName);
			new (AllocCommand<FRHICommandBeginParallelRenderPass>()) FRHICommandBeginParallelRenderPass(InInfo, Data.LocalRHIParallelRenderPass, NameCopy);
		}
		return ParallelRenderPassCmdList;
	}

	void EndParallelRenderPass(FRHIParallelRenderPassCommandList& ParallelRenderPass)
	{
		check(Data.LocalRHIParallelRenderPass);

		if (Bypass())
		{
			CMD_CONTEXT(RHIEndParallelRenderPass)(Data.LocalRHIParallelRenderPass->RenderPass);
		}
		else
		{
			// Cmd list deletion happens during FRHICommandEndRenderPass::Execute()
			new (AllocCommand<FRHICommandEndParallelRenderPass>()) FRHICommandEndParallelRenderPass(Data.LocalRHIParallelRenderPass);
		}

		Data.LocalRHIParallelRenderPass = nullptr;
	}

	// These 6 are special in that they must be called on the immediate command list and they force a flush only when we are not doing RHI thread
	void BeginScene();
	void EndScene();
	void BeginDrawingViewport(FViewportRHIParamRef Viewport, FTextureRHIParamRef RenderTargetRHI);
	void EndDrawingViewport(FViewportRHIParamRef Viewport, bool bPresent, bool bLockToVsync);
	void BeginFrame();
	void EndFrame();

	FORCEINLINE_DEBUGGABLE void PushEvent(const TCHAR* Name, FColor Color)
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHIPushEvent)(Name, Color);
			return;
		}
		TCHAR* NameCopy  = AllocString(Name);
		new (AllocCommand<FRHICommandPushEvent<ECmdList::EGfx>>()) FRHICommandPushEvent<ECmdList::EGfx>(NameCopy, Color);
	}

	FORCEINLINE_DEBUGGABLE void PopEvent()
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHIPopEvent)();
			return;
		}
		new (AllocCommand<FRHICommandPopEvent<ECmdList::EGfx>>()) FRHICommandPopEvent<ECmdList::EGfx>();
	}
	
	FORCEINLINE_DEBUGGABLE void RHIInvalidateCachedState()
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHIInvalidateCachedState)();
			return;
		}
		new (AllocCommand<FRHICommandInvalidateCachedState>()) FRHICommandInvalidateCachedState();
	}
	
	FORCEINLINE_DEBUGGABLE void BreakPoint()
	{
#if !UE_BUILD_SHIPPING
		if (Bypass())
		{
			if (FPlatformMisc::IsDebuggerPresent())
			{
				FPlatformMisc::DebugBreak();
			}
			return;
		}
		new (AllocCommand<FRHICommandDebugBreak>()) FRHICommandDebugBreak();
#endif
	}

	// NvFlow begin
	FORCEINLINE_DEBUGGABLE void NvFlowWork(void(*WorkFunc)(void*,SIZE_T,IRHICommandContext*), void* ParamData, SIZE_T NumBytes)
	{
		if (Bypass())
		{
			CMD_CONTEXT(NvFlowWork)(WorkFunc, ParamData, NumBytes);
			return;
		}
		// need local copy
		void* UseData = ParamData;
		if (NumBytes > 0u)
		{
			UseData = Alloc(NumBytes, 16u);
			FMemory::Memcpy(UseData, ParamData, NumBytes);
		}
		new (AllocCommand<FRHICommandNvFlowWork>()) FRHICommandNvFlowWork(WorkFunc, UseData, NumBytes);
	}
	// NvFlow end

	// NVCHANGE_BEGIN: Add VXGI
#if WITH_GFSDK_VXGI
	FORCEINLINE_DEBUGGABLE void VXGICleanupAfterVoxelization()
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHIVXGICleanupAfterVoxelization)();
			return;
		}
		new (AllocCommand<FRHIVXGICleanupAfterVoxelization>()) FRHIVXGICleanupAfterVoxelization();
	}

	FORCEINLINE_DEBUGGABLE void SetViewportsAndScissorRects(uint32 Count, const FViewportBounds* Viewports, const FScissorRect* ScissorRects)
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHISetViewportsAndScissorRects)(Count, Viewports, ScissorRects);
			return;
		}
		new (AllocCommand<FRHISetViewportsAndScissorRects>()) FRHISetViewportsAndScissorRects(Count, Viewports, ScissorRects);
	}

	FORCEINLINE_DEBUGGABLE void DispatchIndirectComputeShaderStructured(FStructuredBufferRHIParamRef ArgumentBuffer, uint32 ArgumentOffset)
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHIDispatchIndirectComputeShaderStructured)(ArgumentBuffer, ArgumentOffset);
			return;
		}
		new (AllocCommand<FRHIDispatchIndirectComputeShaderStructured>()) FRHIDispatchIndirectComputeShaderStructured(ArgumentBuffer, ArgumentOffset);
	}

	FORCEINLINE_DEBUGGABLE void CopyStructuredBufferData(FStructuredBufferRHIParamRef DestBuffer, uint32 DestOffset, FStructuredBufferRHIParamRef SrcBuffer, uint32 SrcOffset, uint32 DataSize)
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHICopyStructuredBufferData)(DestBuffer, DestOffset, SrcBuffer, SrcOffset, DataSize);
			return;
		}
		new (AllocCommand<FRHICopyStructuredBufferData>()) FRHICopyStructuredBufferData(DestBuffer, DestOffset, SrcBuffer, SrcOffset, DataSize);
	}

	FORCEINLINE_DEBUGGABLE void ExecuteVxgiRenderingCommand(NVRHI::IRenderThreadCommand* Command)
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHIExecuteVxgiRenderingCommand)(Command);
			return;
		}
		new (AllocCommand<FRHIExecuteVxgiRenderingCommand>()) FRHIExecuteVxgiRenderingCommand(Command);
	}
#endif
	// NVCHANGE_END: Add VXGI

	// NVCHANGE_BEGIN: Nvidia Volumetric Lighting
#if WITH_NVVOLUMETRICLIGHTING
	FORCEINLINE_DEBUGGABLE void BeginAccumulation(FTextureRHIParamRef SceneDepthTextureRHI, const TArray<NvVl::ViewerDesc>& ViewerDescs, const NvVl::MediumDesc& MediumDesc, NvVl::DebugFlags DebugFlags)
	{
		if (Bypass())
		{
			if (GNVVolumetricLightingRHI)
			{
				GNVVolumetricLightingRHI->BeginAccumulation(SceneDepthTextureRHI, ViewerDescs, MediumDesc, DebugFlags);
			}
			return;
		}
		new (AllocCommand<FRHICommandBeginAccumulation>()) FRHICommandBeginAccumulation(SceneDepthTextureRHI, ViewerDescs, MediumDesc, DebugFlags);
	}

	FORCEINLINE_DEBUGGABLE void RenderVolume(const TArray<FTextureRHIParamRef>& ShadowMapTextures, const NvVl::ShadowMapDesc& ShadowMapDesc, const NvVl::LightDesc& LightDesc, const NvVl::VolumeDesc& VolumeDesc)
	{
		if (Bypass())
		{
			if (GNVVolumetricLightingRHI)
			{
				GNVVolumetricLightingRHI->RenderVolume(ShadowMapTextures, ShadowMapDesc, LightDesc, VolumeDesc);
			}
			return;
		}
		new (AllocCommand<FRHICommandRenderVolume>()) FRHICommandRenderVolume(ShadowMapTextures, ShadowMapDesc, LightDesc, VolumeDesc);
	}

	FORCEINLINE_DEBUGGABLE void EndAccumulation()
	{
		if (Bypass())
		{
			if (GNVVolumetricLightingRHI)
			{
				GNVVolumetricLightingRHI->EndAccumulation();
			}
			return;
		}
		new (AllocCommand<FRHICommandEndAccumulation>()) FRHICommandEndAccumulation();
	}

	FORCEINLINE_DEBUGGABLE void ApplyLighting(FTextureRHIParamRef SceneColorSurfaceRHI, const NvVl::PostprocessDesc& PostprocessDesc)
	{
		if (Bypass())
		{
			if (GNVVolumetricLightingRHI)
			{
				GNVVolumetricLightingRHI->ApplyLighting(SceneColorSurfaceRHI, PostprocessDesc);
			}
			return;
		}
		new (AllocCommand<FRHICommandApplyLighting>()) FRHICommandApplyLighting(SceneColorSurfaceRHI, PostprocessDesc);
	}
#endif
	// NVCHANGE_END: Nvidia Volumetric Lighting

};

class RHI_API FRHIAsyncComputeCommandList : public FRHICommandListBase
{
public:

	/** Custom new/delete with recycling */
	void* operator new(size_t Size);
	void operator delete(void *RawMemory);

	FORCEINLINE_DEBUGGABLE void SetShaderUniformBuffer(FComputeShaderRHIParamRef Shader, uint32 BaseIndex, FUniformBufferRHIParamRef UniformBuffer)
	{
		if (Bypass())
		{
			COMPUTE_CONTEXT(RHISetShaderUniformBuffer)(Shader, BaseIndex, UniformBuffer);
			return;
		}
		new (AllocCommand<FRHICommandSetShaderUniformBuffer<FComputeShaderRHIParamRef, ECmdList::ECompute> >()) FRHICommandSetShaderUniformBuffer<FComputeShaderRHIParamRef, ECmdList::ECompute>(Shader, BaseIndex, UniformBuffer);		
	}
	
	FORCEINLINE void SetShaderUniformBuffer(FComputeShaderRHIRef& Shader, uint32 BaseIndex, FUniformBufferRHIParamRef UniformBuffer)
	{
		SetShaderUniformBuffer(Shader.GetReference(), BaseIndex, UniformBuffer);
	}

	FORCEINLINE_DEBUGGABLE void SetShaderParameter(FComputeShaderRHIParamRef Shader, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue)
	{
		if (Bypass())
		{
			COMPUTE_CONTEXT(RHISetShaderParameter)(Shader, BufferIndex, BaseIndex, NumBytes, NewValue);
			return;
		}
		void* UseValue = Alloc(NumBytes, 16);
		FMemory::Memcpy(UseValue, NewValue, NumBytes);
		new (AllocCommand<FRHICommandSetShaderParameter<FComputeShaderRHIParamRef, ECmdList::ECompute> >()) FRHICommandSetShaderParameter<FComputeShaderRHIParamRef, ECmdList::ECompute>(Shader, BufferIndex, BaseIndex, NumBytes, UseValue);
	}
	
	FORCEINLINE void SetShaderParameter(FComputeShaderRHIRef& Shader, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue)
	{
		SetShaderParameter(Shader.GetReference(), BufferIndex, BaseIndex, NumBytes, NewValue);
	}
	
	FORCEINLINE_DEBUGGABLE void SetShaderTexture(FComputeShaderRHIParamRef Shader, uint32 TextureIndex, FTextureRHIParamRef Texture)
	{
		if (Bypass())
		{
			COMPUTE_CONTEXT(RHISetShaderTexture)(Shader, TextureIndex, Texture);
			return;
		}
		new (AllocCommand<FRHICommandSetShaderTexture<FComputeShaderRHIParamRef, ECmdList::ECompute> >()) FRHICommandSetShaderTexture<FComputeShaderRHIParamRef, ECmdList::ECompute>(Shader, TextureIndex, Texture);
	}
	
	FORCEINLINE_DEBUGGABLE void SetShaderResourceViewParameter(FComputeShaderRHIParamRef Shader, uint32 SamplerIndex, FShaderResourceViewRHIParamRef SRV)
	{
		if (Bypass())
		{
			COMPUTE_CONTEXT(RHISetShaderResourceViewParameter)(Shader, SamplerIndex, SRV);
			return;
		}
		new (AllocCommand<FRHICommandSetShaderResourceViewParameter<FComputeShaderRHIParamRef, ECmdList::ECompute> >()) FRHICommandSetShaderResourceViewParameter<FComputeShaderRHIParamRef, ECmdList::ECompute>(Shader, SamplerIndex, SRV);
	}
	
	FORCEINLINE_DEBUGGABLE void SetShaderSampler(FComputeShaderRHIParamRef Shader, uint32 SamplerIndex, FSamplerStateRHIParamRef State)
	{
		if (Bypass())
		{
			COMPUTE_CONTEXT(RHISetShaderSampler)(Shader, SamplerIndex, State);
			return;
		}
		new (AllocCommand<FRHICommandSetShaderSampler<FComputeShaderRHIParamRef, ECmdList::ECompute> >()) FRHICommandSetShaderSampler<FComputeShaderRHIParamRef, ECmdList::ECompute>(Shader, SamplerIndex, State);
	}

	FORCEINLINE_DEBUGGABLE void SetUAVParameter(FComputeShaderRHIParamRef Shader, uint32 UAVIndex, FUnorderedAccessViewRHIParamRef UAV)
	{
		if (Bypass())
		{
			COMPUTE_CONTEXT(RHISetUAVParameter)(Shader, UAVIndex, UAV);
			return;
		}
		new (AllocCommand<FRHICommandSetUAVParameter<FComputeShaderRHIParamRef, ECmdList::ECompute> >()) FRHICommandSetUAVParameter<FComputeShaderRHIParamRef, ECmdList::ECompute>(Shader, UAVIndex, UAV);
	}

	FORCEINLINE_DEBUGGABLE void SetUAVParameter(FComputeShaderRHIParamRef Shader, uint32 UAVIndex, FUnorderedAccessViewRHIParamRef UAV, uint32 InitialCount)
	{
		if (Bypass())
		{
			COMPUTE_CONTEXT(RHISetUAVParameter)(Shader, UAVIndex, UAV, InitialCount);
			return;
		}
		new (AllocCommand<FRHICommandSetUAVParameter_IntialCount<FComputeShaderRHIParamRef, ECmdList::ECompute> >()) FRHICommandSetUAVParameter_IntialCount<FComputeShaderRHIParamRef, ECmdList::ECompute>(Shader, UAVIndex, UAV, InitialCount);
	}
	
	FORCEINLINE_DEBUGGABLE void SetComputeShader(FComputeShaderRHIParamRef ComputeShader)
	{
		if (Bypass())
		{
			COMPUTE_CONTEXT(RHISetComputeShader)(ComputeShader);
			return;
		}
		new (AllocCommand<FRHICommandSetComputeShader<ECmdList::ECompute> >()) FRHICommandSetComputeShader<ECmdList::ECompute>(ComputeShader);
	}

	FORCEINLINE_DEBUGGABLE void SetComputePipelineState(FComputePipelineState* ComputePipelineState)
	{
		if (Bypass())
		{
			extern FRHIComputePipelineState* ExecuteSetComputePipelineState(FComputePipelineState* ComputePipelineState);
			FRHIComputePipelineState* RHIComputePipelineState = ExecuteSetComputePipelineState(ComputePipelineState);
			COMPUTE_CONTEXT(RHISetComputePipelineState)(RHIComputePipelineState);
			return;
		}
		new (AllocCommand<FRHICommandSetComputePipelineState<ECmdList::ECompute> >()) FRHICommandSetComputePipelineState<ECmdList::ECompute>(ComputePipelineState);
	}

	FORCEINLINE_DEBUGGABLE void SetAsyncComputeBudget(EAsyncComputeBudget Budget)
	{
		if (Bypass())
		{
			COMPUTE_CONTEXT(RHISetAsyncComputeBudget)(Budget);
			return;
		}
		new (AllocCommand<FRHICommandSetAsyncComputeBudget<ECmdList::ECompute>>()) FRHICommandSetAsyncComputeBudget<ECmdList::ECompute>(Budget);
	}

	FORCEINLINE_DEBUGGABLE void DispatchComputeShader(uint32 ThreadGroupCountX, uint32 ThreadGroupCountY, uint32 ThreadGroupCountZ)
	{
		if (Bypass())
		{
			COMPUTE_CONTEXT(RHIDispatchComputeShader)(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);
			return;
		}
		new (AllocCommand<FRHICommandDispatchComputeShader<ECmdList::ECompute> >()) FRHICommandDispatchComputeShader<ECmdList::ECompute>(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);
	}

	FORCEINLINE_DEBUGGABLE void DispatchIndirectComputeShader(FVertexBufferRHIParamRef ArgumentBuffer, uint32 ArgumentOffset)
	{
		if (Bypass())
		{
			COMPUTE_CONTEXT(RHIDispatchIndirectComputeShader)(ArgumentBuffer, ArgumentOffset);
			return;
		}
		new (AllocCommand<FRHICommandDispatchIndirectComputeShader<ECmdList::ECompute> >()) FRHICommandDispatchIndirectComputeShader<ECmdList::ECompute>(ArgumentBuffer, ArgumentOffset);
	}

	FORCEINLINE_DEBUGGABLE void TransitionResource(EResourceTransitionAccess TransitionType, EResourceTransitionPipeline TransitionPipeline, FUnorderedAccessViewRHIParamRef InUAV, FComputeFenceRHIParamRef WriteFence)
	{
		FUnorderedAccessViewRHIParamRef UAV = InUAV;
		if (Bypass())
		{
			COMPUTE_CONTEXT(RHITransitionResources)(TransitionType, TransitionPipeline, &UAV, 1, WriteFence);
			return;
		}

		// Allocate space to hold the single UAV pointer inline in the command list itself.
		FUnorderedAccessViewRHIParamRef* UAVArray = (FUnorderedAccessViewRHIParamRef*)Alloc(sizeof(FUnorderedAccessViewRHIParamRef), alignof(FUnorderedAccessViewRHIParamRef));
		UAVArray[0] = UAV;
		new (AllocCommand<FRHICommandTransitionUAVs<ECmdList::ECompute> >()) FRHICommandTransitionUAVs<ECmdList::ECompute>(TransitionType, TransitionPipeline, UAVArray, 1, WriteFence);
	}

	FORCEINLINE_DEBUGGABLE void TransitionResource(EResourceTransitionAccess TransitionType, EResourceTransitionPipeline TransitionPipeline, FUnorderedAccessViewRHIParamRef InUAV)
	{
		TransitionResource(TransitionType, TransitionPipeline, InUAV, nullptr);
	}

	FORCEINLINE_DEBUGGABLE void TransitionResources(EResourceTransitionAccess TransitionType, EResourceTransitionPipeline TransitionPipeline, FUnorderedAccessViewRHIParamRef* InUAVs, int32 NumUAVs, FComputeFenceRHIParamRef WriteFence)
	{
		if (Bypass())
		{
			COMPUTE_CONTEXT(RHITransitionResources)(TransitionType, TransitionPipeline, InUAVs, NumUAVs, WriteFence);
			return;
		}

		// Allocate space to hold the list UAV pointers inline in the command list itself.
		FUnorderedAccessViewRHIParamRef* UAVArray = (FUnorderedAccessViewRHIParamRef*)Alloc(sizeof(FUnorderedAccessViewRHIParamRef) * NumUAVs, alignof(FUnorderedAccessViewRHIParamRef));
		for (int32 Index = 0; Index < NumUAVs; ++Index)
		{
			UAVArray[Index] = InUAVs[Index];
		}

		new (AllocCommand<FRHICommandTransitionUAVs<ECmdList::ECompute> >()) FRHICommandTransitionUAVs<ECmdList::ECompute>(TransitionType, TransitionPipeline, UAVArray, NumUAVs, WriteFence);
	}

	FORCEINLINE_DEBUGGABLE void TransitionResources(EResourceTransitionAccess TransitionType, EResourceTransitionPipeline TransitionPipeline, FUnorderedAccessViewRHIParamRef* InUAVs, int32 NumUAVs)
	{
		TransitionResources(TransitionType, TransitionPipeline, InUAVs, NumUAVs, nullptr);
	}
	
	FORCEINLINE_DEBUGGABLE void PushEvent(const TCHAR* Name, FColor Color)
	{
		if (Bypass())
		{
			COMPUTE_CONTEXT(RHIPushEvent)(Name, Color);
			return;
		}
		TCHAR* NameCopy  = AllocString(Name);
		new (AllocCommand<FRHICommandPushEvent<ECmdList::ECompute> >()) FRHICommandPushEvent<ECmdList::ECompute>(NameCopy, Color);
	}

	FORCEINLINE_DEBUGGABLE void PopEvent()
	{
		if (Bypass())
		{
			COMPUTE_CONTEXT(RHIPopEvent)();
			return;
		}
		new (AllocCommand<FRHICommandPopEvent<ECmdList::ECompute> >()) FRHICommandPopEvent<ECmdList::ECompute>();
	}

	FORCEINLINE_DEBUGGABLE void BreakPoint()
	{
#if !UE_BUILD_SHIPPING
		if (Bypass())
		{
			if (FPlatformMisc::IsDebuggerPresent())
			{
				FPlatformMisc::DebugBreak();
			}
			return;
		}
		new (AllocCommand<FRHICommandDebugBreak>()) FRHICommandDebugBreak();
#endif
	}

	FORCEINLINE_DEBUGGABLE void SubmitCommandsHint()
	{
		if (Bypass())
		{
			COMPUTE_CONTEXT(RHISubmitCommandsHint)();
			return;
		}
		new (AllocCommand<FRHICommandSubmitCommandsHint<ECmdList::ECompute>>()) FRHICommandSubmitCommandsHint<ECmdList::ECompute>();
	}

	FORCEINLINE_DEBUGGABLE void WaitComputeFence(FComputeFenceRHIParamRef WaitFence)
	{
		if (Bypass())
		{
			COMPUTE_CONTEXT(RHIWaitComputeFence)(WaitFence);
			return;
		}
		new (AllocCommand<FRHICommandWaitComputeFence<ECmdList::ECompute>>()) FRHICommandWaitComputeFence<ECmdList::ECompute>(WaitFence);
	}
};

namespace EImmediateFlushType
{
	enum Type
	{ 
		WaitForOutstandingTasksOnly = 0, 
		DispatchToRHIThread, 
		WaitForDispatchToRHIThread,
		FlushRHIThread,
		FlushRHIThreadFlushResources
	};
};

class FScopedRHIThreadStaller
{
	class FRHICommandListImmediate* Immed; // non-null if we need to unstall
public:
	FScopedRHIThreadStaller(class FRHICommandListImmediate& InImmed);
	~FScopedRHIThreadStaller();
};

class RHI_API FRHICommandListImmediate : public FRHICommandList
{
	friend class FRHICommandListExecutor;
	FRHICommandListImmediate()
	{
		Data.Type = FRHICommandListBase::FCommonData::ECmdListType::Immediate;
	}
	~FRHICommandListImmediate()
	{
		check(!HasCommands());
	}
public:

	void ImmediateFlush(EImmediateFlushType::Type FlushType);
	bool StallRHIThread();
	void UnStallRHIThread();
	static bool IsStalled();

	void SetCurrentStat(TStatId Stat);

	static FGraphEventRef RenderThreadTaskFence();
	static void WaitOnRenderThreadTaskFence(FGraphEventRef& Fence);
	static bool AnyRenderThreadTasksOutstanding();
	FGraphEventRef RHIThreadFence(bool bSetLockFence = false);

	//Queue the given async compute commandlists in order with the current immediate commandlist
	void QueueAsyncCompute(FRHIAsyncComputeCommandList& RHIComputeCmdList);
	
	FORCEINLINE FSamplerStateRHIRef CreateSamplerState(const FSamplerStateInitializerRHI& Initializer)
	{
		LLM_SCOPE(ELLMTag::RHIMisc);
		return RHICreateSamplerState(Initializer);
	}
	
	FORCEINLINE FRasterizerStateRHIRef CreateRasterizerState(const FRasterizerStateInitializerRHI& Initializer)
	{
		LLM_SCOPE(ELLMTag::RHIMisc);
		return RHICreateRasterizerState(Initializer);
	}
	
	FORCEINLINE FDepthStencilStateRHIRef CreateDepthStencilState(const FDepthStencilStateInitializerRHI& Initializer)
	{
		LLM_SCOPE(ELLMTag::RHIMisc);
		return RHICreateDepthStencilState(Initializer);
	}
	
	FORCEINLINE FBlendStateRHIRef CreateBlendState(const FBlendStateInitializerRHI& Initializer)
	{
		LLM_SCOPE(ELLMTag::RHIMisc);
		return RHICreateBlendState(Initializer);
	}
	
	FORCEINLINE FVertexDeclarationRHIRef CreateVertexDeclaration(const FVertexDeclarationElementList& Elements)
	{
		LLM_SCOPE(ELLMTag::Shaders);
		return GDynamicRHI->CreateVertexDeclaration_RenderThread(*this, Elements);
	}
	
	FORCEINLINE FPixelShaderRHIRef CreatePixelShader(const TArray<uint8>& Code)
	{
		LLM_SCOPE(ELLMTag::Shaders);
		return GDynamicRHI->CreatePixelShader_RenderThread(*this, Code);
	}
	
	FORCEINLINE FPixelShaderRHIRef CreatePixelShader(FRHIShaderLibraryParamRef Library, FSHAHash Hash)
	{
		LLM_SCOPE(ELLMTag::Shaders);
		return GDynamicRHI->CreatePixelShader_RenderThread(*this, Library, Hash);
	}
	
	FORCEINLINE FVertexShaderRHIRef CreateVertexShader(const TArray<uint8>& Code)
	{
		LLM_SCOPE(ELLMTag::Shaders);
		return GDynamicRHI->CreateVertexShader_RenderThread(*this, Code);
	}
	
	FORCEINLINE FVertexShaderRHIRef CreateVertexShader(FRHIShaderLibraryParamRef Library, FSHAHash Hash)
	{
		LLM_SCOPE(ELLMTag::Shaders);
		return GDynamicRHI->CreateVertexShader_RenderThread(*this, Library, Hash);
	}
	
	FORCEINLINE FHullShaderRHIRef CreateHullShader(const TArray<uint8>& Code)
	{
		LLM_SCOPE(ELLMTag::Shaders);
		return GDynamicRHI->CreateHullShader_RenderThread(*this, Code);
	}
	
	FORCEINLINE FHullShaderRHIRef CreateHullShader(FRHIShaderLibraryParamRef Library, FSHAHash Hash)
	{
		LLM_SCOPE(ELLMTag::Shaders);
		return GDynamicRHI->CreateHullShader_RenderThread(*this, Library, Hash);
	}
	
	FORCEINLINE FDomainShaderRHIRef CreateDomainShader(const TArray<uint8>& Code)
	{
		LLM_SCOPE(ELLMTag::Shaders);
		return GDynamicRHI->CreateDomainShader_RenderThread(*this, Code);
	}
	
	FORCEINLINE FDomainShaderRHIRef CreateDomainShader(FRHIShaderLibraryParamRef Library, FSHAHash Hash)
	{
		LLM_SCOPE(ELLMTag::Shaders);
		return GDynamicRHI->CreateDomainShader_RenderThread(*this, Library, Hash);
	}
	
	FORCEINLINE FGeometryShaderRHIRef CreateGeometryShader(const TArray<uint8>& Code)
	{
		LLM_SCOPE(ELLMTag::Shaders);
		return GDynamicRHI->CreateGeometryShader_RenderThread(*this, Code);
	}
	
	FORCEINLINE FGeometryShaderRHIRef CreateGeometryShader(FRHIShaderLibraryParamRef Library, FSHAHash Hash)
	{
		LLM_SCOPE(ELLMTag::Shaders);
		return GDynamicRHI->CreateGeometryShader_RenderThread(*this, Library, Hash);
	}
	
	FORCEINLINE FGeometryShaderRHIRef CreateGeometryShaderWithStreamOutput(const TArray<uint8>& Code, const FStreamOutElementList& ElementList, uint32 NumStrides, const uint32* Strides, int32 RasterizedStream)
	{
		LLM_SCOPE(ELLMTag::Shaders);
		return GDynamicRHI->CreateGeometryShaderWithStreamOutput_RenderThread(*this, Code, ElementList, NumStrides, Strides, RasterizedStream);
	}
	
	FORCEINLINE FGeometryShaderRHIRef CreateGeometryShaderWithStreamOutput(const FStreamOutElementList& ElementList, uint32 NumStrides, const uint32* Strides, int32 RasterizedStream, FRHIShaderLibraryParamRef Library, FSHAHash Hash)
	{
		LLM_SCOPE(ELLMTag::Shaders);
		return GDynamicRHI->CreateGeometryShaderWithStreamOutput_RenderThread(*this, ElementList, NumStrides, Strides, RasterizedStream, Library, Hash);
	}
	
	FORCEINLINE FComputeShaderRHIRef CreateComputeShader(const TArray<uint8>& Code)
	{
		LLM_SCOPE(ELLMTag::Shaders);
		return GDynamicRHI->CreateComputeShader_RenderThread(*this, Code);
	}
	
	FORCEINLINE FComputeShaderRHIRef CreateComputeShader(FRHIShaderLibraryParamRef Library, FSHAHash Hash)
	{
		LLM_SCOPE(ELLMTag::Shaders);
		return GDynamicRHI->CreateComputeShader_RenderThread(*this, Library, Hash);
	}
	
	FORCEINLINE FComputeFenceRHIRef CreateComputeFence(const FName& Name)
	{		
		return GDynamicRHI->RHICreateComputeFence(Name);
	}	
	
	FORCEINLINE FBoundShaderStateRHIRef CreateBoundShaderState(FVertexDeclarationRHIParamRef VertexDeclaration, FVertexShaderRHIParamRef VertexShader, FHullShaderRHIParamRef HullShader, FDomainShaderRHIParamRef DomainShader, FPixelShaderRHIParamRef PixelShader, FGeometryShaderRHIParamRef GeometryShader)
	{
		LLM_SCOPE(ELLMTag::Shaders);
		return RHICreateBoundShaderState(VertexDeclaration, VertexShader, HullShader, DomainShader, PixelShader, GeometryShader);
	}

	FORCEINLINE FGraphicsPipelineStateRHIRef CreateGraphicsPipelineState(const FGraphicsPipelineStateInitializer& Initializer)
	{
		LLM_SCOPE(ELLMTag::Shaders);
		return RHICreateGraphicsPipelineState(Initializer);
	}

	FORCEINLINE TRefCountPtr<FRHIComputePipelineState> CreateComputePipelineState(FRHIComputeShader* ComputeShader)
	{
		LLM_SCOPE(ELLMTag::Shaders);
		return RHICreateComputePipelineState(ComputeShader);
	}

	FORCEINLINE FUniformBufferRHIRef CreateUniformBuffer(const void* Contents, const FRHIUniformBufferLayout& Layout, EUniformBufferUsage Usage)
	{
		LLM_SCOPE(ELLMTag::RHIMisc);
		return RHICreateUniformBuffer(Contents, Layout, Usage);
	}
	
	FORCEINLINE FIndexBufferRHIRef CreateAndLockIndexBuffer(uint32 Stride, uint32 Size, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo, void*& OutDataBuffer)
	{
		LLM_SCOPE(ELLMTag::Meshes);
		return GDynamicRHI->CreateAndLockIndexBuffer_RenderThread(*this, Stride, Size, InUsage, CreateInfo, OutDataBuffer);
	}
	
	FORCEINLINE FIndexBufferRHIRef CreateIndexBuffer(uint32 Stride, uint32 Size, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo)
	{
		LLM_SCOPE(ELLMTag::Meshes);
		return GDynamicRHI->CreateIndexBuffer_RenderThread(*this, Stride, Size, InUsage, CreateInfo);
	}
	
	FORCEINLINE void* LockIndexBuffer(FIndexBufferRHIParamRef IndexBuffer, uint32 Offset, uint32 SizeRHI, EResourceLockMode LockMode)
	{
		LLM_SCOPE(ELLMTag::Meshes);
		return GDynamicRHI->LockIndexBuffer_RenderThread(*this, IndexBuffer, Offset, SizeRHI, LockMode);
	}
	
	FORCEINLINE void UnlockIndexBuffer(FIndexBufferRHIParamRef IndexBuffer)
	{
		GDynamicRHI->UnlockIndexBuffer_RenderThread(*this, IndexBuffer);
	}
	
	FORCEINLINE FVertexBufferRHIRef CreateAndLockVertexBuffer(uint32 Size, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo, void*& OutDataBuffer)
	{
		LLM_SCOPE(ELLMTag::Meshes);
		return GDynamicRHI->CreateAndLockVertexBuffer_RenderThread(*this, Size, InUsage, CreateInfo, OutDataBuffer);
	}

	FORCEINLINE FVertexBufferRHIRef CreateVertexBuffer(uint32 Size, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo)
	{
		LLM_SCOPE(ELLMTag::Meshes);
		return GDynamicRHI->CreateVertexBuffer_RenderThread(*this, Size, InUsage, CreateInfo);
	}
	
	FORCEINLINE void* LockVertexBuffer(FVertexBufferRHIParamRef VertexBuffer, uint32 Offset, uint32 SizeRHI, EResourceLockMode LockMode)
	{
		LLM_SCOPE(ELLMTag::Meshes);
		return GDynamicRHI->LockVertexBuffer_RenderThread(*this, VertexBuffer, Offset, SizeRHI, LockMode);
	}
	
	FORCEINLINE void UnlockVertexBuffer(FVertexBufferRHIParamRef VertexBuffer)
	{
		GDynamicRHI->UnlockVertexBuffer_RenderThread(*this, VertexBuffer);
	}
	
	FORCEINLINE void CopyVertexBuffer(FVertexBufferRHIParamRef SourceBuffer,FVertexBufferRHIParamRef DestBuffer)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_CopyVertexBuffer_Flush);
		ImmediateFlush(EImmediateFlushType::FlushRHIThread);  
		GDynamicRHI->RHICopyVertexBuffer(SourceBuffer,DestBuffer);
	}

	FORCEINLINE FStructuredBufferRHIRef CreateStructuredBuffer(uint32 Stride, uint32 Size, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo)
	{
		LLM_SCOPE(ELLMTag::RHIMisc);
		return GDynamicRHI->CreateStructuredBuffer_RenderThread(*this, Stride, Size, InUsage, CreateInfo);
	}
	
	FORCEINLINE void* LockStructuredBuffer(FStructuredBufferRHIParamRef StructuredBuffer, uint32 Offset, uint32 SizeRHI, EResourceLockMode LockMode)
	{
		LLM_SCOPE(ELLMTag::RHIMisc);
		QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_LockStructuredBuffer_Flush);
		ImmediateFlush(EImmediateFlushType::FlushRHIThread); 
		 
		return GDynamicRHI->RHILockStructuredBuffer(StructuredBuffer, Offset, SizeRHI, LockMode);
	}
	
	FORCEINLINE void UnlockStructuredBuffer(FStructuredBufferRHIParamRef StructuredBuffer)
	{
		LLM_SCOPE(ELLMTag::RHIMisc);
		QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_UnlockStructuredBuffer_Flush);
		ImmediateFlush(EImmediateFlushType::FlushRHIThread); 
		  
		GDynamicRHI->RHIUnlockStructuredBuffer(StructuredBuffer);
	}
	
	FORCEINLINE FUnorderedAccessViewRHIRef CreateUnorderedAccessView(FStructuredBufferRHIParamRef StructuredBuffer, bool bUseUAVCounter, bool bAppendBuffer)
	{
		LLM_SCOPE(ELLMTag::RHIMisc);
		return GDynamicRHI->RHICreateUnorderedAccessView_RenderThread(*this, StructuredBuffer, bUseUAVCounter, bAppendBuffer);
	}
	
	FORCEINLINE FUnorderedAccessViewRHIRef CreateUnorderedAccessView(FTextureRHIParamRef Texture, uint32 MipLevel)
	{
		LLM_SCOPE(ELLMTag::RHIMisc);
		return GDynamicRHI->RHICreateUnorderedAccessView_RenderThread(*this, Texture, MipLevel);
	}
	
	FORCEINLINE FUnorderedAccessViewRHIRef CreateUnorderedAccessView(FVertexBufferRHIParamRef VertexBuffer, uint8 Format)
	{
		LLM_SCOPE(ELLMTag::RHIMisc);
		return GDynamicRHI->RHICreateUnorderedAccessView_RenderThread(*this, VertexBuffer, Format);
	}
	
	FORCEINLINE FShaderResourceViewRHIRef CreateShaderResourceView(FStructuredBufferRHIParamRef StructuredBuffer)
	{
		LLM_SCOPE(ELLMTag::RHIMisc);
		return GDynamicRHI->RHICreateShaderResourceView_RenderThread(*this, StructuredBuffer);
	}
	
	FORCEINLINE FShaderResourceViewRHIRef CreateShaderResourceView(FVertexBufferRHIParamRef VertexBuffer, uint32 Stride, uint8 Format)
	{
		LLM_SCOPE(ELLMTag::RHIMisc);
		return GDynamicRHI->CreateShaderResourceView_RenderThread(*this, VertexBuffer, Stride, Format);
	}
	
	FORCEINLINE FShaderResourceViewRHIRef CreateShaderResourceView(FIndexBufferRHIParamRef Buffer)
	{
		LLM_SCOPE(ELLMTag::RHIMisc);
		return GDynamicRHI->CreateShaderResourceView_RenderThread(*this, Buffer);
	}
	
	FORCEINLINE uint64 CalcTexture2DPlatformSize(uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, uint32 NumSamples, uint32 Flags, uint32& OutAlign)
	{
		return RHICalcTexture2DPlatformSize(SizeX, SizeY, Format, NumMips, NumSamples, Flags, OutAlign);
	}
	
	FORCEINLINE uint64 CalcTexture3DPlatformSize(uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint8 Format, uint32 NumMips, uint32 Flags, uint32& OutAlign)
	{
		return RHICalcTexture3DPlatformSize(SizeX, SizeY, SizeZ, Format, NumMips, Flags, OutAlign);
	}
	
	FORCEINLINE uint64 CalcTextureCubePlatformSize(uint32 Size, uint8 Format, uint32 NumMips, uint32 Flags, uint32& OutAlign)
	{
		return RHICalcTextureCubePlatformSize(Size, Format, NumMips, Flags, OutAlign);
	}
	
	FORCEINLINE void GetTextureMemoryStats(FTextureMemoryStats& OutStats)
	{
		RHIGetTextureMemoryStats(OutStats);
	}
	
	FORCEINLINE bool GetTextureMemoryVisualizeData(FColor* TextureData,int32 SizeX,int32 SizeY,int32 Pitch,int32 PixelSize)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_GetTextureMemoryVisualizeData_Flush);
		ImmediateFlush(EImmediateFlushType::FlushRHIThread); 
		return GDynamicRHI->RHIGetTextureMemoryVisualizeData(TextureData,SizeX,SizeY,Pitch,PixelSize);
	}
	
	FORCEINLINE FTextureReferenceRHIRef CreateTextureReference(FLastRenderTimeContainer* LastRenderTime)
	{
		LLM_SCOPE(ELLMTag::Textures);
		FScopedRHIThreadStaller StallRHIThread(*this);
		return GDynamicRHI->RHICreateTextureReference(LastRenderTime);
	}
	
	FORCEINLINE FTexture2DRHIRef CreateTexture2D(uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, uint32 NumSamples, uint32 Flags, FRHIResourceCreateInfo& CreateInfo)
	{
		LLM_SCOPE((Flags & (TexCreate_RenderTargetable | TexCreate_DepthStencilTargetable)) != 0 ? ELLMTag::RenderTargets : ELLMTag::Textures);
		return GDynamicRHI->RHICreateTexture2D_RenderThread(*this, SizeX, SizeY, Format, NumMips, NumSamples, Flags, CreateInfo);
	}

	FORCEINLINE FTexture2DRHIRef CreateTextureExternal2D(uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, uint32 NumSamples, uint32 Flags, FRHIResourceCreateInfo& CreateInfo)
	{
		return GDynamicRHI->RHICreateTextureExternal2D_RenderThread(*this, SizeX, SizeY, Format, NumMips, NumSamples, Flags, CreateInfo);
	}

	FORCEINLINE FStructuredBufferRHIRef CreateRTWriteMaskBuffer(FTexture2DRHIRef RenderTarget)
	{
		LLM_SCOPE(ELLMTag::RenderTargets);
		return GDynamicRHI->RHICreateRTWriteMaskBuffer(RenderTarget);
	}

	FORCEINLINE FTexture2DRHIRef AsyncCreateTexture2D(uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, uint32 Flags, void** InitialMipData, uint32 NumInitialMips)
	{
		LLM_SCOPE((Flags & (TexCreate_RenderTargetable | TexCreate_DepthStencilTargetable)) != 0 ? ELLMTag::RenderTargets : ELLMTag::Textures);
		return GDynamicRHI->RHIAsyncCreateTexture2D(SizeX, SizeY, Format, NumMips, Flags, InitialMipData, NumInitialMips);
	}
	
	FORCEINLINE void CopySharedMips(FTexture2DRHIParamRef DestTexture2D, FTexture2DRHIParamRef SrcTexture2D)
	{
		LLM_SCOPE(ELLMTag::Textures);
		QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_CopySharedMips_Flush);
		ImmediateFlush(EImmediateFlushType::FlushRHIThread); 
		 
		return GDynamicRHI->RHICopySharedMips(DestTexture2D, SrcTexture2D);
	}
	
	FORCEINLINE FTexture2DArrayRHIRef CreateTexture2DArray(uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint8 Format, uint32 NumMips, uint32 Flags, FRHIResourceCreateInfo& CreateInfo)
	{
		LLM_SCOPE((Flags & (TexCreate_RenderTargetable | TexCreate_DepthStencilTargetable)) != 0 ? ELLMTag::RenderTargets : ELLMTag::Textures);
		return GDynamicRHI->RHICreateTexture2DArray_RenderThread(*this, SizeX, SizeY, SizeZ, Format, NumMips, Flags, CreateInfo);
	}
	
	FORCEINLINE FTexture3DRHIRef CreateTexture3D(uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint8 Format, uint32 NumMips, uint32 Flags, FRHIResourceCreateInfo& CreateInfo)
	{
		LLM_SCOPE((Flags & (TexCreate_RenderTargetable | TexCreate_DepthStencilTargetable)) != 0 ? ELLMTag::RenderTargets : ELLMTag::Textures);
		return GDynamicRHI->RHICreateTexture3D_RenderThread(*this, SizeX, SizeY, SizeZ, Format, NumMips, Flags, CreateInfo);
	}
	
	FORCEINLINE void GetResourceInfo(FTextureRHIParamRef Ref, FRHIResourceInfo& OutInfo)
	{
		return RHIGetResourceInfo(Ref, OutInfo);
	}
	
	FORCEINLINE FShaderResourceViewRHIRef CreateShaderResourceView(FTexture2DRHIParamRef Texture2DRHI, uint8 MipLevel)
	{
		LLM_SCOPE(ELLMTag::RHIMisc);
		return GDynamicRHI->RHICreateShaderResourceView_RenderThread(*this, Texture2DRHI, MipLevel);
	}
	
	FORCEINLINE FShaderResourceViewRHIRef CreateShaderResourceView(FTexture2DRHIParamRef Texture2DRHI, uint8 MipLevel, uint8 NumMipLevels, uint8 Format)
	{
		LLM_SCOPE(ELLMTag::RHIMisc);
		return GDynamicRHI->RHICreateShaderResourceView_RenderThread(*this, Texture2DRHI, MipLevel, NumMipLevels, Format);
	}
	
	FORCEINLINE FShaderResourceViewRHIRef CreateShaderResourceView(FTexture3DRHIParamRef Texture3DRHI, uint8 MipLevel)
	{
		LLM_SCOPE(ELLMTag::RHIMisc);
		return GDynamicRHI->RHICreateShaderResourceView_RenderThread(*this, Texture3DRHI, MipLevel);
	}

	FORCEINLINE FShaderResourceViewRHIRef CreateShaderResourceView(FTexture2DArrayRHIParamRef Texture2DArrayRHI, uint8 MipLevel)
	{
		LLM_SCOPE(ELLMTag::RHIMisc);
		return GDynamicRHI->RHICreateShaderResourceView_RenderThread(*this, Texture2DArrayRHI, MipLevel);
	}

	FORCEINLINE FShaderResourceViewRHIRef CreateShaderResourceView(FTextureCubeRHIParamRef TextureCubeRHI, uint8 MipLevel)
	{
		LLM_SCOPE(ELLMTag::RHIMisc);
		return GDynamicRHI->RHICreateShaderResourceView_RenderThread(*this, TextureCubeRHI, MipLevel);
	}

	FORCEINLINE void GenerateMips(FTextureRHIParamRef Texture)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_GenerateMips_Flush);
		ImmediateFlush(EImmediateFlushType::FlushRHIThread); return GDynamicRHI->RHIGenerateMips(Texture);
	}
	
	FORCEINLINE uint32 ComputeMemorySize(FTextureRHIParamRef TextureRHI)
	{
		return RHIComputeMemorySize(TextureRHI);
	}
	
	FORCEINLINE FTexture2DRHIRef AsyncReallocateTexture2D(FTexture2DRHIParamRef Texture2D, int32 NewMipCount, int32 NewSizeX, int32 NewSizeY, FThreadSafeCounter* RequestStatus)
	{
		LLM_SCOPE(ELLMTag::Textures);
		return GDynamicRHI->AsyncReallocateTexture2D_RenderThread(*this, Texture2D, NewMipCount, NewSizeX, NewSizeY, RequestStatus);
	}
	
	FORCEINLINE ETextureReallocationStatus FinalizeAsyncReallocateTexture2D(FTexture2DRHIParamRef Texture2D, bool bBlockUntilCompleted)
	{
		LLM_SCOPE(ELLMTag::Textures);
		return GDynamicRHI->FinalizeAsyncReallocateTexture2D_RenderThread(*this, Texture2D, bBlockUntilCompleted);
	}
	
	FORCEINLINE ETextureReallocationStatus CancelAsyncReallocateTexture2D(FTexture2DRHIParamRef Texture2D, bool bBlockUntilCompleted)
	{
		return GDynamicRHI->CancelAsyncReallocateTexture2D_RenderThread(*this, Texture2D, bBlockUntilCompleted);
	}
	
	FORCEINLINE void* LockTexture2D(FTexture2DRHIParamRef Texture, uint32 MipIndex, EResourceLockMode LockMode, uint32& DestStride, bool bLockWithinMiptail, bool bFlushRHIThread = true)
	{
		LLM_SCOPE(ELLMTag::Textures);
		return GDynamicRHI->LockTexture2D_RenderThread(*this, Texture, MipIndex, LockMode, DestStride, bLockWithinMiptail, bFlushRHIThread);
	}
	
	FORCEINLINE void UnlockTexture2D(FTexture2DRHIParamRef Texture, uint32 MipIndex, bool bLockWithinMiptail, bool bFlushRHIThread = true)
	{		
		GDynamicRHI->UnlockTexture2D_RenderThread(*this, Texture, MipIndex, bLockWithinMiptail, bFlushRHIThread);
	}
	
	FORCEINLINE void* LockTexture2DArray(FTexture2DArrayRHIParamRef Texture, uint32 TextureIndex, uint32 MipIndex, EResourceLockMode LockMode, uint32& DestStride, bool bLockWithinMiptail)
	{
		LLM_SCOPE(ELLMTag::Textures);
		QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_LockTexture2DArray_Flush);
		ImmediateFlush(EImmediateFlushType::FlushRHIThread);  
		return GDynamicRHI->RHILockTexture2DArray(Texture, TextureIndex, MipIndex, LockMode, DestStride, bLockWithinMiptail);
	}
	
	FORCEINLINE void UnlockTexture2DArray(FTexture2DArrayRHIParamRef Texture, uint32 TextureIndex, uint32 MipIndex, bool bLockWithinMiptail)
	{
		LLM_SCOPE(ELLMTag::Textures);
		QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_UnlockTexture2DArray_Flush);
		ImmediateFlush(EImmediateFlushType::FlushRHIThread);   
		GDynamicRHI->RHIUnlockTexture2DArray(Texture, TextureIndex, MipIndex, bLockWithinMiptail);
	}
	
	FORCEINLINE void UpdateTexture2D(FTexture2DRHIParamRef Texture, uint32 MipIndex, const struct FUpdateTextureRegion2D& UpdateRegion, uint32 SourcePitch, const uint8* SourceData)
	{		
		LLM_SCOPED_TAG_WITH_STAT(STAT_TextureMemoryLLM, ELLMTracker::Default);
		checkf(UpdateRegion.DestX + UpdateRegion.Width <= Texture->GetSizeX(), TEXT("UpdateTexture2D out of bounds on X. Texture: %s, %i, %i, %i"), *Texture->GetName().ToString(), UpdateRegion.DestX, UpdateRegion.Width, Texture->GetSizeX());
		checkf(UpdateRegion.DestY + UpdateRegion.Height <= Texture->GetSizeY(), TEXT("UpdateTexture2D out of bounds on Y. Texture: %s, %i, %i, %i"), *Texture->GetName().ToString(), UpdateRegion.DestY, UpdateRegion.Height, Texture->GetSizeY());
		LLM_SCOPE(ELLMTag::Textures);
		GDynamicRHI->UpdateTexture2D_RenderThread(*this, Texture, MipIndex, UpdateRegion, SourcePitch, SourceData);
	}

	FORCEINLINE FUpdateTexture3DData BeginUpdateTexture3D(FTexture3DRHIParamRef Texture, uint32 MipIndex, const struct FUpdateTextureRegion3D& UpdateRegion)
	{
		checkf(UpdateRegion.DestX + UpdateRegion.Width <= Texture->GetSizeX(), TEXT("UpdateTexture3D out of bounds on X. Texture: %s, %i, %i, %i"), *Texture->GetName().ToString(), UpdateRegion.DestX, UpdateRegion.Width, Texture->GetSizeX());
		checkf(UpdateRegion.DestY + UpdateRegion.Height <= Texture->GetSizeY(), TEXT("UpdateTexture3D out of bounds on Y. Texture: %s, %i, %i, %i"), *Texture->GetName().ToString(), UpdateRegion.DestY, UpdateRegion.Height, Texture->GetSizeY());
		checkf(UpdateRegion.DestZ + UpdateRegion.Depth <= Texture->GetSizeZ(), TEXT("UpdateTexture3D out of bounds on Z. Texture: %s, %i, %i, %i"), *Texture->GetName().ToString(), UpdateRegion.DestZ, UpdateRegion.Depth, Texture->GetSizeZ());
		LLM_SCOPE(ELLMTag::Textures);
		return GDynamicRHI->BeginUpdateTexture3D_RenderThread(*this, Texture, MipIndex, UpdateRegion);
	}

	FORCEINLINE void EndUpdateTexture3D(FUpdateTexture3DData& UpdateData)
	{		
		LLM_SCOPE(ELLMTag::Textures);
		GDynamicRHI->EndUpdateTexture3D_RenderThread(*this, UpdateData);
	}
	
	FORCEINLINE void UpdateTexture3D(FTexture3DRHIParamRef Texture, uint32 MipIndex, const struct FUpdateTextureRegion3D& UpdateRegion, uint32 SourceRowPitch, uint32 SourceDepthPitch, const uint8* SourceData)
	{
		checkf(UpdateRegion.DestX + UpdateRegion.Width <= Texture->GetSizeX(), TEXT("UpdateTexture3D out of bounds on X. Texture: %s, %i, %i, %i"), *Texture->GetName().ToString(), UpdateRegion.DestX, UpdateRegion.Width, Texture->GetSizeX());
		checkf(UpdateRegion.DestY + UpdateRegion.Height <= Texture->GetSizeY(), TEXT("UpdateTexture3D out of bounds on Y. Texture: %s, %i, %i, %i"), *Texture->GetName().ToString(), UpdateRegion.DestY, UpdateRegion.Height, Texture->GetSizeY());
		checkf(UpdateRegion.DestZ + UpdateRegion.Depth <= Texture->GetSizeZ(), TEXT("UpdateTexture3D out of bounds on Z. Texture: %s, %i, %i, %i"), *Texture->GetName().ToString(), UpdateRegion.DestZ, UpdateRegion.Depth, Texture->GetSizeZ());
		LLM_SCOPE(ELLMTag::Textures);
		GDynamicRHI->UpdateTexture3D_RenderThread(*this, Texture, MipIndex, UpdateRegion, SourceRowPitch, SourceDepthPitch, SourceData);
	}
	
	FORCEINLINE FTextureCubeRHIRef CreateTextureCube(uint32 Size, uint8 Format, uint32 NumMips, uint32 Flags, FRHIResourceCreateInfo& CreateInfo)
	{
		LLM_SCOPE((Flags & (TexCreate_RenderTargetable | TexCreate_DepthStencilTargetable)) != 0 ? ELLMTag::RenderTargets : ELLMTag::Textures);
		return GDynamicRHI->RHICreateTextureCube_RenderThread(*this, Size, Format, NumMips, Flags, CreateInfo);
	}
	
	FORCEINLINE FTextureCubeRHIRef CreateTextureCubeArray(uint32 Size, uint32 ArraySize, uint8 Format, uint32 NumMips, uint32 Flags, FRHIResourceCreateInfo& CreateInfo)
	{
		LLM_SCOPE((Flags & (TexCreate_RenderTargetable | TexCreate_DepthStencilTargetable)) != 0 ? ELLMTag::RenderTargets : ELLMTag::Textures);
		return GDynamicRHI->RHICreateTextureCubeArray_RenderThread(*this, Size, ArraySize, Format, NumMips, Flags, CreateInfo);
	}
	
	FORCEINLINE void* LockTextureCubeFace(FTextureCubeRHIParamRef Texture, uint32 FaceIndex, uint32 ArrayIndex, uint32 MipIndex, EResourceLockMode LockMode, uint32& DestStride, bool bLockWithinMiptail)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_LockTextureCubeFace_Flush);
		ImmediateFlush(EImmediateFlushType::FlushRHIThread);  
		return GDynamicRHI->RHILockTextureCubeFace(Texture, FaceIndex, ArrayIndex, MipIndex, LockMode, DestStride, bLockWithinMiptail);
	}
	
	FORCEINLINE void UnlockTextureCubeFace(FTextureCubeRHIParamRef Texture, uint32 FaceIndex, uint32 ArrayIndex, uint32 MipIndex, bool bLockWithinMiptail)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_UnlockTextureCubeFace_Flush);
		ImmediateFlush(EImmediateFlushType::FlushRHIThread);   
		GDynamicRHI->RHIUnlockTextureCubeFace(Texture, FaceIndex, ArrayIndex, MipIndex, bLockWithinMiptail);
	}
	
	FORCEINLINE void BindDebugLabelName(FTextureRHIParamRef Texture, const TCHAR* Name)
	{
		RHIBindDebugLabelName(Texture, Name);
	}

	FORCEINLINE void BindDebugLabelName(FUnorderedAccessViewRHIParamRef UnorderedAccessViewRHI, const TCHAR* Name)
	{
		RHIBindDebugLabelName(UnorderedAccessViewRHI, Name);
	}

	FORCEINLINE void ReadSurfaceData(FTextureRHIParamRef Texture,FIntRect Rect,TArray<FColor>& OutData,FReadSurfaceDataFlags InFlags)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_ReadSurfaceData_Flush);
		ImmediateFlush(EImmediateFlushType::FlushRHIThread);  
		GDynamicRHI->RHIReadSurfaceData(Texture,Rect,OutData,InFlags);
	}

	FORCEINLINE void ReadSurfaceData(FTextureRHIParamRef Texture, FIntRect Rect, TArray<FLinearColor>& OutData, FReadSurfaceDataFlags InFlags)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_ReadSurfaceData_Flush);
		ImmediateFlush(EImmediateFlushType::FlushRHIThread);
		GDynamicRHI->RHIReadSurfaceData(Texture, Rect, OutData, InFlags);
	}
	
	FORCEINLINE void MapStagingSurface(FTextureRHIParamRef Texture,void*& OutData,int32& OutWidth,int32& OutHeight)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_MapStagingSurface_Flush);
		ImmediateFlush(EImmediateFlushType::FlushRHIThread);  
		GDynamicRHI->RHIMapStagingSurface(Texture,OutData,OutWidth,OutHeight);
	}
	
	FORCEINLINE void UnmapStagingSurface(FTextureRHIParamRef Texture)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_UnmapStagingSurface_Flush);
		ImmediateFlush(EImmediateFlushType::FlushRHIThread);  
		GDynamicRHI->RHIUnmapStagingSurface(Texture);
	}
	
	FORCEINLINE void ReadSurfaceFloatData(FTextureRHIParamRef Texture,FIntRect Rect,TArray<FFloat16Color>& OutData,ECubeFace CubeFace,int32 ArrayIndex,int32 MipIndex)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_ReadSurfaceFloatData_Flush);
		ImmediateFlush(EImmediateFlushType::FlushRHIThread);  
		GDynamicRHI->RHIReadSurfaceFloatData(Texture,Rect,OutData,CubeFace,ArrayIndex,MipIndex);
	}

	FORCEINLINE void Read3DSurfaceFloatData(FTextureRHIParamRef Texture,FIntRect Rect,FIntPoint ZMinMax,TArray<FFloat16Color>& OutData)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_Read3DSurfaceFloatData_Flush);
		ImmediateFlush(EImmediateFlushType::FlushRHIThread);  
		GDynamicRHI->RHIRead3DSurfaceFloatData(Texture,Rect,ZMinMax,OutData);
	}
	
	FORCEINLINE FRenderQueryRHIRef CreateRenderQuery(ERenderQueryType QueryType)
	{
		FScopedRHIThreadStaller StallRHIThread(*this);
		return GDynamicRHI->RHICreateRenderQuery(QueryType);
	}

	FORCEINLINE void AcquireTransientResource_RenderThread(FTextureRHIParamRef Texture)
	{
		if (!Texture->IsCommitted() )
		{
			if (GSupportsTransientResourceAliasing)
			{
				GDynamicRHI->RHIAcquireTransientResource_RenderThread(Texture);
			}
			Texture->SetCommitted(true);
		}
	}

	FORCEINLINE void DiscardTransientResource_RenderThread(FTextureRHIParamRef Texture)
	{
		if (Texture->IsCommitted())
		{
			if (GSupportsTransientResourceAliasing)
			{
				GDynamicRHI->RHIDiscardTransientResource_RenderThread(Texture);
			}
			Texture->SetCommitted(false);
		}
	}

	FORCEINLINE void AcquireTransientResource_RenderThread(FVertexBufferRHIParamRef Buffer)
	{
		if (!Buffer->IsCommitted())
		{
			if (GSupportsTransientResourceAliasing)
			{
				GDynamicRHI->RHIAcquireTransientResource_RenderThread(Buffer);
			}
			Buffer->SetCommitted(true);
		}
	}

	FORCEINLINE void DiscardTransientResource_RenderThread(FVertexBufferRHIParamRef Buffer)
	{
		if (Buffer->IsCommitted())
		{
			if (GSupportsTransientResourceAliasing)
			{
				GDynamicRHI->RHIDiscardTransientResource_RenderThread(Buffer);
			}
			Buffer->SetCommitted(false);
		}
	}

	FORCEINLINE void AcquireTransientResource_RenderThread(FStructuredBufferRHIParamRef Buffer)
	{
		if (!Buffer->IsCommitted())
		{
			if (GSupportsTransientResourceAliasing)
			{
				GDynamicRHI->RHIAcquireTransientResource_RenderThread(Buffer);
			}
			Buffer->SetCommitted(true);
		}
	}

	FORCEINLINE void DiscardTransientResource_RenderThread(FStructuredBufferRHIParamRef Buffer)
	{
		if (Buffer->IsCommitted())
		{
			if (GSupportsTransientResourceAliasing)
			{
				GDynamicRHI->RHIDiscardTransientResource_RenderThread(Buffer);
			}
			Buffer->SetCommitted(false);
		}
	}

	FORCEINLINE bool GetRenderQueryResult(FRenderQueryRHIParamRef RenderQuery, uint64& OutResult, bool bWait)
	{
		return RHIGetRenderQueryResult(RenderQuery, OutResult, bWait);
	}
	
	FORCEINLINE FTexture2DRHIRef GetViewportBackBuffer(FViewportRHIParamRef Viewport)
	{
		return RHIGetViewportBackBuffer(Viewport);
	}
	
	FORCEINLINE void AdvanceFrameForGetViewportBackBuffer(FViewportRHIParamRef Viewport)
	{
		return RHIAdvanceFrameForGetViewportBackBuffer(Viewport);
	}
	
	FORCEINLINE void AcquireThreadOwnership()
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_AcquireThreadOwnership_Flush);
		ImmediateFlush(EImmediateFlushType::FlushRHIThread); 
		 
		return GDynamicRHI->RHIAcquireThreadOwnership();
	}
	
	FORCEINLINE void ReleaseThreadOwnership()
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_ReleaseThreadOwnership_Flush);
		ImmediateFlush(EImmediateFlushType::FlushRHIThread); 
		 
		return GDynamicRHI->RHIReleaseThreadOwnership();
	}
	
	FORCEINLINE void FlushResources()
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_FlushResources_Flush);
		ImmediateFlush(EImmediateFlushType::FlushRHIThread); 
		 
		return GDynamicRHI->RHIFlushResources();
	}
	
	FORCEINLINE uint32 GetGPUFrameCycles()
	{
		return RHIGetGPUFrameCycles();
	}
	
	FORCEINLINE FViewportRHIRef CreateViewport(void* WindowHandle, uint32 SizeX, uint32 SizeY, bool bIsFullscreen, EPixelFormat PreferredPixelFormat)
	{
		LLM_SCOPE(ELLMTag::RenderTargets);
		return RHICreateViewport(WindowHandle, SizeX, SizeY, bIsFullscreen, PreferredPixelFormat);
	}
	
	FORCEINLINE void ResizeViewport(FViewportRHIParamRef Viewport, uint32 SizeX, uint32 SizeY, bool bIsFullscreen, EPixelFormat PreferredPixelFormat)
	{
		LLM_SCOPE(ELLMTag::RenderTargets);
		RHIResizeViewport(Viewport, SizeX, SizeY, bIsFullscreen, PreferredPixelFormat);
	}
	
	FORCEINLINE void Tick(float DeltaTime)
	{
		LLM_SCOPE(ELLMTag::RHIMisc);
		RHITick(DeltaTime);
	}
	
	FORCEINLINE void SetStreamOutTargets(uint32 NumTargets,const FVertexBufferRHIParamRef* VertexBuffers,const uint32* Offsets)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_SetStreamOutTargets_Flush);
		ImmediateFlush(EImmediateFlushType::FlushRHIThread);  
		GDynamicRHI->RHISetStreamOutTargets(NumTargets,VertexBuffers,Offsets);
	}
	
	FORCEINLINE void DiscardRenderTargets(bool Depth,bool Stencil,uint32 ColorBitMask)
	{
		GDynamicRHI->RHIDiscardRenderTargets(Depth,Stencil,ColorBitMask);
	}
	
	FORCEINLINE void BlockUntilGPUIdle()
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_BlockUntilGPUIdle_Flush);
		ImmediateFlush(EImmediateFlushType::FlushRHIThread);  
		GDynamicRHI->RHIBlockUntilGPUIdle();
	}

	FORCEINLINE_DEBUGGABLE void SubmitCommandsAndFlushGPU()
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_SubmitCommandsAndFlushGPU_Flush);
		ImmediateFlush(EImmediateFlushType::FlushRHIThread);
		GDynamicRHI->RHISubmitCommandsAndFlushGPU();
	}
	
	FORCEINLINE void SuspendRendering()
	{
		RHISuspendRendering();
	}
	
	FORCEINLINE void ResumeRendering()
	{
		RHIResumeRendering();
	}
	
	FORCEINLINE bool IsRenderingSuspended()
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_IsRenderingSuspended_Flush);
		ImmediateFlush(EImmediateFlushType::FlushRHIThread); 
		return GDynamicRHI->RHIIsRenderingSuspended();
	}

	FORCEINLINE bool EnqueueDecompress(uint8_t* SrcBuffer, uint8_t* DestBuffer, int CompressedSize, void* ErrorCodeBuffer)
	{
		return GDynamicRHI->RHIEnqueueDecompress(SrcBuffer, DestBuffer, CompressedSize, ErrorCodeBuffer);
	}

	FORCEINLINE bool EnqueueCompress(uint8_t* SrcBuffer, uint8_t* DestBuffer, int UnCompressedSize, void* ErrorCodeBuffer)
	{
		return GDynamicRHI->RHIEnqueueCompress(SrcBuffer, DestBuffer, UnCompressedSize, ErrorCodeBuffer);
	}
	
	FORCEINLINE bool GetAvailableResolutions(FScreenResolutionArray& Resolutions, bool bIgnoreRefreshRate)
	{
		return RHIGetAvailableResolutions(Resolutions, bIgnoreRefreshRate);
	}
	
	FORCEINLINE void GetSupportedResolution(uint32& Width, uint32& Height)
	{
		RHIGetSupportedResolution(Width, Height);
	}
	
	FORCEINLINE void VirtualTextureSetFirstMipInMemory(FTexture2DRHIParamRef Texture, uint32 FirstMip)
	{
		FScopedRHIThreadStaller StallRHIThread(*this);
		GDynamicRHI->RHIVirtualTextureSetFirstMipInMemory(Texture, FirstMip);
	}
	
	FORCEINLINE void VirtualTextureSetFirstMipVisible(FTexture2DRHIParamRef Texture, uint32 FirstMip)
	{
		FScopedRHIThreadStaller StallRHIThread(*this);
		GDynamicRHI->RHIVirtualTextureSetFirstMipVisible(Texture, FirstMip);
	}

	FORCEINLINE void CopySubTextureRegion(FTexture2DRHIParamRef SourceTexture, FTexture2DRHIParamRef DestinationTexture, FBox2D SourceBox, FBox2D DestinationBox)
	{
		GDynamicRHI->RHICopySubTextureRegion_RenderThread(*this, SourceTexture, DestinationTexture, SourceBox, DestinationBox);
	}
	
	FORCEINLINE void ExecuteCommandList(FRHICommandList* CmdList)
	{
		FScopedRHIThreadStaller StallRHIThread(*this);
		GDynamicRHI->RHIExecuteCommandList(CmdList);
	}
	
	FORCEINLINE void SetResourceAliasability(EResourceAliasability AliasMode, FTextureRHIParamRef* InTextures, int32 NumTextures)
	{
		GDynamicRHI->RHISetResourceAliasability_RenderThread(*this, AliasMode, InTextures, NumTextures);
	}
	
	FORCEINLINE void* GetNativeDevice()
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_GetNativeDevice_Flush);
		ImmediateFlush(EImmediateFlushType::FlushRHIThread); 
		 
		return GDynamicRHI->RHIGetNativeDevice();
	}
	
	FORCEINLINE class IRHICommandContext* GetDefaultContext()
	{
		return RHIGetDefaultContext();
	}
	
	FORCEINLINE class IRHICommandContextContainer* GetCommandContextContainer(int32 Index, int32 Num)
	{
		return RHIGetCommandContextContainer(Index, Num);
	}
	void UpdateTextureReference(FTextureReferenceRHIParamRef TextureRef, FTextureRHIParamRef NewTexture);
	
	FORCEINLINE FRHIShaderLibraryRef CreateShaderLibrary(EShaderPlatform Platform, FString FilePath)
	{
		FScopedRHIThreadStaller StallRHIThread(*this);
		return GDynamicRHI->RHICreateShaderLibrary(Platform, FilePath);
	}

};

// Single commandlist for async compute generation.  In the future we may expand this to allow async compute command generation
// on multiple threads at once.
class RHI_API FRHIAsyncComputeCommandListImmediate : public FRHIAsyncComputeCommandList
{
public:

	//If RHIThread is enabled this will dispatch all current commands to the RHI Thread.  If RHI thread is disabled
	//this will immediately execute the current commands.
	//This also queues a GPU Submission command as the final command in the dispatch.
	static void ImmediateDispatch(FRHIAsyncComputeCommandListImmediate& RHIComputeCmdList);

private:
};

// typedef to mark the recursive use of commandlists in the RHI implementations

class RHI_API FRHICommandList_RecursiveHazardous : public FRHICommandList
{
	FRHICommandList_RecursiveHazardous()
	{

	}
public:
	FRHICommandList_RecursiveHazardous(IRHICommandContext *Context)
	{
		SetContext(Context);
	}
};


// This controls if the cmd list bypass can be toggled at runtime. It is quite expensive to have these branches in there.
#define CAN_TOGGLE_COMMAND_LIST_BYPASS (!UE_BUILD_SHIPPING && !UE_BUILD_TEST)

class RHI_API FRHICommandListExecutor
{
public:
	enum
	{
		DefaultBypass = PLATFORM_RHITHREAD_DEFAULT_BYPASS
	};
	FRHICommandListExecutor()
		: bLatchedBypass(!!DefaultBypass)
		, bLatchedUseParallelAlgorithms(false)
	{
	}
	static inline FRHICommandListImmediate& GetImmediateCommandList();
	static inline FRHIAsyncComputeCommandListImmediate& GetImmediateAsyncComputeCommandList();

	void ExecuteList(FRHICommandListBase& CmdList);
	void ExecuteList(FRHICommandListImmediate& CmdList);
	void LatchBypass();

	static void WaitOnRHIThreadFence(FGraphEventRef& Fence);

	FORCEINLINE_DEBUGGABLE bool Bypass()
	{
#if CAN_TOGGLE_COMMAND_LIST_BYPASS
		return bLatchedBypass;
#else
		return !!DefaultBypass;
#endif
	}
	FORCEINLINE_DEBUGGABLE bool UseParallelAlgorithms()
	{
#if CAN_TOGGLE_COMMAND_LIST_BYPASS
		return bLatchedUseParallelAlgorithms;
#else
		return  FApp::ShouldUseThreadingForPerformance() && !Bypass();
#endif
	}
	static void CheckNoOutstandingCmdLists();
	static bool IsRHIThreadActive();
	static bool IsRHIThreadCompletelyFlushed();

private:

	void ExecuteInner(FRHICommandListBase& CmdList);
	friend class FExecuteRHIThreadTask;
	static void ExecuteInner_DoExecute(FRHICommandListBase& CmdList);

	bool bLatchedBypass;
	bool bLatchedUseParallelAlgorithms;
	friend class FRHICommandListBase;
	FThreadSafeCounter UIDCounter;
	FThreadSafeCounter OutstandingCmdListCount;
	FRHICommandListImmediate CommandListImmediate;
	FRHIAsyncComputeCommandListImmediate AsyncComputeCmdListImmediate;
};

extern RHI_API FRHICommandListExecutor GRHICommandList;

extern RHI_API FAutoConsoleTaskPriority CPrio_SceneRenderingTask;

class FRenderTask
{
public:
	FORCEINLINE static ENamedThreads::Type GetDesiredThread()
	{
		return CPrio_SceneRenderingTask.Get();
	}
};


FORCEINLINE_DEBUGGABLE FRHICommandListImmediate& FRHICommandListExecutor::GetImmediateCommandList()
{
	return GRHICommandList.CommandListImmediate;
}

FORCEINLINE_DEBUGGABLE FRHIAsyncComputeCommandListImmediate& FRHICommandListExecutor::GetImmediateAsyncComputeCommandList()
{
	return GRHICommandList.AsyncComputeCmdListImmediate;
}

struct FScopedCommandListWaitForTasks
{
	FRHICommandListImmediate& RHICmdList;
	bool bWaitForTasks;

	FScopedCommandListWaitForTasks(bool InbWaitForTasks, FRHICommandListImmediate& InRHICmdList = FRHICommandListExecutor::GetImmediateCommandList())
		: RHICmdList(InRHICmdList)
		, bWaitForTasks(InbWaitForTasks)
	{
	}
	RHI_API ~FScopedCommandListWaitForTasks();
};


FORCEINLINE FVertexDeclarationRHIRef RHICreateVertexDeclaration(const FVertexDeclarationElementList& Elements)
{
	return FRHICommandListExecutor::GetImmediateCommandList().CreateVertexDeclaration(Elements);
}

FORCEINLINE FPixelShaderRHIRef RHICreatePixelShader(const TArray<uint8>& Code)
{
	return FRHICommandListExecutor::GetImmediateCommandList().CreatePixelShader(Code);
}

FORCEINLINE FPixelShaderRHIRef RHICreatePixelShader(FRHIShaderLibraryParamRef Library, FSHAHash Hash)
{
	return FRHICommandListExecutor::GetImmediateCommandList().CreatePixelShader(Library, Hash);
}

FORCEINLINE FVertexShaderRHIRef RHICreateVertexShader(const TArray<uint8>& Code)
{
	return FRHICommandListExecutor::GetImmediateCommandList().CreateVertexShader(Code);
}

FORCEINLINE FVertexShaderRHIRef RHICreateVertexShader(FRHIShaderLibraryParamRef Library, FSHAHash Hash)
{
	return FRHICommandListExecutor::GetImmediateCommandList().CreateVertexShader(Library, Hash);
}

FORCEINLINE FHullShaderRHIRef RHICreateHullShader(const TArray<uint8>& Code)
{
	return FRHICommandListExecutor::GetImmediateCommandList().CreateHullShader(Code);
}

FORCEINLINE FHullShaderRHIRef RHICreateHullShader(FRHIShaderLibraryParamRef Library, FSHAHash Hash)
{
	return FRHICommandListExecutor::GetImmediateCommandList().CreateHullShader(Library, Hash);
}

FORCEINLINE FDomainShaderRHIRef RHICreateDomainShader(const TArray<uint8>& Code)
{
	return FRHICommandListExecutor::GetImmediateCommandList().CreateDomainShader(Code);
}

FORCEINLINE FDomainShaderRHIRef RHICreateDomainShader(FRHIShaderLibraryParamRef Library, FSHAHash Hash)
{
	return FRHICommandListExecutor::GetImmediateCommandList().CreateDomainShader(Library, Hash);
}

FORCEINLINE FGeometryShaderRHIRef RHICreateGeometryShader(const TArray<uint8>& Code)
{
	return FRHICommandListExecutor::GetImmediateCommandList().CreateGeometryShader(Code);
}

FORCEINLINE FGeometryShaderRHIRef RHICreateGeometryShader(FRHIShaderLibraryParamRef Library, FSHAHash Hash)
{
	return FRHICommandListExecutor::GetImmediateCommandList().CreateGeometryShader(Library, Hash);
}

FORCEINLINE FGeometryShaderRHIRef RHICreateGeometryShaderWithStreamOutput(const TArray<uint8>& Code, const FStreamOutElementList& ElementList, uint32 NumStrides, const uint32* Strides, int32 RasterizedStream)
{
	return FRHICommandListExecutor::GetImmediateCommandList().CreateGeometryShaderWithStreamOutput(Code, ElementList, NumStrides, Strides, RasterizedStream);
}

FORCEINLINE FGeometryShaderRHIRef RHICreateGeometryShaderWithStreamOutput(const FStreamOutElementList& ElementList, uint32 NumStrides, const uint32* Strides, int32 RasterizedStream, FRHIShaderLibraryParamRef Library, FSHAHash Hash)
{
	return FRHICommandListExecutor::GetImmediateCommandList().CreateGeometryShaderWithStreamOutput(ElementList, NumStrides, Strides, RasterizedStream, Library, Hash);
}

FORCEINLINE FComputeShaderRHIRef RHICreateComputeShader(const TArray<uint8>& Code)
{
	return FRHICommandListExecutor::GetImmediateCommandList().CreateComputeShader(Code);
}

FORCEINLINE FComputeShaderRHIRef RHICreateComputeShader(FRHIShaderLibraryParamRef Library, FSHAHash Hash)
{
	return FRHICommandListExecutor::GetImmediateCommandList().CreateComputeShader(Library, Hash);
}

FORCEINLINE FComputeFenceRHIRef RHICreateComputeFence(const FName& Name)
{
	return FRHICommandListExecutor::GetImmediateCommandList().CreateComputeFence(Name);
}

FORCEINLINE FIndexBufferRHIRef RHICreateAndLockIndexBuffer(uint32 Stride, uint32 Size, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo, void*& OutDataBuffer)
{
	return FRHICommandListExecutor::GetImmediateCommandList().CreateAndLockIndexBuffer(Stride, Size, InUsage, CreateInfo, OutDataBuffer);
}

FORCEINLINE FIndexBufferRHIRef RHICreateIndexBuffer(uint32 Stride, uint32 Size, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo)
{
	return FRHICommandListExecutor::GetImmediateCommandList().CreateIndexBuffer(Stride, Size, InUsage, CreateInfo);
}

FORCEINLINE void* RHILockIndexBuffer(FIndexBufferRHIParamRef IndexBuffer, uint32 Offset, uint32 Size, EResourceLockMode LockMode)
{
	return FRHICommandListExecutor::GetImmediateCommandList().LockIndexBuffer(IndexBuffer, Offset, Size, LockMode);
}

FORCEINLINE void RHIUnlockIndexBuffer(FIndexBufferRHIParamRef IndexBuffer)
{
	 FRHICommandListExecutor::GetImmediateCommandList().UnlockIndexBuffer(IndexBuffer);
}

FORCEINLINE FVertexBufferRHIRef RHICreateAndLockVertexBuffer(uint32 Size, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo, void*& OutDataBuffer)
{
	return FRHICommandListExecutor::GetImmediateCommandList().CreateAndLockVertexBuffer(Size, InUsage, CreateInfo, OutDataBuffer);
}

FORCEINLINE FVertexBufferRHIRef RHICreateVertexBuffer(uint32 Size, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo)
{
	return FRHICommandListExecutor::GetImmediateCommandList().CreateVertexBuffer(Size, InUsage, CreateInfo);
}

FORCEINLINE void* RHILockVertexBuffer(FVertexBufferRHIParamRef VertexBuffer, uint32 Offset, uint32 SizeRHI, EResourceLockMode LockMode)
{
	return FRHICommandListExecutor::GetImmediateCommandList().LockVertexBuffer(VertexBuffer, Offset, SizeRHI, LockMode);
}

FORCEINLINE void RHIUnlockVertexBuffer(FVertexBufferRHIParamRef VertexBuffer)
{
	 FRHICommandListExecutor::GetImmediateCommandList().UnlockVertexBuffer(VertexBuffer);
}

FORCEINLINE FStructuredBufferRHIRef RHICreateStructuredBuffer(uint32 Stride, uint32 Size, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo)
{
	return FRHICommandListExecutor::GetImmediateCommandList().CreateStructuredBuffer(Stride, Size, InUsage, CreateInfo);
}

FORCEINLINE void* RHILockStructuredBuffer(FStructuredBufferRHIParamRef StructuredBuffer, uint32 Offset, uint32 SizeRHI, EResourceLockMode LockMode)
{
	return FRHICommandListExecutor::GetImmediateCommandList().LockStructuredBuffer(StructuredBuffer, Offset, SizeRHI, LockMode);
}

FORCEINLINE void RHIUnlockStructuredBuffer(FStructuredBufferRHIParamRef StructuredBuffer)
{
	 FRHICommandListExecutor::GetImmediateCommandList().UnlockStructuredBuffer(StructuredBuffer);
}

FORCEINLINE FUnorderedAccessViewRHIRef RHICreateUnorderedAccessView(FStructuredBufferRHIParamRef StructuredBuffer, bool bUseUAVCounter, bool bAppendBuffer)
{
	return FRHICommandListExecutor::GetImmediateCommandList().CreateUnorderedAccessView(StructuredBuffer, bUseUAVCounter, bAppendBuffer);
}

FORCEINLINE FUnorderedAccessViewRHIRef RHICreateUnorderedAccessView(FTextureRHIParamRef Texture, uint32 MipLevel = 0)
{
	return FRHICommandListExecutor::GetImmediateCommandList().CreateUnorderedAccessView(Texture, MipLevel);
}

FORCEINLINE FUnorderedAccessViewRHIRef RHICreateUnorderedAccessView(FVertexBufferRHIParamRef VertexBuffer, uint8 Format)
{
	return FRHICommandListExecutor::GetImmediateCommandList().CreateUnorderedAccessView(VertexBuffer, Format);
}

FORCEINLINE FShaderResourceViewRHIRef RHICreateShaderResourceView(FStructuredBufferRHIParamRef StructuredBuffer)
{
	return FRHICommandListExecutor::GetImmediateCommandList().CreateShaderResourceView(StructuredBuffer);
}

FORCEINLINE FShaderResourceViewRHIRef RHICreateShaderResourceView(FVertexBufferRHIParamRef VertexBuffer, uint32 Stride, uint8 Format)
{
	return FRHICommandListExecutor::GetImmediateCommandList().CreateShaderResourceView(VertexBuffer, Stride, Format);
}

FORCEINLINE FShaderResourceViewRHIRef RHICreateShaderResourceView(FIndexBufferRHIParamRef Buffer)
{
	return FRHICommandListExecutor::GetImmediateCommandList().CreateShaderResourceView(Buffer);
}

FORCEINLINE FTextureReferenceRHIRef RHICreateTextureReference(FLastRenderTimeContainer* LastRenderTime)
{
	return FRHICommandListExecutor::GetImmediateCommandList().CreateTextureReference(LastRenderTime);
}

FORCEINLINE void RHIUpdateTextureReference(FTextureReferenceRHIParamRef TextureRef, FTextureRHIParamRef NewTexture)
{
	 FRHICommandListExecutor::GetImmediateCommandList().UpdateTextureReference(TextureRef, NewTexture);
}

FORCEINLINE FTexture2DRHIRef RHICreateTexture2D(uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, uint32 NumSamples, uint32 Flags, FRHIResourceCreateInfo& CreateInfo)
{
	return FRHICommandListExecutor::GetImmediateCommandList().CreateTexture2D(SizeX, SizeY, Format, NumMips, NumSamples, Flags, CreateInfo);
}

FORCEINLINE FStructuredBufferRHIRef RHICreateRTWriteMaskBuffer(FTexture2DRHIRef RenderTarget)
{
	return FRHICommandListExecutor::GetImmediateCommandList().CreateRTWriteMaskBuffer(RenderTarget);
}

FORCEINLINE FTexture2DRHIRef RHIAsyncCreateTexture2D(uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, uint32 Flags, void** InitialMipData, uint32 NumInitialMips)
{
	return FRHICommandListExecutor::GetImmediateCommandList().AsyncCreateTexture2D(SizeX, SizeY, Format, NumMips, Flags, InitialMipData, NumInitialMips);
}

FORCEINLINE void RHICopySharedMips(FTexture2DRHIParamRef DestTexture2D, FTexture2DRHIParamRef SrcTexture2D)
{
	return FRHICommandListExecutor::GetImmediateCommandList().CopySharedMips(DestTexture2D, SrcTexture2D);
}

FORCEINLINE FTexture2DArrayRHIRef RHICreateTexture2DArray(uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint8 Format, uint32 NumMips, uint32 Flags, FRHIResourceCreateInfo& CreateInfo)
{
	return FRHICommandListExecutor::GetImmediateCommandList().CreateTexture2DArray(SizeX, SizeY, SizeZ, Format, NumMips, Flags, CreateInfo);
}

FORCEINLINE FTexture3DRHIRef RHICreateTexture3D(uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint8 Format, uint32 NumMips, uint32 Flags, FRHIResourceCreateInfo& CreateInfo)
{
	return FRHICommandListExecutor::GetImmediateCommandList().CreateTexture3D(SizeX, SizeY, SizeZ, Format, NumMips, Flags, CreateInfo);
}

FORCEINLINE FShaderResourceViewRHIRef RHICreateShaderResourceView(FTexture2DRHIParamRef Texture2DRHI, uint8 MipLevel)
{
	return FRHICommandListExecutor::GetImmediateCommandList().CreateShaderResourceView(Texture2DRHI, MipLevel);
}

FORCEINLINE FShaderResourceViewRHIRef RHICreateShaderResourceView(FTexture2DRHIParamRef Texture2DRHI, uint8 MipLevel, uint8 NumMipLevels, uint8 Format)
{
	return FRHICommandListExecutor::GetImmediateCommandList().CreateShaderResourceView(Texture2DRHI, MipLevel, NumMipLevels, Format);
}

FORCEINLINE FShaderResourceViewRHIRef RHICreateShaderResourceView(FTexture3DRHIParamRef Texture3DRHI, uint8 MipLevel)
{
	return FRHICommandListExecutor::GetImmediateCommandList().CreateShaderResourceView(Texture3DRHI, MipLevel);
}

FORCEINLINE FShaderResourceViewRHIRef RHICreateShaderResourceView(FTexture2DArrayRHIParamRef Texture2DArrayRHI, uint8 MipLevel)
{
	return FRHICommandListExecutor::GetImmediateCommandList().CreateShaderResourceView(Texture2DArrayRHI, MipLevel);
}

FORCEINLINE FShaderResourceViewRHIRef RHICreateShaderResourceView(FTextureCubeRHIParamRef TextureCubeRHI, uint8 MipLevel)
{
	return FRHICommandListExecutor::GetImmediateCommandList().CreateShaderResourceView(TextureCubeRHI, MipLevel);
}

FORCEINLINE FTexture2DRHIRef RHIAsyncReallocateTexture2D(FTexture2DRHIParamRef Texture2D, int32 NewMipCount, int32 NewSizeX, int32 NewSizeY, FThreadSafeCounter* RequestStatus)
{
	return FRHICommandListExecutor::GetImmediateCommandList().AsyncReallocateTexture2D(Texture2D, NewMipCount, NewSizeX, NewSizeY, RequestStatus);
}

FORCEINLINE ETextureReallocationStatus RHIFinalizeAsyncReallocateTexture2D(FTexture2DRHIParamRef Texture2D, bool bBlockUntilCompleted)
{
	return FRHICommandListExecutor::GetImmediateCommandList().FinalizeAsyncReallocateTexture2D(Texture2D, bBlockUntilCompleted);
}

FORCEINLINE ETextureReallocationStatus RHICancelAsyncReallocateTexture2D(FTexture2DRHIParamRef Texture2D, bool bBlockUntilCompleted)
{
	return FRHICommandListExecutor::GetImmediateCommandList().CancelAsyncReallocateTexture2D(Texture2D, bBlockUntilCompleted);
}

FORCEINLINE void* RHILockTexture2D(FTexture2DRHIParamRef Texture, uint32 MipIndex, EResourceLockMode LockMode, uint32& DestStride, bool bLockWithinMiptail, bool bFlushRHIThread = true)
{
	return FRHICommandListExecutor::GetImmediateCommandList().LockTexture2D(Texture, MipIndex, LockMode, DestStride, bLockWithinMiptail, bFlushRHIThread);
}

FORCEINLINE void RHIUnlockTexture2D(FTexture2DRHIParamRef Texture, uint32 MipIndex, bool bLockWithinMiptail, bool bFlushRHIThread = true)
{
	 FRHICommandListExecutor::GetImmediateCommandList().UnlockTexture2D(Texture, MipIndex, bLockWithinMiptail, bFlushRHIThread);
}

FORCEINLINE void* RHILockTexture2DArray(FTexture2DArrayRHIParamRef Texture, uint32 TextureIndex, uint32 MipIndex, EResourceLockMode LockMode, uint32& DestStride, bool bLockWithinMiptail)
{
	return FRHICommandListExecutor::GetImmediateCommandList().LockTexture2DArray(Texture, TextureIndex, MipIndex, LockMode, DestStride, bLockWithinMiptail);
}

FORCEINLINE void RHIUnlockTexture2DArray(FTexture2DArrayRHIParamRef Texture, uint32 TextureIndex, uint32 MipIndex, bool bLockWithinMiptail)
{
	 FRHICommandListExecutor::GetImmediateCommandList().UnlockTexture2DArray(Texture, TextureIndex, MipIndex, bLockWithinMiptail);
}

FORCEINLINE void RHIUpdateTexture2D(FTexture2DRHIParamRef Texture, uint32 MipIndex, const struct FUpdateTextureRegion2D& UpdateRegion, uint32 SourcePitch, const uint8* SourceData)
{
	 FRHICommandListExecutor::GetImmediateCommandList().UpdateTexture2D(Texture, MipIndex, UpdateRegion, SourcePitch, SourceData);
}

FORCEINLINE FUpdateTexture3DData RHIBeginUpdateTexture3D(FTexture3DRHIParamRef Texture, uint32 MipIndex, const struct FUpdateTextureRegion3D& UpdateRegion)
{
	return FRHICommandListExecutor::GetImmediateCommandList().BeginUpdateTexture3D(Texture, MipIndex, UpdateRegion);
}

FORCEINLINE void RHIEndUpdateTexture3D(FUpdateTexture3DData& UpdateData)
{
	FRHICommandListExecutor::GetImmediateCommandList().EndUpdateTexture3D(UpdateData);
}

FORCEINLINE void RHIUpdateTexture3D(FTexture3DRHIParamRef Texture, uint32 MipIndex, const struct FUpdateTextureRegion3D& UpdateRegion, uint32 SourceRowPitch, uint32 SourceDepthPitch, const uint8* SourceData)
{
	 FRHICommandListExecutor::GetImmediateCommandList().UpdateTexture3D(Texture, MipIndex, UpdateRegion, SourceRowPitch, SourceDepthPitch, SourceData);
}

FORCEINLINE FTextureCubeRHIRef RHICreateTextureCube(uint32 Size, uint8 Format, uint32 NumMips, uint32 Flags, FRHIResourceCreateInfo& CreateInfo)
{
	return FRHICommandListExecutor::GetImmediateCommandList().CreateTextureCube(Size, Format, NumMips, Flags, CreateInfo);
}

FORCEINLINE FTextureCubeRHIRef RHICreateTextureCubeArray(uint32 Size, uint32 ArraySize, uint8 Format, uint32 NumMips, uint32 Flags, FRHIResourceCreateInfo& CreateInfo)
{
	return FRHICommandListExecutor::GetImmediateCommandList().CreateTextureCubeArray(Size, ArraySize, Format, NumMips, Flags, CreateInfo);
}

FORCEINLINE void* RHILockTextureCubeFace(FTextureCubeRHIParamRef Texture, uint32 FaceIndex, uint32 ArrayIndex, uint32 MipIndex, EResourceLockMode LockMode, uint32& DestStride, bool bLockWithinMiptail)
{
	return FRHICommandListExecutor::GetImmediateCommandList().LockTextureCubeFace(Texture, FaceIndex, ArrayIndex, MipIndex, LockMode, DestStride, bLockWithinMiptail);
}

FORCEINLINE void RHIUnlockTextureCubeFace(FTextureCubeRHIParamRef Texture, uint32 FaceIndex, uint32 ArrayIndex, uint32 MipIndex, bool bLockWithinMiptail)
{
	 FRHICommandListExecutor::GetImmediateCommandList().UnlockTextureCubeFace(Texture, FaceIndex, ArrayIndex, MipIndex, bLockWithinMiptail);
}

FORCEINLINE FRenderQueryRHIRef RHICreateRenderQuery(ERenderQueryType QueryType)
{
	return FRHICommandListExecutor::GetImmediateCommandList().CreateRenderQuery(QueryType);
}

FORCEINLINE void RHIAcquireTransientResource(FTextureRHIParamRef Resource)
{
	FRHICommandListExecutor::GetImmediateCommandList().AcquireTransientResource_RenderThread(Resource);
}

FORCEINLINE void RHIDiscardTransientResource(FTextureRHIParamRef Resource)
{
	FRHICommandListExecutor::GetImmediateCommandList().DiscardTransientResource_RenderThread(Resource);
}

FORCEINLINE void RHIAcquireTransientResource(FVertexBufferRHIParamRef Resource)
{
	FRHICommandListExecutor::GetImmediateCommandList().AcquireTransientResource_RenderThread(Resource);
}

FORCEINLINE void RHIDiscardTransientResource(FVertexBufferRHIParamRef Resource)
{
	FRHICommandListExecutor::GetImmediateCommandList().DiscardTransientResource_RenderThread(Resource);
}

FORCEINLINE void RHIAcquireTransientResource(FStructuredBufferRHIParamRef Resource)
{
	FRHICommandListExecutor::GetImmediateCommandList().AcquireTransientResource_RenderThread(Resource);
}

FORCEINLINE void RHIDiscardTransientResource(FStructuredBufferRHIParamRef Resource)
{
	FRHICommandListExecutor::GetImmediateCommandList().DiscardTransientResource_RenderThread(Resource);
}

FORCEINLINE void RHIAcquireThreadOwnership()
{
	return FRHICommandListExecutor::GetImmediateCommandList().AcquireThreadOwnership();
}

FORCEINLINE void RHIReleaseThreadOwnership()
{
	return FRHICommandListExecutor::GetImmediateCommandList().ReleaseThreadOwnership();
}

FORCEINLINE void RHIFlushResources()
{
	return FRHICommandListExecutor::GetImmediateCommandList().FlushResources();
}

FORCEINLINE void RHIVirtualTextureSetFirstMipInMemory(FTexture2DRHIParamRef Texture, uint32 FirstMip)
{
	 FRHICommandListExecutor::GetImmediateCommandList().VirtualTextureSetFirstMipInMemory(Texture, FirstMip);
}

FORCEINLINE void RHIVirtualTextureSetFirstMipVisible(FTexture2DRHIParamRef Texture, uint32 FirstMip)
{
	 FRHICommandListExecutor::GetImmediateCommandList().VirtualTextureSetFirstMipVisible(Texture, FirstMip);
}

FORCEINLINE void RHIExecuteCommandList(FRHICommandList* CmdList)
{
	 FRHICommandListExecutor::GetImmediateCommandList().ExecuteCommandList(CmdList);
}

FORCEINLINE void* RHIGetNativeDevice()
{
	return FRHICommandListExecutor::GetImmediateCommandList().GetNativeDevice();
}

FORCEINLINE void RHIRecreateRecursiveBoundShaderStates()
{
	FRHICommandListExecutor::GetImmediateCommandList(). ImmediateFlush(EImmediateFlushType::FlushRHIThread);
	GDynamicRHI->RHIRecreateRecursiveBoundShaderStates();
}

FORCEINLINE FRHIShaderLibraryRef RHICreateShaderLibrary(EShaderPlatform Platform, FString FilePath)
{
	return FRHICommandListExecutor::GetImmediateCommandList().CreateShaderLibrary(Platform, FilePath);
}


#include "RHICommandList.inl"
