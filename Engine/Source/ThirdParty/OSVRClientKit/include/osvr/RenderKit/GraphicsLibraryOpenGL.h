/** @file
@brief Header file describing the OSVR OpenGL rendering library callback info

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

namespace osvr {
namespace renderkit {

    /// @brief Describes the OpenGL rendering library being used
    ///
    /// This is one of the members of the GraphicsLibrary union
    /// from RenderManager.h.  It stores the information needed
    /// for a render callback handler in an application using OpenGL
    /// as its renderer.  It is in a separate include file so
    /// that only code that actually uses this needs to
    /// include it.  NOTE: You must #include <GL/gl.h> or
    /// and SDL or other version of GLES include file before
    /// including this file.

    class GraphicsLibraryOpenGL {
      public:
        /// Handling the case of multiple OpenGL contexts
        /// in the client, with the one to be shared not the current
        /// one at the time the display is opened, is going to be
        /// challenging to do in a cross-platform manner, because
        /// we need a context that depends on the library used to
        /// open it (SDL, native interface, ...).
        ///  This flag tells whether to share the OpenGL context
        /// that is open when the library is opened.  If this is
        /// true, the application should open a context and make it
        /// current before calling OpenDisplay() and then again make
        /// sure it is current before calling any RenderManager
        /// method.
        bool shareOpenGLContext = false;
    };

    /// @brief Describes a OpenGL textures to be rendered
    ///
    /// This is one of the members of the RenderBuffer union
    /// from RenderManager.h.  It stores the information needed
    /// for an OpenGL texture.
    /// NOTE: You must #include <GL/gl.h> or
    /// and SDL or other version of GLES include file before
    /// including this file.

    class RenderBufferOpenGL {
      public:
        GLuint colorBufferName;
        GLuint depthStencilBufferName;
    };

} // namespace renderkit
} // namespace osvr
