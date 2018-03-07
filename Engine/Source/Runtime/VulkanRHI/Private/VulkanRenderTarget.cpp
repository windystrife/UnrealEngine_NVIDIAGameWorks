// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanRenderTarget.cpp: Vulkan render target implementation.
=============================================================================*/

#include "VulkanRHIPrivate.h"
#include "ScreenRendering.h"
#include "VulkanPendingState.h"
#include "VulkanContext.h"
#include "SceneUtils.h"

static int32 GSubmitOnCopyToResolve = 0;
static FAutoConsoleVariableRef CVarVulkanSubmitOnCopyToResolve(
	TEXT("r.Vulkan.SubmitOnCopyToResolve"),
	GSubmitOnCopyToResolve,
	TEXT("Submits the Queue to the GPU on every RHICopyToResolveTarget call.\n")
	TEXT(" 0: Do not submit (default)\n")
	TEXT(" 1: Submit"),
	ECVF_Default
	);

static int32 GIgnoreCPUReads = 0;
static FAutoConsoleVariableRef CVarVulkanIgnoreCPUReads(
	TEXT("r.Vulkan.IgnoreCPUReads"),
	GIgnoreCPUReads,
	TEXT("Debugging utility for GPU->CPU reads.\n")
	TEXT(" 0 will read from the GPU (default).\n")
	TEXT(" 1 will read from GPU but fill the buffer instead of copying from a texture.\n")
	TEXT(" 2 will NOT read from the GPU and fill with zeros.\n"),
	ECVF_Default
	);

void FVulkanCommandListContext::FTransitionState::Destroy(FVulkanDevice& InDevice)
{
	{
		FScopeLock Lock(&RenderPassesCS);
		for (auto& Pair : RenderPasses)
		{
			delete Pair.Value;
		}
		RenderPasses.Reset();
	}

	for (auto& Pair : Framebuffers)
	{
		FFramebufferList* List = Pair.Value;
		for (int32 Index = List->Framebuffer.Num() - 1; Index >= 0; --Index)
		{
			List->Framebuffer[Index]->Destroy(InDevice);
			delete List->Framebuffer[Index];
		}
		delete List;
	}
	Framebuffers.Reset();
}

FVulkanFramebuffer* FVulkanCommandListContext::FTransitionState::GetOrCreateFramebuffer(FVulkanDevice& InDevice, const FRHISetRenderTargetsInfo& RenderTargetsInfo, const FVulkanRenderTargetLayout& RTLayout, FVulkanRenderPass* RenderPass)
{
	uint32 RTLayoutHash = RTLayout.GetHash();

	uint64 MipsAndSlicesValues[MaxSimultaneousRenderTargets];
	for (int32 Index = 0; Index < MaxSimultaneousRenderTargets; ++Index)
	{
		MipsAndSlicesValues[Index] = ((uint64)RenderTargetsInfo.ColorRenderTarget[Index].ArraySliceIndex << (uint64)32) | (uint64)RenderTargetsInfo.ColorRenderTarget[Index].MipIndex;
	}
	RTLayoutHash = FCrc::MemCrc32(MipsAndSlicesValues, sizeof(MipsAndSlicesValues), RTLayoutHash);

	FFramebufferList** FoundFramebufferList = Framebuffers.Find(RTLayoutHash);
	FFramebufferList* FramebufferList = nullptr;
	if (FoundFramebufferList)
	{
		FramebufferList = *FoundFramebufferList;

		for (int32 Index = 0; Index < FramebufferList->Framebuffer.Num(); ++Index)
		{
			if (FramebufferList->Framebuffer[Index]->Matches(RenderTargetsInfo))
			{
				return FramebufferList->Framebuffer[Index];
			}
		}
	}
	else
	{
		FramebufferList = new FFramebufferList;
		Framebuffers.Add(RTLayoutHash, FramebufferList);
	}

	FVulkanFramebuffer* Framebuffer = new FVulkanFramebuffer(InDevice, RenderTargetsInfo, RTLayout, *RenderPass);
	FramebufferList->Framebuffer.Add(Framebuffer);
	return Framebuffer;
}

FVulkanRenderPass* FVulkanCommandListContext::PrepareRenderPassForPSOCreation(const FGraphicsPipelineStateInitializer& Initializer)
{
	FVulkanRenderTargetLayout RTLayout(Initializer);
	return PrepareRenderPassForPSOCreation(RTLayout);
}

FVulkanRenderPass* FVulkanCommandListContext::PrepareRenderPassForPSOCreation(const FVulkanRenderTargetLayout& RTLayout)
{
	FVulkanRenderPass* RenderPass = nullptr;
	RenderPass = TransitionState.GetOrCreateRenderPass(*Device, RTLayout);
	return RenderPass;
}

void FVulkanCommandListContext::FTransitionState::BeginRenderPass(FVulkanCommandListContext& Context, FVulkanDevice& InDevice, FVulkanCmdBuffer* CmdBuffer, const FRHISetRenderTargetsInfo& RenderTargetsInfo, const FVulkanRenderTargetLayout& RTLayout, FVulkanRenderPass* RenderPass, FVulkanFramebuffer* Framebuffer)
{
	check(!CurrentRenderPass);
	VkClearValue ClearValues[MaxSimultaneousRenderTargets + 1];
	FMemory::Memzero(ClearValues);

	FVulkanCommandListContext::FTransitionState::FFlushMipsInfo NewInfo;
	int32 Index = 0;
	for (Index = 0; Index < RenderTargetsInfo.NumColorRenderTargets; ++Index)
	{
		FTextureRHIParamRef Texture = RenderTargetsInfo.ColorRenderTarget[Index].Texture;
		if (Texture)
		{
			FVulkanSurface& Surface = FVulkanTextureBase::Cast(Texture)->Surface;
			VkImage Image = Surface.Image;
			if (Index == 0)
			{
				NewInfo.Image = Image;
				NewInfo.MipIndex = RenderTargetsInfo.ColorRenderTarget[Index].MipIndex;
			}

			VkImageLayout* Found = CurrentLayout.Find(Image);
			if (!Found)
			{
				Found = &CurrentLayout.Add(Image, VK_IMAGE_LAYOUT_UNDEFINED);
			}

			if (*Found != VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
			{
				if (*Found == VK_IMAGE_LAYOUT_UNDEFINED)
				{
					VulkanRHI::ImagePipelineBarrier(CmdBuffer->GetHandle(), Image, EImageLayoutBarrier::Undefined, EImageLayoutBarrier::ColorAttachment, SetupImageSubresourceRange());
				}
				else
				{
					Context.RHITransitionResources(EResourceTransitionAccess::EWritable, &Texture, 1);
				}
			}

			const FLinearColor& ClearColor = Texture->HasClearValue() ? Texture->GetClearColor() : FLinearColor::Black;
			ClearValues[Index].color.float32[0] = ClearColor.R;
			ClearValues[Index].color.float32[1] = ClearColor.G;
			ClearValues[Index].color.float32[2] = ClearColor.B;
			ClearValues[Index].color.float32[3] = ClearColor.A;

			*Found = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		}
	}

	if (RenderTargetsInfo.DepthStencilRenderTarget.Texture)
	{
		FTextureRHIParamRef DSTexture = RenderTargetsInfo.DepthStencilRenderTarget.Texture;
		FVulkanSurface& Surface = FVulkanTextureBase::Cast(DSTexture)->Surface;
		VkImageLayout& DSLayout = CurrentLayout.FindOrAdd(Surface.Image);
		if (DSLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL || DSLayout == VK_IMAGE_LAYOUT_UNDEFINED)
		{
			Context.RHITransitionResources(EResourceTransitionAccess::EWritable, &DSTexture, 1);
			DSLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		}
		else
		{
			ensure(DSLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
		}
		if (DSTexture->HasClearValue())
		{
			float Depth = 0;
			uint32 Stencil = 0;
			DSTexture->GetDepthStencilClearValue(Depth, Stencil);
			ClearValues[RenderTargetsInfo.NumColorRenderTargets].depthStencil.depth = Depth;
			ClearValues[RenderTargetsInfo.NumColorRenderTargets].depthStencil.stencil = Stencil;
		}
	}
	
	// Special case, add a barrier while generating mips
	if (NewInfo.Image == FlushMipsInfo.Image && NewInfo.MipIndex == FlushMipsInfo.MipIndex + 1 && !RenderingMipChainInfo.bInsideRenderingMipChain)
	{
		VkImageSubresourceRange Range = VulkanRHI::SetupImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, NewInfo.MipIndex);
		VulkanRHI::ImagePipelineBarrier(CmdBuffer->GetHandle(), NewInfo.Image, EImageLayoutBarrier::ColorAttachment, EImageLayoutBarrier::PixelShaderRead, Range);
	}
	FlushMipsInfo = NewInfo;

	CmdBuffer->BeginRenderPass(RenderPass->GetLayout(), RenderPass, Framebuffer, ClearValues);

	{
		const VkExtent3D& Extents = RTLayout.GetExtent3D();
		Context.GetPendingGfxState()->SetViewport(0, 0, 0, Extents.width, Extents.height, 1);
	}

	CurrentFramebuffer = Framebuffer;
	CurrentRenderPass = RenderPass;
}

void FVulkanCommandListContext::FTransitionState::EndRenderPass(FVulkanCmdBuffer* CmdBuffer)
{
	check(CurrentRenderPass);
	CmdBuffer->EndRenderPass();
	PreviousRenderPass = CurrentRenderPass;
	CurrentRenderPass = nullptr;
}

void FVulkanCommandListContext::FTransitionState::NotifyDeletedRenderTarget(FVulkanDevice& InDevice, VkImage Image)
{
	for (auto It = Framebuffers.CreateIterator(); It; ++It)
	{
		FFramebufferList* List = It->Value;
		for (int32 Index = List->Framebuffer.Num() - 1; Index >= 0; --Index)
		{
			FVulkanFramebuffer* Framebuffer = List->Framebuffer[Index];
			if (Framebuffer->ContainsRenderTarget(Image))
			{
				List->Framebuffer.RemoveAtSwap(Index, 1, false);
				Framebuffer->Destroy(InDevice);

				if (Framebuffer == CurrentFramebuffer)
				{
					CurrentFramebuffer = nullptr;
				}

				delete Framebuffer;
			}
		}

		if (List->Framebuffer.Num() == 0)
		{
			delete List;
			It.RemoveCurrent();
		}
	}
}

void FVulkanCommandListContext::FTransitionState::TransitionResource(FVulkanCmdBuffer* CmdBuffer, FVulkanSurface& Surface, VulkanRHI::EImageLayoutBarrier DestLayout)
{
	VkImageLayout* FoundLayout = CurrentLayout.Find(Surface.Image);
	VkImageLayout VulkanDestLayout = VulkanRHI::GetImageLayout(DestLayout);
	if (FoundLayout)
	{
		if (*FoundLayout != VulkanDestLayout)
		{
			VulkanRHI::EImageLayoutBarrier SourceLayout = GetImageLayoutFromVulkanLayout(*FoundLayout);
			VulkanRHI::ImagePipelineBarrier(CmdBuffer->GetHandle(), Surface.Image, SourceLayout, DestLayout, VulkanRHI::SetupImageSubresourceRange(Surface.GetFullAspectMask()));
			*FoundLayout = VulkanDestLayout;
		}
	}
	else
	{
		VulkanRHI::ImagePipelineBarrier(CmdBuffer->GetHandle(), Surface.Image, EImageLayoutBarrier::Undefined, DestLayout, VulkanRHI::SetupImageSubresourceRange(Surface.GetFullAspectMask()));
		CurrentLayout.Add(Surface.Image, VulkanDestLayout);
	}
}

inline void FVulkanCommandListContext::FTransitionState::ProcessMipChainTransitions(FVulkanCmdBuffer* CmdBuffer, FVulkanFramebuffer* FrameBuffer, uint32 DestMip)
{
	check(FrameBuffer->ContainsRenderTarget(RenderingMipChainInfo.Texture->Surface.Image));
	check(FrameBuffer->GetNumColorAttachments() == 1);
	check(RenderingMipChainInfo.LastRenderedMip + 1 == DestMip);
	RenderingMipChainInfo.CurrentMip = DestMip;
	// transition last mip to readable
	VkImageLayout FoundLayout = FindOrAddLayout(RenderingMipChainInfo.Texture->Surface.Image, VK_IMAGE_LAYOUT_UNDEFINED);
	// Can't happen!
	check(FoundLayout != VK_IMAGE_LAYOUT_UNDEFINED);

	FVulkanSurface& Surface = RenderingMipChainInfo.Texture->Surface;
	VkImageSubresourceRange Range = VulkanRHI::SetupImageSubresourceRange(Surface.GetFullAspectMask());
	Range.baseMipLevel = DestMip - 1;
	VulkanRHI::ImagePipelineBarrier(CmdBuffer->GetHandle(), Surface.Image, VulkanRHI::EImageLayoutBarrier::ColorAttachment, VulkanRHI::EImageLayoutBarrier::PixelShaderRead, Range);
	//VulkanSetImageLayout(CmdBuffer->GetHandle(), Surface.Image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, Range);

	// Already in color state...
	//++Range.baseMipLevel;
	//VulkanSetImageLayout(CmdBuffer->GetHandle(), Surface.Image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, Range);

	RenderingMipChainInfo.LastRenderedMip = DestMip;
}

void FVulkanCommandListContext::RHISetRenderTargets(uint32 NumSimultaneousRenderTargets, const FRHIRenderTargetView* NewRenderTargets, const FRHIDepthRenderTargetView* NewDepthStencilTarget, uint32 NumUAVs, const FUnorderedAccessViewRHIParamRef* UAVs)
{
	//FRCLog::Printf(FString::Printf(TEXT("RHISetRenderTargets")));

	FRHIDepthRenderTargetView DepthView;
	if (NewDepthStencilTarget)
	{
		DepthView = *NewDepthStencilTarget;
	}
	else
	{
		DepthView = FRHIDepthRenderTargetView(FTextureRHIParamRef(), ERenderTargetLoadAction::ENoAction, ERenderTargetStoreAction::ENoAction, ERenderTargetLoadAction::ENoAction, ERenderTargetStoreAction::ENoAction);
	}

	if (NumSimultaneousRenderTargets == 1 && (!NewRenderTargets || !NewRenderTargets->Texture))
	{
		--NumSimultaneousRenderTargets;
	}

	FRHISetRenderTargetsInfo RenderTargetsInfo(NumSimultaneousRenderTargets, NewRenderTargets, DepthView);

	FVulkanRenderTargetLayout RTLayout(RenderTargetsInfo);
	const uint32 RTLayoutHash = RTLayout.GetHash();

	FVulkanRenderPass* RenderPass = nullptr;
	FVulkanFramebuffer* Framebuffer = nullptr;

	if (RTLayout.GetExtent2D().width != 0 && RTLayout.GetExtent2D().height != 0)
	{
		RenderPass = TransitionState.GetOrCreateRenderPass(*Device, RTLayout);
		Framebuffer = TransitionState.GetOrCreateFramebuffer(*Device, RenderTargetsInfo, RTLayout, RenderPass);
	}

	if (Framebuffer == TransitionState.CurrentFramebuffer && RenderPass == TransitionState.CurrentRenderPass)
	{
		return;
	}

	FVulkanCmdBuffer* CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();
	if (CmdBuffer->IsInsideRenderPass())
	{
		TransitionState.EndRenderPass(CmdBuffer);
	}

	if (TransitionState.RenderingMipChainInfo.bInsideRenderingMipChain)
	{
		if (!Framebuffer)
		{
			UE_LOG(LogVulkanRHI, Fatal, TEXT("Unable to find framebuffer during mipchain generation: W,H:%d,%d CurrMip:%d LastMip:%d #Mips:%d VkViewType:%d PF_:%d"),
				RTLayout.GetExtent2D().width,
				RTLayout.GetExtent2D().height,
				TransitionState.RenderingMipChainInfo.CurrentMip,
				TransitionState.RenderingMipChainInfo.LastRenderedMip,
				TransitionState.RenderingMipChainInfo.Texture->Surface.GetNumMips(),
				(int32)TransitionState.RenderingMipChainInfo.Texture->Surface.GetViewType(),
				(int32)TransitionState.RenderingMipChainInfo.Texture->Surface.PixelFormat);
		}
		TransitionState.ProcessMipChainTransitions(CmdBuffer, Framebuffer, Framebuffer->RTInfo.ColorRenderTarget[0].MipIndex);
	}

	if (SafePointSubmit())
	{
		CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();
	}

	if (RenderPass != nullptr && Framebuffer != nullptr)
	{
		// Verify we are not setting the same render targets again
		if (RenderTargetsInfo.DepthStencilRenderTarget.Texture ||
			RenderTargetsInfo.NumColorRenderTargets > 1 ||
			((RenderTargetsInfo.NumColorRenderTargets == 1) && RenderTargetsInfo.ColorRenderTarget[0].Texture))
		{
			TransitionState.BeginRenderPass(*this, *Device, CmdBuffer, RenderTargetsInfo, RTLayout, RenderPass, Framebuffer);
		}
	}

	// Yuck - Bind pending pixel shader UAVs from SetRenderTargets
	{
		PendingPixelUAVs.Reset();
		for (uint32 UAVIndex = 0; UAVIndex < NumUAVs; ++UAVIndex)
		{
			FVulkanUnorderedAccessView* UAV = ResourceCast(UAVs[UAVIndex]);
			if (UAV)
			{
				PendingPixelUAVs.Add({UAV, UAVIndex});
			}
		}
	}
}

void FVulkanCommandListContext::RHISetRenderTargetsAndClear(const FRHISetRenderTargetsInfo& RenderTargetsInfo)
{
	//FRCLog::Printf(FString::Printf(TEXT("RHISetRenderTargetsAndClear")));
	FVulkanCmdBuffer* CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();
	if (CmdBuffer->IsInsideRenderPass())
	{
		TransitionState.EndRenderPass(CmdBuffer);
	}

	check(!TransitionState.RenderingMipChainInfo.bInsideRenderingMipChain);

	if (SafePointSubmit())
	{
		CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();
	}

	if (RenderTargetsInfo.DepthStencilRenderTarget.Texture ||
		RenderTargetsInfo.NumColorRenderTargets > 1 ||
		((RenderTargetsInfo.NumColorRenderTargets == 1) && RenderTargetsInfo.ColorRenderTarget[0].Texture))
	{
		FVulkanRenderTargetLayout RTLayout(RenderTargetsInfo);
		const uint32 RTLayoutHash = RTLayout.GetHash();
		FVulkanRenderPass* RenderPass = TransitionState.GetOrCreateRenderPass(*Device, RTLayout);
		FVulkanFramebuffer* Framebuffer = TransitionState.GetOrCreateFramebuffer(*Device, RenderTargetsInfo, RTLayout, RenderPass);

		TransitionState.BeginRenderPass(*this, *Device, CmdBuffer, RenderTargetsInfo, RTLayout, RenderPass, Framebuffer);
	}
	else
	{
		ensure(0);
	}
}

void FVulkanCommandListContext::RHICopyToResolveTarget(FTextureRHIParamRef SourceTextureRHI, FTextureRHIParamRef DestTextureRHI, bool bKeepOriginalSurface, const FResolveParams& InResolveParams)
{
	//FRCLog::Printf(FString::Printf(TEXT("RHICopyToResolveTarget")));
	if (!SourceTextureRHI || !DestTextureRHI)
	{
		// no need to do anything (silently ignored)
		return;
	}

	RHITransitionResources(EResourceTransitionAccess::EReadable, &SourceTextureRHI, 1);

	auto CopyImage = [](FTransitionState& InRenderPassState, FVulkanCmdBuffer* InCmdBuffer, FVulkanSurface& SrcSurface, FVulkanSurface& DstSurface, uint32 SrcNumLayers, uint32 DstNumLayers, const FResolveParams& ResolveParams)
	{
		VkImageLayout* SrcLayoutPtr = InRenderPassState.CurrentLayout.Find(SrcSurface.Image);
		check(SrcLayoutPtr);
		VkImageLayout SrcLayout = *SrcLayoutPtr;
		VkImageLayout* DstLayoutPtr = InRenderPassState.CurrentLayout.Find(DstSurface.Image);
		bool bIsDepth = (DstSurface.GetFullAspectMask() & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) != 0;
		VkImageLayout DstLayout = DstLayoutPtr ? *DstLayoutPtr : VK_IMAGE_LAYOUT_UNDEFINED;
		bool bCopyIntoCPUReadable = (DstSurface.UEFlags & TexCreate_CPUReadback) == TexCreate_CPUReadback;

		check(InCmdBuffer->IsOutsideRenderPass());
		VkCommandBuffer CmdBuffer = InCmdBuffer->GetHandle();

		VkImageSubresourceRange SrcRange;
		SrcRange.aspectMask = SrcSurface.GetFullAspectMask();
		SrcRange.baseMipLevel = ResolveParams.MipIndex;
		SrcRange.levelCount = 1;
		SrcRange.baseArrayLayer = ResolveParams.SourceArrayIndex * SrcNumLayers + (SrcNumLayers == 6 ? ResolveParams.CubeFace : 0);
		SrcRange.layerCount = 1;

		VkImageSubresourceRange DstRange;
		DstRange.aspectMask = DstSurface.GetFullAspectMask();
		DstRange.baseMipLevel = ResolveParams.MipIndex;
		DstRange.levelCount = 1;
		DstRange.baseArrayLayer = ResolveParams.DestArrayIndex * DstNumLayers + (DstNumLayers == 6 ? ResolveParams.CubeFace : 0);
		DstRange.layerCount = 1;

		VulkanSetImageLayout(CmdBuffer, SrcSurface.Image, SrcLayout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, SrcRange);
		VulkanSetImageLayout(CmdBuffer, DstSurface.Image, bCopyIntoCPUReadable ? VK_IMAGE_LAYOUT_UNDEFINED : DstLayout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, DstRange);

		VkImageCopy Region;
		FMemory::Memzero(Region);
		ensure(SrcSurface.Width == DstSurface.Width && SrcSurface.Height == DstSurface.Height);
		Region.extent.width = FMath::Max(1u, SrcSurface.Width>> ResolveParams.MipIndex);
		Region.extent.height = FMath::Max(1u, SrcSurface.Height >> ResolveParams.MipIndex);
		Region.extent.depth = 1;
		Region.srcSubresource.aspectMask = SrcSurface.GetFullAspectMask();
		Region.srcSubresource.baseArrayLayer = SrcRange.baseArrayLayer;
		Region.srcSubresource.layerCount = 1;
		Region.srcSubresource.mipLevel = ResolveParams.MipIndex;
		Region.dstSubresource.aspectMask = DstSurface.GetFullAspectMask();
		Region.dstSubresource.baseArrayLayer = DstRange.baseArrayLayer;
		Region.dstSubresource.layerCount = 1;
		Region.dstSubresource.mipLevel = ResolveParams.MipIndex;
		VulkanRHI::vkCmdCopyImage(CmdBuffer,
			SrcSurface.Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			DstSurface.Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1, &Region);

		VulkanSetImageLayout(CmdBuffer, SrcSurface.Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, SrcLayout, SrcRange);
		if (bCopyIntoCPUReadable)
		{
			VulkanSetImageLayout(CmdBuffer, DstSurface.Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, DstRange);
			if (DstLayoutPtr)
			{
				*DstLayoutPtr = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			}
			else
			{
				DstLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			}
		}
		else
		{
			DstLayout = bIsDepth ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			VulkanSetImageLayout(CmdBuffer, DstSurface.Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, DstLayout, DstRange);
		}

		if (DstLayoutPtr)
		{
			*DstLayoutPtr = DstLayout;
		}
		else
		{
			InRenderPassState.CurrentLayout.Add(DstSurface.Image, DstLayout);
		}
	};

	FRHITexture2D* SourceTexture2D = SourceTextureRHI->GetTexture2D();
	FRHITexture3D* SourceTexture3D = SourceTextureRHI->GetTexture3D();
	FRHITextureCube* SourceTextureCube = SourceTextureRHI->GetTextureCube();
	FRHITexture2D* DestTexture2D = DestTextureRHI->GetTexture2D();
	FRHITexture3D* DestTexture3D = DestTextureRHI->GetTexture3D();
	FRHITextureCube* DestTextureCube = DestTextureRHI->GetTextureCube();
	FVulkanCmdBuffer* CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();

	if (SourceTexture2D && DestTexture2D)
	{
		FVulkanTexture2D* VulkanSourceTexture = (FVulkanTexture2D*)SourceTexture2D;
		FVulkanTexture2D* VulkanDestTexture = (FVulkanTexture2D*)DestTexture2D;
		if (VulkanSourceTexture->Surface.Image != VulkanDestTexture->Surface.Image) 
		{
			CopyImage(TransitionState, CmdBuffer, VulkanSourceTexture->Surface, VulkanDestTexture->Surface, 1, 1, InResolveParams);
		}
	}
	else if (SourceTextureCube && DestTextureCube) 
	{
		FVulkanTextureCube* VulkanSourceTexture = (FVulkanTextureCube*)SourceTextureCube;
		FVulkanTextureCube* VulkanDestTexture = (FVulkanTextureCube*)DestTextureCube;
		if (VulkanSourceTexture->Surface.Image != VulkanDestTexture->Surface.Image) 
		{
			CopyImage(TransitionState, CmdBuffer, VulkanSourceTexture->Surface, VulkanDestTexture->Surface, 6, 6, InResolveParams);
		}
	}
	else if (SourceTexture2D && DestTextureCube) 
	{
		FVulkanTexture2D* VulkanSourceTexture = (FVulkanTexture2D*)SourceTexture2D;
		FVulkanTextureCube* VulkanDestTexture = (FVulkanTextureCube*)DestTextureCube;
		if (VulkanSourceTexture->Surface.Image != VulkanDestTexture->Surface.Image) 
		{
			CopyImage(TransitionState, CmdBuffer, VulkanSourceTexture->Surface, VulkanDestTexture->Surface, 1, 6, InResolveParams);
		}
	}
	else if (SourceTexture3D && DestTexture3D) 
	{
		FVulkanTexture3D* VulkanSourceTexture = (FVulkanTexture3D*)SourceTexture3D;
		FVulkanTexture3D* VulkanDestTexture = (FVulkanTexture3D*)DestTexture3D;
		if (VulkanSourceTexture->Surface.Image != VulkanDestTexture->Surface.Image)
		{
			CopyImage(TransitionState, CmdBuffer, VulkanSourceTexture->Surface, VulkanDestTexture->Surface, 1, 1, InResolveParams);
		}
	}
	else 
	{
		checkf(false, TEXT("Using unsupported Resolve combination"));
	}
}

void FVulkanDynamicRHI::RHIReadSurfaceData(FTextureRHIParamRef TextureRHI, FIntRect Rect, TArray<FColor>& OutData, FReadSurfaceDataFlags InFlags)
{
	FRHITexture2D* TextureRHI2D = TextureRHI->GetTexture2D();
	check(TextureRHI2D);
	FVulkanTexture2D* Texture2D = (FVulkanTexture2D*)TextureRHI2D;
	uint32 NumPixels = TextureRHI2D->GetSizeX() * TextureRHI2D->GetSizeY();

	if (GIgnoreCPUReads == 2)
	{
		OutData.Empty(0);
		OutData.AddZeroed(NumPixels);
		return;
	}

	Device->PrepareForCPURead();

	FVulkanCommandListContext& ImmediateContext = Device->GetImmediateContext();

	FVulkanCmdBuffer* CmdBuffer = ImmediateContext.GetCommandBufferManager()->GetUploadCmdBuffer();

	ensure(Texture2D->Surface.StorageFormat == VK_FORMAT_R8G8B8A8_UNORM || Texture2D->Surface.StorageFormat == VK_FORMAT_B8G8R8A8_UNORM || Texture2D->Surface.StorageFormat == VK_FORMAT_R16G16B16A16_SFLOAT || Texture2D->Surface.StorageFormat == VK_FORMAT_A2B10G10R10_UNORM_PACK32);
	const uint32 Size = NumPixels * sizeof(FColor);
	VulkanRHI::FStagingBuffer* StagingBuffer = Device->GetStagingManager().AcquireBuffer(Size, VK_BUFFER_USAGE_TRANSFER_DST_BIT, true);
	if (GIgnoreCPUReads == 0)
	{
		VkBufferImageCopy CopyRegion;
		FMemory::Memzero(CopyRegion);
		//Region.bufferOffset = 0;
		CopyRegion.bufferRowLength = TextureRHI2D->GetSizeX();
		CopyRegion.bufferImageHeight = TextureRHI2D->GetSizeY();
		CopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		//Region.imageSubresource.mipLevel = 0;
		//Region.imageSubresource.baseArrayLayer = 0;
		CopyRegion.imageSubresource.layerCount = 1;
		CopyRegion.imageExtent.width = TextureRHI2D->GetSizeX();
		CopyRegion.imageExtent.height = TextureRHI2D->GetSizeY();
		CopyRegion.imageExtent.depth = 1;

		//#todo-rco: Multithreaded!
		const VkImageLayout* CurrentLayout = Device->GetImmediateContext().TransitionState.CurrentLayout.Find(Texture2D->Surface.Image);
		VulkanSetImageLayoutSimple(CmdBuffer->GetHandle(), Texture2D->Surface.Image, CurrentLayout ? *CurrentLayout : VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

		VulkanRHI::vkCmdCopyImageToBuffer(CmdBuffer->GetHandle(), Texture2D->Surface.Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, StagingBuffer->GetHandle(), 1, &CopyRegion);
		if (CurrentLayout)
		{
			VulkanSetImageLayoutSimple(CmdBuffer->GetHandle(), Texture2D->Surface.Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, *CurrentLayout);
		}
		else
		{
			Device->GetImmediateContext().TransitionState.CurrentLayout.Add(Texture2D->Surface.Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
		}
	}
	else
	{
		VulkanRHI::vkCmdFillBuffer(CmdBuffer->GetHandle(), StagingBuffer->GetHandle(), 0, Size, (uint32)0xffffffff);
	}

	VkBufferMemoryBarrier Barrier;
	ensure(StagingBuffer->GetSize() >= Size);
		//#todo-rco: Change offset if reusing a buffer suballocation
	VulkanRHI::SetupAndZeroBufferBarrier(Barrier, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT, StagingBuffer->GetHandle(), 0/*StagingBuffer->GetOffset()*/, Size);
	VulkanRHI::vkCmdPipelineBarrier(CmdBuffer->GetHandle(), VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 0, nullptr, 1, &Barrier, 0, nullptr);

	// Force upload
	ImmediateContext.GetCommandBufferManager()->SubmitUploadCmdBuffer(true);
	Device->WaitUntilIdle();

	VkMappedMemoryRange MappedRange;
	FMemory::Memzero(MappedRange);
	MappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
	MappedRange.memory = StagingBuffer->GetDeviceMemoryHandle();
	MappedRange.offset = StagingBuffer->GetAllocationOffset();
	MappedRange.size = Size;
	VulkanRHI::vkInvalidateMappedMemoryRanges(Device->GetInstanceHandle(), 1, &MappedRange);

	OutData.SetNum(NumPixels);
	FColor* Dest = OutData.GetData();

	if (Texture2D->Surface.StorageFormat == VK_FORMAT_R16G16B16A16_SFLOAT)
	{
		for (int32 Row = Rect.Min.Y; Row < Rect.Max.Y; ++Row)
		{
			FFloat16Color* Src = (FFloat16Color*)StagingBuffer->GetMappedPointer() + Row * TextureRHI2D->GetSizeX() + Rect.Min.X;
			for (int32 Col = Rect.Min.X; Col < Rect.Max.X; ++Col)
			{
				Dest->R = (uint8)(uint32)FMath::Clamp<int32>((int32)(Src->B.GetFloat() * 255.0f), 0, 255);
				Dest->G = (uint8)(uint32)FMath::Clamp<int32>((int32)(Src->G.GetFloat() * 255.0f), 0, 255);
				Dest->B = (uint8)(uint32)FMath::Clamp<int32>((int32)(Src->R.GetFloat() * 255.0f), 0, 255);
				Dest->A = (uint8)(uint32)FMath::Clamp<int32>((int32)(Src->A.GetFloat() * 255.0f), 0, 255);
				Dest++;
				Src++;
			}
		}
	}
	else if (Texture2D->Surface.StorageFormat == VK_FORMAT_A2B10G10R10_UNORM_PACK32)
	{
		struct FR10G10B10A2
		{
			uint32 R : 10;
			uint32 G : 10;
			uint32 B : 10;
			uint32 A : 2;
		};
		for (int32 Row = Rect.Min.Y; Row < Rect.Max.Y; ++Row)
		{
			FR10G10B10A2* Src = (FR10G10B10A2*)StagingBuffer->GetMappedPointer() + Row * TextureRHI2D->GetSizeX() + Rect.Min.X;
			for (int32 Col = Rect.Min.X; Col < Rect.Max.X; ++Col)
			{
				*Dest = FLinearColor(
					(float)Src->R / 1023.0f,
					(float)Src->G / 1023.0f,
					(float)Src->B / 1023.0f,
					(float)Src->A / 3.0f
				).Quantize();
				++Dest;
				++Src;
			}
		}
	}
	else if (Texture2D->Surface.StorageFormat == VK_FORMAT_R8G8B8A8_UNORM)
	{
		for (int32 Row = Rect.Min.Y; Row < Rect.Max.Y; ++Row)
		{
			FColor* Src = (FColor*)StagingBuffer->GetMappedPointer() + Row * TextureRHI2D->GetSizeX() + Rect.Min.X;
			for (int32 Col = Rect.Min.X; Col < Rect.Max.X; ++Col)
			{
				Dest->R = Src->B;
				Dest->G = Src->G;
				Dest->B = Src->R;
				Dest->A = Src->A;
				Dest++;
				Src++;
			}
		}
	}
	else if (Texture2D->Surface.StorageFormat == VK_FORMAT_B8G8R8A8_UNORM)
	{
		FColor* Src = (FColor*)StagingBuffer->GetMappedPointer() + Rect.Min.Y * TextureRHI2D->GetSizeX() + Rect.Min.X;
		for (int32 Row = Rect.Min.Y; Row < Rect.Max.Y; ++Row)
		{
			const int32 NumCols = Rect.Max.X - Rect.Min.X;
			FMemory::Memcpy(Dest, Src, NumCols * sizeof(FColor));
			Src += TextureRHI2D->GetSizeX();
			Dest += NumCols;
		}
	}

	Device->GetStagingManager().ReleaseBuffer(CmdBuffer, StagingBuffer);
	ImmediateContext.GetCommandBufferManager()->PrepareForNewActiveCommandBuffer();
}

void FVulkanDynamicRHI::RHIMapStagingSurface(FTextureRHIParamRef TextureRHI,void*& OutData,int32& OutWidth,int32& OutHeight)
{
	FRHITexture2D* TextureRHI2D = TextureRHI->GetTexture2D();
	check(TextureRHI2D);
	FVulkanTexture2D* Texture2D = (FVulkanTexture2D*)TextureRHI2D;
	check(Texture2D->GetFlags() & TexCreate_CPUReadback);

	FDeviceMemoryAllocation* Allocation = Texture2D->Surface.GetAllocation();
	check(Allocation->CanBeMapped());

	if (Allocation->IsMapped()) // allocation already mapped
	{
		OutData = Allocation->GetMappedPointer();
	}
	else
	{
		Device->PrepareForCPURead(); //make sure the results are ready 
		Device->GetImmediateContext().GetCommandBufferManager()->PrepareForNewActiveCommandBuffer();

		OutData = Allocation->Map(Allocation->GetSize(), 0);
	}
	OutWidth = Texture2D->GetSizeX();
	OutHeight = Texture2D->GetSizeY();
}

void FVulkanDynamicRHI::RHIUnmapStagingSurface(FTextureRHIParamRef TextureRHI)
{
	FRHITexture2D* TextureRHI2D = TextureRHI->GetTexture2D();
	check(TextureRHI2D);
	FVulkanTexture2D* Texture2D = (FVulkanTexture2D*)TextureRHI2D;

	FDeviceMemoryAllocation* Allocation = Texture2D->Surface.GetAllocation();
	if (Allocation->IsMapped()) //only when actually mapped
	{
		Allocation->Unmap();
	}
}

void FVulkanDynamicRHI::RHIReadSurfaceFloatData(FTextureRHIParamRef TextureRHI, FIntRect Rect, TArray<FFloat16Color>& OutData, ECubeFace CubeFace,int32 ArrayIndex,int32 MipIndex)
{
	auto DoCopyFloat = [](FVulkanDevice* InDevice, FVulkanCmdBuffer* InCmdBuffer, const FVulkanSurface& Surface, uint32 InMipIndex, uint32 SrcBaseArrayLayer, FIntRect InRect, TArray<FFloat16Color>& OutputData)
	{
		ensure(Surface.StorageFormat == VK_FORMAT_R16G16B16A16_SFLOAT);

		uint32 NumPixels = (Surface.Width >> InMipIndex) * (Surface.Height >> InMipIndex);
		const uint32 Size = NumPixels * sizeof(FFloat16Color);
		VulkanRHI::FStagingBuffer* StagingBuffer = InDevice->GetStagingManager().AcquireBuffer(Size, VK_BUFFER_USAGE_TRANSFER_DST_BIT, true);

		if (GIgnoreCPUReads == 0)
		{
			VkBufferImageCopy CopyRegion;
			FMemory::Memzero(CopyRegion);
			//Region.bufferOffset = 0;
			CopyRegion.bufferRowLength = Surface.Width >> InMipIndex;
			CopyRegion.bufferImageHeight = Surface.Height >> InMipIndex;
			CopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			CopyRegion.imageSubresource.mipLevel = InMipIndex;
			CopyRegion.imageSubresource.baseArrayLayer = SrcBaseArrayLayer;
			CopyRegion.imageSubresource.layerCount = 1;
			CopyRegion.imageExtent.width = Surface.Width >> InMipIndex;
			CopyRegion.imageExtent.height = Surface.Height >> InMipIndex;
			CopyRegion.imageExtent.depth = 1;

			//#todo-rco: Multithreaded!
			const VkImageLayout* CurrentLayout = InDevice->GetImmediateContext().TransitionState.CurrentLayout.Find(Surface.Image);
			VulkanSetImageLayoutSimple(InCmdBuffer->GetHandle(), Surface.Image, CurrentLayout ? *CurrentLayout : VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

			VulkanRHI::vkCmdCopyImageToBuffer(InCmdBuffer->GetHandle(), Surface.Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, StagingBuffer->GetHandle(), 1, &CopyRegion);

			if (CurrentLayout)
			{
				VulkanSetImageLayoutSimple(InCmdBuffer->GetHandle(), Surface.Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, *CurrentLayout);
			}
			else
			{
				InDevice->GetImmediateContext().TransitionState.CurrentLayout.Add(Surface.Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
			}
		}
		else
		{
			VulkanRHI::vkCmdFillBuffer(InCmdBuffer->GetHandle(), StagingBuffer->GetHandle(), 0, Size, (FFloat16(1.0).Encoded << 16) + FFloat16(1.0).Encoded);
		}

		VkBufferMemoryBarrier Barrier;
		// the staging buffer size may be bigger then the size due to alignment, etc. but it must not be smaller!
		ensure(StagingBuffer->GetSize() >= Size);
		//#todo-rco: Change offset if reusing a buffer suballocation
		VulkanRHI::SetupAndZeroBufferBarrier(Barrier, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, StagingBuffer->GetHandle(), 0/*StagingBuffer->GetOffset()*/, StagingBuffer->GetSize());
		VulkanRHI::vkCmdPipelineBarrier(InCmdBuffer->GetHandle(), VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 0, nullptr, 1, &Barrier, 0, nullptr);

		// Force upload
		InDevice->GetImmediateContext().GetCommandBufferManager()->SubmitUploadCmdBuffer(true);
		InDevice->WaitUntilIdle();

		VkMappedMemoryRange MappedRange;
		FMemory::Memzero(MappedRange);
		MappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
		MappedRange.memory = StagingBuffer->GetDeviceMemoryHandle();
		MappedRange.offset = StagingBuffer->GetAllocationOffset();
		MappedRange.size = Size;
		VulkanRHI::vkInvalidateMappedMemoryRanges(InDevice->GetInstanceHandle(), 1, &MappedRange);

		OutputData.SetNum(NumPixels);
		FFloat16Color* Dest = OutputData.GetData();
		for (int32 Row = InRect.Min.Y; Row < InRect.Max.Y; ++Row)
		{
			FFloat16Color* Src = (FFloat16Color*)StagingBuffer->GetMappedPointer() + Row * (Surface.Width >> InMipIndex) + InRect.Min.X;
			for (int32 Col = InRect.Min.X; Col < InRect.Max.X; ++Col)
			{
				*Dest++ = *Src++;
			}
		}
		InDevice->GetStagingManager().ReleaseBuffer(InCmdBuffer, StagingBuffer);
	};

	if (GIgnoreCPUReads == 2)
	{
		// FIll with CPU
		uint32 NumPixels = 0;
		if (TextureRHI->GetTextureCube())
		{
			FRHITextureCube* TextureRHICube = TextureRHI->GetTextureCube();
			FVulkanTextureCube* TextureCube = (FVulkanTextureCube*)TextureRHICube;
			NumPixels = (TextureCube->Surface.Width >> MipIndex) * (TextureCube->Surface.Height >> MipIndex);
		}
		else
		{
			FRHITexture2D* TextureRHI2D = TextureRHI->GetTexture2D();
			check(TextureRHI2D);
			FVulkanTexture2D* Texture2D = (FVulkanTexture2D*)TextureRHI2D;
			NumPixels = (Texture2D->Surface.Width >> MipIndex) * (Texture2D->Surface.Height >> MipIndex);
		}

		OutData.Empty(0);
		OutData.AddZeroed(NumPixels);
	}
	else
	{
		Device->PrepareForCPURead();

		FVulkanCmdBuffer* CmdBuffer = Device->GetImmediateContext().GetCommandBufferManager()->GetUploadCmdBuffer();
		if (TextureRHI->GetTextureCube())
		{
			FRHITextureCube* TextureRHICube = TextureRHI->GetTextureCube();
			FVulkanTextureCube* TextureCube = (FVulkanTextureCube*)TextureRHICube;
			DoCopyFloat(Device, CmdBuffer, TextureCube->Surface, MipIndex, CubeFace, Rect, OutData);
		}
		else
		{
			FRHITexture2D* TextureRHI2D = TextureRHI->GetTexture2D();
			check(TextureRHI2D);
			FVulkanTexture2D* Texture2D = (FVulkanTexture2D*)TextureRHI2D;
			DoCopyFloat(Device, CmdBuffer, Texture2D->Surface, MipIndex, 0, Rect, OutData);
		}
		Device->GetImmediateContext().GetCommandBufferManager()->PrepareForNewActiveCommandBuffer();
	}
}

void FVulkanDynamicRHI::RHIRead3DSurfaceFloatData(FTextureRHIParamRef TextureRHI,FIntRect InRect,FIntPoint ZMinMax,TArray<FFloat16Color>& OutData)
{
	Device->PrepareForCPURead();

	VULKAN_SIGNAL_UNIMPLEMENTED();

	Device->GetImmediateContext().GetCommandBufferManager()->PrepareForNewActiveCommandBuffer();

}

void FVulkanCommandListContext::RHITransitionResources(EResourceTransitionAccess TransitionType, EResourceTransitionPipeline TransitionPipeline, FUnorderedAccessViewRHIParamRef* InUAVs, int32 NumUAVs, FComputeFenceRHIParamRef WriteComputeFenceRHI)
{
	FVulkanCmdBuffer* CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();
	TArray<VkBufferMemoryBarrier> BufferBarriers;
	TArray<VkImageMemoryBarrier> ImageBarriers;
	for (int32 Index = 0; Index < NumUAVs; ++Index)
	{
		FVulkanUnorderedAccessView* UAV = ResourceCast(InUAVs[Index]);

		VkAccessFlags SrcAccess = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, DestAccess = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
		switch (TransitionType)
		{
		case EResourceTransitionAccess::EWritable:
			SrcAccess = VK_ACCESS_SHADER_READ_BIT;
			DestAccess = VK_ACCESS_SHADER_WRITE_BIT;
			break;
		case EResourceTransitionAccess::EReadable:
			SrcAccess = VK_ACCESS_SHADER_WRITE_BIT;
			DestAccess = VK_ACCESS_SHADER_READ_BIT;
			break;
		case EResourceTransitionAccess::ERWBarrier:
			SrcAccess = VK_ACCESS_SHADER_READ_BIT;
			DestAccess = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
			break;
		case EResourceTransitionAccess::ERWNoBarrier:
			//#todo-rco: Skip for now
			continue;
		default:
			ensure(0);
			break;
		}

		if (!UAV)
		{
			continue;
		}

		if (UAV->SourceVertexBuffer)
		{
			VkBufferMemoryBarrier& Barrier = BufferBarriers[BufferBarriers.AddUninitialized()];
			VulkanRHI::SetupAndZeroBufferBarrier(Barrier, SrcAccess, DestAccess, UAV->SourceVertexBuffer->GetHandle(), UAV->SourceVertexBuffer->GetOffset(), UAV->SourceVertexBuffer->GetSize());
		}
		else if (UAV->SourceTexture)
		{
			VkImageMemoryBarrier& Barrier = ImageBarriers[ImageBarriers.AddUninitialized()];
			FVulkanTextureBase* VulkanTexture = FVulkanTextureBase::Cast(UAV->SourceTexture);
			VkImageLayout Layout = TransitionState.FindOrAddLayout(VulkanTexture->Surface.Image, VK_IMAGE_LAYOUT_GENERAL);
			VulkanRHI::SetupAndZeroImageBarrierOLD(Barrier, VulkanTexture->Surface, SrcAccess, Layout, DestAccess, Layout);
		}
		else if (UAV->SourceStructuredBuffer)
		{
			VkBufferMemoryBarrier& Barrier = BufferBarriers[BufferBarriers.AddUninitialized()];
			VulkanRHI::SetupAndZeroBufferBarrier(Barrier, SrcAccess, DestAccess, UAV->SourceStructuredBuffer->GetHandle(), UAV->SourceStructuredBuffer->GetOffset(), UAV->SourceStructuredBuffer->GetSize());
		}
		else if (UAV->SourceIndexBuffer)
		{
			VkBufferMemoryBarrier& Barrier = BufferBarriers[BufferBarriers.AddUninitialized()];
			VulkanRHI::SetupAndZeroBufferBarrier(Barrier, SrcAccess, DestAccess, UAV->SourceIndexBuffer->GetHandle(), UAV->SourceIndexBuffer->GetOffset(), UAV->SourceIndexBuffer->GetSize());
		}
		else
		{
			ensure(0);
		}
	}

	VkPipelineStageFlagBits SourceStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, DestStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
	switch (TransitionPipeline)
	{
	case EResourceTransitionPipeline::EGfxToCompute:
		SourceStage = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
		DestStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		break;
	case EResourceTransitionPipeline::EComputeToGfx:
		SourceStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		DestStage = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
		break;
	case EResourceTransitionPipeline::EComputeToCompute:
		SourceStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		DestStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		break;
	default:
		ensure(0);
		break;
	}

	if (BufferBarriers.Num() && TransitionState.CurrentRenderPass != nullptr)
	{
		TransitionState.EndRenderPass(CmdBuffer);
	}

	VulkanRHI::vkCmdPipelineBarrier(CmdBuffer->GetHandle(), SourceStage, DestStage, 0, 0, nullptr, BufferBarriers.Num(), BufferBarriers.GetData(), ImageBarriers.Num(), ImageBarriers.GetData());

	if (WriteComputeFenceRHI)
	{
		FVulkanComputeFence* Fence = ResourceCast(WriteComputeFenceRHI);
		Fence->WriteCmd(CmdBuffer->GetHandle());
	}
}

void FVulkanCommandListContext::RHITransitionResources(EResourceTransitionAccess TransitionType, FTextureRHIParamRef* InTextures, int32 NumTextures)
{
	static IConsoleVariable* CVarShowTransitions = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ProfileGPU.ShowTransitions"));
	bool bShowTransitionEvents = CVarShowTransitions->GetInt() != 0;

	//FRCLog::Printf(FString::Printf(TEXT("RHITransitionResources")));
	if (NumTextures == 0)
	{
		return;
	}

	SCOPED_RHI_CONDITIONAL_DRAW_EVENTF(*this, RHITransitionResources, bShowTransitionEvents, TEXT("TransitionTo: %s: %i Textures"), *FResourceTransitionUtility::ResourceTransitionAccessStrings[(int32)TransitionType], NumTextures);

	FVulkanCmdBuffer* CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();
	check(CmdBuffer->HasBegun());

	if (TransitionType == EResourceTransitionAccess::EReadable)
	{
		if (TransitionState.CurrentRenderPass)
		{
			// If any of the textures are in the current render pass, we need to end it
			uint32 TexturesInsideRenderPass = 0;
			for (int32 Index = 0; Index < NumTextures; ++Index)
			{
				FVulkanTextureBase* VulkanTexture = FVulkanTextureBase::Cast(InTextures[Index]);
				if (TransitionState.CurrentFramebuffer->ContainsRenderTarget(InTextures[Index]))
				{
					++TexturesInsideRenderPass;
					bool bIsDepthStencil = (VulkanTexture->Surface.GetFullAspectMask() & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) != 0;
					VkImageLayout* Found = TransitionState.CurrentLayout.Find(VulkanTexture->Surface.Image);
					VkImageLayout EnsureLayout = (bIsDepthStencil ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
					// Static Analysis DOESN'T WORK WITH ensure((Found != nullptr) && (*Found == EnsureLayout));
					if (Found)
					{
						VkImageLayout FoundLayout = *Found;
						ensure(FoundLayout == EnsureLayout);
					}
					else
					{
						ensure(Found);
					}
				}
				else
				{
					//ensureMsgf(0, TEXT("Unable to transition texture as we're inside a render pass!"));
				}
			}

			if (TexturesInsideRenderPass > 0)
			{
				TransitionState.EndRenderPass(CmdBuffer);
			}
		}

		for (int32 Index = 0; Index < NumTextures; ++Index)
		{
			SCOPED_RHI_CONDITIONAL_DRAW_EVENTF(*this, RHITransitionResourcesLoop, bShowTransitionEvents, TEXT("To:%i - %s"), Index, *InTextures[Index]->GetName().ToString());

			FVulkanTextureBase* VulkanTexture = FVulkanTextureBase::Cast(InTextures[Index]);
			VkImageLayout SrcLayout = TransitionState.FindOrAddLayout(VulkanTexture->Surface.Image, VK_IMAGE_LAYOUT_UNDEFINED);
			ensure(SrcLayout != VK_IMAGE_LAYOUT_UNDEFINED);
			bool bIsDepthStencil = (VulkanTexture->Surface.GetFullAspectMask() & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) != 0;
			VkImageLayout DstLayout = bIsDepthStencil ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			VulkanSetImageLayout(CmdBuffer->GetHandle(), VulkanTexture->Surface.Image, SrcLayout, DstLayout, VulkanRHI::SetupImageSubresourceRange(VulkanTexture->Surface.GetFullAspectMask()));
			TransitionState.CurrentLayout[VulkanTexture->Surface.Image] = DstLayout;
		}
	}
	else if (TransitionType == EResourceTransitionAccess::EWritable)
	{
		//#todo-rco: Until render passes come online, assume writable means end render pass
		if (TransitionState.CurrentRenderPass)
		{
			TransitionState.EndRenderPass(CmdBuffer);
		}

		auto SetImageLayout = [](FTransitionState& InRenderPassState, VkCommandBuffer InCmdBuffer, FVulkanSurface& Surface, uint32 NumArraySlices)
		{
			const VkImageAspectFlags AspectMask = Surface.GetFullAspectMask();
			VkImageSubresourceRange SubresourceRange = { AspectMask, 0, Surface.GetNumMips(), 0, NumArraySlices};

			VkImageLayout SrcLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			VkImageLayout* FoundLayout = InRenderPassState.CurrentLayout.Find(Surface.Image);
			if (FoundLayout)
			{
				SrcLayout = *FoundLayout;
			}

			if ((AspectMask & VK_IMAGE_ASPECT_COLOR_BIT) != 0)
			{
				if (SrcLayout == VK_IMAGE_LAYOUT_UNDEFINED)
				{
					VulkanRHI::ImagePipelineBarrier(InCmdBuffer, Surface.Image, EImageLayoutBarrier::Undefined, EImageLayoutBarrier::ColorAttachment, SubresourceRange);
				}
				else
				{
					VulkanSetImageLayout(InCmdBuffer, Surface.Image, SrcLayout, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, SubresourceRange);
				}
				InRenderPassState.CurrentLayout.FindOrAdd(Surface.Image) = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			}
			else
			{
				check((AspectMask & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) != 0);
				VulkanSetImageLayout(InCmdBuffer, Surface.Image, SrcLayout, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, SubresourceRange);
				InRenderPassState.CurrentLayout.FindOrAdd(Surface.Image) = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			}
		};

		for (int32 i = 0; i < NumTextures; ++i)
		{
			FRHITexture* RHITexture = InTextures[i];
			if (!RHITexture)
			{
				continue;
			}

			SCOPED_RHI_CONDITIONAL_DRAW_EVENTF(*this, RHITransitionResourcesLoop, bShowTransitionEvents, TEXT("To:%i - %s"), i, *InTextures[i]->GetName().ToString());

			FRHITextureCube* RHITextureCube = RHITexture->GetTextureCube();
			if (RHITextureCube)
			{
				FVulkanTextureCube* TextureCube = (FVulkanTextureCube*)RHITextureCube;
				SetImageLayout(TransitionState, CmdBuffer->GetHandle(), TextureCube->Surface, 6);
			}
			else
			{
				FVulkanTextureBase* Base = GetVulkanTextureFromRHITexture(RHITexture);
				SetImageLayout(TransitionState, CmdBuffer->GetHandle(), Base->Surface, 1);
			}
		}
	}
	else if (TransitionType == EResourceTransitionAccess::ERWSubResBarrier)
	{
		// This mode is only used for generating mipmaps only
		check(NumTextures == 1);
		FVulkanTextureBase* VulkanTexture = FVulkanTextureBase::Cast(InTextures[0]);

		check(CmdBuffer->IsInsideRenderPass());

		if (TransitionState.CurrentRenderPass)
		{
			if (TransitionState.RenderingMipChainInfo.bInsideRenderingMipChain)
			{
				// Verify we are transitioning the same texture
				check(TransitionState.RenderingMipChainInfo.Texture == VulkanTexture);
				TransitionState.EndRenderPass(CmdBuffer);
				if (TransitionState.RenderingMipChainInfo.CurrentMip == VulkanTexture->Surface.GetNumMips() - 1)
				{
					VkImageSubresourceRange Range = VulkanRHI::SetupImageSubresourceRange(VulkanTexture->Surface.GetFullAspectMask(), TransitionState.RenderingMipChainInfo.CurrentMip);
					VulkanRHI::ImagePipelineBarrier(CmdBuffer->GetHandle(), VulkanTexture->Surface.Image, VulkanRHI::EImageLayoutBarrier::ColorAttachment, VulkanRHI::EImageLayoutBarrier::PixelShaderRead, Range);
					TransitionState.FindOrAddLayout(VulkanTexture->Surface.Image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

					TransitionState.RenderingMipChainInfo.bInsideRenderingMipChain = false;
					TransitionState.RenderingMipChainInfo.CurrentMip = 0;
					TransitionState.RenderingMipChainInfo.LastRenderedMip = 0;
				}
			}
			else
			{
				// First time through; verify the texture is in the current render pass and end it
				ensure(TransitionState.CurrentFramebuffer->ContainsRenderTarget(InTextures[0]));
				TransitionState.RenderingMipChainInfo.Texture = VulkanTexture;
				//#todo-rco: Is this a correct assumption?
				TransitionState.RenderingMipChainInfo.LastRenderedMip = 0;
				TransitionState.RenderingMipChainInfo.CurrentMip = 0;
				TransitionState.RenderingMipChainInfo.bInsideRenderingMipChain = true;
				TransitionState.EndRenderPass(CmdBuffer);
			}
		}
		else
		{
			ensureMsgf(0, TEXT("Invalid call to transition ERWSubResBarrier!"));
		}
	}
	else if (TransitionType == EResourceTransitionAccess::EMetaData)
	{
		// Nothing to do here
	}

	if (CommandBufferManager->GetActiveCmdBuffer()->IsOutsideRenderPass())
	{
		if (SafePointSubmit())
		{
			CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();
		}
	}
}

// Need a separate struct so we can memzero/remove dependencies on reference counts
struct FRenderPassHashableStruct
{
	uint8						NumAttachments;
	uint8						NumSamples;

	VkFormat					Formats[MaxSimultaneousRenderTargets + 1];
	ERenderTargetLoadAction		LoadActions[MaxSimultaneousRenderTargets];
	ERenderTargetStoreAction	StoreActions[MaxSimultaneousRenderTargets];
	ERenderTargetLoadAction		DepthLoad;
	ERenderTargetStoreAction	DepthStore;
	ERenderTargetLoadAction		StencilLoad;
	ERenderTargetStoreAction	StencilStore;
};


FVulkanRenderTargetLayout::FVulkanRenderTargetLayout(const FRHISetRenderTargetsInfo& RTInfo)
	: NumAttachmentDescriptions(0)
	, NumColorAttachments(0)
	, bHasDepthStencil(false)
	, bHasResolveAttachments(false)
	, NumSamples(0)
	, NumUsedClearValues(0)
	, OldHash(0)
	, RenderPassHash(0)
{
	FMemory::Memzero(ColorReferences);
	FMemory::Memzero(ResolveReferences);
	FMemory::Memzero(DepthStencilReference);
	FMemory::Memzero(Desc);
	FMemory::Memzero(Extent);

	bool bSetExtent = false;
	int32 StartClearEntry = -1;
	for (int32 Index = 0; Index < RTInfo.NumColorRenderTargets; ++Index)
	{
		const FRHIRenderTargetView& RTView = RTInfo.ColorRenderTarget[Index];
		if (RTView.Texture)
		{
			FVulkanTextureBase* Texture = FVulkanTextureBase::Cast(RTView.Texture);
			check(Texture);
	
			if (bSetExtent)
			{
				ensure(Extent.Extent3D.width == FMath::Max(1u, Texture->Surface.Width >> RTView.MipIndex));
				ensure(Extent.Extent3D.height == FMath::Max(1u, Texture->Surface.Height >> RTView.MipIndex));
				ensure(Extent.Extent3D.depth == Texture->Surface.Depth);
			}
			else
			{
				bSetExtent = true;
				Extent.Extent3D.width = FMath::Max(1u, Texture->Surface.Width >> RTView.MipIndex);
				Extent.Extent3D.height = FMath::Max(1u, Texture->Surface.Height >> RTView.MipIndex);
				Extent.Extent3D.depth = Texture->Surface.Depth;
			}

			ensure(!NumSamples || NumSamples == RTView.Texture->GetNumSamples());
			NumSamples = RTView.Texture->GetNumSamples();
		
			VkAttachmentDescription& CurrDesc = Desc[NumAttachmentDescriptions];
			
			//@TODO: Check this, it should be a power-of-two. Might be a VulkanConvert helper function.
			CurrDesc.samples = static_cast<VkSampleCountFlagBits>(NumSamples);
			CurrDesc.format = UEToVkFormat(RTView.Texture->GetFormat(), (Texture->Surface.UEFlags & TexCreate_SRGB) == TexCreate_SRGB);
			CurrDesc.loadOp = RenderTargetLoadActionToVulkan(RTView.LoadAction);
			if (CurrDesc.loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR)
			{
				if (StartClearEntry == -1)
				{
					StartClearEntry = NumAttachmentDescriptions;
					NumUsedClearValues = StartClearEntry + 1;
				}
				else
				{
					NumUsedClearValues = NumAttachmentDescriptions + 1;
				}
			}
			CurrDesc.storeOp = RenderTargetStoreActionToVulkan(RTView.StoreAction);
			CurrDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			CurrDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			CurrDesc.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			CurrDesc.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			ColorReferences[NumColorAttachments].attachment = NumAttachmentDescriptions;
			ColorReferences[NumColorAttachments].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			if (CurrDesc.samples > VK_SAMPLE_COUNT_1_BIT)
			{
				Desc[NumAttachmentDescriptions + 1] = Desc[NumAttachmentDescriptions];
				Desc[NumAttachmentDescriptions + 1].samples = VK_SAMPLE_COUNT_1_BIT;
				ResolveReferences[NumColorAttachments].attachment = NumAttachmentDescriptions + 1;
				ResolveReferences[NumColorAttachments].layout = VK_IMAGE_LAYOUT_GENERAL;
				++NumAttachmentDescriptions;
				bHasResolveAttachments = true;
			}

			++NumAttachmentDescriptions;
			NumColorAttachments++;
		}
	}

	if (RTInfo.DepthStencilRenderTarget.Texture)
	{
		VkAttachmentDescription& CurrDesc = Desc[NumAttachmentDescriptions];
		FMemory::Memzero(CurrDesc);
		FVulkanTextureBase* Texture = FVulkanTextureBase::Cast(RTInfo.DepthStencilRenderTarget.Texture);
		check(Texture);

		//@TODO: Check this, it should be a power-of-two. Might be a VulkanConvert helper function.
		CurrDesc.samples = static_cast<VkSampleCountFlagBits>(RTInfo.DepthStencilRenderTarget.Texture->GetNumSamples());
		ensure(!NumSamples || CurrDesc.samples == NumSamples);
		NumSamples = CurrDesc.samples;
		CurrDesc.format = UEToVkFormat(RTInfo.DepthStencilRenderTarget.Texture->GetFormat(), false);
		CurrDesc.loadOp = RenderTargetLoadActionToVulkan(RTInfo.DepthStencilRenderTarget.DepthLoadAction);
		CurrDesc.stencilLoadOp = RenderTargetLoadActionToVulkan(RTInfo.DepthStencilRenderTarget.StencilLoadAction);
		if (CurrDesc.loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR || CurrDesc.stencilLoadOp == VK_ATTACHMENT_LOAD_OP_CLEAR)
		{
			if (StartClearEntry == -1)
			{
				StartClearEntry = NumAttachmentDescriptions;
				NumUsedClearValues = StartClearEntry + 1;
			}
			else
			{
				NumUsedClearValues = NumAttachmentDescriptions + 1;
			}
		}
		if (CurrDesc.samples == VK_SAMPLE_COUNT_1_BIT)
		{
			CurrDesc.storeOp = RenderTargetStoreActionToVulkan(RTInfo.DepthStencilRenderTarget.DepthStoreAction);
			CurrDesc.stencilStoreOp = RenderTargetStoreActionToVulkan(RTInfo.DepthStencilRenderTarget.GetStencilStoreAction());
		}
		else
		{
			// Never want to store MSAA depth/stencil
			CurrDesc.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			CurrDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		}
		CurrDesc.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		CurrDesc.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		
		DepthStencilReference.attachment = NumAttachmentDescriptions;
		DepthStencilReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		++NumAttachmentDescriptions;
/*
		if (CurrDesc.samples > VK_SAMPLE_COUNT_1_BIT)
		{
			Desc[NumAttachments + 1] = Desc[NumAttachments];
			Desc[NumAttachments + 1].samples = VK_SAMPLE_COUNT_1_BIT;
			ResolveReferences[NumColorAttachments].attachment = NumAttachments + 1;
			ResolveReferences[NumColorAttachments].layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			++NumAttachments;
			bHasResolveAttachments = true;
		}*/

		bHasDepthStencil = true;

		if (bSetExtent)
		{
			ensure(Extent.Extent3D.width == Texture->Surface.Width);
			ensure(Extent.Extent3D.height == Texture->Surface.Height);
		}
		else
		{
			bSetExtent = true;
			Extent.Extent3D.width = Texture->Surface.Width;
			Extent.Extent3D.height = Texture->Surface.Height;
			Extent.Extent3D.depth = 1;
		}
	}

	// Fill up hash struct
	FRenderPassHashableStruct RTHash;
	{
		FMemory::Memzero(RTHash);
		RTHash.NumAttachments = RTInfo.NumColorRenderTargets;
		RTHash.NumSamples = NumSamples;
		for (int32 Index = 0; Index < RTInfo.NumColorRenderTargets; ++Index)
		{
			RTHash.LoadActions[Index] = RTInfo.ColorRenderTarget[Index].LoadAction;
			RTHash.StoreActions[Index] = RTInfo.ColorRenderTarget[Index].StoreAction;
			if (RTInfo.ColorRenderTarget[Index].Texture)
			{
				const FRHIRenderTargetView& RTView = RTInfo.ColorRenderTarget[Index];
				FVulkanTextureBase* Texture = FVulkanTextureBase::Cast(RTView.Texture);
				RTHash.Formats[Index] = Texture->Surface.ViewFormat;
			}
			else
			{
				RTHash.Formats[Index] = VK_FORMAT_UNDEFINED;
			}
		}

		RTHash.Formats[MaxSimultaneousRenderTargets] = UEToVkFormat(RTInfo.DepthStencilRenderTarget.Texture ? RTInfo.DepthStencilRenderTarget.Texture->GetFormat() : PF_Unknown, false);
		RTHash.DepthLoad= RTInfo.DepthStencilRenderTarget.DepthLoadAction;
		RTHash.DepthStore = RTInfo.DepthStencilRenderTarget.DepthStoreAction;
		RTHash.StencilLoad = RTInfo.DepthStencilRenderTarget.StencilLoadAction;
		RTHash.StencilStore = RTInfo.DepthStencilRenderTarget.GetStencilStoreAction();
	}
	OldHash = FCrc::MemCrc32(&RTHash, sizeof(RTHash));

	CreateRenderPassHash();
}

FVulkanRenderTargetLayout::FVulkanRenderTargetLayout(const FGraphicsPipelineStateInitializer& Initializer)
	: NumAttachmentDescriptions(0)
	, NumColorAttachments(0)
	, bHasDepthStencil(false)
	, bHasResolveAttachments(false)
	, NumSamples(0)
	, NumUsedClearValues(0)
	, OldHash(0)
	, RenderPassHash(0)
{
	FMemory::Memzero(ColorReferences);
	FMemory::Memzero(ResolveReferences);
	FMemory::Memzero(DepthStencilReference);
	FMemory::Memzero(Desc);
	FMemory::Memzero(Extent);

	bool bSetExtent = false;
	int32 StartClearEntry = -1;
	NumSamples = Initializer.NumSamples;
	for (uint32 Index = 0; Index < Initializer.RenderTargetsEnabled; ++Index)
	{
		EPixelFormat UEFormat = Initializer.RenderTargetFormats[Index];
		if (UEFormat != PF_Unknown)
		{
			VkAttachmentDescription& CurrDesc = Desc[NumAttachmentDescriptions];

			//@TODO: Check this, it should be a power-of-two. Might be a VulkanConvert helper function.
			CurrDesc.samples = static_cast<VkSampleCountFlagBits>(NumSamples);
			CurrDesc.format = UEToVkFormat(UEFormat, (Initializer.RenderTargetFlags[Index] & TexCreate_SRGB) == TexCreate_SRGB);
			CurrDesc.loadOp = RenderTargetLoadActionToVulkan(Initializer.RenderTargetLoadActions[Index]);
			if (CurrDesc.loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR)
			{
				if (StartClearEntry == -1)
				{
					StartClearEntry = NumAttachmentDescriptions;
					NumUsedClearValues = StartClearEntry + 1;
				}
				else
				{
					NumUsedClearValues = NumAttachmentDescriptions + 1;
				}
			}
			CurrDesc.storeOp = RenderTargetStoreActionToVulkan(Initializer.RenderTargetStoreActions[Index]);
			CurrDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			CurrDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			CurrDesc.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			CurrDesc.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			ColorReferences[NumColorAttachments].attachment = NumAttachmentDescriptions;
			ColorReferences[NumColorAttachments].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			if (CurrDesc.samples > VK_SAMPLE_COUNT_1_BIT)
			{
				Desc[NumAttachmentDescriptions + 1] = Desc[NumAttachmentDescriptions];
				Desc[NumAttachmentDescriptions + 1].samples = VK_SAMPLE_COUNT_1_BIT;
				ResolveReferences[NumColorAttachments].attachment = NumAttachmentDescriptions + 1;
				ResolveReferences[NumColorAttachments].layout = VK_IMAGE_LAYOUT_GENERAL;
				++NumAttachmentDescriptions;
				bHasResolveAttachments = true;
			}

			++NumAttachmentDescriptions;
			NumColorAttachments++;
		}
	}

	if (Initializer.DepthStencilTargetFormat != PF_Unknown)
	{
		VkAttachmentDescription& CurrDesc = Desc[NumAttachmentDescriptions];
		FMemory::Memzero(CurrDesc);

		//@TODO: Check this, it should be a power-of-two. Might be a VulkanConvert helper function.
		CurrDesc.samples = static_cast<VkSampleCountFlagBits>(NumSamples);
		CurrDesc.format = UEToVkFormat(Initializer.DepthStencilTargetFormat, false);
		CurrDesc.loadOp = RenderTargetLoadActionToVulkan(Initializer.DepthTargetLoadAction);
		CurrDesc.stencilLoadOp = RenderTargetLoadActionToVulkan(Initializer.StencilTargetLoadAction);
		if (CurrDesc.loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR || CurrDesc.stencilLoadOp == VK_ATTACHMENT_LOAD_OP_CLEAR)
		{
			if (StartClearEntry == -1)
			{
				StartClearEntry = NumAttachmentDescriptions;
				NumUsedClearValues = StartClearEntry + 1;
			}
			else
			{
				NumUsedClearValues = NumAttachmentDescriptions + 1;
			}
		}
		if (CurrDesc.samples == VK_SAMPLE_COUNT_1_BIT)
		{
			CurrDesc.storeOp = RenderTargetStoreActionToVulkan(Initializer.StencilTargetStoreAction);
			CurrDesc.stencilStoreOp = RenderTargetStoreActionToVulkan(Initializer.StencilTargetStoreAction);
		}
		else
		{
			// Never want to store MSAA depth/stencil
			CurrDesc.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			CurrDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		}
		CurrDesc.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		CurrDesc.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		DepthStencilReference.attachment = NumAttachmentDescriptions;
		DepthStencilReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		++NumAttachmentDescriptions;
		/*
		if (CurrDesc.samples > VK_SAMPLE_COUNT_1_BIT)
		{
		Desc[NumAttachments + 1] = Desc[NumAttachments];
		Desc[NumAttachments + 1].samples = VK_SAMPLE_COUNT_1_BIT;
		ResolveReferences[NumColorAttachments].attachment = NumAttachments + 1;
		ResolveReferences[NumColorAttachments].layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		++NumAttachments;
		bHasResolveAttachments = true;
		}*/

		bHasDepthStencil = true;
	}

	// Fill up hash struct
	FRenderPassHashableStruct RTHash;
	{
		FMemory::Memzero(RTHash);
		RTHash.NumAttachments = Initializer.RenderTargetsEnabled;
		RTHash.NumSamples = NumSamples;
		for (uint32 Index = 0; Index < Initializer.RenderTargetsEnabled; ++Index)
		{
			RTHash.LoadActions[Index] = Initializer.RenderTargetLoadActions[Index];
			RTHash.StoreActions[Index] = Initializer.RenderTargetStoreActions[Index];
			RTHash.Formats[Index] = UEToVkFormat(Initializer.RenderTargetFormats[Index], (Initializer.RenderTargetFlags[Index] & TexCreate_SRGB) == TexCreate_SRGB);
		}

		RTHash.Formats[MaxSimultaneousRenderTargets] = UEToVkFormat(Initializer.DepthStencilTargetFormat, false);
		RTHash.DepthLoad= Initializer.DepthTargetLoadAction;
		RTHash.DepthStore = Initializer.DepthTargetStoreAction;
		RTHash.StencilLoad = Initializer.StencilTargetLoadAction;
		RTHash.StencilStore = Initializer.StencilTargetStoreAction;
	}
	OldHash = FCrc::MemCrc32(&RTHash, sizeof(RTHash));
	CreateRenderPassHash();
}


void FVulkanRenderTargetLayout::CreateRenderPassHash()
{
	// This would be the hashable structure:
	//struct FRenderPassHashable
	//{
	//	FRenderPassHashable(const FVulkanRenderTargetLayout& InRenderTargetLayout)
	//	{
	//		FMemory::Memcpy(Descriptions, InRenderTargetLayout.Desc);
	//	}
	//	VkAttachmentDescription Descriptions[MaxSimultaneousRenderTargets * 2 + 1];
	//};
	// FRenderPassHashable RenderPassHashable(*this);
	// RenderPassHash = FCrc::MemCrc32(&RenderPassHashable, sizeof(RenderPassHashable));
	// If more items are needed for the render pass, re-add the structure and add members. As for right now since the contents
	// match exactly what we have, just avoid the memcpy!
	RenderPassHash = FCrc::MemCrc32(&Desc, sizeof(Desc));
}
