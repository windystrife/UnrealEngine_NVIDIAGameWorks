/************************************************************************************

Filename    :   VrApi_Input.h
Content     :   
Created     :   Feb 9, 2016
Authors     : 

Copyright   :   Copyright 2015 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/
#ifndef OVR_VrApi_Input_h
#define OVR_VrApi_Input_h

///--BEGIN_SDK_REMOVE
#if !defined( ANDROID )

// NOTE: Everything in this section should stay in sync with CAPI.

#include "VrApi_Config.h"
#include "VrApi_Types.h"
#include <stdint.h>

// Describes button input types.
// Button inputs are combined; that is they will be reported as pressed if they are 
// pressed on either one of the two devices.
// The ovrButton_Up/Down/Left/Right map to both XBox D-Pad and directional buttons.
// The ovrButton_Enter and ovrButton_Return map to Start and Back controller buttons, respectively.
typedef enum ovrButton_
{
	ovrButton_A         = 0x00000001, /// A button on XBox controllers and right Touch controller. Select button on Oculus Remote.
	ovrButton_B         = 0x00000002, /// B button on XBox controllers and right Touch controller. Back button on Oculus Remote.
	ovrButton_RThumb    = 0x00000004, /// Right thumbstick on XBox controllers and Touch controllers. Not present on Oculus Remote.
	ovrButton_RShoulder = 0x00000008, /// Right shoulder button on XBox controllers. Not present on Touch controllers or Oculus Remote.

	ovrButton_X         = 0x00000100,  /// X button on XBox controllers and left Touch controller. Not present on Oculus Remote.
	ovrButton_Y         = 0x00000200,  /// Y button on XBox controllers and left Touch controller. Not present on Oculus Remote.
	ovrButton_LThumb    = 0x00000400,  /// Left thumbstick on XBox controllers and Touch controllers. Not present on Oculus Remote.
	ovrButton_LShoulder = 0x00000800,  /// Left shoulder button on XBox controllers. Not present on Touch controllers or Oculus Remote.

	ovrButton_Up        = 0x00010000,  /// Up button on XBox controllers and Oculus Remote. Not present on Touch controllers.
	ovrButton_Down      = 0x00020000,  /// Down button on XBox controllers and Oculus Remote. Not present on Touch controllers.
	ovrButton_Left      = 0x00040000,  /// Left button on XBox controllers and Oculus Remote. Not present on Touch controllers.
	ovrButton_Right     = 0x00080000,  /// Right button on XBox controllers and Oculus Remote. Not present on Touch controllers.
	ovrButton_Enter     = 0x00100000,  /// Start on XBox 360 controller. Menu on XBox One controller and Left Touch controller. Should be referred to as the Menu button in user-facing documentation.
	ovrButton_Back      = 0x00200000,  /// Back on Xbox 360 controller. View button on XBox One controller. Not present on Touch controllers or Oculus Remote.
	ovrButton_VolUp     = 0x00400000,  /// Volume button on Oculus Remote. Not present on XBox or Touch controllers.
	ovrButton_VolDown   = 0x00800000,  /// Volume button on Oculus Remote. Not present on XBox or Touch controllers.
	ovrButton_Home      = 0x01000000,  /// Home button on XBox controllers. Oculus button on Touch controllers and Oculus Remote.

	// Bit mask of all buttons that are for private usage by Oculus
	ovrButton_Private   = ovrButton_VolUp | ovrButton_VolDown | ovrButton_Home,

	// Bit mask of all buttons on the right Touch controller
	ovrButton_RMask = ovrButton_A | ovrButton_B | ovrButton_RThumb | ovrButton_RShoulder,

	// Bit mask of all buttons on the left Touch controller
	ovrButton_LMask = ovrButton_X | ovrButton_Y | ovrButton_LThumb | ovrButton_LShoulder |
						ovrButton_Enter,

	ovrButton_EnumSize  = 0x7fffffff
} ovrButton;

// Describes touch input types.
// These values map to capacitive touch values reported ovrInputState::Touch.
// Some of these values are mapped to button bits for consistency.
typedef enum ovrTouch_
{
	ovrTouch_A              = ovrButton_A,
	ovrTouch_B              = ovrButton_B,
	ovrTouch_RThumb         = ovrButton_RThumb,
	ovrTouch_RThumbRest     = 0x00000008,
	ovrTouch_RIndexTrigger  = 0x00000010,

	// Bit mask of all the button touches on the right controller
	ovrTouch_RButtonMask    = ovrTouch_A | ovrTouch_B | ovrTouch_RThumb | ovrTouch_RThumbRest | ovrTouch_RIndexTrigger,

	ovrTouch_X              = ovrButton_X,
	ovrTouch_Y              = ovrButton_Y,
	ovrTouch_LThumb         = ovrButton_LThumb,
	ovrTouch_LThumbRest     = 0x00000800,
	ovrTouch_LIndexTrigger  = 0x00001000,

	// Bit mask of all the button touches on the left controller
	ovrTouch_LButtonMask    = ovrTouch_X | ovrTouch_Y | ovrTouch_LThumb | ovrTouch_LThumbRest | ovrTouch_LIndexTrigger,

	// Finger pose state
	// Derived internally based on distance, proximity to sensors and filtering.
	ovrTouch_RIndexPointing = 0x00000020,
	ovrTouch_RThumbUp       = 0x00000040,
	ovrTouch_LIndexPointing = 0x00002000,
	ovrTouch_LThumbUp       = 0x00004000,

	// Bit mask of all right controller poses
	ovrTouch_RPoseMask      = ovrTouch_RIndexPointing | ovrTouch_RThumbUp,

	// Bit mask of all left controller poses
	ovrTouch_LPoseMask      = ovrTouch_LIndexPointing | ovrTouch_LThumbUp,

	ovrTouch_EnumSize
} ovrTouch;

// Specifies which controller is connected; multiple can be connected at once.
typedef enum ovrControllerType_
{
	ovrControllerType_None      = 0x00,
	ovrControllerType_LTouch    = 0x01,
	ovrControllerType_RTouch    = 0x02,
	ovrControllerType_Touch     = 0x03,
	ovrControllerType_Remote    = 0x04,
#if 1 // NOTE: This is not part of CAPI and is only added to ease porting.
	ovrControllerType_Headset	= 0x08,
#endif
	ovrControllerType_XBox      = 0x10,

	ovrControllerType_Active    = 0xff,      //< Operate on or query whichever controller is active.

	ovrControllerType_EnumSize  = 0x7fffffff
} ovrControllerType;

// Provides names for the left and right hand array indexes.
typedef enum ovrHandType_
{
	ovrHand_Left  = 0,
	ovrHand_Right = 1,
	ovrHand_Count = 2,
	ovrHand_EnumSize = 0x7fffffff
} ovrHandType;

#if 1 // NOTE: This is not part of CAPI and is only added to ease porting.
typedef uint32_t ovrDeviceID;
#endif

typedef enum ovrDeviceIdType_
{
	ovrDeviceIdType_Invalid		= 0x7fffffff
} ovrDeviceIdType;

// ovrInputState describes the complete controller input state, including Oculus Touch,
// and XBox gamepad. If multiple inputs are connected and used at the same time,
// their inputs are combined.
typedef struct ovrInputState_
{
	// System type when the controller state was last updated.
	double              TimeInSeconds;

	// Values for buttons described by ovrButton.
	unsigned int        Buttons;

	// Touch values for buttons and sensors as described by ovrTouch.
	unsigned int        Touches;

	// Left and right finger trigger values (ovrHand_Left and ovrHand_Right), in the range 0.0 to 1.0f.
	// Returns 0 if the value would otherwise be less than 0.1176, for ovrControllerType_XBox.
	// This has been formally named simply "Trigger". We retain the name IndexTrigger for backwards code compatibility.
	// User-facing documentation should refer to it as the Trigger.
	float               IndexTrigger[ovrHand_Count];

	// Left and right hand trigger values (ovrHand_Left and ovrHand_Right), in the range 0.0 to 1.0f.
	// This has been formally named "Grip Button". We retain the name HandTrigger for backwards code compatibility.
	// User-facing documentation should refer to it as the Grip Button or simply Grip.
	float               HandTrigger[ovrHand_Count];

	// Horizontal and vertical thumbstick axis values (ovrHand_Left and ovrHand_Right), in the range -1.0f to 1.0f.
	// Returns a deadzone (value 0) per each axis if the value on that axis would otherwise have been between -.2746 to +.2746, for ovrControllerType_XBox
	ovrVector2f         Thumbstick[ovrHand_Count];

	// The type of the controller this state is for.
	ovrControllerType   ControllerType;

	// Left and right finger trigger values (ovrHand_Left and ovrHand_Right), in the range 0.0 to 1.0f.
	// Does not apply a deadzone.  Only touch applies a filter.
	// This has been formally named simply "Trigger". We retain the name IndexTrigger for backwards code compatibility.
	// User-facing documentation should refer to it as the Trigger.
	// Added in 1.7
	float               IndexTriggerNoDeadzone[ovrHand_Count];

	// Left and right hand trigger values (ovrHand_Left and ovrHand_Right), in the range 0.0 to 1.0f.
	// Does not apply a deadzone. Only touch applies a filter.
	// This has been formally named "Grip Button". We retain the name HandTrigger for backwards code compatibility.
	// User-facing documentation should refer to it as the Grip Button or simply Grip.
	// Added in 1.7
	float               HandTriggerNoDeadzone[ovrHand_Count];

	// Horizontal and vertical thumbstick axis values (ovrHand_Left and ovrHand_Right), in the range -1.0f to 1.0f
	// Does not apply a deadzone or filter.
	// Added in 1.7
	ovrVector2f         ThumbstickNoDeadzone[ovrHand_Count];

	OVR_VRAPI_PADDING( 4 );
} ovrInputState;

OVR_VRAPI_ASSERT_TYPE_SIZE( ovrInputState, 88 );

#if defined( __cplusplus )
extern "C" {
#endif

//-----------------------------------------------------------------
// Input - Currently only supported for PC
//-----------------------------------------------------------------

// Returns the most recent input state for controllers, without positional tracking info.
OVR_VRAPI_EXPORT ovrResult vrapi_GetInputState( ovrControllerType controllerType, ovrInputState * inputState );

// Returns controller types connected to the system OR'ed together.
OVR_VRAPI_EXPORT unsigned int vrapi_GetConnectedControllerTypes();

// Turns on vibration of the given controller.
//
// To disable vibration, call ovr_SetControllerVibration with an amplitude of 0.
// Vibration automatically stops after a nominal amount of time, so if you want vibration 
// to be continuous over multiple seconds then you need to call this function periodically.
OVR_VRAPI_EXPORT ovrResult vrapi_SetControllerVibration( ovrControllerType controllerType, float frequency, float amplitude );

#if defined( __cplusplus )
}	// extern "C"
#endif

#else
///--END_SDK_REMOVE

#include <stddef.h>
#include <stdint.h>
#include "VrApi_Config.h"
#include "VrApi_Types.h"

// Describes button input types.
// Only the following Button types are reported to the application in this release,
//
// ovrButton_Back, ovrButton_Home, ovrButton_A, ovrButton_Enter, ovrButton_VolUp, ovrButton_VolDown
//
typedef enum ovrButton_
{
	ovrButton_A         = 0x00000001,
	ovrButton_B         = 0x00000002,
	ovrButton_RThumb    = 0x00000004,
	ovrButton_RShoulder = 0x00000008,

	ovrButton_X         = 0x00000100,
	ovrButton_Y         = 0x00000200,
	ovrButton_LThumb    = 0x00000400,
	ovrButton_LShoulder = 0x00000800,

	ovrButton_Up        = 0x00010000,
	ovrButton_Down      = 0x00020000,
	ovrButton_Left      = 0x00040000,
	ovrButton_Right     = 0x00080000,
	ovrButton_Enter     = 0x00100000,
	ovrButton_Back      = 0x00200000,
///--BEGIN_SDK_REMOVE
	ovrButton_VolUp     = 0x00400000,
	ovrButton_VolDown   = 0x00800000,
	ovrButton_Home      = 0x01000000,
///--END_SDK_REMOVE

	ovrButton_EnumSize  = 0x7fffffff
} ovrButton;

// Specifies which controller is connected; multiple can be connected at once.
typedef enum ovrControllerType_
{
	ovrControllerType_None			= 0,
	ovrControllerType_Reserved0		= ( 1 << 0 ),	// LTouch in CAPI
	ovrControllerType_Reserved1		= ( 1 << 1 ),	// RTouch in CAPI
	ovrControllerType_TrackedRemote	= ( 1 << 2 ),
	ovrControllerType_Headset		= ( 1 << 3 ),
	ovrControllerType_Reserved2		= ( 1 << 4 ),	// Xbox in CAPI

	ovrControllerType_EnumSize		= 0x7fffffff
} ovrControllerType;

typedef uint32_t ovrDeviceID;

typedef enum ovrDeviceIdType_
{
	ovrDeviceIdType_Invalid	= 0x7fffffff
} ovrDeviceIdType;

// This header starts all ovrInputCapabilities structures. It should only hold fields
// that are common to all input controllers.
typedef struct ovrInputCapabilityHeader_
{
	ovrControllerType	Type;

	// A unique ID for the input device
	ovrDeviceID			DeviceID;
} ovrInputCapabilityHeader;

// specifies capabilites of a controller
// Note that left and right hand are non-exclusive (a two-handed controller could set both)
typedef enum ovrControllerCapabilities_
{
	ovrControllerCaps_HasOrientationTracking 	= 0x00000001,
	ovrControllerCaps_HasPositionTracking 		= 0x00000002,
	ovrControllerCaps_LeftHand					= 0x00000004,	// controller is configured for left hand
	ovrControllerCaps_RightHand					= 0x00000008,	// controller is configured for right hand

	ovrControllerCaps_EnumSize 					= 0x7fffffff
} ovrControllerCapabilties;

typedef struct ovrInputTrackedRemoteCapabilities_
{
	ovrInputCapabilityHeader	Header;

	// mask of controller capabilities described by ovrControllerCapabilities
	uint32_t					ControllerCapabilities;

	// mask of button capabilities described by ovrButton
	uint32_t					ButtonCapabilities;

	// Maximum coordinates of the Trackpad, bottom right exclusive
	// For a 300x200 Trackpad, return 299x199
	uint16_t					TrackpadMaxX;
	uint16_t					TrackpadMaxY;

	// Size of the Trackpad in mm (millimeters)
	float						TrackpadSizeX;
	float						TrackpadSizeY;
} ovrInputTrackedRemoteCapabilities;

// Capabilities for the Head Mounted Tracking device (i.e. the headset).
// Note that the GearVR headset firmware always sends relative coordinates 
// with the initial touch position offset by (1280,720). There is no way
// to get purely raw coordinates from the headset. In addition, these 
// coordinates get adjusted for acceleration resulting in a slow movement 
// from one edge to the other the having a coordinate range of about 300 
// units, while a fast movement from edge to edge may result in a range
// close to 900 units.
// This means the headset touchpad needs to be handled differently than 
// the GearVR Controller touchpad.
typedef struct ovrInputHeadsetCapabilities_
{
	ovrInputCapabilityHeader	Header;

	// mask of controller capabilities described by ovrControllerCapabilities
	uint32_t					ControllerCapabilities;

	// mask of button capabilities described by ovrButton
	uint32_t					ButtonCapabilities;

	// Maximum coordinates of the Trackpad, bottom right exclusive
	// For a 300x200 Trackpad, return 299x199
	uint16_t					TrackpadMaxX;
	uint16_t					TrackpadMaxY;

	// Size of the Trackpad in mm (millimeters)
	float						TrackpadSizeX;
	float						TrackpadSizeY;
} ovrInputHeadsetCapabilities;

// This header starts all ovrInputState structures. It should only hold fields
// that are common to all input controllers.
typedef struct ovrInputStateHeader_
{
	// type type of controller
	ovrControllerType 	ControllerType;

	// System time when the controller state was last updated.
	double				TimeInSeconds;
} ovrInputStateHeader;

// ovrInputStateTrackedRemote describes the complete input state for the 
// orientation-tracked remote. The TrackpadPosition coordinates returned 
// for the GearVR Controller are in raw, absolute units.
typedef struct ovrInputStateTrackedRemote_
{
	ovrInputStateHeader Header;

	// Values for buttons described by ovrButton.
	uint32_t	        Buttons;

	// finger contact status for trackpad
	// true = finger is on trackpad, false = finger is off trackpad
	uint32_t            TrackpadStatus;

	// X and Y coordinates of the Trackpad
	ovrVector2f         TrackpadPosition;

	// The percentage of max battery charge remaining.
	uint8_t				BatteryPercentRemaining;	
	// Increments every time the remote is recentered. If this changes, the application may need
	// to adjust its arm model accordingly.
	uint8_t				RecenterCount;				
	// Reserved for future use.
	uint16_t			Reserved;
} ovrInputStateTrackedRemote;

// ovrInputStateHeadset describes the complete input state for the 
// GearVR headset. The TrackpadPosition coordinates return for the
// headset are relative coordinates, centered at (1280,720). See the
// comments on ovrInputHeadsetCapabilities for more information.
typedef struct ovrInputStateHeadset_
{
	ovrInputStateHeader	Header;

	// Values for buttons described by ovrButton.
	uint32_t	        Buttons;

	// finger contact status for trackpad
	// true = finger is on trackpad, false = finger is off trackpad
	uint32_t            TrackpadStatus;

	// X and Y coordinates of the Trackpad
	ovrVector2f         TrackpadPosition;
} ovrInputStateHeadset;

#if defined( __cplusplus )
extern "C" {
#endif

// Enumerates the input devices connected to the system
// Start with index=0 and counting up. Stop when ovrResult is < 0
//
// Input: ovrMobile, device index, and a capabilities header
// The capabilities header does not need to have any fields set before calling.
// Output: capabilitiesHeader with information for that enumeration index
OVR_VRAPI_EXPORT ovrResult vrapi_EnumerateInputDevices( ovrMobile * ovr, const uint32_t index, ovrInputCapabilityHeader * capsHeader );

// Returns the capabilities of the input device for the corresponding device ID
//
// Input: ovr, pointer to a capabilities structure
// Output: capabilities will be filled with information for the deviceID
// Example:
//     The Type field of the capabilitiesHeader must be set when calling this function.
//     Normally the capabilitiesHeader is obtained from the vrapi_EnumerateInputDevices API
//     The Type field in the header should match the structure type that is passed.
//
//         ovrInputCapabilityHeader capsHeader;
//         if ( vrapi_EnumerateInputDevices( ovr, deviceIndex, &capsHeader ) >= 0 ) {
//             if ( capsHeader.Type == ovrDeviceType_TrackedRemote ) {
//                 ovrInputTrackedRemoteCapabilities remoteCaps;
//                 remoteCaps.Header = capsHeader;
//                 vrapi_GetInputDeviceCapabilities( ovr, &remoteCaps.Header );
OVR_VRAPI_EXPORT ovrResult vrapi_GetInputDeviceCapabilities( ovrMobile * ovr, ovrInputCapabilityHeader * capsHeader );

// Returns the current input state for controllers, without positional tracking info.
//
// Input: ovr, deviceID, pointer to a capabilities structure (with Type field set)
// Output: Upon return the inputState structure will be set to the device's current input state
// Example:
//     The Type field of the passed ovrInputStateHeader must be set to the type that
//     corresponds to the type of structure being passed.
//     The pointer to the ovrInputStateHeader should be a pointer to a Header field in
//     structure matching the value of the Type field.
// 
//     ovrInputStateTrackedRemote state;
//     state.Header.Type = ovrControllerType_TrackedRemote;
//     if ( vrapi_GetCurrentInputState( ovr, remoteDeviceID, &state.Header ) >= 0 ) {
OVR_VRAPI_EXPORT ovrResult vrapi_GetCurrentInputState( ovrMobile * ovr, const ovrDeviceID deviceID, ovrInputStateHeader * inputState );

// Returns the predicted input state based on the specified absolute system time
// in seconds. Pass absTime value of 0.0 to request the most recent sensor reading.
// Input: ovr, device ID, prediction time
// Output: ovrTracking structure containing the device's predicted tracking state.
OVR_VRAPI_EXPORT ovrResult vrapi_GetInputTrackingState( ovrMobile * ovr, const ovrDeviceID deviceID, 
														const double absTimeInSeconds, ovrTracking * tracking );

// Can be called from any thread while in VR mode. Recenters the tracked remote to the current yaw of the headset.
// Input: ovr, device ID
// Output: None
OVR_VRAPI_EXPORT void vrapi_RecenterInputPose( ovrMobile * ovr, const ovrDeviceID deviceID );

// Enable or disable emulation for the GearVR Controller. 
// Emulation is on by default.
// If emulationOn == true, then button and touch events on the GearVR Controller will be sent through the Android
// dispatchKeyEvent and dispatchTouchEvent path as if they were from the headset buttons and touchpad.
// Applications that are intentionally enumerating the controller will likely want to turn emulation off in order
// to differentiate between controller and headset input events.
OVR_VRAPI_EXPORT ovrResult vrapi_SetRemoteEmulation( ovrMobile * ovr, const bool emulationOn );

#if defined( __cplusplus )
}   // extern "C"
#endif

///--BEGIN_SDK_REMOVE
#endif // !defined( ANDROID )
///--END_SDK_REMOVE

#endif	// OVR_VrApi_Input_h
