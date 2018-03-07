// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreTypes.h"

#include "GenericPlatform/GenericPlatformMallocCrash.h"

#if PLATFORM_USES_STACKBASED_MALLOC_CRASH
typedef FGenericStackBasedMallocCrash FPlatformMallocCrash;
#else
typedef FGenericPlatformMallocCrash FPlatformMallocCrash;
#endif
