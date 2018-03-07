// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
D3D12CommandContext.cpp: RHI  Command Context implementation.
=============================================================================*/

#include "D3D12RHIPrivate.h"

#if PLATFORM_XBOXONE
// @TODO: We fixed this on PC. Need to check it works on XB before re-enabling. 
// Aggressive batching saves ~0.1ms on the RHI thread, reduces executecommandlist calls by around 25%
int32 GCommandListBatchingMode = CLB_NormalBatching;
#else
int32 GCommandListBatchingMode = CLB_AggressiveBatching;
#endif 

static FAutoConsoleVariableRef CVarCommandListBatchingMode(
	TEXT("D3D12.CommandListBatchingMode"),
	GCommandListBatchingMode,
	TEXT("Changes how command lists are batched and submitted to the GPU."),
	ECVF_RenderThreadSafe
	);

FD3D12CommandContext::FD3D12CommandContext(FD3D12Device* InParent, FD3D12SubAllocatedOnlineHeap::SubAllocationDesc& SubHeapDesc, bool InIsDefaultContext, bool InIsAsyncComputeContext) :
	OwningRHI(*InParent->GetOwningRHI()),
	bUsingTessellation(false),
	PendingNumVertices(0),
	PendingVertexDataStride(0),
	PendingPrimitiveType(0),
	PendingNumPrimitives(0),
	PendingMinVertexIndex(0),
	PendingNumIndices(0),
	PendingIndexDataStride(0),
	CurrentDepthTexture(nullptr),
	NumSimultaneousRenderTargets(0),
	NumUAVs(0),
	CurrentDSVAccessType(FExclusiveDepthStencil::DepthWrite_StencilWrite),
	bDiscardSharedConstants(false),
	bIsDefaultContext(InIsDefaultContext),
	bIsAsyncComputeContext(InIsAsyncComputeContext),
#if PLATFORM_SUPPORTS_VIRTUAL_TEXTURES
	bNeedFlushTextureCache(false),
#endif
	CommandListHandle(),
	CommandAllocator(nullptr),
	CommandAllocatorManager(InParent, InIsAsyncComputeContext ? D3D12_COMMAND_LIST_TYPE_COMPUTE : D3D12_COMMAND_LIST_TYPE_DIRECT),
	ConstantsAllocator(InParent, GetVisibilityMask(), (1024*1024) * 3),
	DynamicVB(InParent),
	DynamicIB(InParent),
	StateCache(InParent->GetNodeMask()),
	VSConstantBuffer(InParent, ConstantsAllocator),
	HSConstantBuffer(InParent, ConstantsAllocator),
	DSConstantBuffer(InParent, ConstantsAllocator),
	PSConstantBuffer(InParent, ConstantsAllocator),
	GSConstantBuffer(InParent, ConstantsAllocator),
	CSConstantBuffer(InParent, ConstantsAllocator),
	bIsMGPUAware(InParent->GetParentAdapter()->GetNumGPUNodes() > 1),
	CurrentDepthStencilTarget(nullptr),
	FD3D12DeviceChild(InParent),
	FD3D12SingleNodeGPUObject(InParent->GetNodeMask())
{
	FMemory::Memzero(DirtyUniformBuffers);
	FMemory::Memzero(BoundUniformBuffers);
	for (int i = 0; i < ARRAY_COUNT(BoundUniformBufferRefs); i++)
	{
		for (int j = 0; j < ARRAY_COUNT(BoundUniformBufferRefs[i]); j++)
		{
			BoundUniformBufferRefs[i][j] = NULL;
		}
	}
	FMemory::Memzero(CurrentRenderTargets);
	FMemory::Memzero(CurrentUAVs);
	StateCache.Init(GetParentDevice(), this, nullptr, SubHeapDesc);

	ConstantsAllocator.Init();
}

FD3D12CommandContext::~FD3D12CommandContext()
{
	ClearState();
}

void FD3D12CommandContext::RHIPushEvent(const TCHAR* Name, FColor Color)
{
	if (IsDefaultContext())
	{
		GetParentDevice()->PushGPUEvent(Name, Color);
	}

#if USE_PIX
	PIXBeginEvent(CommandListHandle.GraphicsCommandList(), PIX_COLOR(Color.R, Color.G, Color.B), Name);
#endif
}

void FD3D12CommandContext::RHIPopEvent()
{
	if (IsDefaultContext())
	{
		GetParentDevice()->PopGPUEvent();
	}

#if USE_PIX
	PIXEndEvent(CommandListHandle.GraphicsCommandList());
#endif
}

void FD3D12CommandContext::RHIAutomaticCacheFlushAfterComputeShader(bool bEnable)
{
	StateCache.AutoFlushComputeShaderCache(bEnable);
}

void FD3D12CommandContext::RHIFlushComputeShaderCache()
{
	StateCache.FlushComputeShaderCache(true);
}

FD3D12CommandListManager& FD3D12CommandContext::GetCommandListManager()
{
	return bIsAsyncComputeContext ? GetParentDevice()->GetAsyncCommandListManager() : GetParentDevice()->GetCommandListManager();
}

void FD3D12CommandContext::ConditionalObtainCommandAllocator()
{
	if (CommandAllocator == nullptr)
	{
		// Obtain a command allocator if the context doesn't already have one.
		// This will check necessary fence values to ensure the returned command allocator isn't being used by the GPU, then reset it.
		CommandAllocator = CommandAllocatorManager.ObtainCommandAllocator();
	}
}

void FD3D12CommandContext::ReleaseCommandAllocator()
{
	if (CommandAllocator != nullptr)
	{
		// Release the command allocator so it can be reused.
		CommandAllocatorManager.ReleaseCommandAllocator(CommandAllocator);
		CommandAllocator = nullptr;
	}
}

void FD3D12CommandContext::OpenCommandList()
{
	// Conditionally get a new command allocator.
	// Each command context uses a new allocator for all command lists within a "frame".
	ConditionalObtainCommandAllocator();

	// Get a new command list
	CommandListHandle = GetCommandListManager().ObtainCommandList(*CommandAllocator);
	CommandListHandle.SetCurrentOwningContext(this);

	// Notify the descriptor cache about the new command list
	// This will set the descriptor cache's current heaps on the new command list.
	StateCache.GetDescriptorCache()->NotifyCurrentCommandList(CommandListHandle);

	// Mark state as dirty so next time ApplyState is called, it will set all state on this new command list
	StateCache.DirtyState();

	numDraws = 0;
	numDispatches = 0;
	numClears = 0;
	numBarriers = 0;
	numCopies = 0;
	otherWorkCounter = 0;
}

void FD3D12CommandContext::CloseCommandList()
{
	CommandListHandle.Close();
}

FD3D12CommandListHandle FD3D12CommandContext::FlushCommands(bool WaitForCompletion)
{
	// We should only be flushing the default context
	check(IsDefaultContext());

	FD3D12Device* Device = GetParentDevice();
	const bool bHasPendingWork = Device->PendingCommandLists.Num() > 0;
	const bool bHasDoneWork = HasDoneWork() || bHasPendingWork;

	// Only submit a command list if it does meaningful work or the flush is expected to wait for completion.
	if (WaitForCompletion || bHasDoneWork)
	{
		// Close the current command list
		CloseCommandList();

		if (bHasPendingWork)
		{
			// Submit all pending command lists and the current command list
			Device->PendingCommandLists.Add(CommandListHandle);
			GetCommandListManager().ExecuteCommandLists(Device->PendingCommandLists, WaitForCompletion);
			Device->PendingCommandLists.Reset();
			Device->PendingCommandListsTotalWorkCommands = 0;
		}
		else
		{
			// Just submit the current command list
			CommandListHandle.Execute(WaitForCompletion);
		}

		// Get a new command list to replace the one we submitted for execution. 
		// Restore the state from the previous command list.
		OpenCommandList();
	}

	return CommandListHandle;
}

void FD3D12CommandContext::Finish(TArray<FD3D12CommandListHandle>& CommandLists)
{
	CloseCommandList();

	if (HasDoneWork())
	{
		CommandLists.Add(CommandListHandle);
	}
	else
	{
		// Release the unused command list.
		GetCommandListManager().ReleaseCommandList(CommandListHandle);
	}

	// The context is done with this command list handle.
	CommandListHandle = FD3D12CommandListHandle();
}

void FD3D12CommandContext::RHIBeginFrame()
{
	FD3D12Device* Device = GetParentDevice();
	FD3D12Adapter* Adapter = Device->GetParentAdapter();

	check(IsDefaultContext());
	check(Adapter->GetCurrentNodeMask() == Device->GetNodeMask());
	RHIPrivateBeginFrame();

	FD3D12GlobalOnlineHeap& SamplerHeap = Device->GetGlobalSamplerHeap();

	if (SamplerHeap.DescriptorTablesDirty())
	{
		//Rearrange the set for better look-up performance
		SamplerHeap.GetUniqueDescriptorTables().Compact();
	}

	const uint32 NumContexts = Device->GetNumContexts();
	for (uint32 i = 0; i < NumContexts; ++i)
	{
		Device->GetCommandContext(i).StateCache.GetDescriptorCache()->BeginFrame();
	}

	const uint32 NumAsyncContexts = Device->GetNumAsyncComputeContexts();
	for (uint32 i = 0; i < NumAsyncContexts; ++i)
	{
		Device->GetAsyncComputeContext(i).StateCache.GetDescriptorCache()->BeginFrame();
	}

	Device->GetGlobalSamplerHeap().ToggleDescriptorTablesDirtyFlag(false);

	Adapter->GetGPUProfiler().BeginFrame(Device->GetOwningRHI());
}

void FD3D12CommandContext::ClearState()
{
	StateCache.ClearState();

	bDiscardSharedConstants = false;

	FMemory::Memzero(BoundUniformBuffers, sizeof(BoundUniformBuffers));
	FMemory::Memzero(DirtyUniformBuffers, sizeof(DirtyUniformBuffers));

	for (int i = 0; i < ARRAY_COUNT(BoundUniformBufferRefs); i++)
	{
		for (int j = 0; j < ARRAY_COUNT(BoundUniformBufferRefs[i]); j++)
		{
			BoundUniformBufferRefs[i][j] = NULL;
		}
	}

	FMemory::Memzero(CurrentUAVs, sizeof(CurrentUAVs));
	NumUAVs = 0;

	if (!bIsAsyncComputeContext)
	{
		FMemory::Memzero(CurrentRenderTargets, sizeof(CurrentRenderTargets));
		NumSimultaneousRenderTargets = 0;

		CurrentDepthStencilTarget = nullptr;
		CurrentDepthTexture = nullptr;

		CurrentDSVAccessType = FExclusiveDepthStencil::DepthWrite_StencilWrite;

		bUsingTessellation = false;

		CurrentBoundShaderState = nullptr;
	}
}

void FD3D12CommandContext::ConditionalClearShaderResource(FD3D12ResourceLocation* Resource)
{
	check(Resource);
	StateCache.ClearShaderResourceViews<SF_Vertex>(Resource);
	StateCache.ClearShaderResourceViews<SF_Hull>(Resource);
	StateCache.ClearShaderResourceViews<SF_Domain>(Resource);
	StateCache.ClearShaderResourceViews<SF_Pixel>(Resource);
	StateCache.ClearShaderResourceViews<SF_Geometry>(Resource);
	StateCache.ClearShaderResourceViews<SF_Compute>(Resource);
}

void FD3D12CommandContext::ClearAllShaderResources()
{
	StateCache.ClearSRVs();
}

void FD3D12CommandContext::RHIEndFrame()
{
	FD3D12Device* Device = GetParentDevice();
	FD3D12Adapter* Adapter = Device->GetParentAdapter();
	{
		check(IsDefaultContext());
		check(Adapter->GetCurrentNodeMask() == Device->GetNodeMask());

		Adapter->EndFrame();

		const uint32 NumContexts = Device->GetNumContexts();
		for (uint32 i = 0; i < NumContexts; ++i)
		{
			Device->GetCommandContext(i).EndFrame();
		}

		const uint32 NumAsyncContexts = Device->GetNumAsyncComputeContexts();
		for (uint32 i = 0; i < NumAsyncContexts; ++i)
		{
			Device->GetAsyncComputeContext(i).EndFrame();
		}

		Device->GetTextureAllocator().CleanUpAllocations();
		Device->GetDefaultBufferAllocator().CleanupFreeBlocks();

		Device->GetDefaultFastAllocator().CleanupPages<FD3D12ScopeLock>(10);

		// The Texture streaming threads
		{
			for (int32 i = 0; i < FD3D12DynamicRHI::GetD3DRHI()->NumThreadDynamicHeapAllocators; ++i)
			{
				FD3D12FastAllocator* TextureStreamingAllocator = FD3D12DynamicRHI::GetD3DRHI()->ThreadDynamicHeapAllocatorArray[i];
				if (TextureStreamingAllocator)
				{
					TextureStreamingAllocator->CleanupPages<FD3D12ScopeLock>(10);
				}
			}
		}

		GetCommandListManager().ReleaseResourceBarrierCommandListAllocator();

		UpdateMemoryStats();
	}

#if PLATFORM_SUPPORTS_MGPU
	if (Adapter->AlternateFrameRenderingEnabled())
	{
		// When doing AFR rendering we need to switch to the next GPU
		Adapter->SwitchToNextGPU();

		// Update the default context redirector so that the next frame will work on the correct context
		Adapter->GetDefaultContextRedirector().SetCurrentDeviceIndex(Adapter->GetCurrentDevice()->GetNodeIndex());
		Adapter->GetDefaultAsyncComputeContextRedirector().SetCurrentDeviceIndex(Adapter->GetCurrentDevice()->GetNodeIndex());
	}
#endif
}

void FD3D12CommandContext::UpdateMemoryStats()
{
#if PLATFORM_WINDOWS && STATS
	DXGI_QUERY_VIDEO_MEMORY_INFO LocalVideoMemoryInfo;
	GetParentDevice()->GetLocalVideoMemoryInfo(&LocalVideoMemoryInfo);

	const int64 Budget = LocalVideoMemoryInfo.Budget;
	const int64 AvailableSpace = Budget - int64(LocalVideoMemoryInfo.CurrentUsage);
	SET_MEMORY_STAT(STAT_D3D12UsedVideoMemory, LocalVideoMemoryInfo.CurrentUsage);
	SET_MEMORY_STAT(STAT_D3D12AvailableVideoMemory, AvailableSpace);
	SET_MEMORY_STAT(STAT_D3D12TotalVideoMemory, Budget);
#endif
}

void FD3D12CommandContext::RHIBeginScene()
{
}

void FD3D12CommandContext::RHIEndScene()
{
}

#if D3D12_SUPPORTS_PARALLEL_RHI_EXECUTE

//todo recycle these to avoid alloc

class FD3D12CommandContextContainer : public IRHICommandContextContainer
{
	FD3D12Adapter* Adapter;
	FD3D12CommandContext* CmdContext;
	TArray<FD3D12CommandListHandle> CommandLists;
	const int32 FrameIndex;

public:

	/** Custom new/delete with recycling */
	void* operator new(size_t Size);
	void operator delete(void* RawMemory);

	FD3D12CommandContextContainer(FD3D12Adapter* InAdapter, int32 Index)
		: Adapter(InAdapter), CmdContext(nullptr), FrameIndex(Index)
	{
		CommandLists.Reserve(16);
	}

	virtual ~FD3D12CommandContextContainer() override
	{
	}

	virtual IRHICommandContext* GetContext() override
	{
		FD3D12Device* Device = Adapter->GetDeviceByIndex(FrameIndex);

		check(CmdContext == nullptr);
		CmdContext = Device->ObtainCommandContext();
		check(CmdContext != nullptr);
		check(CmdContext->CommandListHandle == nullptr);

		CmdContext->OpenCommandList();
		CmdContext->ClearState();

		return CmdContext;
	}

	virtual void FinishContext() override
	{
		FD3D12Device* Device = Adapter->GetDeviceByIndex(FrameIndex);

		// We never "Finish" the default context. It gets submitted when FlushCommands() is called.
		check(!CmdContext->IsDefaultContext());

		CmdContext->Finish(CommandLists);

		Device->ReleaseCommandContext(CmdContext);
		CmdContext = nullptr;
	}

	virtual void SubmitAndFreeContextContainer(int32 Index, int32 Num) override
	{
		FD3D12Device* Device = Adapter->GetDeviceByIndex(FrameIndex);

		if (Index == 0)
		{
			check((IsInRenderingThread() || IsInRHIThread()));

			FD3D12CommandContext& DefaultContext = Device->GetDefaultCommandContext();

			// Don't really submit the default context yet, just start a new command list.
			// Close the command list, add it to the pending command lists, then open a new command list (with the previous state restored).
			DefaultContext.CloseCommandList();

			Device->PendingCommandLists.Add(DefaultContext.CommandListHandle);
			Device->PendingCommandListsTotalWorkCommands +=
				DefaultContext.numClears +
				DefaultContext.numCopies +
				DefaultContext.numDraws;

			DefaultContext.OpenCommandList();
		}

		// Add the current lists for execution (now or possibly later depending on the command list batching mode).
		for (int32 i = 0; i < CommandLists.Num(); ++i)
		{
			Device->PendingCommandLists.Add(CommandLists[i]);
			Device->PendingCommandListsTotalWorkCommands +=
				CommandLists[i].GetCurrentOwningContext()->numClears +
				CommandLists[i].GetCurrentOwningContext()->numCopies +
				CommandLists[i].GetCurrentOwningContext()->numDraws;
		}

		CommandLists.Reset();

		bool Flush = false;
		// If the GPU is starving (i.e. we are CPU bound) feed it asap!
		if (Device->IsGPUIdle() && Device->PendingCommandLists.Num() > 0)
		{
			Flush = true;
		}
		else
		{
			if (GCommandListBatchingMode != CLB_AggressiveBatching)
			{
				// Submit when the batch is finished.
				const bool FinalCommandListInBatch = Index == (Num - 1);
				if (FinalCommandListInBatch && Device->PendingCommandLists.Num() > 0)
				{
					Flush = true;
				}
			}
		}

		if (Flush)
		{
			Device->GetCommandListManager().ExecuteCommandLists(Device->PendingCommandLists);
			Device->PendingCommandLists.Reset();
			Device->PendingCommandListsTotalWorkCommands = 0;
		}

		delete this;
	}
};

void* FD3D12CommandContextContainer::operator new(size_t Size)
{
	return FMemory::Malloc(Size);
}

void FD3D12CommandContextContainer::operator delete(void* RawMemory)
{
	FMemory::Free(RawMemory);
}

IRHICommandContextContainer* FD3D12DynamicRHI::RHIGetCommandContextContainer(int32 Index, int32 Num)
{
	return new FD3D12CommandContextContainer(&GetAdapter(), GFrameNumberRenderThread % GetAdapter().GetNumGPUNodes());
}

#endif // D3D12_SUPPORTS_PARALLEL_RHI_EXECUTE


//////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// FD3D12CommandContextRedirector
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////

FD3D12CommandContextRedirector::FD3D12CommandContextRedirector(class FD3D12Adapter* InParent)
	: CurrentDeviceIndex(0)
	, FD3D12AdapterChild(InParent)
{
	FMemory::Memzero(PhysicalContexts, sizeof(PhysicalContexts[0]) * MAX_NUM_LDA_NODES);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// FD3D12TemporalEffect
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////

FD3D12TemporalEffect::FD3D12TemporalEffect()
	: EffectFence(nullptr, "TemporalEffectFence")
	, FD3D12AdapterChild(nullptr)
{}

FD3D12TemporalEffect::FD3D12TemporalEffect(FD3D12Adapter* Parent, const FName& InEffectName)
	: EffectFence(Parent, InEffectName.GetPlainANSIString())
	, FD3D12AdapterChild(Parent)
{}

FD3D12TemporalEffect::FD3D12TemporalEffect(const FD3D12TemporalEffect& Other)
{
	FMemory::Memcpy(EffectFence, Other.EffectFence);
}

void FD3D12TemporalEffect::Init()
{
	EffectFence.CreateFence();
}

void FD3D12TemporalEffect::Destroy()
{
	EffectFence.Destroy();
}

void FD3D12TemporalEffect::WaitForPrevious(ID3D12CommandQueue* Queue)
{
	const uint64 CurrentFence = EffectFence.GetCurrentFence();
	if (CurrentFence > 1)
	{
		EffectFence.GpuWait(Queue, CurrentFence - 1);
	}
}

void FD3D12TemporalEffect::SignalSyncComplete(ID3D12CommandQueue* Queue)
{
	EffectFence.Signal(Queue);
}

