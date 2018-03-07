// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


/*=============================================================================================
	MacPlatformSurvey.h: Apple Mac OSX platform hardware-survey classes
==============================================================================================*/

#pragma once
#include "GenericPlatform/GenericPlatformSurvey.h"

class FString;

/**
* Mac implementation of FGenericPlatformSurvey
**/
struct FMacPlatformSurvey : public FGenericPlatformSurvey
{
	/** Start, or check on, the hardware survey */
	static bool GetSurveyResults( FHardwareSurveyResults& OutResults, bool bWait = false );

private:
	/** Start run the hardware survey now but return immediately without waiting for results **/
	static void BeginSurveyHardware();

	/** Check to see if in-progress survey started with BeginSurveyHardware() has completed and return results **/
	static void TickSurveyHardware( FHardwareSurveyResults& Results );

	/**
	 * Safely write strings into the fixed length TCHAR buffers of a FHardwareSurveyResults member
	 */
	static void WriteFStringToResults(TCHAR* OutBuffer, const FString& InString);

	static bool bSurveyPending;
	static bool bSurveyComplete;
	static bool bSurveyFailed;
	static double SurveyStartTimeSeconds;
	static FHardwareSurveyResults Results;
};

typedef FMacPlatformSurvey FPlatformSurvey;
