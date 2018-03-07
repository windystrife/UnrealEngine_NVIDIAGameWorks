// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanContext.h: Class to generate Vulkan command buffers from RHI CommandLists
=============================================================================*/

#pragma once 

#include "VulkanResources.h"

class FVulkanDevice;
class FVulkanCommandBufferManager;
class FVulkanPendingGfxState;
class FVulkanPendingComputeState;
class FVulkanQueue;

class FVulkanCommandListContext : public IRHICommandContext
{
public:
	FVulkanCommandListContext(FVulkanDynamicRHI* InRHI, FVulkanDevice* InDevice, FVulkanQueue* InQueue, bool bInIsImmediate);
	virtual ~FVulkanCommandListContext();

	inline bool IsImmediate() const
	{
		return bIsImmediate;
	}

	virtual void RHISetStreamSource(uint32 StreamIndex, FVertexBufferRHIParamRef VertexBuffer, uint32 Stride, uint32 Offset) final override;
	virtual void RHISetStreamSource(uint32 StreamIndex, FVertexBufferRHIParamRef VertexBuffer, uint32 Offset) final override;
	virtual void RHISetRasterizerState(FRasterizerStateRHIParamRef NewState) final override;
	virtual void RHISetViewport(uint32 MinX, uint32 MinY, float MinZ, uint32 MaxX, uint32 MaxY, float MaxZ) final override;
	virtual void RHISetScissorRect(bool bEnable, uint32 MinX, uint32 MinY, uint32 MaxX, uint32 MaxY) final override;
	virtual void RHISetBoundShaderState(FBoundShaderStateRHIParamRef BoundShaderState) final override;
	virtual void RHISetGraphicsPipelineState(FGraphicsPipelineStateRHIParamRef GraphicsState) final override;
	virtual void RHISetShaderTexture(FVertexShaderRHIParamRef VertexShader, uint32 TextureIndex, FTextureRHIParamRef NewTexture) final override;
	virtual void RHISetShaderTexture(FHullShaderRHIParamRef HullShader, uint32 TextureIndex, FTextureRHIParamRef NewTexture) final override;
	virtual void RHISetShaderTexture(FDomainShaderRHIParamRef DomainShader, uint32 TextureIndex, FTextureRHIParamRef NewTexture) final override;
	virtual void RHISetShaderTexture(FGeometryShaderRHIParamRef GeometryShader, uint32 TextureIndex, FTextureRHIParamRef NewTexture) final override;
	virtual void RHISetShaderTexture(FPixelShaderRHIParamRef PixelShader, uint32 TextureIndex, FTextureRHIParamRef NewTexture) final override;
	virtual void RHISetShaderTexture(FComputeShaderRHIParamRef PixelShader, uint32 TextureIndex, FTextureRHIParamRef NewTexture) final override;
	virtual void RHISetShaderSampler(FComputeShaderRHIParamRef ComputeShader, uint32 SamplerIndex, FSamplerStateRHIParamRef NewState) final override;
	virtual void RHISetShaderSampler(FVertexShaderRHIParamRef VertexShader, uint32 SamplerIndex, FSamplerStateRHIParamRef NewState) final override;
	virtual void RHISetShaderSampler(FGeometryShaderRHIParamRef GeometryShader, uint32 SamplerIndex, FSamplerStateRHIParamRef NewState) final override;
	virtual void RHISetShaderSampler(FDomainShaderRHIParamRef DomainShader, uint32 SamplerIndex, FSamplerStateRHIParamRef NewState) final override;
	virtual void RHISetShaderSampler(FHullShaderRHIParamRef HullShader, uint32 SamplerIndex, FSamplerStateRHIParamRef NewState) final override;
	virtual void RHISetShaderSampler(FPixelShaderRHIParamRef PixelShader, uint32 SamplerIndex, FSamplerStateRHIParamRef NewState) final override;
	virtual void RHISetUAVParameter(FComputeShaderRHIParamRef ComputeShader, uint32 UAVIndex, FUnorderedAccessViewRHIParamRef UAV) final override;
	virtual void RHISetUAVParameter(FComputeShaderRHIParamRef ComputeShader, uint32 UAVIndex, FUnorderedAccessViewRHIParamRef UAV, uint32 InitialCount) final override;
	virtual void RHISetShaderResourceViewParameter(FPixelShaderRHIParamRef PixelShader, uint32 SamplerIndex, FShaderResourceViewRHIParamRef SRV) final override;
	virtual void RHISetShaderResourceViewParameter(FVertexShaderRHIParamRef VertexShader, uint32 SamplerIndex, FShaderResourceViewRHIParamRef SRV) final override;
	virtual void RHISetShaderResourceViewParameter(FComputeShaderRHIParamRef ComputeShader, uint32 SamplerIndex, FShaderResourceViewRHIParamRef SRV) final override;
	virtual void RHISetShaderResourceViewParameter(FHullShaderRHIParamRef HullShader, uint32 SamplerIndex, FShaderResourceViewRHIParamRef SRV) final override;
	virtual void RHISetShaderResourceViewParameter(FDomainShaderRHIParamRef DomainShader, uint32 SamplerIndex, FShaderResourceViewRHIParamRef SRV) final override;
	virtual void RHISetShaderResourceViewParameter(FGeometryShaderRHIParamRef GeometryShader, uint32 SamplerIndex, FShaderResourceViewRHIParamRef SRV) final override;
	virtual void RHISetShaderUniformBuffer(FVertexShaderRHIParamRef VertexShader, uint32 BufferIndex, FUniformBufferRHIParamRef Buffer) final override;
	virtual void RHISetShaderUniformBuffer(FHullShaderRHIParamRef HullShader, uint32 BufferIndex, FUniformBufferRHIParamRef Buffer) final override;
	virtual void RHISetShaderUniformBuffer(FDomainShaderRHIParamRef DomainShader, uint32 BufferIndex, FUniformBufferRHIParamRef Buffer) final override;
	virtual void RHISetShaderUniformBuffer(FGeometryShaderRHIParamRef GeometryShader, uint32 BufferIndex, FUniformBufferRHIParamRef Buffer) final override;
	virtual void RHISetShaderUniformBuffer(FPixelShaderRHIParamRef PixelShader, uint32 BufferIndex, FUniformBufferRHIParamRef Buffer) final override;
	virtual void RHISetShaderUniformBuffer(FComputeShaderRHIParamRef ComputeShader, uint32 BufferIndex, FUniformBufferRHIParamRef Buffer) final override;
	virtual void RHISetShaderParameter(FVertexShaderRHIParamRef VertexShader, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue) final override;
	virtual void RHISetShaderParameter(FHullShaderRHIParamRef HullShader, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue) final override;
	virtual void RHISetShaderParameter(FDomainShaderRHIParamRef DomainShader, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue) final override;
	virtual void RHISetShaderParameter(FGeometryShaderRHIParamRef GeometryShader, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue) final override;
	virtual void RHISetShaderParameter(FPixelShaderRHIParamRef PixelShader, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue) final override;
	virtual void RHISetShaderParameter(FComputeShaderRHIParamRef ComputeShader, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue) final override;
	virtual void RHISetDepthStencilState(FDepthStencilStateRHIParamRef NewState, uint32 StencilRef) final override;
	virtual void RHISetStencilRef(uint32 StencilRef) final override;
	virtual void RHISetBlendState(FBlendStateRHIParamRef NewState, const FLinearColor& BlendFactor) final override;
	virtual void RHISetRenderTargets(uint32 NumSimultaneousRenderTargets, const FRHIRenderTargetView* NewRenderTargets, const FRHIDepthRenderTargetView* NewDepthStencilTarget, uint32 NumUAVs, const FUnorderedAccessViewRHIParamRef* UAVs) final override;
	virtual void RHISetRenderTargetsAndClear(const FRHISetRenderTargetsInfo& RenderTargetsInfo) final override;
	virtual void RHIDrawPrimitive(uint32 PrimitiveType, uint32 BaseVertexIndex, uint32 NumPrimitives, uint32 NumInstances) final override;
	virtual void RHIDrawPrimitiveIndirect(uint32 PrimitiveType, FVertexBufferRHIParamRef ArgumentBuffer, uint32 ArgumentOffset) final override;
	virtual void RHIDrawIndexedIndirect(FIndexBufferRHIParamRef IndexBufferRHI, uint32 PrimitiveType, FStructuredBufferRHIParamRef ArgumentsBufferRHI, int32 DrawArgumentsIndex, uint32 NumInstances) final override;
	virtual void RHIDrawIndexedPrimitive(FIndexBufferRHIParamRef IndexBuffer, uint32 PrimitiveType, int32 BaseVertexIndex, uint32 FirstInstance, uint32 NumVertices, uint32 StartIndex, uint32 NumPrimitives, uint32 NumInstances) final override;
	virtual void RHIDrawIndexedPrimitiveIndirect(uint32 PrimitiveType, FIndexBufferRHIParamRef IndexBuffer, FVertexBufferRHIParamRef ArgumentBuffer, uint32 ArgumentOffset) final override;
	virtual void RHIBeginDrawPrimitiveUP(uint32 PrimitiveType, uint32 NumPrimitives, uint32 NumVertices, uint32 VertexDataStride, void*& OutVertexData) final override;
	virtual void RHIEndDrawPrimitiveUP() final override;
	virtual void RHIBeginDrawIndexedPrimitiveUP(uint32 PrimitiveType, uint32 NumPrimitives, uint32 NumVertices, uint32 VertexDataStride, void*& OutVertexData, uint32 MinVertexIndex, uint32 NumIndices, uint32 IndexDataStride, void*& OutIndexData) final override;
	virtual void RHIEndDrawIndexedPrimitiveUP() final override;
	virtual void RHIEnableDepthBoundsTest(bool bEnable, float MinDepth, float MaxDepth) final override;
	virtual void RHIPushEvent(const TCHAR* Name, FColor Color) final override;
	virtual void RHIPopEvent() final override;

	//#todo-rco: Switch virtual override final
	void RHIGenerateMips(FTextureRHIParamRef Texture, int32 NumMips);

	virtual void RHISetComputeShader(FComputeShaderRHIParamRef ComputeShader) final override;
	virtual void RHISetComputePipelineState(FRHIComputePipelineState* ComputePipelineState) final override;
	virtual void RHIWaitComputeFence(FComputeFenceRHIParamRef InFence) final override;
	virtual void RHIDispatchComputeShader(uint32 ThreadGroupCountX, uint32 ThreadGroupCountY, uint32 ThreadGroupCountZ) final override;
	virtual void RHIDispatchIndirectComputeShader(FVertexBufferRHIParamRef ArgumentBuffer, uint32 ArgumentOffset) final override;
	virtual void RHIAutomaticCacheFlushAfterComputeShader(bool bEnable) final override;

	virtual void RHIFlushComputeShaderCache() final override;
	virtual void RHISetMultipleViewports(uint32 Count, const FViewportBounds* Data) final override;
	virtual void RHIClearTinyUAV(FUnorderedAccessViewRHIParamRef UnorderedAccessViewRHI, const uint32* Values) final override;
	virtual void RHICopyToResolveTarget(FTextureRHIParamRef SourceTexture, FTextureRHIParamRef DestTexture, bool bKeepOriginalSurface, const FResolveParams& ResolveParams) final override;
	virtual void RHITransitionResources(EResourceTransitionAccess TransitionType, FTextureRHIParamRef* InRenderTargets, int32 NumTextures) final override;
	virtual void RHITransitionResources(EResourceTransitionAccess TransitionType, EResourceTransitionPipeline TransitionPipeline, FUnorderedAccessViewRHIParamRef* InUAVs, int32 NumUAVs, FComputeFenceRHIParamRef WriteComputeFence) final override;

	// Render time measurement
	virtual void RHIBeginRenderQuery(FRenderQueryRHIParamRef RenderQuery) final override;
	virtual void RHIEndRenderQuery(FRenderQueryRHIParamRef RenderQuery) final override;

	virtual void RHIUpdateTextureReference(FTextureReferenceRHIParamRef TextureRef, FTextureRHIParamRef NewTexture) final override;
	virtual void RHIBeginOcclusionQueryBatch() final override;
	virtual void RHIEndOcclusionQueryBatch() final override;
	virtual void RHISubmitCommandsHint() final override;

	virtual void RHIBeginDrawingViewport(FViewportRHIParamRef Viewport, FTextureRHIParamRef RenderTargetRHI) final override;
	virtual void RHIEndDrawingViewport(FViewportRHIParamRef Viewport, bool bPresent, bool bLockToVsync) final override;

	virtual void RHIBeginFrame() final override;
	virtual void RHIEndFrame() final override;

	virtual void RHIBeginScene() final override;
	virtual void RHIEndScene() final override;

	inline FVulkanCommandBufferManager* GetCommandBufferManager()
	{
		return CommandBufferManager;
	}

	inline VulkanRHI::FTempFrameAllocationBuffer& GetTempFrameAllocationBuffer()
	{
		return TempFrameAllocationBuffer;
	}

	inline FVulkanPendingGfxState* GetPendingGfxState()
	{
		return PendingGfxState;
	}

	inline FVulkanPendingComputeState* GetPendingComputeState()
	{
		return PendingComputeState;
	}

	// OutSets must have been previously pre-allocated
	FVulkanDescriptorPool* AllocateDescriptorSets(const VkDescriptorSetAllocateInfo& DescriptorSetAllocateInfo, const FVulkanDescriptorSetsLayout& Layout, VkDescriptorSet* OutSets);

	inline void NotifyDeletedRenderTarget(VkImage Image)
	{
		TransitionState.NotifyDeletedRenderTarget(*Device, Image);
	}

	inline void NotifyDeletedImage(VkImage Image)
	{
		TransitionState.NotifyDeletedImage(Image);
	}

	inline FVulkanRenderPass* GetCurrentRenderPass()
	{
		return TransitionState.CurrentRenderPass;
	}

	inline FVulkanRenderPass* GetPreviousRenderPass()
	{
		return TransitionState.PreviousRenderPass;
	}

	inline uint64 GetFrameCounter() const
	{
		return FrameCounter;
	}

	inline FVulkanUniformBufferUploader* GetUniformBufferUploader()
	{
		return UniformBufferUploader;
	}

	inline FVulkanQueue* GetQueue()
	{
		return Queue;
	}

	void WriteBeginTimestamp(FVulkanCmdBuffer* CmdBuffer);
	void WriteEndTimestamp(FVulkanCmdBuffer* CmdBuffer);

	void ReadAndCalculateGPUFrameTime();
	
	inline FVulkanGPUProfiler& GetGPUProfiler() { return GpuProfiler; }
	inline FVulkanDevice* GetDevice() const { return Device; }
	void EndRenderQueryInternal(FVulkanCmdBuffer* CmdBuffer, FVulkanRenderQuery* Query);

	inline VkImageLayout FindLayout(VkImage Image)
	{
		VkImageLayout* Found = TransitionState.CurrentLayout.Find(Image);
		check(Found);
		return *Found;
	}

	inline VkImageLayout FindOrAddLayout(VkImage Image, VkImageLayout NewLayout)
	{
		VkImageLayout* Found = TransitionState.CurrentLayout.Find(Image);
		if (Found)
		{
			return *Found;
		}
		TransitionState.CurrentLayout.Add(Image, NewLayout);
		return NewLayout;
	}

protected:
	FVulkanDynamicRHI* RHI;
	FVulkanDevice* Device;
	FVulkanQueue* Queue;
	const bool bIsImmediate;
	bool bSubmitAtNextSafePoint;
	bool bAutomaticFlushAfterComputeShader;
	FVulkanUniformBufferUploader* UniformBufferUploader;

	void SetShaderUniformBuffer(EShaderFrequency Stage, const FVulkanUniformBuffer* UniformBuffer, int32 BindingIndex, FVulkanShader* ExpectedShader);

	/** Some locally global variables to track the pending primitive information uised in RHIEnd*UP functions */
	VulkanRHI::FTempFrameAllocationBuffer::FTempAllocInfo PendingDrawPrimUPVertexAllocInfo;
	uint32 PendingNumVertices;
	uint32 PendingVertexDataStride;

	VulkanRHI::FTempFrameAllocationBuffer::FTempAllocInfo PendingDrawPrimUPIndexAllocInfo;
	VkIndexType PendingPrimitiveIndexType;
	uint32 PendingPrimitiveType;
	uint32 PendingNumPrimitives;
	uint32 PendingMinVertexIndex;
	uint32 PendingIndexDataStride;

	VulkanRHI::FTempFrameAllocationBuffer TempFrameAllocationBuffer;

	TArray<FString> EventStack;

	FVulkanCommandBufferManager* CommandBufferManager;

	TArray<FVulkanDescriptorPool*> DescriptorPools;

	struct FTransitionState
	{
		FTransitionState()
			: CurrentRenderPass(nullptr)
			, PreviousRenderPass(nullptr)
			, CurrentFramebuffer(nullptr)
		{
		}

		void Destroy(FVulkanDevice& InDevice);

		FVulkanFramebuffer* GetOrCreateFramebuffer(FVulkanDevice& InDevice, const FRHISetRenderTargetsInfo& RenderTargetsInfo, const FVulkanRenderTargetLayout& RTLayout, FVulkanRenderPass* RenderPass);
		FVulkanRenderPass* GetOrCreateRenderPass(FVulkanDevice& InDevice, const FVulkanRenderTargetLayout& RTLayout)
		{
			uint32 RenderPassHash = RTLayout.GetRenderPassHash();
			FVulkanRenderPass** FoundRenderPass = nullptr;
			{
				FScopeLock Lock(&RenderPassesCS);
				FoundRenderPass = RenderPasses.Find(RenderPassHash);
			}
			if (FoundRenderPass)
			{
				return *FoundRenderPass;
			}

			FVulkanRenderPass* RenderPass = new FVulkanRenderPass(InDevice, RTLayout);
			{
				FScopeLock Lock(&RenderPassesCS); 
				RenderPasses.Add(RenderPassHash, RenderPass);
			}
			return RenderPass;
		}

		void BeginRenderPass(FVulkanCommandListContext& Context, FVulkanDevice& InDevice, FVulkanCmdBuffer* CmdBuffer, const FRHISetRenderTargetsInfo& RenderTargetsInfo, const FVulkanRenderTargetLayout& RTLayout, FVulkanRenderPass* RenderPass, FVulkanFramebuffer* Framebuffer);
		void EndRenderPass(FVulkanCmdBuffer* CmdBuffer);
		void ProcessMipChainTransitions(FVulkanCmdBuffer* CmdBuffer, FVulkanFramebuffer* FrameBuffer, uint32 DestMip);

		FVulkanRenderPass* CurrentRenderPass;
		FVulkanRenderPass* PreviousRenderPass;
		FVulkanFramebuffer* CurrentFramebuffer;

		struct FRenderingMipChainInfo
		{
			bool bInsideRenderingMipChain = false;
			FVulkanTextureBase* Texture = nullptr;
			uint32 LastRenderedMip = 0;
			uint32 CurrentMip = 0;
		} RenderingMipChainInfo;

		struct FFlushMipsInfo
		{
			VkImage Image = VK_NULL_HANDLE;
			int8 MipIndex = -1;
		} FlushMipsInfo;

		TMap<VkImage, VkImageLayout> CurrentLayout;

		TMap<uint32, FVulkanRenderPass*> RenderPasses;
		FCriticalSection RenderPassesCS;

		struct FFramebufferList
		{
			TArray<FVulkanFramebuffer*> Framebuffer;
		};
		TMap<uint32, FFramebufferList*> Framebuffers;

		void NotifyDeletedRenderTarget(FVulkanDevice& InDevice, VkImage Image);

		inline void NotifyDeletedImage(VkImage Image)
		{
			CurrentLayout.Remove(Image);
		}

		VkImageLayout FindOrAddLayout(VkImage Image, VkImageLayout LayoutIfNotFound)
		{
			VkImageLayout* Found = CurrentLayout.Find(Image);
			if (Found)
			{
				return *Found;
			}

			CurrentLayout.Add(Image, LayoutIfNotFound);
			return LayoutIfNotFound;
		}

		void TransitionResource(FVulkanCmdBuffer* CmdBuffer, FVulkanSurface& Surface, VulkanRHI::EImageLayoutBarrier DestLayout);
	};
	FTransitionState TransitionState;

	struct FOcclusionQueryData
	{
		FVulkanCmdBuffer* CmdBuffer;
		uint64 FenceCounter;

		FOcclusionQueryData()
			: CmdBuffer(nullptr)
			, FenceCounter(0)
		{
		}

		void AddToResetList(FVulkanQueryPool* Pool, int32 QueryIndex)
		{
			TArray<uint64>& ListPerPool = ResetList.FindOrAdd(Pool);
			int32 Word = QueryIndex / 64;
			uint64 Bit = QueryIndex % 64;
			uint64 BitMask = (uint64)1 << Bit;
			if (Word >= ListPerPool.Num())
			{
				ListPerPool.AddZeroed(Word - ListPerPool.Num() + 1);
			}
			ListPerPool[Word] = ListPerPool[Word] | BitMask;
		}

		void ResetQueries(FVulkanCmdBuffer* CmdBuffer);

		void ClearResetList()
		{
			for (auto& Pair : ResetList)
			{
				FMemory::Memzero(&Pair.Value[0], Pair.Value.Num() * sizeof(uint64));
			}
		}

		TMap<FVulkanQueryPool*, TArray<uint64>> ResetList;
	};
	FOcclusionQueryData CurrentOcclusionQueryData;
	void AdvanceQuery(FVulkanRenderQuery* Query);

	// List of UAVs which need setting for pixel shaders. D3D treats UAVs like rendertargets so the RHI doesn't make SetUAV calls at the right time
	struct FPendingPixelUAV
	{
		FVulkanUnorderedAccessView* UAV;
		uint32 BindIndex;
	};
	TArray<FPendingPixelUAV> PendingPixelUAVs;

	FVulkanPendingGfxState* PendingGfxState;
	FVulkanPendingComputeState* PendingComputeState;

	void PrepareForCPURead();
	void RequestSubmitCurrentCommands();

	void InternalClearMRT(FVulkanCmdBuffer* CmdBuffer, bool bClearColor, int32 NumClearColors, const FLinearColor* ColorArray, bool bClearDepth, float Depth, bool bClearStencil, uint32 Stencil);

public:
	inline FTransitionState& GetTransitionState()
	{
		return TransitionState;
	}

	FVulkanRenderPass* PrepareRenderPassForPSOCreation(const FGraphicsPipelineStateInitializer& Initializer);
	FVulkanRenderPass* PrepareRenderPassForPSOCreation(const FVulkanRenderTargetLayout& Initializer);

private:
	void RHIClearMRT(bool bClearColor, int32 NumClearColors, const FLinearColor* ColorArray, bool bClearDepth, float Depth, bool bClearStencil, uint32 Stencil);

	inline bool SafePointSubmit()
	{
		if (bSubmitAtNextSafePoint)
		{
			InternalSubmitActiveCmdBuffer();
			bSubmitAtNextSafePoint = false;
			return true;
		}

		return false;
	}

	void InternalSubmitActiveCmdBuffer();
	void FlushAfterComputeShader();

	friend class FVulkanDevice;
	friend class FVulkanDynamicRHI;

	// Number of times EndFrame() has been called on this context
	uint64 FrameCounter;

	FVulkanGPUProfiler GpuProfiler;
	FVulkanGPUTiming* FrameTiming;

	friend struct FVulkanCommandContextContainer;
};


struct FVulkanCommandContextContainer : public IRHICommandContextContainer, public VulkanRHI::FDeviceChild
{
	FVulkanCommandListContext* CmdContext;

	FVulkanCommandContextContainer(FVulkanDevice* InDevice)
		: VulkanRHI::FDeviceChild(InDevice)
		, CmdContext(nullptr)
	{
	}

	virtual IRHICommandContext* GetContext() override final;
	virtual void FinishContext() override final;
	virtual void SubmitAndFreeContextContainer(int32 Index, int32 Num) override final;

	/** Custom new/delete with recycling */
	void* operator new(size_t Size);
	void operator delete(void* RawMemory);

private:
	friend class FVulkanDevice;
};
