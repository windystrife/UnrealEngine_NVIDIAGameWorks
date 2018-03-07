/** @file
    @brief Header

    @date 2015

    @author
    Sensics, Inc.
    <http://sensics.com/osvr>
*/

// Copyright 2015 Sensics, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef INCLUDED_RenderManagerD3D11C_h_GUID_F034D624_0A3E_4D9E_D7DC_9D36CEECB0CE
#define INCLUDED_RenderManagerD3D11C_h_GUID_F034D624_0A3E_4D9E_D7DC_9D36CEECB0CE

// Internal Includes
#include <osvr/RenderKit/RenderManagerC.h>

// Library/third-party includes
#include <d3d11.h>

// Standard includes
// - none

OSVR_EXTERN_C_BEGIN

typedef void* OSVR_RenderManagerD3D11;

typedef struct OSVR_GraphicsLibraryD3D11 {
    ID3D11Device* device;
    ID3D11DeviceContext* context;
} OSVR_GraphicsLibraryD3D11;

// Could possibly re-use this for the C++ API
typedef struct OSVR_RenderInfoD3D11 {
    OSVR_GraphicsLibraryD3D11 library;
    OSVR_ViewportDescription viewport;
    OSVR_PoseState pose;
    OSVR_ProjectionMatrix projection;
} OSVR_RenderInfoD3D11;

typedef struct OSVR_RenderBufferD3D11 {
    ID3D11Texture2D* colorBuffer;
    ID3D11RenderTargetView* colorBufferView;
    ID3D11Texture2D* depthStencilBuffer;
    ID3D11DepthStencilView* depthStencilView;
} OSVR_RenderBufferD3D11;

typedef struct OSVR_OpenResultsD3D11 {
    OSVR_OpenStatus status;
    OSVR_GraphicsLibraryD3D11 library;
} OSVR_OpenResultsD3D11;

OSVR_RENDERMANAGER_EXPORT OSVR_ReturnCode osvrCreateRenderManagerD3D11(
    OSVR_ClientContext clientContext, const char graphicsLibraryName[],
    OSVR_GraphicsLibraryD3D11 graphicsLibrary,
    OSVR_RenderManager* renderManagerOut,
    OSVR_RenderManagerD3D11* renderManagerD3D11Out);

/**  DEPRECATED, use the collection render info API instead. */
OSVR_RENDERMANAGER_EXPORT OSVR_ReturnCode osvrRenderManagerGetRenderInfoD3D11(
    OSVR_RenderManagerD3D11 renderManager, OSVR_RenderInfoCount renderInfoIndex,
    OSVR_RenderParams renderParams, OSVR_RenderInfoD3D11* renderInfoOut);

OSVR_RENDERMANAGER_EXPORT OSVR_ReturnCode
osvrRenderManagerOpenDisplayD3D11(OSVR_RenderManagerD3D11 renderManager,
                                  OSVR_OpenResultsD3D11* openResultsOut);

/// This function must be bracketed by osvrRenderManagerStartPresentRenderBuffers
/// and osvrRenderManagerFinishPresentRenderBuffers.
/// All buffers must be registered before they are presented.
OSVR_RENDERMANAGER_EXPORT OSVR_ReturnCode
osvrRenderManagerPresentRenderBufferD3D11(
    OSVR_RenderManagerPresentState presentState, OSVR_RenderBufferD3D11 buffer,
    OSVR_RenderInfoD3D11 renderInfoUsed,
    OSVR_ViewportDescription normalizedCroppingViewport);

/// This function must be bracketed by osvrRenderManagerStartRegisterRenderBuffers
/// and osvrRenderManagerFinishPresentRegisterBuffers.
OSVR_RENDERMANAGER_EXPORT OSVR_ReturnCode
osvrRenderManagerRegisterRenderBufferD3D11(
    OSVR_RenderManagerRegisterBufferState registerBufferState,
    OSVR_RenderBufferD3D11 renderBuffer);

/// Gets an OSVR_RenderInfoD3D11 from a given OSVR_RenderInfoCollection.
OSVR_RENDERMANAGER_EXPORT OSVR_ReturnCode osvrRenderManagerGetRenderInfoFromCollectionD3D11(
    OSVR_RenderInfoCollection renderInfoCollection,
    OSVR_RenderInfoCount index,
    OSVR_RenderInfoD3D11* renderInfoOut);

OSVR_EXTERN_C_END

#endif // INCLUDED_RenderManagerD3D11C_h_GUID_F034D624_0A3E_4D9E_D7DC_9D36CEECB0CE
