ANSEL INTEGRATION GUIDE
=======================


1. SETTING UP
-------------
### 1.1 CODE, COMPILER, AND LINKER CONFIGURATION
The include folder in the Ansel SDK contains all the public header files you will need. A top-level include file, 
AnselSDK.h, is provided for convenience so that you only need to include that one file in your project's code. If you 
add the path to the Ansel SDK include folder to your project settings this is all you need to gain access to the 
functionality of the SDK: 

```cpp
#include <AnselSDK.h>
```
Note that we do not offer Ansel SDK for static linking: this is intentional. However we do provide an option of either
linking to the Ansel SDK using an import library or delay loading it. The lib folder in the Ansel SDK contains the import 
libraries for 32-bit and 64-bit architecture DLLs. Based on the targeted platform architecture you must link in the 
corresponding import library. If you wish to delay load the Ansel SDK, link the AnselSDKLoader static library for appropriate 
architecture also provided in the lib folder. This method requires you to also call the ```ansel::loadLibrary(<optional path>)``` 
function (see ```ansel/DelayLoad.h``` header> before any other call to the Ansel SDK. The function will use the default 
search rules to locate the SDK DLL or use the provided path (using wide or narrow string, depending on your configuration). 
All other aspects of the Ansel SDK integration should remain the same and no special workflow is required to work 
with delay loaded SDK.

The redist folder in the Ansel SDK contains the DLLs for 32-bit and 64-bit architecture. These DLLs must be 
redistributed with the game - if the game is only offered for 64-bit architecture then only 64-bit DLLs should be 
redistributed (and similarly for 32-bit).

### 1.2 MACHINE CONFIGURATION
In order to use Ansel you need 

* Windows PC with Windows 7 (32-bit or 64-bit) or newer
* GeForce GTX 600 series or newer
* Ansel-ready display driver. Any NVIDIA display driver of version 368.81 or higher will meet this requirement
* A game using Dx11 that has integrated the Ansel SDK

> **NOTE:**
> * Support for Dx12 is coming soon and options for OpenGL are being investigated.
> * We currently do not support Ansel for the following NVIDIA GPU / Display configurations 
>   * SLI
>   * Optimus / Hybrid
>   * Surround
>   Support for the above is coming soon, though. 

:warning: Your game executable needs to be whitelisted by the driver for Ansel activation to succeed. If you do not 
have a driver with the proper whitelisting you can force whitelisting to succeed for all executables with this command:

```ms-dos
NvCameraEnable.exe whitelisting-everything
```

This utility is provided with the SDK. The setting will be persisted - until you perform a clean driver install or call 
the command again with <code>whitelisting-default</code> option.


2. INTEGRATING SDK WITH YOUR GAME
---------------------------------
This Ansel SDK uses four major concepts: Configuration, Session, Camera, and optionally Hints. We will go through each of 
them in turn and build up an example of game integration in the process.

> **NOTE:** Please consult the header files in the Ansel SDK for reference style documentation.

### 2.1 CONFIGURATION
During the initialization phase of the game the Ansel configuration should be specified. This is done via the 
<code>ansel::Configuration</code> object.  Please consult the header file, ansel/Configuration.h, for detailed documentation on
each field and default values for each.  This is how configuration is typically performed:

```cpp
#include <AnselSDK.h>
...
// during initialization phase of game:
ansel::Configuration config;
// Configure values that we want different from defaults:
config.translationalSpeedInWorldUnitsPerSecond = 5.0f;
config.right =   { -axis_left.x,   -axis_left.y,   -axis_left.z };
config.up =      {  axis_up.x,      axis_up.y,      axis_up.z };
config.forward = {  axis_forward.x, axis_forward.y, axis_forward.z };

config.fovType = ansel::kVerticalFov;
config.isCameraOffcenteredProjectionSupported = true;
config.isCameraRotationSupported = true;
config.isCameraTranslationSupported = true; 
config.isCameraFovSupported = true;
config.isFilterOutsideSessionAllowed = false; 

config.startSessionCallback = startAnselSessionCallback;
config.stopSessionCallback = stopAnselSessionCallback;
config.startCaptureCallback = startAnselCaptureCallback;
config.stopCaptureCallback = stopAnselCaptureCallback;

ansel::setConfiguration(config);
```

> **NOTE:** Ansel cannot be used until the configuration has been set. It is therefore advisable to perform the configuration 
soon after the AnselSDK DLL has been loaded.

Let's go through this particular configuration in order. 

Games used different units to represent size - it is therefore not possible in Ansel to have a uniform translational 
speed that works for all games. This is why you will have to set the translationalSpeedInWorldUnitsPerSecond to a value
that feels right for your game. Move the camera around once Ansel mode has been activated to test your settings; test it
also with the accelerator key pressed (see Chapter 3 for Ansel controls)

Note that even though a game must specify how many meters (or fraction of a meter) are in a world unit 
(see Configuration::metersInWorldUnit) this by itself is not enough to derive a default speed that works well for all 
games. Some games have large worlds where travel is often performed in vehicles (or on mounts) while other games use 
much smaller worlds and thus much lower travel speeds.    

Games use different orientations and chirality (handed-ness) for their coordinate systems. The conversion between game 
coordinate system and Ansel's internal coordinate system is handled internally by Ansel. This greatly simplifies 
integration for game developers. You must specify the unit vectors for right, up, and forward directions that your game 
uses as part of the configuration step. Once that is done all orientations exchanged between Ansel SDK and the game will 
be in the game's coordinate system (see section 2.3 for more details).

Games will either use vertical or horizontal angle to specify the field of view. The default value in the Ansel 
configuration is horizontal angle but if your game uses vertical angle you must specify ansel::kVerticalFov as the 
fovType. This will free you from having to convert between the game's field of view and Ansel's internal field of view 
(see section 2.3 for details).

Not all features of Ansel will necessarily be supported by a game integration. There are also cases where some features 
of Ansel will be disabled under a particular game scenario. Here we will only discuss the general settings that reflect 
the Ansel integration with the particular game engine; we will cover specific game scenarios in section 2.2.

In order to support high resolution capture, where screenshots are taken at resolutions higher than display resolution, 
the game engine needs to support off center projection. In the Configuration object you specify if this feature is 
supported by this particular game integration via the isCameraOffcenteredProjectionSupported field.  Similarly, 
support for allowing players to move, orient and zoom the camera is specified via the other Supported fields. We will 
discuss in detail how support for these features is implemented in section 2.3.

Finally we have the session and capture callbacks. The capture callbacks are optional. Those should only be configured 
if the game uses non-uniform screen based effects that cause issues in multipart shots. An example of an effect that 
causes issues is vignette. Applying this effect to all the individual tiles of a highres capture or 360 capture will 
produce poor results. The startCaptureCallback will be called when the multipart capture sequence starts and can 
therefore be used to disable vignette. The vignette can then be restored when stopCaptureCallback is called. A game 
integration could of course chose to disable vignette when Ansel session is started but we recommend against this since 
it would remove the vignette from regular screen shots (non-multipart shot). Note that all these callbacks receive the 
value of the userPointer that is specified in the Configuration object.

The session callbacks are mandatory, they are called when a player wants to begin an Ansel session or end a session. 
Without these callbacks Ansel cannot be activated. We will discuss them in detail in the next section.

> **NOTE:** All callbacks in Ansel will be triggered from calling D3D Present. They will therefore also happen on the same 
thread that calls D3D Present.

### 2.2 SESSION
The time period from when a player successfully starts Ansel and until Ansel is stopped is called a session. A session 
is collaboratively started and operated between the game and Ansel. When a player requests a session start (for example 
by pressing ALT+F2) Ansel will call the registered session start callback. It is however expected that Ansel cannot 
always be activated. The game may for instance be on a loading screen or playing a movie sequence. The callback should 
then immediately return with Ansel::kDisallowed return value. 

During an Ansel session the game:

* Must stop drawing UI and HUD elements on the screen, including mouse cursor
* Must call ansel::updateCamera on every frame (see section 2.3 for details)
* Should pause rendering time (i.e. no movement should be visible in the world)
* Should not act on any input from mouse and keyboard and must not act on any input from gamepads

The function signatures of the session related callbacks are listed below.

```cpp
enum StartSessionStatus
{
	kDisallowed = 0,
	kAllowed
};

typedef StartSessionStatus(*StartSessionCallback)(SessionConfiguration& settings, void* userPointer);

typedef void(*StopSessionCallback)(void* userPointer);
```

As you will notice the start callback receives an additional SessionConfiguration object. This object must reflect the configuration of the currently activated session if Ansel::kAllowed is returned from the callback. Let's take a look at this configuration object:

```cpp
struct SessionConfiguration
{
	// User can move the camera during session
      bool isTranslationAllowed;
	// Camera can be rotated during session
	bool isRotationAllowed;
	// FoV can be modified during session
	bool isFovChangeAllowed;
	// Game is paused during session
	bool isPauseAllowed;
	// Game allows highres capture during session
	bool isHighresAllowed;
	// Game allows 360 capture during session
	bool is360MonoAllowed;
	// Game allows 360 stereo capture during session
	bool is360StereoAllowed;
      // Default constructor not included here
};
```

As you can see each session has fine grained control over what features of Ansel are offered to the player. This is to 
support different contexts that the game may be in - where some features of Ansel may not be desired. For instance, 
let's say that a game uses in-engine movie sequences. Ansel could certainly be used to take regular and highres 
screenshots during those sequences.  However, the game developers may wish to prohibit any player controlled camera 
movement or 360 captures during those sequences since they could expose geometry that was never built because the 
sequences have been carefully orchestrated. This is what such a callback could look like:

```cpp
ansel::StartSessionStatus startAnselSessionCallback(ansel::SessionConfiguration& conf, 
                           void* userPointer)
{
	if (isGameLoading || isGameInMenuScreens)
		return ansel::kDisallowed;

	if (isGameCutScenePlaying)
	{
		conf.isTranslationAllowed = false:
		conf.isRotationAllowed = false:
		conf.isFovChangeAllowed = false;
		conf.is360MonoAllowed = false:
		conf.is360StereoAllowed = false;
	}

	 g_isAnselSessionActive = true;

      return ansel::kAllowed;
}
```

> **NOTE:** The final feature set presented in the Ansel UI is always a combination of the global configuration 
specified via the <code>ansel::Configuration</code> object and the particular session configuration specified via the 
<code>ansel::SessionConfiguration</code> object. For instance, if off-center projection is not supported by the 
integration and this is marked as such in the global configuration then the <code>isHighresAllowed</code> setting will 
have no effect for an Ansel session because the feature is simply not supported by the integration. 

> Similarly, if `isRotationAllowed` is set to false for the session then no form of 360 capture will be 
possible and hence the `is360MonoAllowed` and `is360StereoAllowed` will have no effect. In the sample code above we 
still set them but this is done for clarity and completeness.

In a session where rendering time cannot be paused this can be communicated with the isPauseAllowed setting. This could 
for instance be the case in a game that offers multiplayer game modes. That being said, some multiplayer game engines 
still allow rendering time to be frozen - this just means that the state of the world will have advanced when the Ansel 
session ends. The stitcher developed for Ansel does not currently support feature detection or other methods used to 
handle temporal inconsistencies. This means that multipart shots (highres and 360) are not supported during sessions 
when pause is disallowed.

The stop session callback is called when the player requests to end the session (for instance by pressing ALT+F2). This 
function is only called if the previous call to start session returned `ansel::kAllowed`. The matching function to what 
we implemented above would look like this:

```cpp
void stopAnselSessionCallback(void* userPointer)
{
	g_isAnselSessionActive = false;
}
```

### 2.3 CAMERA
The camera object acts as the communication channel between Ansel and the game. The concepts described earlier are 
either used for one-off configuration or rare events. Once an Ansel session has been started the game needs to call 
Ansel on every frame to update the camera. It's helpful to first look at the Camera interface:

```cpp
struct Camera
{
	nv::Vec3 position;
	nv::Quat rotation;
	float fov;
	float projectionOffsetX, projectionOffsetY;
};

ANSEL_SDK_API void updateCamera(Camera& camera);
```

As noted before the header file contains documentation for each field.  Here it suffices to say that the values are all 
in the game's coordinate system, since this has been established via the `ansel::setConfiguration` call 
(see section 2.1). The field of view, fov, is in degrees and in the format specified during the previously mentioned 
call. The final two values are used to specify the off-center projection amount. Let's illustrate how this all works 
with sample code following the earlier session callbacks we had created:

```cpp
if (g_isAnselSessionActive)
{
  if (!was_ansel_in_control)
  {
    store_original_camera_settings();
    was_ansel_in_control = true;
  }
  ansel::Camera cam;
  cam.fov = get_game_fov_vertical_degrees();
  cam.position = { game_cam_position.x, game_cam_position.y,
                   game_cam_position.z };
  cam.rotation = { game_cam_orientation.x,
                   game_cam_orientation.y,
                   game_cam_orientation.z,
                   game_cam_orientation.w };

  ansel::updateCamera(cam);
  // This is where a game would typically perform collision detection
  // and adjust the values requested by player in cam.position 

  if (cam.projectionOffsetX != 0.0f ||
      cam.projectionOffsetY != 0.0f))
  {
    // modify projection matrices by the offset amounts -
    // we will explore this in detail separately  
  }

  game_cam_position.x = cam.position.x;
  game_cam_position.y = cam.position.y;
  game_cam_position.z = cam.position.z;

  game_cam_orientation.x = cam.rotation.x;
  game_cam_orientation.y = cam.rotation.y;
  game_cam_orientation.z = cam.rotation.z;
  game_cam_orientation.w = cam.rotation.w;

  set_game_fov_vertical_degrees(cam.fov);

  return;
}
else
{
  if (was_ansel_in_control)
  {
    restore_original_camera_settings();
    was_ansel_in_control = false;
  }
}
```

The sample above is a full implementation of Ansel support, in the sense that it supports camera translation, camera 
rotation, changing of field of view, and offset projection.  The sample does however not employ any collision detection 
or limitation on the camera movement. This is unrealistic since most games will at least want to limit the range the 
camera can travel. These limitations will always be specific to the game in question and collision handling is best 
handled by the game using the systems that are already in place. We do therefore not elaborate on this piece here. It 
should still be noted that Ansel is stateless when it comes to position so the game can adjust the position (based on 
collision response or constraints) any way it sees fit. The new position will always be communicated to Ansel on the 
next frame, via ```ansel::updateCamera```.

Another aspect that is not covered in the sample code above is the handling of projection offset. We will explore that 
aspect in more detail here.
> NOTE: Need to flesh out projection offset code!

### 2.4 HINTS
To capture EXR images Ansel tries to use certain heuristics to detect which of the game buffers contains HDR pixel data.
In order for this to be more robust the game integration is welcome to use optional hints (see ansel/Hints.h)

```
namespace ansel
{
	// Call this right before setting HDR render target active
	ANSEL_SDK_API void markHdrBufferBind();
	// Call this right after the last draw call into the HDR render target
	ANSEL_SDK_API void markHdrBufferFinished();
}
```

In order for Ansel to know exactly which buffer to capture to produce a HDR image the integration needs to call ```markHdrBufferBind```
before binding the buffer to the graphics pipeline. In case the buffer contents is not overwritten before calling ```Present``` 
(or ```glSwapBuffers()```) it is fine to not call ```markHdrBufferFinished```. In case the same buffer is reused for other 
purposes and at the time the ```Present``` (or ```glSwapBuffers```) gets called its content does not represent the framebuffer 
calling ```markHdrBufferFinished``` at the moment where the buffer is not used anymore but before it is unbound from the graphics
pipeline is neccessary.

3. TAKING PICTURES WITH ANSEL
-----------------------------
### 3.1 ACTIVATING AND DEACTIVATING ANSEL
Players can start/stop Ansel session by pressing ALT+F2.

### 3.2 MOVING THE CAMERA
The camera can be moved via keys WASD and XZ for up/down. The camera can also be moved with the left stick on a gamepad 
and trigger buttons for up/down. Movement can be accelerated by holding SHIFT on keyboard or depressing right stick on 
gamepad.
### 3.3 ROTATING THE CAMERA
The yaw and pitch of the camera is directly controlled by mouse or right stick on gamepad. The roll of the camera is 
controlled via the user interface Roll slider.
### 3.4 APPLYING A FILTER
A number of filters can be selected via the Filter slider. Some filters, like Custom', have additional settings that 
can be used to adjust the filter even further.
### 3.5 TAKING A PICTURE
Ansel offers the following capture types (selected via the Capture type slider):
* Screenshot
* Highres
* 360
* Stereo
* 360 Stereo

Not all of these capture types may be available since it depends on the game integration and the current session (see 
sections 2.1 and 2.2). Once a type has been chosen the picture is taken by pressing Snap button. Some pictures may take 
significant time to produce, especially highres shots of large dimensions. If the game uses streaming the streaming 
performance may be affected when shots involving many parts are being stitched together.

> **NOTE:*** Not all filters are valid with multipart Capture types (360 and Highres). You may therefore see filters (or 
aspects of a filter) removed in the final picture.


4. TROUBLESHOOTING AND DEBUGGING COMMON PROBLEMS
------------------------------------------------
In this section we collected commonly occurring problems we've seen while integrating Ansel with games. This section can 
hopefully help you resolve a problem or two.
It is generally useful to be able to inspect the individual shot tiles that are captured when generating pictures that 
require multiple shots. Locate the `NvCameraConfiguration.exe` utility. It can be found inside 
Program Files\NVIDIA Corporation\Ansel\Tools.

Run the utility. A screen similar to this one should appear:

![NvCameraConfiguration utility](NvCameraConfiguration.exe_screen.png)

Check the 'Keep Intermediate Shots' option so that you can inspect the individual tiles. You can also pick a different 
location to store the tiles by changing the 'Temp Directory for Intermediate Shots'.

### 4.1 ARTEFACTS IN MULTIPART SHOTS
This is where we cover the most common errors we've seen while capturing multipart shots in games.
#### 4.1.1 Image tiles suffer from "acne"
This is probably best described with images. Here is a tile exhibiting the problem:

![Image acne](ImageAcne.jpg)

Note the "acne" to the left of the tree, caused by temporal anti-aliasing. This can be fixed in two different ways. One 
method is to use the captureSettleLatency field in the configuration object. This field specifies how many frames Ansel 
should wait when taking multipart shots for the frame to settle - i.e. for any temporal effects to have settled. In this 
specific case (where the image is taken from) it takes one frame so the value should be 1. This is what the image looks 
like with that setting:

![Image without acne](ImageWithoutAcne.jpg)

Another way to solve this problem is to harness the startCapture/stopCapture callbacks to disable temporal AA. This 
effectively disables the temporal AA feature during the multipart capture sequence.
Which solution should you use?  Well, it depends. You need to subjectively evaluate how much the temporal AA enhances 
image quality vs the cost of waiting for the frame to settle. At a settleLatency of 1 the cost is rather small and thus 
weighs in favor of using that solution.

#### 4.1.2 Ghosting everywhere in final picture
Usually it looks something like this:

![Ghosting everywhere](GhostingEverywhere.jpg)

Most often this is the result of incorrect field of view being submitted to Ansel - or error made on conversion or usage 
of value coming back from Ansel. It is recommended that you match the field of view type between game and Ansel to avoid 
any conversion mistakes. See section 2.1 on how you can configure Ansel to use the game's field of view.
#### 4.1.3 Screen space reflections fade out with increased Highres capture resolution
Below is a regular screenshot taken with Ansel:

![Screen space reflections](ScreenSpaceReflections.jpg)

If we now select capture type Highres with a large enough multiplier we get this picture (scaled down in resolution to 
fit this document):

![Screen space reflections reduced](ScreenSpaceReflectionsReduced.jpg)

There is unfortunately no workaround for this problem, it is a limitation of the capture method used.
#### 4.1.4 It's all a blur
Motion blur needs to be disabled during multipart capture. Otherwise results like this can be produced when taking 360 
picture:

![Blurry capture](Blur.jpg)

#### 4.1.5 Streaky reflections
When enabling highres capture for your game you may witness results similar to this:

![Streaky reflections in capture](StreakyReflections.jpg)

The reason may be that the projection offset and reduced field of view employed by the highres capture method is not 
being accounted for in the game's reflection code path.

### 4.2 THE VIEW OF THE WORLD "POPS" WHEN ENTERING AND EXITING ANSEL MODE
This is typically due to incorrect field of view being passed on the first frame or due to a screen space effect being 
disabled when Ansel mode is activated. For the latter it is preferred to deactivate troublesome effects only during 
multipart captures (via the capture callback).

### 4.3 ANSEL MOUSE CURSOR IS NOT BOUND BY GAME WINDOW
This commonly happens for a game that doesn't use a mousetrap or similar system to constrain the system mouse cursor 
from leaving the game window (when the game has focus). The solution is to pass the window handle during configuration, 
i.e. set the Configuration::gameWindowHandle to the window handle where the game is processing input messages.

### 4.4 DOUBLE MOUSE CURSORS
Strictly speaking UI and HUD elements must not be rendered when game is in Ansel mode and this includes any cursors. We 
have however experienced the situation where the game renders a mouse cursor on top of its window when it regains focus. 
If the game is in Ansel mode this will result in two moving mouse cursors - a very confusing experience indeed.  As 
mentioned the game shouldn't be rendering a mouse cursor when Ansel is active. That being said we have a mechanism to 
prevent this from happening - this mechanism is enabled if the game passes its window handle during configuration, in 
the Configuration::gameWindowHandle field.

### 4.5 ANSEL CANNOT BE ACTIVATED
Please consult the configuration requirements for Ansel listed in section 1.2. You can verify that Ansel is enabled by 
using the NvCameraConfiguration.exe utility that was introduced at the beginning of this chapter.  You can also disable 
whitelisting as outlined in section 1.2. Finally, setting a breakpoint on the startSessionCallback can be used to verify 
that Ansel is trying to start a session.

### 4.6 CAMERA ROTATION OR MOVEMENT IS INCORRECT
Incorrect rotation is best observed with a gamepad - i.e. pushing the joystick left doesn't rotate the view towards the 
left or pushing the joystick up doesn't rotate the view up.  Incorrect movement can be verified with either keyboard or 
gamepad. 

This problem is usually rooted in incorrect axes provided for right, up, and down directions in the 
`ansel::Configuration` struct.  See section 2.1.

### 4.7 CAMERA ROTATION OR MOVEMENT IS TOO SLOW / TOO FAST
The speed for rotation is set via the `rotationalSpeedInDegreesPerSecond` field during configuration.  The default value 
is 45 degrees/second. The speed for movement is set via the `translationalSpeedInWorldUnitsPerSecond` field during 
configuration.  The default value is 1 world unit/second.


APPENDIX A 
----------

### Notice

The information provided in this specification is believed to be accurate and reliable as of the date provided. However, 
NVIDIA Corporation ("NVIDIA") does not give any representations or warranties, expressed or implied, as to the accuracy or completeness of such information. NVIDIA shall have no liability for the consequences or use of such information or for any infringement of patents or other rights of third parties that may result from its use. This publication supersedes and replaces all other specifications for the product that may have been previously supplied.
NVIDIA reserves the right to make corrections, modifications, enhancements, improvements, and other changes to this specification, at any time and/or to discontinue any product or service without notice. Customer should obtain the latest relevant specification before placing orders and should verify that such information is current and complete. 
NVIDIA products are sold subject to the NVIDIA standard terms and conditions of sale supplied at the time of order acknowledgement, unless otherwise agreed in an individual sales agreement signed by authorized representatives of NVIDIA and customer. NVIDIA hereby expressly objects to applying any customer general terms and conditions with regard to the purchase of the NVIDIA product referenced in this specification.
NVIDIA products are not designed, authorized or warranted to be suitable for use in medical, military, aircraft, space or life support equipment, nor in applications where failure or malfunction of the NVIDIA product can reasonably be expected to result in personal injury, death or property or environmental damage. NVIDIA accepts no liability for inclusion and/or use of NVIDIA products in such equipment or applications and therefore such inclusion and/or use is at customer's own risk. 
NVIDIA makes no representation or warranty that products based on these specifications will be suitable for any specified use without further testing or modification. Testing of all parameters of each product is not necessarily performed by NVIDIA. It is customer's sole responsibility to ensure the product is suitable and fit for the application planned by customer and to do the necessary testing for the application in order to avoid a default of the application or the product. Weaknesses in customer's product designs may affect the quality and reliability of the NVIDIA product and may result in additional or different conditions and/or requirements beyond those contained in this specification. NVIDIA does not accept any liability related to any default, damage, costs or problem which may be based on or attributable to: (i) the use of the NVIDIA product in any manner that is contrary to this specification, or (ii) customer product designs. 
No license, either expressed or implied, is granted under any NVIDIA patent right, copyright, or other NVIDIA intellectual property right under this specification. Information published by NVIDIA regarding third-party products or services does not constitute a license from NVIDIA to use such products or services or a warranty or endorsement thereof. Use of such information may require a license from a third party under the patents or other intellectual property rights of the third party, or a license from NVIDIA under the patents or other intellectual property rights of NVIDIA. Reproduction of information in this specification is permissible only if reproduction is approved by NVIDIA in writing, is reproduced without alteration, and is accompanied by all associated conditions, limitations, and notices. 
ALL NVIDIA DESIGN SPECIFICATIONS, REFERENCE BOARDS, FILES, DRAWINGS, DIAGNOSTICS, LISTS, AND OTHER DOCUMENTS (TOGETHER AND SEPARATELY, "MATERIALS") ARE BEING PROVIDED "AS IS." NVIDIA MAKES NO WARRANTIES, EXPRESSED, IMPLIED, STATUTORY, OR OTHERWISE WITH RESPECT TO THE MATERIALS, AND EXPRESSLY DISCLAIMS ALL IMPLIED WARRANTIES OF NONINFRINGEMENT, MERCHANTABILITY, AND FITNESS FOR A PARTICULAR PURPOSE. Notwithstanding any damages that customer might incur for any reason whatsoever, NVIDIA's aggregate and cumulative liability towards customer for the products described herein shall be limited in accordance with the NVIDIA terms and conditions of sale for the product.

VESA DisplayPort
DisplayPort and DisplayPort Compliance Logo, DisplayPort Compliance Logo for Dual-mode Sources, and DisplayPort Compliance Logo for Active Cables are trademarks owned by the Video Electronics Standards Association in the United States and other countries.

HDMI
HDMI, the HDMI logo, and High-Definition Multimedia Interface are trademarks or registered trademarks of HDMI Licensing LLC.

ROVI Compliance Statement
NVIDIA Products that support Rovi Corporation's Revision 7.1.L1 Anti-Copy Process (ACP) encoding technology can only be sold or distributed to buyers with a valid and existing authorization from ROVI to purchase and incorporate the device into buyer's products.
This device is protected by U.S. patent numbers 6,516,132; 5,583,936; 6,836,549; 7,050,698; and 7,492,896 and other intellectual property rights.  The use of ROVI Corporation's copy protection technology in the device must be authorized by ROVI Corporation and is intended for home and other limited pay-per-view uses only, unless otherwise authorized in writing by ROVI Corporation.  Reverse engineering or disassembly is prohibited.

OpenCL
OpenCL is a trademark of Apple Inc. used under license to the Khronos Group Inc.

Trademarks
NVIDIA and the NVIDIA logo are trademarks and/or registered trademarks of NVIDIA Corporation in the U.S. and other countries. Other company and product names may be trademarks of the respective companies with which they are associated.

Copyright 
(c) 2016 NVIDIA Corporation. All rights reserved.  


www.nvidia.com

