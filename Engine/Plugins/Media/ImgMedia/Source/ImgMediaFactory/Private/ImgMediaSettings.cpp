// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "ImgMediaSettings.h"


/* UImgMediaSettings structors
 *****************************************************************************/

UImgMediaSettings::UImgMediaSettings()
	: DefaultFps(24.0f)
	, CacheBehindPercentage(25)
	, CacheSizeGB(1.0f)
	, ExrDecoderThreads(0)
	, DefaultProxy(TEXT("proxy"))
	, UseDefaultProxy(false)
{ }
