// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanStatePipeline.cpp: Vulkan pipeline state implementation.
=============================================================================*/

#include "VulkanRHIPrivate.h"
#include "VulkanPipelineState.h"
#include "VulkanResources.h"
#include "VulkanPipeline.h"
#include "VulkanContext.h"
#include "VulkanPendingState.h"
#include "VulkanPipeline.h"


FVulkanComputePipelineState::FVulkanComputePipelineState(FVulkanDevice* InDevice, FVulkanComputePipeline* InComputePipeline)
	: FVulkanCommonPipelineState(InDevice)
	, PackedUniformBuffersMask(0)
	, PackedUniformBuffersDirty(0)
	, ComputePipeline(InComputePipeline)
{
	PackedUniformBuffers.Init(&InComputePipeline->GetShaderCodeHeader(), PackedUniformBuffersMask, UniformBuffersWithDataMask);

	CreateDescriptorWriteInfos();
	InComputePipeline->AddRef();
}

void FVulkanComputePipelineState::CreateDescriptorWriteInfos()
{
	check(DSWriteContainer.DescriptorWrites.Num() == 0);

	const FVulkanCodeHeader& CodeHeader = ComputePipeline->GetShaderCodeHeader();

	DSWriteContainer.DescriptorWrites.AddZeroed(CodeHeader.NEWDescriptorInfo.DescriptorTypes.Num());
	DSWriteContainer.DescriptorImageInfo.AddZeroed(CodeHeader.NEWDescriptorInfo.NumImageInfos);
	DSWriteContainer.DescriptorBufferInfo.AddZeroed(CodeHeader.NEWDescriptorInfo.NumBufferInfos);

	VkSampler DefaultSampler = Device->GetDefaultSampler();
	VkImageView DefaultImageView = Device->GetDefaultImageView();
	for (int32 Index = 0; Index < DSWriteContainer.DescriptorImageInfo.Num(); ++Index)
	{
		// Texture.Load() still requires a default sampler...
		DSWriteContainer.DescriptorImageInfo[Index].sampler = DefaultSampler;
		DSWriteContainer.DescriptorImageInfo[Index].imageView = DefaultImageView;
		DSWriteContainer.DescriptorImageInfo[Index].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	}

	VkWriteDescriptorSet* CurrentDescriptorWrite = DSWriteContainer.DescriptorWrites.GetData();
	VkDescriptorImageInfo* CurrentImageInfo = DSWriteContainer.DescriptorImageInfo.GetData();
	VkDescriptorBufferInfo* CurrentBufferInfo = DSWriteContainer.DescriptorBufferInfo.GetData();

	DSWriter.SetupDescriptorWrites(CodeHeader.NEWDescriptorInfo, CurrentDescriptorWrite, CurrentImageInfo, CurrentBufferInfo);
}

bool FVulkanComputePipelineState::UpdateDescriptorSets(FVulkanCommandListContext* CmdListContext, FVulkanCmdBuffer* CmdBuffer, FVulkanGlobalUniformPool* GlobalUniformPool)
{
#if VULKAN_ENABLE_AGGRESSIVE_STATS
	SCOPE_CYCLE_COUNTER(STAT_VulkanUpdateDescriptorSets);
#endif
	int32 WriteIndex = 0;

	DSRingBuffer.CurrDescriptorSets = DSRingBuffer.RequestDescriptorSets(CmdListContext, CmdBuffer, ComputePipeline->GetLayout());
	if (!DSRingBuffer.CurrDescriptorSets)
	{
		return false;
	}

	const FVulkanDescriptorSets::FDescriptorSetArray& DescriptorSetHandles = DSRingBuffer.CurrDescriptorSets->GetHandles();
	int32 DescriptorSetIndex = 0;

	FVulkanUniformBufferUploader* UniformBufferUploader = CmdListContext->GetUniformBufferUploader();
	uint8* CPURingBufferBase = (uint8*)UniformBufferUploader->GetCPUMappedPointer();

	const VkDescriptorSet DescriptorSet = DescriptorSetHandles[DescriptorSetIndex];
	++DescriptorSetIndex;

	bool bRequiresPackedUBUpdate = (PackedUniformBuffersDirty != 0);
	if (bRequiresPackedUBUpdate)
	{
		SCOPE_CYCLE_COUNTER(STAT_VulkanApplyDSUniformBuffers);
		UpdatePackedUniformBuffers(Device->GetLimits().minUniformBufferOffsetAlignment, ComputePipeline->GetShaderCodeHeader(), PackedUniformBuffers, DSWriter, UniformBufferUploader, CPURingBufferBase, PackedUniformBuffersDirty);
		PackedUniformBuffersDirty = 0;
	}

	bool bRequiresNonPackedUBUpdate = (DSWriter.DirtyMask != 0);
	if (!bRequiresNonPackedUBUpdate && !bRequiresPackedUBUpdate)
	{
		//#todo-rco: Skip this desc set writes and only call update for the modified ones!
		//continue;
		int x = 0;
	}

	DSWriter.SetDescriptorSet(DescriptorSet);

#if VULKAN_ENABLE_AGGRESSIVE_STATS
	INC_DWORD_STAT_BY(STAT_VulkanNumUpdateDescriptors, DSWriteContainer.DescriptorWrites.Num());
	INC_DWORD_STAT_BY(STAT_VulkanNumDescSets, DescriptorSetIndex);
#endif

	{
#if VULKAN_ENABLE_AGGRESSIVE_STATS
		SCOPE_CYCLE_COUNTER(STAT_VulkanVkUpdateDS);
#endif
		VulkanRHI::vkUpdateDescriptorSets(Device->GetInstanceHandle(), DSWriteContainer.DescriptorWrites.Num(), DSWriteContainer.DescriptorWrites.GetData(), 0, nullptr);
	}

	return true;
}


FVulkanGfxPipelineState::FVulkanGfxPipelineState(FVulkanDevice* InDevice, FVulkanGraphicsPipelineState* InGfxPipeline, FVulkanBoundShaderState* InBSS)
	: FVulkanCommonPipelineState(InDevice)
	, GfxPipeline(InGfxPipeline), BSS(InBSS)
{
	FMemory::Memzero(PackedUniformBuffersMask);
	FMemory::Memzero(PackedUniformBuffersDirty);

	PackedUniformBuffers[SF_Vertex].Init(&BSS->GetVertexShader()->GetCodeHeader(), PackedUniformBuffersMask[SF_Vertex], UniformBuffersWithDataMask[SF_Vertex]);

	if (BSS->GetPixelShader())
	{
		PackedUniformBuffers[SF_Pixel].Init(&BSS->GetPixelShader()->GetCodeHeader(), PackedUniformBuffersMask[SF_Pixel], UniformBuffersWithDataMask[SF_Pixel]);
	}
	if (BSS->GetGeometryShader())
	{
		PackedUniformBuffers[SF_Geometry].Init(&BSS->GetGeometryShader()->GetCodeHeader(), PackedUniformBuffersMask[SF_Geometry], UniformBuffersWithDataMask[SF_Geometry]);
	}
	if (BSS->GetHullShader())
	{
		PackedUniformBuffers[SF_Domain].Init(&BSS->GetDomainShader()->GetCodeHeader(), PackedUniformBuffersMask[SF_Domain], UniformBuffersWithDataMask[SF_Domain]);
		PackedUniformBuffers[SF_Hull].Init(&BSS->GetHullShader()->GetCodeHeader(), PackedUniformBuffersMask[SF_Hull], UniformBuffersWithDataMask[SF_Hull]);
	}

	CreateDescriptorWriteInfos();

	static int32 IDCounter = 0;
	ID = IDCounter++;

	//UE_LOG(LogVulkanRHI, Warning, TEXT("GfxPSOState %p For PSO %p Writes:%d"), this, InGfxPipeline, DSWriteContainer.DescriptorWrites.Num());

	InGfxPipeline->AddRef();
	BSS->AddRef();
}

void FVulkanGfxPipelineState::CreateDescriptorWriteInfos()
{
	check(DSWriteContainer.DescriptorWrites.Num() == 0);

	static_assert(SF_Geometry + 1 == SF_Compute, "Loop assumes compute is after gfx stages!");

	for (uint32 Stage = 0; Stage < SF_Compute; Stage++)
	{
		const FVulkanShader* StageShader = BSS->GetShader((EShaderFrequency)Stage);
		if (!StageShader)
		{
			continue;
		}

		const FVulkanCodeHeader& CodeHeader = StageShader->GetCodeHeader();
		DSWriteContainer.DescriptorWrites.AddZeroed(CodeHeader.NEWDescriptorInfo.DescriptorTypes.Num());
		DSWriteContainer.DescriptorImageInfo.AddZeroed(CodeHeader.NEWDescriptorInfo.NumImageInfos);
		DSWriteContainer.DescriptorBufferInfo.AddZeroed(CodeHeader.NEWDescriptorInfo.NumBufferInfos);
	}

	VkSampler DefaultSampler = Device->GetDefaultSampler();
	VkImageView DefaultImageView = Device->GetDefaultImageView();
	for (int32 Index = 0; Index < DSWriteContainer.DescriptorImageInfo.Num(); ++Index)
	{
		// Texture.Load() still requires a default sampler...
		DSWriteContainer.DescriptorImageInfo[Index].sampler = DefaultSampler;
		DSWriteContainer.DescriptorImageInfo[Index].imageView = DefaultImageView;
		DSWriteContainer.DescriptorImageInfo[Index].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	}

	VkWriteDescriptorSet* CurrentDescriptorWrite = DSWriteContainer.DescriptorWrites.GetData();
	VkDescriptorImageInfo* CurrentImageInfo = DSWriteContainer.DescriptorImageInfo.GetData();
	VkDescriptorBufferInfo* CurrentBufferInfo = DSWriteContainer.DescriptorBufferInfo.GetData();

	for (uint32 Stage = 0; Stage < SF_Compute; Stage++)
	{
		const FVulkanShader* StageShader = BSS->GetShader((EShaderFrequency)Stage);
		if (!StageShader)
		{
			continue;
		}

		const FVulkanCodeHeader& CodeHeader = StageShader->GetCodeHeader();
		DSWriter[Stage].SetupDescriptorWrites(CodeHeader.NEWDescriptorInfo, CurrentDescriptorWrite, CurrentImageInfo, CurrentBufferInfo);

		CurrentDescriptorWrite += CodeHeader.NEWDescriptorInfo.DescriptorTypes.Num();
		CurrentImageInfo += CodeHeader.NEWDescriptorInfo.NumImageInfos;
		CurrentBufferInfo += CodeHeader.NEWDescriptorInfo.NumBufferInfos;
	}
}

bool FVulkanGfxPipelineState::UpdateDescriptorSets(FVulkanCommandListContext* CmdListContext, FVulkanCmdBuffer* CmdBuffer, FVulkanGlobalUniformPool* GlobalUniformPool)
{
#if VULKAN_ENABLE_AGGRESSIVE_STATS
	SCOPE_CYCLE_COUNTER(STAT_VulkanUpdateDescriptorSets);
#endif

	check(GlobalUniformPool);

	int32 WriteIndex = 0;

	DSRingBuffer.CurrDescriptorSets = DSRingBuffer.RequestDescriptorSets(CmdListContext, CmdBuffer, GfxPipeline->Pipeline->GetLayout());
	if (!DSRingBuffer.CurrDescriptorSets)
	{
		return false;
	}

	const FVulkanDescriptorSets::FDescriptorSetArray& DescriptorSetHandles = DSRingBuffer.CurrDescriptorSets->GetHandles();
	int32 DescriptorSetIndex = 0;

	FVulkanUniformBufferUploader* UniformBufferUploader = CmdListContext->GetUniformBufferUploader();
	uint8* CPURingBufferBase = (uint8*)UniformBufferUploader->GetCPUMappedPointer();

	static_assert(SF_Geometry + 1 == SF_Compute, "Loop assumes compute is after gfx stages!");
	for (uint32 Stage = 0; Stage < SF_Compute; Stage++)
	{
		const FVulkanShader* StageShader = BSS->GetShader((EShaderFrequency)Stage);
		if (!StageShader)
		{
			++DescriptorSetIndex;
			continue;
		}

		const FVulkanCodeHeader& CodeHeader = StageShader->GetCodeHeader();
		if (CodeHeader.NEWDescriptorInfo.DescriptorTypes.Num() == 0)
		{
			// Empty set, still has its own index
			++DescriptorSetIndex;
			continue;
		}

		const VkDescriptorSet DescriptorSet = DescriptorSetHandles[DescriptorSetIndex];
		++DescriptorSetIndex;

		bool bRequiresPackedUBUpdate = (PackedUniformBuffersDirty[Stage] != 0);
		if (bRequiresPackedUBUpdate)
		{
			SCOPE_CYCLE_COUNTER(STAT_VulkanApplyDSUniformBuffers);
			UpdatePackedUniformBuffers(Device->GetLimits().minUniformBufferOffsetAlignment, CodeHeader, PackedUniformBuffers[Stage], DSWriter[Stage], UniformBufferUploader, CPURingBufferBase, PackedUniformBuffersDirty[Stage]);
			PackedUniformBuffersDirty[Stage] = 0;
		}

		bool bRequiresNonPackedUBUpdate = (DSWriter[Stage].DirtyMask != 0);
		if (!bRequiresNonPackedUBUpdate && !bRequiresPackedUBUpdate)
		{
			//#todo-rco: Skip this desc set writes and only call update for the modified ones!
			//continue;
			int x = 0;
		}

		DSWriter[Stage].SetDescriptorSet(DescriptorSet);
	}

#if VULKAN_ENABLE_AGGRESSIVE_STATS
	INC_DWORD_STAT_BY(STAT_VulkanNumUpdateDescriptors, DSWriteContainer.DescriptorWrites.Num());
	INC_DWORD_STAT_BY(STAT_VulkanNumDescSets, DescriptorSetIndex);
#endif

	{
#if VULKAN_ENABLE_AGGRESSIVE_STATS
		SCOPE_CYCLE_COUNTER(STAT_VulkanVkUpdateDS);
#endif
		VulkanRHI::vkUpdateDescriptorSets(Device->GetInstanceHandle(), DSWriteContainer.DescriptorWrites.Num(), DSWriteContainer.DescriptorWrites.GetData(), 0, nullptr);
	}

	return true;
}

void FVulkanCommandListContext::RHISetGraphicsPipelineState(FGraphicsPipelineStateRHIParamRef GraphicsState)
{
	FVulkanGraphicsPipelineState* Pipeline = ResourceCast(GraphicsState);
	
	FVulkanCmdBuffer* CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();
	if (PendingGfxState->SetGfxPipeline(Pipeline) || !CmdBuffer->bHasPipeline)
	{
		SCOPE_CYCLE_COUNTER(STAT_VulkanPipelineBind);
		PendingGfxState->CurrentPipeline->Pipeline->Bind(CmdBuffer->GetHandle());
		CmdBuffer->bHasPipeline = true;
		PendingGfxState->MarkNeedsDynamicStates();
		PendingGfxState->StencilRef = 0;
	}

	// Yuck - Bind pending pixel shader UAVs from SetRenderTargets
	{
		for (int32 Index = 0; Index < PendingPixelUAVs.Num(); ++Index)
		{
			PendingGfxState->SetUAV(SF_Pixel, PendingPixelUAVs[Index].BindIndex, PendingPixelUAVs[Index].UAV);
		}
	}
}
