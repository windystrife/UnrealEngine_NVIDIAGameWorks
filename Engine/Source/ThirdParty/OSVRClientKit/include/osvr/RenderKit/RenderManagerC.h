/** @file
    @brief Header

    Must be c-safe!

    @date 2015

    @author
    Sensics, Inc.
    <http://sensics.com/osvr>
*/

/*
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
*/

#ifndef INCLUDED_RenderManagerC_h_GUID_F235863C_3572_439D_C8A0_D15AE74A22D8
#define INCLUDED_RenderManagerC_h_GUID_F235863C_3572_439D_C8A0_D15AE74A22D8

/* Internal Includes */
#include <osvr/RenderKit/Export.h>
#include <osvr/Util/APIBaseC.h>
#include <osvr/Util/ReturnCodesC.h>
#include <osvr/Util/StdInt.h>
#include <osvr/Util/ClientReportTypesC.h>
#include <osvr/Util/ClientOpaqueTypesC.h>
#include <osvr/Util/BoolC.h>

/* Library/third-party includes */
/* none */

/* Standard includes */
/* none */
#include <stdlib.h>

OSVR_EXTERN_C_BEGIN

// @todo These typedefs might be better off in a separate header.

// @todo implement proper opaque types?
typedef void* OSVR_RenderManager;
typedef void* OSVR_RenderManagerPresentState;
typedef void* OSVR_RenderManagerRegisterBufferState;
typedef void* OSVR_RenderInfoCollection;
typedef size_t OSVR_RenderInfoCount;

// @todo could we use this for the C++ API as well?
typedef struct OSVR_RenderParams {
    // not sure why the original struct had pointers here
    OSVR_PoseState* worldFromRoomAppend; //< Room space to insert
    OSVR_PoseState* roomFromHeadReplace; //< Overrides head space
    double nearClipDistanceMeters;
    double farClipDistanceMeters;
} OSVR_RenderParams;

//=========================================================================
/// Description needed to construct an off-axes projection matrix
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
typedef struct OSVR_ViewportDescription {
    double left;   //< Left side of the viewport in pixels
    double lower;  //< First pixel in the viewport at the bottom.
    double width;  //< Last pixel in the viewport at the top
    double height; //< Last pixel on the right of the viewport in pixels
} OSVR_ViewportDescription;

// ========================================================================
// Float representation of an rgb color (without alpha)
typedef struct OSVR_RGB {
    float r;
    float g;
    float b;
} OSVR_RGB_FLOAT;

typedef enum {
    OSVR_OPEN_STATUS_FAILURE,
    OSVR_OPEN_STATUS_PARTIAL,
    OSVR_OPEN_STATUS_COMPLETE
} OSVR_OpenStatus;

// @todo OSVR_RenderTimingInfo

OSVR_RENDERMANAGER_EXPORT OSVR_ReturnCode
osvrDestroyRenderManager(OSVR_RenderManager renderManager);

/// DEPRECATED - use the RenderInfoCollection API for your specific graphics API instead.
/// This function reads all of the rendering parameters from the
/// underlying RenderManager.  It caches this information locally
/// until the next call, and returns the number that it has cached.
/// Use the graphics-library-specific osvrRenderManagerGetRenderInfo
/// function (for example, osvrRenderManagerGetRenderInfoD3D11) to
/// read each entry that has been cached.
/// @brief Reads all of the RenderInfos and caches them.
/// @return Number of RenderInfos cached.
/// @todo Make this actually cache, for now it does not.
OSVR_RENDERMANAGER_EXPORT OSVR_ReturnCode osvrRenderManagerGetNumRenderInfo(
    OSVR_RenderManager renderManager, OSVR_RenderParams renderParams,
    OSVR_RenderInfoCount* numRenderInfoOut);

OSVR_RENDERMANAGER_EXPORT OSVR_ReturnCode
osvrRenderManagerGetDoingOkay(OSVR_RenderManager renderManager);

OSVR_RENDERMANAGER_EXPORT OSVR_ReturnCode
osvrRenderManagerGetDefaultRenderParams(OSVR_RenderParams* renderParamsOut);

/// This function is used to bracket the start of the presentation of render
/// buffers for a single frame.  Between this function and the associated
/// Finish call below, graphics-library-specific Present calls should be made
/// (for example, osvrRenderManagerPresentRenderBufferD3D11). All buffers
/// must be registered before they are presented.
OSVR_RENDERMANAGER_EXPORT OSVR_ReturnCode
osvrRenderManagerStartPresentRenderBuffers(
    OSVR_RenderManagerPresentState* presentStateOut);

/// This function is used to bracket the end of the presentation of render
/// buffers for a single frame.  This sequence starts with the Start function.
OSVR_RENDERMANAGER_EXPORT OSVR_ReturnCode
osvrRenderManagerFinishPresentRenderBuffers(
    OSVR_RenderManager renderManager,
    OSVR_RenderManagerPresentState presentState, OSVR_RenderParams renderParams,
    OSVR_CBool shouldFlipY);

/// This function is used to bracket the start of the registration of render
/// buffers.  Between this function and the associated Finish call below,
/// graphics-library-specific Register calls should be made
/// (for example, osvrRenderManagerRegisterRenderBufferD3D11). All buffers
/// must be registered before they are presented.
OSVR_RENDERMANAGER_EXPORT OSVR_ReturnCode
osvrRenderManagerStartRegisterRenderBuffers(
    OSVR_RenderManagerRegisterBufferState* registerBufferStateOut);

/// This function is used to bracket the end of the registration of render
/// buffers for a single frame.  This sequence starts with the Start function.
OSVR_RENDERMANAGER_EXPORT OSVR_ReturnCode
osvrRenderManagerFinishRegisterRenderBuffers(
    OSVR_RenderManager renderManager,
    OSVR_RenderManagerRegisterBufferState registerBufferState,
    OSVR_CBool appWillNotOverwriteBeforeNewPresent);

OSVR_RENDERMANAGER_EXPORT OSVR_ReturnCode
osvrRenderManagerPresentSolidColorf(
    OSVR_RenderManager renderManager,
    OSVR_RGB_FLOAT rgb);

/// This function gets all of the RenderInfo collection in one atomic call.
/// Use osvrRenderManagerGetNumRenderInfoInCollection to get the size of the
/// collection, and API-specific methods to get a given render info for that
/// graphics API. Finally, use osvrRenderManagerReleaseRenderInfoCollection
/// when you're done.
OSVR_RENDERMANAGER_EXPORT OSVR_ReturnCode
osvrRenderManagerGetRenderInfoCollection(
    OSVR_RenderManager renderManager,
    OSVR_RenderParams renderParams,
    OSVR_RenderInfoCollection* renderInfoCollectionOut);

/// Releases the OSVR_RenderInfoCollection.
OSVR_RENDERMANAGER_EXPORT OSVR_ReturnCode
osvrRenderManagerReleaseRenderInfoCollection(
    OSVR_RenderInfoCollection renderInfoCollection);

/// Get the size of the OSVR_RenderInfoCollection.
OSVR_RENDERMANAGER_EXPORT OSVR_ReturnCode
osvrRenderManagerGetNumRenderInfoInCollection(
    OSVR_RenderInfoCollection renderInfoCollection,
    OSVR_RenderInfoCount* countOut);


//=========================================================================
// Routines to turn the OSVR_PoseState into ModelView matrices for various
// rendering engines, including OpenGL, Direct3D, Unreal, and Unity.
// Done in such a way that we don't require the inclusion
// of the native API header files.

/// Produce an OpenGL ModelView matrix from an OSVR_PoseState.
/// Assumes that the world is described in a right-handed fashion and
/// that we're going to use a right-handed projection matrix.
/// @brief Produce OpenGL ModelView transform from OSVR_PoseState
/// @param state_in Input state from RenderManager.
/// @param OpenGL_out Pointer to 16-element double array that has
///        been allocated by the caller.
/// @return True on success, false on failure (null pointer).
OSVR_RENDERMANAGER_EXPORT OSVR_ReturnCode OSVR_PoseState_to_OpenGL(
  double* OpenGL_out, OSVR_PoseState state_in);

/// Produce a D3D ModelView matrix from an OSVR_PoseState.
/// Handles transitioning from the right-handed OSVR coordinate
/// system to the left-handed projection matrix that is typical
/// for D3D applications.
/// @brief Produce D3D ModelView transform from OSVR_PoseState
/// @param state_in Input state from RenderManager.
/// @param OpenGL_out Pointer to 16-element double array that has
///        been allocated by the caller.
/// @return True on success, false on failure (null pointer).
OSVR_RENDERMANAGER_EXPORT OSVR_ReturnCode OSVR_PoseState_to_D3D(
  float D3D_out[16], OSVR_PoseState state_in);

/// Modify the OSVR_PoseState from OSVR to be appropriate for use
/// in a Unity application.  OSVR's world is right handed, and Unity's
/// is left handed.
/// @brief Modify OSVR_PoseState for use by Unity.
/// @param state_in Input state from RenderManager.
/// @param state_out Ouput state for use by Unity
/// @return True on success, false on failure (null pointer).
OSVR_RENDERMANAGER_EXPORT OSVR_ReturnCode OSVR_PoseState_to_Unity(
  OSVR_PoseState* state_out, OSVR_PoseState state_in);

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
OSVR_RENDERMANAGER_EXPORT OSVR_ReturnCode OSVR_Projection_to_OpenGL(
  double* OpenGL_out, OSVR_ProjectionMatrix projection_in);

/// Produce a D3D Projection matrix from an OSVR_ProjectionMatrix.
/// Produces a left-handed projection matrix as is typical
/// for D3D applications.
/// @brief Produce D3D Projection transform from OSVR projection info
/// @param projection_in Input projection description from RenderManager.
/// @param D3D_out Pointer to 16-element float array that has
///        been allocated by the caller.
/// @return True on success, false on failure (null pointer).
OSVR_RENDERMANAGER_EXPORT OSVR_ReturnCode OSVR_Projection_to_D3D(
  float D3D_out[16], OSVR_ProjectionMatrix projection_in);

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
OSVR_RENDERMANAGER_EXPORT OSVR_ReturnCode OSVR_Projection_to_Unreal(
  float Unreal_out[16], OSVR_ProjectionMatrix projection_in);

OSVR_EXTERN_C_END

#endif
