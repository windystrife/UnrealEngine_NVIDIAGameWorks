// Copyright (c) 2015, NVIDIA CORPORATION. All rights reserved.

#include "D3D12RHIPrivate.h"

// NVCHANGE_BEGIN: Add VXGI
#if WITH_GFSDK_VXGI


VXGI::IGlobalIllumination* FD3D12DynamicRHI::RHIVXGIGetInterface()
{
	return VxgiInterface;
}

void FD3D12DynamicRHI::CreateVxgiInterface()
{
	check(!VxgiRendererD3D12);
	VxgiRendererD3D12 = new NVRHI::FRendererInterfaceD3D12(GetRHIDevice());
	check(VxgiRendererD3D12);

	VXGI::GIParameters Params;
	Params.rendererInterface = VxgiRendererD3D12;
	Params.errorCallback = VxgiRendererD3D12;
	Params.perfMonitor = VxgiRendererD3D12;

	check(!VxgiInterface);
	auto Status = VXGI::VFX_VXGI_CreateGIObject(Params, &VxgiInterface);
	check(VXGI_SUCCEEDED(Status));

	VXGI::Version VxgiVersion;
	UE_LOG(LogD3D12RHI, Log, TEXT("VXGI: Version %u.%u.%u.%u"), VxgiVersion.Major, VxgiVersion.Minor, VxgiVersion.Branch, VxgiVersion.Revision);

	bVxgiVoxelizationParametersSet = false;
}

void FD3D12DynamicRHI::ReleaseVxgiInterface()
{
	if (VxgiInterface)
	{
		VXGI::VFX_VXGI_DestroyGIObject(VxgiInterface);
		VxgiInterface = NULL;
	}

	if (VxgiRendererD3D12)
	{
		delete VxgiRendererD3D12;
		VxgiRendererD3D12 = NULL;
	}

	bVxgiVoxelizationParametersSet = false;
}

void FD3D12DynamicRHI::RHIVXGISetVoxelizationParameters(const VXGI::VoxelizationParameters& Parameters)
{
	// If the cvars define a new set of parameters, see if it's valid and try to set them
	if(!bVxgiVoxelizationParametersSet || Parameters != VxgiVoxelizationParameters)
	{
		VxgiRendererD3D12->setTreatErrorsAsFatal(false);
		auto Status = VxgiInterface->validateVoxelizationParameters(Parameters);
		VxgiRendererD3D12->setTreatErrorsAsFatal(true);

		if(VXGI_SUCCEEDED(Status))
		{
			// If the call to setVoxelizationParameters fails, VXGI will be in an unititialized state, so set bVxgiVoxelizationParametersSet to false
			bVxgiVoxelizationParametersSet = VXGI_SUCCEEDED(VxgiInterface->setVoxelizationParameters(Parameters));
		}
	}

	// If the new parameters are invalid, set the default parameters - they should always work
	if(!bVxgiVoxelizationParametersSet)
	{
		VXGI::VoxelizationParameters DefaultVParams;
        DefaultVParams.persistentVoxelData = false;

		auto Status = VxgiInterface->setVoxelizationParameters(DefaultVParams);
		check(VXGI_SUCCEEDED(Status));
		bVxgiVoxelizationParametersSet = true;
	}

	// Regardless of whether the new parameters are valid, store them to avoid re-initializing VXGI on the next frame
	VxgiVoxelizationParameters = Parameters;
}

void FD3D12DynamicRHI::RHIVXGISetPixelShaderResourceAttributes(NVRHI::ShaderHandle PixelShader, const TArray<uint8>& ShaderResourceTable, bool bUsesGlobalCB)
{
	VxgiRendererD3D12->setPixelShaderResourceAttributes(PixelShader, ShaderResourceTable, bUsesGlobalCB);
}

void FD3D12DynamicRHI::RHIVXGIApplyDrawStateOverrideShaders(const NVRHI::DrawCallState& DrawCallState, const FBoundShaderStateInput* BoundShaderStateInput, EPrimitiveType PrimitiveTypeOverride)
{
	VxgiRendererD3D12->applyState(DrawCallState, BoundShaderStateInput, PrimitiveTypeOverride);
	VxgiRendererD3D12->applyResources(DrawCallState);
}

void FD3D12DynamicRHI::RHIVXGIApplyShaderResources(const NVRHI::DrawCallState& DrawCallState)
{
	VxgiRendererD3D12->applyResources(DrawCallState);
}

void FD3D12DynamicRHI::RHIVXGISetCommandList(FRHICommandList* RHICommandList) 
{ 
	VxgiRendererD3D12->setRHICommandList(RHICommandList);
}

void FD3D12CommandContext::RHIVXGICleanupAfterVoxelization()
{
	FlushCommands(false);
}

FRHITexture* FD3D12DynamicRHI::GetRHITextureFromVXGI(NVRHI::TextureHandle texture)
{
	return VxgiRendererD3D12->getRHITexture(texture);
}

NVRHI::TextureHandle FD3D12DynamicRHI::GetVXGITextureFromRHI(FRHITexture* texture)
{
	return VxgiRendererD3D12->getTextureFromRHI(texture);
}

bool FD3D12DynamicRHI::RHISetExtensionsForNextShader(const void* const* Extensions, uint32 NumExtensions)
{
	NvidiaShaderExtensions.SetNum(NumExtensions);
	if(NumExtensions > 0)
		FMemory::Memcpy(NvidiaShaderExtensions.GetData(), Extensions, NumExtensions * sizeof(void*));
	return true;
}

void FD3D12CommandContext::RHISetViewportsAndScissorRects(uint32 Count, const FViewportBounds* Viewports, const FScissorRect* ScissorRects)
{
	StateCache.SetViewports(Count, (D3D12_VIEWPORT*)Viewports);
	StateCache.SetScissorRects(Count, (D3D12_RECT*)ScissorRects);
}

void FD3D12CommandContext::RHIDispatchIndirectComputeShaderStructured(FStructuredBufferRHIParamRef ArgumentBufferRHI, uint32 ArgumentOffset)
{
	// Can't use m_RHICmdList->DispatchIndirectComputeShader(...) method here because:
	// - It requires the argument buffer to be a VertexBuffer, and it's a StructuredBuffer so far as UE is concerned;
	// - It multiplies the offset by 20, and VXGI uses different offsets.
	// So below is a modified version of FD3D12CommandContext::RHIDispatchIndirectComputeShader method.


	FD3D12StructuredBuffer* ArgumentBuffer = FD3D12DynamicRHI::ResourceCast(ArgumentBufferRHI);

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

	StateCache.ApplyState<true>();

	FD3D12DynamicRHI::TransitionResource(CommandListHandle, Location.GetResource(), D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
	CommandListHandle.FlushResourceBarriers();

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

	StateCache.SetComputeShader(nullptr);
}

void FD3D12CommandContext::RHICopyStructuredBufferData(FStructuredBufferRHIParamRef DestBufferRHI, uint32 DestOffset, FStructuredBufferRHIParamRef SrcBufferRHI, uint32 SrcOffset, uint32 DataSize) 
{
	auto DestBuffer = FD3D12DynamicRHI::ResourceCast(DestBufferRHI);
	auto SrcBuffer = FD3D12DynamicRHI::ResourceCast(SrcBufferRHI);

	FD3D12DynamicRHI::TransitionResource(CommandListHandle, DestBuffer->ResourceLocation.GetResource(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);

	if(SrcBuffer->ResourceLocation.GetResource()->RequiresResourceStateTracking())
		FD3D12DynamicRHI::TransitionResource(CommandListHandle, SrcBuffer->ResourceLocation.GetResource(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
	
	numCopies++;
	CommandListHandle->CopyBufferRegion(
		DestBuffer->ResourceLocation.GetResource()->GetResource(), DestOffset + DestBuffer->ResourceLocation.GetOffsetFromBaseOfResource(),
		SrcBuffer->ResourceLocation.GetResource()->GetResource(), SrcOffset + SrcBuffer->ResourceLocation.GetOffsetFromBaseOfResource(),
		DataSize);

	DEBUG_EXECUTE_COMMAND_LIST(this);
}


#endif
// NVCHANGE_END: Add VXGI
