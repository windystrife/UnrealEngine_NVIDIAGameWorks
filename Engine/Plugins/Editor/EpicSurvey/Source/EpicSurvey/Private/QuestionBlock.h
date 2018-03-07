// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "EpicSurvey.h"

class FJsonObject;

class FQuestionBlock : public TSharedFromThis< FQuestionBlock > 
{
public:

	~FQuestionBlock() {};

	static TSharedPtr< FQuestionBlock > Create( const TSharedRef< class FEpicSurvey >& InEpicSurvey, const TSharedRef< FJsonObject >& JsonConfig );

	enum EResponse
	{
		Response_Multi = 0,
		Response_Single = 1
	};

	enum EStyle
	{
		Style_Inline,
		Style_InlineText,

		Style_Multiline,
		Style_MultilineText,

		Style_Columns
	};		

	struct FAnswer
	{
		FText Text;
		TMap< FString, int32 > Branches;
	};

public:

	void Initialize();
	EContentInitializationState::Type GetInitializationState();

	FText GetInstructions() const { return Instructions; }
	EStyle GetStyle() const { return Style; }
	EResponse GetResponse() const { return Response; }
	bool IsRequired() const { return bIsRequired; }

	const TArray< FText >& GetQuestions() { return Questions; }
	const TArray< FAnswer >& GetAnswers() { return Answers; }
	const TArray< int32 > GetUserAnswers( int32 QuestionIndex ) const { return UserAnswers[QuestionIndex]; }
	const TArray< FString > GetUserTextAnswer( int32 QuestionIndex ) const { return UserTextAnswers[QuestionIndex]; }

	void SetUserTextAnswer( int32 QuestionIndex, int32 AnswerIndex, const FString& Text ) { UserTextAnswers[QuestionIndex][AnswerIndex] = Text; }

	void MarkAnswerByIndex( int32 QuestionIndex, int32 AnswerIndex );
	void UnmarkAnswerByIndex( int32 QuestionIndex, int32 AnswerIndex );

	bool IsReadyToSubmit() const;
	void SubmitQuestions( const FGuid& SurveyIdentifier );

	/** Adds or removes all branch points for all answered questions */
	void UpdateAllBranchPoints( bool bAdd ) const;

	void Reset();

private:

	FQuestionBlock( const TSharedRef< class FEpicSurvey >& InEpicSurvey );

	/** Adds the branch points from the FAnswer to the survey
	 *
	 * @param Answer	The selected answer
	 * @param bAdd		true for adding the points, false for removing the points
	 */
	void UpdateBranchPoints( const FAnswer& Answer, const bool bAdd ) const;

private:

	TSharedRef< class FEpicSurvey > EpicSurvey;

	EContentInitializationState::Type InitializationState;

	FText Instructions;
	EStyle Style;
	EResponse Response;
	bool bIsRequired;

	TArray< FText > Questions;
	TArray< FAnswer > Answers;

	TArray< TArray< int32 > > UserAnswers;
	TArray< TArray< FString > > UserTextAnswers;
};
