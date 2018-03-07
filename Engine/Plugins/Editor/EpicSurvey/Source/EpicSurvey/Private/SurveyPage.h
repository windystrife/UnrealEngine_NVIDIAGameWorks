// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "EpicSurvey.h"
#include "QuestionBlock.h"

class FJsonObject;
class FSurvey;

class FSurveyPage
{
public:

	static TSharedPtr<FSurveyPage> Create( const TSharedRef< class FEpicSurvey >& InEpicSurvey, const TSharedRef< FJsonObject >& JsonObject );

	/** Get all the question blocks associated with this page */
	const TArray< TSharedRef< FQuestionBlock > >& GetBlocks() const { return Blocks; }

	/** Set the survey the page belongs to, if the page is in a branch */
	void SetBranchSurvey( const TSharedPtr< FSurvey >& InSurvey ) { BranchSurvey = InSurvey; }

	/** Get the branch survey the page belongs to */
	TSharedPtr< FSurvey > GetBranchSurvey() const { return BranchSurvey; }

	/** true, if the survey is completed */
	bool IsReadyToSubmit() const;

	/** adds or removes all the branch points for answered questions from this page */
	void UpdateAllBranchPoints( bool bAdd ) const;

protected:
	FSurveyPage( const TSharedRef< class FEpicSurvey >& InEpicSurvey );

private:
	TSharedPtr< class FEpicSurvey > EpicSurvey;
	TArray< TSharedRef< FQuestionBlock > > Blocks;
	TSharedPtr< FSurvey > BranchSurvey;

};
