// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
D3D12Commands.cpp: D3D RHI commands implementation.
=============================================================================*/

#include "D3D12RHIPrivate.h"
#include "StaticBoundShaderState.h"
#include "GlobalShader.h"
#include "OneColorShader.h"
#include "RHICommandList.h"
#include "RHIStaticStates.h"
#include "ShaderParameterUtils.h"
#include "ShaderCompiler.h"
#include "ScreenRendering.h"
#include "ResolveShader.h"
#include "SceneUtils.h"

int32 AFRSyncTemporalResources = 1;
static FAutoConsoleVariableRef CVarSyncTemporalResources(
	TEXT("D3D12.AFRSyncTemporalResources"),
	AFRSyncTemporalResources,
	TEXT("Synchronize inter-frame dependencies between GPUs"),
	ECVF_RenderThreadSafe
	);

using namespace D3D12RHI;

#define DECLARE_ISBOUNDSHADER(ShaderType) inline void ValidateBoundShader(FD3D12StateCache& InStateCache, F##ShaderType##RHIParamRef ShaderType##RHI) \
{ \
	FD3D12##ShaderType* CachedShader; \
	InStateCache.Get##ShaderType(&CachedShader); \
	FD3D12##ShaderType* ShaderType = FD3D12DynamicRHI::ResourceCast(ShaderType##RHI); \
	ensureMsgf(CachedShader == ShaderType, TEXT("Parameters are being set for a %s which is not currently bound"), TEXT( #ShaderType )); \
}

DECLARE_ISBOUNDSHADER(VertexShader)
DECLARE_ISBOUNDSHADER(PixelShader)
DECLARE_ISBOUNDSHADER(GeometryShader)
DECLARE_ISBOUNDSHADER(HullShader)
DECLARE_ISBOUNDSHADER(DomainShader)
DECLARE_ISBOUNDSHADER(ComputeShader)

#if EXECUTE_DEBUG_COMMAND_LISTS
bool GIsDoingQuery = false;
#endif

#if DO_CHECK
#define VALIDATE_BOUND_SHADER(s) ValidateBoundShader(StateCache, s)
#else
#define VALIDATE_BOUND_SHADER(s)
#endif

void FD3D12DynamicRHI::SetupRecursiveResources()
{
	auto ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);

	{
		TShaderMapRef<FLongGPUTaskPS> PixelShader(ShaderMap);
		PixelShader->GetPixelShader();
	}

	extern ENGINE_API TGlobalResource<FScreenVertexDeclaration> GScreenVertexDeclaration;

	{
		TShaderMapRef<FLongGPUTaskPS> PixelShader(ShaderMap);
		PixelShader->GetPixelShader();
	}

	// TODO: Waiting to integrate MSAA fix for ResolveShader.h
	if (GMaxRHIShaderPlatform == SP_XBOXONE_D3D12)
		return;

	TShaderMapRef<FResolveVS> ResolveVertexShader(ShaderMap);
	if (GMaxRHIShaderPlatform == SP_PCD3D_SM5 || GMaxRHIShaderPlatform == SP_XBOXONE_D3D12)
	{
		TShaderMapRef<FResolveDepthPS> ResolvePixelShader_Depth(ShaderMap);
		ResolvePixelShader_Depth->GetPixelShader();

		TShaderMapRef<FResolveDepthPS> ResolvePixelShader_SingleSample(ShaderMap);
		ResolvePixelShader_SingleSample->GetPixelShader();
	}
	else
	{
		TShaderMapRef<FResolveDepthNonMSPS> ResolvePixelShader_DepthNonMS(ShaderMap);
		ResolvePixelShader_DepthNonMS->GetPixelShader();
	}
}

// Vertex state.
void FD3D12CommandContext::RHISetStreamSource(uint32 StreamIndex, FVertexBufferRHIParamRef VertexBufferRHI, uint32 Stride, uint32 Offset)
{
	FD3D12VertexBuffer* VertexBuffer = RetrieveObject<FD3D12VertexBuffer>(VertexBufferRHI);

	StateCache.SetStreamSource(VertexBuffer ? &VertexBuffer->ResourceLocation : nullptr, StreamIndex, Stride, Offset);
}

void FD3D12CommandContext::RHISetStreamSource(uint32 StreamIndex, FVertexBufferRHIParamRef VertexBufferRHI, uint32 Offset)
{
	FD3D12VertexBuffer* VertexBuffer = RetrieveObject<FD3D12VertexBuffer>(VertexBufferRHI);

	StateCache.SetStreamSource(VertexBuffer ? &VertexBuffer->ResourceLocation : nullptr, StreamIndex, Offset);
}

// Stream-Out state.
void FD3D12DynamicRHI::RHISetStreamOutTargets(uint32 NumTargets, const FVertexBufferRHIParamRef* VertexBuffers, const uint32* Offsets)
{
	FD3D12CommandContext& CmdContext = GetRHIDevice()->GetDefaultCommandContext();
	FD3D12Resource* D3DVertexBuffers[D3D12_SO_BUFFER_SLOT_COUNT] = { 0 };
	uint32 D3DOffsets[D3D12_SO_BUFFER_SLOT_COUNT] = { 0 };

	if (VertexBuffers)
	{
		for (uint32 BufferIndex = 0; BufferIndex < NumTargets; BufferIndex++)
		{
			const FD3D12VertexBuffer* VB = ((FD3D12VertexBuffer*)VertexBuffers[BufferIndex]);
			if (VB != nullptr)
			{
				D3DVertexBuffers[BufferIndex] = VB->ResourceLocation.GetResource();
				D3DOffsets[BufferIndex] = VB->ResourceLocation.GetOffsetFromBaseOfResource();
			}
			else
			{
				D3DVertexBuffers[BufferIndex] = nullptr;
				D3DOffsets[BufferIndex] = 0;
			}
		}
	}

	CmdContext.StateCache.SetStreamOutTargets(NumTargets, D3DVertexBuffers, D3DOffsets);
}

// Rasterizer state.
void FD3D12CommandContext::RHISetRasterizerState(FRasterizerStateRHIParamRef NewStateRHI)
{
	FD3D12RasterizerState* NewState = FD3D12DynamicRHI::ResourceCast(NewStateRHI);
	StateCache.SetRasterizerState(&NewState->Desc);
}

void FD3D12CommandContext::RHISetComputeShader(FComputeShaderRHIParamRef ComputeShaderRHI)
{
	FD3D12ComputeShader* ComputeShader = FD3D12DynamicRHI::ResourceCast(ComputeShaderRHI);
	StateCache.SetComputeShader(ComputeShader);
}

void FD3D12CommandContext::RHIWaitComputeFence(FComputeFenceRHIParamRef InFenceRHI)
{
	FD3D12Fence* Fence = FD3D12DynamicRHI::ResourceCast(InFenceRHI);

	if (Fence)
	{
		check(IsDefaultContext());
		RHISubmitCommandsHint();

		checkf(Fence->GetWriteEnqueued(), TEXT("ComputeFence: %s waited on before being written. This will hang the GPU."), *Fence->GetName().ToString());
		Fence->GpuWait(GetCommandListManager().GetD3DCommandQueue(), Fence->GetLastSignaledFence());
	}
}

void FD3D12CommandContext::RHIDispatchComputeShader(uint32 ThreadGroupCountX, uint32 ThreadGroupCountY, uint32 ThreadGroupCountZ)
{
	FD3D12ComputeShader* ComputeShader = nullptr;
	StateCache.GetComputeShader(&ComputeShader);

	if (IsDefaultContext())
	{
		GetParentDevice()->RegisterGPUWork(1);
	}

	if (ComputeShader->ResourceCounts.bGlobalUniformBufferUsed)
	{
		CommitComputeShaderConstants();
	}
	CommitComputeResourceTables(ComputeShader);
	StateCache.ApplyState<true>();

	numDispatches++;
	CommandListHandle->Dispatch(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);

	DEBUG_EXECUTE_COMMAND_LIST(this);
}

void FD3D12CommandContext::RHIDispatchIndirectComputeShader(FVertexBufferRHIParamRef ArgumentBufferRHI, uint32 ArgumentOffset)
{
	FD3D12VertexBuffer* ArgumentBuffer = FD3D12DynamicRHI::ResourceCast(ArgumentBufferRHI);

	if (IsDefaultContext())
	{
		GetParentDevice()->RegisterGPUWork(1);
	}

	FD3D12ComputeShader* ComputeShader = nullptr;
	StateCache.GetComputeShader(&ComputeShader);

	if (ComputeShader->ResourceCounts.bGlobalUniformBufferUsed)
	{
		CommitComputeShaderConstants();
	}
	CommitComputeResourceTables(ComputeShader);

	FD3D12ResourceLocation& Location = ArgumentBuffer->ResourceLocation;
	FD3D12DynamicRHI::TransitionResource(CommandListHandle, Location.GetResource(), D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);

	StateCache.ApplyState<true>();

	numDispatches++;
	CommandListHandle->ExecuteIndirect(
		GetParentDevice()->GetParentAdapter()->GetDispatchIndirectCommandSignature(),
		1,
		Location.GetResource()->GetResource(),
		Location.GetOffsetFromBaseOfResource() + ArgumentOffset,
		NULL,
		0
		);
	CommandListHandle.UpdateResidency(Location.GetResource());

	DEBUG_EXECUTE_COMMAND_LIST(this);
}


void FD3D12CommandContext::RHITransitionResources(EResourceTransitionAccess TransitionType, FTextureRHIParamRef* InTextures, int32 NumTextures)
{
#if !USE_D3D12RHI_RESOURCE_STATE_TRACKING
	// TODO: Make sure that EMetaData is supported with an aliasing barrier, otherwise the CMask decal optimisation will break.
	check(TransitionType != EResourceTransitionAccess::EMetaData && (TransitionType == EResourceTransitionAccess::EReadable || TransitionType == EResourceTransitionAccess::EWritable || TransitionType == EResourceTransitionAccess::ERWSubResBarrier));
	// TODO: Remove this skip.
	// Skip for now because we don't have enough info about what mip to transition yet.
	// Note: This causes visual corruption.
	if (TransitionType == EResourceTransitionAccess::ERWSubResBarrier)
	{
		return;
	}

	static IConsoleVariable* CVarShowTransitions = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ProfileGPU.ShowTransitions"));
	const bool bShowTransitionEvents = CVarShowTransitions->GetInt() != 0;

	SCOPED_RHI_CONDITIONAL_DRAW_EVENTF(*this, RHITransitionResources, bShowTransitionEvents, TEXT("TransitionTo: %s: %i Textures"), *FResourceTransitionUtility::ResourceTransitionAccessStrings[(int32)TransitionType], NumTextures);

	// Determine the direction of the transitions.
	const D3D12_RESOURCE_STATES* pBefore = nullptr;
	const D3D12_RESOURCE_STATES* pAfter = nullptr;
	D3D12_RESOURCE_STATES WritableState;
	D3D12_RESOURCE_STATES ReadableState;
	switch (TransitionType)
	{
	case EResourceTransitionAccess::EReadable:
		// Write -> Read
		pBefore = &WritableState;
		pAfter = &ReadableState;
		break;

	case EResourceTransitionAccess::EWritable:
		// Read -> Write
		pBefore = &ReadableState;
		pAfter = &WritableState;
		break;

	default:
		check(false);
		break;
	}

	// Create the resource barrier descs for each texture to transition.
	for (int32 i = 0; i < NumTextures; ++i)
	{
		if (InTextures[i])
		{
			FD3D12Resource* Resource = RetrieveTextureBase(InTextures[i])->GetResource();
			check(Resource->RequiresResourceStateTracking());

			SCOPED_RHI_CONDITIONAL_DRAW_EVENTF(*this, RHITransitionResourcesLoop, bShowTransitionEvents, TEXT("To:%i - %s"), i, *Resource->GetName().ToString());

			WritableState = Resource->GetWritableState();
			ReadableState = Resource->GetReadableState();

			CommandListHandle.AddTransitionBarrier(Resource, *pBefore, *pAfter, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);

			DUMP_TRANSITION(Resource->GetName(), TransitionType);
		}
	}
#else
	if (TransitionType == EResourceTransitionAccess::EMetaData)
	{
		FlushMetadata(InTextures, NumTextures);
	}
#endif // !USE_D3D12RHI_RESOURCE_STATE_TRACKING
}


void FD3D12CommandContext::RHITransitionResources(EResourceTransitionAccess TransitionType, EResourceTransitionPipeline TransitionPipeline, FUnorderedAccessViewRHIParamRef* InUAVs, int32 InNumUAVs, FComputeFenceRHIParamRef WriteComputeFenceRHI)
{
	static IConsoleVariable* CVarShowTransitions = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ProfileGPU.ShowTransitions"));
	const bool bShowTransitionEvents = CVarShowTransitions->GetInt() != 0;

	SCOPED_RHI_CONDITIONAL_DRAW_EVENTF(*this, RHITransitionResources, bShowTransitionEvents, TEXT("TransitionTo: %s: %i UAVs"), *FResourceTransitionUtility::ResourceTransitionAccessStrings[(int32)TransitionType], InNumUAVs);
	const bool bTransitionBetweenShaderStages = (TransitionPipeline == EResourceTransitionPipeline::EGfxToCompute) || (TransitionPipeline == EResourceTransitionPipeline::EComputeToGfx);
	const bool bUAVTransition = (TransitionType == EResourceTransitionAccess::EReadable) || (TransitionType == EResourceTransitionAccess::EWritable || TransitionType == EResourceTransitionAccess::ERWBarrier);
	
	// When transitioning between shader stage usage, we can avoid a UAV barrier as an optimization if the resource will be transitioned to a different resource state anyway (E.g RT -> UAV).
	// That being said, there is a danger when going from UAV usage on one stage (E.g. Pixel Shader UAV) to UAV usage on another stage (E.g. Compute Shader UAV), 
	// IFF the 2nd UAV usage relies on the output of the 1st. That would require a UAV barrier since the D3D12 RHI state tracking system would optimize that transition out.
	// The safest option is to always do a UAV barrier when ERWBarrier is passed in. However there is currently no usage like this so we're ok for now. 
	const bool bUAVBarrier = (TransitionType == EResourceTransitionAccess::ERWBarrier && !bTransitionBetweenShaderStages);

	if (bUAVBarrier)
	{
		// UAV barrier between Dispatch() calls to ensure all R/W accesses are complete.
		StateCache.FlushComputeShaderCache(true);
	}
	else if (bUAVTransition)
	{
		// We do a special transition now when called with a particular set of parameters (ERWBarrier && EGfxToCompute) as an optimization when the engine wants to use uavs on the async compute queue.
		// This will transition all specifed UAVs to the UAV state on the 3D queue to avoid stalling the compute queue with pending resource state transitions later.
		if ((TransitionType == EResourceTransitionAccess::ERWBarrier) && (TransitionPipeline == EResourceTransitionPipeline::EGfxToCompute))
		{
			// The 3D queue can safely transition resources to the UAV state, regardless of their current state (RT, SRV, etc.). However the compute queue is limited in what states 
			// it can transition to/from, so we limit this transition logic to only happen when going from Gfx -> Compute. (E.g. The compute queue cannot transition to/from RT, Pixel Shader SRV, etc.).
			for (int32 i = 0; i < InNumUAVs; ++i)
			{
				if (InUAVs[i])
				{
					FD3D12UnorderedAccessView* const UnorderedAccessView = RetrieveObject<FD3D12UnorderedAccessView>(InUAVs[i]);
					FD3D12DynamicRHI::TransitionResource(CommandListHandle, UnorderedAccessView, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				}
			}
		}
		else
		{
#if !USE_D3D12RHI_RESOURCE_STATE_TRACKING
			// Determine the direction of the transitions.
			// Note in this method, the writeable state is always UAV, regardless of the FD3D12Resource's Writeable state.
			const D3D12_RESOURCE_STATES* pBefore = nullptr;
			const D3D12_RESOURCE_STATES* pAfter = nullptr;
			const D3D12_RESOURCE_STATES WritableComputeState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
			D3D12_RESOURCE_STATES WritableGraphicsState;
			D3D12_RESOURCE_STATES ReadableState;
			switch (TransitionType)
			{
			case EResourceTransitionAccess::EReadable:
				// Write -> Read
				pBefore = &WritableComputeState;
				pAfter = &ReadableState;
				break;

			case EResourceTransitionAccess::EWritable:
				// Read -> Write
				pBefore = &ReadableState;
				pAfter = &WritableComputeState;
				break;

			case EResourceTransitionAccess::ERWBarrier:
				// Write -> Write, but switching from Grfx to Compute.
				check(TransitionPipeline == EResourceTransitionPipeline::EGfxToCompute);
				pBefore = &WritableGraphicsState;
				pAfter = &WritableComputeState;
				break;

			default:
				check(false);
				break;
			}

			// Create the resource barrier descs for each texture to transition.
			for (int32 i = 0; i < InNumUAVs; ++i)
			{
				if (InUAVs[i])
				{
					FD3D12UnorderedAccessView* UnorderedAccessView = RetrieveObject<FD3D12UnorderedAccessView>(InUAVs[i]);
					FD3D12Resource* Resource = UnorderedAccessView->GetResource();
					check(Resource->RequiresResourceStateTracking());

					SCOPED_RHI_CONDITIONAL_DRAW_EVENTF(*this, RHITransitionResourcesLoop, bShowTransitionEvents, TEXT("To:%i - %s"), i, *Resource->GetName().ToString());

					// The writable compute state is always UAV.
					WritableGraphicsState = Resource->GetWritableState();
					ReadableState = Resource->GetReadableState();

					// Some ERWBarriers might have the same before and after states.
					if (*pBefore != *pAfter)
					{
						CommandListHandle.AddTransitionBarrier(Resource, *pBefore, *pAfter, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);

						DUMP_TRANSITION(Resource->GetName(), TransitionType);
					}
				}
			}
#endif // !USE_D3D12RHI_RESOURCE_STATE_TRACKING
		}
	}

	if (WriteComputeFenceRHI)
	{
		RHISubmitCommandsHint();

		FD3D12Fence* Fence = FD3D12DynamicRHI::ResourceCast(WriteComputeFenceRHI);
		Fence->WriteFence();

		Fence->Signal(GetCommandListManager().GetD3DCommandQueue());
	}
}

void FD3D12CommandContext::RHISetViewport(uint32 MinX, uint32 MinY, float MinZ, uint32 MaxX, uint32 MaxY, float MaxZ)
{
	// These are the maximum viewport extents for D3D12. Exceeding them leads to badness.
	check(MinX <= (uint32)D3D12_VIEWPORT_BOUNDS_MAX);
	check(MinY <= (uint32)D3D12_VIEWPORT_BOUNDS_MAX);
	check(MaxX <= (uint32)D3D12_VIEWPORT_BOUNDS_MAX);
	check(MaxY <= (uint32)D3D12_VIEWPORT_BOUNDS_MAX);

	D3D12_VIEWPORT Viewport = { (float)MinX, (float)MinY, (float)(MaxX - MinX), (float)(MaxY - MinY), MinZ, MaxZ };
	//avoid setting a 0 extent viewport, which the debug runtime doesn't like
	if (Viewport.Width > 0 && Viewport.Height > 0)
	{
		StateCache.SetViewport(Viewport);
		SetScissorRectIfRequiredWhenSettingViewport(MinX, MinY, MaxX, MaxY);
	}
}

void FD3D12CommandContext::RHISetScissorRect(bool bEnable, uint32 MinX, uint32 MinY, uint32 MaxX, uint32 MaxY)
{
	if (bEnable)
	{
		const CD3DX12_RECT ScissorRect(MinX, MinY, MaxX, MaxY);
		StateCache.SetScissorRect(ScissorRect);
	}
	else
	{
		const CD3DX12_RECT ScissorRect(0, 0, GetMax2DTextureDimension(), GetMax2DTextureDimension());
		StateCache.SetScissorRect(ScissorRect);
	}
}

/**
* Set bound shader state. This will set the vertex decl/shader, and pixel shader
* @param BoundShaderState - state resource
*/
void FD3D12CommandContext::RHISetBoundShaderState(FBoundShaderStateRHIParamRef BoundShaderStateRHI)
{
	//SCOPE_CYCLE_COUNTER(STAT_D3D12SetBoundShaderState);
	FD3D12BoundShaderState* const BoundShaderState = FD3D12DynamicRHI::ResourceCast(BoundShaderStateRHI);

	StateCache.SetBoundShaderState(BoundShaderState);
	CurrentBoundShaderState = BoundShaderState;

	// Prevent transient bound shader states from being recreated for each use by keeping a history of the most recently used bound shader states.
	// The history keeps them alive, and the bound shader state cache allows them to be reused if needed.
	BoundShaderStateHistory.Add(BoundShaderState);

	if (BoundShaderState->GetHullShader() && BoundShaderState->GetDomainShader())
	{
		bUsingTessellation = true;

		// Ensure the command buffers are reset to reduce the amount of data that needs to be versioned.
		HSConstantBuffer.Reset();
		DSConstantBuffer.Reset();
	}
	else
	{
		bUsingTessellation = false;
	}

	// @TODO : really should only discard the constants if the shader state has actually changed.
	bDiscardSharedConstants = true;

	// Ensure the command buffers are reset to reduce the amount of data that needs to be versioned.
	VSConstantBuffer.Reset();
	PSConstantBuffer.Reset();
	GSConstantBuffer.Reset();
	CSConstantBuffer.Reset();	// Should this be here or in RHISetComputeShader? Might need a new bDiscardSharedConstants for CS.
}

void FD3D12CommandContext::RHISetGraphicsPipelineState(FGraphicsPipelineStateRHIParamRef GraphicsState)
{
	FD3D12GraphicsPipelineState* GraphicsPipelineState = FD3D12DynamicRHI::ResourceCast(GraphicsState);

	auto& PsoInit = GraphicsPipelineState->PipelineStateInitializer;

	// TODO: [PSO API] Every thing inside this scope is only necessary to keep the PSO shadow in sync while we convert the high level to only use PSOs
	{
		TRenderTargetFormatsArray RenderTargetFormats;
		DXGI_FORMAT DepthStencilFormat = DXGI_FORMAT_UNKNOWN;
		uint32 NumTargets = PsoInit.ComputeNumValidRenderTargets();

		TranslateRenderTargetFormats(PsoInit, RenderTargetFormats, DepthStencilFormat);

		// Set the tracking cache to the PSO state we are about to set
		RHISetBoundShaderState(
			FD3D12DynamicRHI::ResourceCast(
				RHICreateBoundShaderState(
					PsoInit.BoundShaderState.VertexDeclarationRHI,
					PsoInit.BoundShaderState.VertexShaderRHI,
					PsoInit.BoundShaderState.HullShaderRHI,
					PsoInit.BoundShaderState.DomainShaderRHI,
					PsoInit.BoundShaderState.PixelShaderRHI,
					PsoInit.BoundShaderState.GeometryShaderRHI
				).GetReference()
			)
		);

		RHISetBlendState(PsoInit.BlendState, FLinearColor(1.0f, 1.0f, 1.0f));
		RHISetRasterizerState(PsoInit.RasterizerState);
		RHISetDepthStencilState(PsoInit.DepthStencilState, 0);

		StateCache.SetPrimitiveTopologyType(D3D12PrimitiveTypeToTopologyType(TranslatePrimitiveType(PsoInit.PrimitiveType)));
		StateCache.SetRenderDepthStencilTargetFormats(NumTargets, RenderTargetFormats, DepthStencilFormat, PsoInit.NumSamples);
	}

	// No need to build the PSO, this one is pre-built
	StateCache.CommitPendingGraphicsPipelineState();
	StateCache.SetPipelineState(GraphicsPipelineState->PipelineState);
}

void FD3D12CommandContext::RHISetShaderTexture(FVertexShaderRHIParamRef VertexShaderRHI, uint32 TextureIndex, FTextureRHIParamRef NewTextureRHI)
{
	VALIDATE_BOUND_SHADER(VertexShaderRHI);
	FD3D12TextureBase* const NewTexture = RetrieveTextureBase(NewTextureRHI);
	StateCache.SetShaderResourceView<SF_Vertex>(NewTexture ? NewTexture->GetShaderResourceView() : nullptr, TextureIndex);
}

void FD3D12CommandContext::RHISetShaderTexture(FHullShaderRHIParamRef HullShaderRHI, uint32 TextureIndex, FTextureRHIParamRef NewTextureRHI)
{
	VALIDATE_BOUND_SHADER(HullShaderRHI);
	FD3D12TextureBase* const NewTexture = RetrieveTextureBase(NewTextureRHI);
	StateCache.SetShaderResourceView<SF_Hull>(NewTexture ? NewTexture->GetShaderResourceView() : nullptr, TextureIndex);
}

void FD3D12CommandContext::RHISetShaderTexture(FDomainShaderRHIParamRef DomainShaderRHI, uint32 TextureIndex, FTextureRHIParamRef NewTextureRHI)
{
	VALIDATE_BOUND_SHADER(DomainShaderRHI);
	FD3D12TextureBase* const NewTexture = RetrieveTextureBase(NewTextureRHI);
	StateCache.SetShaderResourceView<SF_Domain>(NewTexture ? NewTexture->GetShaderResourceView() : nullptr, TextureIndex);
}

void FD3D12CommandContext::RHISetShaderTexture(FGeometryShaderRHIParamRef GeometryShaderRHI, uint32 TextureIndex, FTextureRHIParamRef NewTextureRHI)
{
	VALIDATE_BOUND_SHADER(GeometryShaderRHI);
	FD3D12TextureBase* const NewTexture = RetrieveTextureBase(NewTextureRHI);
	StateCache.SetShaderResourceView<SF_Geometry>(NewTexture ? NewTexture->GetShaderResourceView() : nullptr, TextureIndex);
}

void FD3D12CommandContext::RHISetShaderTexture(FPixelShaderRHIParamRef PixelShaderRHI, uint32 TextureIndex, FTextureRHIParamRef NewTextureRHI)
{
	VALIDATE_BOUND_SHADER(PixelShaderRHI);
	FD3D12TextureBase* const NewTexture = RetrieveTextureBase(NewTextureRHI);
	StateCache.SetShaderResourceView<SF_Pixel>(NewTexture ? NewTexture->GetShaderResourceView() : nullptr, TextureIndex);
}

void FD3D12CommandContext::RHISetShaderTexture(FComputeShaderRHIParamRef ComputeShaderRHI, uint32 TextureIndex, FTextureRHIParamRef NewTextureRHI)
{
	//VALIDATE_BOUND_SHADER(ComputeShaderRHI);
	FD3D12TextureBase* const NewTexture = RetrieveTextureBase(NewTextureRHI);
	StateCache.SetShaderResourceView<SF_Compute>(NewTexture ? NewTexture->GetShaderResourceView() : nullptr, TextureIndex);
}

void FD3D12CommandContext::RHISetUAVParameter(FComputeShaderRHIParamRef ComputeShaderRHI, uint32 UAVIndex, FUnorderedAccessViewRHIParamRef UAVRHI)
{
	//VALIDATE_BOUND_SHADER(ComputeShaderRHI);

	FD3D12UnorderedAccessView* UAV = RetrieveObject<FD3D12UnorderedAccessView>(UAVRHI);

	if (UAV)
	{
		ConditionalClearShaderResource(UAV->GetResourceLocation());
	}

	uint32 InitialCount = -1;

	// Actually set the UAV
	StateCache.SetUAVs<SF_Compute>(UAVIndex, 1, &UAV, &InitialCount);
}

void FD3D12CommandContext::RHISetUAVParameter(FComputeShaderRHIParamRef ComputeShaderRHI, uint32 UAVIndex, FUnorderedAccessViewRHIParamRef UAVRHI, uint32 InitialCount)
{
	//VALIDATE_BOUND_SHADER(ComputeShaderRHI);

	FD3D12UnorderedAccessView* UAV = RetrieveObject<FD3D12UnorderedAccessView>(UAVRHI);

	if (UAV)
	{
		ConditionalClearShaderResource(UAV->GetResourceLocation());
	}

	StateCache.SetUAVs<SF_Compute>(UAVIndex, 1, &UAV, &InitialCount);
}

void FD3D12CommandContext::RHISetShaderResourceViewParameter(FPixelShaderRHIParamRef PixelShaderRHI, uint32 TextureIndex, FShaderResourceViewRHIParamRef SRVRHI)
{
	VALIDATE_BOUND_SHADER(PixelShaderRHI);
	FD3D12ShaderResourceView* const SRV = RetrieveObject<FD3D12ShaderResourceView>(SRVRHI);
	StateCache.SetShaderResourceView<SF_Pixel>(SRV, TextureIndex);
}

void FD3D12CommandContext::RHISetShaderResourceViewParameter(FVertexShaderRHIParamRef VertexShaderRHI, uint32 TextureIndex, FShaderResourceViewRHIParamRef SRVRHI)
{
	VALIDATE_BOUND_SHADER(VertexShaderRHI);
	FD3D12ShaderResourceView* const SRV = RetrieveObject<FD3D12ShaderResourceView>(SRVRHI);
	StateCache.SetShaderResourceView<SF_Vertex>(SRV, TextureIndex);
}

void FD3D12CommandContext::RHISetShaderResourceViewParameter(FComputeShaderRHIParamRef ComputeShaderRHI, uint32 TextureIndex, FShaderResourceViewRHIParamRef SRVRHI)
{
	//VALIDATE_BOUND_SHADER(ComputeShaderRHI);
	FD3D12ShaderResourceView* const SRV = RetrieveObject<FD3D12ShaderResourceView>(SRVRHI);
	StateCache.SetShaderResourceView<SF_Compute>(SRV, TextureIndex);
}

void FD3D12CommandContext::RHISetShaderResourceViewParameter(FHullShaderRHIParamRef HullShaderRHI, uint32 TextureIndex, FShaderResourceViewRHIParamRef SRVRHI)
{
	VALIDATE_BOUND_SHADER(HullShaderRHI);
	FD3D12ShaderResourceView* const SRV = RetrieveObject<FD3D12ShaderResourceView>(SRVRHI);
	StateCache.SetShaderResourceView<SF_Hull>(SRV, TextureIndex);
}

void FD3D12CommandContext::RHISetShaderResourceViewParameter(FDomainShaderRHIParamRef DomainShaderRHI, uint32 TextureIndex, FShaderResourceViewRHIParamRef SRVRHI)
{
	VALIDATE_BOUND_SHADER(DomainShaderRHI);
	FD3D12ShaderResourceView* const SRV = RetrieveObject<FD3D12ShaderResourceView>(SRVRHI);
	StateCache.SetShaderResourceView<SF_Domain>(SRV, TextureIndex);
}

void FD3D12CommandContext::RHISetShaderResourceViewParameter(FGeometryShaderRHIParamRef GeometryShaderRHI, uint32 TextureIndex, FShaderResourceViewRHIParamRef SRVRHI)
{
	VALIDATE_BOUND_SHADER(GeometryShaderRHI);
	FD3D12ShaderResourceView* const SRV = RetrieveObject<FD3D12ShaderResourceView>(SRVRHI);
	StateCache.SetShaderResourceView<SF_Geometry>(SRV, TextureIndex);
}

void FD3D12CommandContext::RHISetShaderSampler(FVertexShaderRHIParamRef VertexShaderRHI, uint32 SamplerIndex, FSamplerStateRHIParamRef NewStateRHI)
{
	VALIDATE_BOUND_SHADER(VertexShaderRHI);
	FD3D12SamplerState* NewState = RetrieveObject<FD3D12SamplerState>(NewStateRHI);
	StateCache.SetSamplerState<SF_Vertex>(NewState, SamplerIndex);
}

void FD3D12CommandContext::RHISetShaderSampler(FHullShaderRHIParamRef HullShaderRHI, uint32 SamplerIndex, FSamplerStateRHIParamRef NewStateRHI)
{
	VALIDATE_BOUND_SHADER(HullShaderRHI);
	FD3D12SamplerState* NewState = RetrieveObject<FD3D12SamplerState>(NewStateRHI);
	StateCache.SetSamplerState<SF_Hull>(NewState, SamplerIndex);
}

void FD3D12CommandContext::RHISetShaderSampler(FDomainShaderRHIParamRef DomainShaderRHI, uint32 SamplerIndex, FSamplerStateRHIParamRef NewStateRHI)
{
	VALIDATE_BOUND_SHADER(DomainShaderRHI);
	FD3D12SamplerState* NewState = RetrieveObject<FD3D12SamplerState>(NewStateRHI);
	StateCache.SetSamplerState<SF_Domain>(NewState, SamplerIndex);
}

void FD3D12CommandContext::RHISetShaderSampler(FGeometryShaderRHIParamRef GeometryShaderRHI, uint32 SamplerIndex, FSamplerStateRHIParamRef NewStateRHI)
{
	VALIDATE_BOUND_SHADER(GeometryShaderRHI);
	FD3D12SamplerState* NewState = RetrieveObject<FD3D12SamplerState>(NewStateRHI);
	StateCache.SetSamplerState<SF_Geometry>(NewState, SamplerIndex);
}

void FD3D12CommandContext::RHISetShaderSampler(FPixelShaderRHIParamRef PixelShaderRHI, uint32 SamplerIndex, FSamplerStateRHIParamRef NewStateRHI)
{
	VALIDATE_BOUND_SHADER(PixelShaderRHI);
	FD3D12SamplerState* NewState = RetrieveObject<FD3D12SamplerState>(NewStateRHI);
	StateCache.SetSamplerState<SF_Pixel>(NewState, SamplerIndex);
}

void FD3D12CommandContext::RHISetShaderSampler(FComputeShaderRHIParamRef ComputeShaderRHI, uint32 SamplerIndex, FSamplerStateRHIParamRef NewStateRHI)
{
	//VALIDATE_BOUND_SHADER(ComputeShaderRHI);
	FD3D12SamplerState* NewState = RetrieveObject<FD3D12SamplerState>(NewStateRHI);
	StateCache.SetSamplerState<SF_Compute>(NewState, SamplerIndex);
}

void FD3D12CommandContext::RHISetShaderUniformBuffer(FVertexShaderRHIParamRef VertexShader, uint32 BufferIndex, FUniformBufferRHIParamRef BufferRHI)
{
	//SCOPE_CYCLE_COUNTER(STAT_D3D12SetShaderUniformBuffer);
	VALIDATE_BOUND_SHADER(VertexShader);
	FD3D12UniformBuffer* Buffer = RetrieveObject<FD3D12UniformBuffer>(BufferRHI);

	StateCache.SetConstantsFromUniformBuffer<SF_Vertex>(BufferIndex, Buffer);

	BoundUniformBufferRefs[SF_Vertex][BufferIndex] = BufferRHI;
	BoundUniformBuffers[SF_Vertex][BufferIndex] = Buffer;
	DirtyUniformBuffers[SF_Vertex] |= (1 << BufferIndex);
}

void FD3D12CommandContext::RHISetShaderUniformBuffer(FHullShaderRHIParamRef HullShader, uint32 BufferIndex, FUniformBufferRHIParamRef BufferRHI)
{
	//SCOPE_CYCLE_COUNTER(STAT_D3D12SetShaderUniformBuffer);
	VALIDATE_BOUND_SHADER(HullShader);
	FD3D12UniformBuffer* Buffer = RetrieveObject<FD3D12UniformBuffer>(BufferRHI);

	StateCache.SetConstantsFromUniformBuffer<SF_Hull>(BufferIndex, Buffer);

	BoundUniformBufferRefs[SF_Hull][BufferIndex] = BufferRHI;
	BoundUniformBuffers[SF_Hull][BufferIndex] = Buffer;
	DirtyUniformBuffers[SF_Hull] |= (1 << BufferIndex);
}

void FD3D12CommandContext::RHISetShaderUniformBuffer(FDomainShaderRHIParamRef DomainShader, uint32 BufferIndex, FUniformBufferRHIParamRef BufferRHI)
{
	//SCOPE_CYCLE_COUNTER(STAT_D3D12SetShaderUniformBuffer);
	VALIDATE_BOUND_SHADER(DomainShader);
	FD3D12UniformBuffer* Buffer = RetrieveObject<FD3D12UniformBuffer>(BufferRHI);
	
	StateCache.SetConstantsFromUniformBuffer<SF_Domain>(BufferIndex, Buffer);

	BoundUniformBufferRefs[SF_Domain][BufferIndex] = BufferRHI;
	BoundUniformBuffers[SF_Domain][BufferIndex] = Buffer;
	DirtyUniformBuffers[SF_Domain] |= (1 << BufferIndex);
}

void FD3D12CommandContext::RHISetShaderUniformBuffer(FGeometryShaderRHIParamRef GeometryShader, uint32 BufferIndex, FUniformBufferRHIParamRef BufferRHI)
{
	//SCOPE_CYCLE_COUNTER(STAT_D3D12SetShaderUniformBuffer);
	VALIDATE_BOUND_SHADER(GeometryShader);
	FD3D12UniformBuffer* Buffer = RetrieveObject<FD3D12UniformBuffer>(BufferRHI);

	StateCache.SetConstantsFromUniformBuffer<SF_Geometry>(BufferIndex, Buffer);

	BoundUniformBufferRefs[SF_Geometry][BufferIndex] = BufferRHI;
	BoundUniformBuffers[SF_Geometry][BufferIndex] = Buffer;
	DirtyUniformBuffers[SF_Geometry] |= (1 << BufferIndex);
}

void FD3D12CommandContext::RHISetShaderUniformBuffer(FPixelShaderRHIParamRef PixelShader, uint32 BufferIndex, FUniformBufferRHIParamRef BufferRHI)
{
	//SCOPE_CYCLE_COUNTER(STAT_D3D12SetShaderUniformBuffer);
	VALIDATE_BOUND_SHADER(PixelShader);
	FD3D12UniformBuffer* Buffer = RetrieveObject<FD3D12UniformBuffer>(BufferRHI);

	StateCache.SetConstantsFromUniformBuffer<SF_Pixel>(BufferIndex, Buffer);

	BoundUniformBufferRefs[SF_Pixel][BufferIndex] = BufferRHI;
	BoundUniformBuffers[SF_Pixel][BufferIndex] = Buffer;
	DirtyUniformBuffers[SF_Pixel] |= (1 << BufferIndex);
}

void FD3D12CommandContext::RHISetShaderUniformBuffer(FComputeShaderRHIParamRef ComputeShader, uint32 BufferIndex, FUniformBufferRHIParamRef BufferRHI)
{
	//SCOPE_CYCLE_COUNTER(STAT_D3D12SetShaderUniformBuffer);
	//VALIDATE_BOUND_SHADER(ComputeShader);
	FD3D12UniformBuffer* Buffer = RetrieveObject<FD3D12UniformBuffer>(BufferRHI);

	StateCache.SetConstantsFromUniformBuffer<SF_Compute>(BufferIndex, Buffer);

	BoundUniformBufferRefs[SF_Compute][BufferIndex] = BufferRHI;
	BoundUniformBuffers[SF_Compute][BufferIndex] = Buffer;
	DirtyUniformBuffers[SF_Compute] |= (1 << BufferIndex);
}

void FD3D12CommandContext::RHISetShaderParameter(FHullShaderRHIParamRef HullShaderRHI, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue)
{
	VALIDATE_BOUND_SHADER(HullShaderRHI);
	checkSlow(BufferIndex == 0);
	HSConstantBuffer.UpdateConstant((const uint8*)NewValue, BaseIndex, NumBytes);
}

void FD3D12CommandContext::RHISetShaderParameter(FDomainShaderRHIParamRef DomainShaderRHI, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue)
{
	VALIDATE_BOUND_SHADER(DomainShaderRHI);
	checkSlow(BufferIndex == 0);
	DSConstantBuffer.UpdateConstant((const uint8*)NewValue, BaseIndex, NumBytes);
}

void FD3D12CommandContext::RHISetShaderParameter(FVertexShaderRHIParamRef VertexShaderRHI, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue)
{
	VALIDATE_BOUND_SHADER(VertexShaderRHI);
	checkSlow(BufferIndex == 0);
	VSConstantBuffer.UpdateConstant((const uint8*)NewValue, BaseIndex, NumBytes);
}

void FD3D12CommandContext::RHISetShaderParameter(FPixelShaderRHIParamRef PixelShaderRHI, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue)
{
	VALIDATE_BOUND_SHADER(PixelShaderRHI);
	checkSlow(BufferIndex == 0);
	PSConstantBuffer.UpdateConstant((const uint8*)NewValue, BaseIndex, NumBytes);
}

void FD3D12CommandContext::RHISetShaderParameter(FGeometryShaderRHIParamRef GeometryShaderRHI, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue)
{
	VALIDATE_BOUND_SHADER(GeometryShaderRHI);
	checkSlow(BufferIndex == 0);
	GSConstantBuffer.UpdateConstant((const uint8*)NewValue, BaseIndex, NumBytes);
}

void FD3D12CommandContext::RHISetShaderParameter(FComputeShaderRHIParamRef ComputeShaderRHI, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue)
{
	//VALIDATE_BOUND_SHADER(ComputeShaderRHI);
	checkSlow(BufferIndex == 0);
	CSConstantBuffer.UpdateConstant((const uint8*)NewValue, BaseIndex, NumBytes);
}

void FD3D12CommandContext::ValidateExclusiveDepthStencilAccess(FExclusiveDepthStencil RequestedAccess) const
{
	const bool bSrcDepthWrite = RequestedAccess.IsDepthWrite();
	const bool bSrcStencilWrite = RequestedAccess.IsStencilWrite();

	if (bSrcDepthWrite || bSrcStencilWrite)
	{
		// New Rule: You have to call SetRenderTarget[s]() before
		ensure(CurrentDepthTexture);

		const bool bDstDepthWrite = CurrentDSVAccessType.IsDepthWrite();
		const bool bDstStencilWrite = CurrentDSVAccessType.IsStencilWrite();

		// requested access is not possible, fix SetRenderTarget EExclusiveDepthStencil or request a different one
		check(!bSrcDepthWrite || bDstDepthWrite);
		check(!bSrcStencilWrite || bDstStencilWrite);
	}
}

void FD3D12CommandContext::RHISetDepthStencilState(FDepthStencilStateRHIParamRef NewStateRHI, uint32 StencilRef)
{
	FD3D12DepthStencilState* NewState = FD3D12DynamicRHI::ResourceCast(NewStateRHI);

	ValidateExclusiveDepthStencilAccess(NewState->AccessType);

	StateCache.SetDepthStencilState(&NewState->Desc, StencilRef);
}

void FD3D12CommandContext::RHISetStencilRef(uint32 StencilRef)
{
	StateCache.SetStencilRef(StencilRef);
}

void FD3D12CommandContext::RHISetBlendState(FBlendStateRHIParamRef NewStateRHI, const FLinearColor& BlendFactor)
{
	FD3D12BlendState* NewState = FD3D12DynamicRHI::ResourceCast(NewStateRHI);
	StateCache.SetBlendState(&NewState->Desc, (const float*)&BlendFactor, 0xffffffff);
}

void FD3D12CommandContext::RHISetBlendFactor(const FLinearColor& BlendFactor)
{
	StateCache.SetBlendFactor((const float*)&BlendFactor);
}

void FD3D12CommandContext::CommitRenderTargetsAndUAVs()
{
	StateCache.SetRenderTargets(NumSimultaneousRenderTargets, CurrentRenderTargets, CurrentDepthStencilTarget);

	if (NumUAVs > 0)
	{
		uint32 UAVInitialCountArray[MAX_UAVS];
		for (uint32 UAVIndex = 0; UAVIndex < NumUAVs; ++UAVIndex)
		{
			// Using the value that indicates to keep the current UAV counter
			UAVInitialCountArray[UAVIndex] = -1;
		}

		StateCache.SetUAVs<SF_Pixel>(NumSimultaneousRenderTargets, NumUAVs, CurrentUAVs, UAVInitialCountArray);
	}
}

struct FRTVDesc
{
	uint32 Width;
	uint32 Height;
	DXGI_SAMPLE_DESC SampleDesc;
};

// Return an FRTVDesc structure whose
// Width and height dimensions are adjusted for the RTV's miplevel.
FRTVDesc GetRenderTargetViewDesc(FD3D12RenderTargetView* RenderTargetView)
{
	const D3D12_RENDER_TARGET_VIEW_DESC &TargetDesc = RenderTargetView->GetDesc();

	FD3D12Resource* BaseResource = RenderTargetView->GetResource();
	uint32 MipIndex = 0;
	FRTVDesc ret;
	memset(&ret, 0, sizeof(ret));

	switch (TargetDesc.ViewDimension)
	{
	case D3D12_RTV_DIMENSION_TEXTURE2D:
	case D3D12_RTV_DIMENSION_TEXTURE2DMS:
	case D3D12_RTV_DIMENSION_TEXTURE2DARRAY:
	case D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY:
	{
		D3D12_RESOURCE_DESC const& Desc = BaseResource->GetDesc();
		ret.Width = (uint32)Desc.Width;
		ret.Height = Desc.Height;
		ret.SampleDesc = Desc.SampleDesc;
		if (TargetDesc.ViewDimension == D3D12_RTV_DIMENSION_TEXTURE2D || TargetDesc.ViewDimension == D3D12_RTV_DIMENSION_TEXTURE2DARRAY)
		{
			// All the non-multisampled texture types have their mip-slice in the same position.
			MipIndex = TargetDesc.Texture2D.MipSlice;
		}
		break;
	}
	case D3D12_RTV_DIMENSION_TEXTURE3D:
	{
		D3D12_RESOURCE_DESC const& Desc = BaseResource->GetDesc();
		ret.Width = (uint32)Desc.Width;
		ret.Height = Desc.Height;
		ret.SampleDesc.Count = 1;
		ret.SampleDesc.Quality = 0;
		MipIndex = TargetDesc.Texture3D.MipSlice;
		break;
	}
	default:
	{
		// not expecting 1D targets.
		checkNoEntry();
	}
	}
	ret.Width >>= MipIndex;
	ret.Height >>= MipIndex;
	return ret;
}

void FD3D12CommandContext::RHISetRenderTargets(
	uint32 NewNumSimultaneousRenderTargets,
	const FRHIRenderTargetView* NewRenderTargetsRHI,
	const FRHIDepthRenderTargetView* NewDepthStencilTargetRHI,
	uint32 NewNumUAVs,
	const FUnorderedAccessViewRHIParamRef* UAVs
	)
{
	FD3D12TextureBase* NewDepthStencilTarget = RetrieveTextureBase(NewDepthStencilTargetRHI ? NewDepthStencilTargetRHI->Texture : nullptr);

	check(NewNumSimultaneousRenderTargets + NewNumUAVs <= MaxSimultaneousRenderTargets);

	bool bTargetChanged = false;

	// Set the appropriate depth stencil view depending on whether depth writes are enabled or not
	FD3D12DepthStencilView* DepthStencilView = NULL;
	if (NewDepthStencilTarget)
	{
		CurrentDSVAccessType = NewDepthStencilTargetRHI->GetDepthStencilAccess();
		DepthStencilView = NewDepthStencilTarget->GetDepthStencilView(CurrentDSVAccessType);

		// Unbind any shader views of the depth stencil target that are bound.
		ConditionalClearShaderResource(&NewDepthStencilTarget->ResourceLocation);
	}

	// Check if the depth stencil target is different from the old state.
	if (CurrentDepthStencilTarget != DepthStencilView)
	{
		CurrentDepthTexture = NewDepthStencilTarget;
		CurrentDepthStencilTarget = DepthStencilView;
		bTargetChanged = true;
	}

	// Gather the render target views for the new render targets.
	FD3D12RenderTargetView* NewRenderTargetViews[MaxSimultaneousRenderTargets];
	for (uint32 RenderTargetIndex = 0;RenderTargetIndex < MaxSimultaneousRenderTargets;++RenderTargetIndex)
	{
		FD3D12RenderTargetView* RenderTargetView = NULL;
		if (RenderTargetIndex < NewNumSimultaneousRenderTargets && NewRenderTargetsRHI[RenderTargetIndex].Texture != nullptr)
		{
			int32 RTMipIndex = NewRenderTargetsRHI[RenderTargetIndex].MipIndex;
			int32 RTSliceIndex = NewRenderTargetsRHI[RenderTargetIndex].ArraySliceIndex;
			FD3D12TextureBase* NewRenderTarget = RetrieveTextureBase(NewRenderTargetsRHI[RenderTargetIndex].Texture);
			RenderTargetView = NewRenderTarget->GetRenderTargetView(RTMipIndex, RTSliceIndex);

			ensureMsgf(RenderTargetView, TEXT("Texture being set as render target has no RTV"));

			// Unbind any shader views of the render target that are bound.
			ConditionalClearShaderResource(&NewRenderTarget->ResourceLocation);
		}

		NewRenderTargetViews[RenderTargetIndex] = RenderTargetView;

		// Check if the render target is different from the old state.
		if (CurrentRenderTargets[RenderTargetIndex] != RenderTargetView)
		{
			CurrentRenderTargets[RenderTargetIndex] = RenderTargetView;
			bTargetChanged = true;
		}
	}
	if (NumSimultaneousRenderTargets != NewNumSimultaneousRenderTargets)
	{
		NumSimultaneousRenderTargets = NewNumSimultaneousRenderTargets;
		bTargetChanged = true;
	}

	// Gather the new UAVs.
	for (uint32 UAVIndex = 0;UAVIndex < MaxSimultaneousUAVs;++UAVIndex)
	{
		FD3D12UnorderedAccessView* RHIUAV = NULL;
		if (UAVIndex < NewNumUAVs && UAVs[UAVIndex] != NULL)
		{
			RHIUAV = RetrieveObject<FD3D12UnorderedAccessView>(UAVs[UAVIndex]);
			FD3D12DynamicRHI::TransitionResource(CommandListHandle, RHIUAV, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

			// Unbind any shader views of the UAV's resource.
			ConditionalClearShaderResource(RHIUAV->GetResourceLocation());
		}

		if (CurrentUAVs[UAVIndex] != RHIUAV)
		{
			CurrentUAVs[UAVIndex] = RHIUAV;
			bTargetChanged = true;
		}
	}
	if (NumUAVs != NewNumUAVs)
	{
		NumUAVs = NewNumUAVs;
		bTargetChanged = true;
	}

	// Only make the D3D call to change render targets if something actually changed.
	if (bTargetChanged)
	{
		CommitRenderTargetsAndUAVs();
	}

	// Set the viewport to the full size of render target 0.
	if (NewRenderTargetViews[0])
	{
		// check target 0 is valid
		check(0 < NewNumSimultaneousRenderTargets && NewRenderTargetsRHI[0].Texture != nullptr);
		FRTVDesc RTTDesc = GetRenderTargetViewDesc(NewRenderTargetViews[0]);
		RHISetViewport(0, 0, 0.0f, RTTDesc.Width, RTTDesc.Height, 1.0f);
	}
	else if (DepthStencilView)
	{
		FD3D12Resource* DepthTargetTexture = DepthStencilView->GetResource();
		D3D12_RESOURCE_DESC const& DTTDesc = DepthTargetTexture->GetDesc();
		RHISetViewport(0, 0, 0.0f, (uint32)DTTDesc.Width, DTTDesc.Height, 1.0f);
	}
}

void FD3D12DynamicRHI::RHIDiscardRenderTargets(bool Depth, bool Stencil, uint32 ColorBitMask)
{
	// Could support in DX12 via ID3D12CommandList::DiscardResource function.
}

void FD3D12CommandContext::RHISetRenderTargetsAndClear(const FRHISetRenderTargetsInfo& RenderTargetsInfo)
{
	// Here convert to FUnorderedAccessViewRHIParamRef* in order to call RHISetRenderTargets
	FUnorderedAccessViewRHIParamRef UAVs[MaxSimultaneousUAVs] = {};
	for (int32 UAVIndex = 0; UAVIndex < RenderTargetsInfo.NumUAVs; ++UAVIndex)
	{
		UAVs[UAVIndex] = RenderTargetsInfo.UnorderedAccessView[UAVIndex].GetReference();
	}

	this->RHISetRenderTargets(RenderTargetsInfo.NumColorRenderTargets,
		RenderTargetsInfo.ColorRenderTarget,
		&RenderTargetsInfo.DepthStencilRenderTarget,
		RenderTargetsInfo.NumUAVs,
		UAVs);
	if (RenderTargetsInfo.bClearColor || RenderTargetsInfo.bClearStencil || RenderTargetsInfo.bClearDepth)
	{
		FLinearColor ClearColors[MaxSimultaneousRenderTargets];
		float DepthClear = 0.0;
		uint32 StencilClear = 0;

		if (RenderTargetsInfo.bClearColor)
		{
			for (int32 i = 0; i < RenderTargetsInfo.NumColorRenderTargets; ++i)
			{
				if (RenderTargetsInfo.ColorRenderTarget[i].Texture != nullptr)
				{
					const FClearValueBinding& ClearValue = RenderTargetsInfo.ColorRenderTarget[i].Texture->GetClearBinding();
					checkf(ClearValue.ColorBinding == EClearBinding::EColorBound, TEXT("Texture: %s does not have a color bound for fast clears"), *RenderTargetsInfo.ColorRenderTarget[i].Texture->GetName().GetPlainNameString());
					ClearColors[i] = ClearValue.GetClearColor();
				}
				else
				{
					ClearColors[i] = FLinearColor(ForceInitToZero);
				}
			}
		}
		if (RenderTargetsInfo.bClearDepth || RenderTargetsInfo.bClearStencil)
		{
			const FClearValueBinding& ClearValue = RenderTargetsInfo.DepthStencilRenderTarget.Texture->GetClearBinding();
			checkf(ClearValue.ColorBinding == EClearBinding::EDepthStencilBound, TEXT("Texture: %s does not have a DS value bound for fast clears"), *RenderTargetsInfo.DepthStencilRenderTarget.Texture->GetName().GetPlainNameString());
			ClearValue.GetDepthStencil(DepthClear, StencilClear);
		}

		this->RHIClearMRTImpl(RenderTargetsInfo.bClearColor, RenderTargetsInfo.NumColorRenderTargets, ClearColors, RenderTargetsInfo.bClearDepth, DepthClear, RenderTargetsInfo.bClearStencil, StencilClear);
	}
}

// Occlusion/Timer queries.
void FD3D12CommandContext::RHIBeginRenderQuery(FRenderQueryRHIParamRef QueryRHI)
{
	FD3D12RenderQuery* Query = RetrieveObject<FD3D12RenderQuery>(QueryRHI);

	check(Query->Type == RQT_Occlusion);
	check(IsDefaultContext());
	Query->bResultIsCached = false;
	Query->HeapIndex = GetParentDevice()->GetQueryHeap()->BeginQuery(*this, D3D12_QUERY_TYPE_OCCLUSION);

#if EXECUTE_DEBUG_COMMAND_LISTS
	GIsDoingQuery = true;
#endif
}

void FD3D12CommandContext::RHIEndRenderQuery(FRenderQueryRHIParamRef QueryRHI)
{
	FD3D12RenderQuery* Query = RetrieveObject<FD3D12RenderQuery>(QueryRHI);

	switch (Query->Type)
	{
	case RQT_Occlusion:
	{
		// End the query
		check(IsDefaultContext());
		FD3D12QueryHeap* pQueryHeap = GetParentDevice()->GetQueryHeap();
		pQueryHeap->EndQuery(*this, D3D12_QUERY_TYPE_OCCLUSION, Query->HeapIndex);

		// Note: This occlusion query result really isn't ready until it's resolved. 
		// This code assumes it will be resolved on the same command list.
		Query->CLSyncPoint = CommandListHandle;
		Query->OwningContext = this;

		CommandListHandle.UpdateResidency(pQueryHeap->GetResultBuffer());
		break;
	}

	case RQT_AbsoluteTime:
	{
		Query->bResultIsCached = false;
		Query->CLSyncPoint = CommandListHandle;
		Query->OwningContext = this;
		this->otherWorkCounter += 2;	// +2 For the EndQuery and the ResolveQueryData
		CommandListHandle->EndQuery(Query->QueryHeap, D3D12_QUERY_TYPE_TIMESTAMP, Query->HeapIndex);
		CommandListHandle->ResolveQueryData(Query->QueryHeap, D3D12_QUERY_TYPE_TIMESTAMP, Query->HeapIndex, 1, Query->ResultBuffer, sizeof(uint64) * Query->HeapIndex);
		break;
	}

	default:
		check(false);
	}

#if EXECUTE_DEBUG_COMMAND_LISTS
	GIsDoingQuery = false;
#endif
}

// Primitive drawing.

static D3D_PRIMITIVE_TOPOLOGY GetD3D12PrimitiveType(uint32 PrimitiveType, bool bUsingTessellation)
{
	if (bUsingTessellation)
	{
		switch (PrimitiveType)
		{
		case PT_1_ControlPointPatchList: return D3D_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST;
		case PT_2_ControlPointPatchList: return D3D_PRIMITIVE_TOPOLOGY_2_CONTROL_POINT_PATCHLIST;

			// This is the case for tessellation without AEN or other buffers, so just flip to 3 CPs
		case PT_TriangleList: return D3D_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST;

		case PT_LineList:
		case PT_TriangleStrip:
		case PT_QuadList:
		case PT_PointList:
			UE_LOG(LogD3D12RHI, Fatal, TEXT("Invalid type specified for tessellated render, probably missing a case in FSkeletalMeshSceneProxy::DrawDynamicElementsByMaterial or FStaticMeshSceneProxy::GetMeshElement"));
			break;
		default:
			// Other cases are valid.
			break;
		};
	}

	switch (PrimitiveType)
	{
	case PT_TriangleList: return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	case PT_TriangleStrip: return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
	case PT_LineList: return D3D_PRIMITIVE_TOPOLOGY_LINELIST;
	case PT_PointList: return D3D_PRIMITIVE_TOPOLOGY_POINTLIST;

		// ControlPointPatchList types will pretend to be TRIANGLELISTS with a stride of N 
		// (where N is the number of control points specified), so we can return them for
		// tessellation and non-tessellation. This functionality is only used when rendering a 
		// default material with something that claims to be tessellated, generally because the 
		// tessellation material failed to compile for some reason.
	case PT_3_ControlPointPatchList: return D3D_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST;
	case PT_4_ControlPointPatchList: return D3D_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST;
	case PT_5_ControlPointPatchList: return D3D_PRIMITIVE_TOPOLOGY_5_CONTROL_POINT_PATCHLIST;
	case PT_6_ControlPointPatchList: return D3D_PRIMITIVE_TOPOLOGY_6_CONTROL_POINT_PATCHLIST;
	case PT_7_ControlPointPatchList: return D3D_PRIMITIVE_TOPOLOGY_7_CONTROL_POINT_PATCHLIST;
	case PT_8_ControlPointPatchList: return D3D_PRIMITIVE_TOPOLOGY_8_CONTROL_POINT_PATCHLIST;
	case PT_9_ControlPointPatchList: return D3D_PRIMITIVE_TOPOLOGY_9_CONTROL_POINT_PATCHLIST;
	case PT_10_ControlPointPatchList: return D3D_PRIMITIVE_TOPOLOGY_10_CONTROL_POINT_PATCHLIST;
	case PT_11_ControlPointPatchList: return D3D_PRIMITIVE_TOPOLOGY_11_CONTROL_POINT_PATCHLIST;
	case PT_12_ControlPointPatchList: return D3D_PRIMITIVE_TOPOLOGY_12_CONTROL_POINT_PATCHLIST;
	case PT_13_ControlPointPatchList: return D3D_PRIMITIVE_TOPOLOGY_13_CONTROL_POINT_PATCHLIST;
	case PT_14_ControlPointPatchList: return D3D_PRIMITIVE_TOPOLOGY_14_CONTROL_POINT_PATCHLIST;
	case PT_15_ControlPointPatchList: return D3D_PRIMITIVE_TOPOLOGY_15_CONTROL_POINT_PATCHLIST;
	case PT_16_ControlPointPatchList: return D3D_PRIMITIVE_TOPOLOGY_16_CONTROL_POINT_PATCHLIST;
	case PT_17_ControlPointPatchList: return D3D_PRIMITIVE_TOPOLOGY_17_CONTROL_POINT_PATCHLIST;
	case PT_18_ControlPointPatchList: return D3D_PRIMITIVE_TOPOLOGY_18_CONTROL_POINT_PATCHLIST;
	case PT_19_ControlPointPatchList: return D3D_PRIMITIVE_TOPOLOGY_19_CONTROL_POINT_PATCHLIST;
	case PT_20_ControlPointPatchList: return D3D_PRIMITIVE_TOPOLOGY_20_CONTROL_POINT_PATCHLIST;
	case PT_21_ControlPointPatchList: return D3D_PRIMITIVE_TOPOLOGY_21_CONTROL_POINT_PATCHLIST;
	case PT_22_ControlPointPatchList: return D3D_PRIMITIVE_TOPOLOGY_22_CONTROL_POINT_PATCHLIST;
	case PT_23_ControlPointPatchList: return D3D_PRIMITIVE_TOPOLOGY_23_CONTROL_POINT_PATCHLIST;
	case PT_24_ControlPointPatchList: return D3D_PRIMITIVE_TOPOLOGY_24_CONTROL_POINT_PATCHLIST;
	case PT_25_ControlPointPatchList: return D3D_PRIMITIVE_TOPOLOGY_25_CONTROL_POINT_PATCHLIST;
	case PT_26_ControlPointPatchList: return D3D_PRIMITIVE_TOPOLOGY_26_CONTROL_POINT_PATCHLIST;
	case PT_27_ControlPointPatchList: return D3D_PRIMITIVE_TOPOLOGY_27_CONTROL_POINT_PATCHLIST;
	case PT_28_ControlPointPatchList: return D3D_PRIMITIVE_TOPOLOGY_28_CONTROL_POINT_PATCHLIST;
	case PT_29_ControlPointPatchList: return D3D_PRIMITIVE_TOPOLOGY_29_CONTROL_POINT_PATCHLIST;
	case PT_30_ControlPointPatchList: return D3D_PRIMITIVE_TOPOLOGY_30_CONTROL_POINT_PATCHLIST;
	case PT_31_ControlPointPatchList: return D3D_PRIMITIVE_TOPOLOGY_31_CONTROL_POINT_PATCHLIST;
	case PT_32_ControlPointPatchList: return D3D_PRIMITIVE_TOPOLOGY_32_CONTROL_POINT_PATCHLIST;
	default: UE_LOG(LogD3D12RHI, Fatal, TEXT("Unknown primitive type: %u"), PrimitiveType);
	};

	return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
}

void FD3D12CommandContext::CommitNonComputeShaderConstants()
{
	//SCOPE_CYCLE_COUNTER(STAT_D3D12CommitGraphicsConstants);

	FD3D12BoundShaderState* RESTRICT CurrentBoundShaderStateRef = CurrentBoundShaderState.GetReference();

	check(CurrentBoundShaderStateRef);

	// Only set the constant buffer if this shader needs the global constant buffer bound
	// Otherwise we will overwrite a different constant buffer
	if (CurrentBoundShaderStateRef->bShaderNeedsGlobalConstantBuffer[SF_Vertex])
	{
		StateCache.SetConstantBuffer<SF_Vertex>(VSConstantBuffer, bDiscardSharedConstants);
	}

	// Skip HS/DS CB updates in cases where tessellation isn't being used
	// Note that this is *potentially* unsafe because bDiscardSharedConstants is cleared at the
	// end of the function, however we're OK for now because bDiscardSharedConstants
	// is always reset whenever bUsingTessellation changes in SetBoundShaderState()
	if (bUsingTessellation)
	{
		if (CurrentBoundShaderStateRef->bShaderNeedsGlobalConstantBuffer[SF_Hull])
		{
			StateCache.SetConstantBuffer<SF_Hull>(HSConstantBuffer, bDiscardSharedConstants);
		}

		if (CurrentBoundShaderStateRef->bShaderNeedsGlobalConstantBuffer[SF_Domain])
		{
			StateCache.SetConstantBuffer<SF_Domain>(DSConstantBuffer, bDiscardSharedConstants);
		}
	}

	if (CurrentBoundShaderStateRef->bShaderNeedsGlobalConstantBuffer[SF_Geometry])
	{
		StateCache.SetConstantBuffer<SF_Geometry>(GSConstantBuffer, bDiscardSharedConstants);
	}

	if (CurrentBoundShaderStateRef->bShaderNeedsGlobalConstantBuffer[SF_Pixel])
	{
		StateCache.SetConstantBuffer<SF_Pixel>(PSConstantBuffer, bDiscardSharedConstants);
	}

	bDiscardSharedConstants = false;
}

void FD3D12CommandContext::CommitComputeShaderConstants()
{
	StateCache.SetConstantBuffer<SF_Compute>(CSConstantBuffer, bDiscardSharedConstants);
}

template <EShaderFrequency Frequency>
FORCEINLINE void SetResource(FD3D12CommandContext& CmdContext, uint32 BindIndex, FD3D12ShaderResourceView* RESTRICT SRV)
{
	// We set the resource through the RHI to track state for the purposes of unbinding SRVs when a UAV or RTV is bound.
	CmdContext.StateCache.SetShaderResourceView<Frequency>(SRV, BindIndex);
}

template <EShaderFrequency Frequency>
FORCEINLINE void SetResource(FD3D12CommandContext& CmdContext, uint32 BindIndex, FD3D12SamplerState* RESTRICT SamplerState)
{
	CmdContext.StateCache.SetSamplerState<Frequency>(SamplerState, BindIndex);
}

template <EShaderFrequency ShaderFrequency>
inline int32 SetShaderResourcesFromBuffer_Surface(FD3D12CommandContext& CmdContext, FD3D12UniformBuffer* RESTRICT Buffer, const uint32 * RESTRICT ResourceMap, int32 BufferIndex)
{
	const TRefCountPtr<FRHIResource>* RESTRICT Resources = Buffer->ResourceTable.GetData();
	const float CurrentTime = FApp::GetCurrentTime();
	int32 NumSetCalls = 0;
	const uint32 BufferOffset = ResourceMap[BufferIndex];
	if (BufferOffset > 0)
	{
		const uint32* RESTRICT ResourceInfos = &ResourceMap[BufferOffset];
		uint32 ResourceInfo = *ResourceInfos++;
		do
		{
			checkSlow(FRHIResourceTableEntry::GetUniformBufferIndex(ResourceInfo) == BufferIndex);
			const uint16 ResourceIndex = FRHIResourceTableEntry::GetResourceIndex(ResourceInfo);
			const uint8 BindIndex = FRHIResourceTableEntry::GetBindIndex(ResourceInfo);

			FRHITexture* TextureRHI = (FRHITexture*)Resources[ResourceIndex].GetReference();
			TextureRHI->SetLastRenderTime(CurrentTime);

			FD3D12TextureBase* TextureD3D12 = CmdContext.RetrieveTextureBase(TextureRHI);
			FD3D12ShaderResourceView* D3D12Resource = TextureD3D12->GetShaderResourceView();

			SetResource<ShaderFrequency>(CmdContext, BindIndex, D3D12Resource);
			NumSetCalls++;
			ResourceInfo = *ResourceInfos++;
		} while (FRHIResourceTableEntry::GetUniformBufferIndex(ResourceInfo) == BufferIndex);
	}

	INC_DWORD_STAT_BY(STAT_D3D12SetTextureInTableCalls, NumSetCalls);
	return NumSetCalls;
}

template <EShaderFrequency ShaderFrequency>
inline int32 SetShaderResourcesFromBuffer_SRV(FD3D12CommandContext& CmdContext, FD3D12UniformBuffer* RESTRICT Buffer, const uint32 * RESTRICT ResourceMap, int32 BufferIndex)
{
	const TRefCountPtr<FRHIResource>* RESTRICT Resources = Buffer->ResourceTable.GetData();
	int32 NumSetCalls = 0;
	const uint32 BufferOffset = ResourceMap[BufferIndex];
	if (BufferOffset > 0)
	{
		const uint32* RESTRICT ResourceInfos = &ResourceMap[BufferOffset];
		uint32 ResourceInfo = *ResourceInfos++;
		do
		{
			checkSlow(FRHIResourceTableEntry::GetUniformBufferIndex(ResourceInfo) == BufferIndex);
			const uint16 ResourceIndex = FRHIResourceTableEntry::GetResourceIndex(ResourceInfo);
			const uint8 BindIndex = FRHIResourceTableEntry::GetBindIndex(ResourceInfo);

			FD3D12ShaderResourceView* D3D12Resource = CmdContext.RetrieveObject<FD3D12ShaderResourceView>((FRHIShaderResourceView*)(Resources[ResourceIndex].GetReference()));

			SetResource<ShaderFrequency>(CmdContext, BindIndex, D3D12Resource);
			NumSetCalls++;
			ResourceInfo = *ResourceInfos++;
		} while (FRHIResourceTableEntry::GetUniformBufferIndex(ResourceInfo) == BufferIndex);
	}

	INC_DWORD_STAT_BY(STAT_D3D12SetTextureInTableCalls, NumSetCalls);
	return NumSetCalls;
}

template <EShaderFrequency ShaderFrequency>
inline int32 SetShaderResourcesFromBuffer_Sampler(FD3D12CommandContext& CmdContext, FD3D12UniformBuffer* RESTRICT Buffer, const uint32 * RESTRICT ResourceMap, int32 BufferIndex)
{
	const TRefCountPtr<FRHIResource>* RESTRICT Resources = Buffer->ResourceTable.GetData();
	int32 NumSetCalls = 0;
	const uint32 BufferOffset = ResourceMap[BufferIndex];
	if (BufferOffset > 0)
	{
		const uint32* RESTRICT ResourceInfos = &ResourceMap[BufferOffset];
		uint32 ResourceInfo = *ResourceInfos++;
		do
		{
			checkSlow(FRHIResourceTableEntry::GetUniformBufferIndex(ResourceInfo) == BufferIndex);
			const uint16 ResourceIndex = FRHIResourceTableEntry::GetResourceIndex(ResourceInfo);
			const uint8 BindIndex = FRHIResourceTableEntry::GetBindIndex(ResourceInfo);

			// todo: could coalesce adjacent bound resources.
			FD3D12SamplerState* D3D12Resource = CmdContext.RetrieveObject<FD3D12SamplerState>((FRHISamplerState*)(Resources[ResourceIndex].GetReference()));

			SetResource<ShaderFrequency>(CmdContext, BindIndex, D3D12Resource);
			NumSetCalls++;
			ResourceInfo = *ResourceInfos++;
		} while (FRHIResourceTableEntry::GetUniformBufferIndex(ResourceInfo) == BufferIndex);
	}

	INC_DWORD_STAT_BY(STAT_D3D12SetTextureInTableCalls, NumSetCalls);
	return NumSetCalls;
}

template <class ShaderType>
void FD3D12CommandContext::SetResourcesFromTables(const ShaderType* RESTRICT Shader)
{
	checkSlow(Shader);

	// Mask the dirty bits by those buffers from which the shader has bound resources.
	uint32 DirtyBits = Shader->ShaderResourceTable.ResourceTableBits & DirtyUniformBuffers[ShaderType::StaticFrequency];
	while (DirtyBits)
	{
		// Scan for the lowest set bit, compute its index, clear it in the set of dirty bits.
		const uint32 LowestBitMask = (DirtyBits)& (-(int32)DirtyBits);
		const int32 BufferIndex = FMath::FloorLog2(LowestBitMask); // todo: This has a branch on zero, we know it could never be zero...
		DirtyBits ^= LowestBitMask;
		FD3D12UniformBuffer* Buffer = BoundUniformBuffers[ShaderType::StaticFrequency][BufferIndex];
		check(Buffer);
		check(BufferIndex < Shader->ShaderResourceTable.ResourceTableLayoutHashes.Num());
		check(Buffer->GetLayout().GetHash() == Shader->ShaderResourceTable.ResourceTableLayoutHashes[BufferIndex]);

		// todo: could make this two pass: gather then set
		SetShaderResourcesFromBuffer_Surface<(EShaderFrequency)ShaderType::StaticFrequency>(*this, Buffer, Shader->ShaderResourceTable.TextureMap.GetData(), BufferIndex);
		SetShaderResourcesFromBuffer_SRV<(EShaderFrequency)ShaderType::StaticFrequency>(*this, Buffer, Shader->ShaderResourceTable.ShaderResourceViewMap.GetData(), BufferIndex);
		SetShaderResourcesFromBuffer_Sampler<(EShaderFrequency)ShaderType::StaticFrequency>(*this, Buffer, Shader->ShaderResourceTable.SamplerMap.GetData(), BufferIndex);
	}

	DirtyUniformBuffers[ShaderType::StaticFrequency] = 0;
}

void FD3D12CommandContext::CommitGraphicsResourceTables()
{
	//SCOPE_CYCLE_COUNTER(STAT_D3D12CommitResourceTables);

	const FD3D12BoundShaderState* const RESTRICT CurrentBoundShaderStateRef = CurrentBoundShaderState.GetReference();
	check(CurrentBoundShaderStateRef);

	if (auto* Shader = CurrentBoundShaderStateRef->GetVertexShader())
	{
		SetResourcesFromTables(Shader);
	}
	if (auto* Shader = CurrentBoundShaderStateRef->GetPixelShader())
	{
		SetResourcesFromTables(Shader);
	}
	if (auto* Shader = CurrentBoundShaderStateRef->GetHullShader())
	{
		SetResourcesFromTables(Shader);
	}
	if (auto* Shader = CurrentBoundShaderStateRef->GetDomainShader())
	{
		SetResourcesFromTables(Shader);
	}
	if (auto* Shader = CurrentBoundShaderStateRef->GetGeometryShader())
	{
		SetResourcesFromTables(Shader);
	}
}

void FD3D12CommandContext::CommitComputeResourceTables(FD3D12ComputeShader* InComputeShader)
{
	//SCOPE_CYCLE_COUNTER(STAT_D3D12CommitResourceTables);

	FD3D12ComputeShader* RESTRICT ComputeShader = InComputeShader;
	check(ComputeShader);
	SetResourcesFromTables(ComputeShader);
}

void FD3D12CommandContext::RHIDrawPrimitive(uint32 PrimitiveType, uint32 BaseVertexIndex, uint32 NumPrimitives, uint32 NumInstances)
{
	RHI_DRAW_CALL_STATS(PrimitiveType, NumInstances*NumPrimitives);

	CommitGraphicsResourceTables();
	CommitNonComputeShaderConstants();

	uint32 VertexCount = GetVertexCountForPrimitiveCount(NumPrimitives, PrimitiveType);

	if (IsDefaultContext())
	{
		GetParentDevice()->RegisterGPUWork(NumPrimitives * NumInstances, VertexCount * NumInstances);
	}

	StateCache.SetPrimitiveTopology(GetD3D12PrimitiveType(PrimitiveType, bUsingTessellation));

	StateCache.ApplyState();
	numDraws++;
	CommandListHandle->DrawInstanced(VertexCount, FMath::Max<uint32>(1, NumInstances), BaseVertexIndex, 0);
#if UE_BUILD_DEBUG	
	OwningRHI.DrawCount++;
#endif
	DEBUG_EXECUTE_COMMAND_LIST(this);

}

void FD3D12CommandContext::RHIDrawPrimitiveIndirect(uint32 PrimitiveType, FVertexBufferRHIParamRef ArgumentBufferRHI, uint32 ArgumentOffset)
{
	FD3D12VertexBuffer* ArgumentBuffer = RetrieveObject<FD3D12VertexBuffer>(ArgumentBufferRHI);

	RHI_DRAW_CALL_INC();

	if (IsDefaultContext())
	{
		GetParentDevice()->RegisterGPUWork(0);
	}

	CommitGraphicsResourceTables();
	CommitNonComputeShaderConstants();

	StateCache.SetPrimitiveTopology(GetD3D12PrimitiveType(PrimitiveType, bUsingTessellation));

	FD3D12ResourceLocation& Location = ArgumentBuffer->ResourceLocation;
	FD3D12DynamicRHI::TransitionResource(CommandListHandle, Location.GetResource(), D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);

	StateCache.ApplyState();

	numDraws++;
	CommandListHandle->ExecuteIndirect(
		GetParentDevice()->GetParentAdapter()->GetDrawIndirectCommandSignature(),
		1,
		Location.GetResource()->GetResource(),
		Location.GetOffsetFromBaseOfResource() + ArgumentOffset,
		NULL,
		0
		);

	CommandListHandle.UpdateResidency(Location.GetResource());

#if UE_BUILD_DEBUG	
	OwningRHI.DrawCount++;
#endif
	DEBUG_EXECUTE_COMMAND_LIST(this);

}

void FD3D12CommandContext::RHIDrawIndexedIndirect(FIndexBufferRHIParamRef IndexBufferRHI, uint32 PrimitiveType, FStructuredBufferRHIParamRef ArgumentsBufferRHI, int32 DrawArgumentsIndex, uint32 NumInstances)
{
	FD3D12IndexBuffer* IndexBuffer = RetrieveObject<FD3D12IndexBuffer>(IndexBufferRHI);
	FD3D12StructuredBuffer* ArgumentsBuffer = RetrieveObject<FD3D12StructuredBuffer>(ArgumentsBufferRHI);

	RHI_DRAW_CALL_INC();

	if (IsDefaultContext())
	{
		GetParentDevice()->RegisterGPUWork(1);
	}

	CommitGraphicsResourceTables();
	CommitNonComputeShaderConstants();

	// determine 16bit vs 32bit indices
	uint32 SizeFormat = sizeof(DXGI_FORMAT);
	const DXGI_FORMAT Format = (IndexBuffer->GetStride() == sizeof(uint16) ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT);

	StateCache.SetIndexBuffer(&IndexBuffer->ResourceLocation, Format, 0);
	StateCache.SetPrimitiveTopology(GetD3D12PrimitiveType(PrimitiveType, bUsingTessellation));

	FD3D12ResourceLocation& Location = ArgumentsBuffer->ResourceLocation;
	FD3D12DynamicRHI::TransitionResource(CommandListHandle, Location.GetResource(), D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);

	StateCache.ApplyState();

	numDraws++;
	CommandListHandle->ExecuteIndirect(
		GetParentDevice()->GetParentAdapter()->GetDrawIndexedIndirectCommandSignature(),
		1,
		Location.GetResource()->GetResource(),
		Location.GetOffsetFromBaseOfResource() + DrawArgumentsIndex * ArgumentsBuffer->GetStride(),
		NULL,
		0
		);

	CommandListHandle.UpdateResidency(Location.GetResource());

#if UE_BUILD_DEBUG	
	OwningRHI.DrawCount++;
#endif
	DEBUG_EXECUTE_COMMAND_LIST(this);
}

void FD3D12CommandContext::RHIDrawIndexedPrimitive(FIndexBufferRHIParamRef IndexBufferRHI, uint32 PrimitiveType, int32 BaseVertexIndex, uint32 FirstInstance, uint32 NumVertices, uint32 StartIndex, uint32 NumPrimitives, uint32 NumInstances)
{
	FD3D12IndexBuffer* IndexBuffer = RetrieveObject<FD3D12IndexBuffer>(IndexBufferRHI);

	// called should make sure the input is valid, this avoid hidden bugs
	ensure(NumPrimitives > 0);

	RHI_DRAW_CALL_STATS(PrimitiveType, NumInstances*NumPrimitives);

	if (IsDefaultContext())
	{
		GetParentDevice()->RegisterGPUWork(NumPrimitives * NumInstances, NumVertices * NumInstances);
	}

	CommitGraphicsResourceTables();
	CommitNonComputeShaderConstants();

	// determine 16bit vs 32bit indices
	uint32 SizeFormat = sizeof(DXGI_FORMAT);
	const DXGI_FORMAT Format = (IndexBuffer->GetStride() == sizeof(uint16) ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT);

	uint32 IndexCount = GetVertexCountForPrimitiveCount(NumPrimitives, PrimitiveType);

	// Verify that we are not trying to read outside the index buffer range
	// test is an optimized version of: StartIndex + IndexCount <= IndexBuffer->GetSize() / IndexBuffer->GetStride() 
	checkf((StartIndex + IndexCount) * IndexBuffer->GetStride() <= IndexBuffer->GetSize(),
		TEXT("Start %u, Count %u, Type %u, Buffer Size %u, Buffer stride %u"), StartIndex, IndexCount, PrimitiveType, IndexBuffer->GetSize(), IndexBuffer->GetStride());

	StateCache.SetIndexBuffer(&IndexBuffer->ResourceLocation, Format, 0);
	StateCache.SetPrimitiveTopology(GetD3D12PrimitiveType(PrimitiveType, bUsingTessellation));
	StateCache.ApplyState();

	numDraws++;
	CommandListHandle->DrawIndexedInstanced(IndexCount, FMath::Max<uint32>(1, NumInstances), StartIndex, BaseVertexIndex, FirstInstance);
#if UE_BUILD_DEBUG	
	OwningRHI.DrawCount++;
#endif
	DEBUG_EXECUTE_COMMAND_LIST(this);

}

void FD3D12CommandContext::RHIDrawIndexedPrimitiveIndirect(uint32 PrimitiveType, FIndexBufferRHIParamRef IndexBufferRHI, FVertexBufferRHIParamRef ArgumentBufferRHI, uint32 ArgumentOffset)
{
	FD3D12IndexBuffer* IndexBuffer = RetrieveObject<FD3D12IndexBuffer>(IndexBufferRHI);
	FD3D12VertexBuffer* ArgumentBuffer = RetrieveObject<FD3D12VertexBuffer>(ArgumentBufferRHI);

	RHI_DRAW_CALL_INC();

	if (IsDefaultContext())
	{
		GetParentDevice()->RegisterGPUWork(0);
	}

	CommitGraphicsResourceTables();
	CommitNonComputeShaderConstants();

	// Set the index buffer.
	const uint32 SizeFormat = sizeof(DXGI_FORMAT);
	const DXGI_FORMAT Format = (IndexBuffer->GetStride() == sizeof(uint16) ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT);
	StateCache.SetIndexBuffer(&IndexBuffer->ResourceLocation, Format, 0);
	StateCache.SetPrimitiveTopology(GetD3D12PrimitiveType(PrimitiveType, bUsingTessellation));

	FD3D12ResourceLocation& Location = ArgumentBuffer->ResourceLocation;
	FD3D12DynamicRHI::TransitionResource(CommandListHandle, Location.GetResource(), D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);

	StateCache.ApplyState();

	numDraws++;
	CommandListHandle->ExecuteIndirect(
		GetParentDevice()->GetParentAdapter()->GetDrawIndexedIndirectCommandSignature(),
		1,
		Location.GetResource()->GetResource(),
		Location.GetOffsetFromBaseOfResource() + ArgumentOffset,
		NULL,
		0
		);

	CommandListHandle.UpdateResidency(Location.GetResource());

#if UE_BUILD_DEBUG	
	OwningRHI.DrawCount++;
#endif
	DEBUG_EXECUTE_COMMAND_LIST(this);
}

/**
* Preallocate memory or get a direct command stream pointer to fill up for immediate rendering . This avoids memcpys below in DrawPrimitiveUP
* @param PrimitiveType The type (triangles, lineloop, etc) of primitive to draw
* @param NumPrimitives The number of primitives in the VertexData buffer
* @param NumVertices The number of vertices to be written
* @param VertexDataStride Size of each vertex
* @param OutVertexData Reference to the allocated vertex memory
*/
void FD3D12CommandContext::RHIBeginDrawPrimitiveUP(uint32 PrimitiveType, uint32 NumPrimitives, uint32 NumVertices, uint32 VertexDataStride, void*& OutVertexData)
{
	checkSlow(PendingNumVertices == 0);

	// Remember the parameters for the draw call.
	PendingPrimitiveType = PrimitiveType;
	PendingNumPrimitives = NumPrimitives;
	PendingNumVertices = NumVertices;
	PendingVertexDataStride = VertexDataStride;

	// Map the dynamic buffer.
	OutVertexData = DynamicVB.Lock(NumVertices * VertexDataStride);
}

/**
* Draw a primitive using the vertex data populated since RHIBeginDrawPrimitiveUP and clean up any memory as needed
*/
void FD3D12CommandContext::RHIEndDrawPrimitiveUP()
{
	RHI_DRAW_CALL_STATS(PendingPrimitiveType, PendingNumPrimitives);

	checkSlow(!bUsingTessellation || PendingPrimitiveType == PT_TriangleList);

	if (IsDefaultContext())
	{
		GetParentDevice()->RegisterGPUWork(PendingNumPrimitives, PendingNumVertices);
	}

	// Unmap the dynamic vertex buffer.
	FD3D12ResourceLocation* BufferLocation = DynamicVB.Unlock();
	uint32 VBOffset = 0;

	// Issue the draw call.
	CommitGraphicsResourceTables();
	CommitNonComputeShaderConstants();
	StateCache.SetStreamSource(BufferLocation, 0, PendingVertexDataStride, VBOffset);
	StateCache.SetPrimitiveTopology(GetD3D12PrimitiveType(PendingPrimitiveType, bUsingTessellation));
	StateCache.ApplyState();
	numDraws++;
	CommandListHandle->DrawInstanced(PendingNumVertices, 1, 0, 0);
#if UE_BUILD_DEBUG	
	OwningRHI.DrawCount++;
#endif
	DEBUG_EXECUTE_COMMAND_LIST(this);

	// Clear these parameters.
	PendingPrimitiveType = 0;
	PendingNumPrimitives = 0;
	PendingNumVertices = 0;
	PendingVertexDataStride = 0;
}

/**
* Preallocate memory or get a direct command stream pointer to fill up for immediate rendering . This avoids memcpys below in DrawIndexedPrimitiveUP
* @param PrimitiveType The type (triangles, lineloop, etc) of primitive to draw
* @param NumPrimitives The number of primitives in the VertexData buffer
* @param NumVertices The number of vertices to be written
* @param VertexDataStride Size of each vertex
* @param OutVertexData Reference to the allocated vertex memory
* @param MinVertexIndex The lowest vertex index used by the index buffer
* @param NumIndices Number of indices to be written
* @param IndexDataStride Size of each index (either 2 or 4 bytes)
* @param OutIndexData Reference to the allocated index memory
*/
void FD3D12CommandContext::RHIBeginDrawIndexedPrimitiveUP(uint32 PrimitiveType, uint32 NumPrimitives, uint32 NumVertices, uint32 VertexDataStride, void*& OutVertexData, uint32 MinVertexIndex, uint32 NumIndices, uint32 IndexDataStride, void*& OutIndexData)
{
	checkSlow((sizeof(uint16) == IndexDataStride) || (sizeof(uint32) == IndexDataStride));

	// Store off information needed for the draw call.
	PendingPrimitiveType = PrimitiveType;
	PendingNumPrimitives = NumPrimitives;
	PendingMinVertexIndex = MinVertexIndex;
	PendingIndexDataStride = IndexDataStride;
	PendingNumVertices = NumVertices;
	PendingNumIndices = NumIndices;
	PendingVertexDataStride = VertexDataStride;

	// Map dynamic vertex and index buffers.
	OutVertexData = DynamicVB.Lock(NumVertices * VertexDataStride);
	OutIndexData = DynamicIB.Lock(NumIndices * IndexDataStride);
}

/**
* Draw a primitive using the vertex and index data populated since RHIBeginDrawIndexedPrimitiveUP and clean up any memory as needed
*/
void FD3D12CommandContext::RHIEndDrawIndexedPrimitiveUP()
{
	// tessellation only supports trilists
	checkSlow(!bUsingTessellation || PendingPrimitiveType == PT_TriangleList);

	RHI_DRAW_CALL_STATS(PendingPrimitiveType, PendingNumPrimitives);

	if (IsDefaultContext())
	{
		GetParentDevice()->RegisterGPUWork(PendingNumPrimitives, PendingNumVertices);
	}

	// Unmap the dynamic buffers.
	FD3D12ResourceLocation* VertexBufferLocation = DynamicVB.Unlock();
	FD3D12ResourceLocation* IndexBufferLocation = DynamicIB.Unlock();
	uint32 VBOffset = 0;

	// Issue the draw call.
	CommitGraphicsResourceTables();
	CommitNonComputeShaderConstants();
	StateCache.SetStreamSource(VertexBufferLocation, 0, PendingVertexDataStride, VBOffset);
	StateCache.SetIndexBuffer(IndexBufferLocation, PendingIndexDataStride == sizeof(uint16) ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT, 0);
	StateCache.SetPrimitiveTopology(GetD3D12PrimitiveType(PendingPrimitiveType, bUsingTessellation));
	StateCache.ApplyState();

	numDraws++;
	CommandListHandle->DrawIndexedInstanced(PendingNumIndices, 1, 0, PendingMinVertexIndex, 0);
#if UE_BUILD_DEBUG	
	OwningRHI.DrawCount++;
#endif
	DEBUG_EXECUTE_COMMAND_LIST(this);

	//It's important to release the locations so the fast alloc page can be freed
	DynamicVB.ReleaseResourceLocation();
	DynamicIB.ReleaseResourceLocation();

	// Clear these parameters.
	PendingPrimitiveType = 0;
	PendingNumPrimitives = 0;
	PendingMinVertexIndex = 0;
	PendingIndexDataStride = 0;
	PendingNumVertices = 0;
	PendingNumIndices = 0;
	PendingVertexDataStride = 0;
}

// Raster operations.
void FD3D12CommandContext::RHIClearMRT(bool bClearColor, int32 NumClearColors, const FLinearColor* ClearColorArray, bool bClearDepth, float Depth, bool bClearStencil, uint32 Stencil)
{
	RHIClearMRTImpl(bClearColor, NumClearColors, ClearColorArray, bClearDepth, Depth, bClearStencil, Stencil);
}

void FD3D12CommandContext::RHIClearMRTImpl(bool bClearColor, int32 NumClearColors, const FLinearColor* ClearColorArray, bool bClearDepth, float Depth, bool bClearStencil, uint32 Stencil)
{
	SCOPE_CYCLE_COUNTER(STAT_D3D12ClearMRT);

	uint32 NumViews = 1;
	D3D12_VIEWPORT Viewport;
	StateCache.GetViewports(&NumViews, &Viewport);

	D3D12_RECT ScissorRect;
	StateCache.GetScissorRect(&ScissorRect);

	if (ScissorRect.left >= ScissorRect.right || ScissorRect.top >= ScissorRect.bottom)
	{
		return;
	}

	FD3D12RenderTargetView* RenderTargetViews[MaxSimultaneousRenderTargets];
	FD3D12DepthStencilView* DSView = nullptr;
	uint32 NumSimultaneousRTs = 0;
	StateCache.GetRenderTargets(RenderTargetViews, &NumSimultaneousRTs, &DSView);
	FD3D12BoundRenderTargets BoundRenderTargets(RenderTargetViews, NumSimultaneousRTs, DSView);
	FD3D12DepthStencilView* DepthStencilView = BoundRenderTargets.GetDepthStencilView();

	// Use rounding for when the number can't be perfectly represented by a float
	const LONG Width = static_cast<LONG>(FMath::RoundToInt(Viewport.Width));
	const LONG Height = static_cast<LONG>(FMath::RoundToInt(Viewport.Height));

	// When clearing we must pay attention to the currently set scissor rect
	bool bClearCoversEntireSurface = false;
	if (ScissorRect.left <= 0 && ScissorRect.top <= 0 &&
		ScissorRect.right >= Width && ScissorRect.bottom >= Height)
	{
		bClearCoversEntireSurface = true;
	}

	// Must specify enough clear colors for all active RTs
	check(!bClearColor || NumClearColors >= BoundRenderTargets.GetNumActiveTargets());

	const bool bSupportsFastClear = true;
	uint32 ClearRectCount = 0;
	D3D12_RECT* pClearRects = nullptr;
	D3D12_RECT ClearRects[4];

	// Only pass a rect down to the driver if we specifically want to clear a sub-rect
	if (!bSupportsFastClear || !bClearCoversEntireSurface)
	{
		{
			ClearRects[ClearRectCount] = ScissorRect;
			ClearRectCount++;
		}

		pClearRects = ClearRects;

		static const bool bSpewPerfWarnings = false;

		if (bSpewPerfWarnings)
		{
			UE_LOG(LogD3D12RHI, Warning, TEXT("RHIClearMRTImpl: Using non-fast clear path! This has performance implications"));
			UE_LOG(LogD3D12RHI, Warning, TEXT("       Viewport: Width %d, Height: %d"), static_cast<LONG>(FMath::RoundToInt(Viewport.Width)), static_cast<LONG>(FMath::RoundToInt(Viewport.Height)));
			UE_LOG(LogD3D12RHI, Warning, TEXT("   Scissor Rect: Width %d, Height: %d"), ScissorRect.right, ScissorRect.bottom);
		}
	}

	const bool ClearRTV = bClearColor && BoundRenderTargets.GetNumActiveTargets() > 0;
	const bool ClearDSV = (bClearDepth || bClearStencil) && DepthStencilView;

	if (ClearRTV)
	{
		for (int32 TargetIndex = 0; TargetIndex < BoundRenderTargets.GetNumActiveTargets(); TargetIndex++)
		{
			FD3D12RenderTargetView* RTView = BoundRenderTargets.GetRenderTargetView(TargetIndex);

			if (RTView != nullptr)
			{
				FD3D12DynamicRHI::TransitionResource(CommandListHandle, RTView, D3D12_RESOURCE_STATE_RENDER_TARGET);
			}
		}
	}

	uint32 ClearFlags = 0;
	if (ClearDSV)
	{
		if (bClearDepth && DepthStencilView->HasDepth())
		{
			ClearFlags |= D3D12_CLEAR_FLAG_DEPTH;
		}
		else if (bClearDepth)
		{
			UE_LOG(LogD3D12RHI, Warning, TEXT("RHIClearMRTImpl: Asking to clear a DSV that does not store depth."));
		}

		if (bClearStencil && DepthStencilView->HasStencil())
		{
			ClearFlags |= D3D12_CLEAR_FLAG_STENCIL;
		}
		else if (bClearStencil)
		{
			UE_LOG(LogD3D12RHI, Warning, TEXT("RHIClearMRTImpl: Asking to clear a DSV that does not store stencil."));
		}

		if (bClearDepth && (!DepthStencilView->HasStencil() || bClearStencil))
		{
			// Transition the entire view (Both depth and stencil planes if applicable)
			// Some DSVs don't have stencil bits.
			FD3D12DynamicRHI::TransitionResource(CommandListHandle, DepthStencilView, D3D12_RESOURCE_STATE_DEPTH_WRITE);
		}
		else
		{
			if (bClearDepth)
			{
				// Transition just the depth plane
				check(bClearDepth && !bClearStencil);
				FD3D12DynamicRHI::TransitionResource(CommandListHandle, DepthStencilView->GetResource(), D3D12_RESOURCE_STATE_DEPTH_WRITE, DepthStencilView->GetDepthOnlyViewSubresourceSubset());
			}
			else
			{
				// Transition just the stencil plane
				check(!bClearDepth && bClearStencil);
				FD3D12DynamicRHI::TransitionResource(CommandListHandle, DepthStencilView->GetResource(), D3D12_RESOURCE_STATE_DEPTH_WRITE, DepthStencilView->GetStencilOnlyViewSubresourceSubset());
			}
		}
	}

	if (ClearRTV || ClearDSV)
	{
		CommandListHandle.FlushResourceBarriers();

		if (ClearRTV)
		{
			for (int32 TargetIndex = 0; TargetIndex < BoundRenderTargets.GetNumActiveTargets(); TargetIndex++)
			{
				FD3D12RenderTargetView* RTView = BoundRenderTargets.GetRenderTargetView(TargetIndex);

				if (RTView != nullptr)
				{
					numClears++;
					CommandListHandle->ClearRenderTargetView(RTView->GetView(), (float*)&ClearColorArray[TargetIndex], ClearRectCount, pClearRects);
					CommandListHandle.UpdateResidency(RTView->GetResource());
				}
			}
		}

		if (ClearDSV)
		{
			numClears++;
			CommandListHandle->ClearDepthStencilView(DepthStencilView->GetView(), (D3D12_CLEAR_FLAGS)ClearFlags, Depth, Stencil, ClearRectCount, pClearRects);
			CommandListHandle.UpdateResidency(DepthStencilView->GetResource());
		}
	}

	if (IsDefaultContext())
	{
		GetParentDevice()->RegisterGPUWork(0);
	}

	DEBUG_EXECUTE_COMMAND_LIST(this);
}

void FD3D12CommandContext::RHIBindClearMRTValues(bool bClearColor, bool bClearDepth, bool bClearStencil)
{
	// Not necessary for d3d.
}

// Blocks the CPU until the GPU catches up and goes idle.
void FD3D12DynamicRHI::RHIBlockUntilGPUIdle()
{
	GetRHIDevice()->GetDefaultCommandContext().RHISubmitCommandsHint();

	GetRHIDevice()->GetCommandListManager().WaitForCommandQueueFlush();
	GetRHIDevice()->GetCopyCommandListManager().WaitForCommandQueueFlush();
	GetRHIDevice()->GetAsyncCommandListManager().WaitForCommandQueueFlush();
}

void FD3D12DynamicRHI::RHISubmitCommandsAndFlushGPU()
{
	GetRHIDevice()->GetDefaultCommandContext().RHISubmitCommandsHint();
}

/*
* Returns the total GPU time taken to render the last frame. Same metric as FPlatformTime::Cycles().
*/
uint32 FD3D12DynamicRHI::RHIGetGPUFrameCycles()
{
	return GGPUFrameTime;
}

void FD3D12DynamicRHI::RHIExecuteCommandList(FRHICommandList* CmdList)
{
	check(0); // this path has gone stale and needs updated methods, starting at ERCT_SetScissorRect
}


void FD3D12CommandContext::RHIEnableDepthBoundsTest(bool bEnable, float MinDepth, float MaxDepth)
{
	if (bEnable)
	{
		StateCache.SetDepthBounds(MinDepth, MaxDepth);
	}
	else
	{
		StateCache.SetDepthBounds(0, 1.0f);
	}
}

void FD3D12CommandContext::SetDepthBounds(float MinDepth, float MaxDepth)
{
	static bool bOnce = false;
	if (!bOnce)
	{
		UE_LOG(LogD3D12RHI, Warning, TEXT("RHIEnableDepthBoundsTest not supported on DX12."));
		bOnce = true;
	}
}

void FD3D12CommandContext::RHISubmitCommandsHint()
{
	// Submit the work we have so far, and start a new command list.
	FlushCommands();
}

#define USE_COPY_QUEUE_FOR_RESOURCE_SYNC 1
/*
* When using AFR certain inter-frame dependecies need to be synchronized across all GPUs.
* For example a rendering technique that relies on results from the previous frame (which occured on the other GPU).
*/

void FD3D12CommandContext::RHIWaitForTemporalEffect(const FName& InEffectName)
{
#if USE_COPY_QUEUE_FOR_RESOURCE_SYNC
	check(IsDefaultContext());
	FD3D12Device* Device = GetParentDevice();
	FD3D12Adapter* Adapter = Device->GetParentAdapter();

	if (Adapter->AlternateFrameRenderingEnabled() && AFRSyncTemporalResources)
	{
		FD3D12TemporalEffect* Effect = Adapter->GetTemporalEffect(InEffectName);

		// Execute the current command list so we can have a point to insert a wait
		FlushCommands();

		FD3D12CommandListManager& Manager = bIsAsyncComputeContext ? Device->GetAsyncCommandListManager() :
			Device->GetCommandListManager();

		// Wait for previous copy by splicing in a wait to the queue
		Effect->WaitForPrevious(Manager.GetD3DCommandQueue());
	}
#endif
}

void FD3D12CommandContext::RHIBroadcastTemporalEffect(const FName& InEffectName, FTextureRHIParamRef* InTextures, int32 NumTextures)
{
	check(IsDefaultContext());
	FD3D12Device* Device = GetParentDevice();
	FD3D12Adapter* Adapter = Device->GetParentAdapter();

#if USE_COPY_QUEUE_FOR_RESOURCE_SYNC
	if (Adapter->AlternateFrameRenderingEnabled() && AFRSyncTemporalResources)
	{
		FD3D12TemporalEffect* Effect = Adapter->GetTemporalEffect(InEffectName);

		FD3D12CommandAllocatorManager& TextureStreamingCommandAllocatorManager = Device->GetTextureStreamingCommandAllocatorManager();
		FD3D12CommandAllocator* CurrentCommandAllocator = TextureStreamingCommandAllocatorManager.ObtainCommandAllocator();
		FD3D12CommandListManager& CopyManager = Device->GetCopyCommandListManager();
		FD3D12CommandListHandle hCopyCommandList = CopyManager.ObtainCommandList(*CurrentCommandAllocator);
		hCopyCommandList.SetCurrentOwningContext(this);

		for (int32 i = 0; i < NumTextures; i++)
		{
			// Get the texture for this frame i.e. the one that was just generated
			FD3D12TextureBase* ThisTexture = RetrieveTextureBase(InTextures[i]);
			FD3D12TextureBase* OtherTexture = ((FD3D12TextureBase*)InTextures[i]->GetTextureBaseRHI());

			FD3D12DynamicRHI::TransitionResource(CommandListHandle, ThisTexture->GetResource(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);

			// Broadcast this texture to all other GPUs in the LDA chain
			while (OtherTexture)
			{
				if (OtherTexture != ThisTexture)
				{
					// Note: We transition on the incomming queue as the copy queue will auto promote from common to whatever the resource is used as.
					FD3D12DynamicRHI::TransitionResource(CommandListHandle, OtherTexture->GetResource(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);

					hCopyCommandList.GetCurrentOwningContext()->numCopies++;
					hCopyCommandList->CopyResource(OtherTexture->GetResource()->GetResource(), ThisTexture->GetResource()->GetResource());
				}

				OtherTexture = OtherTexture->GetNextObject();
			}
		}

		// Flush 3D/Compute Queue
		{
			CommandListHandle.FlushResourceBarriers();
			FlushCommands();
		}

		// Kick off the async copy, signalling when done
		{
			// The consuming engine must wait for the producer before executing
			{
				FD3D12CommandListManager& ProducerManager = bIsAsyncComputeContext ? Device->GetAsyncCommandListManager() :
					Device->GetCommandListManager();

				FD3D12Fence& ProducerFence = ProducerManager.GetFence();
				ProducerFence.GpuWait(CopyManager.GetD3DCommandQueue(), ProducerFence.GetLastSignaledFence());
			}

			hCopyCommandList.Close();
			Device->GetCopyCommandListManager().ExecuteCommandList(hCopyCommandList);

			Effect->SignalSyncComplete(Device->GetCopyCommandListManager().GetD3DCommandQueue());
			TextureStreamingCommandAllocatorManager.ReleaseCommandAllocator(CurrentCommandAllocator);
		}
	}
#else
	for (size_t i = 0; i < NumTextures; i++)
	{
		// Get the texture for this frame i.e. the one that was just generated
		FD3D12TextureBase* ThisTexture = RetrieveTextureBase(InTextures[i]);
		FD3D12TextureBase* OtherTexture = ((FD3D12TextureBase*)InTextures[i]->GetTextureBaseRHI());

		FD3D12DynamicRHI::TransitionResource(CommandListHandle, ThisTexture->GetResource(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);

		// Broadcast this texture to all other GPUs in the LDA chain
		while (OtherTexture)
		{
			if (OtherTexture != ThisTexture)
			{
				FD3D12DynamicRHI::TransitionResource(CommandListHandle, OtherTexture->GetResource(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);

				CommandListHandle.GetCurrentOwningContext()->numCopies++;
				CommandListHandle->CopyResource(OtherTexture->GetResource()->GetResource(), ThisTexture->GetResource()->GetResource());
			}

			OtherTexture = OtherTexture->GetNextObject();
		}
	}
#endif
}
// NVCHANGE_BEGIN: Add HBAO+
#if WITH_GFSDK_SSAO

void FD3D12CommandContext::RHIRenderHBAO(
	const FTextureRHIParamRef SceneDepthTextureRHI,
	const FMatrix& ProjectionMatrix,
	const FTextureRHIParamRef SceneNormalTextureRHI,
	const FMatrix& ViewMatrix,
	const FTextureRHIParamRef SceneColorTextureRHI,
	const GFSDK_SSAO_Parameters& BaseParams
)
{
	// Empty method because HBAO+ doesn't support DX12 yet.
	// Just override the base so that the engine doesn't crash.
}

#endif
// NVCHANGE_END: Add HBAO+