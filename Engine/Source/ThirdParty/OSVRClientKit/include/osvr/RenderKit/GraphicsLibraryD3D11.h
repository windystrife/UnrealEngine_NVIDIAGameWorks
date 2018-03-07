/** @file
@brief Header file describing the OSVR D3D11 graphics library callback info

@date 2015

@author
Russ Taylor <russ@sensics.com>
<http://sensics.com/osvr>
*/

// Copyright 2015 Sensics, Inc.
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

#pragma once

// Required for DLL linkage on Windows
#include <osvr/RenderKit/Export.h>

namespace osvr {
namespace renderkit {

    /// @brief Describes a D3D11 rendering library being used
    ///
    /// This is one of the members of the GraphicsLibrary union
    /// from RenderManager.h.  It stores the information needed
    /// for a render callback handler in an application using D3D11
    /// as its renderer.  It is in a separate include file so
    /// that only code that actually uses this needs to
    /// include it.  NOTE: You must #include <d3d11.h> before
    /// including this file.

    class GraphicsLibraryD3D11 {
      public:
        ID3D11Device* device;
        ID3D11DeviceContext* context;
    };

    /// @brief Describes a D3D11 texture to be rendered
    ///
    /// This is one of the members of the RenderBuffer union
    /// from RenderManager.h.  It stores the information needed
    /// for a D3D11 texture.
    /// NOTE: You must #include <d3d11.h> before
    /// including this file.

    class RenderBufferD3D11 {
      public:
        ID3D11Texture2D* colorBuffer;
        ID3D11RenderTargetView* colorBufferView;
        ID3D11Texture2D* depthStencilBuffer;
        ID3D11DepthStencilView* depthStencilView;
    };

} // namespace renderkit
} // namespace osvr
