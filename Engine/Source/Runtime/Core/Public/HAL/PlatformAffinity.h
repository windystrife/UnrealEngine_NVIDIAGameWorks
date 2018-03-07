// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"

#if PLATFORM_XBOXONE
#include "XboxOne/XBoxOneAffinity.h"
#elif PLATFORM_PS4
#include "PS4/PS4Affinity.h"
#elif PLATFORM_IOS
#include "IOS/IOSPlatformAffinity.h"
#elif PLATFORM_MAC
#include "Mac/MacPlatformAffinity.h"
#elif PLATFORM_ANDROID
#include "Android/AndroidAffinity.h"
#elif PLATFORM_SWITCH
#include "Switch/SwitchPlatformAffinity.h"
#else
#include "GenericPlatform/GenericPlatformAffinity.h"
typedef FGenericPlatformAffinity FPlatformAffinity;
#endif
