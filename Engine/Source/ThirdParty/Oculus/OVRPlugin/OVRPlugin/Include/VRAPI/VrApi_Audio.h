/************************************************************************************

Filename    :   VrApi_Audio.h
Content     :   
Created     :   Feb 9, 2016
Authors     : 
Language    :   C99

Copyright   :   Copyright 2015 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/
#ifndef OVR_VrApi_Audio_h
#define OVR_VrApi_Audio_h

///--BEGIN_SDK_REMOVE

#include "VrApi_Config.h"
#include "VrApi_Types.h"

#if defined( _WIN32 )

#define OVR_AUDIO_MAX_DEVICE_STR_SIZE 128

#if defined( __cplusplus )
extern "C" {
#endif

//struct GUID;

//-----------------------------------------------------------------
// Audio - Currently only supported for Windows
//-----------------------------------------------------------------

// Gets the ID of the preferred VR audio output device.
OVR_VRAPI_EXPORT ovrResult vrapi_GetAudioDeviceOutWaveId( unsigned int * deviceOutId );

// Gets the ID of the preferred VR audio input device.
OVR_VRAPI_EXPORT ovrResult vrapi_GetAudioDeviceInWaveId( unsigned int * deviceInId );

// Gets the GUID of the preferred VR audio device as a string.
OVR_VRAPI_EXPORT ovrResult vrapi_GetAudioDeviceOutGuidStr( wchar_t deviceOutStrBuffer[OVR_AUDIO_MAX_DEVICE_STR_SIZE] );

// Gets the GUID of the preferred VR audio device.
//OVR_VRAPI_EXPORT ovrResult vrapi_GetAudioDeviceOutGuid( struct GUID * deviceOutGuid );

// Gets the GUID of the preferred VR microphone device as a string.
OVR_VRAPI_EXPORT ovrResult vrapi_GetAudioDeviceInGuidStr( wchar_t deviceInStrBuffer[OVR_AUDIO_MAX_DEVICE_STR_SIZE] );

// Gets the GUID of the preferred VR microphone device.
//OVR_VRAPI_EXPORT ovrResult vrapi_GetAudioDeviceInGuid( struct GUID * deviceInGuid );

#if defined( __cplusplus )
}	// extern "C"
#endif

#endif	// _WIN32

///--END_SDK_REMOVE

#endif	// OVR_VrApi_Audio_h
