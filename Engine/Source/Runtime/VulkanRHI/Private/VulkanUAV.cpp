// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "VulkanRHIPrivate.h"
#include "VulkanContext.h"

FVulkanShaderResourceView::FVulkanShaderResourceView(FVulkanDevice* Device, FVulkanResourceMultiBuffer* InSourceBuffer, uint32 InSize, EPixelFormat InFormat)
	: VulkanRHI::FDeviceChild(Device)
	, BufferViewFormat(InFormat)
	, MipLevel(0)
	, NumMips(-1)
	, Size(InSize)
	, SourceBuffer(InSourceBuffer)
	, VolatileLockCounter(MAX_uint32)
{
	int32 NumBuffers = SourceBuffer->IsVolatile() ? 1 : SourceBuffer->GetNumBuffers();
	BufferViews.AddZeroed(NumBuffers);
	check(BufferViewFormat != PF_Unknown);
}


FVulkanShaderResourceView::~FVulkanShaderResourceView()
{
	TextureView.Destroy(*Device);
	BufferViews.Empty(0);
	SourceTexture = nullptr;
	Device = nullptr;
}

void FVulkanShaderResourceView::UpdateView()
{
	SCOPE_CYCLE_COUNTER(STAT_VulkanSRVUpdateTime);

	// update the buffer view for dynamic backed buffers (or if it was never set)
	if (SourceBuffer != nullptr)
	{
		if (SourceBuffer->IsVolatile() && VolatileLockCounter != SourceBuffer->GetVolatileLockCounter())
		{
			bool bNeedNewView = true;
			// We might end up with the same BufferView, so do not recreate in that case
			if (!BufferViews[0] || BufferViews[0]->Offset != SourceBuffer->GetOffset() || BufferViews[0]->Size != Size)
			{
				BufferViews[0] = nullptr;
			}

			VolatileLockCounter = SourceBuffer->GetVolatileLockCounter();
		}
		else if (SourceBuffer->IsDynamic())
		{
			BufferIndex = SourceBuffer->GetDynamicIndex();
		}

		if (!BufferViews[BufferIndex])
		{
			BufferViews[BufferIndex] = new FVulkanBufferView(Device);
			BufferViews[BufferIndex]->Create(SourceBuffer, BufferViewFormat, SourceBuffer->GetOffset(), Size);
		}
	}
	else if (SourceStructuredBuffer)
	{
		// Nothing...
	}
	else
	{
		if (TextureView.View == VK_NULL_HANDLE)
		{
			EPixelFormat Format = (BufferViewFormat == PF_Unknown) ? SourceTexture->GetFormat() : BufferViewFormat;
			if (FRHITexture2D* Tex2D = SourceTexture->GetTexture2D())
			{
				FVulkanTexture2D* VTex2D = ResourceCast(Tex2D);
				EPixelFormat OriginalFormat = Format;
				TextureView.Create(*Device, VTex2D->Surface.Image, VK_IMAGE_VIEW_TYPE_2D, VTex2D->Surface.GetPartialAspectMask(), Format, UEToVkFormat(Format, false), MipLevel, NumMips, 0, 1);
			}
			else if (FRHITextureCube* TexCube = SourceTexture->GetTextureCube())
			{
				FVulkanTextureCube* VTexCube = ResourceCast(TexCube);
				TextureView.Create(*Device, VTexCube->Surface.Image, VK_IMAGE_VIEW_TYPE_CUBE, VTexCube->Surface.GetPartialAspectMask(), Format, UEToVkFormat(Format, false), MipLevel, NumMips, 0, 1);
			}
			else
			{
				ensure(0);
			}
		}
	}
}


FVulkanUnorderedAccessView::~FVulkanUnorderedAccessView()
{
	TextureView.Destroy(*Device);
	BufferView = nullptr;
	SourceVertexBuffer = nullptr;
	SourceTexture = nullptr;
	Device = nullptr;
}

void FVulkanUnorderedAccessView::UpdateView()
{
	SCOPE_CYCLE_COUNTER(STAT_VulkanUAVUpdateTime);

	// update the buffer view for dynamic VB backed buffers (or if it was never set)
	if (SourceVertexBuffer != nullptr)
	{
		if (SourceVertexBuffer->IsVolatile() && VolatileLockCounter != SourceVertexBuffer->GetVolatileLockCounter())
		{
			BufferView = nullptr;
			VolatileLockCounter = SourceVertexBuffer->GetVolatileLockCounter();
		}

		if (BufferView == nullptr || SourceVertexBuffer->IsDynamic())
		{
			// thanks to ref counting, overwriting the buffer will toss the old view
			BufferView = new FVulkanBufferView(Device);
			BufferView->Create(SourceVertexBuffer.GetReference(), BufferViewFormat, SourceVertexBuffer->GetOffset(), SourceVertexBuffer->GetSize());
		}
	}
	else if (SourceIndexBuffer != nullptr)
	{
		if (SourceIndexBuffer->IsVolatile() && VolatileLockCounter != SourceIndexBuffer->GetVolatileLockCounter())
		{
			BufferView = nullptr;
			VolatileLockCounter = SourceIndexBuffer->GetVolatileLockCounter();
		}

		if (BufferView == nullptr || SourceIndexBuffer->IsDynamic())
		{
			// thanks to ref counting, overwriting the buffer will toss the old view
			BufferView = new FVulkanBufferView(Device);
			BufferView->Create(SourceIndexBuffer.GetReference(), BufferViewFormat, SourceIndexBuffer->GetOffset(), SourceIndexBuffer->GetSize());
		}
	}
	else if (SourceStructuredBuffer)
	{
		// Nothing...
		//if (SourceStructuredBuffer->IsVolatile() && VolatileLockCounter != SourceStructuredBuffer->GetVolatileLockCounter())
		//{
		//	BufferView = nullptr;
		//	VolatileLockCounter = SourceStructuredBuffer->GetVolatileLockCounter();
		//}
	}
	else if (TextureView.View == VK_NULL_HANDLE)
	{
		EPixelFormat Format = (BufferViewFormat == PF_Unknown) ? SourceTexture->GetFormat() : BufferViewFormat;
		if (FRHITexture2D* Tex2D = SourceTexture->GetTexture2D())
		{
			FVulkanTexture2D* VTex2D = ResourceCast(Tex2D);
			TextureView.Create(*Device, VTex2D->Surface.Image, VK_IMAGE_VIEW_TYPE_2D, VTex2D->Surface.GetPartialAspectMask(), Format, UEToVkFormat(Format, false), MipLevel, 1, 0, 1);
		}
		else if (FRHITextureCube* TexCube = SourceTexture->GetTextureCube())
		{
			FVulkanTextureCube* VTexCube = ResourceCast(TexCube);
			TextureView.Create(*Device, VTexCube->Surface.Image, VK_IMAGE_VIEW_TYPE_CUBE, VTexCube->Surface.GetPartialAspectMask(), Format, UEToVkFormat(Format, false), MipLevel, 1, 0, 1);
		}
		else if (FRHITexture3D* Tex3D = SourceTexture->GetTexture3D())
		{
			FVulkanTexture3D* VTex3D = ResourceCast(Tex3D);
			TextureView.Create(*Device, VTex3D->Surface.Image, VK_IMAGE_VIEW_TYPE_3D, VTex3D->Surface.GetPartialAspectMask(), Format, UEToVkFormat(Format, false), MipLevel, 1, 0, VTex3D->GetSizeZ());
		}
		else
		{
			ensure(0);
		}
	}
}

FUnorderedAccessViewRHIRef FVulkanDynamicRHI::RHICreateUnorderedAccessView(FStructuredBufferRHIParamRef StructuredBufferRHI, bool bUseUAVCounter, bool bAppendBuffer)
{
	FVulkanStructuredBuffer* StructuredBuffer = ResourceCast(StructuredBufferRHI);

	FVulkanUnorderedAccessView* UAV = new FVulkanUnorderedAccessView(Device);
	// delay the shader view create until we use it, so we just track the source info here
	UAV->SourceStructuredBuffer = StructuredBuffer;

	//#todo-rco: bUseUAVCounter and bAppendBuffer

	return UAV;
}

FUnorderedAccessViewRHIRef FVulkanDynamicRHI::RHICreateUnorderedAccessView(FTextureRHIParamRef TextureRHI, uint32 MipLevel)
{
	FVulkanUnorderedAccessView* UAV = new FVulkanUnorderedAccessView(Device);
	UAV->SourceTexture = TextureRHI;
	UAV->MipLevel = MipLevel;
	return UAV;
}

FUnorderedAccessViewRHIRef FVulkanDynamicRHI::RHICreateUnorderedAccessView(FVertexBufferRHIParamRef VertexBufferRHI, uint8 Format)
{
	FVulkanVertexBuffer* VertexBuffer = ResourceCast(VertexBufferRHI);

	FVulkanUnorderedAccessView* UAV = new FVulkanUnorderedAccessView(Device);
	// delay the shader view create until we use it, so we just track the source info here
	UAV->BufferViewFormat = (EPixelFormat)Format;
	UAV->SourceVertexBuffer = VertexBuffer;

	return UAV;
}

FShaderResourceViewRHIRef FVulkanDynamicRHI::RHICreateShaderResourceView(FStructuredBufferRHIParamRef StructuredBufferRHI)
{
	FVulkanStructuredBuffer* StructuredBuffer = ResourceCast(StructuredBufferRHI);
	FVulkanShaderResourceView* SRV = new FVulkanShaderResourceView(Device, StructuredBuffer);
	return SRV;
}

FShaderResourceViewRHIRef FVulkanDynamicRHI::RHICreateShaderResourceView(FVertexBufferRHIParamRef VertexBufferRHI, uint32 Stride, uint8 Format)
{	
	FVulkanVertexBuffer* VertexBuffer = ResourceCast(VertexBufferRHI);
	FVulkanShaderResourceView* SRV = new FVulkanShaderResourceView(Device, VertexBuffer, VertexBuffer->GetSize(), (EPixelFormat)Format);
	return SRV;
}

FShaderResourceViewRHIRef FVulkanDynamicRHI::RHICreateShaderResourceView(FTexture2DRHIParamRef Texture2DRHI, uint8 MipLevel)
{
	FVulkanShaderResourceView* SRV = new FVulkanShaderResourceView(Device, Texture2DRHI, MipLevel, 1, ResourceCast(Texture2DRHI)->Surface.PixelFormat);
	return SRV;
}

FShaderResourceViewRHIRef FVulkanDynamicRHI::RHICreateShaderResourceView(FTexture2DRHIParamRef Texture2DRHI, uint8 MipLevel, uint8 NumMipLevels, uint8 Format)
{
	FVulkanShaderResourceView* SRV = new FVulkanShaderResourceView(Device, Texture2DRHI, MipLevel, NumMipLevels, (EPixelFormat)Format);
	return SRV;
}

FShaderResourceViewRHIRef FVulkanDynamicRHI::RHICreateShaderResourceView(FTexture3DRHIParamRef Texture3DRHI, uint8 MipLevel)
{
	FVulkanShaderResourceView* SRV = new FVulkanShaderResourceView(Device, Texture3DRHI, MipLevel, 1, ResourceCast(Texture3DRHI)->Surface.PixelFormat);
	return SRV;
}

FShaderResourceViewRHIRef FVulkanDynamicRHI::RHICreateShaderResourceView(FTexture2DArrayRHIParamRef Texture2DArrayRHI, uint8 MipLevel)
{
	FVulkanShaderResourceView* SRV = new FVulkanShaderResourceView(Device, Texture2DArrayRHI, MipLevel, 1, ResourceCast(Texture2DArrayRHI)->Surface.PixelFormat);
	return SRV;
}

FShaderResourceViewRHIRef FVulkanDynamicRHI::RHICreateShaderResourceView(FTextureCubeRHIParamRef TextureCubeRHI, uint8 MipLevel)
{
	FVulkanShaderResourceView* SRV = new FVulkanShaderResourceView(Device, TextureCubeRHI, MipLevel, 1, ResourceCast(TextureCubeRHI)->Surface.PixelFormat);
	return SRV;
}

FShaderResourceViewRHIRef FVulkanDynamicRHI::RHICreateShaderResourceView(FIndexBufferRHIParamRef IndexBufferRHI)
{
	FVulkanIndexBuffer* IndexBuffer = ResourceCast(IndexBufferRHI);
	check(IndexBufferRHI->GetStride() == 2 || IndexBufferRHI->GetStride() == 4);
	EPixelFormat Format = (IndexBufferRHI->GetStride() == 4) ? PF_R32_UINT : PF_R16_UINT;
	FVulkanShaderResourceView* SRV = new FVulkanShaderResourceView(Device, IndexBuffer, IndexBuffer->GetSize(), Format);
	return SRV;
}

void FVulkanCommandListContext::RHIClearTinyUAV(FUnorderedAccessViewRHIParamRef UnorderedAccessViewRHI, const uint32* Values)
{
	FVulkanUnorderedAccessView* UnorderedAccessView = ResourceCast(UnorderedAccessViewRHI);
	FVulkanCmdBuffer* CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();

	if (CmdBuffer->IsInsideRenderPass())
	{
		TransitionState.EndRenderPass(CmdBuffer);
	}

	if (UnorderedAccessView->SourceVertexBuffer)
	{
		FVulkanVertexBuffer* VertexBuffer = UnorderedAccessView->SourceVertexBuffer;
		switch (UnorderedAccessView->BufferViewFormat)
		{
		case PF_R32_SINT:
		case PF_R32_FLOAT:
		case PF_R32_UINT:
			break;
		case PF_A8R8G8B8:
		case PF_R8G8B8A8:
		case PF_B8G8R8A8:
			ensure(Values[0] == Values[1] && Values[1] == Values[2] && Values[2] == Values[3]);
			break;
		default:
			ensureMsgf(0, TEXT("Unsupported format (EPixelFormat)%d!"), (uint32)UnorderedAccessView->BufferViewFormat);
			break;
		}
		VulkanRHI::vkCmdFillBuffer(CmdBuffer->GetHandle(), VertexBuffer->GetHandle(), VertexBuffer->GetOffset(), VertexBuffer->GetSize(), Values[0]);
	}
	else
	{
		ensure(0);
	}
}

FVulkanComputeFence::FVulkanComputeFence(FVulkanDevice* InDevice, FName InName)
	: FRHIComputeFence(InName)
	, VulkanRHI::FGPUEvent(InDevice)
{
}

FVulkanComputeFence::~FVulkanComputeFence()
{
}

void FVulkanComputeFence::WriteCmd(VkCommandBuffer CmdBuffer)
{
	FRHIComputeFence::WriteFence();
	VulkanRHI::vkCmdSetEvent(CmdBuffer, Handle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
}


FComputeFenceRHIRef FVulkanDynamicRHI::RHICreateComputeFence(const FName& Name)
{
	return new FVulkanComputeFence(Device, Name);
}

void FVulkanCommandListContext::RHIWaitComputeFence(FComputeFenceRHIParamRef InFence)
{
	FVulkanComputeFence* Fence = ResourceCast(InFence);
	FVulkanCmdBuffer* CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();
	VkEvent Event = Fence->GetHandle();
	VulkanRHI::vkCmdWaitEvents(CmdBuffer->GetHandle(), 1, &Event, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, nullptr, 0, nullptr, 0, nullptr);
	IRHICommandContext::RHIWaitComputeFence(InFence);
}