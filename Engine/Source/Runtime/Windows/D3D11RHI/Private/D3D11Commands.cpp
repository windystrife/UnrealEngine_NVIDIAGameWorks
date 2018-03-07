// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	D3D11Commands.cpp: D3D RHI commands implementation.
=============================================================================*/

#include "D3D11RHIPrivate.h"
#include "Windows/D3D11RHIPrivateUtil.h"
#include "StaticBoundShaderState.h"
#include "GlobalShader.h"
#include "OneColorShader.h"
#include "RHICommandList.h"
#include "RHIStaticStates.h"
#include "ShaderParameterUtils.h"
#include "SceneUtils.h"
#include "EngineGlobals.h"

#if PLATFORM_DESKTOP
// For Depth Bounds Test interface
#include "AllowWindowsPlatformTypes.h"
	#include "nvapi.h"
	#include "amd_ags.h"
#include "HideWindowsPlatformTypes.h"
#endif

#define DECLARE_ISBOUNDSHADER(ShaderType) inline void ValidateBoundShader(FD3D11StateCache& InStateCache, F##ShaderType##RHIParamRef ShaderType##RHI) \
{ \
	ID3D11##ShaderType* CachedShader; \
	InStateCache.Get##ShaderType(&CachedShader); \
	FD3D11##ShaderType* ShaderType = FD3D11DynamicRHI::ResourceCast(ShaderType##RHI); \
	ensureMsgf(CachedShader == ShaderType->Resource, TEXT("Parameters are being set for a %s which is not currently bound"), TEXT( #ShaderType )); \
	if (CachedShader) { CachedShader->Release(); } \
}

DECLARE_ISBOUNDSHADER(VertexShader)
DECLARE_ISBOUNDSHADER(PixelShader)
DECLARE_ISBOUNDSHADER(GeometryShader)
DECLARE_ISBOUNDSHADER(HullShader)
DECLARE_ISBOUNDSHADER(DomainShader)
DECLARE_ISBOUNDSHADER(ComputeShader)


#if DO_CHECK
#define VALIDATE_BOUND_SHADER(s) ValidateBoundShader(StateCache, s)
#else
#define VALIDATE_BOUND_SHADER(s)
#endif

int32 GEnableDX11TransitionChecks = 0;
static FAutoConsoleVariableRef CVarDX11TransitionChecks(
	TEXT("r.TransitionChecksEnableDX11"),
	GEnableDX11TransitionChecks,
	TEXT("Enables transition checks in the DX11 RHI."),
	ECVF_Default
	);

int32 GUnbindResourcesBetweenDrawsInDX11 = 0;
static FAutoConsoleVariableRef CVarUnbindResourcesBetweenDrawsInDX11(
	TEXT("r.UnbindResourcesBetweenDrawsInDX11"),
	GUnbindResourcesBetweenDrawsInDX11,
	TEXT("Unbind resources between material changes in DX11."),
	ECVF_Default
	);

void FD3D11BaseShaderResource::SetDirty(bool bInDirty, uint32 CurrentFrame)
{
	bDirty = bInDirty;
	if (bDirty)
	{
		LastFrameWritten = CurrentFrame;
	}
	ensureMsgf((GEnableDX11TransitionChecks == 0) || !(CurrentGPUAccess == EResourceTransitionAccess::EReadable && bDirty), TEXT("ShaderResource is dirty, but set to Readable."));
}

//MultiGPU
void FD3D11DynamicRHI::RHIBeginUpdateMultiFrameResource(FTextureRHIParamRef RHITexture)
{
	if (!IsRHIDeviceNVIDIA() || GNumActiveGPUsForRendering == 1) return;

	FD3D11TextureBase* Texture = GetD3D11TextureFromRHITexture(RHITexture);

	if (!Texture)
	{
		return;
	}

	if (!Texture->GetIHVResourceHandle())
	{
		// get a resource handle for this texture
		void* IHVHandle = nullptr;
		NvAPI_D3D_GetObjectHandleForResource(Direct3DDevice, Texture->GetResource(), (NVDX_ObjectHandle*)&(IHVHandle));
		Texture->SetIHVResourceHandle(IHVHandle);
	}
	
	RHIPushEvent(TEXT("BeginMFUpdate"), FColor::Black);
	NvAPI_D3D_BeginResourceRendering(Direct3DDevice, (NVDX_ObjectHandle)Texture->GetIHVResourceHandle(), 0);
	RHIPopEvent();
}

void FD3D11DynamicRHI::RHIEndUpdateMultiFrameResource(FTextureRHIParamRef RHITexture)
{
	if (!IsRHIDeviceNVIDIA() || GNumActiveGPUsForRendering == 1) return;

	FD3D11TextureBase* Texture = GetD3D11TextureFromRHITexture(RHITexture);

	if (!Texture || !Texture->GetIHVResourceHandle())
	{
		return;
	}

	RHIPushEvent(TEXT("EndMFUpdate"), FColor::Black);
	NvAPI_D3D_EndResourceRendering(Direct3DDevice, (NVDX_ObjectHandle)Texture->GetIHVResourceHandle(), 0);
	RHIPopEvent();	
}

void FD3D11DynamicRHI::RHIBeginUpdateMultiFrameResource(FUnorderedAccessViewRHIParamRef UAVRHI)
{
	if (!IsRHIDeviceNVIDIA() || GNumActiveGPUsForRendering == 1) return;

	FD3D11UnorderedAccessView* UAV = ResourceCast(UAVRHI);
	
	if (!UAV)
	{
		return;
	}

	if (!UAV->IHVResourceHandle)
	{
		// get a resource handle for this texture		
		ID3D11Resource* D3DResource = nullptr;
		UAV->View->GetResource(&D3DResource);
		NvAPI_D3D_GetObjectHandleForResource(Direct3DDevice, D3DResource, (NVDX_ObjectHandle*)&(UAV->IHVResourceHandle));
	}
	
	RHIPushEvent(TEXT("BeginMFUpdateUAV"), FColor::Black);
	NvAPI_D3D_BeginResourceRendering(Direct3DDevice, (NVDX_ObjectHandle)UAV->IHVResourceHandle, 0);
	RHIPopEvent();
}

void FD3D11DynamicRHI::RHIEndUpdateMultiFrameResource(FUnorderedAccessViewRHIParamRef UAVRHI)
{
	if (!IsRHIDeviceNVIDIA() || GNumActiveGPUsForRendering == 1) return;

	FD3D11UnorderedAccessView* UAV = ResourceCast(UAVRHI);

	if (!UAV || !UAV->IHVResourceHandle)
	{
		return;
	}

	RHIPushEvent(TEXT("EndMFUpdateUAV"), FColor::Black);
	NvAPI_D3D_EndResourceRendering(Direct3DDevice, (NVDX_ObjectHandle)UAV->IHVResourceHandle, 0);
	RHIPopEvent();
}

// Vertex state.
void FD3D11DynamicRHI::RHISetStreamSource(uint32 StreamIndex,FVertexBufferRHIParamRef VertexBufferRHI,uint32 Stride,uint32 Offset)
{
	FD3D11VertexBuffer* VertexBuffer = ResourceCast(VertexBufferRHI);

	ID3D11Buffer* D3DBuffer = VertexBuffer ? VertexBuffer->Resource : NULL;
	StateCache.SetStreamSource(D3DBuffer, StreamIndex, Stride, Offset);
}

void FD3D11DynamicRHI::RHISetStreamSource(uint32 StreamIndex, FVertexBufferRHIParamRef VertexBufferRHI, uint32 Offset)
{
	FD3D11VertexBuffer* VertexBuffer = ResourceCast(VertexBufferRHI);

	ID3D11Buffer* D3DBuffer = VertexBuffer ? VertexBuffer->Resource : NULL;
	StateCache.SetStreamSource(D3DBuffer, StreamIndex, Offset);
}

void FD3D11DynamicRHI::RHISetStreamOutTargets(uint32 NumTargets, const FVertexBufferRHIParamRef* VertexBuffers, const uint32* Offsets)
{
	ID3D11Buffer* D3DVertexBuffers[D3D11_SO_BUFFER_SLOT_COUNT] = {0};

	if (VertexBuffers)
	{
		for (uint32 BufferIndex = 0; BufferIndex < NumTargets; BufferIndex++)
		{
			D3DVertexBuffers[BufferIndex] = VertexBuffers[BufferIndex] ? ((FD3D11VertexBuffer*)VertexBuffers[BufferIndex])->Resource : NULL;
		}
	}

	Direct3DDeviceIMContext->SOSetTargets(NumTargets, D3DVertexBuffers, Offsets);
}

// Rasterizer state.
void FD3D11DynamicRHI::RHISetRasterizerState(FRasterizerStateRHIParamRef NewStateRHI)
{
	FD3D11RasterizerState* NewState = ResourceCast(NewStateRHI);
	StateCache.SetRasterizerState(NewState->Resource);
}

void FD3D11DynamicRHI::RHISetComputeShader(FComputeShaderRHIParamRef ComputeShaderRHI)
{
	FD3D11ComputeShader* ComputeShader = ResourceCast(ComputeShaderRHI);
	SetCurrentComputeShader(ComputeShaderRHI);
}

void FD3D11DynamicRHI::RHIDispatchComputeShader(uint32 ThreadGroupCountX, uint32 ThreadGroupCountY, uint32 ThreadGroupCountZ) 
{ 
	FComputeShaderRHIParamRef ComputeShaderRHI = GetCurrentComputeShader();
	FD3D11ComputeShader* ComputeShader = ResourceCast(ComputeShaderRHI);

	StateCache.SetComputeShader(ComputeShader->Resource);

	GPUProfilingData.RegisterGPUWork(1);	

	if (ComputeShader->bShaderNeedsGlobalConstantBuffer)
	{
		CommitComputeShaderConstants();
	}
	CommitComputeResourceTables(ComputeShader);
	
	Direct3DDeviceIMContext->Dispatch(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);	
	StateCache.SetComputeShader(nullptr);
}

void FD3D11DynamicRHI::RHIDispatchIndirectComputeShader(FVertexBufferRHIParamRef ArgumentBufferRHI, uint32 ArgumentOffset) 
{ 
	FComputeShaderRHIParamRef ComputeShaderRHI = GetCurrentComputeShader();
	FD3D11ComputeShader* ComputeShader = ResourceCast(ComputeShaderRHI);
	FD3D11VertexBuffer* ArgumentBuffer = ResourceCast(ArgumentBufferRHI);

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

void FD3D11DynamicRHI::RHISetViewport(uint32 MinX,uint32 MinY,float MinZ,uint32 MaxX,uint32 MaxY,float MaxZ)
{
	// These are the maximum viewport extents for D3D11. Exceeding them leads to badness.
	check(MinX <= (uint32)D3D11_VIEWPORT_BOUNDS_MAX);
	check(MinY <= (uint32)D3D11_VIEWPORT_BOUNDS_MAX);
	check(MaxX <= (uint32)D3D11_VIEWPORT_BOUNDS_MAX);
	check(MaxY <= (uint32)D3D11_VIEWPORT_BOUNDS_MAX);

	D3D11_VIEWPORT Viewport = { (float)MinX, (float)MinY, (float)MaxX - MinX, (float)MaxY - MinY, MinZ, MaxZ };
	//avoid setting a 0 extent viewport, which the debug runtime doesn't like
	if (Viewport.Width > 0 && Viewport.Height > 0)
	{
		StateCache.SetViewport(Viewport);
		SetScissorRectIfRequiredWhenSettingViewport(MinX, MinY, MaxX, MaxY);
	}
}

void FD3D11DynamicRHI::RHISetScissorRect(bool bEnable,uint32 MinX,uint32 MinY,uint32 MaxX,uint32 MaxY)
{
	if(bEnable)
	{
		D3D11_RECT ScissorRect;
		ScissorRect.left = MinX;
		ScissorRect.right = MaxX;
		ScissorRect.top = MinY;
		ScissorRect.bottom = MaxY;
		Direct3DDeviceIMContext->RSSetScissorRects(1,&ScissorRect);
	}
	else
	{
		D3D11_RECT ScissorRect;
		ScissorRect.left = 0;
		ScissorRect.right = GetMax2DTextureDimension();
		ScissorRect.top = 0;
		ScissorRect.bottom = GetMax2DTextureDimension();
		Direct3DDeviceIMContext->RSSetScissorRects(1,&ScissorRect);
	}
}

/**
* Set bound shader state. This will set the vertex decl/shader, and pixel shader
* @param BoundShaderState - state resource
*/
void FD3D11DynamicRHI::RHISetBoundShaderState( FBoundShaderStateRHIParamRef BoundShaderStateRHI)
{
	// Non-PSO
	PSOPrimitiveType = PT_Num;

	FD3D11BoundShaderState* BoundShaderState = ResourceCast(BoundShaderStateRHI);

	StateCache.SetStreamStrides(BoundShaderState->StreamStrides);
	StateCache.SetInputLayout(BoundShaderState->InputLayout);
	StateCache.SetVertexShader(BoundShaderState->VertexShader);
	StateCache.SetPixelShader(BoundShaderState->PixelShader);

	StateCache.SetHullShader(BoundShaderState->HullShader);
	StateCache.SetDomainShader(BoundShaderState->DomainShader);
	StateCache.SetGeometryShader(BoundShaderState->GeometryShader);

	if(BoundShaderState->HullShader != NULL && BoundShaderState->DomainShader != NULL)
	{
		bUsingTessellation = true;
	}
	else
	{
		bUsingTessellation = false;
	}

	// @TODO : really should only discard the constants if the shader state has actually changed.
	bDiscardSharedConstants = true;

	// Prevent transient bound shader states from being recreated for each use by keeping a history of the most recently used bound shader states.
	// The history keeps them alive, and the bound shader state cache allows them to be reused if needed.
	BoundShaderStateHistory.Add(BoundShaderState);

	// Shader changed so all resource tables are dirty
	DirtyUniformBuffers[SF_Vertex] = 0xffff;
	DirtyUniformBuffers[SF_Pixel] = 0xffff;
	DirtyUniformBuffers[SF_Hull] = 0xffff;
	DirtyUniformBuffers[SF_Domain] = 0xffff;
	DirtyUniformBuffers[SF_Geometry] = 0xffff;

	// Shader changed.  All UB's must be reset by high level code to match other platforms anway.
	// Clear to catch those bugs, and bugs with stale UB's causing layout mismatches.
	// Release references to bound uniform buffers.
	for (int32 Frequency = 0; Frequency < SF_NumFrequencies; ++Frequency)
	{
		for (int32 BindIndex = 0; BindIndex < MAX_UNIFORM_BUFFERS_PER_SHADER_STAGE; ++BindIndex)
		{
			BoundUniformBuffers[Frequency][BindIndex].SafeRelease();
		}
	}

	if (GUnbindResourcesBetweenDrawsInDX11)
	{
		ClearAllShaderResources();
	}
}

void FD3D11DynamicRHI::RHISetShaderTexture(FVertexShaderRHIParamRef VertexShaderRHI,uint32 TextureIndex,FTextureRHIParamRef NewTextureRHI)
{
	VALIDATE_BOUND_SHADER(VertexShaderRHI);

	FD3D11TextureBase* NewTexture = GetD3D11TextureFromRHITexture(NewTextureRHI);
	ID3D11ShaderResourceView* ShaderResourceView = NewTexture ? NewTexture->GetShaderResourceView() : NULL;

	if ((NewTexture == NULL) || (NewTexture->GetRenderTargetView(0, 0) != NULL) || (NewTexture->HasDepthStencilView()))
	{
		SetShaderResourceView<SF_Vertex>(NewTexture, ShaderResourceView, TextureIndex, NewTextureRHI ? NewTextureRHI->GetName() : NAME_None, FD3D11StateCache::SRV_Dynamic);
	}
	else
	{
		SetShaderResourceView<SF_Vertex>(NewTexture, ShaderResourceView, TextureIndex, NewTextureRHI->GetName(), FD3D11StateCache::SRV_Static);
	}
}

void FD3D11DynamicRHI::RHISetShaderTexture(FHullShaderRHIParamRef HullShaderRHI,uint32 TextureIndex,FTextureRHIParamRef NewTextureRHI)
{
	VALIDATE_BOUND_SHADER(HullShaderRHI);

	FD3D11TextureBase* NewTexture = GetD3D11TextureFromRHITexture(NewTextureRHI);
	ID3D11ShaderResourceView* ShaderResourceView = NewTexture ? NewTexture->GetShaderResourceView() : NULL;

	if ((NewTexture == NULL) || (NewTexture->GetRenderTargetView(0, 0) != NULL) || (NewTexture->HasDepthStencilView()))
	{
		SetShaderResourceView<SF_Hull>(NewTexture, ShaderResourceView, TextureIndex, NewTextureRHI ? NewTextureRHI->GetName() : NAME_None, FD3D11StateCache::SRV_Dynamic);
	}
	else
	{
		SetShaderResourceView<SF_Hull>(NewTexture, ShaderResourceView, TextureIndex, NewTextureRHI->GetName(), FD3D11StateCache::SRV_Static);
	}
}

void FD3D11DynamicRHI::RHISetShaderTexture(FDomainShaderRHIParamRef DomainShaderRHI,uint32 TextureIndex,FTextureRHIParamRef NewTextureRHI)
{
	VALIDATE_BOUND_SHADER(DomainShaderRHI);

	FD3D11TextureBase* NewTexture = GetD3D11TextureFromRHITexture(NewTextureRHI);
	ID3D11ShaderResourceView* ShaderResourceView = NewTexture ? NewTexture->GetShaderResourceView() : NULL;

	if ((NewTexture == NULL) || (NewTexture->GetRenderTargetView(0, 0) != NULL) || (NewTexture->HasDepthStencilView()))
	{
		SetShaderResourceView<SF_Domain>(NewTexture, ShaderResourceView, TextureIndex, NewTextureRHI ? NewTextureRHI->GetName() : NAME_None, FD3D11StateCache::SRV_Dynamic);
	}
	else
	{
		SetShaderResourceView<SF_Domain>(NewTexture, ShaderResourceView, TextureIndex, NewTextureRHI->GetName(), FD3D11StateCache::SRV_Static);
	}
}

void FD3D11DynamicRHI::RHISetShaderTexture(FGeometryShaderRHIParamRef GeometryShaderRHI,uint32 TextureIndex,FTextureRHIParamRef NewTextureRHI)
{
	VALIDATE_BOUND_SHADER(GeometryShaderRHI);

	FD3D11TextureBase* NewTexture = GetD3D11TextureFromRHITexture(NewTextureRHI);
	ID3D11ShaderResourceView* ShaderResourceView = NewTexture ? NewTexture->GetShaderResourceView() : NULL;

	if ((NewTexture == NULL) || (NewTexture->GetRenderTargetView(0, 0) != NULL) || (NewTexture->HasDepthStencilView()))
	{
		SetShaderResourceView<SF_Geometry>(NewTexture, ShaderResourceView, TextureIndex, NewTextureRHI ? NewTextureRHI->GetName() : NAME_None, FD3D11StateCache::SRV_Dynamic);
	}
	else
	{
		SetShaderResourceView<SF_Geometry>(NewTexture, ShaderResourceView, TextureIndex, NewTextureRHI->GetName(), FD3D11StateCache::SRV_Static);
	}
}

void FD3D11DynamicRHI::RHISetShaderTexture(FPixelShaderRHIParamRef PixelShaderRHI,uint32 TextureIndex,FTextureRHIParamRef NewTextureRHI)
{

	VALIDATE_BOUND_SHADER(PixelShaderRHI);

	FD3D11TextureBase* NewTexture = GetD3D11TextureFromRHITexture(NewTextureRHI);
	ID3D11ShaderResourceView* ShaderResourceView = NewTexture ? NewTexture->GetShaderResourceView() : NULL;
	if ((NewTexture == NULL) || (NewTexture->GetRenderTargetView(0, 0) != NULL) || (NewTexture->HasDepthStencilView()))
	{
		SetShaderResourceView<SF_Pixel>(NewTexture, ShaderResourceView, TextureIndex, NewTextureRHI ? NewTextureRHI->GetName() : NAME_None, FD3D11StateCache::SRV_Dynamic);
	}
	else
	{
		SetShaderResourceView<SF_Pixel>(NewTexture, ShaderResourceView, TextureIndex, NewTextureRHI->GetName(), FD3D11StateCache::SRV_Static);
	}
}

void FD3D11DynamicRHI::RHISetShaderTexture(FComputeShaderRHIParamRef ComputeShaderRHI,uint32 TextureIndex,FTextureRHIParamRef NewTextureRHI)
{
	//VALIDATE_BOUND_SHADER(ComputeShaderRHI);

	FD3D11TextureBase* NewTexture = GetD3D11TextureFromRHITexture(NewTextureRHI);
	ID3D11ShaderResourceView* ShaderResourceView = NewTexture ? NewTexture->GetShaderResourceView() : NULL;

	if ((NewTexture == NULL) || (NewTexture->GetRenderTargetView(0, 0) != NULL) || (NewTexture->HasDepthStencilView()))
	{
		SetShaderResourceView<SF_Compute>(NewTexture, ShaderResourceView, TextureIndex, NewTextureRHI ? NewTextureRHI->GetName() : NAME_None, FD3D11StateCache::SRV_Dynamic);
	}
	else
	{
		SetShaderResourceView<SF_Compute>(NewTexture, ShaderResourceView, TextureIndex, NewTextureRHI->GetName(), FD3D11StateCache::SRV_Static);
	}
}

void FD3D11DynamicRHI::RHISetUAVParameter(FComputeShaderRHIParamRef ComputeShaderRHI,uint32 UAVIndex,FUnorderedAccessViewRHIParamRef UAVRHI)
{
	//VALIDATE_BOUND_SHADER(ComputeShaderRHI);

	FD3D11UnorderedAccessView* UAV = ResourceCast(UAVRHI);

	if(UAV)
	{
		ConditionalClearShaderResource(UAV->Resource);		

		//check it's safe for r/w for this UAV
		const EResourceTransitionAccess CurrentUAVAccess = UAV->Resource->GetCurrentGPUAccess();
		const bool UAVDirty = UAV->Resource->IsDirty();
		ensureMsgf((GEnableDX11TransitionChecks == 0) || !UAVDirty || (CurrentUAVAccess == EResourceTransitionAccess::ERWNoBarrier), TEXT("UAV: %i is in unsafe state for GPU R/W: %s, Dirty: %i"), UAVIndex, *FResourceTransitionUtility::ResourceTransitionAccessStrings[(int32)CurrentUAVAccess], (int32)UAVDirty);

		//UAVs always dirty themselves. If a shader wanted to just read, it should use an SRV.
		UAV->Resource->SetDirty(true, PresentCounter);
	}

	ID3D11UnorderedAccessView* D3D11UAV = UAV ? UAV->View : NULL;

	uint32 InitialCount = -1;
	Direct3DDeviceIMContext->CSSetUnorderedAccessViews(UAVIndex,1,&D3D11UAV, &InitialCount );
}

void FD3D11DynamicRHI::RHISetUAVParameter(FComputeShaderRHIParamRef ComputeShaderRHI,uint32 UAVIndex,FUnorderedAccessViewRHIParamRef UAVRHI, uint32 InitialCount )
{
	//VALIDATE_BOUND_SHADER(ComputeShaderRHI);

	FD3D11UnorderedAccessView* UAV = ResourceCast(UAVRHI);
	
	if(UAV)
	{
		ConditionalClearShaderResource(UAV->Resource);

		//check it's safe for r/w for this UAV
		const EResourceTransitionAccess CurrentUAVAccess = UAV->Resource->GetCurrentGPUAccess();
		const bool UAVDirty = UAV->Resource->IsDirty();
		ensureMsgf((GEnableDX11TransitionChecks == 0) || !UAVDirty || (CurrentUAVAccess == EResourceTransitionAccess::ERWNoBarrier), TEXT("UAV: %i is in unsafe state for GPU R/W: %s, Dirty: %i"), UAVIndex, *FResourceTransitionUtility::ResourceTransitionAccessStrings[(int32)CurrentUAVAccess], (int32)UAVDirty);

		//UAVs always dirty themselves. If a shader wanted to just read, it should use an SRV.
		UAV->Resource->SetDirty(true, PresentCounter);
	}

	ID3D11UnorderedAccessView* D3D11UAV = UAV ? UAV->View : NULL;
	Direct3DDeviceIMContext->CSSetUnorderedAccessViews(UAVIndex,1,&D3D11UAV, &InitialCount );
}

void FD3D11DynamicRHI::RHISetShaderResourceViewParameter(FPixelShaderRHIParamRef PixelShaderRHI,uint32 TextureIndex,FShaderResourceViewRHIParamRef SRVRHI)
{
	VALIDATE_BOUND_SHADER(PixelShaderRHI);

	FD3D11ShaderResourceView* SRV = ResourceCast(SRVRHI);

	FD3D11BaseShaderResource* Resource = nullptr;
	ID3D11ShaderResourceView* D3D11SRV = nullptr;

	if (SRV)
	{
		Resource = SRV->Resource;
		D3D11SRV = SRV->View;
	}

	SetShaderResourceView<SF_Pixel>(Resource, D3D11SRV, TextureIndex, NAME_None);
}

void FD3D11DynamicRHI::RHISetShaderResourceViewParameter(FVertexShaderRHIParamRef VertexShaderRHI,uint32 TextureIndex,FShaderResourceViewRHIParamRef SRVRHI)
{
	VALIDATE_BOUND_SHADER(VertexShaderRHI);

	FD3D11ShaderResourceView* SRV = ResourceCast(SRVRHI);

	FD3D11BaseShaderResource* Resource = nullptr;
	ID3D11ShaderResourceView* D3D11SRV = nullptr;
	
	if (SRV)
	{
		Resource = SRV->Resource;
		D3D11SRV = SRV->View;
	}

	SetShaderResourceView<SF_Vertex>(Resource, D3D11SRV, TextureIndex, NAME_None);
}

void FD3D11DynamicRHI::RHISetShaderResourceViewParameter(FComputeShaderRHIParamRef ComputeShaderRHI,uint32 TextureIndex,FShaderResourceViewRHIParamRef SRVRHI)
{
	//VALIDATE_BOUND_SHADER(ComputeShaderRHI);

	FD3D11ShaderResourceView* SRV = ResourceCast(SRVRHI);

	FD3D11BaseShaderResource* Resource = nullptr;
	ID3D11ShaderResourceView* D3D11SRV = nullptr;
	
	if (SRV)
	{
		Resource = SRV->Resource;
		D3D11SRV = SRV->View;
	}

	SetShaderResourceView<SF_Compute>(Resource, D3D11SRV, TextureIndex, NAME_None);
}

void FD3D11DynamicRHI::RHISetShaderResourceViewParameter(FHullShaderRHIParamRef HullShaderRHI,uint32 TextureIndex,FShaderResourceViewRHIParamRef SRVRHI)
{
	VALIDATE_BOUND_SHADER(HullShaderRHI);

	FD3D11ShaderResourceView* SRV = ResourceCast(SRVRHI);

	FD3D11BaseShaderResource* Resource = nullptr;
	ID3D11ShaderResourceView* D3D11SRV = nullptr;
	
	if (SRV)
	{
		Resource = SRV->Resource;
		D3D11SRV = SRV->View;
	}

	SetShaderResourceView<SF_Hull>(Resource, D3D11SRV, TextureIndex, NAME_None);
}

void FD3D11DynamicRHI::RHISetShaderResourceViewParameter(FDomainShaderRHIParamRef DomainShaderRHI,uint32 TextureIndex,FShaderResourceViewRHIParamRef SRVRHI)
{
	VALIDATE_BOUND_SHADER(DomainShaderRHI);

	FD3D11ShaderResourceView* SRV = ResourceCast(SRVRHI);

	FD3D11BaseShaderResource* Resource = nullptr;
	ID3D11ShaderResourceView* D3D11SRV = nullptr;
	
	if (SRV)
	{
		Resource = SRV->Resource;
		D3D11SRV = SRV->View;
	}

	SetShaderResourceView<SF_Domain>(Resource, D3D11SRV, TextureIndex, NAME_None);
}

void FD3D11DynamicRHI::RHISetShaderResourceViewParameter(FGeometryShaderRHIParamRef GeometryShaderRHI,uint32 TextureIndex,FShaderResourceViewRHIParamRef SRVRHI)
{
	VALIDATE_BOUND_SHADER(GeometryShaderRHI);

	FD3D11ShaderResourceView* SRV = ResourceCast(SRVRHI);

	FD3D11BaseShaderResource* Resource = nullptr;
	ID3D11ShaderResourceView* D3D11SRV = nullptr;
	
	if (SRV)
	{
		Resource = SRV->Resource;
		D3D11SRV = SRV->View;
	}

	SetShaderResourceView<SF_Geometry>(Resource, D3D11SRV, TextureIndex, NAME_None);
}

void FD3D11DynamicRHI::RHISetShaderSampler(FVertexShaderRHIParamRef VertexShaderRHI,uint32 SamplerIndex,FSamplerStateRHIParamRef NewStateRHI)
{
	VALIDATE_BOUND_SHADER(VertexShaderRHI);

	FD3D11VertexShader* VertexShader = ResourceCast(VertexShaderRHI);
	FD3D11SamplerState* NewState = ResourceCast(NewStateRHI);

	ID3D11SamplerState* StateResource = NewState->Resource;
	StateCache.SetSamplerState<SF_Vertex>(StateResource, SamplerIndex);
}

void FD3D11DynamicRHI::RHISetShaderSampler(FHullShaderRHIParamRef HullShaderRHI,uint32 SamplerIndex,FSamplerStateRHIParamRef NewStateRHI)
{
	VALIDATE_BOUND_SHADER(HullShaderRHI);

	FD3D11HullShader* HullShader = ResourceCast(HullShaderRHI);
	FD3D11SamplerState* NewState = ResourceCast(NewStateRHI);

	ID3D11SamplerState* StateResource = NewState->Resource;
	StateCache.SetSamplerState<SF_Hull>(StateResource, SamplerIndex);
}

void FD3D11DynamicRHI::RHISetShaderSampler(FDomainShaderRHIParamRef DomainShaderRHI,uint32 SamplerIndex,FSamplerStateRHIParamRef NewStateRHI)
{
	VALIDATE_BOUND_SHADER(DomainShaderRHI);

	FD3D11DomainShader* DomainShader = ResourceCast(DomainShaderRHI);
	FD3D11SamplerState* NewState = ResourceCast(NewStateRHI);

	ID3D11SamplerState* StateResource = NewState->Resource;
	StateCache.SetSamplerState<SF_Domain>(StateResource, SamplerIndex);
}

void FD3D11DynamicRHI::RHISetShaderSampler(FGeometryShaderRHIParamRef GeometryShaderRHI,uint32 SamplerIndex,FSamplerStateRHIParamRef NewStateRHI)
{
	VALIDATE_BOUND_SHADER(GeometryShaderRHI);

	FD3D11GeometryShader* GeometryShader = ResourceCast(GeometryShaderRHI);
	FD3D11SamplerState* NewState = ResourceCast(NewStateRHI);

	ID3D11SamplerState* StateResource = NewState->Resource;
	StateCache.SetSamplerState<SF_Geometry>(StateResource, SamplerIndex);
}

void FD3D11DynamicRHI::RHISetShaderSampler(FPixelShaderRHIParamRef PixelShaderRHI,uint32 SamplerIndex,FSamplerStateRHIParamRef NewStateRHI)
{
	VALIDATE_BOUND_SHADER(PixelShaderRHI);

	FD3D11PixelShader* PixelShader = ResourceCast(PixelShaderRHI);
	FD3D11SamplerState* NewState = ResourceCast(NewStateRHI);

	ID3D11SamplerState* StateResource = NewState->Resource;
	StateCache.SetSamplerState<SF_Pixel>(StateResource, SamplerIndex);
}

void FD3D11DynamicRHI::RHISetShaderSampler(FComputeShaderRHIParamRef ComputeShaderRHI,uint32 SamplerIndex,FSamplerStateRHIParamRef NewStateRHI)
{
	//VALIDATE_BOUND_SHADER(ComputeShaderRHI);
	FD3D11ComputeShader* ComputeShader = ResourceCast(ComputeShaderRHI);
	FD3D11SamplerState* NewState = ResourceCast(NewStateRHI);

	ID3D11SamplerState* StateResource = NewState->Resource;
	StateCache.SetSamplerState<SF_Compute>(StateResource, SamplerIndex);
}

void FD3D11DynamicRHI::RHISetShaderUniformBuffer(FVertexShaderRHIParamRef VertexShader,uint32 BufferIndex,FUniformBufferRHIParamRef BufferRHI)
{
	VALIDATE_BOUND_SHADER(VertexShader);
	FD3D11UniformBuffer* Buffer = ResourceCast(BufferRHI);
	{
		ID3D11Buffer* ConstantBuffer = Buffer ? Buffer->Resource : NULL;
		StateCache.SetConstantBuffer<SF_Vertex>(ConstantBuffer, BufferIndex);
	}

	BoundUniformBuffers[SF_Vertex][BufferIndex] = BufferRHI;
	DirtyUniformBuffers[SF_Vertex] |= (1 << BufferIndex);
}

void FD3D11DynamicRHI::RHISetShaderUniformBuffer(FHullShaderRHIParamRef HullShader,uint32 BufferIndex,FUniformBufferRHIParamRef BufferRHI)
{
	VALIDATE_BOUND_SHADER(HullShader);
	FD3D11UniformBuffer* Buffer = ResourceCast(BufferRHI);
	{
		ID3D11Buffer* ConstantBuffer = Buffer ? Buffer->Resource : NULL;
		StateCache.SetConstantBuffer<SF_Hull>(ConstantBuffer, BufferIndex);
	}

	BoundUniformBuffers[SF_Hull][BufferIndex] = BufferRHI;
	DirtyUniformBuffers[SF_Hull] |= (1 << BufferIndex);
}

void FD3D11DynamicRHI::RHISetShaderUniformBuffer(FDomainShaderRHIParamRef DomainShader,uint32 BufferIndex,FUniformBufferRHIParamRef BufferRHI)
{
	VALIDATE_BOUND_SHADER(DomainShader);
	FD3D11UniformBuffer* Buffer = ResourceCast(BufferRHI);
	{
		ID3D11Buffer* ConstantBuffer = Buffer ? Buffer->Resource : NULL;
		StateCache.SetConstantBuffer<SF_Domain>(ConstantBuffer, BufferIndex);
	}

	BoundUniformBuffers[SF_Domain][BufferIndex] = BufferRHI;
	DirtyUniformBuffers[SF_Domain] |= (1 << BufferIndex);
}

void FD3D11DynamicRHI::RHISetShaderUniformBuffer(FGeometryShaderRHIParamRef GeometryShader,uint32 BufferIndex,FUniformBufferRHIParamRef BufferRHI)
{
	VALIDATE_BOUND_SHADER(GeometryShader);
	FD3D11UniformBuffer* Buffer = ResourceCast(BufferRHI);
	{
		ID3D11Buffer* ConstantBuffer = Buffer ? Buffer->Resource : NULL;
		StateCache.SetConstantBuffer<SF_Geometry>(ConstantBuffer, BufferIndex);
	}

	BoundUniformBuffers[SF_Geometry][BufferIndex] = BufferRHI;
	DirtyUniformBuffers[SF_Geometry] |= (1 << BufferIndex);
}

void FD3D11DynamicRHI::RHISetShaderUniformBuffer(FPixelShaderRHIParamRef PixelShader,uint32 BufferIndex,FUniformBufferRHIParamRef BufferRHI)
{
	VALIDATE_BOUND_SHADER(PixelShader);
	FD3D11UniformBuffer* Buffer = ResourceCast(BufferRHI);

	{
		ID3D11Buffer* ConstantBuffer = Buffer ? Buffer->Resource : NULL;
		StateCache.SetConstantBuffer<SF_Pixel>(ConstantBuffer, BufferIndex);
	}

	BoundUniformBuffers[SF_Pixel][BufferIndex] = BufferRHI;
	DirtyUniformBuffers[SF_Pixel] |= (1 << BufferIndex);
}

void FD3D11DynamicRHI::RHISetShaderUniformBuffer(FComputeShaderRHIParamRef ComputeShader,uint32 BufferIndex,FUniformBufferRHIParamRef BufferRHI)
{
	//VALIDATE_BOUND_SHADER(ComputeShader);
	FD3D11UniformBuffer* Buffer = ResourceCast(BufferRHI);
	{
		ID3D11Buffer* ConstantBuffer = Buffer ? Buffer->Resource : NULL;
		StateCache.SetConstantBuffer<SF_Compute>(ConstantBuffer, BufferIndex);
	}

	BoundUniformBuffers[SF_Compute][BufferIndex] = BufferRHI;
	DirtyUniformBuffers[SF_Compute] |= (1 << BufferIndex);
}

void FD3D11DynamicRHI::RHISetShaderParameter(FHullShaderRHIParamRef HullShaderRHI,uint32 BufferIndex,uint32 BaseIndex,uint32 NumBytes,const void* NewValue)
{
	VALIDATE_BOUND_SHADER(HullShaderRHI);
	checkSlow(HSConstantBuffers[BufferIndex]);
	HSConstantBuffers[BufferIndex]->UpdateConstant((const uint8*)NewValue,BaseIndex,NumBytes);
}

void FD3D11DynamicRHI::RHISetShaderParameter(FDomainShaderRHIParamRef DomainShaderRHI,uint32 BufferIndex,uint32 BaseIndex,uint32 NumBytes,const void* NewValue)
{
	VALIDATE_BOUND_SHADER(DomainShaderRHI);
	checkSlow(DSConstantBuffers[BufferIndex]);
	DSConstantBuffers[BufferIndex]->UpdateConstant((const uint8*)NewValue,BaseIndex,NumBytes);
}

void FD3D11DynamicRHI::RHISetShaderParameter(FVertexShaderRHIParamRef VertexShaderRHI,uint32 BufferIndex,uint32 BaseIndex,uint32 NumBytes,const void* NewValue)
{
	VALIDATE_BOUND_SHADER(VertexShaderRHI);
	checkSlow(VSConstantBuffers[BufferIndex]);
	VSConstantBuffers[BufferIndex]->UpdateConstant((const uint8*)NewValue,BaseIndex,NumBytes);
}

void FD3D11DynamicRHI::RHISetShaderParameter(FPixelShaderRHIParamRef PixelShaderRHI,uint32 BufferIndex,uint32 BaseIndex,uint32 NumBytes,const void* NewValue)
{
	VALIDATE_BOUND_SHADER(PixelShaderRHI);
	checkSlow(PSConstantBuffers[BufferIndex]);
	PSConstantBuffers[BufferIndex]->UpdateConstant((const uint8*)NewValue,BaseIndex,NumBytes);
}

void FD3D11DynamicRHI::RHISetShaderParameter(FGeometryShaderRHIParamRef GeometryShaderRHI,uint32 BufferIndex,uint32 BaseIndex,uint32 NumBytes,const void* NewValue)
{
	VALIDATE_BOUND_SHADER(GeometryShaderRHI);
	checkSlow(GSConstantBuffers[BufferIndex]);
	GSConstantBuffers[BufferIndex]->UpdateConstant((const uint8*)NewValue,BaseIndex,NumBytes);
}

void FD3D11DynamicRHI::RHISetShaderParameter(FComputeShaderRHIParamRef ComputeShaderRHI,uint32 BufferIndex,uint32 BaseIndex,uint32 NumBytes,const void* NewValue)
{
	//VALIDATE_BOUND_SHADER(ComputeShaderRHI);
	checkSlow(CSConstantBuffers[BufferIndex]);
	CSConstantBuffers[BufferIndex]->UpdateConstant((const uint8*)NewValue,BaseIndex,NumBytes);
}

void FD3D11DynamicRHI::ValidateExclusiveDepthStencilAccess(FExclusiveDepthStencil RequestedAccess) const
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
		ensureMsgf(
			!bSrcDepthWrite || bDstDepthWrite, 
			TEXT("Expected: SrcDepthWrite := false or DstDepthWrite := true. Actual: SrcDepthWrite := %s or DstDepthWrite := %s"),
			(bSrcDepthWrite) ? TEXT("true") : TEXT("false"),
			(bDstDepthWrite) ? TEXT("true") : TEXT("false")
			);

		ensureMsgf(
			!bSrcStencilWrite || bDstStencilWrite,
			TEXT("Expected: SrcStencilWrite := false or DstStencilWrite := true. Actual: SrcStencilWrite := %s or DstStencilWrite := %s"),
			(bSrcStencilWrite) ? TEXT("true") : TEXT("false"),
			(bDstStencilWrite) ? TEXT("true") : TEXT("false")
			);
	}
}

void FD3D11DynamicRHI::RHISetDepthStencilState(FDepthStencilStateRHIParamRef NewStateRHI,uint32 StencilRef)
{
	FD3D11DepthStencilState* NewState = ResourceCast(NewStateRHI);

	ValidateExclusiveDepthStencilAccess(NewState->AccessType);

	StateCache.SetDepthStencilState(NewState->Resource, StencilRef);
}

void FD3D11DynamicRHI::RHISetStencilRef(uint32 StencilRef)
{
	StateCache.SetStencilRef(StencilRef);
}

void FD3D11DynamicRHI::RHISetBlendState(FBlendStateRHIParamRef NewStateRHI,const FLinearColor& BlendFactor)
{
	FD3D11BlendState* NewState = ResourceCast(NewStateRHI);
	StateCache.SetBlendState(NewState->Resource, (const float*)&BlendFactor, 0xffffffff);
}

void FD3D11DynamicRHI::RHISetBlendFactor(const FLinearColor& BlendFactor)
{
	StateCache.SetBlendFactor((const float*)&BlendFactor, 0xffffffff);
}

// WaveWorks Start
void FD3D11DynamicRHI::RHISetWaveWorksState(FWaveWorksRHIParamRef State, const FMatrix& ViewMatrix, const TArray<uint32>& ShaderInputMappings)
{
	StateCache.SetWaveWorksState(State, ViewMatrix, ShaderInputMappings);
}
// WaveWorks End

void FD3D11DynamicRHI::CommitRenderTargetsAndUAVs()
{
	ID3D11RenderTargetView* RTArray[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
	for(uint32 RenderTargetIndex = 0;RenderTargetIndex < NumSimultaneousRenderTargets;++RenderTargetIndex)
	{
		RTArray[RenderTargetIndex] = CurrentRenderTargets[RenderTargetIndex];
	}
	ID3D11UnorderedAccessView* UAVArray[D3D11_PS_CS_UAV_REGISTER_COUNT];
	uint32 UAVInitialCountArray[D3D11_PS_CS_UAV_REGISTER_COUNT];
	for(uint32 UAVIndex = 0;UAVIndex < NumUAVs;++UAVIndex)
	{
		UAVArray[UAVIndex] = CurrentUAVs[UAVIndex];
		// Using the value that indicates to keep the current UAV counter
		UAVInitialCountArray[UAVIndex] = -1;
	}

	if (NumUAVs > 0)
	{
		Direct3DDeviceIMContext->OMSetRenderTargetsAndUnorderedAccessViews(
			NumSimultaneousRenderTargets,
			RTArray,
			CurrentDepthStencilTarget,
			NumSimultaneousRenderTargets,
			NumUAVs,
			UAVArray,
			UAVInitialCountArray
			);
	}
	else
	{
		// Use OMSetRenderTargets if there are no UAVs, works around a crash in PIX
		Direct3DDeviceIMContext->OMSetRenderTargets(
			NumSimultaneousRenderTargets,
			RTArray,
			CurrentDepthStencilTarget
			);
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
FRTVDesc GetRenderTargetViewDesc(ID3D11RenderTargetView* RenderTargetView)
{
	D3D11_RENDER_TARGET_VIEW_DESC TargetDesc;
	RenderTargetView->GetDesc(&TargetDesc);

	TRefCountPtr<ID3D11Resource> BaseResource;
	RenderTargetView->GetResource((ID3D11Resource**)BaseResource.GetInitReference());
	uint32 MipIndex = 0;
	FRTVDesc ret;
	memset(&ret, 0, sizeof(ret));

	switch (TargetDesc.ViewDimension)
	{
		case D3D11_RTV_DIMENSION_TEXTURE2D:
		case D3D11_RTV_DIMENSION_TEXTURE2DMS:
		case D3D11_RTV_DIMENSION_TEXTURE2DARRAY:
		case D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY:
		{
			D3D11_TEXTURE2D_DESC Desc;
			((ID3D11Texture2D*)(BaseResource.GetReference()))->GetDesc(&Desc);
			ret.Width = Desc.Width;
			ret.Height = Desc.Height;
			ret.SampleDesc = Desc.SampleDesc;
			if (TargetDesc.ViewDimension == D3D11_RTV_DIMENSION_TEXTURE2D || TargetDesc.ViewDimension == D3D11_RTV_DIMENSION_TEXTURE2DARRAY)
			{
				// All the non-multisampled texture types have their mip-slice in the same position.
				MipIndex = TargetDesc.Texture2D.MipSlice;
			}
			break;
		}
		case D3D11_RTV_DIMENSION_TEXTURE3D:
		{
			D3D11_TEXTURE3D_DESC Desc;
			((ID3D11Texture3D*)(BaseResource.GetReference()))->GetDesc(&Desc);
			ret.Width = Desc.Width;
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

void FD3D11DynamicRHI::RHISetRenderTargets(
	uint32 NewNumSimultaneousRenderTargets,
	const FRHIRenderTargetView* NewRenderTargetsRHI,
	const FRHIDepthRenderTargetView* NewDepthStencilTargetRHI,
	uint32 NewNumUAVs,
	const FUnorderedAccessViewRHIParamRef* UAVs
	)
{
	FD3D11TextureBase* NewDepthStencilTarget = GetD3D11TextureFromRHITexture(NewDepthStencilTargetRHI ? NewDepthStencilTargetRHI->Texture : nullptr);

#if CHECK_SRV_TRANSITIONS
	// if the depth buffer is writable then it counts as unresolved.
	if (NewDepthStencilTargetRHI && NewDepthStencilTargetRHI->GetDepthStencilAccess() == FExclusiveDepthStencil::DepthWrite_StencilWrite && NewDepthStencilTarget)
	{		
		check(UnresolvedTargetsConcurrencyGuard.Increment() == 1);
		UnresolvedTargets.Add(NewDepthStencilTarget->GetResource(), FUnresolvedRTInfo(NewDepthStencilTargetRHI->Texture->GetName(), 0, 1, -1, 1));
		check(UnresolvedTargetsConcurrencyGuard.Decrement() == 0);
	}
#endif

	check(NewNumSimultaneousRenderTargets + NewNumUAVs <= MaxSimultaneousRenderTargets);

	bool bTargetChanged = false;

	// Set the appropriate depth stencil view depending on whether depth writes are enabled or not
	ID3D11DepthStencilView* DepthStencilView = NULL;
	if(NewDepthStencilTarget)
	{
		CurrentDSVAccessType = NewDepthStencilTargetRHI->GetDepthStencilAccess();
		DepthStencilView = NewDepthStencilTarget->GetDepthStencilView(CurrentDSVAccessType);

		// Unbind any shader views of the depth stencil target that are bound.
		ConditionalClearShaderResource(NewDepthStencilTarget);
	}

	// Check if the depth stencil target is different from the old state.
	if(CurrentDepthStencilTarget != DepthStencilView)
	{
		CurrentDepthTexture = NewDepthStencilTarget;
		CurrentDepthStencilTarget = DepthStencilView;
		bTargetChanged = true;
	}

	if (NewDepthStencilTarget)
	{
		uint32 CurrentFrame = PresentCounter;
		const EResourceTransitionAccess CurrentAccess = NewDepthStencilTarget->GetCurrentGPUAccess();
		const uint32 LastFrameWritten = NewDepthStencilTarget->GetLastFrameWritten();
		const bool bReadable = CurrentAccess == EResourceTransitionAccess::EReadable;
		const bool bDepthWrite = NewDepthStencilTargetRHI->GetDepthStencilAccess().IsDepthWrite();
		const bool bAccessValid =	!bReadable ||
									LastFrameWritten != CurrentFrame || 
									!bDepthWrite;

		ensureMsgf((GEnableDX11TransitionChecks == 0) || bAccessValid, TEXT("DepthTarget '%s' is not GPU writable."), *NewDepthStencilTargetRHI->Texture->GetName().ToString());

		//switch to writable state if this is the first render of the frame.  Don't switch if it's a later render and this is a depth test only situation
		if (!bAccessValid || (bReadable && bDepthWrite))
		{
			DUMP_TRANSITION(NewDepthStencilTargetRHI->Texture->GetName(), EResourceTransitionAccess::EWritable);
			NewDepthStencilTarget->SetCurrentGPUAccess(EResourceTransitionAccess::EWritable);
		}

		if (bDepthWrite)
		{
			NewDepthStencilTarget->SetDirty(true, CurrentFrame);
		}
	}

	// Gather the render target views for the new render targets.
	ID3D11RenderTargetView* NewRenderTargetViews[MaxSimultaneousRenderTargets];
	for(uint32 RenderTargetIndex = 0;RenderTargetIndex < MaxSimultaneousRenderTargets;++RenderTargetIndex)
	{
		ID3D11RenderTargetView* RenderTargetView = NULL;
		if(RenderTargetIndex < NewNumSimultaneousRenderTargets && NewRenderTargetsRHI[RenderTargetIndex].Texture != nullptr)
		{
			int32 RTMipIndex = NewRenderTargetsRHI[RenderTargetIndex].MipIndex;
			int32 RTSliceIndex = NewRenderTargetsRHI[RenderTargetIndex].ArraySliceIndex;
			FD3D11TextureBase* NewRenderTarget = GetD3D11TextureFromRHITexture(NewRenderTargetsRHI[RenderTargetIndex].Texture);

			if (NewRenderTarget)
			{
				RenderTargetView = NewRenderTarget->GetRenderTargetView(RTMipIndex, RTSliceIndex);
				uint32 CurrentFrame = PresentCounter;
				const EResourceTransitionAccess CurrentAccess = NewRenderTarget->GetCurrentGPUAccess();
				const uint32 LastFrameWritten = NewRenderTarget->GetLastFrameWritten();
				const bool bReadable = CurrentAccess == EResourceTransitionAccess::EReadable;
				const bool bAccessValid = !bReadable || LastFrameWritten != CurrentFrame;
				ensureMsgf((GEnableDX11TransitionChecks == 0) || bAccessValid, TEXT("RenderTarget '%s' is not GPU writable."), *NewRenderTargetsRHI[RenderTargetIndex].Texture->GetName().ToString());
								
				if (!bAccessValid || bReadable)
				{
					DUMP_TRANSITION(NewRenderTargetsRHI[RenderTargetIndex].Texture->GetName(), EResourceTransitionAccess::EWritable);
					NewRenderTarget->SetCurrentGPUAccess(EResourceTransitionAccess::EWritable);
				}
				NewRenderTarget->SetDirty(true, CurrentFrame);
			}

			ensureMsgf(RenderTargetView, TEXT("Texture being set as render target has no RTV"));
#if CHECK_SRV_TRANSITIONS			
			if (RenderTargetView)
			{
				// remember this target as having been bound for write.
				ID3D11Resource* RTVResource;
				RenderTargetView->GetResource(&RTVResource);
				check(UnresolvedTargetsConcurrencyGuard.Increment() == 1);
				UnresolvedTargets.Add(RTVResource, FUnresolvedRTInfo(NewRenderTargetsRHI[RenderTargetIndex].Texture->GetName(), RTMipIndex, 1, RTSliceIndex, 1));
				check(UnresolvedTargetsConcurrencyGuard.Decrement() == 0);
				RTVResource->Release();
			}
#endif
			
			// Unbind any shader views of the render target that are bound.
			ConditionalClearShaderResource(NewRenderTarget);

#if UE_BUILD_DEBUG	
			// A check to allow you to pinpoint what is using mismatching targets
			// We filter our d3ddebug spew that checks for this as the d3d runtime's check is wrong.
			// For filter code, see D3D11Device.cpp look for "OMSETRENDERTARGETS_INVALIDVIEW"
			if(RenderTargetView && DepthStencilView)
			{
				FRTVDesc RTTDesc = GetRenderTargetViewDesc(RenderTargetView);

				TRefCountPtr<ID3D11Texture2D> DepthTargetTexture;
				DepthStencilView->GetResource((ID3D11Resource**)DepthTargetTexture.GetInitReference());

				D3D11_TEXTURE2D_DESC DTTDesc;
				DepthTargetTexture->GetDesc(&DTTDesc);

				// enforce color target is <= depth and MSAA settings match
				if(RTTDesc.Width > DTTDesc.Width || RTTDesc.Height > DTTDesc.Height || 
					RTTDesc.SampleDesc.Count != DTTDesc.SampleDesc.Count || 
					RTTDesc.SampleDesc.Quality != DTTDesc.SampleDesc.Quality)
				{
					UE_LOG(LogD3D11RHI, Fatal,TEXT("RTV(%i,%i c=%i,q=%i) and DSV(%i,%i c=%i,q=%i) have mismatching dimensions and/or MSAA levels!"),
						RTTDesc.Width,RTTDesc.Height,RTTDesc.SampleDesc.Count,RTTDesc.SampleDesc.Quality,
						DTTDesc.Width,DTTDesc.Height,DTTDesc.SampleDesc.Count,DTTDesc.SampleDesc.Quality);
				}
			}
#endif
		}

		NewRenderTargetViews[RenderTargetIndex] = RenderTargetView;

		// Check if the render target is different from the old state.
		if(CurrentRenderTargets[RenderTargetIndex] != RenderTargetView)
		{
			CurrentRenderTargets[RenderTargetIndex] = RenderTargetView;
			bTargetChanged = true;
		}
	}
	if(NumSimultaneousRenderTargets != NewNumSimultaneousRenderTargets)
	{
		NumSimultaneousRenderTargets = NewNumSimultaneousRenderTargets;
		bTargetChanged = true;
	}

	// Gather the new UAVs.
	for(uint32 UAVIndex = 0;UAVIndex < MaxSimultaneousUAVs;++UAVIndex)
	{
		ID3D11UnorderedAccessView* UAV = NULL;
		if(UAVIndex < NewNumUAVs && UAVs[UAVIndex] != NULL)
		{
			FD3D11UnorderedAccessView* RHIUAV = (FD3D11UnorderedAccessView*)UAVs[UAVIndex];
			UAV = RHIUAV->View;

			if (UAV)
			{
				//check it's safe for r/w for this UAV
				const EResourceTransitionAccess CurrentUAVAccess = RHIUAV->Resource->GetCurrentGPUAccess();
				const bool UAVDirty = RHIUAV->Resource->IsDirty();
				const bool bAccessPass = (CurrentUAVAccess == EResourceTransitionAccess::ERWBarrier && !UAVDirty) || (CurrentUAVAccess == EResourceTransitionAccess::ERWNoBarrier);
				ensureMsgf((GEnableDX11TransitionChecks == 0) || bAccessPass, TEXT("UAV: %i is in unsafe state for GPU R/W: %s"), UAVIndex, *FResourceTransitionUtility::ResourceTransitionAccessStrings[(int32)CurrentUAVAccess]);

				//UAVs get set to dirty.  If the shader just wanted to read it should have used an SRV.
				RHIUAV->Resource->SetDirty(true, PresentCounter);
			}

			// Unbind any shader views of the UAV's resource.
			ConditionalClearShaderResource(RHIUAV->Resource);
		}

		if(CurrentUAVs[UAVIndex] != UAV)
		{
			CurrentUAVs[UAVIndex] = UAV;
			bTargetChanged = true;
		}
	}
	if(NumUAVs != NewNumUAVs)
	{
		NumUAVs = NewNumUAVs;
		bTargetChanged = true;
	}

	// NVCHANGE_BEGIN: Add VXGI
	// Only make the D3D call to change render targets if something actually changed.
	if(bTargetChanged)
	{
		CommitRenderTargetsAndUAVs();

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
			TRefCountPtr<ID3D11Texture2D> DepthTargetTexture;
			DepthStencilView->GetResource((ID3D11Resource**)DepthTargetTexture.GetInitReference());

			D3D11_TEXTURE2D_DESC DTTDesc;
			DepthTargetTexture->GetDesc(&DTTDesc);
			RHISetViewport(0, 0, 0.0f, DTTDesc.Width, DTTDesc.Height, 1.0f);
		}
	}
	// NVCHANGE_END: Add VXGI
}

void FD3D11DynamicRHI::RHIDiscardRenderTargets(bool Depth, bool Stencil, uint32 ColorBitMask)
{
	// Could support in DX11.1 via ID3D11DeviceContext1::Discard*() functions.
}

void FD3D11DynamicRHI::RHISetRenderTargetsAndClear(const FRHISetRenderTargetsInfo& RenderTargetsInfo)
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

// Primitive drawing.

static D3D11_PRIMITIVE_TOPOLOGY GetD3D11PrimitiveType(uint32 PrimitiveType, bool bUsingTessellation)
{
	if(bUsingTessellation)
	{
		switch(PrimitiveType)
		{
		case PT_1_ControlPointPatchList: return D3D11_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST;
		case PT_2_ControlPointPatchList: return D3D11_PRIMITIVE_TOPOLOGY_2_CONTROL_POINT_PATCHLIST;

		// This is the case for tessellation without AEN or other buffers, so just flip to 3 CPs
		case PT_TriangleList: return D3D11_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST;

		case PT_LineList:
		case PT_TriangleStrip:
		case PT_QuadList:
		case PT_PointList:
			UE_LOG(LogD3D11RHI, Fatal,TEXT("Invalid type specified for tessellated render, probably missing a case in FStaticMeshSceneProxy::GetMeshElement"));
			break;
		default:
			// Other cases are valid.
			break;
		};
	}

	switch(PrimitiveType)
	{
	case PT_TriangleList: return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	case PT_TriangleStrip: return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
	case PT_LineList: return D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
	case PT_PointList: return D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;

	// ControlPointPatchList types will pretend to be TRIANGLELISTS with a stride of N 
	// (where N is the number of control points specified), so we can return them for
	// tessellation and non-tessellation. This functionality is only used when rendering a 
	// default material with something that claims to be tessellated, generally because the 
	// tessellation material failed to compile for some reason.
	case PT_3_ControlPointPatchList: return D3D11_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST;
	case PT_4_ControlPointPatchList: return D3D11_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST;
	case PT_5_ControlPointPatchList: return D3D11_PRIMITIVE_TOPOLOGY_5_CONTROL_POINT_PATCHLIST;
	case PT_6_ControlPointPatchList: return D3D11_PRIMITIVE_TOPOLOGY_6_CONTROL_POINT_PATCHLIST;
	case PT_7_ControlPointPatchList: return D3D11_PRIMITIVE_TOPOLOGY_7_CONTROL_POINT_PATCHLIST;
	case PT_8_ControlPointPatchList: return D3D11_PRIMITIVE_TOPOLOGY_8_CONTROL_POINT_PATCHLIST; 
	case PT_9_ControlPointPatchList: return D3D11_PRIMITIVE_TOPOLOGY_9_CONTROL_POINT_PATCHLIST; 
	case PT_10_ControlPointPatchList: return D3D11_PRIMITIVE_TOPOLOGY_10_CONTROL_POINT_PATCHLIST; 
	case PT_11_ControlPointPatchList: return D3D11_PRIMITIVE_TOPOLOGY_11_CONTROL_POINT_PATCHLIST; 
	case PT_12_ControlPointPatchList: return D3D11_PRIMITIVE_TOPOLOGY_12_CONTROL_POINT_PATCHLIST; 
	case PT_13_ControlPointPatchList: return D3D11_PRIMITIVE_TOPOLOGY_13_CONTROL_POINT_PATCHLIST; 
	case PT_14_ControlPointPatchList: return D3D11_PRIMITIVE_TOPOLOGY_14_CONTROL_POINT_PATCHLIST; 
	case PT_15_ControlPointPatchList: return D3D11_PRIMITIVE_TOPOLOGY_15_CONTROL_POINT_PATCHLIST; 
	case PT_16_ControlPointPatchList: return D3D11_PRIMITIVE_TOPOLOGY_16_CONTROL_POINT_PATCHLIST; 
	case PT_17_ControlPointPatchList: return D3D11_PRIMITIVE_TOPOLOGY_17_CONTROL_POINT_PATCHLIST; 
	case PT_18_ControlPointPatchList: return D3D11_PRIMITIVE_TOPOLOGY_18_CONTROL_POINT_PATCHLIST; 
	case PT_19_ControlPointPatchList: return D3D11_PRIMITIVE_TOPOLOGY_19_CONTROL_POINT_PATCHLIST; 
	case PT_20_ControlPointPatchList: return D3D11_PRIMITIVE_TOPOLOGY_20_CONTROL_POINT_PATCHLIST; 
	case PT_21_ControlPointPatchList: return D3D11_PRIMITIVE_TOPOLOGY_21_CONTROL_POINT_PATCHLIST; 
	case PT_22_ControlPointPatchList: return D3D11_PRIMITIVE_TOPOLOGY_22_CONTROL_POINT_PATCHLIST; 
	case PT_23_ControlPointPatchList: return D3D11_PRIMITIVE_TOPOLOGY_23_CONTROL_POINT_PATCHLIST; 
	case PT_24_ControlPointPatchList: return D3D11_PRIMITIVE_TOPOLOGY_24_CONTROL_POINT_PATCHLIST; 
	case PT_25_ControlPointPatchList: return D3D11_PRIMITIVE_TOPOLOGY_25_CONTROL_POINT_PATCHLIST; 
	case PT_26_ControlPointPatchList: return D3D11_PRIMITIVE_TOPOLOGY_26_CONTROL_POINT_PATCHLIST; 
	case PT_27_ControlPointPatchList: return D3D11_PRIMITIVE_TOPOLOGY_27_CONTROL_POINT_PATCHLIST; 
	case PT_28_ControlPointPatchList: return D3D11_PRIMITIVE_TOPOLOGY_28_CONTROL_POINT_PATCHLIST; 
	case PT_29_ControlPointPatchList: return D3D11_PRIMITIVE_TOPOLOGY_29_CONTROL_POINT_PATCHLIST; 
	case PT_30_ControlPointPatchList: return D3D11_PRIMITIVE_TOPOLOGY_30_CONTROL_POINT_PATCHLIST; 
	case PT_31_ControlPointPatchList: return D3D11_PRIMITIVE_TOPOLOGY_31_CONTROL_POINT_PATCHLIST; 
	case PT_32_ControlPointPatchList: return D3D11_PRIMITIVE_TOPOLOGY_32_CONTROL_POINT_PATCHLIST; 
	default: UE_LOG(LogD3D11RHI, Fatal,TEXT("Unknown primitive type: %u"),PrimitiveType);
	};

	return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
}

static inline void VerifyPrimitiveType(EPrimitiveType PSOPrimitiveType, uint32 PrimitiveType)
{
	ensureAlwaysMsgf(PSOPrimitiveType == PT_Num || (EPrimitiveType)PrimitiveType == PSOPrimitiveType, TEXT("PSO was created using PrimitiveType %d, but the Draw call is using %d! This will break D3D12, Metal and Vulkan"), (uint32)PSOPrimitiveType, (uint32)PrimitiveType);
}

void FD3D11DynamicRHI::CommitNonComputeShaderConstants()
{
	FD3D11BoundShaderState* CurrentBoundShaderState = (FD3D11BoundShaderState*)BoundShaderStateHistory.GetLast();
	check(CurrentBoundShaderState);

	// Only set the constant buffer if this shader needs the global constant buffer bound
	// Otherwise we will overwrite a different constant buffer
	if (CurrentBoundShaderState->bShaderNeedsGlobalConstantBuffer[SF_Vertex])
	{
		// Commit and bind vertex shader constants
		for(uint32 i=0;i<MAX_CONSTANT_BUFFER_SLOTS; i++)
		{
			FD3D11ConstantBuffer* ConstantBuffer = VSConstantBuffers[i];
			FD3DRHIUtil::CommitConstants<SF_Vertex>(ConstantBuffer, StateCache, i, bDiscardSharedConstants);
		}
	}

	// Skip HS/DS CB updates in cases where tessellation isn't being used
	// Note that this is *potentially* unsafe because bDiscardSharedConstants is cleared at the
	// end of the function, however we're OK for now because bDiscardSharedConstants
	// is always reset whenever bUsingTessellation changes in SetBoundShaderState()
	if(bUsingTessellation)
	{
		if (CurrentBoundShaderState->bShaderNeedsGlobalConstantBuffer[SF_Hull])
		{
			// Commit and bind hull shader constants
			for(uint32 i=0;i<MAX_CONSTANT_BUFFER_SLOTS; i++)
			{
				FD3D11ConstantBuffer* ConstantBuffer = HSConstantBuffers[i];
				FD3DRHIUtil::CommitConstants<SF_Hull>(ConstantBuffer, StateCache, i, bDiscardSharedConstants);
			}
		}

		if (CurrentBoundShaderState->bShaderNeedsGlobalConstantBuffer[SF_Domain])
		{
			// Commit and bind domain shader constants
			for(uint32 i=0;i<MAX_CONSTANT_BUFFER_SLOTS; i++)
			{
				FD3D11ConstantBuffer* ConstantBuffer = DSConstantBuffers[i];
				FD3DRHIUtil::CommitConstants<SF_Domain>(ConstantBuffer, StateCache, i, bDiscardSharedConstants);
			}
		}
	}

	if (CurrentBoundShaderState->bShaderNeedsGlobalConstantBuffer[SF_Geometry])
	{
		// Commit and bind geometry shader constants
		for(uint32 i=0;i<MAX_CONSTANT_BUFFER_SLOTS; i++)
		{
			FD3D11ConstantBuffer* ConstantBuffer = GSConstantBuffers[i];
			FD3DRHIUtil::CommitConstants<SF_Geometry>(ConstantBuffer, StateCache, i, bDiscardSharedConstants);
		}
	}

	if (CurrentBoundShaderState->bShaderNeedsGlobalConstantBuffer[SF_Pixel])
	{
		// Commit and bind pixel shader constants
		for(uint32 i=0;i<MAX_CONSTANT_BUFFER_SLOTS; i++)
		{
			FD3D11ConstantBuffer* ConstantBuffer = PSConstantBuffers[i];
			FD3DRHIUtil::CommitConstants<SF_Pixel>(ConstantBuffer, StateCache, i, bDiscardSharedConstants);
		}
	}

	bDiscardSharedConstants = false;
}

void FD3D11DynamicRHI::CommitComputeShaderConstants()
{
	bool bLocalDiscardSharedConstants = true;

	// Commit and bind compute shader constants
	for(uint32 i=0;i<MAX_CONSTANT_BUFFER_SLOTS; i++)
	{
		FD3D11ConstantBuffer* ConstantBuffer = CSConstantBuffers[i];
		FD3DRHIUtil::CommitConstants<SF_Compute>(ConstantBuffer, StateCache, i, bDiscardSharedConstants);
	}
}

template <EShaderFrequency Frequency>
FORCEINLINE void SetResource(FD3D11DynamicRHI* RESTRICT D3D11RHI, FD3D11StateCache* RESTRICT StateCache, uint32 BindIndex, FD3D11BaseShaderResource* RESTRICT ShaderResource, ID3D11ShaderResourceView* RESTRICT SRV, FName ResourceName = FName())
{
	// We set the resource through the RHI to track state for the purposes of unbinding SRVs when a UAV or RTV is bound.
	// todo: need to support SRV_Static for faster calls when possible
	D3D11RHI->SetShaderResourceView<Frequency>(ShaderResource, SRV, BindIndex, ResourceName,FD3D11StateCache::SRV_Unknown);
}

template <EShaderFrequency Frequency>
FORCEINLINE void SetResource(FD3D11DynamicRHI* RESTRICT D3D11RHI, FD3D11StateCache* RESTRICT StateCache, uint32 BindIndex, ID3D11SamplerState* RESTRICT SamplerState)
{
	StateCache->SetSamplerState<Frequency>(SamplerState,BindIndex);
}

template <EShaderFrequency ShaderFrequency>
inline int32 SetShaderResourcesFromBuffer_Surface(FD3D11DynamicRHI* RESTRICT D3D11RHI, FD3D11StateCache* RESTRICT StateCache, FD3D11UniformBuffer* RESTRICT Buffer, const uint32* RESTRICT ResourceMap, int32 BufferIndex)
{
	const TRefCountPtr<FRHIResource>* RESTRICT Resources = Buffer->ResourceTable.GetData();
	float CurrentTime = FApp::GetCurrentTime();
	int32 NumSetCalls = 0;
	uint32 BufferOffset = ResourceMap[BufferIndex];
	if (BufferOffset > 0)
	{
		const uint32* RESTRICT ResourceInfos = &ResourceMap[BufferOffset];
		uint32 ResourceInfo = *ResourceInfos++;
		do
		{
			checkSlow(FRHIResourceTableEntry::GetUniformBufferIndex(ResourceInfo) == BufferIndex);
			const uint16 ResourceIndex = FRHIResourceTableEntry::GetResourceIndex(ResourceInfo);
			const uint8 BindIndex = FRHIResourceTableEntry::GetBindIndex(ResourceInfo);

			FD3D11BaseShaderResource* ShaderResource = nullptr;
			ID3D11ShaderResourceView* D3D11Resource = nullptr;

			FRHITexture* TextureRHI = (FRHITexture*)Resources[ResourceIndex].GetReference();
			TextureRHI->SetLastRenderTime(CurrentTime);
			FD3D11TextureBase* TextureD3D11 = GetD3D11TextureFromRHITexture(TextureRHI);
			ShaderResource = TextureD3D11->GetBaseShaderResource();
			D3D11Resource = TextureD3D11->GetShaderResourceView();

			// todo: could coalesce adjacent bound resources.
			SetResource<ShaderFrequency>(D3D11RHI, StateCache, BindIndex, ShaderResource, D3D11Resource, TextureRHI->GetName());
			NumSetCalls++;
			ResourceInfo = *ResourceInfos++;
		} while (FRHIResourceTableEntry::GetUniformBufferIndex(ResourceInfo) == BufferIndex);
	}
	return NumSetCalls;
}


template <EShaderFrequency ShaderFrequency>
inline int32 SetShaderResourcesFromBuffer_SRV(FD3D11DynamicRHI* RESTRICT D3D11RHI, FD3D11StateCache* RESTRICT StateCache, FD3D11UniformBuffer* RESTRICT Buffer, const uint32* RESTRICT ResourceMap, int32 BufferIndex)
{
	const TRefCountPtr<FRHIResource>* RESTRICT Resources = Buffer->ResourceTable.GetData();
	float CurrentTime = FApp::GetCurrentTime();
	int32 NumSetCalls = 0;
	uint32 BufferOffset = ResourceMap[BufferIndex];
	if (BufferOffset > 0)
	{
		const uint32* RESTRICT ResourceInfos = &ResourceMap[BufferOffset];
		uint32 ResourceInfo = *ResourceInfos++;
		do
		{
			checkSlow(FRHIResourceTableEntry::GetUniformBufferIndex(ResourceInfo) == BufferIndex);
			const uint16 ResourceIndex = FRHIResourceTableEntry::GetResourceIndex(ResourceInfo);
			const uint8 BindIndex = FRHIResourceTableEntry::GetBindIndex(ResourceInfo);

			FD3D11BaseShaderResource* ShaderResource = nullptr;
			ID3D11ShaderResourceView* D3D11Resource = nullptr;

			FD3D11ShaderResourceView* ShaderResourceViewRHI = (FD3D11ShaderResourceView*)Resources[ResourceIndex].GetReference();
			ShaderResource = ShaderResourceViewRHI->Resource.GetReference();
			D3D11Resource = ShaderResourceViewRHI->View.GetReference();

			// todo: could coalesce adjacent bound resources.
			SetResource<ShaderFrequency>(D3D11RHI, StateCache, BindIndex, ShaderResource, D3D11Resource);
			NumSetCalls++;
			ResourceInfo = *ResourceInfos++;
		} while (FRHIResourceTableEntry::GetUniformBufferIndex(ResourceInfo) == BufferIndex);
	}
	return NumSetCalls;
}

template <EShaderFrequency ShaderFrequency>
inline int32 SetShaderResourcesFromBuffer_Sampler(FD3D11DynamicRHI* RESTRICT D3D11RHI, FD3D11StateCache* RESTRICT StateCache, FD3D11UniformBuffer* RESTRICT Buffer, const uint32* RESTRICT ResourceMap, int32 BufferIndex)
{
	const TRefCountPtr<FRHIResource>* RESTRICT Resources = Buffer->ResourceTable.GetData();
	int32 NumSetCalls = 0;
	uint32 BufferOffset = ResourceMap[BufferIndex];
	if (BufferOffset > 0)
	{
		const uint32* RESTRICT ResourceInfos = &ResourceMap[BufferOffset];
		uint32 ResourceInfo = *ResourceInfos++;
		do
		{
			checkSlow(FRHIResourceTableEntry::GetUniformBufferIndex(ResourceInfo) == BufferIndex);
			const uint16 ResourceIndex = FRHIResourceTableEntry::GetResourceIndex(ResourceInfo);
			const uint8 BindIndex = FRHIResourceTableEntry::GetBindIndex(ResourceInfo);

			ID3D11SamplerState* D3D11Resource = ((FD3D11SamplerState*)Resources[ResourceIndex].GetReference())->Resource.GetReference();

			// todo: could coalesce adjacent bound resources.
			SetResource<ShaderFrequency>(D3D11RHI, StateCache, BindIndex, D3D11Resource);
			NumSetCalls++;
			ResourceInfo = *ResourceInfos++;
		} while (FRHIResourceTableEntry::GetUniformBufferIndex(ResourceInfo) == BufferIndex);
	}
	return NumSetCalls;
}


template <class ShaderType>
void FD3D11DynamicRHI::SetResourcesFromTables(const ShaderType* RESTRICT Shader)
{
	checkSlow(Shader);

	// Mask the dirty bits by those buffers from which the shader has bound resources.
	uint32 DirtyBits = Shader->ShaderResourceTable.ResourceTableBits & DirtyUniformBuffers[ShaderType::StaticFrequency];
	while (DirtyBits)
	{
		// Scan for the lowest set bit, compute its index, clear it in the set of dirty bits.
		const uint32 LowestBitMask = (DirtyBits) & (-(int32)DirtyBits);
		const int32 BufferIndex = FMath::FloorLog2(LowestBitMask); // todo: This has a branch on zero, we know it could never be zero...
		DirtyBits ^= LowestBitMask;
		FD3D11UniformBuffer* Buffer = (FD3D11UniformBuffer*)BoundUniformBuffers[ShaderType::StaticFrequency][BufferIndex].GetReference();
		check(Buffer);
		check(BufferIndex < Shader->ShaderResourceTable.ResourceTableLayoutHashes.Num());

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		// to track down OR-7159 CRASH: Client crashed at start of match in D3D11Commands.cpp
		{
			if (Buffer->GetLayout().GetHash() != Shader->ShaderResourceTable.ResourceTableLayoutHashes[BufferIndex])
			{
				auto& BufferLayout = Buffer->GetLayout();
				FString DebugName = BufferLayout.GetDebugName().GetPlainNameString();
				const FString& ShaderName = Shader->ShaderName;
#if UE_BUILD_DEBUG
				FString ShaderUB;
				if (BufferIndex < Shader->UniformBuffers.Num())
				{
					ShaderUB = FString::Printf(TEXT("expecting UB '%s'"), *Shader->UniformBuffers[BufferIndex].GetPlainNameString());
				}
				UE_LOG(LogD3D11RHI, Error, TEXT("SetResourcesFromTables upcoming check(%08x != %08x); Bound Layout='%s' Shader='%s' %s"), BufferLayout.GetHash(), Shader->ShaderResourceTable.ResourceTableLayoutHashes[BufferIndex], *DebugName, *ShaderName, *ShaderUB);
				FString ResourcesString;
				for (int32 Index = 0; Index < BufferLayout.Resources.Num(); ++Index)
				{
					ResourcesString += FString::Printf(TEXT("%d "), BufferLayout.Resources[Index]);
				}
				UE_LOG(LogD3D11RHI, Error, TEXT("Layout CB Size %d Res Offs %d; %d Resources: %s"), BufferLayout.ConstantBufferSize, BufferLayout.ResourceOffset, BufferLayout.Resources.Num(), *ResourcesString);
#else
				UE_LOG(LogD3D11RHI, Error, TEXT("Bound Layout='%s' Shader='%s', Layout CB Size %d Res Offs %d; %d"), *DebugName, *ShaderName, BufferLayout.ConstantBufferSize, BufferLayout.ResourceOffset, BufferLayout.Resources.Num());
#endif
				// this might mean you are accessing a data you haven't bound e.g. GBuffer
				check(BufferLayout.GetHash() == Shader->ShaderResourceTable.ResourceTableLayoutHashes[BufferIndex]);
			}
		}
#endif

		// todo: could make this two pass: gather then set
		SetShaderResourcesFromBuffer_Surface<(EShaderFrequency)ShaderType::StaticFrequency>(this, &StateCache, Buffer, Shader->ShaderResourceTable.TextureMap.GetData(), BufferIndex);
		SetShaderResourcesFromBuffer_SRV<(EShaderFrequency)ShaderType::StaticFrequency>(this, &StateCache, Buffer, Shader->ShaderResourceTable.ShaderResourceViewMap.GetData(), BufferIndex);
		SetShaderResourcesFromBuffer_Sampler<(EShaderFrequency)ShaderType::StaticFrequency>(this, &StateCache, Buffer, Shader->ShaderResourceTable.SamplerMap.GetData(), BufferIndex);
	}
	DirtyUniformBuffers[ShaderType::StaticFrequency] = 0;
}

static int32 PeriodicCheck = 0;

void FD3D11DynamicRHI::CommitGraphicsResourceTables()
{
	FD3D11BoundShaderState* RESTRICT CurrentBoundShaderState = (FD3D11BoundShaderState*)BoundShaderStateHistory.GetLast();
	check(CurrentBoundShaderState);

	if (auto* Shader = CurrentBoundShaderState->GetVertexShader())
	{
		SetResourcesFromTables(Shader);
	}
	if (auto* Shader = CurrentBoundShaderState->GetPixelShader())
	{
		SetResourcesFromTables(Shader);
	}
	if (auto* Shader = CurrentBoundShaderState->GetHullShader())
	{
		SetResourcesFromTables(Shader);
	}
	if (auto* Shader = CurrentBoundShaderState->GetDomainShader())
	{
		SetResourcesFromTables(Shader);
	}
	if (auto* Shader = CurrentBoundShaderState->GetGeometryShader())
	{
		SetResourcesFromTables(Shader);
	}
}

void FD3D11DynamicRHI::CommitComputeResourceTables(FD3D11ComputeShader* InComputeShader)
{
	FD3D11ComputeShader* RESTRICT ComputeShader = InComputeShader;
	check(ComputeShader);
	SetResourcesFromTables(ComputeShader);
}

// WaveWorks Start
void FD3D11DynamicRHI::CommitResources()
{
	CommitGraphicsResourceTables();
	CommitNonComputeShaderConstants();
}
// WaveWorks End

void FD3D11DynamicRHI::RHIDrawPrimitive(uint32 PrimitiveType,uint32 BaseVertexIndex,uint32 NumPrimitives,uint32 NumInstances)
{
	RHI_DRAW_CALL_STATS(PrimitiveType,NumInstances*NumPrimitives);

	CommitGraphicsResourceTables();
	CommitNonComputeShaderConstants();

	uint32 VertexCount = GetVertexCountForPrimitiveCount(NumPrimitives,PrimitiveType);

	GPUProfilingData.RegisterGPUWork(NumPrimitives * NumInstances, VertexCount * NumInstances);
	VerifyPrimitiveType(PSOPrimitiveType, PrimitiveType);
	StateCache.SetPrimitiveTopology(GetD3D11PrimitiveType(PrimitiveType,bUsingTessellation));
	if(NumInstances > 1)
	{
		Direct3DDeviceIMContext->DrawInstanced(VertexCount,NumInstances,BaseVertexIndex,0);
	}
	else
	{
		Direct3DDeviceIMContext->Draw(VertexCount,BaseVertexIndex);
	}
}

void FD3D11DynamicRHI::RHIDrawPrimitiveIndirect(uint32 PrimitiveType,FVertexBufferRHIParamRef ArgumentBufferRHI,uint32 ArgumentOffset)
{
	FD3D11VertexBuffer* ArgumentBuffer = ResourceCast(ArgumentBufferRHI);

	RHI_DRAW_CALL_INC();

	GPUProfilingData.RegisterGPUWork(0);

	CommitGraphicsResourceTables();
	CommitNonComputeShaderConstants();

	VerifyPrimitiveType(PSOPrimitiveType, PrimitiveType);
	StateCache.SetPrimitiveTopology(GetD3D11PrimitiveType(PrimitiveType,bUsingTessellation));
	Direct3DDeviceIMContext->DrawInstancedIndirect(ArgumentBuffer->Resource,ArgumentOffset);
}

void FD3D11DynamicRHI::RHIDrawIndexedIndirect(FIndexBufferRHIParamRef IndexBufferRHI, uint32 PrimitiveType, FStructuredBufferRHIParamRef ArgumentsBufferRHI, int32 DrawArgumentsIndex, uint32 NumInstances)
{
	FD3D11IndexBuffer* IndexBuffer = ResourceCast(IndexBufferRHI);
	FD3D11StructuredBuffer* ArgumentsBuffer = ResourceCast(ArgumentsBufferRHI);

	RHI_DRAW_CALL_INC();

	GPUProfilingData.RegisterGPUWork(1);

	CommitGraphicsResourceTables();
	CommitNonComputeShaderConstants();

	// determine 16bit vs 32bit indices
	uint32 SizeFormat = sizeof(DXGI_FORMAT);
	const DXGI_FORMAT Format = (IndexBuffer->GetStride() == sizeof(uint16) ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT);

	StateCache.SetIndexBuffer(IndexBuffer->Resource, Format, 0);
	VerifyPrimitiveType(PSOPrimitiveType, PrimitiveType);
	StateCache.SetPrimitiveTopology(GetD3D11PrimitiveType(PrimitiveType,bUsingTessellation));

	if(NumInstances > 1)
	{
		Direct3DDeviceIMContext->DrawIndexedInstancedIndirect(ArgumentsBuffer->Resource, DrawArgumentsIndex * 5 * sizeof(uint32));
	}
	else
	{
		check(0);
	}
}

void FD3D11DynamicRHI::RHIDrawIndexedPrimitive(FIndexBufferRHIParamRef IndexBufferRHI,uint32 PrimitiveType,int32 BaseVertexIndex,uint32 FirstInstance,uint32 NumVertices,uint32 StartIndex,uint32 NumPrimitives,uint32 NumInstances)
{
	FD3D11IndexBuffer* IndexBuffer = ResourceCast(IndexBufferRHI);

	// called should make sure the input is valid, this avoid hidden bugs
	ensure(NumPrimitives > 0);

	RHI_DRAW_CALL_STATS(PrimitiveType,NumInstances*NumPrimitives);

	GPUProfilingData.RegisterGPUWork(NumPrimitives * NumInstances, NumVertices * NumInstances);

	CommitGraphicsResourceTables();
	CommitNonComputeShaderConstants();

	// determine 16bit vs 32bit indices
	uint32 SizeFormat = sizeof(DXGI_FORMAT);
	const DXGI_FORMAT Format = (IndexBuffer->GetStride() == sizeof(uint16) ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT);

	uint32 IndexCount = GetVertexCountForPrimitiveCount(NumPrimitives,PrimitiveType);

	// Verify that we are not trying to read outside the index buffer range
	// test is an optimized version of: StartIndex + IndexCount <= IndexBuffer->GetSize() / IndexBuffer->GetStride() 
	checkf((StartIndex + IndexCount) * IndexBuffer->GetStride() <= IndexBuffer->GetSize(), 		
		TEXT("Start %u, Count %u, Type %u, Buffer Size %u, Buffer stride %u"), StartIndex, IndexCount, PrimitiveType, IndexBuffer->GetSize(), IndexBuffer->GetStride());

	StateCache.SetIndexBuffer(IndexBuffer->Resource, Format, 0);
	VerifyPrimitiveType(PSOPrimitiveType, PrimitiveType);
	StateCache.SetPrimitiveTopology(GetD3D11PrimitiveType(PrimitiveType,bUsingTessellation));

	if (NumInstances > 1 || FirstInstance != 0)
	{
		Direct3DDeviceIMContext->DrawIndexedInstanced(IndexCount, NumInstances, StartIndex, BaseVertexIndex, FirstInstance);
	}
	else
	{
		Direct3DDeviceIMContext->DrawIndexed(IndexCount,StartIndex,BaseVertexIndex);
	}
}

void FD3D11DynamicRHI::RHIDrawIndexedPrimitiveIndirect(uint32 PrimitiveType,FIndexBufferRHIParamRef IndexBufferRHI,FVertexBufferRHIParamRef ArgumentBufferRHI,uint32 ArgumentOffset)
{
	FD3D11IndexBuffer* IndexBuffer = ResourceCast(IndexBufferRHI);
	FD3D11VertexBuffer* ArgumentBuffer = ResourceCast(ArgumentBufferRHI);

	RHI_DRAW_CALL_INC();

	GPUProfilingData.RegisterGPUWork(0);
	
	CommitGraphicsResourceTables();
	CommitNonComputeShaderConstants();
	
	// Set the index buffer.
	const uint32 SizeFormat = sizeof(DXGI_FORMAT);
	const DXGI_FORMAT Format = (IndexBuffer->GetStride() == sizeof(uint16) ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT);
	StateCache.SetIndexBuffer(IndexBuffer->Resource, Format, 0);
	VerifyPrimitiveType(PSOPrimitiveType, PrimitiveType);
	StateCache.SetPrimitiveTopology(GetD3D11PrimitiveType(PrimitiveType,bUsingTessellation));
	Direct3DDeviceIMContext->DrawIndexedInstancedIndirect(ArgumentBuffer->Resource,ArgumentOffset);
}

/**
 * Preallocate memory or get a direct command stream pointer to fill up for immediate rendering . This avoids memcpys below in DrawPrimitiveUP
 * @param PrimitiveType The type (triangles, lineloop, etc) of primitive to draw
 * @param NumPrimitives The number of primitives in the VertexData buffer
 * @param NumVertices The number of vertices to be written
 * @param VertexDataStride Size of each vertex 
 * @param OutVertexData Reference to the allocated vertex memory
 */
void FD3D11DynamicRHI::RHIBeginDrawPrimitiveUP( uint32 PrimitiveType, uint32 NumPrimitives, uint32 NumVertices, uint32 VertexDataStride, void*& OutVertexData)
{
	checkSlow( PendingNumVertices == 0 );

	// Remember the parameters for the draw call.
	PendingPrimitiveType = PrimitiveType;
	PendingNumPrimitives = NumPrimitives;
	PendingNumVertices = NumVertices;
	PendingVertexDataStride = VertexDataStride;

	// Map the dynamic buffer.
	OutVertexData = DynamicVB->Lock(NumVertices * VertexDataStride);
}

/**
 * Draw a primitive using the vertex data populated since RHIBeginDrawPrimitiveUP and clean up any memory as needed
 */
void FD3D11DynamicRHI::RHIEndDrawPrimitiveUP()
{
	RHI_DRAW_CALL_STATS(PendingPrimitiveType,PendingNumPrimitives);

	checkSlow(!bUsingTessellation || PendingPrimitiveType == PT_TriangleList);

	GPUProfilingData.RegisterGPUWork(PendingNumPrimitives,PendingNumVertices);

	// Unmap the dynamic vertex buffer.
	ID3D11Buffer* D3DBuffer = DynamicVB->Unlock();
	uint32 VBOffset = 0;

	// Issue the draw call.
	CommitGraphicsResourceTables();
	CommitNonComputeShaderConstants();
	StateCache.SetStreamSource(D3DBuffer, 0, PendingVertexDataStride, VBOffset);
	VerifyPrimitiveType(PSOPrimitiveType, PendingPrimitiveType);
	StateCache.SetPrimitiveTopology(GetD3D11PrimitiveType(PendingPrimitiveType,bUsingTessellation));
	Direct3DDeviceIMContext->Draw(PendingNumVertices,0);

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
void FD3D11DynamicRHI::RHIBeginDrawIndexedPrimitiveUP( uint32 PrimitiveType, uint32 NumPrimitives, uint32 NumVertices, uint32 VertexDataStride, void*& OutVertexData, uint32 MinVertexIndex, uint32 NumIndices, uint32 IndexDataStride, void*& OutIndexData)
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
	OutVertexData = DynamicVB->Lock(NumVertices * VertexDataStride);
	OutIndexData = DynamicIB->Lock(NumIndices * IndexDataStride);
}

/**
 * Draw a primitive using the vertex and index data populated since RHIBeginDrawIndexedPrimitiveUP and clean up any memory as needed
 */
void FD3D11DynamicRHI::RHIEndDrawIndexedPrimitiveUP()
{
	// tessellation only supports trilists
	checkSlow(!bUsingTessellation || PendingPrimitiveType == PT_TriangleList);

	RHI_DRAW_CALL_STATS(PendingPrimitiveType,PendingNumPrimitives);

	GPUProfilingData.RegisterGPUWork(PendingNumPrimitives,PendingNumVertices);

	// Unmap the dynamic buffers.
	ID3D11Buffer* VertexBuffer = DynamicVB->Unlock();
	ID3D11Buffer* IndexBuffer = DynamicIB->Unlock();
	uint32 VBOffset = 0;

	// Issue the draw call.
	CommitGraphicsResourceTables();
	CommitNonComputeShaderConstants();
	StateCache.SetStreamSource(VertexBuffer, 0, PendingVertexDataStride, VBOffset);
	StateCache.SetIndexBuffer(IndexBuffer, PendingIndexDataStride == sizeof(uint16) ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT, 0);
	VerifyPrimitiveType(PSOPrimitiveType, PendingPrimitiveType);
	StateCache.SetPrimitiveTopology(GetD3D11PrimitiveType(PendingPrimitiveType, bUsingTessellation));
	Direct3DDeviceIMContext->DrawIndexed(PendingNumIndices,0,PendingMinVertexIndex);

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
void FD3D11DynamicRHI::RHIClearMRT(bool bClearColor, int32 NumClearColors, const FLinearColor* ClearColorArray, bool bClearDepth, float Depth, bool bClearStencil, uint32 Stencil)
{
	RHIClearMRTImpl(bClearColor, NumClearColors, ClearColorArray, bClearDepth, Depth, bClearStencil, Stencil);
}

void FD3D11DynamicRHI::RHIClearMRTImpl(bool bClearColor, int32 NumClearColors, const FLinearColor* ClearColorArray, bool bClearDepth, float Depth, bool bClearStencil, uint32 Stencil)
{
	FD3D11BoundRenderTargets BoundRenderTargets(Direct3DDeviceIMContext);

	// Must specify enough clear colors for all active RTs
	check(!bClearColor || NumClearColors >= BoundRenderTargets.GetNumActiveTargets());

	// If we're clearing depth or stencil and we have a readonly depth/stencil view bound, we need to use a writable depth/stencil view
	if (CurrentDepthTexture)
	{
		FExclusiveDepthStencil RequestedAccess;
		
		RequestedAccess.SetDepthStencilWrite(bClearDepth, bClearStencil);

		ensure(RequestedAccess.IsValid(CurrentDSVAccessType));
	}

	ID3D11DepthStencilView* DepthStencilView = BoundRenderTargets.GetDepthStencilView();

	if (bClearColor && BoundRenderTargets.GetNumActiveTargets() > 0)
	{
		for (int32 TargetIndex = 0; TargetIndex < BoundRenderTargets.GetNumActiveTargets(); TargetIndex++)
		{				
			ID3D11RenderTargetView* RenderTargetView = BoundRenderTargets.GetRenderTargetView(TargetIndex);
			if (RenderTargetView != nullptr)
			{
				Direct3DDeviceIMContext->ClearRenderTargetView(RenderTargetView, (float*)&ClearColorArray[TargetIndex]);
			}
		}
	}

	if ((bClearDepth || bClearStencil) && DepthStencilView)
	{
		uint32 ClearFlags = 0;
		if (bClearDepth)
		{
			ClearFlags |= D3D11_CLEAR_DEPTH;
		}
		if (bClearStencil)
		{
			ClearFlags |= D3D11_CLEAR_STENCIL;
		}
		Direct3DDeviceIMContext->ClearDepthStencilView(DepthStencilView,ClearFlags,Depth,Stencil);
	}

	GPUProfilingData.RegisterGPUWork(0);
}

void FD3D11DynamicRHI::RHIBindClearMRTValues(bool bClearColor, bool bClearDepth, bool bClearStencil)
{
	// Not necessary for d3d.
}

// Blocks the CPU until the GPU catches up and goes idle.
void FD3D11DynamicRHI::RHIBlockUntilGPUIdle()
{
	// Not really supported
}

/**
 * Returns the total GPU time taken to render the last frame. Same metric as FPlatformTime::Cycles().
 */
uint32 FD3D11DynamicRHI::RHIGetGPUFrameCycles()
{
	return GGPUFrameTime;
}

void FD3D11DynamicRHI::RHIExecuteCommandList(FRHICommandList* CmdList)
{
	check(0); // this path has gone stale and needs updated methods, starting at ERCT_SetScissorRect
}

// NVIDIA Depth Bounds Test interface
void FD3D11DynamicRHI::RHIEnableDepthBoundsTest(bool bEnable,float MinDepth,float MaxDepth)
{
#if PLATFORM_DESKTOP
	if(!IsRHIDeviceNVIDIA() && !IsRHIDeviceAMD()) return;

	if(MinDepth > MaxDepth)
	{
		UE_LOG(LogD3D11RHI, Error,TEXT("RHIEnableDepthBoundsTest(%i,%f, %f) MinDepth > MaxDepth, cannot set DBT."),bEnable,MinDepth,MaxDepth);
		return;
	}

	if( MinDepth < 0.f || MaxDepth > 1.f)
	{
		UE_LOG(LogD3D11RHI, Verbose,TEXT("RHIEnableDepthBoundsTest(%i,%f, %f) depths out of range, will clamp."),bEnable,MinDepth,MaxDepth);
	}

	if(MinDepth > 1) MinDepth = 1.f;
	else if(MinDepth < 0) MinDepth = 0.f;

	if(MaxDepth > 1) MaxDepth = 1.f;
	else if(MaxDepth < 0) MaxDepth = 0.f;

	if (IsRHIDeviceNVIDIA())
	{
		auto result = NvAPI_D3D11_SetDepthBoundsTest( Direct3DDevice, bEnable, MinDepth, MaxDepth );
		if(result != NVAPI_OK)
		{
			static bool bOnce = false;
			if (!bOnce)
			{
				bOnce = true;
				UE_LOG(LogD3D11RHI, Error,TEXT("NvAPI_D3D11_SetDepthBoundsTest(%i,%f, %f) returned error code %i. **********PLEASE UPDATE YOUR VIDEO DRIVERS*********"),bEnable,MinDepth,MaxDepth,(unsigned int)result);
			}
		}
	}
	else if (IsRHIDeviceAMD())
	{
		auto result = agsDriverExtensionsDX11_SetDepthBounds( AmdAgsContext, bEnable, MinDepth, MaxDepth );
		if(result != AGS_SUCCESS)
		{
			static bool bOnce = false;
			if (!bOnce)
			{
				bOnce = true;
				UE_LOG(LogD3D11RHI, Error,TEXT("agsDriverExtensionsDX11_SetDepthBounds(%i,%f, %f) returned error code %i. **********PLEASE UPDATE YOUR VIDEO DRIVERS*********"),bEnable,MinDepth,MaxDepth,(unsigned int)result);
			}
		}
	}
#endif
}

void FD3D11DynamicRHI::RHISubmitCommandsHint()
{

}
IRHICommandContext* FD3D11DynamicRHI::RHIGetDefaultContext()
{
	return this;
}

IRHICommandContextContainer* FD3D11DynamicRHI::RHIGetCommandContextContainer(int32 Index, int32 Num)
{
	return nullptr;
}

void FD3D11DynamicRHI::RHITransitionResources(EResourceTransitionAccess TransitionType, FTextureRHIParamRef* InTextures, int32 NumTextures)
{
	static IConsoleVariable* CVarShowTransitions = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ProfileGPU.ShowTransitions"));
	bool bShowTransitionEvents = CVarShowTransitions->GetInt() != 0;

	SCOPED_RHI_CONDITIONAL_DRAW_EVENTF(*this, RHITransitionResources, bShowTransitionEvents, TEXT("TransitionTo: %s: %i Textures"), *FResourceTransitionUtility::ResourceTransitionAccessStrings[(int32)TransitionType], NumTextures);
	for (int32 i = 0; i < NumTextures; ++i)
	{				
		FTextureRHIParamRef RenderTarget = InTextures[i];
		if (RenderTarget)
		{
			SCOPED_RHI_CONDITIONAL_DRAW_EVENTF(*this, RHITransitionResourcesLoop, bShowTransitionEvents, TEXT("To:%i - %s"), i, *RenderTarget->GetName().ToString());

			FD3D11BaseShaderResource* Resource = nullptr;
			FD3D11Texture2D* SourceTexture2D = static_cast<FD3D11Texture2D*>(RenderTarget->GetTexture2D());
			if (SourceTexture2D)
			{
				Resource = SourceTexture2D;
			}
			FD3D11Texture2DArray* SourceTexture2DArray = static_cast<FD3D11Texture2DArray*>(RenderTarget->GetTexture2DArray());
			if (SourceTexture2DArray)
			{
				Resource = SourceTexture2DArray;
			}
			FD3D11TextureCube* SourceTextureCube = static_cast<FD3D11TextureCube*>(RenderTarget->GetTextureCube());
			if (SourceTextureCube)
			{
				Resource = SourceTextureCube;
			}
			FD3D11Texture3D* SourceTexture3D = static_cast<FD3D11Texture3D*>(RenderTarget->GetTexture3D());
			if (SourceTexture3D)
			{
				Resource = SourceTexture3D;
			}
			DUMP_TRANSITION(RenderTarget->GetName(), TransitionType);
			Resource->SetCurrentGPUAccess(TransitionType);
		}
	}
}

void FD3D11DynamicRHI::RHITransitionResources(EResourceTransitionAccess TransitionType, EResourceTransitionPipeline TransitionPipeline, FUnorderedAccessViewRHIParamRef* InUAVs, int32 InNumUAVs, FComputeFenceRHIParamRef WriteFence)
{
	for (int32 i = 0; i < InNumUAVs; ++i)
	{
		if (InUAVs[i])
		{
			FD3D11UnorderedAccessView* UAV = ResourceCast(InUAVs[i]);
			if (UAV && UAV->Resource)
			{
				UAV->Resource->SetCurrentGPUAccess(TransitionType);
				if (TransitionType != EResourceTransitionAccess::ERWNoBarrier)
				{
					UAV->Resource->SetDirty(false, PresentCounter);
				}
			}
		}
	}

	if (WriteFence)
	{
		WriteFence->WriteFence();
	}
}

// NVCHANGE_BEGIN: Add HBAO+
#if WITH_GFSDK_SSAO

static TAutoConsoleVariable<int32> CVarHBAOGBufferNormals(
	TEXT("r.HBAO.GBufferNormals"),
	1,
	TEXT(" 0: reconstruct normals from depths\n")
	TEXT(" 1: fetch GBuffer normals\n"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarHBAOVisualizeAO(
	TEXT("r.HBAO.VisualizeAO"),
	0,
	TEXT("To visualize the AO only"),
	ECVF_Cheat | ECVF_RenderThreadSafe);

void FD3D11DynamicRHI::RHIRenderHBAO(
	const FTextureRHIParamRef SceneDepthTextureRHI,
	const FMatrix& ProjectionMatrix,
	const FTextureRHIParamRef SceneNormalTextureRHI,
	const FMatrix& ViewMatrix,
	const FTextureRHIParamRef SceneColorTextureRHI,
	const GFSDK_SSAO_Parameters& BaseParams
)
{
	if (!HBAOContext)
	{
		return;
	}

	D3D11_VIEWPORT Viewport;
	uint32 NumViewports = 1;
	Direct3DDeviceIMContext->RSGetViewports(&NumViewports, &Viewport);

	FD3D11TextureBase* DepthTexture = GetD3D11TextureFromRHITexture(SceneDepthTextureRHI);
	ID3D11ShaderResourceView* DepthSRV = DepthTexture->GetShaderResourceView();

	FD3D11TextureBase* NewRenderTarget = GetD3D11TextureFromRHITexture(SceneColorTextureRHI);
	ID3D11RenderTargetView* RenderTargetView = NewRenderTarget->GetRenderTargetView(0, -1);

	GFSDK_SSAO_InputData_D3D11 Input;
	Input.DepthData.DepthTextureType = GFSDK_SSAO_HARDWARE_DEPTHS;
	Input.DepthData.pFullResDepthTextureSRV = DepthSRV;
	Input.DepthData.Viewport.Enable = true;
	Input.DepthData.Viewport.TopLeftX = uint32(Viewport.TopLeftX);
	Input.DepthData.Viewport.TopLeftY = uint32(Viewport.TopLeftY);
	Input.DepthData.Viewport.Width = uint32(Viewport.Width);
	Input.DepthData.Viewport.Height = uint32(Viewport.Height);
	Input.DepthData.ProjectionMatrix.Data = GFSDK_SSAO_Float4x4(&ProjectionMatrix.M[0][0]);
	Input.DepthData.ProjectionMatrix.Layout = GFSDK_SSAO_ROW_MAJOR_ORDER;
	Input.DepthData.MetersToViewSpaceUnits = 100.f;

	FD3D11TextureBase* NormalTexture = GetD3D11TextureFromRHITexture(SceneNormalTextureRHI);
	ID3D11ShaderResourceView* NormalSRV = NormalTexture->GetShaderResourceView();

	Input.NormalData.Enable = CVarHBAOGBufferNormals.GetValueOnRenderThread();
	Input.NormalData.pFullResNormalTextureSRV = NormalSRV;
	Input.NormalData.DecodeScale = 2.f;
	Input.NormalData.DecodeBias = -1.f;
	Input.NormalData.WorldToViewMatrix.Data = GFSDK_SSAO_Float4x4(&ViewMatrix.M[0][0]);
	Input.NormalData.WorldToViewMatrix.Layout = GFSDK_SSAO_ROW_MAJOR_ORDER;

	GFSDK_SSAO_Output_D3D11 Output;
	ZeroMemory(&Output, sizeof(Output));
	Output.pRenderTargetView = RenderTargetView;
	Output.Blend.Mode = CVarHBAOVisualizeAO.GetValueOnRenderThread() ? GFSDK_SSAO_OVERWRITE_RGB : GFSDK_SSAO_MULTIPLY_RGB;
	Output.TwoPassBlend.Enable = false;

	GFSDK_SSAO_Status Status;
	Status = HBAOContext->RenderAO(Direct3DDeviceIMContext, Input, BaseParams, Output, GFSDK_SSAO_RenderMask::GFSDK_SSAO_RENDER_AO);
	check(Status == GFSDK_SSAO_OK);
}

#endif
// NVCHANGE_END: Add HBAO+
