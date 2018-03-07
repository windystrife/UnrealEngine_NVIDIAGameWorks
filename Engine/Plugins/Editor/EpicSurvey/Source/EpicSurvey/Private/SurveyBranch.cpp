// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SurveyBranch.h"
#include "Serialization/JsonTypes.h"
#include "Dom/JsonObject.h"
#include "Survey.h"

#define LOCTEXT_NAMESPACE "EpicSurvey"

TSharedPtr< FSurveyBranch > FSurveyBranch::Create( const TSharedRef< class FEpicSurvey >& InEpicSurvey, const TSharedRef< FJsonObject >& JsonConfig )
{
	TSharedPtr< FSurveyBranch > Branch = MakeShareable( new FSurveyBranch( InEpicSurvey ) );

	if ( JsonConfig->HasTypedField< EJson::String >( TEXT("branch_name") ) )
	{
		Branch->BranchName = JsonConfig->GetStringField( TEXT("branch_name") );
	}

	if ( JsonConfig->HasTypedField< EJson::String >( TEXT("survey") ) )
	{
		FString BranchSurveyFileName = JsonConfig->GetStringField( TEXT("survey") );
		Branch->BranchSurvey = InEpicSurvey->GetBranchSurvey(BranchSurveyFileName);

		if( Branch->BranchSurvey.IsValid() )
		{
			Branch->BranchSurvey->SetBranchUsed( false );
		}
	}

	if ( JsonConfig->HasTypedField< EJson::Number >( TEXT("threshold") ) )
	{
		Branch->BranchPointsThreshold = int32(JsonConfig->GetNumberField( TEXT("threshold") ));
	}

	return Branch;
}

FSurveyBranch::FSurveyBranch( const TSharedRef< class FEpicSurvey >& InEpicSurvey )
	: EpicSurvey( InEpicSurvey )
	, BranchPointsThreshold(0)
{
}

void FSurveyBranch::Reset()
{
	if( BranchSurvey.IsValid() )
	{
		BranchSurvey->Reset();
	}
}

#undef LOCTEXT_NAMESPACE
