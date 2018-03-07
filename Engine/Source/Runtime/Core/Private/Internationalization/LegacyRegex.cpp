// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreTypes.h"
#include "Containers/UnrealString.h"

#if !UE_ENABLE_ICU
#include "Regex.h"

class FRegexPatternImplementation
{

};

FRegexPattern::FRegexPattern(const FString& SourceString) : Implementation( MakeShareable( new FRegexPatternImplementation() ) )
{
}

class FRegexMatcherImplementation
{

};

FRegexMatcher::FRegexMatcher(const FRegexPattern& Pattern, const FString& InputString) : Implementation( MakeShareable( new FRegexMatcherImplementation() ) )
{
}	

bool FRegexMatcher::FindNext()
{
	return false;
}

int32 FRegexMatcher::GetMatchBeginning()
{
	return INDEX_NONE;
}

int32 FRegexMatcher::GetMatchEnding()
{
	return INDEX_NONE;
}

int32 FRegexMatcher::GetCaptureGroupBeginning(const int32 Index)
{
	return INDEX_NONE;
}

int32 FRegexMatcher::GetCaptureGroupEnding(const int32 Index)
{
	return INDEX_NONE;
}

FString FRegexMatcher::GetCaptureGroup(const int32 Index)
{
	return FString();
}

int32 FRegexMatcher::GetBeginLimit()
{
	return INDEX_NONE;
}

int32 FRegexMatcher::GetEndLimit()
{
	return INDEX_NONE;
}

void FRegexMatcher::SetLimits(const int32 BeginIndex, const int32 EndIndex)
{
}
#endif
