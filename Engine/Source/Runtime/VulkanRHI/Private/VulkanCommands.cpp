// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanCommands.cpp: Vulkan RHI commands implementation.
=============================================================================*/

#include "VulkanRHIPrivate.h"
#include "VulkanPendingState.h"
#include "VulkanContext.h"
#include "EngineGlobals.h"

//#todo-rco: One of this per Context!
static TGlobalResource< TBoundShaderStateHistory<10000, false> > GBoundShaderStateHistory;

static TAutoConsoleVariable<int32> GCVarSubmitOnDispatch(
	TEXT("r.Vulkan.SubmitOnDispatch"),
	0,
	TEXT("0 to not do anything special on dispatch(default)\n")\
	TEXT("1 to submit the cmd buffer after each dispatch"),
	ECVF_RenderThreadSafe
);

static inline bool UseRealUBs()
{
	static TConsoleVariableData<int32>* CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Vulkan.UseRealUBs"));
	return (CVar && CVar->GetValueOnAnyThread() != 0);
}

void FVulkanCommandListContext::RHISetStreamSource(uint32 StreamIndex,FVertexBufferRHIParamRef VertexBufferRHI, uint32 Stride, uint32 Offset)
{
	FVulkanVertexBuffer* VertexBuffer = ResourceCast(VertexBufferRHI);
	if (VertexBuffer != NULL)
	{
		PendingGfxState->SetStreamSource(StreamIndex, VertexBuffer, Offset + VertexBuffer->GetOffset());
	}
}

void FVulkanCommandListContext::RHISetStreamSource(uint32 StreamIndex, FVertexBufferRHIParamRef VertexBufferRHI, uint32 Offset)
{
	FVulkanVertexBuffer* VertexBuffer = ResourceCast(VertexBufferRHI);
	if (VertexBuffer != NULL)
	{
		PendingGfxState->SetStreamSource(StreamIndex, VertexBuffer, Offset + VertexBuffer->GetOffset());
	}
}

void FVulkanDynamicRHI::RHISetStreamOutTargets(uint32 NumTargets, const FVertexBufferRHIParamRef* VertexBuffers, const uint32* Offsets)
{
	VULKAN_SIGNAL_UNIMPLEMENTED();
}

void FVulkanCommandListContext::RHISetRasterizerState(FRasterizerStateRHIParamRef NewStateRHI)
{
	check(0);
}

void FVulkanCommandListContext::RHISetComputeShader(FComputeShaderRHIParamRef ComputeShaderRHI)
{
	FVulkanComputeShader* ComputeShader = ResourceCast(ComputeShaderRHI);
	FVulkanComputePipeline* ComputePipeline = Device->GetPipelineStateCache()->GetOrCreateComputePipeline(ComputeShader);
	RHISetComputePipelineState(ComputePipeline);
}

void FVulkanCommandListContext::RHISetComputePipelineState(FRHIComputePipelineState* ComputePipelineState)
{
	FVulkanCmdBuffer* CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();
	if (CmdBuffer->IsInsideRenderPass())
	{
		TransitionState.EndRenderPass(CmdBuffer);
	}

	//#todo-rco: Set PendingGfx to null
	FVulkanComputePipeline* ComputePipeline = ResourceCast(ComputePipelineState);
	PendingComputeState->SetComputePipeline(ComputePipeline);
}

void FVulkanCommandListContext::RHIDispatchComputeShader(uint32 ThreadGroupCountX, uint32 ThreadGroupCountY, uint32 ThreadGroupCountZ)
{
	SCOPE_CYCLE_COUNTER(STAT_VulkanDispatchCallTime);

	FVulkanCmdBuffer* Cmd = CommandBufferManager->GetActiveCmdBuffer();
	ensure(Cmd->IsOutsideRenderPass());
	VkCommandBuffer CmdBuffer = Cmd->GetHandle();
	PendingComputeState->PrepareForDispatch(Cmd);
	VulkanRHI::vkCmdDispatch(CmdBuffer, ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);

	if (GCVarSubmitOnDispatch.GetValueOnRenderThread())
	{
		InternalSubmitActiveCmdBuffer();
	}

	// flush any needed buffers that the compute shader wrote to	
	if (bAutomaticFlushAfterComputeShader)
	{
		FlushAfterComputeShader();
	}

	if (IsImmediate())
	{
		GpuProfiler.RegisterGPUWork(1);
	}
}

void FVulkanCommandListContext::RHIDispatchIndirectComputeShader(FVertexBufferRHIParamRef ArgumentBufferRHI, uint32 ArgumentOffset) 
{ 
	FVulkanVertexBuffer* ArgumentBuffer = ResourceCast(ArgumentBufferRHI);

	FVulkanCmdBuffer* Cmd = CommandBufferManager->GetActiveCmdBuffer();
	ensure(Cmd->IsOutsideRenderPass());
	VkCommandBuffer CmdBuffer = Cmd->GetHandle();
	PendingComputeState->PrepareForDispatch(Cmd);
	VulkanRHI::vkCmdDispatchIndirect(CmdBuffer, ArgumentBuffer->GetHandle(), ArgumentOffset);

	if (GCVarSubmitOnDispatch.GetValueOnRenderThread())
	{
		InternalSubmitActiveCmdBuffer();
	}

	// flush any needed buffers that the compute shader wrote to	
	if (bAutomaticFlushAfterComputeShader)
	{
		FlushAfterComputeShader();
	}

	//if (IsImmediate())
	{
		GpuProfiler.RegisterGPUWork(1);
	}
}

void FVulkanCommandListContext::RHISetBoundShaderState(FBoundShaderStateRHIParamRef BoundShaderStateRHI)
{
	check(0);
}


void FVulkanCommandListContext::RHISetUAVParameter(FComputeShaderRHIParamRef ComputeShaderRHI, uint32 UAVIndex, FUnorderedAccessViewRHIParamRef UAVRHI)
{
	check(PendingComputeState->GetCurrentShader() == ResourceCast(ComputeShaderRHI));

	FVulkanUnorderedAccessView* UAV = ResourceCast(UAVRHI);
	PendingComputeState->SetUAV(UAVIndex, UAV);
	if (bAutomaticFlushAfterComputeShader && UAV)
	{
		PendingComputeState->AddUAVForAutoFlush(UAV);
	}
}

void FVulkanCommandListContext::RHISetUAVParameter(FComputeShaderRHIParamRef ComputeShaderRHI,uint32 UAVIndex,FUnorderedAccessViewRHIParamRef UAVRHI, uint32 InitialCount)
{
	check(PendingComputeState->GetCurrentShader() == ResourceCast(ComputeShaderRHI));

	FVulkanUnorderedAccessView* UAV = ResourceCast(UAVRHI);
	ensure(0);
}


void FVulkanCommandListContext::RHISetShaderTexture(FVertexShaderRHIParamRef VertexShaderRHI, uint32 TextureIndex, FTextureRHIParamRef NewTextureRHI)
{
	check(PendingGfxState->CurrentBSS && PendingGfxState->CurrentBSS->GetShader(SF_Vertex) == ResourceCast(VertexShaderRHI));
	FVulkanTextureBase* Texture = GetVulkanTextureFromRHITexture(NewTextureRHI);
	PendingGfxState->SetTexture(SF_Vertex, TextureIndex, Texture);
}

void FVulkanCommandListContext::RHISetShaderTexture(FHullShaderRHIParamRef HullShaderRHI, uint32 TextureIndex, FTextureRHIParamRef NewTextureRHI)
{
	check(PendingGfxState->CurrentBSS && PendingGfxState->CurrentBSS->GetShader(SF_Hull) == ResourceCast(HullShaderRHI));
	FVulkanTextureBase* Texture = GetVulkanTextureFromRHITexture(NewTextureRHI);
	PendingGfxState->SetTexture(SF_Hull, TextureIndex, Texture);
}

void FVulkanCommandListContext::RHISetShaderTexture(FDomainShaderRHIParamRef DomainShaderRHI, uint32 TextureIndex, FTextureRHIParamRef NewTextureRHI)
{
	check(PendingGfxState->CurrentBSS && PendingGfxState->CurrentBSS->GetShader(SF_Domain) == ResourceCast(DomainShaderRHI));
	FVulkanTextureBase* Texture = GetVulkanTextureFromRHITexture(NewTextureRHI);
	PendingGfxState->SetTexture(SF_Domain, TextureIndex, Texture);
}

void FVulkanCommandListContext::RHISetShaderTexture(FGeometryShaderRHIParamRef GeometryShaderRHI, uint32 TextureIndex, FTextureRHIParamRef NewTextureRHI)
{
	check(PendingGfxState->CurrentBSS && PendingGfxState->CurrentBSS->GetShader(SF_Geometry) == ResourceCast(GeometryShaderRHI));
	FVulkanTextureBase* Texture = GetVulkanTextureFromRHITexture(NewTextureRHI);
	PendingGfxState->SetTexture(SF_Geometry, TextureIndex, Texture);
}

void FVulkanCommandListContext::RHISetShaderTexture(FPixelShaderRHIParamRef PixelShaderRHI, uint32 TextureIndex, FTextureRHIParamRef NewTextureRHI)
{
	check(PendingGfxState->CurrentBSS && PendingGfxState->CurrentBSS->GetShader(SF_Pixel) == ResourceCast(PixelShaderRHI));
	FVulkanTextureBase* Texture = GetVulkanTextureFromRHITexture(NewTextureRHI);
	PendingGfxState->SetTexture(SF_Pixel, TextureIndex, Texture);
}

void FVulkanCommandListContext::RHISetShaderTexture(FComputeShaderRHIParamRef ComputeShader, uint32 TextureIndex, FTextureRHIParamRef NewTextureRHI)
{
	check(PendingComputeState->GetCurrentShader() == ResourceCast(ComputeShader));

	FVulkanTextureBase* VulkanTexture = GetVulkanTextureFromRHITexture(NewTextureRHI);
	PendingComputeState->SetTexture(TextureIndex, VulkanTexture);
}

void FVulkanCommandListContext::RHISetShaderResourceViewParameter(FVertexShaderRHIParamRef VertexShaderRHI, uint32 TextureIndex, FShaderResourceViewRHIParamRef SRVRHI)
{
	check(PendingGfxState->CurrentBSS && PendingGfxState->CurrentBSS->GetShader(SF_Vertex) == ResourceCast(VertexShaderRHI));
	FVulkanShaderResourceView* SRV = ResourceCast(SRVRHI);
	PendingGfxState->SetSRV(SF_Vertex, TextureIndex, SRV);
}

void FVulkanCommandListContext::RHISetShaderResourceViewParameter(FHullShaderRHIParamRef HullShaderRHI,uint32 TextureIndex,FShaderResourceViewRHIParamRef SRVRHI)
{
	check(PendingGfxState->CurrentBSS && PendingGfxState->CurrentBSS->GetShader(SF_Hull) == ResourceCast(HullShaderRHI));
	FVulkanShaderResourceView* SRV = ResourceCast(SRVRHI);
	PendingGfxState->SetSRV(SF_Hull, TextureIndex, SRV);
}

void FVulkanCommandListContext::RHISetShaderResourceViewParameter(FDomainShaderRHIParamRef DomainShaderRHI,uint32 TextureIndex,FShaderResourceViewRHIParamRef SRVRHI)
{
	check(PendingGfxState->CurrentBSS && PendingGfxState->CurrentBSS->GetShader(SF_Domain) == ResourceCast(DomainShaderRHI));
	FVulkanShaderResourceView* SRV = ResourceCast(SRVRHI);
	PendingGfxState->SetSRV(SF_Domain, TextureIndex, SRV);
}

void FVulkanCommandListContext::RHISetShaderResourceViewParameter(FGeometryShaderRHIParamRef GeometryShaderRHI,uint32 TextureIndex,FShaderResourceViewRHIParamRef SRVRHI)
{
	check(PendingGfxState->CurrentBSS && PendingGfxState->CurrentBSS->GetShader(SF_Geometry) == ResourceCast(GeometryShaderRHI));
	FVulkanShaderResourceView* SRV = ResourceCast(SRVRHI);
	PendingGfxState->SetSRV(SF_Geometry, TextureIndex, SRV);
}

void FVulkanCommandListContext::RHISetShaderResourceViewParameter(FPixelShaderRHIParamRef PixelShaderRHI,uint32 TextureIndex,FShaderResourceViewRHIParamRef SRVRHI)
{
	check(PendingGfxState->CurrentBSS && PendingGfxState->CurrentBSS->GetShader(SF_Pixel) == ResourceCast(PixelShaderRHI));
	FVulkanShaderResourceView* SRV = ResourceCast(SRVRHI);
	PendingGfxState->SetSRV(SF_Pixel, TextureIndex, SRV);
}

void FVulkanCommandListContext::RHISetShaderResourceViewParameter(FComputeShaderRHIParamRef ComputeShaderRHI,uint32 TextureIndex,FShaderResourceViewRHIParamRef SRVRHI)
{
	check(PendingComputeState->GetCurrentShader() == ResourceCast(ComputeShaderRHI));
	FVulkanShaderResourceView* SRV = ResourceCast(SRVRHI);
	PendingComputeState->SetSRV(TextureIndex, SRV);
}

void FVulkanCommandListContext::RHISetShaderSampler(FVertexShaderRHIParamRef VertexShaderRHI, uint32 SamplerIndex, FSamplerStateRHIParamRef NewStateRHI)
{
	check(PendingGfxState->CurrentBSS && PendingGfxState->CurrentBSS->GetShader(SF_Vertex) == ResourceCast(VertexShaderRHI));
	FVulkanSamplerState* Sampler = ResourceCast(NewStateRHI);
	PendingGfxState->SetSamplerState(SF_Vertex, SamplerIndex, Sampler);
}

void FVulkanCommandListContext::RHISetShaderSampler(FHullShaderRHIParamRef HullShaderRHI, uint32 SamplerIndex, FSamplerStateRHIParamRef NewStateRHI)
{
	check(PendingGfxState->CurrentBSS && PendingGfxState->CurrentBSS->GetShader(SF_Hull) == ResourceCast(HullShaderRHI));
	FVulkanSamplerState* Sampler = ResourceCast(NewStateRHI);
	PendingGfxState->SetSamplerState(SF_Hull, SamplerIndex, Sampler);
}

void FVulkanCommandListContext::RHISetShaderSampler(FDomainShaderRHIParamRef DomainShaderRHI, uint32 SamplerIndex, FSamplerStateRHIParamRef NewStateRHI)
{
	check(PendingGfxState->CurrentBSS && PendingGfxState->CurrentBSS->GetShader(SF_Domain) == ResourceCast(DomainShaderRHI));
	FVulkanSamplerState* Sampler = ResourceCast(NewStateRHI);
	PendingGfxState->SetSamplerState(SF_Domain, SamplerIndex, Sampler);
}

void FVulkanCommandListContext::RHISetShaderSampler(FGeometryShaderRHIParamRef GeometryShaderRHI, uint32 SamplerIndex, FSamplerStateRHIParamRef NewStateRHI)
{
	check(PendingGfxState->CurrentBSS && PendingGfxState->CurrentBSS->GetShader(SF_Geometry) == ResourceCast(GeometryShaderRHI));
	FVulkanSamplerState* Sampler = ResourceCast(NewStateRHI);
	PendingGfxState->SetSamplerState(SF_Geometry, SamplerIndex, Sampler);
}

void FVulkanCommandListContext::RHISetShaderSampler(FPixelShaderRHIParamRef PixelShaderRHI, uint32 SamplerIndex, FSamplerStateRHIParamRef NewStateRHI)
{
	check(PendingGfxState->CurrentBSS && PendingGfxState->CurrentBSS->GetShader(SF_Pixel) == ResourceCast(PixelShaderRHI));
	FVulkanSamplerState* Sampler = ResourceCast(NewStateRHI);
	PendingGfxState->SetSamplerState(SF_Pixel, SamplerIndex, Sampler);
}

void FVulkanCommandListContext::RHISetShaderSampler(FComputeShaderRHIParamRef ComputeShaderRHI, uint32 SamplerIndex, FSamplerStateRHIParamRef NewStateRHI)
{
	check(PendingComputeState->GetCurrentShader() == ResourceCast(ComputeShaderRHI));
	FVulkanSamplerState* Sampler = ResourceCast(NewStateRHI);
	PendingComputeState->SetSamplerState(SamplerIndex, Sampler);
}

void FVulkanCommandListContext::RHISetShaderParameter(FVertexShaderRHIParamRef VertexShaderRHI, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue)
{
	check(PendingGfxState->CurrentBSS && PendingGfxState->CurrentBSS->GetShader(SF_Vertex) == ResourceCast(VertexShaderRHI));

	PendingGfxState->SetShaderParameter(SF_Vertex, BufferIndex, BaseIndex, NumBytes, NewValue);
}

void FVulkanCommandListContext::RHISetShaderParameter(FHullShaderRHIParamRef HullShaderRHI, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue)
{
	check(PendingGfxState->CurrentBSS && PendingGfxState->CurrentBSS->GetShader(SF_Hull) == ResourceCast(HullShaderRHI));

	PendingGfxState->SetShaderParameter(SF_Hull, BufferIndex, BaseIndex, NumBytes, NewValue);
}

void FVulkanCommandListContext::RHISetShaderParameter(FDomainShaderRHIParamRef DomainShaderRHI, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue)
{
	check(PendingGfxState->CurrentBSS && PendingGfxState->CurrentBSS->GetShader(SF_Domain) == ResourceCast(DomainShaderRHI));

	PendingGfxState->SetShaderParameter(SF_Domain, BufferIndex, BaseIndex, NumBytes, NewValue);
}

void FVulkanCommandListContext::RHISetShaderParameter(FGeometryShaderRHIParamRef GeometryShaderRHI, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue)
{
	check(PendingGfxState->CurrentBSS && PendingGfxState->CurrentBSS->GetShader(SF_Geometry) == ResourceCast(GeometryShaderRHI));

	PendingGfxState->SetShaderParameter(SF_Geometry, BufferIndex, BaseIndex, NumBytes, NewValue);
}

void FVulkanCommandListContext::RHISetShaderParameter(FPixelShaderRHIParamRef PixelShaderRHI, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue)
{
	check(PendingGfxState->CurrentBSS && PendingGfxState->CurrentBSS->GetShader(SF_Pixel) == ResourceCast(PixelShaderRHI));

	PendingGfxState->SetShaderParameter(SF_Pixel, BufferIndex, BaseIndex, NumBytes, NewValue);
}

void FVulkanCommandListContext::RHISetShaderParameter(FComputeShaderRHIParamRef ComputeShaderRHI,uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue)
{
	check(PendingComputeState->GetCurrentShader() == ResourceCast(ComputeShaderRHI));

	PendingComputeState->SetShaderParameter(BufferIndex, BaseIndex, NumBytes, NewValue);
}

struct FSrtResourceBinding
{
	typedef TRefCountPtr<FRHIResource> ResourceRef;

	FSrtResourceBinding(): BindingIndex(-1), Resource(nullptr) {}
	FSrtResourceBinding(int32 InBindingIndex, FRHIResource* InResource): BindingIndex(InBindingIndex), Resource(InResource) {}

	int32 BindingIndex;
	ResourceRef Resource;
};


static void GatherUniformBufferResources(
	const TArray<uint32>& InBindingArray,
	const uint32& InBindingMask,
	const FVulkanUniformBuffer* UniformBuffer,
	uint32 BufferIndex,
	TArray<FSrtResourceBinding>& OutResourcesBindings)
{
	check(UniformBuffer);

	if (!((1 << BufferIndex) & InBindingMask))
	{
		return;
	}

	const TArray<TRefCountPtr<FRHIResource>>& ResourceArray = UniformBuffer->GetResourceTable();

	// Expected to get an empty array
	check(OutResourcesBindings.Num() == 0);

	OutResourcesBindings.Empty(ResourceArray.Num());

	// Verify mask and array corelational validity
	check(InBindingMask == 0 ? (InBindingArray.Num() == 0) : (InBindingArray.Num() > 0));

	// InBindingArray contains index to the buffer offset and also buffer offsets
	uint32 BufferOffset = InBindingArray[BufferIndex];
	const uint32* ResourceInfos = &InBindingArray[BufferOffset];
	uint32 ResourceInfo = *ResourceInfos++;

	// Extract all resources related to the current BufferIndex
	do
	{
		// Verify that we have correct buffer index
		check(FRHIResourceTableEntry::GetUniformBufferIndex(ResourceInfo) == BufferIndex);

		// Extract binding index from ResourceInfo
		const uint32 BindingIndex = FRHIResourceTableEntry::GetBindIndex(ResourceInfo);

		// Extract index of the resource stored in the resource table from ResourceInfo
		const uint16 ResourceIndex = FRHIResourceTableEntry::GetResourceIndex(ResourceInfo);

		if(ResourceIndex < ResourceArray.Num())
		{
			check(ResourceArray[ResourceIndex]);
			OutResourcesBindings.Add(FSrtResourceBinding(BindingIndex, ResourceArray[ResourceIndex]));
		}

		// Iterate to next info
		ResourceInfo = *ResourceInfos++;
	}
	while(FRHIResourceTableEntry::GetUniformBufferIndex(ResourceInfo) == BufferIndex);
}

inline void FVulkanCommandListContext::SetShaderUniformBuffer(EShaderFrequency Stage, const FVulkanUniformBuffer* UniformBuffer, int32 BindingIndex, FVulkanShader* ExpectedShader)
{
#if VULKAN_ENABLE_AGGRESSIVE_STATS
	SCOPE_CYCLE_COUNTER(STAT_VulkanSetUniformBufferTime);
#endif

	const FVulkanShader* Shader = PendingGfxState->CurrentBSS->GetShader(Stage);
	check(Shader == ExpectedShader);
	if (UniformBuffer->GetLayout().ConstantBufferSize > 0)
	{
		if (UseRealUBs())
		{
			PendingGfxState->SetUniformBuffer(Stage, BindingIndex, UniformBuffer);
		}
		else
		{
			PendingGfxState->SetUniformBufferConstantData(Stage, BindingIndex, UniformBuffer->ConstantData);
		}
	}

	const FShaderCompilerResourceTable& ResourceBindingTable = Shader->GetCodeHeader().SerializedBindings.ShaderResourceTable;
	if (ResourceBindingTable.ResourceTableLayoutHashes.Num() == 0)
	{
		return;
	}

	// Uniform Buffers
	//#todo-rco: Quite slow...
	// Gather texture bindings from the SRT table
	TArray<FSrtResourceBinding> TextureBindings;
	if (ResourceBindingTable.TextureMap.Num() != 0)
	{
		GatherUniformBufferResources(ResourceBindingTable.TextureMap, ResourceBindingTable.ResourceTableBits, UniformBuffer, BindingIndex, TextureBindings);
	}

	// Gather Sampler bindings from the SRT table
	TArray<FSrtResourceBinding> SamplerBindings;
	if (ResourceBindingTable.SamplerMap.Num() != 0)
	{
		GatherUniformBufferResources(ResourceBindingTable.SamplerMap, ResourceBindingTable.ResourceTableBits, UniformBuffer, BindingIndex, SamplerBindings);
	}

	float CurrentTime = (float)FApp::GetCurrentTime();

	for(int32 Index = 0; Index < TextureBindings.Num(); Index++)
	{
		const FSrtResourceBinding& CurrTextureBinding = TextureBindings[Index];
			FTextureRHIParamRef TexRef = (FTextureRHIParamRef)CurrTextureBinding.Resource.GetReference();
			const FVulkanTextureBase* BaseTexture = FVulkanTextureBase::Cast(TexRef);
			if(BaseTexture)
			{
				PendingGfxState->SetTexture(Stage, CurrTextureBinding.BindingIndex, BaseTexture);
				TexRef->SetLastRenderTime(CurrentTime);
			}
			else
			{
				UE_LOG(LogVulkanRHI, Warning, TEXT("Invalid texture in SRT table for shader '%s'"), *Shader->DebugName);
			}
		}

	for (int32 Index = 0; Index < SamplerBindings.Num(); Index++)
	{
		const FSrtResourceBinding* CurrSamplerBinding = &SamplerBindings[Index];
		FVulkanSamplerState* CurrSampler = static_cast<FVulkanSamplerState*>(CurrSamplerBinding->Resource.GetReference());
		if (CurrSampler)
		{
			if (CurrSampler->Sampler)
			{
				PendingGfxState->SetSamplerState(Stage, CurrSamplerBinding->BindingIndex, CurrSampler);
			}
		}
		else
		{
			UE_LOG(LogVulkanRHI, Warning, TEXT("Invalid sampler in SRT table for shader '%s'"), *Shader->DebugName);
		}
	}
}

void FVulkanCommandListContext::RHISetShaderUniformBuffer(FVertexShaderRHIParamRef VertexShaderRHI, uint32 BufferIndex, FUniformBufferRHIParamRef BufferRHI)
{
	FVulkanUniformBuffer* UniformBuffer = ResourceCast(BufferRHI);
	SetShaderUniformBuffer(SF_Vertex, UniformBuffer, BufferIndex, ResourceCast(VertexShaderRHI));
}

void FVulkanCommandListContext::RHISetShaderUniformBuffer(FHullShaderRHIParamRef HullShaderRHI, uint32 BufferIndex, FUniformBufferRHIParamRef BufferRHI)
{
	FVulkanUniformBuffer* UniformBuffer = ResourceCast(BufferRHI);
	SetShaderUniformBuffer(SF_Hull, UniformBuffer, BufferIndex, ResourceCast(HullShaderRHI));
}

void FVulkanCommandListContext::RHISetShaderUniformBuffer(FDomainShaderRHIParamRef DomainShaderRHI, uint32 BufferIndex, FUniformBufferRHIParamRef BufferRHI)
{
	FVulkanUniformBuffer* UniformBuffer = ResourceCast(BufferRHI);
	SetShaderUniformBuffer(SF_Domain, UniformBuffer, BufferIndex, ResourceCast(DomainShaderRHI));
}

void FVulkanCommandListContext::RHISetShaderUniformBuffer(FGeometryShaderRHIParamRef GeometryShaderRHI, uint32 BufferIndex, FUniformBufferRHIParamRef BufferRHI)
{
	FVulkanUniformBuffer* UniformBuffer = ResourceCast(BufferRHI);
	SetShaderUniformBuffer(SF_Geometry, UniformBuffer, BufferIndex, ResourceCast(GeometryShaderRHI));
}

void FVulkanCommandListContext::RHISetShaderUniformBuffer(FPixelShaderRHIParamRef PixelShaderRHI, uint32 BufferIndex, FUniformBufferRHIParamRef BufferRHI)
{
	FVulkanUniformBuffer* UniformBuffer = ResourceCast(BufferRHI);
	SetShaderUniformBuffer(SF_Pixel, UniformBuffer, BufferIndex, ResourceCast(PixelShaderRHI));
}

void FVulkanCommandListContext::RHISetShaderUniformBuffer(FComputeShaderRHIParamRef ComputeShaderRHI, uint32 BufferIndex, FUniformBufferRHIParamRef BufferRHI)
{
	check(PendingComputeState->GetCurrentShader() == ResourceCast(ComputeShaderRHI));

#if VULKAN_ENABLE_AGGRESSIVE_STATS
	SCOPE_CYCLE_COUNTER(STAT_VulkanSetUniformBufferTime);
#endif
	FVulkanComputePipelineState& State = *PendingComputeState->CurrentState;

	// Walk through all resources to set all appropriate states
	FVulkanComputeShader* Shader = ResourceCast(ComputeShaderRHI);

	FVulkanUniformBuffer* UniformBuffer = ResourceCast(BufferRHI);
	
	// Uniform Buffers
	if (UniformBuffer->GetLayout().ConstantBufferSize > 0)
	{
		if (UseRealUBs())
		{
			State.SetUniformBuffer(BufferIndex, UniformBuffer);
		}
		else
		{
			State.SetUniformBufferConstantData(BufferIndex, UniformBuffer->ConstantData);
		}
	}

	const FShaderCompilerResourceTable& ResourceBindingTable = Shader->CodeHeader.SerializedBindings.ShaderResourceTable;
	if (ResourceBindingTable.ResourceTableLayoutHashes.Num() == 0)
	{
		return;
	}

	//#todo-rco: Quite slow...
	// Gather texture bindings from the SRT table
	TArray<FSrtResourceBinding> TextureBindings;
	if (ResourceBindingTable.TextureMap.Num() != 0)
	{
		GatherUniformBufferResources(ResourceBindingTable.TextureMap, ResourceBindingTable.ResourceTableBits, UniformBuffer, BufferIndex, TextureBindings);
	}

	// Gather Sampler bindings from the SRT table
	TArray<FSrtResourceBinding> SamplerBindings;
	if (ResourceBindingTable.SamplerMap.Num() != 0)
	{
		GatherUniformBufferResources(ResourceBindingTable.SamplerMap, ResourceBindingTable.ResourceTableBits, UniformBuffer, BufferIndex, SamplerBindings);
	}

	float CurrentTime = (float)FApp::GetCurrentTime();

	for (int32 Index = 0; Index < TextureBindings.Num(); Index++)
	{
		const FSrtResourceBinding& CurrTextureBinding = TextureBindings[Index];
		FTextureRHIParamRef TexRef = (FTextureRHIParamRef)CurrTextureBinding.Resource.GetReference();
		const FVulkanTextureBase* BaseTexture = FVulkanTextureBase::Cast(TexRef);
		if (BaseTexture)
		{
			State.SetTexture(CurrTextureBinding.BindingIndex, BaseTexture);
			TexRef->SetLastRenderTime(CurrentTime);
		}
		else
		{
			UE_LOG(LogVulkanRHI, Warning, TEXT("Invalid texture in SRT table for shader '%s'"), *Shader->DebugName);
		}
	}

	for (int32 Index = 0; Index < SamplerBindings.Num(); Index++)
	{
		const FSrtResourceBinding* CurrSamplerBinding = &SamplerBindings[Index];
		FVulkanSamplerState* CurrSampler = static_cast<FVulkanSamplerState*>(CurrSamplerBinding->Resource.GetReference());
		if (CurrSampler)
		{
			State.SetSamplerState(CurrSamplerBinding->BindingIndex, CurrSampler);
		}
		else
		{
			UE_LOG(LogVulkanRHI, Warning, TEXT("Invalid sampler in SRT table for shader '%s'"), *Shader->DebugName);
		}
	}
}

void FVulkanCommandListContext::RHISetDepthStencilState(FDepthStencilStateRHIParamRef NewStateRHI, uint32 StencilRef)
{
	check(0);
}

void FVulkanCommandListContext::RHISetBlendState(FBlendStateRHIParamRef NewStateRHI, const FLinearColor& BlendFactor)
{
	check(0);
}

void FVulkanCommandListContext::RHISetStencilRef(uint32 StencilRef)
{
	PendingGfxState->SetStencilRef(StencilRef);
}

void FVulkanCommandListContext::RHIDrawPrimitive(uint32 PrimitiveType, uint32 BaseVertexIndex, uint32 NumPrimitives, uint32 NumInstances)
{
	SCOPE_CYCLE_COUNTER(STAT_VulkanDrawCallTime);
	RHI_DRAW_CALL_STATS(PrimitiveType, NumInstances*NumPrimitives);

	FVulkanCmdBuffer* CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();
	PendingGfxState->PrepareForDraw(CmdBuffer, UEToVulkanType((EPrimitiveType)PrimitiveType));
	NumInstances = FMath::Max(1U, NumInstances);
	uint32 NumVertices = GetVertexCountForPrimitiveCount(NumPrimitives, PrimitiveType);
	VulkanRHI::vkCmdDraw(CmdBuffer->GetHandle(), NumVertices, NumInstances, BaseVertexIndex, 0);

	//if (IsImmediate())
	{
		GpuProfiler.RegisterGPUWork(NumPrimitives * NumInstances, NumVertices * NumInstances);
	}
}

void FVulkanCommandListContext::RHIDrawPrimitiveIndirect(uint32 PrimitiveType, FVertexBufferRHIParamRef ArgumentBufferRHI, uint32 ArgumentOffset)
{
	SCOPE_CYCLE_COUNTER(STAT_VulkanDrawCallTime);
	RHI_DRAW_CALL_INC();

	FVulkanCmdBuffer* Cmd = CommandBufferManager->GetActiveCmdBuffer();
	VkCommandBuffer CmdBuffer = Cmd->GetHandle();
	PendingGfxState->PrepareForDraw(Cmd, UEToVulkanType((EPrimitiveType)PrimitiveType));

	FVulkanVertexBuffer* ArgumentBuffer = ResourceCast(ArgumentBufferRHI);

	VulkanRHI::vkCmdDrawIndirect(CmdBuffer, ArgumentBuffer->GetHandle(), ArgumentOffset, 1, sizeof(VkDrawIndexedIndirectCommand));

	if (IsImmediate())
	{
		GpuProfiler.RegisterGPUWork(1);
	}
}

void FVulkanCommandListContext::RHIDrawIndexedPrimitive(FIndexBufferRHIParamRef IndexBufferRHI, uint32 PrimitiveType, int32 BaseVertexIndex, uint32 FirstInstance,
	uint32 NumVertices, uint32 StartIndex, uint32 NumPrimitives, uint32 NumInstances)
{
	SCOPE_CYCLE_COUNTER(STAT_VulkanDrawCallTime);
	RHI_DRAW_CALL_STATS(PrimitiveType, NumInstances*NumPrimitives);

	FVulkanIndexBuffer* IndexBuffer = ResourceCast(IndexBufferRHI);
	FVulkanCmdBuffer* Cmd = CommandBufferManager->GetActiveCmdBuffer();
	VkCommandBuffer CmdBuffer = Cmd->GetHandle();
	PendingGfxState->PrepareForDraw(Cmd, UEToVulkanType((EPrimitiveType)PrimitiveType));
	VulkanRHI::vkCmdBindIndexBuffer(CmdBuffer, IndexBuffer->GetHandle(), IndexBuffer->GetOffset(), IndexBuffer->GetIndexType());

	uint32 NumIndices = GetVertexCountForPrimitiveCount(NumPrimitives, PrimitiveType);
	NumInstances = FMath::Max(1U, NumInstances);
	VulkanRHI::vkCmdDrawIndexed(CmdBuffer, NumIndices, NumInstances, StartIndex, BaseVertexIndex, FirstInstance);

	if (IsImmediate())
	{
		GpuProfiler.RegisterGPUWork(NumPrimitives * NumInstances, NumVertices * NumInstances);
	}
}

void FVulkanCommandListContext::RHIDrawIndexedIndirect(FIndexBufferRHIParamRef IndexBufferRHI, uint32 PrimitiveType, FStructuredBufferRHIParamRef ArgumentsBufferRHI, int32 DrawArgumentsIndex, uint32 NumInstances)
{
	SCOPE_CYCLE_COUNTER(STAT_VulkanDrawCallTime);

#if 0
	//@NOTE: don't prepare draw without actually drawing
#if !PLATFORM_ANDROID
	PendingState->PrepareDraw(UEToVulkanType((EPrimitiveType)PrimitiveType));
	FVulkanIndexBuffer* IndexBuffer = ResourceCast(IndexBufferRHI);
#if 0
	FVulkanStructuredBuffer* ArgumentsBuffer = ResourceCast(ArgumentsBufferRHI);
#endif
#endif
#endif
	VULKAN_SIGNAL_UNIMPLEMENTED();

	if (IsImmediate())
	{
#if 0
		VulkanRHI::GManager.GPUProfilingData.RegisterGPUWork(0);
#endif
	}
}

void FVulkanCommandListContext::RHIDrawIndexedPrimitiveIndirect(uint32 PrimitiveType,FIndexBufferRHIParamRef IndexBufferRHI,FVertexBufferRHIParamRef ArgumentBufferRHI,uint32 ArgumentOffset)
{
	SCOPE_CYCLE_COUNTER(STAT_VulkanDrawCallTime);
	RHI_DRAW_CALL_INC();

	FVulkanIndexBuffer* IndexBuffer = ResourceCast(IndexBufferRHI);
	FVulkanCmdBuffer* Cmd = CommandBufferManager->GetActiveCmdBuffer();
	VkCommandBuffer CmdBuffer = Cmd->GetHandle();
	PendingGfxState->PrepareForDraw(Cmd, UEToVulkanType((EPrimitiveType)PrimitiveType));
	VulkanRHI::vkCmdBindIndexBuffer(CmdBuffer, IndexBuffer->GetHandle(), IndexBuffer->GetOffset(), IndexBuffer->GetIndexType());

	FVulkanVertexBuffer* ArgumentBuffer = ResourceCast(ArgumentBufferRHI);

	VulkanRHI::vkCmdDrawIndexedIndirect(CmdBuffer, ArgumentBuffer->GetHandle(), ArgumentOffset, 1, sizeof(VkDrawIndexedIndirectCommand));

	if (IsImmediate())
	{
		GpuProfiler.RegisterGPUWork(1); 
	}
}

void FVulkanCommandListContext::RHIBeginDrawPrimitiveUP(uint32 PrimitiveType, uint32 NumPrimitives, uint32 NumVertices, uint32 VertexDataStride, void*& OutVertexData)
{
	SCOPE_CYCLE_COUNTER(STAT_VulkanUPPrepTime);

//	checkSlow(GPendingDrawPrimitiveUPVertexData == nullptr);

	TempFrameAllocationBuffer.Alloc(VertexDataStride * NumVertices, VertexDataStride, PendingDrawPrimUPVertexAllocInfo);
	OutVertexData = PendingDrawPrimUPVertexAllocInfo.Data;

	PendingPrimitiveType = PrimitiveType;
	PendingNumPrimitives = NumPrimitives;
	PendingNumVertices = NumVertices;
	PendingVertexDataStride = VertexDataStride;
}


void FVulkanCommandListContext::RHIEndDrawPrimitiveUP()
{
	SCOPE_CYCLE_COUNTER(STAT_VulkanDrawCallTime);
	RHI_DRAW_CALL_STATS(PendingPrimitiveType, PendingNumPrimitives);
	PendingGfxState->SetStreamSource(0, PendingDrawPrimUPVertexAllocInfo.GetHandle(), PendingDrawPrimUPVertexAllocInfo.GetBindOffset());
	FVulkanCmdBuffer* CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();
	PendingGfxState->PrepareForDraw(CmdBuffer, UEToVulkanType((EPrimitiveType)PendingPrimitiveType));
	VkCommandBuffer Cmd = CmdBuffer->GetHandle();
	VulkanRHI::vkCmdDraw(CmdBuffer->GetHandle(), PendingNumVertices, 1, PendingMinVertexIndex, 0);

	if (IsImmediate())
	{
		GpuProfiler.RegisterGPUWork(PendingNumPrimitives, PendingNumVertices);
	}
}

void FVulkanCommandListContext::RHIBeginDrawIndexedPrimitiveUP( uint32 PrimitiveType, uint32 NumPrimitives, uint32 NumVertices, uint32 VertexDataStride,
	void*& OutVertexData, uint32 MinVertexIndex, uint32 NumIndices, uint32 IndexDataStride, void*& OutIndexData)
{
	SCOPE_CYCLE_COUNTER(STAT_VulkanUPPrepTime);

	TempFrameAllocationBuffer.Alloc(VertexDataStride * NumVertices, IndexDataStride, PendingDrawPrimUPVertexAllocInfo);
	OutVertexData = PendingDrawPrimUPVertexAllocInfo.Data;

	check(IndexDataStride == 2 || IndexDataStride == 4);
	PendingPrimitiveIndexType = IndexDataStride == 2 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;
	TempFrameAllocationBuffer.Alloc(IndexDataStride * NumIndices, IndexDataStride, PendingDrawPrimUPIndexAllocInfo);
	OutIndexData = PendingDrawPrimUPIndexAllocInfo.Data;

	PendingPrimitiveType = PrimitiveType;
	PendingNumPrimitives = NumPrimitives;
	PendingMinVertexIndex = MinVertexIndex;
	PendingIndexDataStride = IndexDataStride;

	PendingNumVertices = NumVertices;
	PendingVertexDataStride = VertexDataStride;
}

void FVulkanCommandListContext::RHIEndDrawIndexedPrimitiveUP()
{
	SCOPE_CYCLE_COUNTER(STAT_VulkanDrawCallTime);
	RHI_DRAW_CALL_STATS(PendingPrimitiveType, PendingNumPrimitives);
	PendingGfxState->SetStreamSource(0, PendingDrawPrimUPVertexAllocInfo.GetHandle(), PendingDrawPrimUPVertexAllocInfo.GetBindOffset());
	FVulkanCmdBuffer* CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();
	PendingGfxState->PrepareForDraw(CmdBuffer, UEToVulkanType((EPrimitiveType)PendingPrimitiveType));
	VkCommandBuffer Cmd = CmdBuffer->GetHandle();
	uint32 NumIndices = GetVertexCountForPrimitiveCount(PendingNumPrimitives, PendingPrimitiveType);
	VulkanRHI::vkCmdBindIndexBuffer(Cmd, PendingDrawPrimUPIndexAllocInfo.GetHandle(), PendingDrawPrimUPIndexAllocInfo.GetBindOffset(), PendingPrimitiveIndexType);
	VulkanRHI::vkCmdDrawIndexed(Cmd, NumIndices, 1, PendingMinVertexIndex, 0, 0);

	if (IsImmediate())
	{
		GpuProfiler.RegisterGPUWork(PendingNumPrimitives, PendingNumVertices);
	}
}

void FVulkanCommandListContext::RHIClearMRT(bool bClearColor, int32 NumClearColors, const FLinearColor* ClearColorArray, bool bClearDepth, float Depth, bool bClearStencil, uint32 Stencil)
{
	if (!(bClearColor || bClearDepth || bClearStencil))
	{
		return;
	}

	check(bClearColor ? NumClearColors > 0 : true);

	FVulkanCmdBuffer* CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();
	//FRCLog::Printf(TEXT("RHIClearMRT"));

	const uint32 NumColorAttachments = TransitionState.CurrentFramebuffer->GetNumColorAttachments();
	check(!bClearColor || (uint32)NumClearColors <= NumColorAttachments);
	InternalClearMRT(CmdBuffer, bClearColor, bClearColor ? NumClearColors : 0, ClearColorArray, bClearDepth, Depth, bClearStencil, Stencil);
}

void FVulkanCommandListContext::InternalClearMRT(FVulkanCmdBuffer* CmdBuffer, bool bClearColor, int32 NumClearColors, const FLinearColor* ClearColorArray, bool bClearDepth, float Depth, bool bClearStencil, uint32 Stencil)
{
	if (TransitionState.CurrentRenderPass)
	{
		const VkExtent2D& Extents = TransitionState.CurrentRenderPass->GetLayout().GetExtent2D();
		VkClearRect Rect;
		FMemory::Memzero(Rect);
		Rect.rect.offset.x = 0;
		Rect.rect.offset.y = 0;
		Rect.rect.extent = Extents;

		VkClearAttachment Attachments[MaxSimultaneousRenderTargets + 1];
		FMemory::Memzero(Attachments);

		uint32 NumAttachments = NumClearColors;
		if (bClearColor)
		{
			for (int32 i = 0; i < NumClearColors; ++i)
			{
				Attachments[i].aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				Attachments[i].colorAttachment = i;
				Attachments[i].clearValue.color.float32[0] = ClearColorArray[i].R;
				Attachments[i].clearValue.color.float32[1] = ClearColorArray[i].G;
				Attachments[i].clearValue.color.float32[2] = ClearColorArray[i].B;
				Attachments[i].clearValue.color.float32[3] = ClearColorArray[i].A;
			}
		}

		if (bClearDepth || bClearStencil)
		{
			Attachments[NumClearColors].aspectMask = bClearDepth ? VK_IMAGE_ASPECT_DEPTH_BIT : 0;
			Attachments[NumClearColors].aspectMask |= bClearStencil ? VK_IMAGE_ASPECT_STENCIL_BIT : 0;
			Attachments[NumClearColors].colorAttachment = 0;
			Attachments[NumClearColors].clearValue.depthStencil.depth = Depth;
			Attachments[NumClearColors].clearValue.depthStencil.stencil = Stencil;
			++NumAttachments;
		}

		VulkanRHI::vkCmdClearAttachments(CmdBuffer->GetHandle(), NumAttachments, Attachments, 1, &Rect);
	}
	else
	{
		ensure(0);
		//VulkanRHI::vkCmdClearColorImage(CmdBuffer->GetHandle(), )
	}
}

void FVulkanDynamicRHI::RHISuspendRendering()
{
}

void FVulkanDynamicRHI::RHIResumeRendering()
{
}

bool FVulkanDynamicRHI::RHIIsRenderingSuspended()
{
	return false;
}

void FVulkanDynamicRHI::RHIBlockUntilGPUIdle()
{
	Device->WaitUntilIdle();
}

uint32 FVulkanDynamicRHI::RHIGetGPUFrameCycles()
{
	return GGPUFrameTime;
}

void FVulkanCommandListContext::RHIAutomaticCacheFlushAfterComputeShader(bool bEnable)
{
	bAutomaticFlushAfterComputeShader = bEnable;
}

void FVulkanCommandListContext::RHIFlushComputeShaderCache()
{
	FlushAfterComputeShader();
}

void FVulkanDynamicRHI::RHIExecuteCommandList(FRHICommandList* CmdList)
{
	VULKAN_SIGNAL_UNIMPLEMENTED();
}

void FVulkanCommandListContext::RHIEnableDepthBoundsTest(bool bEnable, float MinDepth, float MaxDepth)
{
	VULKAN_SIGNAL_UNIMPLEMENTED();
}

void FVulkanCommandListContext::RequestSubmitCurrentCommands()
{
	ensure(IsImmediate());
	bSubmitAtNextSafePoint = true;
}

void FVulkanCommandListContext::InternalSubmitActiveCmdBuffer()
{
	CommandBufferManager->SubmitActiveCmdBuffer(false);
	CommandBufferManager->PrepareForNewActiveCommandBuffer();
}

void FVulkanCommandListContext::PrepareForCPURead()
{
	ensure(IsImmediate());
	FVulkanCmdBuffer* CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();
	if (CmdBuffer && CmdBuffer->HasBegun())
	{
		if (CmdBuffer->IsInsideRenderPass())
		{
			//#todo-rco: If we get real render passes then this is not needed
			TransitionState.EndRenderPass(CmdBuffer);
		}

		CommandBufferManager->SubmitActiveCmdBuffer(true);
	}
}

void FVulkanCommandListContext::RHISubmitCommandsHint()
{
	RequestSubmitCurrentCommands();
	FVulkanCmdBuffer* CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();
	if (CmdBuffer && CmdBuffer->HasBegun() && CmdBuffer->IsOutsideRenderPass())
	{
		SafePointSubmit();
	}
	CommandBufferManager->RefreshFenceStatus();
}

void FVulkanCommandListContext::FlushAfterComputeShader()
{
	FVulkanCmdBuffer* CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();
	int32 NumResourcesToFlush = PendingComputeState->UAVListForAutoFlush.Num();
	if (NumResourcesToFlush > 0)
	{
		TArray<VkImageMemoryBarrier> ImageBarriers;
		TArray<VkBufferMemoryBarrier> BufferBarriers;
		for (FVulkanUnorderedAccessView* UAV : PendingComputeState->UAVListForAutoFlush)
		{
			if (UAV->SourceVertexBuffer)
			{
				VkBufferMemoryBarrier Barrier;
				VulkanRHI::SetupAndZeroBufferBarrier(Barrier, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT, UAV->SourceVertexBuffer->GetHandle(), UAV->SourceVertexBuffer->GetOffset(), UAV->SourceVertexBuffer->GetSize());
				BufferBarriers.Add(Barrier);
			}
			else if (UAV->SourceStructuredBuffer)
			{
				VkBufferMemoryBarrier Barrier;
				VulkanRHI::SetupAndZeroBufferBarrier(Barrier, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT, UAV->SourceStructuredBuffer->GetHandle(), UAV->SourceStructuredBuffer->GetOffset(), UAV->SourceStructuredBuffer->GetSize());
				BufferBarriers.Add(Barrier);
			}
			else if (UAV->SourceTexture)
			{
				FVulkanTextureBase* Texture = (FVulkanTextureBase*)UAV->SourceTexture->GetTextureBaseRHI();
				VkImageMemoryBarrier Barrier;
				VkImageLayout Layout = TransitionState.FindOrAddLayout(Texture->Surface.Image, VK_IMAGE_LAYOUT_GENERAL);
				VulkanRHI::SetupAndZeroImageBarrierOLD(Barrier, Texture->Surface, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, Layout, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, Layout);
				ImageBarriers.Add(Barrier);
			}
			else if (UAV->SourceIndexBuffer)
			{
				VkBufferMemoryBarrier Barrier;
				VulkanRHI::SetupAndZeroBufferBarrier(Barrier, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT, UAV->SourceIndexBuffer->GetHandle(), UAV->SourceIndexBuffer->GetOffset(), UAV->SourceIndexBuffer->GetSize());
				BufferBarriers.Add(Barrier);
			}
			else
			{
				ensure(0);
			}
		}
		VulkanRHI::vkCmdPipelineBarrier(CmdBuffer->GetHandle(), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, BufferBarriers.Num(), BufferBarriers.GetData(), ImageBarriers.Num(), ImageBarriers.GetData());
		PendingComputeState->UAVListForAutoFlush.SetNum(0, false);
	}
}

IRHICommandContext* FVulkanCommandContextContainer::GetContext()
{
	check(!CmdContext /*&& FinalCommandList.SubmissionAddrs.Num() == 0*/);
	//FPlatformTLS::SetTlsValue(GGnmManager.GetParallelTranslateTLS(), (void*)1);

	// these are expensive and we don't want to worry about allocating them on the fly, so they should only be allocated while actually used, and it should not be possible to have more than we preallocated, based on the number of task threads
	CmdContext = Device->AcquireDeferredContext();
	//CmdContext->InitContextBuffers();
	//CmdContext->ClearState();
	return CmdContext;
}

void FVulkanCommandContextContainer::FinishContext()
{
	check(CmdContext/* && FinalCommandList.SubmissionAddrs.Num() == 0*/);
	//GGnmManager.TimeSubmitOnCmdListEnd(CmdContext);

	//store off all memory ranges for DCBs to be submitted to the GPU.
	//FinalCommandList = CmdContext->GetContext().Finalize(CmdContext->GetBeginCmdListTimestamp(), CmdContext->GetEndCmdListTimestamp());

	Device->ReleaseDeferredContext(CmdContext);
	//CmdContext = nullptr;
	//CmdContext->CommandBufferManager->GetActiveCmdBuffer()->End();
	//check(!CmdContext/* && FinalCommandList.SubmissionAddrs.Num() > 0*/);

	//FPlatformTLS::SetTlsValue(GGnmManager.GetParallelTranslateTLS(), (void*)0);
}

void FVulkanCommandContextContainer::SubmitAndFreeContextContainer(int32 Index, int32 Num)
{
	if (!Index)
	{
		//printf("BeginParallelContext: %i, %i\n", Index, Num);
		//GGnmManager.BeginParallelContexts();
	}
	//GGnmManager.AddSubmission(FinalCommandList);
	check(CmdContext);
	FVulkanCommandBufferManager* CmdBufMgr = CmdContext->GetCommandBufferManager();
	if (CmdBufMgr->HasPendingUploadCmdBuffer())
	{
		CmdBufMgr->SubmitUploadCmdBuffer(false);
	}
	CmdBufMgr->SubmitActiveCmdBuffer(false);
	CmdBufMgr->PrepareForNewActiveCommandBuffer();
	CmdContext = nullptr;
	//check(!CmdContext/* && FinalCommandList.SubmissionAddrs.Num() != 0*/);
	//if (Index == Num - 1)
	{
		//printf("EndParallelContexts: %i, %i\n", Index, Num);
		//GGnmManager.EndParallelContexts();
	}
	//FinalCommandList.Reset();
	delete this;
}

void* FVulkanCommandContextContainer::operator new(size_t Size)
{
	return FMemory::Malloc(Size);
}

void FVulkanCommandContextContainer::operator delete(void* RawMemory)
{
	FMemory::Free(RawMemory);
}
