// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreTypes.h"
#include "Containers/UnrealString.h"

#if UE_ENABLE_ICU
THIRD_PARTY_INCLUDES_START
	#include <unicode/regex.h>
THIRD_PARTY_INCLUDES_END
#include "Templates/SharedPointer.h"
#include "Internationalization/Regex.h"
#include "Internationalization/ICUUtilities.h"

namespace
{
	TSharedPtr<icu::RegexPattern> CreateRegexPattern(const FString& SourceString)
	{
		icu::UnicodeString ICUSourceString;
		ICUUtilities::ConvertString(SourceString, ICUSourceString);

		UErrorCode ICUStatus = U_ZERO_ERROR;
		return MakeShareable( icu::RegexPattern::compile(ICUSourceString, 0, ICUStatus) );
	}
}

class FRegexPatternImplementation
{
public:
	FRegexPatternImplementation(const FString& SourceString) 
		: ICURegexPattern( CreateRegexPattern(SourceString) ) 
	{
	}

public:
	TSharedPtr<icu::RegexPattern> ICURegexPattern;
};

FRegexPattern::FRegexPattern(const FString& SourceString) 
	: Implementation(new FRegexPatternImplementation(SourceString))
{
}

namespace
{
	TSharedPtr<icu::RegexMatcher> CreateRegexMatcher(const FRegexPatternImplementation& Pattern, const icu::UnicodeString& InputString)
	{
		if (Pattern.ICURegexPattern.IsValid())
		{
			UErrorCode ICUStatus = U_ZERO_ERROR;
			return MakeShareable( Pattern.ICURegexPattern->matcher(InputString, ICUStatus) );
		}
		return nullptr;
	}
}

class FRegexMatcherImplementation
{
public:
	FRegexMatcherImplementation(const FRegexPatternImplementation& Pattern, const FString& InputString) 
		: ICUInputString(ICUUtilities::ConvertString(InputString))
		, ICURegexMatcher(CreateRegexMatcher(Pattern, ICUInputString))
		, OriginalString(InputString)
	{
	}

public:
	const icu::UnicodeString ICUInputString;
	TSharedPtr<icu::RegexMatcher> ICURegexMatcher;
	FString OriginalString;
};

FRegexMatcher::FRegexMatcher(const FRegexPattern& Pattern, const FString& InputString) 
	: Implementation(new FRegexMatcherImplementation(Pattern.Implementation.Get(), InputString))
{
}	

bool FRegexMatcher::FindNext()
{
	return Implementation->ICURegexMatcher.IsValid() && Implementation->ICURegexMatcher->find() != 0;
}

int32 FRegexMatcher::GetMatchBeginning()
{
	UErrorCode ICUStatus = U_ZERO_ERROR;
	return Implementation->ICURegexMatcher.IsValid() ? Implementation->ICURegexMatcher->start(ICUStatus) : INDEX_NONE;
}

int32 FRegexMatcher::GetMatchEnding()
{
	UErrorCode ICUStatus = U_ZERO_ERROR;
	return Implementation->ICURegexMatcher.IsValid() ? Implementation->ICURegexMatcher->end(ICUStatus) : INDEX_NONE;
}

int32 FRegexMatcher::GetCaptureGroupBeginning(const int32 Index)
{
	UErrorCode ICUStatus = U_ZERO_ERROR;
	return Implementation->ICURegexMatcher.IsValid() ? Implementation->ICURegexMatcher->start(Index, ICUStatus) : INDEX_NONE;
}

int32 FRegexMatcher::GetCaptureGroupEnding(const int32 Index)
{
	UErrorCode ICUStatus = U_ZERO_ERROR;
	return Implementation->ICURegexMatcher.IsValid() ? Implementation->ICURegexMatcher->end(Index, ICUStatus) : INDEX_NONE;
}

FString FRegexMatcher::GetCaptureGroup(const int32 Index)
{
	int32 CaptureGroupBeginning = GetCaptureGroupBeginning(Index);
	CaptureGroupBeginning = FMath::Max(0, CaptureGroupBeginning);

	int32 CaptureGroupEnding = GetCaptureGroupEnding(Index);
	CaptureGroupEnding = FMath::Max(CaptureGroupBeginning, CaptureGroupEnding);

	return Implementation->OriginalString.Mid(CaptureGroupBeginning, CaptureGroupEnding - CaptureGroupBeginning);
}

int32 FRegexMatcher::GetBeginLimit()
{
	return Implementation->ICURegexMatcher.IsValid() ? Implementation->ICURegexMatcher->regionStart() : INDEX_NONE;
}

int32 FRegexMatcher::GetEndLimit()
{
	return Implementation->ICURegexMatcher.IsValid() ? Implementation->ICURegexMatcher->regionEnd() : INDEX_NONE;
}

void FRegexMatcher::SetLimits(const int32 BeginIndex, const int32 EndIndex)
{
	if (Implementation->ICURegexMatcher.IsValid())
	{
		UErrorCode ICUStatus = U_ZERO_ERROR;
		Implementation->ICURegexMatcher->region(BeginIndex, EndIndex, ICUStatus);
	}
}
#endif
