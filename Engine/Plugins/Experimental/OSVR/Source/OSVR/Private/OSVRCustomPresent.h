//
// Copyright 2016 Sensics, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#pragma once

#include "CoreMinimal.h"
#include "OSVRPrivate.h"
#include "IOSVR.h"
#include <osvr/RenderKit/RenderManagerC.h>

DECLARE_LOG_CATEGORY_EXTERN(FOSVRCustomPresentLog, Log, All);

template<class TGraphicsDevice>
class FOSVRCustomPresent : public FRHICustomPresent
{
public:
    FTexture2DRHIRef mRenderTexture;
  
    FOSVRCustomPresent(OSVR_ClientContext clientContext) :
        FRHICustomPresent(nullptr)
    {
        // If we are passed in a client context to use, we don't own it, so
        // we won't shut it down when we're done with it. Otherwise we will.
        // @todo - we're not currently using the passed-in clientContext, so
        // for now we always own it.
        //bOwnClientContext = (clientContext == nullptr);
        bOwnClientContext = true;
        mClientContext = osvrClientInit("com.osvr.unreal.plugin.FOSVRCustomPresent");
    }

    virtual ~FOSVRCustomPresent()
    {
        OSVR_ReturnCode rc;
        if (mRenderManager)
        {
            rc = osvrDestroyRenderManager(mRenderManager);
            if (rc != OSVR_RETURN_SUCCESS)
            {
                UE_LOG(FOSVRCustomPresentLog, Warning, TEXT("[OSVR] Failed to destroy the render manager in ~FOSVRCustomPresent()."));
            }
        }

        // only shut down the client context if we own it (currently always)
        if (bOwnClientContext && mClientContext)
        {
            rc = osvrClientShutdown(mClientContext);
            if (rc != OSVR_RETURN_SUCCESS)
            {
                UE_LOG(FOSVRCustomPresentLog, Warning, TEXT("[OSVR] Failed to shut down client context in ~FOSVRCustomPresent()."));
            }
        }
    }

    // virtual methods from FRHICustomPresent

    virtual void OnBackBufferResize() override
    {
    }

	virtual bool NeedsNativePresent() override
	{
		return true;
	}

	virtual bool Present(int32 &inOutSyncInterval) override
    {
        check(IsInRenderingThread());
        FScopeLock lock(&mOSVRMutex);
        InitializeImpl();
        FinishRendering();
        return true;
    }

    // implement this in the sub-class
    virtual bool Initialize()
    {
        FScopeLock lock(&mOSVRMutex);
        return InitializeImpl();
    }

    virtual bool IsInitialized()
    {
        return bInitialized;
    }

    virtual void GetProjectionMatrix(OSVR_RenderInfoCount eye, float &left, float &right, float &bottom, float &top, float nearClip, float farClip) = 0;
    virtual bool UpdateViewport(const FViewport& InViewport, class FRHIViewport* InViewportRHI) = 0;

    // RenderManager normalizes displays a bit. We create the render target assuming horizontal side-by-side.
    // RenderManager then rotates that render texture if needed for vertical side-by-side displays.
    virtual bool CalculateRenderTargetSize(uint32& InOutSizeX, uint32& InOutSizeY, float screenScale)
    {
        FScopeLock lock(&mOSVRMutex);
        return CalculateRenderTargetSizeImpl(InOutSizeX, InOutSizeY, screenScale);
    }

    virtual bool AllocateRenderTargetTexture(uint32 index, uint32 sizeX, uint32 sizeY, uint8 format, uint32 numMips, uint32 flags, uint32 targetableTextureFlags, FTexture2DRHIRef& outTargetableTexture, FTexture2DRHIRef& outShaderResourceTexture, uint32 numSamples = 1) = 0;

protected:
    FCriticalSection mOSVRMutex;
    TArray<OSVR_ViewportDescription> mViewportDescriptions;
    OSVR_RenderParams mRenderParams;

    bool bRenderBuffersNeedToUpdate = true;
    bool bInitialized = false;
    bool bOwnClientContext = true;
    OSVR_ClientContext mClientContext = nullptr;
    OSVR_RenderManager mRenderManager = nullptr;
    OSVR_RenderInfoCollection mCachedRenderInfoCollection = nullptr;

    virtual bool CalculateRenderTargetSizeImpl(uint32& InOutSizeX, uint32& InOutSizeY, float screenScale) = 0;

    virtual bool InitializeImpl() = 0;

    virtual TGraphicsDevice* GetGraphicsDevice()
    {
        auto ret = RHIGetNativeDevice();
        return reinterpret_cast<TGraphicsDevice*>(ret);
    }

    virtual void FinishRendering() = 0;

    // abstract methods, implement in DirectX/OpenGL specific subclasses
    virtual FString GetGraphicsLibraryName() = 0;
    virtual bool ShouldFlipY() = 0;
    virtual void UpdateRenderBuffers() = 0;
};

