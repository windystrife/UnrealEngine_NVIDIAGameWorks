// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved..

/*=============================================================================
	VulkanPendingState.cpp: Private VulkanPendingState function definitions.
=============================================================================*/

#include "VulkanRHIPrivate.h"
#include "VulkanPendingState.h"
#include "VulkanPipeline.h"
#include "VulkanContext.h"

FVulkanDescriptorPool::FVulkanDescriptorPool(FVulkanDevice* InDevice)
	: Device(InDevice)
	, MaxDescriptorSets(0)
	, NumAllocatedDescriptorSets(0)
	, PeakAllocatedDescriptorSets(0)
	, DescriptorPool(VK_NULL_HANDLE)
{
	// Increased from 8192 to prevent Protostar crashing on Mali
	MaxDescriptorSets = 16384;

	const VkPhysicalDeviceLimits& Limits = Device->GetLimits();
	FMemory::Memzero(MaxAllocatedTypes);
	FMemory::Memzero(NumAllocatedTypes);
	FMemory::Memzero(PeakAllocatedTypes);

	//#todo-rco: Get some initial values
	uint32 LimitMaxUniformBuffers = 2048;
	uint32 LimitMaxSamplers = 1024;
	uint32 LimitMaxCombinedImageSamplers = 4096;
	uint32 LimitMaxUniformTexelBuffers = 512;
	uint32 LimitMaxStorageTexelBuffers = 512;
	uint32 LimitMaxStorageBuffers = 512;
	uint32 LimitMaxStorageImage = 512;

	TArray<VkDescriptorPoolSize> Types;
	VkDescriptorPoolSize* Type = new(Types) VkDescriptorPoolSize;
	FMemory::Memzero(*Type);
	Type->type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	Type->descriptorCount = LimitMaxUniformBuffers;

	Type = new(Types) VkDescriptorPoolSize;
	FMemory::Memzero(*Type);
	Type->type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
	Type->descriptorCount = LimitMaxUniformBuffers;

	Type = new(Types) VkDescriptorPoolSize;
	FMemory::Memzero(*Type);
	Type->type = VK_DESCRIPTOR_TYPE_SAMPLER;
	Type->descriptorCount = LimitMaxSamplers;

	Type = new(Types) VkDescriptorPoolSize;
	FMemory::Memzero(*Type);
	Type->type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	Type->descriptorCount = LimitMaxCombinedImageSamplers;

	Type = new(Types) VkDescriptorPoolSize;
	FMemory::Memzero(*Type);
	Type->type = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
	Type->descriptorCount = LimitMaxUniformTexelBuffers;

	Type = new(Types) VkDescriptorPoolSize;
	FMemory::Memzero(*Type);
	Type->type = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
	Type->descriptorCount = LimitMaxStorageTexelBuffers;

	Type = new(Types) VkDescriptorPoolSize;
	FMemory::Memzero(*Type);
	Type->type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	Type->descriptorCount = LimitMaxStorageBuffers;

	Type = new(Types) VkDescriptorPoolSize;
	FMemory::Memzero(*Type);
	Type->type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	Type->descriptorCount = LimitMaxStorageImage;

	for (const VkDescriptorPoolSize& PoolSize : Types)
	{
		MaxAllocatedTypes[PoolSize.type] = PoolSize.descriptorCount;
	}

	VkDescriptorPoolCreateInfo PoolInfo;
	FMemory::Memzero(PoolInfo);
	PoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	PoolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	PoolInfo.poolSizeCount = Types.Num();
	PoolInfo.pPoolSizes = Types.GetData();
	PoolInfo.maxSets = MaxDescriptorSets;

	VERIFYVULKANRESULT(VulkanRHI::vkCreateDescriptorPool(Device->GetInstanceHandle(), &PoolInfo, nullptr, &DescriptorPool));
}

FVulkanDescriptorPool::~FVulkanDescriptorPool()
{
	if (DescriptorPool != VK_NULL_HANDLE)
	{
		VulkanRHI::vkDestroyDescriptorPool(Device->GetInstanceHandle(), DescriptorPool, nullptr);
		DescriptorPool = VK_NULL_HANDLE;
	}
}

void FVulkanDescriptorPool::TrackAddUsage(const FVulkanDescriptorSetsLayout& Layout)
{
	// Check and increment our current type usage
	for (uint32 TypeIndex = VK_DESCRIPTOR_TYPE_BEGIN_RANGE; TypeIndex < VK_DESCRIPTOR_TYPE_END_RANGE; ++TypeIndex)
	{
		NumAllocatedTypes[TypeIndex] +=	(int32)Layout.GetTypesUsed((VkDescriptorType)TypeIndex);
		PeakAllocatedTypes[TypeIndex] = FMath::Max(PeakAllocatedTypes[TypeIndex], NumAllocatedTypes[TypeIndex]);
	}

	NumAllocatedDescriptorSets += Layout.GetLayouts().Num();
	PeakAllocatedDescriptorSets = FMath::Max(NumAllocatedDescriptorSets, PeakAllocatedDescriptorSets);
}

void FVulkanDescriptorPool::TrackRemoveUsage(const FVulkanDescriptorSetsLayout& Layout)
{
	for (uint32 TypeIndex = VK_DESCRIPTOR_TYPE_BEGIN_RANGE; TypeIndex < VK_DESCRIPTOR_TYPE_END_RANGE; ++TypeIndex)
	{
		NumAllocatedTypes[TypeIndex] -=	(int32)Layout.GetTypesUsed((VkDescriptorType)TypeIndex);
		check(NumAllocatedTypes[TypeIndex] >= 0);
	}

	NumAllocatedDescriptorSets -= Layout.GetLayouts().Num();
}


FVulkanPendingComputeState::~FVulkanPendingComputeState()
{
	for (auto Pair : PipelineStates)
	{
		FVulkanComputePipelineState* State = Pair.Value;
		delete State;
	}
}


void FVulkanPendingComputeState::SetSRV(uint32 BindIndex, FVulkanShaderResourceView* SRV)
{
	if (SRV)
	{
		// make sure any dynamically backed SRV points to current memory
		SRV->UpdateView();
		if (SRV->BufferViews.Num() != 0)
		{
			FVulkanBufferView* BufferView = SRV->GetBufferView();
			checkf(BufferView->View != VK_NULL_HANDLE, TEXT("Empty SRV"));
			CurrentState->SetSRVBufferViewState(BindIndex, BufferView);
		}
		else if (SRV->SourceStructuredBuffer)
		{
			CurrentState->SetStorageBuffer(BindIndex, SRV->SourceStructuredBuffer->GetHandle(), SRV->SourceStructuredBuffer->GetOffset(), SRV->SourceStructuredBuffer->GetSize(), SRV->SourceStructuredBuffer->GetBufferUsageFlags());
		}
		else
		{
			checkf(SRV->TextureView.View != VK_NULL_HANDLE, TEXT("Empty SRV"));
			VkImageLayout Layout = Context.FindLayout(SRV->TextureView.Image);
			CurrentState->SetSRVTextureView(BindIndex, SRV->TextureView, Layout);
		}
	}
	else
	{
		//CurrentState->SetSRVBufferViewState(BindIndex, nullptr);
	}
}

void FVulkanPendingComputeState::SetUAV(uint32 UAVIndex, FVulkanUnorderedAccessView* UAV)
{
	if (UAV)
	{
		// make sure any dynamically backed UAV points to current memory
		UAV->UpdateView();
		if (UAV->SourceStructuredBuffer)
		{
			CurrentState->SetStorageBuffer(UAVIndex, UAV->SourceStructuredBuffer->GetHandle(), UAV->SourceStructuredBuffer->GetOffset(), UAV->SourceStructuredBuffer->GetSize(), UAV->SourceStructuredBuffer->GetBufferUsageFlags());
		}
		else if (UAV->BufferView)
		{
			CurrentState->SetUAVTexelBufferViewState(UAVIndex, UAV->BufferView);
		}
		else if (UAV->SourceTexture)
		{
			VkImageLayout Layout = Context.FindOrAddLayout(UAV->TextureView.Image, VK_IMAGE_LAYOUT_UNDEFINED);
			if (Layout != VK_IMAGE_LAYOUT_GENERAL)
			{
				FVulkanTextureBase* VulkanTexture = GetVulkanTextureFromRHITexture(UAV->SourceTexture);
				FVulkanCmdBuffer* CmdBuffer = Context.GetCommandBufferManager()->GetActiveCmdBuffer();
				ensure(CmdBuffer->IsOutsideRenderPass());
				Context.GetTransitionState().TransitionResource(CmdBuffer, VulkanTexture->Surface, VulkanRHI::EImageLayoutBarrier::ComputeGeneralRW);
			}
			CurrentState->SetUAVTextureView(UAVIndex, UAV->TextureView);
		}
		else
		{
			ensure(0);
		}
	}
}


void FVulkanPendingComputeState::PrepareForDispatch(FVulkanCmdBuffer* InCmdBuffer)
{
	SCOPE_CYCLE_COUNTER(STAT_VulkanDispatchCallPrepareTime);

	check(CurrentState);
	const bool bHasDescriptorSets = CurrentState->UpdateDescriptorSets(&Context, InCmdBuffer, &GlobalUniformPool);

	VkCommandBuffer CmdBuffer = InCmdBuffer->GetHandle();

	{
		//#todo-rco: Move this to SetComputePipeline()
		SCOPE_CYCLE_COUNTER(STAT_VulkanPipelineBind);
		CurrentPipeline->Bind(CmdBuffer);
		if (bHasDescriptorSets)
		{
			CurrentState->BindDescriptorSets(CmdBuffer);
		}
	}
}

FVulkanPendingGfxState::~FVulkanPendingGfxState()
{
	for (auto Pair : PipelineStates)
	{
		FVulkanGfxPipelineState* State = Pair.Value;
		delete State;
	}
}

void FVulkanPendingGfxState::PrepareForDraw(FVulkanCmdBuffer* CmdBuffer, VkPrimitiveTopology Topology)
{
	SCOPE_CYCLE_COUNTER(STAT_VulkanDrawCallPrepareTime);

	ensure(Topology == UEToVulkanType(CurrentPipeline->PipelineStateInitializer.PrimitiveType));

	bool bHasDescriptorSets = CurrentState->UpdateDescriptorSets(&Context, CmdBuffer, &GlobalUniformPool);

	UpdateDynamicStates(CmdBuffer);

	if (bHasDescriptorSets)
	{
		CurrentState->BindDescriptorSets(CmdBuffer->GetHandle());
	}

	if (bDirtyVertexStreams)
	{
#if VULKAN_ENABLE_AGGRESSIVE_STATS
		SCOPE_CYCLE_COUNTER(STAT_VulkanBindVertexStreamsTime);
#endif
		// Its possible to have no vertex buffers
		const FVulkanVertexInputStateInfo& VertexInputStateInfo = CurrentPipeline->Pipeline->GetVertexInputState();
		if (VertexInputStateInfo.AttributesNum == 0)
		{
			// However, we need to verify that there are also no bindings
			check(VertexInputStateInfo.BindingsNum == 0);
			return;
		}

		TemporaryIA.VertexBuffers.Reset(0);
		TemporaryIA.VertexOffsets.Reset(0);
		const VkVertexInputAttributeDescription* CurrAttribute = nullptr;
		for (uint32 BindingIndex = 0; BindingIndex < VertexInputStateInfo.BindingsNum; BindingIndex++)
		{
			const VkVertexInputBindingDescription& CurrBinding = VertexInputStateInfo.Bindings[BindingIndex];

			uint32 StreamIndex = VertexInputStateInfo.BindingToStream.FindChecked(BindingIndex);
			FVulkanPendingGfxState::FVertexStream& CurrStream = PendingStreams[StreamIndex];

			// Verify the vertex buffer is set
			if (/*!CurrStream.Stream && */!CurrStream.Stream2 && CurrStream.Stream3 == VK_NULL_HANDLE)
			{
				// The attribute in stream index is probably compiled out
#if VULKAN_HAS_DEBUGGING_ENABLED
				// Lets verify
				for (uint32 AttributeIndex = 0; AttributeIndex < VertexInputStateInfo.AttributesNum; AttributeIndex++)
				{
					if (VertexInputStateInfo.Attributes[AttributeIndex].binding == CurrBinding.binding)
					{
						UE_LOG(LogVulkanRHI, Warning, TEXT("Missing binding on location %d in '%s' vertex shader"),
							CurrBinding.binding,
							*CurrentBSS->GetShader(SF_Vertex)->GetDebugName());
						ensure(0);
					}
				}
#endif
				continue;
			}

			TemporaryIA.VertexBuffers.Add(/*CurrStream.Stream
								  ? CurrStream.Stream->GetBufferHandle()
								  : (*/CurrStream.Stream2
								  ? CurrStream.Stream2->GetHandle()
								  : CurrStream.Stream3//)
			);
			TemporaryIA.VertexOffsets.Add(CurrStream.BufferOffset);
		}

		if (TemporaryIA.VertexBuffers.Num() > 0)
		{
			// Bindings are expected to be in ascending order with no index gaps in between:
			// Correct:		0, 1, 2, 3
			// Incorrect:	1, 0, 2, 3
			// Incorrect:	0, 2, 3, 5
			// Reordering and creation of stream binding index is done in "GenerateVertexInputStateInfo()"
			VulkanRHI::vkCmdBindVertexBuffers(CmdBuffer->GetHandle(), 0, TemporaryIA.VertexBuffers.Num(), TemporaryIA.VertexBuffers.GetData(), TemporaryIA.VertexOffsets.GetData());
		}

		bDirtyVertexStreams = true;
	}
}

void FVulkanPendingGfxState::InternalUpdateDynamicStates(FVulkanCmdBuffer* Cmd)
{
	bool bInCmdNeedsDynamicState = Cmd->bNeedsDynamicStateSet;

	bool bNeedsUpdateViewport = !Cmd->bHasViewport || (Cmd->bHasViewport && FMemory::Memcmp((const void*)&Cmd->CurrentViewport, (const void*)&Viewport, sizeof(VkViewport)) != 0);
	// Validate and update Viewport
	if (bNeedsUpdateViewport)
	{
		ensure(Viewport.width > 0 || Viewport.height > 0);
		VulkanRHI::vkCmdSetViewport(Cmd->GetHandle(), 0, 1, &Viewport);
		FMemory::Memcpy(Cmd->CurrentViewport, Viewport);
		Cmd->bHasViewport = true;
	}

	bool bNeedsUpdateScissor = !Cmd->bHasScissor || (Cmd->bHasScissor && FMemory::Memcmp((const void*)&Cmd->CurrentScissor, (const void*)&Scissor, sizeof(VkRect2D)) != 0);
	if (bNeedsUpdateScissor)
	{
		VulkanRHI::vkCmdSetScissor(Cmd->GetHandle(), 0, 1, &Scissor);
		FMemory::Memcpy(Cmd->CurrentScissor, Scissor);
		Cmd->bHasScissor = true;
	}

	bool bNeedsUpdateStencil = !Cmd->bHasStencilRef || (Cmd->bHasStencilRef && Cmd->CurrentStencilRef != StencilRef);
	if (bNeedsUpdateStencil)
	{
		VulkanRHI::vkCmdSetStencilReference(Cmd->GetHandle(), VK_STENCIL_FRONT_AND_BACK, StencilRef);
		Cmd->CurrentStencilRef = StencilRef;
		Cmd->bHasStencilRef = true;
	}

	Cmd->bNeedsDynamicStateSet = false;
}

void FVulkanPendingGfxState::SetSRV(EShaderFrequency Stage, uint32 BindIndex, FVulkanShaderResourceView* SRV)
{
	if (SRV)
	{
		// make sure any dynamically backed SRV points to current memory
		SRV->UpdateView();
		if (SRV->BufferViews.Num() != 0)
		{
			FVulkanBufferView* BufferView = SRV->GetBufferView();
			checkf(BufferView->View != VK_NULL_HANDLE, TEXT("Empty SRV"));
			CurrentState->SetSRVBufferViewState(Stage, BindIndex, BufferView);
		}
		else if (SRV->SourceStructuredBuffer)
		{
			CurrentState->SetStorageBuffer(Stage, BindIndex, SRV->SourceStructuredBuffer->GetHandle(), SRV->SourceStructuredBuffer->GetOffset(), SRV->SourceStructuredBuffer->GetSize(), SRV->SourceStructuredBuffer->GetBufferUsageFlags());
		}
		else
		{
			checkf(SRV->TextureView.View != VK_NULL_HANDLE, TEXT("Empty SRV"));
			VkImageLayout Layout = Context.FindLayout(SRV->TextureView.Image);
			CurrentState->SetSRVTextureView(Stage, BindIndex, SRV->TextureView, Layout);
		}
	}
	else
	{
		//CurrentState->SetSRVBufferViewState(Stage, BindIndex, nullptr);
	}
}

void FVulkanPendingGfxState::SetUAV(EShaderFrequency Stage, uint32 UAVIndex, FVulkanUnorderedAccessView* UAV)
{
	if (UAV)
	{
		// make sure any dynamically backed UAV points to current memory
		UAV->UpdateView();
		if (UAV->SourceStructuredBuffer)
		{
			CurrentState->SetStorageBuffer(Stage, UAVIndex, UAV->SourceStructuredBuffer->GetHandle(), UAV->SourceStructuredBuffer->GetOffset(), UAV->SourceStructuredBuffer->GetSize(), UAV->SourceStructuredBuffer->GetBufferUsageFlags());
		}
		else if (UAV->BufferView)
		{
			CurrentState->SetUAVTexelBufferViewState(Stage, UAVIndex, UAV->BufferView);
		}
		else if (UAV->SourceTexture)
		{
			VkImageLayout Layout = Context.FindLayout(UAV->TextureView.Image);
			CurrentState->SetUAVTextureView(Stage, UAVIndex, UAV->TextureView, Layout);
		}
		else
		{
			ensure(0);
		}
	}
}
