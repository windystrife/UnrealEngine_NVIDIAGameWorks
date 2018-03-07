//
// Copyright 2014, 2015 Razer Inc.
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
#include "Misc/ScopeLock.h"

#if PLATFORM_WINDOWS

#include "IOSVR.h"
#include "OSVRCustomPresent.h"

#include "AllowWindowsPlatformTypes.h"
#include <osvr/RenderKit/RenderManagerD3D11C.h>
#include "HideWindowsPlatformTypes.h"
#include "Runtime/Windows/D3D11RHI/Private/D3D11RHIPrivate.h"

class FCurrentCustomPresent : public FOSVRCustomPresent<ID3D11Device>
{
public:
    FCurrentCustomPresent(OSVR_ClientContext clientContext) :
        FOSVRCustomPresent(clientContext)
    {
    }

    virtual bool UpdateViewport(const FViewport& InViewport, class FRHIViewport* InViewportRHI) override
    {
        FScopeLock lock(&mOSVRMutex);

        check(IsInGameThread());
        if (!IsInitialized())
        {
            UE_LOG(FOSVRCustomPresentLog, Warning, TEXT("UpdateViewport called but custom present is not initialized - doing nothing"));
            return false;
        }
        else
        {
            check(InViewportRHI);
            //const FTexture2DRHIRef& rt = InViewport.GetRenderTargetTexture();
            //check(IsValidRef(rt));
            //SetRenderTargetTexture((ID3D11Texture2D*)rt->GetNativeResource()); // @todo: do we need to do this?
            auto oldCustomPresent = InViewportRHI->GetCustomPresent();
            if (oldCustomPresent != this)
            {
                InViewportRHI->SetCustomPresent(this);
            }
            // UpdateViewport is called before we're initialized, so we have to
            // defer updates to the render buffers until we're in the render thread.
            //bRenderBuffersNeedToUpdate = true;
            return true;
        }
    }

    virtual bool AllocateRenderTargetTexture(uint32 index, uint32 sizeX, uint32 sizeY, uint8 format, uint32 numMips, uint32 flags, uint32 targetableTextureFlags, FTexture2DRHIRef& outTargetableTexture, FTexture2DRHIRef& outShaderResourceTexture, uint32 numSamples = 1) override
    {
        // @todo how should we determine SRGB?
        const bool bSRGB = false;
        const DXGI_FORMAT platformResourceFormat = (DXGI_FORMAT)GPixelFormats[format].PlatformFormat;
        const DXGI_FORMAT platformShaderResourceFormat = FindShaderResourceDXGIFormat(platformResourceFormat, bSRGB);
        const DXGI_FORMAT platformRenderTargetFormat = FindShaderResourceDXGIFormat(platformResourceFormat, bSRGB);

        FScopeLock lock(&mOSVRMutex);
        if (IsInitialized())
        {
            auto d3d11RHI = static_cast<FD3D11DynamicRHI*>(GDynamicRHI);
            auto graphicsDevice = GetGraphicsDevice();
            HRESULT hr;
            D3D11_TEXTURE2D_DESC textureDesc;
            memset(&textureDesc, 0, sizeof(textureDesc));
            textureDesc.Width = sizeX;
            textureDesc.Height = sizeY;
            textureDesc.MipLevels = numMips;
            textureDesc.ArraySize = 1;
            //textureDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
            //textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            textureDesc.Format = platformResourceFormat;
            textureDesc.SampleDesc.Count = numSamples;
            textureDesc.SampleDesc.Quality = numSamples > 0 ? D3D11_STANDARD_MULTISAMPLE_PATTERN : 0;
            textureDesc.Usage = D3D11_USAGE_DEFAULT;
            // We need it to be both a render target and a shader resource
            textureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
            textureDesc.CPUAccessFlags = 0;
            textureDesc.MiscFlags = 0;

            ID3D11Texture2D *D3DTexture = nullptr;
            hr = graphicsDevice->CreateTexture2D(
                &textureDesc, NULL, &D3DTexture);
            check(!FAILED(hr));

            SetRenderTargetTexture(D3DTexture);

            D3D11_RENDER_TARGET_VIEW_DESC renderTargetViewDesc;
            memset(&renderTargetViewDesc, 0, sizeof(renderTargetViewDesc));
            // This must match what was created in the texture to be rendered
            //renderTargetViewDesc.Format = renderTextureDesc.Format;
            //renderTargetViewDesc.Format = textureDesc.Format;
            renderTargetViewDesc.Format = platformRenderTargetFormat;
            renderTargetViewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
            renderTargetViewDesc.Texture2D.MipSlice = 0;

            // Create the render target view.
            ID3D11RenderTargetView *renderTargetView; //< Pointer to our render target view
            hr = graphicsDevice->CreateRenderTargetView(
                RenderTargetTexture, &renderTargetViewDesc, &renderTargetView);
            check(!FAILED(hr));

            RenderTargetView = renderTargetView;

            ID3D11ShaderResourceView* shaderResourceView = nullptr;
            bool bCreatedRTVsPerSlice = false;
            int32 rtvArraySize = 1;
            TArray<TRefCountPtr<ID3D11RenderTargetView>> renderTargetViews;
            TRefCountPtr<ID3D11DepthStencilView>* depthStencilViews = nullptr;
            uint32 sizeZ = 0;
            EPixelFormat epFormat = EPixelFormat(format);
            bool bCubemap = false;
            bool bPooled = false;
            // override flags
            flags = TexCreate_RenderTargetable | TexCreate_ShaderResource;

            renderTargetViews.Add(renderTargetView);
            D3D11_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc;
            memset(&shaderResourceViewDesc, 0, sizeof(shaderResourceViewDesc));
            //shaderResourceViewDesc.Format = textureDesc.Format;
            shaderResourceViewDesc.Format = platformShaderResourceFormat;
            shaderResourceViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            shaderResourceViewDesc.Texture2D.MipLevels = textureDesc.MipLevels;
            shaderResourceViewDesc.Texture2D.MostDetailedMip = textureDesc.MipLevels - 1;

            hr = graphicsDevice->CreateShaderResourceView(
                RenderTargetTexture, &shaderResourceViewDesc, &shaderResourceView);
            check(!FAILED(hr));

            auto targetableTexture = new FD3D11Texture2D(
                d3d11RHI, D3DTexture, shaderResourceView, bCreatedRTVsPerSlice,
                rtvArraySize, renderTargetViews, depthStencilViews,
                textureDesc.Width, textureDesc.Height, sizeZ, numMips, numSamples, epFormat,
                bCubemap, flags, bPooled, FClearValueBinding::Black);

            outTargetableTexture = targetableTexture->GetTexture2D();
            outShaderResourceTexture = targetableTexture->GetTexture2D();
            mRenderTexture = targetableTexture;
            bRenderBuffersNeedToUpdate = true;
            UpdateRenderBuffers();
            return true;
        }
        return false;
    }


    virtual void GetProjectionMatrix(OSVR_RenderInfoCount eye, float &left, float &right, float &bottom, float &top, float nearClip, float farClip) override
    {
        OSVR_ReturnCode rc;
        rc = osvrRenderManagerGetDefaultRenderParams(&mRenderParams);
        check(rc == OSVR_RETURN_SUCCESS);

        mRenderParams.nearClipDistanceMeters = static_cast<double>(nearClip);
        mRenderParams.farClipDistanceMeters = static_cast<double>(farClip);

        // this method gets called with alternating eyes starting with the left. We get the render info when
        // the left eye (index 0) is requested (releasing the old one, if any),
        // and re-use the same collection when the right eye (index 0) is requested
        if (eye == 0 || !mCachedRenderInfoCollection) {
            if (mCachedRenderInfoCollection) {
                rc = osvrRenderManagerReleaseRenderInfoCollection(mCachedRenderInfoCollection);
                check(rc == OSVR_RETURN_SUCCESS);
            }
            rc = osvrRenderManagerGetRenderInfoCollection(mRenderManager, mRenderParams, &mCachedRenderInfoCollection);
            check(rc == OSVR_RETURN_SUCCESS);
        }

        OSVR_RenderInfoD3D11 renderInfo;
        rc = osvrRenderManagerGetRenderInfoFromCollectionD3D11(mCachedRenderInfoCollection, eye, &renderInfo);
        check(rc == OSVR_RETURN_SUCCESS);

        // previously we divided these by renderInfo.projection.nearClip but we need
        // to pass these unmodified through to the OSVR_Projection_to_D3D call (and OpenGL
        // equivalent)
        left = static_cast<float>(renderInfo.projection.left);
        right = static_cast<float>(renderInfo.projection.right);
        top = static_cast<float>(renderInfo.projection.top);
        bottom = static_cast<float>(renderInfo.projection.bottom);
    }

protected:
    ID3D11Texture2D* RenderTargetTexture = NULL;
    ID3D11RenderTargetView* RenderTargetView = NULL;

    TArray<OSVR_RenderBufferD3D11> mRenderBuffers;
    TArray<OSVR_RenderInfoD3D11> mRenderInfos;
    OSVR_RenderManagerD3D11 mRenderManagerD3D11 = nullptr;

    virtual bool CalculateRenderTargetSizeImpl(uint32& InOutSizeX, uint32& InOutSizeY, float screenScale) override
    {
        if (InitializeImpl())
        {
            // Should we create a RenderParams?
            OSVR_ReturnCode rc;

            rc = osvrRenderManagerGetDefaultRenderParams(&mRenderParams);
            check(rc == OSVR_RETURN_SUCCESS);

            OSVR_RenderInfoCount numRenderInfo;
            rc = osvrRenderManagerGetNumRenderInfo(mRenderManager, mRenderParams, &numRenderInfo);
            check(rc == OSVR_RETURN_SUCCESS);

            mRenderInfos.Empty();
            for (size_t i = 0; i < numRenderInfo; i++)
            {
                OSVR_RenderInfoD3D11 renderInfo;
                rc = osvrRenderManagerGetRenderInfoD3D11(mRenderManagerD3D11, i, mRenderParams, &renderInfo);
                check(rc == OSVR_RETURN_SUCCESS);

                mRenderInfos.Add(renderInfo);
            }

            // check some assumptions. Should all be the same height.
            check(mRenderInfos.Num() == 2);
            check(mRenderInfos[0].viewport.height == mRenderInfos[1].viewport.height);

            mRenderInfos[0].viewport.width = int(float(mRenderInfos[0].viewport.width) * screenScale);
            mRenderInfos[0].viewport.height = int(float(mRenderInfos[0].viewport.height) * screenScale);
            mRenderInfos[1].viewport.width = mRenderInfos[0].viewport.width;
            mRenderInfos[1].viewport.height = mRenderInfos[0].viewport.height;
            mRenderInfos[1].viewport.left = mRenderInfos[0].viewport.width;
            
            InOutSizeX = mRenderInfos[0].viewport.width + mRenderInfos[1].viewport.width;
            InOutSizeY = mRenderInfos[0].viewport.height;
            check(InOutSizeX != 0 && InOutSizeY != 0);
            return true;
        }
        return false;
    }

    virtual bool InitializeImpl() override
    {
        if (!IsInitialized())
        {
            auto graphicsLibrary = CreateGraphicsLibrary();
            auto graphicsLibraryName = GetGraphicsLibraryName();
            OSVR_ReturnCode rc;

            if (!mClientContext)
            {
                UE_LOG(FOSVRCustomPresentLog, Warning, TEXT("Can't initialize FOSVRCustomPresent without a valid client context"));
                return false;
            }

            rc = osvrCreateRenderManagerD3D11(mClientContext, TCHAR_TO_ANSI(*graphicsLibraryName), graphicsLibrary, &mRenderManager, &mRenderManagerD3D11);
            if (rc == OSVR_RETURN_FAILURE || !mRenderManager || !mRenderManagerD3D11)
            {
                UE_LOG(FOSVRCustomPresentLog, Warning, TEXT("osvrCreateRenderManagerD3D11 call failed, or returned numm renderManager/renderManagerD3D11 instances"));
                return false;
            }

            rc = osvrRenderManagerGetDoingOkay(mRenderManager);
            if (rc == OSVR_RETURN_FAILURE)
            {
                UE_LOG(FOSVRCustomPresentLog, Warning, TEXT("osvrRenderManagerGetDoingOkay call failed. Perhaps there was an error during initialization?"));
                return false;
            }

            OSVR_OpenResultsD3D11 results;
            rc = osvrRenderManagerOpenDisplayD3D11(mRenderManagerD3D11, &results);
            if (rc == OSVR_RETURN_FAILURE || results.status == OSVR_OPEN_STATUS_FAILURE)
            {
                UE_LOG(FOSVRCustomPresentLog, Warning,
                    TEXT("osvrRenderManagerOpenDisplayD3D11 call failed, or the result status was OSVR_OPEN_STATUS_FAILURE. Potential causes could be that the display is already open in direct mode with another app, or the display does not support direct mode"));
                return false;
            }

            // @todo: create the textures?

            bInitialized = true;
        }
        return true;
    }

    virtual void FinishRendering() override
    {
        check(IsInitialized());
        UpdateRenderBuffers();
        // all of the render manager samples keep the flipY at the default false,
        // for both OpenGL and DirectX. Is this even needed anymore?
        OSVR_ReturnCode rc;
        OSVR_RenderManagerPresentState presentState;
        rc = osvrRenderManagerStartPresentRenderBuffers(&presentState);
        check(rc == OSVR_RETURN_SUCCESS);
        check(mRenderBuffers.Num() == mRenderInfos.Num() && mRenderBuffers.Num() == mViewportDescriptions.Num());
        for (int32 i = 0; i < mRenderBuffers.Num(); i++)
        {
            rc = osvrRenderManagerPresentRenderBufferD3D11(presentState, mRenderBuffers[i], mRenderInfos[i], mViewportDescriptions[i]);
            check(rc == OSVR_RETURN_SUCCESS);
        }
        rc = osvrRenderManagerFinishPresentRenderBuffers(mRenderManager, presentState, mRenderParams, ShouldFlipY() ? OSVR_TRUE : OSVR_FALSE);
        check(rc == OSVR_RETURN_SUCCESS);
    }

    void SetRenderTargetTexture(ID3D11Texture2D* renderTargetTexture)
    {
        if (RenderTargetTexture != nullptr && RenderTargetTexture != renderTargetTexture)
        {
            // @todo: testing if this is causing problems later on.
            //RenderTargetTexture->Release();
        }
        RenderTargetTexture = renderTargetTexture;
        RenderTargetTexture->AddRef();
    }

    virtual void UpdateRenderBuffers() override
    {
        HRESULT hr;

        check(IsInitialized());
        if (bRenderBuffersNeedToUpdate)
        {

            //check(mRenderTexture);
            //SetRenderTargetTexture(reinterpret_cast<ID3D11Texture2D*>(mRenderTexture->GetNativeResource()));
            check(RenderTargetTexture);

            D3D11_TEXTURE2D_DESC renderTextureDesc;
            RenderTargetTexture->GetDesc(&renderTextureDesc);

            auto graphicsDevice = GetGraphicsDevice();

            //D3D11_RENDER_TARGET_VIEW_DESC renderTargetViewDesc;
            //memset(&renderTargetViewDesc, 0, sizeof(renderTargetViewDesc));
            //// This must match what was created in the texture to be rendered
            ////renderTargetViewDesc.Format = renderTextureDesc.Format;
            //renderTargetViewDesc.Format = renderTextureDesc.Format;
            //renderTargetViewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
            //renderTargetViewDesc.Texture2D.MipSlice = 0;

            //// Create the render target view.
            //ID3D11RenderTargetView *renderTargetView; //< Pointer to our render target view
            //hr = graphicsDevice->CreateRenderTargetView(
            //    RenderTargetTexture, &renderTargetViewDesc, &renderTargetView);
            //check(!FAILED(hr));


            mRenderBuffers.Empty();

            // Adding two RenderBuffers, but they both point to the same D3D11 texture target
            for (int i = 0; i < 2; i++)
            {
                OSVR_RenderBufferD3D11 buffer;
                buffer.colorBuffer = RenderTargetTexture;
                //buffer.colorBufferView = renderTargetView;
                buffer.colorBufferView = RenderTargetView;
                //buffer.depthStencilBuffer = ???;
                //buffer.depthStencilView = ???;
                mRenderBuffers.Add(buffer);
            }

            // We need to register these new buffers.
            // @todo RegisterRenderBuffers doesn't do anything other than set a flag and crash
            // if you pass it a non-empty vector here. Passing it a dummy array for now.

            {
                OSVR_RenderManagerRegisterBufferState state;
                hr = osvrRenderManagerStartRegisterRenderBuffers(&state);
                check(hr == OSVR_RETURN_SUCCESS);

                for (int32 i = 0; i < mRenderBuffers.Num(); i++)
                {
                    hr = osvrRenderManagerRegisterRenderBufferD3D11(state, mRenderBuffers[i]);
                    check(hr == OSVR_RETURN_SUCCESS);
                }

                hr = osvrRenderManagerFinishRegisterRenderBuffers(mRenderManager, state, false);
                check(hr == OSVR_RETURN_SUCCESS);
            }

            // Now specify the viewports for each.
            mViewportDescriptions.Empty();

            OSVR_ViewportDescription leftEye, rightEye;

            leftEye.left = 0;
            leftEye.lower = 0;
            leftEye.width = 0.5;
            leftEye.height = 1.0;
            mViewportDescriptions.Add(leftEye);

            rightEye.left = 0.5;
            rightEye.lower = 0;
            rightEye.width = 0.5;
            rightEye.height = 1.0;
            mViewportDescriptions.Add(rightEye);

            bRenderBuffersNeedToUpdate = false;
        }
    }

    virtual OSVR_GraphicsLibraryD3D11 CreateGraphicsLibrary()
    {
        OSVR_GraphicsLibraryD3D11 ret;
        // Put the device and context into a structure to let RenderManager
        // know to use this one rather than creating its own.
        ret.device = GetGraphicsDevice();
        ID3D11DeviceContext *ctx = NULL;
        ret.device->GetImmediateContext(&ctx);
        check(ctx);
        ret.context = ctx;

        return ret;
    }

    virtual FString GetGraphicsLibraryName() override
    {
        return FString("Direct3D11");
    }

    virtual bool ShouldFlipY() override
    {
        return false;
    }
};

#endif // #if PLATFORM_WINDOWS