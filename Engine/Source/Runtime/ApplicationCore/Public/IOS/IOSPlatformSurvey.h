// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


/*=============================================================================================
	IOSPlatformSurvey.h: Apple iOS platform hardware-survey classes
==============================================================================================*/

#pragma once
#include "GenericPlatform/GenericPlatformSurvey.h"

/**
* iOS implementation of FGenericPlatformSurvey
**/
struct FIOSPlatformSurvey : public FGenericPlatformSurvey
{
	static bool GetSurveyResults(FHardwareSurveyResults& OutResults, bool bWait = false);
};

typedef FIOSPlatformSurvey FPlatformSurvey;