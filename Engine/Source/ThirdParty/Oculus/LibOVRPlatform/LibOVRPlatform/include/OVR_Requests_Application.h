// This file was @generated with LibOVRPlatform/codegen/main. Do not modify it!

#ifndef OVR_REQUESTS_APPLICATION_H
#define OVR_REQUESTS_APPLICATION_H

#include "OVR_Types.h"
#include "OVR_Platform_Defs.h"

#include "OVR_ApplicationVersion.h"

/// \file
/// *** Application Overview:
/// An application is what you're writing! These requests/methods will allow you to
/// pull basic metadata about your application.

/// Requests version information, including the currently installed and latest
/// available version name and version code.
///
/// A message with type ::ovrMessage_Application_GetVersion will be generated in response.
///
/// First call ::ovr_Message_IsError() to check if an error occurred.
///
/// If no error occurred, the message will contain a payload of type ::ovrApplicationVersionHandle.
/// Extract the payload from the message handle with ::ovr_Message_GetApplicationVersion().
OVRP_PUBLIC_FUNCTION(ovrRequest) ovr_Application_GetVersion();

#endif
