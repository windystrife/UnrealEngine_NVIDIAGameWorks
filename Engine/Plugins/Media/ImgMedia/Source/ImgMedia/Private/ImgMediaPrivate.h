// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ImgMediaSettings.h"
#include "Logging/LogMacros.h"
#include "UObject/NameTypes.h"


/** The OpenEXR image reader is supported on macOS and Windows only. */
#define IMGMEDIA_EXR_SUPPORTED_PLATFORM (PLATFORM_MAC || PLATFORM_WINDOWS)

/** Default number of frames per second for image sequences. */
#define IMGMEDIA_DEFAULT_FPS 24.0

/** Log category for the this module. */
DECLARE_LOG_CATEGORY_EXTERN(LogImgMedia, Log, All);


namespace ImgMedia
{
	/** Name of the FramesPerSecondAttribute media option. */
	static FName FramesPerSecondAttributeOption("FramesPerSecondAttribute");

	/** Name of the FramesPerSecondOverride media option. */
	static FName FramesPerSecondOverrideOption("FramesPerSecondOverride");

	/** Name of the ProxyOverride media option. */
	static FName ProxyOverrideOption("ProxyOverride");
}
