// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "GenericPlatform/GenericPlatformSurvey.h"


/**
 * Windows implementation of FGenericPlatformSurvey.
 */
struct APPLICATIONCORE_API FWindowsPlatformSurvey
	: public FGenericPlatformSurvey
{
	/** Start, or check on, the hardware survey */
	static bool GetSurveyResults( FHardwareSurveyResults& OutResults, bool bWait = false );

private:

	/** Start run the hardware survey now but return immediately without waiting for results **/
	static void BeginSurveyHardware();

	/** Check to see if in-progress survey started with BeginSurveyHardware() has completed and return results **/
	static void TickSurveyHardware( FHardwareSurveyResults& Results );

	/**
	 * Helper returns the dxdiag output text filepath used by the survey
	 */
	static class FString GetDxDiagOutputFilepath();

	/**
	 * Helper used by SurveyHardware to get the Windows Experience Index score for individual sub-components
	 * @return True if successful
	 */
	static bool GetSubComponentIndex( struct IProvideWinSATResultsInfo* WinSATResults, FHardwareSurveyResults& OutSurveyResults, int32 SubComponent, float& OutSubComponentIndex );

	/**
	 * Find the line that contains Token and return the contents of the line after the token
	 * @return True if the line is found and the function has written the following string to OutString
	 */
	static bool GetLineFollowing(const FString& Token, const TArray<FString>& Lines, FString& OutString, int32 NthHit = 0);

	/**
	 * Look for a section of a file beginning with SectionName
	 */
	static bool GetNamedSection(FString SectionName, const TArray<FString>& InLines, TArray<FString>& OutSectionLines);

	static bool bSurveyPending;
	static bool bSurveyComplete;
	static bool bSurveyFailed;
	static double SurveyStartTimeSeconds;
	static FHardwareSurveyResults Results;
};


typedef FWindowsPlatformSurvey FPlatformSurvey;
