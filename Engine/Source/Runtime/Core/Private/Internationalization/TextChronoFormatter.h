// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Text.h"

/**
 * Utility for performing low-level localized chronological formats.
 * The implementation can be found in LegacyText.cpp and ICUText.cpp.
 */
class FTextChronoFormatter
{
public:
	static FString AsDate(const FDateTime& DateTime, const EDateTimeStyle::Type DateStyle, const FString& TimeZone, const FCulture& TargetCulture);
	static FString AsDateTime(const FDateTime& DateTime, const EDateTimeStyle::Type DateStyle, const EDateTimeStyle::Type TimeStyle, const FString& TimeZone, const FCulture& TargetCulture);
	static FString AsTime(const FDateTime& DateTime, const EDateTimeStyle::Type TimeStyle, const FString& TimeZone, const FCulture& TargetCulture);
};
