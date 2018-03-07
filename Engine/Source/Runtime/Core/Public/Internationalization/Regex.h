// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreTypes.h"
#include "Containers/UnrealString.h"
#include "Templates/SharedPointer.h"

class FRegexMatcherImplementation;
class FRegexPatternImplementation;

class CORE_API FRegexPattern
{
	friend class FRegexMatcher;
public:
	FRegexPattern(const FString& SourceString);

private:
	TSharedRef<FRegexPatternImplementation> Implementation;
};

class FRegexMatcherImplementation;

class CORE_API FRegexMatcher
{
public:
	FRegexMatcher(const FRegexPattern& Pattern, const FString& Input);

	bool FindNext();

	int32 GetMatchBeginning();
	int32 GetMatchEnding();

	int32 GetCaptureGroupBeginning(const int32 Index);
	int32 GetCaptureGroupEnding(const int32 Index);
	FString GetCaptureGroup(const int32 Index);

	int32 GetBeginLimit();
	int32 GetEndLimit();
	void SetLimits(const int32 BeginIndex, const int32 EndIndex);

private:
	TSharedRef<FRegexMatcherImplementation> Implementation;
};
