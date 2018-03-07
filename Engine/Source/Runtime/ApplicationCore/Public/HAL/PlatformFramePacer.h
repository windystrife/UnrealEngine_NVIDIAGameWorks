// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreTypes.h"

#if PLATFORM_IOS
#include "IOS/IOSPlatformFramePacer.h"
#elif PLATFORM_MAC
#include "Mac/MacPlatformFramePacer.h"
#else
#include "GenericPlatform/GenericPlatformFramePacer.h"
typedef FGenericPlatformRHIFramePacer FPlatformRHIFramePacer;
#endif
