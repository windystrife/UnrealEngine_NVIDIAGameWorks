/** @file
@brief Header file describing the OSVR graphics transformations interface

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

// Internal Includes
#include <osvr/RenderKit/Export.h>

// Library/third-party includes
#include <osvr/Util/ClientReportTypesC.h>

// Standard includes
#include <memory>

namespace osvr {
namespace renderkit {

    //=========================================================================
    /// Description needed to construct an off-axis projection matrix
    typedef struct {
        double left;
        double right;
        double top;
        double bottom;
        double nearClip; //< Cannot name "near" because Visual Studio keyword
        double farClip;
    } OSVR_ProjectionMatrix;

    //=========================================================================
    /// Viewport description with lower-left corner of the screen as (0,0)
    struct OSVR_ViewportDescription {
        double left;   //< Left side of the viewport in pixels
        double lower;  //< First pixel in the viewport at the bottom.
        double width;  //< Last pixel in the viewport at the top
        double height; //< Last pixel on the right of the viewport in pixels

        bool operator==(const OSVR_ViewportDescription& v) const {
            return (v.left == left) && (v.lower == lower) &&
                   (v.width == width) && (v.height == height);
        }

        bool operator!=(const OSVR_ViewportDescription& v) const {
            return (v.left != left) || (v.lower != lower) ||
                   (v.width != width) || (v.height != height);
        }
    };

    //=========================================================================
    // Routines to turn the OSVR_PoseState into ModelView matrices for OpenGL
    // and Direct3D.  Done in such a way that we don't require the inclusion
    // of the native API header files (since most apps will not include all
    // of the libraries).

    /// Produce an OpenGL ModelView matrix from an OSVR_PoseState.
    /// Assumes that the world is described in a right-handed fashion and
    /// that we're going to use a right-handed projection matrix.
    /// @brief Produce OpenGL ModelView transform from OSVR_PoseState
    /// @param state_in Input state from RenderManager.
    /// @param OpenGL_out Pointer to 16-element double array that has
    ///        been allocated by the caller.
    /// @return True on success, false on failure (null pointer).
    bool OSVR_RENDERMANAGER_EXPORT OSVR_PoseState_to_OpenGL(
        double* OpenGL_out, const OSVR_PoseState& state_in);

    /// Produce a D3D ModelView matrix from an OSVR_PoseState.
    /// Handles transitioning from the right-handed OSVR coordinate
    /// system to the left-handed projection matrix that is typical
    /// for D3D applications.
    /// @brief Produce D3D ModelView transform from OSVR_PoseState
    /// @param state_in Input state from RenderManager.
    /// @param OpenGL_out Pointer to 16-element double array that has
    ///        been allocated by the caller.
    /// @return True on success, false on failure (null pointer).
    bool OSVR_RENDERMANAGER_EXPORT
    OSVR_PoseState_to_D3D(float D3D_out[16], const OSVR_PoseState& state_in);

    /// Modify the OSVR_PoseState from OSVR to be appropriate for use
    /// in a Unity application.  OSVR's world is right handed, and Unity's
    /// is left handed.
    /// @brief Modify OSVR_PoseState for use by Unity.
    /// @param state_in Input state from RenderManager.
    /// @param state_out Ouput state for use by Unity
    /// @return True on success, false on failure (null pointer).
    bool OSVR_RENDERMANAGER_EXPORT
      OSVR_PoseState_to_Unity(OSVR_PoseState& state_out,
                              const OSVR_PoseState& state_in);

    //=========================================================================
    // Routines to turn the 4x4 projection matrices returned as part of the
    // RenderCallback class into Projection matrices for OpenGL and
    // Direct3D.  Done in such a way that we don't require the inclusion of the
    // native API header files (since most apps will not include all of the
    // libraries).

    /// Produce an OpenGL Projection matrix from an OSVR_ProjectionMatrix.
    /// Assumes that the world is described in a right-handed fashion and
    /// that we're going to use a right-handed projection matrix.
    /// @brief Produce OpenGL Projection matrix from OSVR projection info.
    /// @param projection_in Input projection description from RenderManager.
    /// @param OpenGL_out Pointer to 16-element double array that has
    ///        been allocated by the caller.
    /// @return True on success, false on failure (null pointer).
    bool OSVR_RENDERMANAGER_EXPORT OSVR_Projection_to_OpenGL(
        double* OpenGL_out, const OSVR_ProjectionMatrix& projection_in);

    /// Produce a D3D Projection matrix from an OSVR_ProjectionMatrix.
    /// Produces a left-handed projection matrix as is typical
    /// for D3D applications.
    /// @brief Produce D3D Projection transform from OSVR projection info
    /// @param projection_in Input projection description from RenderManager.
    /// @param D3D_out Pointer to 16-element float array that has
    ///        been allocated by the caller.
    /// @return True on success, false on failure (null pointer).
    bool OSVR_RENDERMANAGER_EXPORT OSVR_Projection_to_D3D(
        float D3D_out[16], const OSVR_ProjectionMatrix& projection_in);

    /// Produce an Unreal Projection matrix from an OSVR_ProjectionMatrix.
    /// Produces a left-handed projection matrix whose Z values are
    /// in the opposite order, with Z=0 at the far clipping plane and
    /// Z=1 at the near clipping plane.  If there is not a far clipping
    /// plane defined, then set it to be the same as the near
    /// clipping plane before calling this function.  If there is not a
    /// near clipping plane set, then set it to 1 before calling this
    /// function.
    /// To put the result into an Unreal FMatrix, do the following:
    ///   float p[16];
    ///   OSVR_Projection_to_D3D(p, projection_in);
    ///   FPlane row1(p[0], p[1], p[2], p[3]);
    ///   FPlane row2(p[4], p[5], p[6], p[7]);
    ///   FPlane row3(p[8], p[9], p[10], p[11]);
    ///   FPlane row4(p[12], p[13], p[14], p[15]);
    ///   FMatrix ret = FMatrix(row1, row2, row3, row4);
    /// @brief Produce Unreal Projection transform from OSVR projection info
    /// @param projection_in Input projection description from RenderManager.
    /// @param Unreal_out Pointer to 16-element float array that has
    ///        been allocated by the caller.
    /// @return True on success, false on failure (null pointer).
    bool OSVR_RENDERMANAGER_EXPORT OSVR_Projection_to_Unreal(
      float Unreal_out[16], const OSVR_ProjectionMatrix& projection_in);

} // namespace renderkit
} // namespace osvr
