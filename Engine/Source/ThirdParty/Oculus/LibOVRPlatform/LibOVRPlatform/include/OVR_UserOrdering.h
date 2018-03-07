// This file was @generated with LibOVRPlatform/codegen/main. Do not modify it!

#ifndef OVR_USER_ORDERING_H
#define OVR_USER_ORDERING_H

#include "OVR_Platform_Defs.h"

typedef enum ovrUserOrdering_ {
  ovrUserOrdering_Unknown,
  ovrUserOrdering_None,
  ovrUserOrdering_PresenceAlphabetical,
} ovrUserOrdering;

/// Converts an ovrUserOrdering enum value to a string
/// The returned string does not need to be freed
OVRPL_PUBLIC_FUNCTION(const char*) ovrUserOrdering_ToString(ovrUserOrdering value);

/// Converts a string representing an ovrUserOrdering enum value to an enum value
///
OVRPL_PUBLIC_FUNCTION(ovrUserOrdering) ovrUserOrdering_FromString(const char* str);

#endif
