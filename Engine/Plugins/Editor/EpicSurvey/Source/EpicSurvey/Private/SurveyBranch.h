// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "EpicSurvey.h"

class FJsonObject;
class FSurvey;

/** Contains another survey which is used as a branch */
class FSurveyBranch : public TSharedFromThis< FSurveyBranch > 
{
public:
	struct BranchPointsType 
	{
		BranchPointsType(const TSharedRef<FSurveyBranch>& InBranchBlock) : BranchBlock(InBranchBlock), Points(0) {}
		TSharedRef<FSurveyBranch> BranchBlock;
		int32 Points;
	};
	typedef TMap<FString,BranchPointsType> BranchPointsMap;

public:

	static TSharedPtr< FSurveyBranch > Create( const TSharedRef< class FEpicSurvey >& InEpicSurvey, const TSharedRef< FJsonObject >& JsonConfig );

	/** returns the name of the branch */
	const FString& GetBranchName() const { return BranchName; }

	/** returns the points threshold at which which triggers the branch */
	const int32 GetBranchPointsThreshold() const { return BranchPointsThreshold; }

	/** returns the survey which represents the branch */
	const TSharedPtr<FSurvey>& GetBranchSurvey() const { return BranchSurvey; }

	void Reset();

private:
	FSurveyBranch( const TSharedRef< class FEpicSurvey >& InEpicSurvey );

	TSharedPtr< class FEpicSurvey > EpicSurvey;

	/** The name identifier of the branch */
	FString BranchName;

	/** The actual, created survey */
	TSharedPtr<FSurvey> BranchSurvey;

	/** The number of points required to trigger the branch when 'next page' is clicked */
	int32 BranchPointsThreshold;
};
