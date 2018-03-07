// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
//
#include "CoreMinimal.h"
#include "SteamVRPrivate.h"

#if STEAMVR_SUPPORTED_PLATFORMS

#include "SteamVRHMD.h"

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "PostProcess/PostProcessHMD.h"
#include "PipelineStateCache.h"
#include "ClearQuad.h"
#include "DefaultSpectatorScreenController.h"
#include "ScreenRendering.h"

#if PLATFORM_MAC
#include <Metal/Metal.h>
#else
#include "VulkanRHIPrivate.h"
#include "VulkanPendingState.h"
#include "VulkanContext.h"
#endif

static TAutoConsoleVariable<int32> CUsePostPresentHandoff(TEXT("vr.SteamVR.UsePostPresentHandoff"), 0, TEXT("Whether or not to use PostPresentHandoff.  If true, more GPU time will be available, but this relies on no SceneCaptureComponent2D or WidgetComponents being active in the scene.  Otherwise, it will break async reprojection."));

void FSteamVRHMD::DrawDistortionMesh_RenderThread(struct FRenderingCompositePassContext& Context, const FIntPoint& TextureSize)
{
	check(0);
}

void FSteamVRHMD::RenderTexture_RenderThread(FRHICommandListImmediate& RHICmdList, FTexture2DRHIParamRef BackBuffer, FTexture2DRHIParamRef SrcTexture, FVector2D WindowSize) const
{
	check(IsInRenderingThread());
	const_cast<FSteamVRHMD*>(this)->UpdateStereoLayers_RenderThread();

	if (bSplashIsShown)
	{
		SetRenderTarget(RHICmdList, SrcTexture, FTextureRHIRef());
		DrawClearQuad(RHICmdList, FLinearColor(0, 0, 0, 0));
	}

	check(SpectatorScreenController);
	SpectatorScreenController->RenderSpectatorScreen_RenderThread(RHICmdList, BackBuffer, SrcTexture, WindowSize);
}

static void DrawOcclusionMesh(FRHICommandList& RHICmdList, EStereoscopicPass StereoPass, const FHMDViewMesh MeshAssets[])
{
	check(IsInRenderingThread());
	check(StereoPass != eSSP_FULL);

	const uint32 MeshIndex = (StereoPass == eSSP_LEFT_EYE) ? 0 : 1;
	const FHMDViewMesh& Mesh = MeshAssets[MeshIndex];
	check(Mesh.IsValid());

	DrawIndexedPrimitiveUP(
		RHICmdList,
		PT_TriangleList,
		0,
		Mesh.NumVertices,
		Mesh.NumTriangles,
		Mesh.pIndices,
		sizeof(Mesh.pIndices[0]),
		Mesh.pVertices,
		sizeof(Mesh.pVertices[0])
		);
}

void FSteamVRHMD::DrawHiddenAreaMesh_RenderThread(FRHICommandList& RHICmdList, EStereoscopicPass StereoPass) const
{
	DrawOcclusionMesh(RHICmdList, StereoPass, HiddenAreaMeshes);
}

void FSteamVRHMD::DrawVisibleAreaMesh_RenderThread(FRHICommandList& RHICmdList, EStereoscopicPass StereoPass) const
{
	DrawOcclusionMesh(RHICmdList, StereoPass, VisibleAreaMeshes);
}

bool FSteamVRHMD::BridgeBaseImpl::NeedsNativePresent()
{
    if (Plugin->VRCompositor == nullptr)
    {
        return false;
    }
    
    return true;
}

void FSteamVRHMD::BridgeBaseImpl::UpdateFrameSettings(FSteamVRHMD::FFrameSettings& NewSettings)
{
	FrameSettingsStack.Add(NewSettings);
	if (FrameSettingsStack.Num() > 3)
	{
		FrameSettingsStack.RemoveAt(0);
	}
}

FSteamVRHMD::FFrameSettings FSteamVRHMD::BridgeBaseImpl::GetFrameSettings(int32 NumBufferedFrames/*=0*/)
{
	check(FrameSettingsStack.Num() > 0);
	if (NumBufferedFrames < FrameSettingsStack.Num())
	{
		return FrameSettingsStack[NumBufferedFrames];
	}
	else
	{
		// Until we build a buffer of adequate size, stick with the last submitted
		return FrameSettingsStack[0];
	}
}

#if PLATFORM_WINDOWS

FSteamVRHMD::D3D11Bridge::D3D11Bridge(FSteamVRHMD* plugin):
	BridgeBaseImpl(plugin),
	RenderTargetTexture(nullptr)
{
}

void FSteamVRHMD::D3D11Bridge::BeginRendering()
{
	check(IsInRenderingThread());

	static bool Inited = false;
	if (!Inited)
	{
		Inited = true;
	}
}

void FSteamVRHMD::D3D11Bridge::FinishRendering()
{
	vr::Texture_t Texture;
	Texture.handle = RenderTargetTexture;
	Texture.eType = vr::TextureType_DirectX;
	Texture.eColorSpace = vr::ColorSpace_Auto;

	vr::VRTextureBounds_t LeftBounds;
    LeftBounds.uMin = 0.0f;
	LeftBounds.uMax = 0.5f;
    LeftBounds.vMin = 0.0f;
    LeftBounds.vMax = 1.0f;
	
    vr::EVRCompositorError Error = Plugin->VRCompositor->Submit(vr::Eye_Left, &Texture, &LeftBounds);

	vr::VRTextureBounds_t RightBounds;
	RightBounds.uMin = 0.5f;
    RightBounds.uMax = 1.0f;
    RightBounds.vMin = 0.0f;
    RightBounds.vMax = 1.0f;
	
    
	Texture.handle = RenderTargetTexture;
	Error = Plugin->VRCompositor->Submit(vr::Eye_Right, &Texture, &RightBounds);
    if (Error != vr::VRCompositorError_None)
	{
		UE_LOG(LogHMD, Log, TEXT("Warning:  SteamVR Compositor had an error on present (%d)"), (int32)Error);
	}
}

void FSteamVRHMD::D3D11Bridge::Reset()
{
}

void FSteamVRHMD::D3D11Bridge::UpdateViewport(const FViewport& Viewport, FRHIViewport* InViewportRHI)
{
	check(IsInGameThread());
	check(InViewportRHI);

	const FTexture2DRHIRef& RT = Viewport.GetRenderTargetTexture();
	check(IsValidRef(RT));

	if (RenderTargetTexture != nullptr)
	{
		RenderTargetTexture->Release();	//@todo steamvr: need to also release in reset
	}

	RenderTargetTexture = (ID3D11Texture2D*)RT->GetNativeResource();
	RenderTargetTexture->AddRef();

	InViewportRHI->SetCustomPresent(this);
}


void FSteamVRHMD::D3D11Bridge::OnBackBufferResize()
{
}

bool FSteamVRHMD::D3D11Bridge::Present(int& SyncInterval)
{
	check(IsInRenderingThread());

	if (Plugin->VRCompositor == nullptr)
	{
		return false;
	}

	FinishRendering();

	return true;
}

void FSteamVRHMD::D3D11Bridge::PostPresent()
{
	if (CUsePostPresentHandoff.GetValueOnRenderThread() == 1)
	{
		Plugin->VRCompositor->PostPresentHandoff();
	}
}
#endif // PLATFORM_WINDOWS

#if !PLATFORM_MAC
FSteamVRHMD::VulkanBridge::VulkanBridge(FSteamVRHMD* plugin):
	BridgeBaseImpl(plugin),
	RenderTargetTexture(0)
{
	bInitialized = true;
}

void FSteamVRHMD::VulkanBridge::BeginRendering()
{
//	check(IsInRenderingThread());
}

void FSteamVRHMD::VulkanBridge::FinishRendering()
{
	auto vlkRHI = static_cast<FVulkanDynamicRHI*>(GDynamicRHI);

	if(RenderTargetTexture.IsValid())
	{
		FVulkanTexture2D* Texture2D = (FVulkanTexture2D*)RenderTargetTexture.GetReference();
		FVulkanCommandListContext& ImmediateContext = vlkRHI->GetDevice()->GetImmediateContext();
		const VkImageLayout* CurrentLayout = ImmediateContext.GetTransitionState().CurrentLayout.Find(Texture2D->Surface.Image);
		FVulkanCmdBuffer* CmdBuffer = ImmediateContext.GetCommandBufferManager()->GetUploadCmdBuffer();
		VkImageSubresourceRange SubresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
		vlkRHI->VulkanSetImageLayout( CmdBuffer->GetHandle(), Texture2D->Surface.Image, CurrentLayout ? *CurrentLayout : VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, SubresourceRange );

		vr::VRTextureBounds_t LeftBounds;
		LeftBounds.uMin = 0.0f;
		LeftBounds.uMax = 0.5f;
		LeftBounds.vMin = 0.0f;
		LeftBounds.vMax = 1.0f;

		vr::VRTextureBounds_t RightBounds;
		RightBounds.uMin = 0.5f;
		RightBounds.uMax = 1.0f;
		RightBounds.vMin = 0.0f;
		RightBounds.vMax = 1.0f;

		vr::VRVulkanTextureData_t vulkanData {};
		vulkanData.m_pInstance			= vlkRHI->GetInstance();
		vulkanData.m_pDevice			= vlkRHI->GetDevice()->GetInstanceHandle();
		vulkanData.m_pPhysicalDevice	= vlkRHI->GetDevice()->GetPhysicalHandle();
		vulkanData.m_pQueue				= vlkRHI->GetDevice()->GetGraphicsQueue()->GetHandle();
		vulkanData.m_nQueueFamilyIndex	= vlkRHI->GetDevice()->GetGraphicsQueue()->GetFamilyIndex();
		vulkanData.m_nImage				= (uint64_t)Texture2D->Surface.Image;
		vulkanData.m_nWidth				= Texture2D->Surface.Width;
		vulkanData.m_nHeight			= Texture2D->Surface.Height;
		vulkanData.m_nFormat			= (uint32_t)Texture2D->Surface.ViewFormat;
		vulkanData.m_nSampleCount = 1;

		vr::Texture_t texture = {&vulkanData, vr::TextureType_Vulkan, vr::ColorSpace_Auto};

		Plugin->VRCompositor->Submit(vr::Eye_Left, &texture, &LeftBounds);
		Plugin->VRCompositor->Submit(vr::Eye_Right, &texture, &RightBounds);

		ImmediateContext.GetCommandBufferManager()->SubmitUploadCmdBuffer(false);
	}
}

void FSteamVRHMD::VulkanBridge::Reset()
{

}

void FSteamVRHMD::VulkanBridge::UpdateViewport(const FViewport& Viewport, FRHIViewport* InViewportRHI)
{
	RenderTargetTexture = Viewport.GetRenderTargetTexture();
	check(IsValidRef(RenderTargetTexture));

	InViewportRHI->SetCustomPresent(this);
}


void FSteamVRHMD::VulkanBridge::OnBackBufferResize()
{
}

bool FSteamVRHMD::VulkanBridge::Present(int& SyncInterval)
{
	if (Plugin->VRCompositor == nullptr)
	{
		return false;
	}

	FinishRendering();

	return true;
}

void FSteamVRHMD::VulkanBridge::PostPresent()
{
	if (CUsePostPresentHandoff.GetValueOnRenderThread() == 1)
	{
		Plugin->VRCompositor->PostPresentHandoff();
	}
}

FSteamVRHMD::OpenGLBridge::OpenGLBridge(FSteamVRHMD* plugin):
	BridgeBaseImpl(plugin),
	RenderTargetTexture(0)
{
	bInitialized = true;
}

void FSteamVRHMD::OpenGLBridge::BeginRendering()
{
	check(IsInRenderingThread());


}

void FSteamVRHMD::OpenGLBridge::FinishRendering()
{
	// Yaakuro:
	// TODO This is a workaround. After exiting VR Editor the texture gets invalid at some point.
	// Need to find it. This at least prevents to use this method when texture name is not valid anymore.
	if(!glIsTexture(RenderTargetTexture))
	{
		return;
	}

	vr::VRTextureBounds_t LeftBounds;
	LeftBounds.uMin = 0.0f;
	LeftBounds.uMax = 0.5f;
	LeftBounds.vMin = 1.0f;
	LeftBounds.vMax = 0.0f;

	vr::VRTextureBounds_t RightBounds;
	RightBounds.uMin = 0.5f;
	RightBounds.uMax = 1.0f;
	RightBounds.vMin = 1.0f;
	RightBounds.vMax = 0.0f;

	vr::Texture_t Texture;
	Texture.handle = reinterpret_cast<void*>(static_cast<size_t>(RenderTargetTexture));
	Texture.eType = vr::TextureType_OpenGL;
	Texture.eColorSpace = vr::ColorSpace_Auto;

	Plugin->VRCompositor->Submit(vr::Eye_Left, &Texture, &LeftBounds);
	Plugin->VRCompositor->Submit(vr::Eye_Right, &Texture, &RightBounds);
}

void FSteamVRHMD::OpenGLBridge::Reset()
{
	RenderTargetTexture = 0;
}

void FSteamVRHMD::OpenGLBridge::UpdateViewport(const FViewport& Viewport, FRHIViewport* InViewportRHI)
{
	check(IsInGameThread());

	const FTexture2DRHIRef& RT = Viewport.GetRenderTargetTexture();
	check(IsValidRef(RT));

	RenderTargetTexture = *reinterpret_cast<GLuint*>(RT->GetNativeResource());

	InViewportRHI->SetCustomPresent(this);
}


void FSteamVRHMD::OpenGLBridge::OnBackBufferResize()
{
}

bool FSteamVRHMD::OpenGLBridge::Present(int& SyncInterval)
{
	check(IsInRenderingThread());

	if (Plugin->VRCompositor == nullptr)
	{
		return false;
	}

	FinishRendering();

	return true;
}

void FSteamVRHMD::OpenGLBridge::PostPresent()
{
	if (CUsePostPresentHandoff.GetValueOnRenderThread() == 1)
	{
		Plugin->VRCompositor->PostPresentHandoff();
	}
}

#elif PLATFORM_MAC

FSteamVRHMD::MetalBridge::MetalBridge(FSteamVRHMD* plugin):
	BridgeBaseImpl(plugin)
{}

void FSteamVRHMD::MetalBridge::BeginRendering()
{
	check(IsInRenderingThread());

	static bool Inited = false;
	if (!Inited)
	{
		Inited = true;
	}
}

void FSteamVRHMD::MetalBridge::FinishRendering()
{
	if(IsOnLastPresentedFrame())
	{
		return;
	}
	
	LastPresentedFrameNumber = GetFrameNumber();
	
	check(TextureSet.IsValid());

	vr::VRTextureBounds_t LeftBounds;
	LeftBounds.uMin = 0.0f;
	LeftBounds.uMax = 0.5f;
	LeftBounds.vMin = 0.0f;
	LeftBounds.vMax = 1.0f;
	
	id<MTLTexture> TextureHandle = (id<MTLTexture>)TextureSet->GetNativeResource();
	
	vr::Texture_t Texture;
	Texture.handle = (void*)TextureHandle.iosurface;
	Texture.eType = vr::TextureType_IOSurface;
	Texture.eColorSpace = vr::ColorSpace_Auto;
	vr::EVRCompositorError Error = Plugin->VRCompositor->Submit(vr::Eye_Left, &Texture, &LeftBounds);

	vr::VRTextureBounds_t RightBounds;
	RightBounds.uMin = 0.5f;
	RightBounds.uMax = 1.0f;
	RightBounds.vMin = 0.0f;
	RightBounds.vMax = 1.0f;
	
	Error = Plugin->VRCompositor->Submit(vr::Eye_Right, &Texture, &RightBounds);
	if (Error != vr::VRCompositorError_None)
	{
		UE_LOG(LogHMD, Log, TEXT("Warning:  SteamVR Compositor had an error on present (%d)"), (int32)Error);
	}

	static_cast<FRHITextureSet2D*>(TextureSet.GetReference())->Advance();
}

void FSteamVRHMD::MetalBridge::Reset()
{
}

void FSteamVRHMD::MetalBridge::UpdateViewport(const FViewport& Viewport, FRHIViewport* InViewportRHI)
{
	check(IsInGameThread());
	check(InViewportRHI);
	InViewportRHI->SetCustomPresent(this);
}

void FSteamVRHMD::MetalBridge::OnBackBufferResize()
{
}

bool FSteamVRHMD::MetalBridge::Present(int& SyncInterval)
{
	// Editor appears to be in rt, game appears to be in rhi?
	//check(IsInRenderingThread());
	//check(IsInRHIThread());

	if (Plugin->VRCompositor == nullptr)
	{
		return false;
	}

	FinishRendering();

	return true;
}

void FSteamVRHMD::MetalBridge::PostPresent()
{
	if (CUsePostPresentHandoff.GetValueOnRenderThread() == 1)
	{
		Plugin->VRCompositor->PostPresentHandoff();
	}
}

IOSurfaceRef FSteamVRHMD::MetalBridge::GetSurface(const uint32 SizeX, const uint32 SizeY)
{
	// @todo: Get our man in MacVR to switch to a modern & secure method of IOSurface sharing...
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	const NSDictionary* SurfaceDefinition = @{
											(id)kIOSurfaceWidth: @(SizeX),
											(id)kIOSurfaceHeight: @(SizeY),
											(id)kIOSurfaceBytesPerElement: @(4), // sizeof(PF_B8G8R8A8)..
											(id)kIOSurfaceIsGlobal: @YES
											};
	
	return IOSurfaceCreate((CFDictionaryRef)SurfaceDefinition);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

#endif // PLATFORM_MAC

#endif // STEAMVR_SUPPORTED_PLATFORMS
