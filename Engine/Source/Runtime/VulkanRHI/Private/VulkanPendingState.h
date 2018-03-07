// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved..

/*=============================================================================
	VulkanPendingState.h: Private VulkanPendingState definitions.
=============================================================================*/

#pragma once

// Dependencies
#include "VulkanRHI.h"
#include "VulkanPipeline.h"
#include "VulkanGlobalUniformBuffer.h"
#include "VulkanPipelineState.h"

// All the current compute pipeline states in use
class FVulkanPendingComputeState : public VulkanRHI::FDeviceChild
{
public:
	FVulkanPendingComputeState(FVulkanDevice* InDevice, FVulkanCommandListContext& InContext)
		: VulkanRHI::FDeviceChild(InDevice)
		, Context(InContext)
	{
	}

	~FVulkanPendingComputeState();

	inline FVulkanGlobalUniformPool& GetGlobalUniformPool()
	{
		return GlobalUniformPool;
	}

	void SetComputePipeline(FVulkanComputePipeline* InComputePipeline)
	{
		if (InComputePipeline != CurrentPipeline)
		{
			CurrentPipeline = InComputePipeline;
			FVulkanComputePipelineState** Found = PipelineStates.Find(InComputePipeline);
			if (Found)
			{
				CurrentState = *Found;
				check(CurrentState->ComputePipeline == InComputePipeline);
			}
			else
			{
				CurrentState = new FVulkanComputePipelineState(Device, InComputePipeline);
				PipelineStates.Add(CurrentPipeline, CurrentState);
			}

			CurrentState->Reset();
		}
	}

	void PrepareForDispatch(FVulkanCmdBuffer* CmdBuffer);

	inline const FVulkanComputeShader* GetCurrentShader() const
	{
		return CurrentPipeline ? CurrentPipeline->GetShader() : nullptr;
	}

	inline void AddUAVForAutoFlush(FVulkanUnorderedAccessView* UAV)
	{
		UAVListForAutoFlush.Add(UAV);
	}

	void SetUAV(uint32 UAVIndex, FVulkanUnorderedAccessView* UAV);

	inline void SetTexture(uint32 BindPoint, const FVulkanTextureBase* TextureBase)
	{
		CurrentState->SetTexture(BindPoint, TextureBase);
	}

	void SetSRV(uint32 BindIndex, FVulkanShaderResourceView* SRV);

	inline void SetShaderParameter(uint32 BufferIndex, uint32 ByteOffset, uint32 NumBytes, const void* NewValue)
	{
		CurrentState->SetShaderParameter(BufferIndex, ByteOffset, NumBytes, NewValue);
	}

	inline void SetUniformBufferConstantData(uint32 BindPoint, const TArray<uint8>& ConstantData)
	{
		CurrentState->SetUniformBufferConstantData(BindPoint, ConstantData);
	}

	inline void SetSamplerState(uint32 BindPoint, FVulkanSamplerState* Sampler)
	{
		CurrentState->SetSamplerState(BindPoint, Sampler);
	}

	void NotifyDeletedPipeline(FVulkanComputePipeline* Pipeline)
	{
		PipelineStates.Remove(Pipeline);
	}

protected:
	FVulkanGlobalUniformPool GlobalUniformPool;
	TArray<FVulkanUnorderedAccessView*> UAVListForAutoFlush;

	FVulkanComputePipeline* CurrentPipeline;
	FVulkanComputePipelineState* CurrentState;

	TMap<FVulkanComputePipeline*, FVulkanComputePipelineState*> PipelineStates;

	FVulkanCommandListContext& Context;

	friend class FVulkanCommandListContext;
};

// All the current gfx pipeline states in use
class FVulkanPendingGfxState : public VulkanRHI::FDeviceChild
{
public:
	FVulkanPendingGfxState(FVulkanDevice* InDevice, FVulkanCommandListContext& InContext)
		: VulkanRHI::FDeviceChild(InDevice)
		, Context(InContext)
	{
		Reset();
	}

	~FVulkanPendingGfxState();

	inline FVulkanGlobalUniformPool& GetGlobalUniformPool()
	{
		return GlobalUniformPool;
	}

	void Reset()
	{
		FMemory::Memzero(Scissor);
		FMemory::Memzero(Viewport);
		StencilRef = 0;
		bScissorEnable = false;

		CurrentPipeline = nullptr;
		CurrentState = nullptr;
		CurrentBSS = nullptr;
		bDirtyVertexStreams = true;

		//#todo-rco: Would this cause issues?
		//FMemory::Memzero(PendingStreams);
	}

	void SetViewport(uint32 MinX, uint32 MinY, float MinZ, uint32 MaxX, uint32 MaxY, float MaxZ)
	{
		FMemory::Memzero(Viewport);

		Viewport.x = MinX;
		Viewport.y = MinY;
		Viewport.width = MaxX - MinX;
		Viewport.height = MaxY - MinY;
		Viewport.minDepth = MinZ;
		if (MinZ == MaxZ)
		{
			// Engine pases in some cases MaxZ as 0.0
			Viewport.maxDepth = MinZ + 1.0f;
		}
		else
		{
			Viewport.maxDepth = MaxZ;
		}

		SetScissorRect(MinX, MinY, MaxX - MinX, MaxY - MinY);
		bScissorEnable = false;
	}

	inline void SetScissor(bool bInEnable, uint32 MinX, uint32 MinY, uint32 MaxX, uint32 MaxY)
	{
		if (bInEnable)
		{
			SetScissorRect(MinX, MinY, MaxX - MinX, MaxY - MinY);
		}
		else
		{
			SetScissorRect(Viewport.x, Viewport.y, Viewport.width, Viewport.height);
		}

		bScissorEnable = bInEnable;
	}

	inline void SetScissorRect(uint32 MinX, uint32 MinY, uint32 Width, uint32 Height)
	{
		FMemory::Memzero(Scissor);

		Scissor.offset.x = MinX;
		Scissor.offset.y = MinY;
		Scissor.extent.width = Width;
		Scissor.extent.height = Height;
	}

	inline void SetStreamSource(uint32 StreamIndex, FVulkanResourceMultiBuffer* VertexBuffer, uint32 Offset)
	{
		//PendingStreams[StreamIndex].Stream = VertexBuffer;
		PendingStreams[StreamIndex].Stream2  = VertexBuffer;
		PendingStreams[StreamIndex].Stream3  = VK_NULL_HANDLE;
		PendingStreams[StreamIndex].BufferOffset = Offset;
		bDirtyVertexStreams = true;
	}

	inline void SetStreamSource(uint32 StreamIndex, VkBuffer VertexBuffer, uint32 Offset)
	{
		PendingStreams[StreamIndex].Stream2  = nullptr;
		PendingStreams[StreamIndex].Stream3 = VertexBuffer;
		PendingStreams[StreamIndex].BufferOffset = Offset;
		bDirtyVertexStreams = true;
	}

	inline void SetTexture(EShaderFrequency Stage, uint32 BindPoint, const FVulkanTextureBase* TextureBase)
	{
		CurrentState->SetTexture(Stage, BindPoint, TextureBase);
	}

	inline void SetUniformBufferConstantData(EShaderFrequency Stage, uint32 BindPoint, const TArray<uint8>& ConstantData)
	{
		CurrentState->SetUniformBufferConstantData(Stage, BindPoint, ConstantData);
	}

	inline void SetUniformBuffer(EShaderFrequency Stage, uint32 BindPoint, const FVulkanUniformBuffer* UniformBuffer)
	{
		CurrentState->SetUniformBuffer(Stage, BindPoint, UniformBuffer);
	}

	void SetUAV(EShaderFrequency Stage, uint32 UAVIndex, FVulkanUnorderedAccessView* UAV);

	void SetSRV(EShaderFrequency Stage, uint32 BindIndex, FVulkanShaderResourceView* SRV);

	inline void SetSamplerState(EShaderFrequency Stage, uint32 BindPoint, FVulkanSamplerState* Sampler)
	{
		CurrentState->SetSamplerState(Stage, BindPoint, Sampler);
	}

	inline void SetShaderParameter(EShaderFrequency Stage, uint32 BufferIndex, uint32 ByteOffset, uint32 NumBytes, const void* NewValue)
	{
		CurrentState->SetShaderParameter(Stage, BufferIndex, ByteOffset, NumBytes, NewValue);
	}

	void PrepareForDraw(FVulkanCmdBuffer* CmdBuffer, VkPrimitiveTopology Topology);

	bool SetGfxPipeline(FVulkanGraphicsPipelineState* InGfxPipeline)
	{
		if (InGfxPipeline != CurrentPipeline)
		{
			// note: BSS objects are cached so this should only be a lookup
			CurrentBSS = ResourceCast(
				RHICreateBoundShaderState(
					InGfxPipeline->PipelineStateInitializer.BoundShaderState.VertexDeclarationRHI,
					InGfxPipeline->PipelineStateInitializer.BoundShaderState.VertexShaderRHI,
					InGfxPipeline->PipelineStateInitializer.BoundShaderState.HullShaderRHI,
					InGfxPipeline->PipelineStateInitializer.BoundShaderState.DomainShaderRHI,
					InGfxPipeline->PipelineStateInitializer.BoundShaderState.PixelShaderRHI,
					InGfxPipeline->PipelineStateInitializer.BoundShaderState.GeometryShaderRHI
				).GetReference()
			);

			CurrentPipeline = InGfxPipeline;
			FVulkanGfxPipelineState** Found = PipelineStates.Find(InGfxPipeline);
			if (Found)
			{
				CurrentState = *Found;
				check(CurrentState->GfxPipeline == InGfxPipeline);
			}
			else
			{
				CurrentState = new FVulkanGfxPipelineState(Device, InGfxPipeline, CurrentBSS);
				PipelineStates.Add(CurrentPipeline, CurrentState);
			}

			CurrentState->Reset();
			return true;
		}

		return false;
	}

	inline void UpdateDynamicStates(FVulkanCmdBuffer* Cmd)
	{
		InternalUpdateDynamicStates(Cmd);
	}

	inline void SetStencilRef(uint32 InStencilRef)
	{
		if (InStencilRef != StencilRef)
		{
			StencilRef = InStencilRef;
		}
	}

	void NotifyDeletedPipeline(FVulkanGraphicsPipelineState* Pipeline)
	{
		PipelineStates.Remove(Pipeline);
	}

	inline void MarkNeedsDynamicStates()
	{
	}

protected:
	FVulkanGlobalUniformPool GlobalUniformPool;

	VkViewport Viewport;
	uint32 StencilRef;
	bool bScissorEnable;
	VkRect2D Scissor;

	bool bNeedToClear;

	FVulkanGraphicsPipelineState* CurrentPipeline;
	FVulkanGfxPipelineState* CurrentState;
	FVulkanBoundShaderState* CurrentBSS;

	TMap<FVulkanGraphicsPipelineState*, FVulkanGfxPipelineState*> PipelineStates;

	struct FVertexStream
	{
		FVertexStream() :
			//Stream(nullptr),
			Stream2(nullptr),
			Stream3(VK_NULL_HANDLE),
			BufferOffset(0)
		{
		}

		//FVulkanBuffer* Stream;
		FVulkanResourceMultiBuffer* Stream2;
		VkBuffer Stream3;
		uint32 BufferOffset;
	};
	FVertexStream PendingStreams[MaxVertexElementCount];
	struct FTemporaryIA
	{
		TArray<VkBuffer> VertexBuffers;
		TArray<VkDeviceSize> VertexOffsets;
	} TemporaryIA;
	bool bDirtyVertexStreams;

	void InternalUpdateDynamicStates(FVulkanCmdBuffer* Cmd);

	FVulkanCommandListContext& Context;

	friend class FVulkanCommandListContext;
};
