// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "OculusHMDPrivate.h"

#if OCULUS_HMD_SUPPORTED_PLATFORMS

namespace OculusHMD
{

class FCustomPresent;


//-------------------------------------------------------------------------------------------------
// FTextureSetProxy
//-------------------------------------------------------------------------------------------------

class FTextureSetProxy : public TSharedFromThis<FTextureSetProxy, ESPMode::ThreadSafe>
{
public:
	FTextureSetProxy(FTextureRHIParamRef InRHITexture, const TArray<FTextureRHIRef>& InRHITextureSwapChain);
	virtual ~FTextureSetProxy();

	FRHITexture* GetTexture() const { return RHITexture.GetReference(); }
	FRHITexture2D* GetTexture2D() const { return RHITexture->GetTexture2D(); }
	FRHITextureCube* GetTextureCube() const { return RHITexture->GetTextureCube(); }
	uint32 GetSwapChainLength() const { return (uint32) RHITextureSwapChain.Num(); }

	void GenerateMips_RenderThread(FRHICommandListImmediate& RHICmdList);
	uint32 GetSwapChainIndex_RHIThread() { return SwapChainIndex_RHIThread; }
	void IncrementSwapChainIndex_RHIThread(FCustomPresent* CustomPresent);

protected:
	void ReleaseResources_RHIThread();

	FTextureRHIRef RHITexture;
	TArray<FTextureRHIRef> RHITextureSwapChain;
	uint32 SwapChainIndex_RHIThread;
};

typedef TSharedPtr<FTextureSetProxy, ESPMode::ThreadSafe> FTextureSetProxyPtr;


} // namespace OculusHMD

#endif //OCULUS_HMD_SUPPORTED_PLATFORMS
