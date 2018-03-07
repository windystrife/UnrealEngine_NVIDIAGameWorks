/** @file
@brief Header file describing the OSVR direct-to-device rendering interface

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
#include "osvr_display_configuration.h"
#include "RenderKitGraphicsTransforms.h"
#include "DistortionParameters.h"
#include "UnstructuredMeshInterpolator.h"
#include "Float2.h"
#include "DistortionMesh.h"

// Library/third-party includes
#include <osvr/ClientKit/ContextC.h>
#include <osvr/ClientKit/InterfaceC.h>
#include <osvr/Util/TimeValueC.h>

// Standard includes
#include <vector>
#include <string>
#include <memory>
#include <mutex>
#include <array>

namespace osvr {
namespace renderkit {

    //=========================================================================
    // Handles optimizing rendering given a description of the desired rendering
    // style and set of callback routines to handle rendering in various spaces.
    // It also has a get/present interface that enables the render buffer
    // generation to be handled by the client.
    // The base class describes the interface.  There is a derived class for
    // each available rendering system.  This class handles getting an
    // appropriate window to render in, setting up the graphics systems, getting
    // new data from all trackers in the OSVR context just in time to use for
    // rendering.
    // NOTE: You use the factory function below to create an actual instance.

    /// @brief Information about the rendering system, passed to client callback
    ///
    /// Structure that holds information about the rendering
    /// system that may be needed by the client callbacks.
    /// Because OSVR supports multiple rendering libraries,
    /// the client will need to select the appropriate entry
    /// and also #include the appropriate file that describes the class.
    class GraphicsLibraryD3D11;
    class GraphicsLibraryOpenGL;
    class GraphicsLibrary {
      public:
        GraphicsLibraryD3D11* D3D11 =
            nullptr; //< #include <osvr/RenderKit/GraphicsLibraryD3D11.h>
        GraphicsLibraryOpenGL* OpenGL =
            nullptr; //< #include <osvr/RenderKit/GraphicsLibraryOpenGL.h>
    };

    /// @brief Used to pass Render Texture targets to be rendered
    ///
    /// Structure that holds a pointer to the information needed
    /// to render from texture for each supported rendering library.
    /// Because OSVR supports multiple rendering libraries,
    /// the client will need to select the appropriate entry
    /// from the union and also #include the appropriate
    /// file that describes the class.
    class RenderBufferD3D11;
    class RenderBufferOpenGL;
    class RenderBuffer {
      public:
        OSVR_RENDERMANAGER_EXPORT RenderBuffer() {
            D3D11 = nullptr;
            OpenGL = nullptr;
        }

        RenderBufferD3D11*
            D3D11; //< #include <osvr/RenderKit/GraphicsLibraryD3D11.h>
        RenderBufferOpenGL*
            OpenGL; //< #include <osvr/RenderKit/GraphicsLibraryOpenGL.h>
    };

    /// @brief Returns timing information about the rendering system
    ///
    /// Structure that holds timing information about the system.
    /// Each of these times will have the value (0,0) if they are
    /// not available from a particular RenderManager.
    /// @todo Move these into a C API file
    typedef struct {
        OSVR_TimeValue
            hardwareDisplayInterval; //< Time between refresh of display device
        OSVR_TimeValue timeSincelastVerticalRetrace; //< Time since the last
        // retrace ended (the last presentation)
        OSVR_TimeValue timeUntilNextPresentRequired; //< How long until must
        // send images to RenderManager to display before the next frame is
        // presented.
    } RenderTimingInfo;

    /// @brief Simple structure for representing a float based RGB color
    typedef struct {
        float r;
        float g;
        float b;
    } RGBColorf;

    /// @brief Describes the parameters for a display callback handler.
    ///
    /// Description of the type of a Display callback handler.  The user defines
    /// functions of this type and the user-data passed in when the callback
    /// is added is passed to the function.
    ///  NOTE: Because OSVR supports multiple graphics libraries, the
    /// client will need select the appropriate entry from the union.
    typedef void (*DisplayCallback)(
        void* userData //< Passed into SetDisplayCallback
        ,
        GraphicsLibrary library //< Graphics library context to use
        ,
        RenderBuffer buffers //< Information on buffers to render to
        );

    /// @brief Describes the parameters for a view/projection callback handler.
    ///
    /// Description of the type of a ViewProjection callback handler.  The user
    /// defines functions of this type and the user-data passed in when the
    /// callback is added is passed to the function.  The viewport and
    /// projection matrices will be configured to draw things for the current
    /// viewpoint; the RenderManager will call the function as many times as
    /// needed (once per eye).  The world should not be changed between
    /// callbacks, to prevent misalignment between the eyes.
    ///  The viewport and projection matrix are passed in in case the user is
    /// using a custom vertex or shader that needs this information.  Both will
    /// have already been set in graphics libraries that support this.
    ///  NOTE: Because OSVR supports multiple graphics libraries, the
    /// client will need select the appropriate entry from the union.
    typedef void (*ViewProjectionCallback)(
        void* userData //< Passed into SetViewProjectionCallback
        ,
        GraphicsLibrary library //< Graphics library context to use
        ,
        RenderBuffer buffers //< Information on buffers to render to
        ,
        OSVR_ViewportDescription viewport //< Viewport set by RenderManager
        ,
        OSVR_ProjectionMatrix
            projection //< Projection matrix set by RenderManager
        ,
        size_t whichEye //< Which eye are we setting up for?
        );

    /// @brief Describes the parameters for a render callback handler.
    ///
    /// Description of the type of a render callback handler.  The user defines
    /// functions of this type and the user-data passed in when the callback
    /// is added is passed to the function so it can know what to draw.  The
    /// Modelview and projection matrices will be configured to draw things
    /// in the specified space for the current viewpoint; the RenderManager will
    /// call the function as many times as needed.  The world should not be
    /// changed between callbacks, to prevent misalignment between the eyes.
    /// The time at which the scene should be rendered is specified to help
    /// ensure synchrony.  The pose used to define the ModelView transformation
    /// is passed in in case the user is using a custom vertex or shader that
    /// needs this information.  The 4x4 projection matrix is passed in as
    /// well, but will also have already been set.
    ///  NOTE: Because OSVR supports multiple graphics libraries, the
    /// client will need select the appropriate entry from the union.
    typedef void (*RenderCallback)(
        void* userData //< Passed into AddRenderCallback
        ,
        GraphicsLibrary library //< Graphics library context to use
        ,
        RenderBuffer buffers //< Information on buffers to render to
        ,
        OSVR_ViewportDescription viewport //< Viewport we're rendering into
        ,
        OSVR_PoseState pose //< OSVR ModelView matrix set by RenderManager
        ,
        OSVR_ProjectionMatrix
            projection //< Projection matrix set by RenderManager
        ,
        OSVR_TimeValue deadline //< When the frame should be sent to the screen
        );

    /// @brief Describes the parameters needed to render to an eye.
    ///
    /// Description of what is needed to construct and fill in a
    /// RenderTexture.  A vector of these is passed back to describe
    /// all of the needed renderings for a given frame.
    /// NOTE: The fields here should be the same as those in the
    /// RenderCallback above.
    /// Modelview and projection matrices will be configured to draw things
    /// in the specified space for the current viewpoint; the RenderManager will
    /// call the function as many times as needed.  The world should not be
    /// changed between callbacks, to prevent misalignment between the eyes.
    /// The time at which the scene should be rendered is specified to help
    /// ensure synchrony.  The pose used to define the ModelView transformation
    /// is passed in in case the user is using a custom vertex or shader that
    /// needs this information.  The 4x4 projection matrix is passed in as
    /// well, but will also have already been set.
    ///  NOTE: Because OSVR supports multiple graphics libraries, the
    /// client will need select the appropriate entry from the union.
    /// @todo Use this struct as the return info in the callback above,
    /// and modify examples to go one level deeper to get the info.
    typedef struct {
        GraphicsLibrary library; //< Graphics library context to use
        OSVR_ViewportDescription
            viewport;        //< Viewport to render into (will start at 0,0)
        OSVR_PoseState pose; //< OSVR ModelView matrix set by RenderManager
        OSVR_ProjectionMatrix
            projection; //< Projection matrix set by RenderManager
    } RenderInfo;

    class RenderManager {
      public:
        ///-------------------------------------------------------------
        /// Create a RenderManager using the createRenderManager() function.

        /// Virtual destructor to let the derived classes shut down
        /// correctly.
        virtual OSVR_RENDERMANAGER_EXPORT ~RenderManager();

        /// Is the renderer currently working?
        virtual bool OSVR_RENDERMANAGER_EXPORT doingOkay() = 0;

        ///-------------------------------------------------------------
        /// @brief Did we get all we asked for, some of it, or nothing
        typedef enum { FAILURE, PARTIAL, COMPLETE } OpenStatus;
        /// @brief Return type from OpenResults() method
        typedef struct {
            OpenStatus status;       //< How did the opening go?
            GraphicsLibrary library; //< Graphics library pointers
        } OpenResults;

        /// @brief Opens the window or display to be used for rendering.
        ///
        /// Opens the rendering system, configuring it according to the
        /// requested parameters.  Returns a description of whether it was
        /// able to get the desired configuration.  Implemented by each
        /// concrete derived class.
        virtual OSVR_RENDERMANAGER_EXPORT OpenResults OpenDisplay() = 0;

        ///-------------------------------------------------------------
        /// @brief Setup callback for a given display
        ///
        /// Set the callback function to handle configuring the display
        /// (mainly, clearing it).  The userdata pointer
        /// will be handed to the callback function.
        bool OSVR_RENDERMANAGER_EXPORT SetDisplayCallback(
            DisplayCallback callback //< Function to call to set this viewport
            ,
            void* userData = nullptr //< Passed to callback function
            );

        ///-------------------------------------------------------------
        /// @brief Set viewport/projection callback for a given eye.
        ///
        /// Set the callback function to handle configuring the viewport and
        /// projection matrix.  The userdata pointer will be handed to the
        /// callback function.
        /// The RenderManager will construct callbacks from the device and
        /// will have set the viewport and projection transform in libraries
        /// where this is available.
        bool OSVR_RENDERMANAGER_EXPORT SetViewProjectionCallback(
            ViewProjectionCallback
                callback //< Function to call to set this viewport
            ,
            void* userData = nullptr //< Passed to callback function
            );

        ///-------------------------------------------------------------
        /// @brief Add render callback for a given space.
        ///
        /// Add a callback function to handle rendering objects in the
        /// specified tracker interface.  This provides a way for the
        /// application to easily draw things in hand space, or head space, or
        /// any other space configured on the server.  An empty string means
        /// "world space", which is the root of the hierarchy.  The userdata
        /// pointer will be handed to the callback function.
        /// The RenderManager will construct callbacks from the interfaces and
        /// maintain the poses of the objects; the callback handler will already
        /// have the correct space configured in the rendering engine.
        bool OSVR_RENDERMANAGER_EXPORT AddRenderCallback(
            const std::string&
                interfaceName //< Name of the space, or "/" for world
            ,
            RenderCallback callback //< Function to call to render this space
            ,
            void* userData = nullptr //< Passed to callback function
            );
        /// @brief Remove a previously-added callback handler for a given space.
        bool OSVR_RENDERMANAGER_EXPORT RemoveRenderCallback(
            const std::string& interfaceName //< Name given to AddRenderCallback
            ,
            RenderCallback
                callback //< Function pointer given to AddRenderCallback
            ,
            void* userData = nullptr //< Pointer given to AddRenderCallback
            );

        ///-------------------------------------------------------------
        /// @brief Parameters passed to Render() method
        ///
        /// Required and optional parameters to the Render method.
        ///   Specify a pointer to the room-space transform to be inserted
        /// between the OSVR native head-space tree and the world space; this
        /// will adjust the user's position within the world and can be used
        /// to rotate them, change height, or make them follow objects or
        /// physics in the world.
        ///   To override the viewpoint all the way up to the head (halfway
        /// between the eyes, with X pointing from the left eye to the right
        /// and Z pointing towards the back of the head, right-handed), send
        /// a pointer to a head transform.  This does not override any
        /// room transform, but will have the room appended.
        class RenderParams {
          public:
            RenderParams(){};
            const OSVR_PoseState* worldFromRoomAppend =
                nullptr; //< Room space to insert
            const OSVR_PoseState* roomFromHeadReplace =
                nullptr; //< Overrides head space
            double nearClipDistanceMeters = 0.1;
            double farClipDistanceMeters = 100.0;
            double IPDMeters = 0.063; //< Inter-puppilary distance of the viewer
        };

        /// @brief Render the scene with minimum latency.
        ///
        /// Uses the various Render* functions below to call subclasses.
        /// NOTE: Use only one of Render() or
        /// GetRenderInfo()/PresentRenderBuffers(),
        /// not both.  They are separate interfaces.
        /// NOTE: Most subclasses will NOT override this function.
        /// Sets up the viewport and projection matrices
        /// for each eye and then starts rendering in that eye.  Within
        /// each eye, sets up a modelview matrix for each space we have
        /// callbacks for and then renders to that space.  All of the
        /// actual work is done by virtual functions defined in the
        /// subclass; this handles the machinery of figuring out how to
        /// handle arbitrary numbers of eyes and their layouts.
        /// @return True on success, False for failure during rendering.
        /// May also return False if the rendering library asks us to
        /// quit the application due to a window-system event.
        virtual bool OSVR_RENDERMANAGER_EXPORT
        Render(const RenderParams& params = RenderParams());

        ///-------------------------------------------------------------
        /// @brief Gets vector of parameters needed to render all eyes and
        /// displays
        ///
        /// Returns the information needed to construct and fill texture
        /// buffers to render into for all eyes.  This should be called
        /// each render loop right when the application is ready to render.
        /// The information presented here should be used to construct and
        /// fill in texture buffers, which will then be passed to the
        /// RegisterRenderBuffers() once before they are rendered into and then
        /// to the PresentRenderBuffers() function each frame to display them.
        ///   Uses the various Render* functions below to call subclasses.
        ///  NOTE: Use only one of Render() or
        ///  GetRenderInfo()/PresentRenderBuffers(),
        /// not both.  They are separate interfaces.
        ///  NOTE: Most subclasses will NOT override this function.
        ///  NOTE: If using the GetRenderInfo()/PresentRenderBuffers()
        /// code path, callbacks will not be called and all of the
        /// modelview matrices will be in world space; the client is
        /// responsible for converting hand space and such into the
        /// correct coordinate system.
        ///  @return Returns an empty vector on failure.
        inline std::vector<RenderInfo> OSVR_RENDERMANAGER_EXPORT
        GetRenderInfo(const RenderParams& params = RenderParams()) {
            std::vector<RenderInfo> ret;
            size_t num = LatchRenderInfo(params);
            for (size_t i = 0; i < num; i++) {
                ret.push_back(GetRenderInfo(i));
            }
            return ret;
        }

        /// @brief Registers texture buffers to be used to render all eyes and
        /// displays.
        ///
        /// Before a vector of RenderBuffers can be used for rendering, they
        /// must be registered.  This call only needs to be made once for a
        /// given set of buffers, even if the buffers are re-used to render
        /// multiple frames.  The buffers must be registered before they are
        /// rendered into by the client code.
        ///  NOTE: Because OSVR supports multiple graphics libraries, the
        /// client will need fill in the appropriate entry from the union.
        ///  @param[in] buffers The vector of buffers that will be presented,
        /// one per entry in the RenderInfo vector received from
        /// GetRenderInfo().
        /// If more than one image is packed into the same buffer (for example,
        /// if the application allocates a double-sized buffer for two eyes
        /// rather than a single buffer per eye), then the buffer only needs to
        /// be registered once.  It is not an error to register the same buffer
        /// multiple times.
        ///  @param[in] appWillNotOverwriteBeforeNewPresent This flag specifies
        /// that the application is double-buffering, registering twice as many
        /// buffers as needed.  It presents one set of buffers and then writes
        /// into the others, presenting half of them each frame.  It promises
        /// not to write to a presented buffer until another buffer has been
        /// presented in its place.  This enables the optimization of
        /// RenderManager not making a copy of the buffers during Asynchronous
        /// Time Warp.  NOTE: If this flag is set to true, the buffers must
        /// be presented as two groups, the first half being the buffers for
        /// all eyes in a single frame and the second half being the buffers
        /// to be used for all eyes in the next frame.
        ///
        ///  @return Returns true on success and false on failure.
        bool OSVR_RENDERMANAGER_EXPORT
        RegisterRenderBuffers(const std::vector<RenderBuffer>& buffers,
                              bool appWillNotOverwriteBeforeNewPresent = false);

        /// @brief Sends texture buffers needed to render all eyes and displays.
        ///
        /// Sends a set of textures appropriate for the rendering engine being
        /// used.
        /// Uses the various Render* functions below to call subclasses.
        ///  NOTE: Use only one of Render() or
        ///  GetRenderInfo()/PresentRenderBuffers(), not both.  They are
        ///  separate interfaces.
        ///  NOTE: Most subclasses will NOT override this function.
        ///  NOTE: Because OSVR supports multiple graphics libraries, the
        /// client will need fill in the appropriate entry from the union.
        ///  NOTE: If using the GetRenderInfo()/PresentRenderBuffers()
        /// code path, callbacks will not be called and all of the
        /// modelview matrices will be in world space; the client is
        /// responsible for converting hand space and such into the
        /// correct coordinate system.
        ///  @param[in] buffers The vector of buffers to present, one per
        /// entry in the RenderInfo vector received from GetRenderInfo().
        /// If the same buffer is used for more than one image, it should be
        /// passed in each time it is used, and the viewports vector should
        /// be filled in to locate each eye within the buffer.
        ///  @param[in] renderInfoUsed The rendering info used by the
        /// application to produce the images in the buffers.  This is
        /// needed to support time warp.
        ///  @param[in] renderParams The renderParams passed to GetRenderInfo()
        /// that produced the renderInfoUsed data.  This is needed so that
        /// we can reproduce any world-to-room transforms that were in place
        /// when the data was constructed.
        ///  @param[in] viewports An optional vector of viewports describing
        /// where in the buffers to look for each eye.  This is useful in the
        /// case where an application has decided to map multiple eyes into
        /// the same larger texture, drawing them all into different parts of
        /// it.  In this case, it needs to specify which part is for each eye.
        /// By default, the full buffer is used for rendering.  There should
        /// either be one of these per surface to be rendered or none.  They
        /// are in the space (0,0) to (1,1) with the origin at the lower-left
        /// corner.  Two side-by-side images should have the first one use
        /// left=lower=0 and the second should use left=0.5, lower = 0; both
        /// should use width=0.5, height = 1.
        ///  @param[in] flipInY Should we flip all of the buffers over in
        /// Y before presentation (this is helpful for OpenGL/Direct3D
        /// interop, where the buffer coordinates are different).
        ///  @return Returns true on success and false on failure.
        bool OSVR_RENDERMANAGER_EXPORT
        PresentRenderBuffers(const std::vector<RenderBuffer>& buffers,
                             const std::vector<RenderInfo>& renderInfoUsed,
                             const RenderParams& renderParams = RenderParams(),
                             const std::vector<OSVR_ViewportDescription>&
                                 normalizedCroppingViewports =
                                     std::vector<OSVR_ViewportDescription>(),
                             bool flipInY = false);

        /// @brief Sends solid color to all eyes and displays.
        ///
        /// Sends a solid color to be displayed on every eye and
        /// display.
        /// Uses the various Render* functions below to call subclasses.
        ///  NOTE: Most subclasses will NOT override this function.
        ///  @param[in] color The colors to present.
        ///  @return Returns true on success and false on failure.
        bool OSVR_RENDERMANAGER_EXPORT
          PresentSolidColor(const RGBColorf &color);

        ///-------------------------------------------------------------
        /// @brief Get rendering-time statistics
        ///
        /// Provides statistics about when the last frame finished, what
        /// the hardware's frame rate is, what the rendering frame rate is.
        /// This is provided to help the application know when it should
        /// reduce the scene complexity to meet the target render time.
        /// @return False on failure, true and filled-in timing information on
        /// success.
        /// NOTE: Every derived class that can fill in this information should
        /// override this function and do so, returning true.
        virtual bool OSVR_RENDERMANAGER_EXPORT GetTimingInfo(
            size_t whichEye //!< Each eye has a potentially different timing
            ,
            RenderTimingInfo& info //!< Info that is returned
            ) {
            return false;
        }

        ///-------------------------------------------------------------
        /// Values that control how we do our rendering.  Some RenderManager
        /// subclasses implement only a subset of the techniques that can be
        /// specified.
        class ConstructorParameters {
          public:
            /// Fill in defaults for the parameters.
            OSVR_RENDERMANAGER_EXPORT ConstructorParameters() {
                m_directMode = false;
                m_directModeIndex = 0;
                m_directDisplayIndex = 0;
                m_directHighPriority = false;
                m_displayRotation = Zero;
                m_numBuffers = 2;
                m_verticalSync = true;
                m_verticalSyncBlocksRendering = false;
                m_renderLibrary = ""; ///< Unspecified, which is invalid.

                m_windowTitle = "OSVR";
                m_windowFullScreen = false;
                m_windowXPosition = 0;
                m_windowYPosition = 0;
                m_bitsPerColor = 8;

                m_renderOverfillFactor = 1.0f;
                m_renderOversampleFactor = 1.0f;
                m_enableTimeWarp = true;
                m_asynchronousTimeWarp = false;
                m_maxMSBeforeVsyncTimeWarp = 3.0f;

                m_distortionCorrection = false;

                m_clientPredictionEnabled = false;
                m_clientPredictionLocalTimeOverride = false;

                m_graphicsLibrary = GraphicsLibrary();
            }
            typedef enum {
                Zero,
                Ninety,
                OneEighty,
                TwoSeventy
            } Display_Rotation;

            bool m_directMode; //< Should we render using DirectMode?

            void addCandidatePNPID(const char* pnpid);
            std::vector<uint32_t>
                m_directVendorIds; //< Vendor IDs of the displays to use
            /// Hardware PNPIDs of the displays, corresponding 1-1 with
            /// m_directVendorIds
            std::vector<std::string> m_pnpIds;

            /// @todo Mode selection should include update rate/pixel format
            /// selections.
            int32_t m_directModeIndex; //< Which mode to use (-1 to select based
            // on params)?
            uint32_t m_directDisplayIndex; //< Which display to use
            bool m_directHighPriority;     //< Do high-priority rendering in
            // DirectMode?
            unsigned m_numBuffers; //< How many buffers (2 = double buffering)
            bool m_verticalSync;   //< Do we wait for Vsync to swap buffers?
            bool m_verticalSyncBlocksRendering; //< Block rendering waiting for
            // sync?
            std::string m_renderLibrary; //< Which rendering library to use

            std::string m_windowTitle; //< Title of any window we create
            bool m_windowFullScreen;   //< If not Direct Mode, do we want full
                                       /// screen?
            int m_windowXPosition;     //< Where to put the window
            int m_windowYPosition;     //< Where to put the window
            /// @todo Implement this
            Display_Rotation m_displayRotation; //< Present mode: Rotate display
            // about Z when presenting
            unsigned m_bitsPerColor; //< Color depth of the window we want

            /// This expands the size of the render window, adding more pixels
            /// around the border, so that there is margin to be rendered when
            /// we're using distortion (which pulls in pixels from outside the
            /// boundary) and when we're using Time Warp (which also pulls in
            /// edge pixels as we move).  The larger this factor, the less
            /// likely we'll see clamped images at the border but the more
            /// work taken during rendering.
            /// A factor of 1.0 means render at standard size, 2.0 would
            /// render 4x as many pixels (2x in both X and Y).
            float m_renderOverfillFactor;

            /// This increases the density of the render texture, adding more
            /// pixels within the texture, so that when it is rendered into
            /// the final buffer with distortion correction it can be
            /// expanded by the distortion without making fat pixels.
            ///   Alternatively, it can be reduced to make rendering
            /// faster at the expense if visible pixel resolution.
            /// A factor of 1.0 means render at standard size, 2.0 would
            /// render 4x as many pixels (2x in both X and Y) and 0.5
            /// would render 1/4 as many pixels.
            float m_renderOversampleFactor;

            bool m_distortionCorrection; //< Use distortion correction?
            std::vector<DistortionParameters>
                m_distortionParameters; //< One set per eye x display

            bool m_enableTimeWarp;       //< Use time warp?
            bool m_asynchronousTimeWarp; //< Use Asynchronous time warp?
                                         //(requires enable)

            /// Render waits until at most this many ms before vsync to do
            /// timewarp (requires enable)
            float m_maxMSBeforeVsyncTimeWarp;

            /// Prediction settings.
            bool m_clientPredictionEnabled; //< Use client-side prediction?
            /// Static Delay + Delay from present to eye start
            std::vector<float> m_eyeDelaysMS;
            bool m_clientPredictionLocalTimeOverride;  //< Override tracker timestamp?

            OSVRDisplayConfiguration
                m_displayConfiguration; //< Display configuration

            std::string m_roomFromHeadName; //< Transform to use for head space

            /// Graphics library (device/context) to use instead of creating one
            /// if the pointer is non-NULL.  Note that the appropriate context
            /// pointer for the m_renderLibrary must be filled in.
            GraphicsLibrary m_graphicsLibrary;
        };

        //--------------------------------------------------------------------------
        // Methods needed to handle passing information across a DLL boundary in
        // a way that is safer.  std::vector cannot be passed across such a
        // boundary
        // in the general case.

        /// @brief C-api method to initiate render info and store it internally.
        ///
        /// Compute the rendering info for all display surfaces based on the
        /// passed-in
        /// rendering parameters.  Store it internally for later retrieval.
        /// @return The number of stored RenderInfos, one per surface.
        virtual size_t OSVR_RENDERMANAGER_EXPORT
        LatchRenderInfo(const RenderParams& params = RenderParams());

        /// @brief C-api method to get parameters needed to render on surface
        /// @return A filled-in RenderInfo structure if index is valid, a
        /// default-constructed one otherwise.
        virtual RenderInfo OSVR_RENDERMANAGER_EXPORT
        GetRenderInfo(size_t index);

        //=============================================================
        // Destroy the existing distortion meshes and create new ones with the
        // given parameters.
        // Can be called even when no distortion meshes yet exists, to create
        // them.
        // There is a different function for each style of distortion mesh.
        virtual OSVR_RENDERMANAGER_EXPORT bool UpdateDistortionMeshes(
            DistortionMeshType type //< Type of mesh to produce
            ,
            std::vector<DistortionParameters> const&
                distort //< Distortion parameters
            );

        //=============================================================
        // Updates the internal "room to world" transformation (applied to all
        // tracker data for this client context instance) based on the user's
        // head
        // orientation, so that the direction the user is facing becomes -Z to
        // your
        // application. Only rotates about the Y axis (yaw).
        //
        // Note that this method internally calls osvrClientUpdate() to get a
        // head pose
        // so your callbacks may be called during its execution!
        virtual OSVR_RENDERMANAGER_EXPORT void SetRoomRotationUsingHead();

        //=============================================================
        // Clears/resets the internal "room to world" transformation back to an
        // identity transformation - that is, clears the effect of any other
        // manipulation of the room to world transform.
        virtual OSVR_RENDERMANAGER_EXPORT void ClearRoomToWorldTransform();

      protected:
        /// @brief Constructor given OSVR context and parameters
        RenderManager(OSVR_ClientContext context,
                      const ConstructorParameters& p);

        /// Mutex to provide thread safety to this class and its
        /// subclasses.  NOTE: All subclasses must lock this mutex
        /// for the duration of all public methods besides their
        /// constructor.
        std::mutex m_mutex;

        /// Internal versions of functions that require a mutex, so that
        /// we can call them from functions with a mutex without blocking.
        virtual std::vector<RenderInfo>
        GetRenderInfoInternal(const RenderParams& params = RenderParams());

        virtual bool RegisterRenderBuffersInternal(
            const std::vector<RenderBuffer>& buffers,
            bool appWillNotOverwriteBeforeNewPresent = false);

        virtual bool PresentRenderBuffersInternal(
            const std::vector<RenderBuffer>& buffers,
            const std::vector<RenderInfo>& renderInfoUsed,
            const RenderParams& renderParams = RenderParams(),
            const std::vector<OSVR_ViewportDescription>&
                normalizedCroppingViewports =
                    std::vector<OSVR_ViewportDescription>(),
            bool flipInY = false);

        virtual bool PresentSolidColorInternal(
          const RGBColorf& color);

        virtual bool UpdateDistortionMeshesInternal(
            DistortionMeshType type //< Type of mesh to produce
            ,
            std::vector<DistortionParameters> const&
                distort //< Distortion parameters
            ) = 0;

        std::vector<RenderInfo>
            m_latchedRenderInfo; //< Stores vector of latched RenderInfo
        virtual size_t
          LatchRenderInfoInternal(const RenderParams& params = RenderParams());

        /// OSVR context to use.
        OSVR_ClientContext m_context;

        // Variables describing the desired characteristics of the
        // rendering, parsed from the display configuration file
        // (vendor, resolution, etc.) and from the pipeline configuration
        // file (direct display mode, high-priority rendering, number
        // of buffers).  These are passed to the constructor.
        ConstructorParameters m_params;

        /// Head space to use, or nullptr in case of none (which should
        /// be checked for, but is sort of an error).
        OSVR_ClientInterface m_roomFromHeadInterface;
        OSVR_PoseState m_roomFromHead; //< Transform to use for head space

        /// @brief Stores display callback information
        ///
        /// Stores the information needed to call the display
        /// handler.
        struct {
            DisplayCallback m_callback;
            void* m_userData;
        } m_displayCallback;

        /// @brief Stores viewport callback information
        ///
        /// Stores the information needed to call the viewport/projection
        /// handler.
        struct {
            ViewProjectionCallback m_callback;
            void* m_userData;
        } m_viewCallback;

        /// @brief Stores rendering callback information
        ///
        /// Stores the vectors of callback information along with the
        /// state that the callback is to update.  It keeps all of the
        /// context needed to unregister the callback by free()ing the
        /// interface.
        class RenderCallbackInfo {
          public:
            std::string m_interfaceName;
            OSVR_ClientInterface m_interface;
            RenderCallback m_callback;
            void* m_userData;
            OSVR_PoseState m_state;
        };
        std::vector<RenderCallbackInfo> m_callbacks;

        /// @brief Tell how many eyes are associated with this RenderManager
        /// @return 0 on failure/not open, number of eyes on success
        size_t GetNumEyes();

        /// @brief Tell how many displays are associated with this RenderManager
        /// @return 0 on failure/not open, number of displays on success
        size_t GetNumDisplays();

        /// @brief Tell how many eyes are associated with each display in this
        /// RenderManager
        /// @return 0 on failure/not open, number of eyes per display on success
        size_t GetNumEyesPerDisplay();

        /// @brief Tell which display is associated with this eye
        /// @return 0 on failure/not open, display index on success
        /// @todo Will need to be generalized when we have multiple displays per
        /// eye
        size_t GetDisplayUsedByEye(size_t eye);

        /// @brief Width of the virtual display in Present mode, actual in
        /// Render
        /// The display window's width.  If m_displayRotation is Ninety
        /// or Two_Seventy, then the physical display does not match the
        /// virtual display that we're developing transformations and
        /// viewports for to hand to the client.  In that case, this value
        /// is for the virtual screen and any hardware devices must switch
        /// width and height when they are creating their buffers and
        /// viewports, and they must rotate their views appropriately. This
        /// is so that text and other sprites will be rendered with the
        /// correct orientation, and then the resulting pixel map will be
        /// aimed at the screen.
        ///  Note: This never includes the rendering overfill, only the actual
        /// display size.
        int m_displayWidth;

        /// @brief Height of the virtual display in Present mode, actual in
        /// Render
        /// The display window's height.  If m_displayRotation is Ninety
        /// or Two_Seventy, then the physical display does not match the
        /// virtual display that we're developing transformations and
        /// viewports for to hand to the client.  In that case, this value
        /// is for the virtual screen and any hardware devices must switch
        /// width and height when they are creating their buffers and
        /// viewports, and they must rotate their views appropriately. This
        /// is so that text and other sprites will be rendered with the
        /// correct orientation, and then the resulting pixel map will be
        /// aimed at the screen.
        ///  Note: This never includes the rendering overfill, only the actual
        /// display size.
        int m_displayHeight;

        /// These are used in the Render() callback-based rendering approach
        /// to store information during the processing.  They are not used in
        /// the Present() rendering approach.
        RenderParams m_renderParamsForRender;
        std::vector<RenderInfo> m_renderInfoForRender;

        /// NOTE: The base-class implementation constructs a texture matrix
        /// that is apropriate for use in OpenGL.  Other rendering libraries
        /// that
        /// use different coordinates (such as Direct3D) should override this
        /// method.  There is not a simple change that can be made to the
        /// resulting
        /// transform to deal with the differences in both viewport coordinates
        /// and matrix storage order.
        ///   Assumes that it is starting in a texture-coordinate space that has
        /// (0,0) at the lower left corner of the image and (1,1) at the
        /// upper-right corner of the image, with Z pointing out of the image.
        ///  Constructs a change of pose that transforms such coordinates from
        /// the "last" space and puts them into the "current" space.
        ///  It proceeds by moving the points from texture space into world
        /// space (by scaling and translating them to match a viewport at a
        /// given distance in Z from the eyepoint).  This puts the points into
        /// projection space.
        ///  It then inverts the ModelView matrix from the last position and
        /// applies it to get the points back into world space.
        ///  In then reverses the process, using the ModelView matrix from the
        /// "current" location (all other matrices are the same) to bring the
        /// points back into texture space.
        ///  It is up to the caller to bring the texture coordinates to and from
        /// the space described above.
        ///  @param [in] usedRenderInfo Rendering info used to construct the
        /// textures we're going to present.
        ///  @param [in] currentRenderInfo Rendering info to warp to.
        ///  @param [in] assumedDepth Depth at which the virtual projected
        ///  window should
        /// be constructed.  The further away it is, the less impact translation
        /// will
        /// have on the result.  It should not be closer than the closest object
        /// in
        /// the scene, and probably not closer than about 1 meter to avoid
        /// excess
        /// translation impact.
        ///  @return True on success, false (with empty transforms vector) on
        /// failure.
        virtual bool
        ComputeAsynchronousTimeWarps(std::vector<RenderInfo> usedRenderInfo,
                                     std::vector<RenderInfo> currentRenderInfo,
                                     float assumedDepth = 2.0f);

        /// Asynchronous time warp matrices suitable for use in OpenGL,
        /// taking (-0.5,-0.5) to (0.5,0.5) coordinates into the appropriate new
        /// location.
        /// Be sure to modify (transpose, etc.) as needed for other rendering
        /// libraries.
        /// @todo Can we make an opaque Eigen matrix/transform type so that we
        /// don't need to copy in the computation and get a more useful type but
        /// also
        /// don't need to #include the Eigen headers in this class?
        typedef struct { float data[16]; } matrix16;
        std::vector<matrix16> m_asynchronousTimeWarps;

        /// Holds a pointer to the graphics library state.
        GraphicsLibrary m_library; //!< Graphics library to use
        RenderBuffer m_buffers;    //!< Buffers to use to render into.

        bool m_renderBuffersRegistered; //!< Keeps track of whether we have
        //! registered buffers

        //=============================================================
        // These methods are helper methods for the Render* callback
        // functions below, making it easy for them to compute the
        // various information they need.

        /// @brief Fill in a projection transform for a given eye
        /// This routine computes the projection matrix needed for the
        /// oversized view required by the m_renderOverfillFactor.
        /// @return True on success, false on failure.
        virtual bool ConstructProjection(
            size_t whichEye //< Input; index of the eye to use
            , double nearClipDistanceMeters //< Perpendicular distance to near
            // clipping plane
            , double farClipDistanceMeters //< Perpendicular distance to far
            // clipping plane
            , OSVR_ProjectionMatrix& projection //< Output projection
            );

        /// @brief Fill in the viewport for a given eye on the Render path
        /// This routine computes the viewport size with the
        /// amount needed by the m_renderOverfillFactor and the
        /// m_renderOversampleFactor.  It also
        /// does not include the shift needed to move the eye to the
        /// correct location in the output display.
        /// @return True on success, false on failure.
        virtual bool ConstructViewportForRender(
            size_t whichEye //< Input; index of the eye to use
            , OSVR_ViewportDescription& viewport //< Output viewport
            );

        /// @brief Fill in the viewport for a given eye on the Present path
        /// This routine computes the viewport size without the
        /// amount needed by the m_renderOverfillFactor or the
        /// m_renderOversampleFactor; if these
        /// is required, they should be added by the caller.  It also
        /// always includes the shift needed to move the eye to the
        /// correct location in the output display; if that is not
        /// needed, then the left and lower fields should be ignored
        /// by the caller.
        ///   This constructs a viewport in the un-rotated space with
        /// respect to the m_displayRotation constructor parameter.
        /// To turn this into a viewport for the Present-buffer rotation
        /// based on that parameter, call RotateViewport().
        /// @return True on success, false on failure.
        virtual bool ConstructViewportForPresent(
            size_t whichEye //< Input; index of the eye to use
            , OSVR_ViewportDescription& viewport //< Output viewport
            , bool swapEyes //< Should we swap left and right eyes?
            );

        /// @brief Adjust the viewport based on m_displayRotation
        /// This routine adjusts a viewport produced by the
        /// ConstructViewport() function based on the rotation
        /// between the rendering space and the presentation space.
        /// @return Adjusted viewport.
        virtual OSVR_ViewportDescription RotateViewport(
            const OSVR_ViewportDescription& viewport //< Input viewport
            );

        /// @brief Construct ModelView for a given eye, space, and RenderParams
        ///
        /// @return True on success, false on failure.
        virtual bool ConstructModelView(
            size_t whichSpace //< Input; index of the space to use
            , size_t whichEye //< Input; index of the eye to use
            , RenderParams params //< Input; render parameters
            , OSVR_PoseState&
                eyeFromSpace //< Output info needed to make ModelView
            );

        /// @brief Compute in-display rotations/flip matrix.
        ///  Assumes that it is starting in a world-space quad render that has
        /// (-1,-1) at the lower left corner of the screen and (1,1) at the
        /// upper-right corner of the screen, with Z pointing backwards.  This
        /// the default projection space.  It only applies rotations and flips,
        /// so can handle uniform scaling of either the projection space or the
        /// quad
        /// vertex locations.
        ///  It assumes that a non-uniform viewport will map this
        /// to the correct pixel size on the display.
        ///  This is expected to be used in the PresentEye functions to help
        /// adjust the ModelView matrix on the quad that the rendered texture
        /// is applied to.
        ///  @return True on success, false (with untouched transform) on
        /// failure.
        bool ComputeDisplayOrientationMatrix(
            float rotateDegrees //< Rotation in degrees around Z
            , bool flipInY //< Flip in Y after rotating?
            , matrix16& outMatrix //< Matrix to use.
            );

        /// @brief Compute texture matrix adjustment to subset render buffer
        /// This is part of the PresentMode rendering pipeline.  It is part
        /// of the calculations used to present an eye.  It produces a
        /// matrix that will adjust the projection matrix used for time
        /// warp to make it point at the specified subset of the texture
        /// in a render buffer that may have more than one eye drawn
        /// into it.
        ///  It takes texture coordinates in the range (0,0)-(1,1) and maps
        /// them to the range specified in the normalized viewport.
        ///  @param normalizedCroppingViewport[in] Viewport specifying
        /// the portion of the texture-coordinate range (0,0) to (1,1)
        /// that contains the eye we are rendering.
        ///  @param outMatrix[out] Transformation to use.
        ///  @return True on success, false (with untouched transform) on
        /// failure.
        bool ComputeRenderBufferCropMatrix(
            OSVR_ViewportDescription normalizedCroppingViewport,
            matrix16& outMatrix);

        /// Vector of interpolators constructed by the
        /// ComputeDistortionMesh() function to be used by the
        /// DistortionCorrectTextureCoordinate() function based on
        /// the number of meshes needed (1 per color, the mesh is
        /// computed per eye so we only need to keep those around
        /// for our current eye)
        /// when it is using an unstructured grid.
        std::vector<UnstructuredMeshInterpolator *> m_interpolators;

        //=============================================================
        // These methods must be implemented by all derived classes.
        //  They enable the Render() method above to do the generic work
        // and only device-specific work is done in the derived classes.
        //  It is okay to define a method that does nothing, if there is
        // nothing to do for a particular operation.  We make these pure
        // virtual so that a developer will know what needs filling in.
        //  NOTE: The calls are guaranteed to be nested as follows, with
        // the potential for multiple calls of any of the inner nesting
        // levels for a particular rendering recipe:
        //  RenderPathSetup
        //  RenderFrameInitialize
        //      RenderDisplayInitialize
        //          RenderEyeInitialize
        //              RenderSpace
        //          RenderEyeFinalize
        //      RenderDisplayFinalize
        //  RenderFrameFinalize

        /// @brief Called at the beginning of the first Render() call.
        virtual bool RenderPathSetup() = 0;
        bool m_renderPathSetupDone = false;

        /// @brief Initialize rendering for a new frame
        virtual bool RenderFrameInitialize() = 0;

        /// @brief Initialize rendering for a new display
        virtual bool
        RenderDisplayInitialize(size_t display //< Which display (0-indexed)
                                ) = 0;

        /// @brief Initialize rendering for a specified eye
        virtual bool RenderEyeInitialize(size_t eye //< Which eye (0-indexed)
                                         ) = 0;

        /// @brief Render objects in a specified space (from m_callbacks)
        virtual bool
        RenderSpace(size_t whichSpace //< Index into m_callbacks vector
                    , size_t whichEye //< Which eye are we rendering for?
                    , OSVR_PoseState pose //< ModelView transform to use
                    , OSVR_ViewportDescription viewport //< Viewport to use
                    , OSVR_ProjectionMatrix projection //< Projection to use
                    ) = 0;

        /// @brief Finalize rendering for a specified eye
        virtual bool RenderEyeFinalize(size_t eye //< Which eye (0-indexed)
                                       ) = 0;

        /// @brief Finalize rendering for a new display
        virtual bool
        RenderDisplayFinalize(size_t display //< Which display (0-indexed)
                              ) = 0;

        /// @brief Finalize rendering for a new frame
        virtual bool RenderFrameFinalize() { return true; }

        //=============================================================
        // These methods must be implemented by all derived classes.
        // They enable the PresentRenderBuffers() method above to do the
        // generic work and only device-specific work is done in the
        // derived classes.
        //  It is okay to define a method that does nothing, if there is
        // nothing to do for a particular operation.  We make these pure
        // virtual so that a developer will know what needs filling in.
        //  NOTE: The calls are guaranteed to be nested as follows, with
        // the potential for multiple calls of any of the inner nesting
        // levels for a particular rendering recipe:
        //  PresentFrameInitialize
        //      PresentDisplayInitialize
        //          PresentEye
        //      PresentDisplayFinalize
        //  PresentFrameFinalize

        /// @brief Initialize presentation for a new frame
        virtual bool PresentFrameInitialize() = 0;

        /// @brief Initialize presentation for a new display
        virtual bool
        PresentDisplayInitialize(size_t display //< Which display (0-indexed)
                                 ) = 0;

        /// @brief Initialize presentation for a specified eye
        /// @todo add time shear parameters
        class PresentEyeParameters {
          public:
            PresentEyeParameters() {
                m_index = 0;
                m_rotateDegrees = 0;
                m_flipInY = false;
                m_buffer.D3D11 = nullptr;
                m_buffer.OpenGL = nullptr;
                m_timeWarp = nullptr;
            }

            size_t m_index;         //< Which eye (0-indexed)
            double m_rotateDegrees; //< How much to rotate eye when showing in
            // screen
            bool m_flipInY; //< Should we flip in Y after all other transforms?
            RenderBuffer m_buffer; //< Buffer to draw.
            /// This is a viewport in the texture-coordinate range of (0-1)
            /// telling
            /// how much of the buffer is taken up by this eye.  It should
            /// normally be
            /// left=lower=0, width=height=1.  It will only be otherwise if
            /// there are
            /// two or more eyes packed into the same buffer.
            OSVR_ViewportDescription m_normalizedCroppingViewport;
            matrix16* m_timeWarp; //< Time Warp matrix to use (nullptr
            // for none)
        };
        virtual bool PresentEye(PresentEyeParameters params) = 0;

        /// @brief Set the specified eye to the specified color
        /// @param eye[in] The eye to set.
        /// @param color[in] The color to set, RGB, 0-1 for each.
        /// @return True on success, false on failure.
        virtual bool SolidColorEye(size_t eye, const RGBColorf &color) = 0;

        /// @brief Finalize presentation for a new display
        virtual bool
        PresentDisplayFinalize(size_t display //< Which display (0-indexed)
                               ) = 0;

        /// @brief Finalize presentation for a new frame
        virtual bool PresentFrameFinalize() = 0;

        friend class RenderManagerNVidiaD3D11OpenGL;
        friend RenderManager OSVR_RENDERMANAGER_EXPORT*
        createRenderManager(OSVR_ClientContext context,
                            const std::string& renderLibraryName,
                            GraphicsLibrary graphicsLibrary);
    };

    //=========================================================================
    /// @brief Factory to create an appropriate RenderManager
    ///
    /// Factory function that creates a pointer to a RenderManager based on the
    /// information in the configuration files from the server.  It is also
    /// given
    /// the OSVR context to use for determining transformation matrices.
    /// @param context OSVR Client context that will be cloned to provide
    ///        thread-safe access to RenderManager.  (@todo clone this when
    ///        that functionality become available, for now a separate one
    ///        is created and used).
    /// @param renderLibraryName Name of the rendering library to use.  It can
    ///        currently be one of: OpenGL, Direct3D11.
    /// @param graphicsLibrary Graphics device to use.  If this is NULL, then
    /// a device and context appropriate to the rendering library defined in the
    /// renderLibraryName parameter will be created.  If the user creates one,
    /// it
    /// must match the one defined by the renderLibraryName.  The caller
    /// is responsible for constructing and destroying both the object pointed
    /// to and the appropriate pointer within it.
    ///   This function hangs until it receives configuration information from
    /// a running server connected to by an OSVR context that it creates.
    /// @return Pointer to a created object or nullptr if it cannot create one
    /// given the configuration read from the server.
    RenderManager OSVR_RENDERMANAGER_EXPORT*
    createRenderManager(OSVR_ClientContext context,
                        const std::string& renderLibraryName,
                        GraphicsLibrary graphicsLibrary = GraphicsLibrary());

    //=========================================================================
    /// C API for the RenderManager (will be in a separate file).
    /// @todo

} // namespace renderkit
} // namespace osvr
