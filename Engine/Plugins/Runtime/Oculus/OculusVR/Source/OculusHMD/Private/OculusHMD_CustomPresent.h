// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "OculusHMDPrivate.h"

#if OCULUS_HMD_SUPPORTED_PLATFORMS
#include "OculusHMD_Settings.h"
#include "OculusHMD_GameFrame.h"
#include "OculusHMD_TextureSetProxy.h"
#include "RHI.h"
#include "RendererInterface.h"
#include "IStereoLayers.h"

#if PLATFORM_WINDOWS
#include "WindowsHWrapper.h"
#endif

DECLARE_STATS_GROUP(TEXT("OculusHMD"), STATGROUP_OculusHMD, STATCAT_Advanced);
DECLARE_CYCLE_STAT(TEXT("BeginRendering"), STAT_BeginRendering, STATGROUP_OculusHMD);
DECLARE_CYCLE_STAT(TEXT("FinishRendering"), STAT_FinishRendering, STATGROUP_OculusHMD);
DECLARE_FLOAT_COUNTER_STAT(TEXT("LatencyRender"), STAT_LatencyRender, STATGROUP_OculusHMD);
DECLARE_FLOAT_COUNTER_STAT(TEXT("LatencyTimewarp"), STAT_LatencyTimewarp, STATGROUP_OculusHMD);
DECLARE_FLOAT_COUNTER_STAT(TEXT("LatencyPostPresent"), STAT_LatencyPostPresent, STATGROUP_OculusHMD);
DECLARE_FLOAT_COUNTER_STAT(TEXT("ErrorRender"), STAT_ErrorRender, STATGROUP_OculusHMD);
DECLARE_FLOAT_COUNTER_STAT(TEXT("ErrorTimewarp"), STAT_ErrorTimewarp, STATGROUP_OculusHMD);

namespace OculusHMD
{

//-------------------------------------------------------------------------------------------------
// FCustomPresent
//-------------------------------------------------------------------------------------------------

class FCustomPresent : public FRHICustomPresent
{
public:
	FCustomPresent(class FOculusHMD* InOculusHMD, ovrpRenderAPIType InRenderAPI, EPixelFormat InDefaultPixelFormat, bool InSupportsSRGB);

	// FRHICustomPresent
	virtual void OnBackBufferResize() override;
	virtual bool NeedsNativePresent() override;
	virtual bool Present(int32& SyncInterval) override;
	// virtual void PostPresent() override;

	ovrpRenderAPIType GetRenderAPI() const { return RenderAPI; }
	virtual bool IsUsingCorrectDisplayAdapter() const { return true; }

	void UpdateMirrorTexture_RenderThread();
	void FinishRendering_RHIThread();
	void ReleaseResources_RHIThread();
	void Shutdown();

	void UpdateViewport(FRHIViewport* InViewportRHI);

	FTexture2DRHIRef GetMirrorTexture() { return MirrorTextureRHI; }

	virtual void* GetOvrpInstance() const { return nullptr; }
	virtual void* GetOvrpDevice() const { return nullptr; }
	virtual void* GetOvrpCommandQueue() const { return nullptr; }
	EPixelFormat GetPixelFormat(EPixelFormat InFormat) const;
	EPixelFormat GetPixelFormat(ovrpTextureFormat InFormat) const;
	EPixelFormat GetDefaultPixelFormat() const { return DefaultPixelFormat; }
	ovrpTextureFormat GetOvrpTextureFormat(EPixelFormat InFormat) const;
	ovrpTextureFormat GetDefaultOvrpTextureFormat() const { return DefaultOvrpTextureFormat; }
	static bool IsSRGB(ovrpTextureFormat InFormat);

	virtual FTextureRHIRef CreateTexture_RenderThread(uint32 InSizeX, uint32 InSizeY, EPixelFormat InFormat, FClearValueBinding InBinding, uint32 InNumMips, uint32 InNumSamples, uint32 InNumSamplesTileMem, ERHIResourceType InResourceType, ovrpTextureHandle InTexture, uint32 TexCreateFlags) = 0;
	FTextureSetProxyPtr CreateTextureSetProxy_RenderThread(uint32 InSizeX, uint32 InSizeY, EPixelFormat InFormat, FClearValueBinding InBinding, uint32 InNumMips, uint32 InNumSamples, uint32 InNumSamplesTileMem, ERHIResourceType InResourceType, const TArray<ovrpTextureHandle>& InTextures, uint32 InTexCreateFlags);
	void CopyTexture_RenderThread(FRHICommandListImmediate& RHICmdList, FTextureRHIParamRef DstTexture, FTextureRHIParamRef SrcTexture, FIntRect DstRect = FIntRect(), FIntRect SrcRect = FIntRect(), bool bAlphaPremultiply = false, bool bNoAlphaWrite = false, bool bInvertY = true) const;
	virtual void AliasTextureResources_RHIThread(FTextureRHIParamRef DestTexture, FTextureRHIParamRef SrcTexture) = 0;

protected:
	FOculusHMD* OculusHMD;
	ovrpRenderAPIType RenderAPI;
	EPixelFormat DefaultPixelFormat;
	ovrpTextureFormat DefaultOvrpTextureFormat;
	bool bSupportsSRGB;
	IRendererModule* RendererModule;
	FTexture2DRHIRef MirrorTextureRHI;
};


//-------------------------------------------------------------------------------------------------
// APIs
//-------------------------------------------------------------------------------------------------

#if OCULUS_HMD_SUPPORTED_PLATFORMS_D3D11
FCustomPresent* CreateCustomPresent_D3D11(FOculusHMD* InOculusHMD);
#endif
#if OCULUS_HMD_SUPPORTED_PLATFORMS_D3D12
FCustomPresent* CreateCustomPresent_D3D12(FOculusHMD* InOculusHMD);
#endif
#if OCULUS_HMD_SUPPORTED_PLATFORMS_OPENGL
FCustomPresent* CreateCustomPresent_OpenGL(FOculusHMD* InOculusHMD);
#endif
#if OCULUS_HMD_SUPPORTED_PLATFORMS_VULKAN
FCustomPresent* CreateCustomPresent_Vulkan(FOculusHMD* InOculusHMD);
#endif


} // namespace OculusHMD

#endif //OCULUS_HMD_SUPPORTED_PLATFORMS
