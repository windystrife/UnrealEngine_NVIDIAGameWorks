// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Logging/LogMacros.h"

#define MFMEDIAFACTORY_WINDOWS (PLATFORM_WINDOWS && (WINVER >= 0x601 /*Win7*/))


/** Log category for the MfMediaFactory module. */
DECLARE_LOG_CATEGORY_EXTERN(LogMfMediaFactory, Log, All);
