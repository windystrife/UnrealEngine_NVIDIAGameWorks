// Copyright (c) 2015, NVIDIA CORPORATION. All rights reserved.

#include "D3D11RHIPrivate.h"
#include "RHIStaticStates.h"

// NVCHANGE_BEGIN: Add VXGI
#if WITH_GFSDK_VXGI

#include "GFSDK_NVRHI.h"

VXGI::IGlobalIllumination* FD3D11DynamicRHI::RHIVXGIGetInterface()
{
	return VxgiInterface;
}

void FD3D11DynamicRHI::CreateVxgiInterface()
{
	check(!VxgiRendererD3D11);
	VxgiRendererD3D11 = new NVRHI::FRendererInterfaceD3D11(GetDevice());
	check(VxgiRendererD3D11);

	VXGI::GIParameters Params;
	Params.rendererInterface = VxgiRendererD3D11;
	Params.errorCallback = VxgiRendererD3D11;
	Params.perfMonitor = VxgiRendererD3D11;

	check(!VxgiInterface);
	auto Status = VXGI::VFX_VXGI_CreateGIObject(Params, &VxgiInterface);
	check(VXGI_SUCCEEDED(Status));

	VXGI::Version VxgiVersion;
	UE_LOG(LogD3D11RHI, Log, TEXT("VXGI: Version %u.%u.%u.%u"), VxgiVersion.Major, VxgiVersion.Minor, VxgiVersion.Branch, VxgiVersion.Revision);

	bVxgiVoxelizationParametersSet = false;
}

void FD3D11DynamicRHI::ReleaseVxgiInterface()
{
	if (VxgiInterface)
	{
		VXGI::VFX_VXGI_DestroyGIObject(VxgiInterface);
		VxgiInterface = NULL;
	}

	if (VxgiRendererD3D11)
	{
		delete VxgiRendererD3D11;
		VxgiRendererD3D11 = NULL;
	}

	bVxgiVoxelizationParametersSet = false;
}

void FD3D11DynamicRHI::RHIVXGISetVoxelizationParameters(const VXGI::VoxelizationParameters& Parameters)
{
	// If the cvars define a new set of parameters, see if it's valid and try to set them
	if(!bVxgiVoxelizationParametersSet || Parameters != VxgiVoxelizationParameters)
	{
		VxgiRendererD3D11->setTreatErrorsAsFatal(false);
		auto Status = VxgiInterface->validateVoxelizationParameters(Parameters);
		VxgiRendererD3D11->setTreatErrorsAsFatal(true);

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

void FD3D11DynamicRHI::RHIVXGISetPixelShaderResourceAttributes(NVRHI::ShaderHandle PixelShader, const TArray<uint8>& ShaderResourceTable, bool bUsesGlobalCB)
{
	VxgiRendererD3D11->setPixelShaderResourceAttributes(PixelShader, ShaderResourceTable, bUsesGlobalCB);
}

void FD3D11DynamicRHI::RHIVXGIApplyDrawStateOverrideShaders(const NVRHI::DrawCallState& DrawCallState, const FBoundShaderStateInput* BoundShaderStateInput, EPrimitiveType PrimitiveTypeOverride)
{
	VxgiRendererD3D11->applyState(DrawCallState, BoundShaderStateInput, PrimitiveTypeOverride);
	VxgiRendererD3D11->applyResources(DrawCallState);
}

void FD3D11DynamicRHI::RHIVXGIApplyShaderResources(const NVRHI::DrawCallState& DrawCallState)
{
	VxgiRendererD3D11->applyResources(DrawCallState);
}

void FD3D11DynamicRHI::RHIVXGISetCommandList(FRHICommandList* RHICommandList) 
{ 
	VxgiRendererD3D11->setRHICommandList(RHICommandList);
}

void FD3D11DynamicRHI::RHIVXGICleanupAfterVoxelization()
{
	RHISetRenderTargets(0, nullptr, nullptr, 0, nullptr);
	RHISetRasterizerState(TStaticRasterizerState<FM_Solid, CM_None>::GetRHI());
	RHISetBlendState(TStaticBlendStateWriteMask<>::GetRHI(), FLinearColor());
	RHISetDepthStencilState(TStaticDepthStencilState<false, CF_DepthNearOrEqual>::GetRHI(), 0);
	RHISetScissorRect(false, 0, 0, 0, 0);
}

FRHITexture* FD3D11DynamicRHI::GetRHITextureFromVXGI(NVRHI::TextureHandle texture)
{
	return VxgiRendererD3D11->getRHITexture(texture);
}

NVRHI::TextureHandle FD3D11DynamicRHI::GetVXGITextureFromRHI(FRHITexture* texture)
{
	return VxgiRendererD3D11->getTextureFromRHI(texture);
}

void FD3D11DynamicRHI::RHIVXGIGetGPUTime(float& OutWorldSpaceTime, float& OutScreenSpaceTime)
{
	OutWorldSpaceTime = GPUProfilingData.VxgiWorldSpaceTime;
	OutScreenSpaceTime = GPUProfilingData.VxgiScreenSpaceTime;

	GPUProfilingData.bRequestProfileForStatUnitVxgi = true;
}

void FD3D11DynamicRHI::RHISetViewportsAndScissorRects(uint32 Count, const FViewportBounds* InViewports, const FScissorRect* InScissorRects)
{
	StateCache.SetViewports(Count, (D3D11_VIEWPORT*)InViewports);
	Direct3DDeviceIMContext->RSSetScissorRects(Count, (D3D11_RECT*)InScissorRects);
}

void FD3D11DynamicRHI::RHIDispatchIndirectComputeShaderStructured(FStructuredBufferRHIParamRef ArgumentBufferRHI, uint32 ArgumentOffset)
{
	FComputeShaderRHIParamRef ComputeShaderRHI = GetCurrentComputeShader();
	FD3D11ComputeShader* ComputeShader = ResourceCast(ComputeShaderRHI);
	FD3D11StructuredBuffer* ArgumentBuffer = ResourceCast(ArgumentBufferRHI);

	GPUProfilingData.RegisterGPUWork(1);

	StateCache.SetComputeShader(ComputeShader->Resource);

	if (ComputeShader->bShaderNeedsGlobalConstantBuffer)
	{
		CommitComputeShaderConstants();
	}
	CommitComputeResourceTables(ComputeShader);

	Direct3DDeviceIMContext->DispatchIndirect(ArgumentBuffer->Resource,ArgumentOffset);
	StateCache.SetComputeShader(nullptr);
}

void FD3D11DynamicRHI::RHICopyStructuredBufferData(FStructuredBufferRHIParamRef DestBufferRHI, uint32 DestOffset, FStructuredBufferRHIParamRef SrcBufferRHI, uint32 SrcOffset, uint32 DataSize) 
{
	auto DestBuffer = ResourceCast(DestBufferRHI);
	auto SrcBuffer = ResourceCast(SrcBufferRHI);

	GPUProfilingData.RegisterGPUWork(1);

	D3D11_BOX SrcBox = { SrcOffset, 0, 0, SrcOffset + DataSize, 1, 1 };
	Direct3DDeviceIMContext->CopySubresourceRegion(DestBuffer->Resource, 0, DestOffset, 0, 0, SrcBuffer->Resource, 0, &SrcBox);
}

void FD3D11DynamicRHI::RHIExecuteVxgiRenderingCommand(NVRHI::IRenderThreadCommand* Command)
{
	check(Command != nullptr);

	Command->executeAndDispose();
}


#endif
// NVCHANGE_END: Add VXGI
