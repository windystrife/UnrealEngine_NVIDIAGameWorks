// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if PLATFORM_WINDOWS

#include "WindowsHWrapper.h"

#include "AllowWindowsPlatformTypes.h"

THIRD_PARTY_INCLUDES_START
#include <Audiopolicy.h>
#include <Mmdeviceapi.h>
#include <Functiondiscoverykeys_devpkey.h>
#include <audiodefs.h>
#include <dsound.h>
THIRD_PARTY_INCLUDES_END

#include "HideWindowsPlatformTypes.h"

#endif // PLATFORM_WINDOWS

#define ANDROIDVOICE_SUPPORTED_PLATFORMS (PLATFORM_ANDROID && (PLATFORM_ANDROID_ARM || PLATFORM_ANDROID_ARM64 || PLATFORM_ANDROID_X64))
#define PLATFORM_SUPPORTS_VOICE_CAPTURE (PLATFORM_WINDOWS || PLATFORM_MAC || ANDROIDVOICE_SUPPORTED_PLATFORMS)

// Module includes
