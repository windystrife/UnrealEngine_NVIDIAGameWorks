// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalRenderPass.cpp: Metal command pass wrapper.
=============================================================================*/

#include "MetalRHIPrivate.h"

#include "MetalRenderPass.h"
#include "MetalCommandBuffer.h"
#include "ShaderCache.h"

#pragma mark - Private Console Variables -

static int32 GMetalCommandBufferCommitThreshold = 0;
static FAutoConsoleVariableRef CVarMetalCommandBufferCommitThreshold(
	TEXT("rhi.Metal.CommandBufferCommitThreshold"),
	GMetalCommandBufferCommitThreshold,
	TEXT("When enabled (> 0) if the command buffer has more than this number of draw/dispatch command encoded then it will be committed at the next encoder boundary to keep the GPU busy. (Default: 0, set to <= 0 to disable)"));

static int32 GMetalTessellationRunTessellationStage = 1;
static FAutoConsoleVariableRef CVarMetalTessellationRunTessellationStage(
	TEXT("rhi.Metal.RunTessellationStage"),
	GMetalTessellationRunTessellationStage,
	TEXT("Whether to run the VS+HS tessellation stage when performing tessellated draw calls in Metal or not. (Default: 1)"));

static int32 GMetalTessellationRunDomainStage = 1;
static FAutoConsoleVariableRef CVarMetalTessellationRunDomainStage(
	TEXT("rhi.Metal.RunDomainStage"),
	GMetalTessellationRunDomainStage,
	TEXT("Whether to run the DS+PS domain stage when performing tessellated draw calls in Metal or not. (Default: 1)"));

#pragma mark - Public C++ Boilerplate -

FMetalRenderPass::FMetalRenderPass(FMetalCommandList& InCmdList, FMetalStateCache& Cache)
: CmdList(InCmdList)
, State(Cache)
, CurrentEncoder(InCmdList)
, PrologueEncoder(InCmdList)
, PassStartFence(nil)
, CurrentEncoderFence(nil)
, PrologueEncoderFence(nil)
, RenderPassDesc(nil)
, NumOutstandingOps(0)
, bWithinRenderPass(false)
{
}

FMetalRenderPass::~FMetalRenderPass(void)
{
	check(!CurrentEncoder.GetCommandBuffer());
	check(!PrologueEncoder.GetCommandBuffer());
	check(!PassStartFence);
}

void FMetalRenderPass::Begin(id<MTLFence> Fence)
{
	check(!CurrentEncoder.GetCommandBuffer());
	check(!PrologueEncoder.GetCommandBuffer());
	check(!PassStartFence);
	PassStartFence = Fence;
	
	CurrentEncoder.StartCommandBuffer();
	check(CurrentEncoder.GetCommandBuffer());
}

void FMetalRenderPass::Wait(id<MTLFence> Fence)
{
	if (Fence)
	{
		if (PrologueEncoder.IsBlitCommandEncoderActive() || PrologueEncoder.IsComputeCommandEncoderActive())
		{
			PrologueEncoder.WaitForFence(Fence);
		}
		if (CurrentEncoder.IsRenderCommandEncoderActive() || CurrentEncoder.IsBlitCommandEncoderActive() || CurrentEncoder.IsComputeCommandEncoderActive())
		{
			CurrentEncoder.WaitForFence(Fence);
		}
	}
	PassStartFence = Fence;
		
}

void FMetalRenderPass::Update(id<MTLFence> Fence)
{
	if (Fence)
	{
		// Force an encoder - possibly consuming the start fence so that we get the proper order
		// the higher-level can generate empty contexts but we have no sane way to deal with that.
		if (!CurrentEncoder.IsRenderCommandEncoderActive() && !CurrentEncoder.IsBlitCommandEncoderActive() && !CurrentEncoder.IsComputeCommandEncoderActive())
		{
			ConditionalSwitchToCompute();
		}
		CurrentEncoder.UpdateFence(Fence);
		CurrentEncoderFence = CurrentEncoder.EndEncoding();
	}
}

id<MTLFence> FMetalRenderPass::Submit(EMetalSubmitFlags Flags)
{
	if (CurrentEncoder.GetCommandBuffer() || (Flags & EMetalSubmitFlagsAsyncCommandBuffer))
	{
		if (PrologueEncoder.IsBlitCommandEncoderActive() || PrologueEncoder.IsComputeCommandEncoderActive())
		{
			check(PrologueEncoder.GetCommandBuffer());
			PrologueEncoderFence = PrologueEncoder.EndEncoding();
		}
		if (PrologueEncoder.GetCommandBuffer())
		{
			PrologueEncoder.CommitCommandBuffer((Flags & EMetalSubmitFlagsAsyncCommandBuffer) ? Flags : EMetalSubmitFlagsNone);
        }
    }
    
    if (CurrentEncoder.GetCommandBuffer() && !(Flags & EMetalSubmitFlagsAsyncCommandBuffer))
    {
        if (CurrentEncoder.IsRenderCommandEncoderActive() || CurrentEncoder.IsBlitCommandEncoderActive() || CurrentEncoder.IsComputeCommandEncoderActive())
        {
            if (CurrentEncoder.IsRenderCommandEncoderActive())
            {
                State.SetRenderStoreActions(CurrentEncoder, (Flags & EMetalSubmitFlagsBreakCommandBuffer));
            }
            CurrentEncoderFence = CurrentEncoder.EndEncoding();
        }
		
        CurrentEncoder.CommitCommandBuffer(Flags);
    }
	
	check((Flags & (EMetalSubmitFlagsCreateCommandBuffer|EMetalSubmitFlagsAsyncCommandBuffer)) || !CurrentEncoder.GetCommandBuffer());
	check(!PrologueEncoder.GetCommandBuffer());
	
	return CurrentEncoderFence;
}

void FMetalRenderPass::BeginRenderPass(MTLRenderPassDescriptor* const RenderPass)
{
	check(!bWithinRenderPass);
	check(!RenderPassDesc);
	check(RenderPass);
	check(CurrentEncoder.GetCommandBuffer());
	check(!CurrentEncoder.IsRenderCommandEncoderActive());
	
	// EndEncoding should provide the encoder fence...
	if (PrologueEncoder.IsBlitCommandEncoderActive() || PrologueEncoder.IsComputeCommandEncoderActive())
	{
		PrologueEncoderFence = PrologueEncoder.EndEncoding();
	}
	if (CurrentEncoder.IsRenderCommandEncoderActive() || CurrentEncoder.IsBlitCommandEncoderActive() || CurrentEncoder.IsComputeCommandEncoderActive())
	{
		CurrentEncoderFence = CurrentEncoder.EndEncoding();
	}
	State.SetStateDirty();
	State.SetRenderTargetsActive(true);
	
	RenderPassDesc = RenderPass;
	
	CurrentEncoder.SetRenderPassDescriptor(RenderPassDesc);
	CurrentEncoder.BeginRenderCommandEncoding();
	if (PassStartFence)
	{
		CurrentEncoder.WaitForFence(PassStartFence);
	}
	if (CurrentEncoderFence)
	{
		CurrentEncoder.WaitForFence(CurrentEncoderFence);
	}
	if (PrologueEncoderFence)
	{
		CurrentEncoder.WaitForFence(PrologueEncoderFence);
	}
	State.SetRenderStoreActions(CurrentEncoder, false);
	
	bWithinRenderPass = true;
	
	check(CurrentEncoder.IsRenderCommandEncoderActive());
	check(!PrologueEncoder.IsBlitCommandEncoderActive() && !PrologueEncoder.IsComputeCommandEncoderActive());
}

void FMetalRenderPass::RestartRenderPass(MTLRenderPassDescriptor* const RenderPass)
{
	check(bWithinRenderPass);
	check(RenderPassDesc);
	check(CurrentEncoder.GetCommandBuffer());
	
	MTLRenderPassDescriptorRef StartDesc;
	if (RenderPass != nil)
	{
		// Just restart with the render pass we were given - the caller should have ensured that this is restartable
		check(State.CanRestartRenderPass());
		StartDesc = RenderPass;
	}
	else if (State.PrepareToRestart())
	{
		// Restart with the render pass we have in the state cache - the state cache says its safe
		StartDesc = State.GetRenderPassDescriptor();
	}
	else
	{
		UE_LOG(LogMetal, Fatal, TEXT("Failed to restart render pass with descriptor: %s"), *FString([(*RenderPassDesc) description]));
	}
	check(StartDesc);
	
	RenderPassDesc = StartDesc;
	
#if METAL_DEBUG_OPTIONS
	if ((GetMetalDeviceContext().GetCommandQueue().GetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation))
	{
		bool bAllLoadActionsOK = true;
		if((*RenderPassDesc).colorAttachments)
		{
			for(uint i = 0; i< 8; i++)
			{
				MTLRenderPassColorAttachmentDescriptor* Desc = [(*RenderPassDesc).colorAttachments objectAtIndexedSubscript:i];
				if(Desc && Desc.texture)
				{
					bAllLoadActionsOK &= (Desc.loadAction != MTLLoadActionClear);
				}
			}
		}
		if((*RenderPassDesc).depthAttachment && (*RenderPassDesc).depthAttachment.texture)
		{
			bAllLoadActionsOK &= ((*RenderPassDesc).depthAttachment.loadAction != MTLLoadActionClear);
		}
		if((*RenderPassDesc).stencilAttachment && (*RenderPassDesc).stencilAttachment.texture)
		{
			bAllLoadActionsOK &= ((*RenderPassDesc).stencilAttachment.loadAction != MTLLoadActionClear);
		}
		
		if (!bAllLoadActionsOK)
		{
			UE_LOG(LogMetal, Warning, TEXT("Tried to restart render encoding with a clear operation - this would erroneously re-clear any existing draw calls: %s"), *FString([(*RenderPassDesc) description]));
			
			if((*RenderPassDesc).colorAttachments)
			{
				for(uint i = 0; i< 8; i++)
				{
					MTLRenderPassColorAttachmentDescriptor* Desc = [(*RenderPassDesc).colorAttachments objectAtIndexedSubscript:i];
					if(Desc && Desc.texture)
					{
						Desc.loadAction = MTLLoadActionLoad;
					}
				}
			}
			if((*RenderPassDesc).depthAttachment && (*RenderPassDesc).depthAttachment.texture)
			{
				(*RenderPassDesc).depthAttachment.loadAction = MTLLoadActionLoad;
			}
			if((*RenderPassDesc).stencilAttachment && (*RenderPassDesc).stencilAttachment.texture)
			{
				(*RenderPassDesc).stencilAttachment.loadAction = MTLLoadActionLoad;
			}
		}
	}
#endif
	
	// EndEncoding should provide the encoder fence...
	if (CurrentEncoder.IsBlitCommandEncoderActive() || CurrentEncoder.IsComputeCommandEncoderActive() || CurrentEncoder.IsRenderCommandEncoderActive())
	{
		if (CurrentEncoder.IsRenderCommandEncoderActive())
		{
			State.SetRenderStoreActions(CurrentEncoder, true);
		}
		CurrentEncoderFence = CurrentEncoder.EndEncoding();
	}
	State.SetStateDirty();
	State.SetRenderTargetsActive(true);
	
	CurrentEncoder.SetRenderPassDescriptor(RenderPassDesc);
	CurrentEncoder.BeginRenderCommandEncoding();
	if (CurrentEncoderFence)
	{
		CurrentEncoder.WaitForFence(CurrentEncoderFence);
	}
	else
	{
		CurrentEncoder.WaitForFence(PassStartFence);
	}
	if (PrologueEncoderFence)
	{
		CurrentEncoder.WaitForFence(PrologueEncoderFence);
	}
	State.SetRenderStoreActions(CurrentEncoder, false);
	
	check(CurrentEncoder.IsRenderCommandEncoderActive());
}

void FMetalRenderPass::DrawPrimitive(uint32 PrimitiveType, uint32 BaseVertexIndex, uint32 NumPrimitives, uint32 NumInstances)
{
	NumInstances = FMath::Max(NumInstances,1u);
	
	if(!State.GetUsingTessellation())
	{
		ConditionalSwitchToRender();
		check(CurrentEncoder.GetCommandBuffer());
		check(CurrentEncoder.IsRenderCommandEncoderActive());
		
		PrepareToRender(PrimitiveType);
	
		// draw!
		if(!FShaderCache::IsPredrawCall(State.GetShaderCacheStateObject()))
		{
			// how many verts to render
			uint32 NumVertices = GetVertexCountForPrimitiveCount(NumPrimitives, PrimitiveType);
			
			[CurrentEncoder.GetRenderCommandEncoder() drawPrimitives:TranslatePrimitiveType(PrimitiveType)
														vertexStart:BaseVertexIndex
														vertexCount:NumVertices
														instanceCount:NumInstances];
		}
	}
	else
	{
		DrawPatches(PrimitiveType, nullptr, 0, BaseVertexIndex, 0, 0, NumPrimitives, NumInstances);
	}
	
	ConditionalSubmit();	
}

void FMetalRenderPass::DrawPrimitiveIndirect(uint32 PrimitiveType, FMetalVertexBuffer* VertexBuffer, uint32 ArgumentOffset)
{
	if (GetMetalDeviceContext().SupportsFeature(EMetalFeaturesIndirectBuffer))
	{
		ConditionalSwitchToRender();
		check(CurrentEncoder.GetCommandBuffer());
		check(CurrentEncoder.IsRenderCommandEncoderActive());
		
		PrepareToRender(PrimitiveType);
		
		if(!FShaderCache::IsPredrawCall(State.GetShaderCacheStateObject()))
		{
			[CurrentEncoder.GetRenderCommandEncoder() drawPrimitives:TranslatePrimitiveType(PrimitiveType)
										 indirectBuffer:VertexBuffer->Buffer
								   indirectBufferOffset:ArgumentOffset];
		}
		ConditionalSubmit();
	}
	else
	{
		NOT_SUPPORTED("RHIDrawPrimitiveIndirect");
	}
}

void FMetalRenderPass::DrawIndexedPrimitive(id<MTLBuffer> IndexBuffer, uint32 IndexStride, uint32 PrimitiveType, int32 BaseVertexIndex, uint32 FirstInstance,
											 uint32 NumVertices, uint32 StartIndex, uint32 NumPrimitives, uint32 NumInstances)
{
	NumInstances = FMath::Max(NumInstances,1u);
	
	if (!State.GetUsingTessellation())
	{
		ConditionalSwitchToRender();
		check(CurrentEncoder.GetCommandBuffer());
		check(CurrentEncoder.IsRenderCommandEncoderActive());
		
		PrepareToRender(PrimitiveType);
		
		uint32 NumIndices = GetVertexCountForPrimitiveCount(NumPrimitives, PrimitiveType);
		
		if(!FShaderCache::IsPredrawCall(State.GetShaderCacheStateObject()))
		{
			if (GRHISupportsBaseVertexIndex && GRHISupportsFirstInstance)
			{
				[CurrentEncoder.GetRenderCommandEncoder() drawIndexedPrimitives:TranslatePrimitiveType(PrimitiveType)
														indexCount:NumIndices
														 indexType:((IndexStride == 2) ? MTLIndexTypeUInt16 : MTLIndexTypeUInt32)
													   indexBuffer:IndexBuffer
												 indexBufferOffset:StartIndex * IndexStride
													 instanceCount:NumInstances
														baseVertex:BaseVertexIndex
													  baseInstance:FirstInstance];
			}
			else
			{
				[CurrentEncoder.GetRenderCommandEncoder() drawIndexedPrimitives:TranslatePrimitiveType(PrimitiveType)
														indexCount:NumIndices
														 indexType:((IndexStride == 2) ? MTLIndexTypeUInt16 : MTLIndexTypeUInt32)
													   indexBuffer:IndexBuffer
												 indexBufferOffset:StartIndex * IndexStride
													 instanceCount:NumInstances];
			}
		}
	}
	else
	{
		DrawPatches(PrimitiveType, IndexBuffer, IndexStride, BaseVertexIndex, FirstInstance, StartIndex, NumPrimitives, NumInstances);
	}
	
	ConditionalSubmit();
}

void FMetalRenderPass::DrawIndexedIndirect(FMetalIndexBuffer* IndexBuffer, uint32 PrimitiveType, FMetalStructuredBuffer* VertexBuffer, int32 DrawArgumentsIndex, uint32 NumInstances)
{
	if (GetMetalDeviceContext().SupportsFeature(EMetalFeaturesIndirectBuffer))
	{
		check(NumInstances > 1);
		
		ConditionalSwitchToRender();
		check(CurrentEncoder.GetCommandBuffer());
		check(CurrentEncoder.IsRenderCommandEncoderActive());
		
		// finalize any pending state
		PrepareToRender(PrimitiveType);
		
		if(!FShaderCache::IsPredrawCall(State.GetShaderCacheStateObject()))
		{
			[CurrentEncoder.GetRenderCommandEncoder() drawIndexedPrimitives:TranslatePrimitiveType(PrimitiveType)
													 indexType:IndexBuffer->IndexType
												   indexBuffer:IndexBuffer->Buffer
											 indexBufferOffset:0
												indirectBuffer:VertexBuffer->Buffer
										  indirectBufferOffset:(DrawArgumentsIndex * 5 * sizeof(uint32))];
		}
		ConditionalSubmit();
	}
	else
	{
		NOT_SUPPORTED("RHIDrawIndexedIndirect");
	}
}

void FMetalRenderPass::DrawIndexedPrimitiveIndirect(uint32 PrimitiveType,FMetalIndexBuffer* IndexBuffer,FMetalVertexBuffer* VertexBuffer,uint32 ArgumentOffset)
{
	if (GetMetalDeviceContext().SupportsFeature(EMetalFeaturesIndirectBuffer))
	{		 
		ConditionalSwitchToRender();
		check(CurrentEncoder.GetCommandBuffer());
		check(CurrentEncoder.IsRenderCommandEncoderActive());
		
		PrepareToRender(PrimitiveType);
		
		if(!FShaderCache::IsPredrawCall(State.GetShaderCacheStateObject()))
		{
			[CurrentEncoder.GetRenderCommandEncoder() drawIndexedPrimitives:TranslatePrimitiveType(PrimitiveType)
												 indexType:IndexBuffer->IndexType
											   indexBuffer:IndexBuffer->Buffer
										 indexBufferOffset:0
											indirectBuffer:VertexBuffer->Buffer
									  indirectBufferOffset:ArgumentOffset];
		}
		ConditionalSubmit();
	}
	else
	{
		NOT_SUPPORTED("RHIDrawIndexedPrimitiveIndirect");
	}
}

void FMetalRenderPass::DrawPatches(uint32 PrimitiveType,id<MTLBuffer> IndexBuffer, uint32 IndexBufferStride, int32 BaseVertexIndex, uint32 FirstInstance, uint32 StartIndex,
									uint32 NumPrimitives, uint32 NumInstances)
{
	if (GetMetalDeviceContext().SupportsFeature(EMetalFeaturesTessellation))
	{
		ConditionalSwitchToTessellation();
		check(CurrentEncoder.GetCommandBuffer());
		check(PrologueEncoder.GetCommandBuffer());
		check(PrologueEncoder.IsComputeCommandEncoderActive());
		check(CurrentEncoder.IsRenderCommandEncoderActive());
		
		size_t hullShaderOutputOffset = 0;
		size_t hullConstShaderOutputOffset = 0;
		size_t tessellationFactorsOffset = 0;
		
		FMetalDeviceContext& deviceContext = (FMetalDeviceContext&)GetMetalDeviceContext();
		id<MTLDevice> device = deviceContext.GetDevice();
		
		FMetalGraphicsPipelineState* boundShaderState = State.GetGraphicsPSO();
		FMetalShaderPipeline* Pipeline = State.GetPipelineState();
		
		// TODO could allocate this as 1 buffer and use the sizes to make the offsets we need...
		auto hullShaderOutputBufferSize = (Pipeline.TessellationPipelineDesc.TessellationPatchControlPointOutSize * boundShaderState->VertexShader->TessellationOutputControlPoints) * NumPrimitives * NumInstances;
		auto hullConstShaderOutputBufferSize = (Pipeline.TessellationPipelineDesc.TessellationPatchConstOutSize) * NumPrimitives * NumInstances;
		auto tessellationFactorBufferSize = (Pipeline.TessellationPipelineDesc.TessellationTessFactorOutSize) * NumPrimitives * NumInstances;
		
		id<MTLBuffer> hullShaderOutputBuffer = nil;
		if(hullShaderOutputBufferSize)
		{
			hullShaderOutputBuffer = deviceContext.CreatePooledBuffer(FMetalPooledBufferArgs(device, hullShaderOutputBufferSize, MTLStorageModePrivate));
		}
		
		id<MTLBuffer> hullConstShaderOutputBuffer = nil;
		if(hullConstShaderOutputBufferSize)
		{
			hullConstShaderOutputBuffer = deviceContext.CreatePooledBuffer(FMetalPooledBufferArgs(device, hullConstShaderOutputBufferSize, MTLStorageModePrivate));
		}
		
		id<MTLBuffer> tessellationFactorBuffer = nil;
		if(tessellationFactorBufferSize)
		{
			tessellationFactorBuffer = deviceContext.CreatePooledBuffer(FMetalPooledBufferArgs(device, tessellationFactorBufferSize, MTLStorageModePrivate));
		}
		
		if(hullShaderOutputBufferSize)
		{
			deviceContext.ReleaseResource(hullShaderOutputBuffer);
		}
		if(hullConstShaderOutputBufferSize)
		{
			deviceContext.ReleaseResource(hullConstShaderOutputBuffer);
		}
		if(tessellationFactorBufferSize)
		{
			deviceContext.ReleaseResource(tessellationFactorBuffer);
		}
	
		auto computeEncoder = PrologueEncoder.GetComputeCommandEncoder();
		auto renderEncoder = CurrentEncoder.GetRenderCommandEncoder();
		
		PrepareToTessellate(PrimitiveType);
		
		// Per-draw call bindings should *not* be cached in the StateCache - causes absolute chaos.
		if(IndexBuffer != nil && Pipeline.TessellationPipelineDesc.TessellationControlPointIndexBufferIndex != UINT_MAX)
		{
			PrologueEncoder.SetShaderBuffer(MTLFunctionTypeKernel, IndexBuffer, StartIndex * IndexBufferStride, IndexBuffer.length - (StartIndex * IndexBufferStride), Pipeline.TessellationPipelineDesc.TessellationControlPointIndexBufferIndex);
			PrologueEncoder.SetShaderBuffer(MTLFunctionTypeKernel, IndexBuffer, StartIndex * IndexBufferStride, IndexBuffer.length - (StartIndex * IndexBufferStride), Pipeline.TessellationPipelineDesc.TessellationIndexBufferIndex);
			State.SetShaderBuffer(SF_Vertex, nil, nil, 0, 0, Pipeline.TessellationPipelineDesc.TessellationControlPointIndexBufferIndex);
			State.SetShaderBuffer(SF_Vertex, nil, nil, 0, 0, Pipeline.TessellationPipelineDesc.TessellationIndexBufferIndex);
		}
		
		if(Pipeline.TessellationPipelineDesc.TessellationOutputControlPointBufferIndex != UINT_MAX) //TessellationOutputControlPointBufferIndex -> hullShaderOutputBuffer
		{
			PrologueEncoder.SetShaderBuffer(MTLFunctionTypeKernel, hullShaderOutputBuffer, hullShaderOutputOffset, hullShaderOutputBuffer.length - hullShaderOutputOffset, Pipeline.TessellationPipelineDesc.TessellationOutputControlPointBufferIndex);
			State.SetShaderBuffer(SF_Vertex, nil, nil, 0, 0, Pipeline.TessellationPipelineDesc.TessellationOutputControlPointBufferIndex);
		}
		
		if(Pipeline.TessellationPipelineDesc.TessellationPatchConstBufferIndex != UINT_MAX) //TessellationPatchConstBufferIndex -> hullConstShaderOutputBuffer
		{
			PrologueEncoder.SetShaderBuffer(MTLFunctionTypeKernel, hullConstShaderOutputBuffer, hullConstShaderOutputOffset, hullConstShaderOutputBuffer.length - hullConstShaderOutputOffset, Pipeline.TessellationPipelineDesc.TessellationPatchConstBufferIndex);
			State.SetShaderBuffer(SF_Vertex, nil, nil, 0, 0, Pipeline.TessellationPipelineDesc.TessellationPatchConstBufferIndex);
		}
		
		if(Pipeline.TessellationPipelineDesc.TessellationFactorBufferIndex != UINT_MAX) // TessellationFactorBufferIndex->tessellationFactorBuffer
		{
			PrologueEncoder.SetShaderBuffer(MTLFunctionTypeKernel, tessellationFactorBuffer, tessellationFactorsOffset, tessellationFactorBuffer.length - tessellationFactorsOffset, Pipeline.TessellationPipelineDesc.TessellationFactorBufferIndex);
			State.SetShaderBuffer(SF_Vertex, nil, nil, 0, 0, Pipeline.TessellationPipelineDesc.TessellationFactorBufferIndex);
		}
		
		if(Pipeline.TessellationPipelineDesc.TessellationInputControlPointBufferIndex != UINT_MAX) //TessellationInputControlPointBufferIndex->hullShaderOutputBuffer
		{
			CurrentEncoder.SetShaderBuffer(MTLFunctionTypeVertex, hullShaderOutputBuffer, hullShaderOutputOffset, hullShaderOutputBuffer.length - hullShaderOutputOffset, Pipeline.TessellationPipelineDesc.TessellationInputControlPointBufferIndex);
			State.SetShaderBuffer(SF_Domain, nil, nil, 0, 0, Pipeline.TessellationPipelineDesc.TessellationInputControlPointBufferIndex);
;
		}
		if(Pipeline.TessellationPipelineDesc.TessellationInputPatchConstBufferIndex != UINT_MAX) //TessellationInputPatchConstBufferIndex->hullConstShaderOutputBuffer
		{
			CurrentEncoder.SetShaderBuffer(MTLFunctionTypeVertex, hullConstShaderOutputBuffer, hullConstShaderOutputOffset, hullConstShaderOutputBuffer.length - hullConstShaderOutputOffset, Pipeline.TessellationPipelineDesc.TessellationInputPatchConstBufferIndex);
			State.SetShaderBuffer(SF_Domain, nil, nil, 0, 0, Pipeline.TessellationPipelineDesc.TessellationInputPatchConstBufferIndex);
		}
		
		// set the patchCount
		PrologueEncoder.SetShaderBytes(MTLFunctionTypeKernel, (const uint8*)&NumPrimitives, sizeof(NumPrimitives), Pipeline.TessellationPipelineDesc.TessellationPatchCountBufferIndex);
		State.SetShaderBuffer(SF_Vertex, nil, nil, 0, 0, Pipeline.TessellationPipelineDesc.TessellationPatchCountBufferIndex);
		
		if (boundShaderState->VertexShader->SideTableBinding >= 0)
		{
			PrologueEncoder.SetShaderSideTable(MTLFunctionTypeKernel, boundShaderState->VertexShader->SideTableBinding);
			State.SetShaderBuffer(SF_Vertex, nil, nil, 0, 0, boundShaderState->VertexShader->SideTableBinding);
		}
		
		if (boundShaderState->DomainShader->SideTableBinding >= 0)
		{
			CurrentEncoder.SetShaderSideTable(MTLFunctionTypeVertex, boundShaderState->DomainShader->SideTableBinding);
			State.SetShaderBuffer(SF_Domain, nil, nil, 0, 0, boundShaderState->DomainShader->SideTableBinding);
		}
		
		if (IsValidRef(boundShaderState->PixelShader) && boundShaderState->PixelShader->SideTableBinding >= 0)
		{
			CurrentEncoder.SetShaderSideTable(MTLFunctionTypeFragment, boundShaderState->PixelShader->SideTableBinding);
			State.SetShaderBuffer(SF_Pixel, nil, nil, 0, 0, boundShaderState->PixelShader->SideTableBinding);
		}
		
		auto patchesPerThreadGroup = boundShaderState->VertexShader->TessellationPatchesPerThreadGroup;
		auto threadgroups = MTLSizeMake((NumPrimitives + (patchesPerThreadGroup - 1)) / patchesPerThreadGroup, NumInstances, 1);
		auto threadsPerThreadgroup = MTLSizeMake(boundShaderState->VertexShader->TessellationInputControlPoints * patchesPerThreadGroup, 1, 1);
		
		[computeEncoder setStageInRegion:MTLRegionMake2D(BaseVertexIndex, FirstInstance, boundShaderState->VertexShader->TessellationInputControlPoints * NumPrimitives, NumInstances)];
		if(GMetalTessellationRunTessellationStage && !FShaderCache::IsPredrawCall(State.GetShaderCacheStateObject()))
		{
			[computeEncoder dispatchThreadgroups:threadgroups threadsPerThreadgroup:threadsPerThreadgroup];
		}
		
		check(computeEncoder != nil);
		check(renderEncoder != nil);
		
		if(tessellationFactorBuffer)
		{
			[renderEncoder setTessellationFactorBuffer: tessellationFactorBuffer offset: tessellationFactorsOffset instanceStride: 0];
		}
		
		if(GMetalTessellationRunDomainStage && !FShaderCache::IsPredrawCall(State.GetShaderCacheStateObject()))
		{
			[renderEncoder drawPatches: boundShaderState->VertexShader->TessellationOutputControlPoints 
										  patchStart: 0 patchCount: NumPrimitives * NumInstances
									patchIndexBuffer:nil patchIndexBufferOffset:0 
									   instanceCount:1 baseInstance:0];
		}
	}
	else
	{
		NOT_SUPPORTED("DrawPatches");
	}
}

void FMetalRenderPass::Dispatch(uint32 ThreadGroupCountX, uint32 ThreadGroupCountY, uint32 ThreadGroupCountZ)
{
	ConditionalSwitchToCompute();
	check(CurrentEncoder.GetCommandBuffer());
	check(CurrentEncoder.IsComputeCommandEncoderActive());

	PrepareToDispatch();
	
	TRefCountPtr<FMetalComputeShader> ComputeShader = State.GetComputeShader();
	check(ComputeShader);
	
	MTLSize ThreadgroupCounts = MTLSizeMake(ComputeShader->NumThreadsX, ComputeShader->NumThreadsY, ComputeShader->NumThreadsZ);
	check(ComputeShader->NumThreadsX > 0 && ComputeShader->NumThreadsY > 0 && ComputeShader->NumThreadsZ > 0);
	MTLSize Threadgroups = MTLSizeMake(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);
	[CurrentEncoder.GetComputeCommandEncoder() dispatchThreadgroups:Threadgroups threadsPerThreadgroup:ThreadgroupCounts];
	
	ConditionalSubmit();
}

void FMetalRenderPass::DispatchIndirect(FMetalVertexBuffer* ArgumentBuffer, uint32 ArgumentOffset)
{
	check(ArgumentBuffer);
	
	ConditionalSwitchToCompute();
	check(CurrentEncoder.GetCommandBuffer());
	check(CurrentEncoder.IsComputeCommandEncoderActive());
	
	PrepareToDispatch();
	
	TRefCountPtr<FMetalComputeShader> ComputeShader = State.GetComputeShader();
	check(ComputeShader);
	
	MTLSize ThreadgroupCounts = MTLSizeMake(ComputeShader->NumThreadsX, ComputeShader->NumThreadsY, ComputeShader->NumThreadsZ);
	check(ComputeShader->NumThreadsX > 0 && ComputeShader->NumThreadsY > 0 && ComputeShader->NumThreadsZ > 0);
	
	[CurrentEncoder.GetComputeCommandEncoder() dispatchThreadgroupsWithIndirectBuffer:ArgumentBuffer->Buffer indirectBufferOffset:ArgumentOffset threadsPerThreadgroup:ThreadgroupCounts];

	ConditionalSubmit();
}

id<MTLFence> FMetalRenderPass::EndRenderPass(void)
{
	if (bWithinRenderPass)
	{
		check(RenderPassDesc);
		check(CurrentEncoder.GetCommandBuffer());
		
		// This just calls End - it exists only to enforce assumptions
		End();
	}
	return CurrentEncoderFence;
}

void FMetalRenderPass::CopyFromTextureToBuffer(id<MTLTexture> Texture, uint32 sourceSlice, uint32 sourceLevel, MTLOrigin sourceOrigin, MTLSize sourceSize, id<MTLBuffer> toBuffer, uint32 destinationOffset, uint32 destinationBytesPerRow, uint32 destinationBytesPerImage, MTLBlitOption options)
{
	ConditionalSwitchToBlit();
	id<MTLBlitCommandEncoder> Encoder = CurrentEncoder.GetBlitCommandEncoder();
	check(Encoder);
	
	if (CmdList.GetCommandQueue().SupportsFeature(EMetalFeaturesDepthStencilBlitOptions))
	{
		[Encoder copyFromTexture : Texture sourceSlice : sourceSlice sourceLevel : sourceLevel sourceOrigin : sourceOrigin sourceSize : sourceSize toBuffer : toBuffer destinationOffset : destinationOffset destinationBytesPerRow : destinationBytesPerRow destinationBytesPerImage : destinationBytesPerImage options : options];
	}
	else
	{
		check(options == MTLBlitOptionNone);
		[Encoder copyFromTexture : Texture sourceSlice : sourceSlice sourceLevel : sourceLevel sourceOrigin : sourceOrigin sourceSize : sourceSize toBuffer : toBuffer destinationOffset : destinationOffset destinationBytesPerRow : destinationBytesPerRow destinationBytesPerImage : destinationBytesPerImage];
	}
	ConditionalSubmit();
}

void FMetalRenderPass::CopyFromBufferToTexture(id<MTLBuffer> Buffer, uint32 sourceOffset, uint32 sourceBytesPerRow, uint32 sourceBytesPerImage, MTLSize sourceSize, id<MTLTexture> toTexture, uint32 destinationSlice, uint32 destinationLevel, MTLOrigin destinationOrigin)
{
	ConditionalSwitchToBlit();
	id<MTLBlitCommandEncoder> Encoder = CurrentEncoder.GetBlitCommandEncoder();
	check(Encoder);
	
	[Encoder copyFromBuffer:Buffer sourceOffset:sourceOffset sourceBytesPerRow:sourceBytesPerRow sourceBytesPerImage:sourceBytesPerImage sourceSize:sourceSize toTexture:toTexture destinationSlice:destinationSlice destinationLevel:destinationLevel destinationOrigin:destinationOrigin];
	ConditionalSubmit();
}

void FMetalRenderPass::CopyFromTextureToTexture(id<MTLTexture> Texture, uint32 sourceSlice, uint32 sourceLevel, MTLOrigin sourceOrigin, MTLSize sourceSize, id<MTLTexture> toTexture, uint32 destinationSlice, uint32 destinationLevel, MTLOrigin destinationOrigin)
{
	ConditionalSwitchToBlit();
	id<MTLBlitCommandEncoder> Encoder = CurrentEncoder.GetBlitCommandEncoder();
	check(Encoder);
	
	[Encoder copyFromTexture:Texture sourceSlice:sourceSlice sourceLevel:sourceLevel sourceOrigin:sourceOrigin sourceSize:sourceSize toTexture:toTexture destinationSlice:destinationSlice destinationLevel:destinationLevel destinationOrigin:destinationOrigin];
	ConditionalSubmit();
}

void FMetalRenderPass::CopyFromBufferToBuffer(id<MTLBuffer> SourceBuffer, NSUInteger SourceOffset, id<MTLBuffer> DestinationBuffer, NSUInteger DestinationOffset, NSUInteger Size)
{
	ConditionalSwitchToBlit();
	id<MTLBlitCommandEncoder> Encoder = CurrentEncoder.GetBlitCommandEncoder();
	check(Encoder);
	
	[Encoder copyFromBuffer:SourceBuffer sourceOffset:SourceOffset toBuffer:DestinationBuffer destinationOffset:DestinationOffset size:Size];
	 ConditionalSubmit();
}

void FMetalRenderPass::PresentTexture(id<MTLTexture> Texture, uint32 sourceSlice, uint32 sourceLevel, MTLOrigin sourceOrigin, MTLSize sourceSize, id<MTLTexture> toTexture, uint32 destinationSlice, uint32 destinationLevel, MTLOrigin destinationOrigin)
{
	ConditionalSwitchToBlit();
	id<MTLBlitCommandEncoder> Encoder = CurrentEncoder.GetBlitCommandEncoder();
	check(Encoder);
	
	[Encoder copyFromTexture:Texture sourceSlice:sourceSlice sourceLevel:sourceLevel sourceOrigin:sourceOrigin sourceSize:sourceSize toTexture:toTexture destinationSlice:destinationSlice destinationLevel:destinationLevel destinationOrigin:destinationOrigin];
}

void FMetalRenderPass::SynchronizeTexture(id<MTLTexture> Texture, uint32 Slice, uint32 Level)
{
	check(Texture);
#if PLATFORM_MAC
	ConditionalSwitchToBlit();
	id<MTLBlitCommandEncoder> Encoder = CurrentEncoder.GetBlitCommandEncoder();
	check(Encoder);
	
	[Encoder synchronizeTexture:Texture slice:Slice level:Level];
	ConditionalSubmit();
#endif
}

void FMetalRenderPass::SynchroniseResource(id<MTLResource> Resource)
{
	check(Resource);
#if PLATFORM_MAC
	ConditionalSwitchToBlit();
	id<MTLBlitCommandEncoder> Encoder = CurrentEncoder.GetBlitCommandEncoder();
	check(Encoder);
	
	[Encoder synchronizeResource:Resource];
	ConditionalSubmit();
#endif
}

void FMetalRenderPass::FillBuffer(id<MTLBuffer> Buffer, NSRange Range, uint8 Value)
{
	check(Buffer);
	
	ConditionalSwitchToBlit();
	id<MTLBlitCommandEncoder> Encoder = CurrentEncoder.GetBlitCommandEncoder();
	check(Encoder);
	
	[Encoder fillBuffer:Buffer range:Range value:Value];
	ConditionalSubmit();
}

void FMetalRenderPass::AsyncCopyFromBufferToTexture(id<MTLBuffer> Buffer, uint32 sourceOffset, uint32 sourceBytesPerRow, uint32 sourceBytesPerImage, MTLSize sourceSize, id<MTLTexture> toTexture, uint32 destinationSlice, uint32 destinationLevel, MTLOrigin destinationOrigin)
{
	ConditionalSwitchToAsyncBlit();
	id<MTLBlitCommandEncoder> Encoder = PrologueEncoder.GetBlitCommandEncoder();
	check(Encoder);
	
	[Encoder copyFromBuffer:Buffer sourceOffset:sourceOffset sourceBytesPerRow:sourceBytesPerRow sourceBytesPerImage:sourceBytesPerImage sourceSize:sourceSize toTexture:toTexture destinationSlice:destinationSlice destinationLevel:destinationLevel destinationOrigin:destinationOrigin];
}

void FMetalRenderPass::AsyncCopyFromTextureToTexture(id<MTLTexture> Texture, uint32 sourceSlice, uint32 sourceLevel, MTLOrigin sourceOrigin, MTLSize sourceSize, id<MTLTexture> toTexture, uint32 destinationSlice, uint32 destinationLevel, MTLOrigin destinationOrigin)
{
	ConditionalSwitchToAsyncBlit();
	id<MTLBlitCommandEncoder> Encoder = PrologueEncoder.GetBlitCommandEncoder();
	check(Encoder);
	
	[Encoder copyFromTexture:Texture sourceSlice:sourceSlice sourceLevel:sourceLevel sourceOrigin:sourceOrigin sourceSize:sourceSize toTexture:toTexture destinationSlice:destinationSlice destinationLevel:destinationLevel destinationOrigin:destinationOrigin];
}

void FMetalRenderPass::AsyncGenerateMipmapsForTexture(id<MTLTexture> Texture)
{
	ConditionalSwitchToAsyncBlit();
	id<MTLBlitCommandEncoder> Encoder = PrologueEncoder.GetBlitCommandEncoder();
	check(Encoder);
	
	[Encoder generateMipmapsForTexture:Texture];
}

id<MTLFence> FMetalRenderPass::End(void)
{
	// EndEncoding should provide the encoder fence...
	if (PrologueEncoder.IsBlitCommandEncoderActive() || PrologueEncoder.IsComputeCommandEncoderActive())
	{
		PrologueEncoderFence = PrologueEncoder.EndEncoding();
	}
	if (CurrentEncoder.IsRenderCommandEncoderActive() || CurrentEncoder.IsBlitCommandEncoderActive() || CurrentEncoder.IsComputeCommandEncoderActive())
	{
		CurrentEncoderFence = CurrentEncoder.EndEncoding();
	}
	
	State.SetRenderTargetsActive(false);
	
	PassStartFence.Reset();
	
	RenderPassDesc = nil;
	bWithinRenderPass = false;
	
	return CurrentEncoderFence;
}

void FMetalRenderPass::InsertCommandBufferFence(FMetalCommandBufferFence& Fence, MTLCommandBufferHandler Handler)
{
	CurrentEncoder.InsertCommandBufferFence(Fence, Handler);
}

void FMetalRenderPass::AddCompletionHandler(MTLCommandBufferHandler Handler)
{
	CurrentEncoder.AddCompletionHandler(Handler);
}

void FMetalRenderPass::AddAsyncCommandBufferHandlers(MTLCommandBufferHandler Scheduled, MTLCommandBufferHandler Completion)
{
	check(PrologueEncoder.GetCommandBuffer() && PrologueEncoder.IsBlitCommandEncoderActive());
	if (Scheduled)
	{
		[PrologueEncoder.GetCommandBuffer() addScheduledHandler:Scheduled];
	}
	if (Completion)
	{
		PrologueEncoder.AddCompletionHandler(Completion);
	}
}

#pragma mark - Public Debug Support -
	
void FMetalRenderPass::InsertDebugSignpost(NSString* const String)
{
	CurrentEncoder.InsertDebugSignpost(String);
	PrologueEncoder.InsertDebugSignpost(String);
}

void FMetalRenderPass::PushDebugGroup(NSString* const String)
{
	CurrentEncoder.PushDebugGroup(String);
	PrologueEncoder.PushDebugGroup(String);
}

void FMetalRenderPass::PopDebugGroup(void)
{
	CurrentEncoder.PopDebugGroup();
	PrologueEncoder.PopDebugGroup();
}

#pragma mark - Public Accessors -
	
id<MTLCommandBuffer> FMetalRenderPass::GetCurrentCommandBuffer(void) const
{
	return CurrentEncoder.GetCommandBuffer();
}
	
FRingBuffer& FMetalRenderPass::GetRingBuffer(void)
{
	return CurrentEncoder.GetRingBuffer();
}

void FMetalRenderPass::ConditionalSwitchToRender(void)
{
	check(bWithinRenderPass);
	check(RenderPassDesc);
	check(CurrentEncoder.GetCommandBuffer());
	
	if (CurrentEncoder.IsComputeCommandEncoderActive() || CurrentEncoder.IsBlitCommandEncoderActive())
	{
		CurrentEncoderFence = CurrentEncoder.EndEncoding();
	}
	
	if (!CurrentEncoder.IsRenderCommandEncoderActive())
	{
		State.SetStateDirty();
		RestartRenderPass(nil);
	}
	
	check(CurrentEncoder.IsRenderCommandEncoderActive());
}

void FMetalRenderPass::ConditionalSwitchToTessellation(void)
{
	check(bWithinRenderPass);
	check(RenderPassDesc);
	check(CurrentEncoder.GetCommandBuffer());
	
	if (PrologueEncoder.IsBlitCommandEncoderActive())
	{
		PrologueEncoderFence = PrologueEncoder.EndEncoding();
	}
	
	if (CurrentEncoder.IsComputeCommandEncoderActive() || CurrentEncoder.IsBlitCommandEncoderActive())
	{
		CurrentEncoderFence = CurrentEncoder.EndEncoding();
	}
	
	bool const bCreatePrologueEncoder = !PrologueEncoder.IsComputeCommandEncoderActive();
	if (bCreatePrologueEncoder)
	{
		State.SetStateDirty();
		if (!PrologueEncoder.GetCommandBuffer())
		{
			PrologueEncoder.StartCommandBuffer();
		}
		PrologueEncoder.BeginComputeCommandEncoding();
		if (PrologueEncoderFence)
		{
			PrologueEncoder.WaitForFence(PrologueEncoderFence);
		}
		if (CurrentEncoderFence)
		{
			PrologueEncoder.WaitForFence(CurrentEncoderFence);
		}
		PrologueEncoder.WaitForFence(PassStartFence);
		PrologueEncoderFence = PrologueEncoder.GetEncoderFence();
#if METAL_DEBUG_OPTIONS
		if (GEmitDrawEvents)
		{
			(*PrologueEncoderFence).label = [NSString stringWithFormat:@"Prologue %@", (*PrologueEncoderFence).label];
		}
#endif
	}
	
	if (!CurrentEncoder.IsRenderCommandEncoderActive())
	{
		State.SetStateDirty();
		RestartRenderPass(nil);
	}
	else if (bCreatePrologueEncoder)
	{
		CurrentEncoder.WaitForFence(PrologueEncoderFence);
	}
	
	check(CurrentEncoder.IsRenderCommandEncoderActive());
	check(PrologueEncoder.IsComputeCommandEncoderActive());
}

void FMetalRenderPass::ConditionalSwitchToCompute(void)
{
	check(CurrentEncoder.GetCommandBuffer());
	
	if (CurrentEncoder.IsRenderCommandEncoderActive() || CurrentEncoder.IsBlitCommandEncoderActive())
	{
		if (CurrentEncoder.IsRenderCommandEncoderActive())
		{
			State.SetRenderStoreActions(CurrentEncoder, true);
		}
		CurrentEncoderFence = CurrentEncoder.EndEncoding();
		State.SetRenderTargetsActive(false);
	}
	
	if (!CurrentEncoder.IsComputeCommandEncoderActive())
	{
		State.SetStateDirty();
		CurrentEncoder.BeginComputeCommandEncoding();
		if (CurrentEncoderFence)
		{
			CurrentEncoder.WaitForFence(CurrentEncoderFence);
		}
		if (PrologueEncoderFence)
		{
			CurrentEncoder.WaitForFence(PrologueEncoderFence);
		}
	}
	
	check(CurrentEncoder.IsComputeCommandEncoderActive());
}

void FMetalRenderPass::ConditionalSwitchToBlit(void)
{
	check(CurrentEncoder.GetCommandBuffer());
	
	if (CurrentEncoder.IsRenderCommandEncoderActive() || CurrentEncoder.IsComputeCommandEncoderActive())
	{
		if (CurrentEncoder.IsRenderCommandEncoderActive())
		{
			State.SetRenderStoreActions(CurrentEncoder, true);
		}
		CurrentEncoderFence = CurrentEncoder.EndEncoding();
		State.SetRenderTargetsActive(false);
	}
	
	if (!CurrentEncoder.IsBlitCommandEncoderActive())
	{
		CurrentEncoder.BeginBlitCommandEncoding();
		if (CurrentEncoderFence)
		{
			CurrentEncoder.WaitForFence(CurrentEncoderFence);
		}
		if (PrologueEncoderFence)
		{
			CurrentEncoder.WaitForFence(PrologueEncoderFence);
		}
	}
	
	check(CurrentEncoder.IsBlitCommandEncoderActive());
}

void FMetalRenderPass::ConditionalSwitchToAsyncBlit(void)
{
	if (PrologueEncoder.IsComputeCommandEncoderActive())
	{
		PrologueEncoderFence = PrologueEncoder.EndEncoding();
	}
	
	if (!PrologueEncoder.IsBlitCommandEncoderActive())
	{
		if (!PrologueEncoder.GetCommandBuffer())
		{
			PrologueEncoder.StartCommandBuffer();
		}
		PrologueEncoder.BeginBlitCommandEncoding();
		if (PrologueEncoderFence)
		{
			PrologueEncoder.WaitForFence(PrologueEncoderFence);
		}
		if (CurrentEncoderFence)
		{
			PrologueEncoder.WaitForFence(CurrentEncoderFence);
		}
		if (PassStartFence)
		{
			PrologueEncoder.WaitForFence(PassStartFence);
		}
		PrologueEncoderFence = PrologueEncoder.GetEncoderFence();
#if METAL_DEBUG_OPTIONS
		if (GEmitDrawEvents)
		{
			(*PrologueEncoderFence).label = [NSString stringWithFormat:@"Prologue %@", (*PrologueEncoderFence).label];
		}
#endif
		
		if (CurrentEncoder.IsRenderCommandEncoderActive() || CurrentEncoder.IsComputeCommandEncoderActive() || CurrentEncoder.IsBlitCommandEncoderActive())
		{
			CurrentEncoder.WaitForFence(PrologueEncoderFence);
		}
	}
	
	check(PrologueEncoder.IsBlitCommandEncoderActive());
}

void FMetalRenderPass::CommitRenderResourceTables(void)
{
	State.CommitRenderResources(&CurrentEncoder);
	
	State.CommitResourceTable(SF_Vertex, MTLFunctionTypeVertex, CurrentEncoder);
	
	FMetalGraphicsPipelineState const* BoundShaderState = State.GetGraphicsPSO();
	
	if (BoundShaderState->VertexShader->SideTableBinding >= 0)
	{
		CurrentEncoder.SetShaderSideTable(MTLFunctionTypeVertex, BoundShaderState->VertexShader->SideTableBinding);
		State.SetShaderBuffer(SF_Vertex, nil, nil, 0, 0, BoundShaderState->VertexShader->SideTableBinding);
	}
	
	if (IsValidRef(BoundShaderState->PixelShader))
	{
		State.CommitResourceTable(SF_Pixel, MTLFunctionTypeFragment, CurrentEncoder);
		if (BoundShaderState->PixelShader->SideTableBinding >= 0)
		{
			CurrentEncoder.SetShaderSideTable(MTLFunctionTypeFragment, BoundShaderState->PixelShader->SideTableBinding);
			State.SetShaderBuffer(SF_Pixel, nil, nil, 0, 0, BoundShaderState->PixelShader->SideTableBinding);
		}
	}
}

void FMetalRenderPass::CommitTessellationResourceTables(void)
{
	State.CommitTessellationResources(&CurrentEncoder, &PrologueEncoder);
	
	State.CommitResourceTable(SF_Vertex, MTLFunctionTypeKernel, PrologueEncoder);
	
	State.CommitResourceTable(SF_Hull, MTLFunctionTypeKernel, PrologueEncoder);
	
	State.CommitResourceTable(SF_Domain, MTLFunctionTypeVertex, CurrentEncoder);
	
	TRefCountPtr<FMetalGraphicsPipelineState> CurrentBoundShaderState = State.GetGraphicsPSO();
	if (IsValidRef(CurrentBoundShaderState->PixelShader))
	{
		State.CommitResourceTable(SF_Pixel, MTLFunctionTypeFragment, CurrentEncoder);
	}
}

void FMetalRenderPass::CommitDispatchResourceTables(void)
{
	State.CommitComputeResources(&CurrentEncoder);
	
	State.CommitResourceTable(SF_Compute, MTLFunctionTypeKernel, CurrentEncoder);
	
	FMetalComputeShader const* ComputeShader = State.GetComputeShader();
	if (ComputeShader->SideTableBinding >= 0)
	{
		CurrentEncoder.SetShaderSideTable(MTLFunctionTypeKernel, ComputeShader->SideTableBinding);
		State.SetShaderBuffer(SF_Compute, nil, nil, 0, 0, ComputeShader->SideTableBinding);
	}
}

void FMetalRenderPass::PrepareToRender(uint32 PrimitiveType)
{
	check(CurrentEncoder.GetCommandBuffer());
	check(CurrentEncoder.IsRenderCommandEncoderActive());
	
	// Set raster state
	State.SetRenderState(CurrentEncoder, nullptr);
	
	// Bind shader resources
	CommitRenderResourceTables();
}

void FMetalRenderPass::PrepareToTessellate(uint32 PrimitiveType)
{
	check(CurrentEncoder.GetCommandBuffer());
	check(PrologueEncoder.GetCommandBuffer());
	check(CurrentEncoder.IsRenderCommandEncoderActive());
	check(PrologueEncoder.IsComputeCommandEncoderActive());
	
	// Set raster state
	State.SetRenderState(CurrentEncoder, &PrologueEncoder);
	
	// Bind shader resources
	CommitTessellationResourceTables();
}

void FMetalRenderPass::PrepareToDispatch(void)
{
	check(CurrentEncoder.GetCommandBuffer());
	check(CurrentEncoder.IsComputeCommandEncoderActive());
	
	TRefCountPtr<FMetalComputeShader> ComputeShader = State.GetComputeShader();
	check(ComputeShader);

	CurrentEncoder.SetComputePipelineState(ComputeShader->Pipeline);
	
	// Bind shader resources
	CommitDispatchResourceTables();
}

void FMetalRenderPass::ConditionalSubmit()
{
	NumOutstandingOps++;
	
	bool bCanForceSubmit = State.CanRestartRenderPass();

#if METAL_DEBUG_OPTIONS
	FRHISetRenderTargetsInfo CurrentRenderTargets = State.GetRenderTargetsInfo();
	
	// Force a command-encoder when GMetalRuntimeDebugLevel is enabled to help track down intermittent command-buffer failures.
	if (GMetalCommandBufferCommitThreshold > 0 && NumOutstandingOps >= GMetalCommandBufferCommitThreshold && CmdList.GetCommandQueue().GetRuntimeDebuggingLevel() >= EMetalDebugLevelConditionalSubmit)
	{
		bool bCanChangeRT = true;
		
		if (bWithinRenderPass)
		{
			const bool bIsMSAAActive = State.GetHasValidRenderTarget() && State.GetSampleCount() != 1;
			bCanChangeRT = !bIsMSAAActive;
			
			for (int32 RenderTargetIndex = 0; bCanChangeRT && RenderTargetIndex < CurrentRenderTargets.NumColorRenderTargets; RenderTargetIndex++)
			{
				FRHIRenderTargetView& RenderTargetView = CurrentRenderTargets.ColorRenderTarget[RenderTargetIndex];
				
				if (RenderTargetView.StoreAction != ERenderTargetStoreAction::EMultisampleResolve)
				{
					RenderTargetView.LoadAction = ERenderTargetLoadAction::ELoad;
					RenderTargetView.StoreAction = ERenderTargetStoreAction::EStore;
				}
				else
				{
					bCanChangeRT = false;
				}
			}
			
			if (bCanChangeRT && CurrentRenderTargets.DepthStencilRenderTarget.Texture)
			{
				if (CurrentRenderTargets.DepthStencilRenderTarget.DepthStoreAction != ERenderTargetStoreAction::EMultisampleResolve && CurrentRenderTargets.DepthStencilRenderTarget.GetStencilStoreAction() != ERenderTargetStoreAction::EMultisampleResolve)
				{
					CurrentRenderTargets.DepthStencilRenderTarget = FRHIDepthRenderTargetView(CurrentRenderTargets.DepthStencilRenderTarget.Texture, ERenderTargetLoadAction::ELoad, ERenderTargetStoreAction::EStore);
				}
				else
				{
					bCanChangeRT = false;
				}
			}
		}
		
		bCanForceSubmit = bCanChangeRT;
	}
#endif
	
	if (GMetalCommandBufferCommitThreshold > 0 && NumOutstandingOps > 0 && NumOutstandingOps >= GMetalCommandBufferCommitThreshold && bCanForceSubmit)
	{
		if (CurrentEncoder.GetCommandBuffer())
		{
			Submit(EMetalSubmitFlagsCreateCommandBuffer);
			NumOutstandingOps = 0;
		}
		
#if METAL_DEBUG_OPTIONS
		// Force a command-encoder when GMetalRuntimeDebugLevel is enabled to help track down intermittent command-buffer failures.
		if (bWithinRenderPass && CmdList.GetCommandQueue().GetRuntimeDebuggingLevel() >= EMetalDebugLevelConditionalSubmit)
		{
			bool bSet = false;
			State.InvalidateRenderTargets();
			if (IsFeatureLevelSupported( GMaxRHIShaderPlatform, ERHIFeatureLevel::SM4 ))
			{
				bSet = State.SetRenderTargetsInfo(CurrentRenderTargets, State.GetVisibilityResultsBuffer(), false);
			}
			else
			{
				bSet = State.SetRenderTargetsInfo(CurrentRenderTargets, NULL, false);
			}
			
			if (bSet)
			{
				RestartRenderPass(State.GetRenderPassDescriptor());
			}
		}
#endif
	}
}
