// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "QuestionBlock.h"
#include "Serialization/JsonTypes.h"
#include "Dom/JsonObject.h"
#include "EngineAnalytics.h"
#include "AnalyticsEventAttribute.h"
#include "Interfaces/IAnalyticsProvider.h"

#define LOCTEXT_NAMESPACE "EpicSurvey"

TSharedPtr< FQuestionBlock > FQuestionBlock::Create( const TSharedRef< class FEpicSurvey >& InEpicSurvey, const TSharedRef< FJsonObject >& JsonConfig )
{
	TSharedPtr< FQuestionBlock > Block = MakeShareable( new FQuestionBlock( InEpicSurvey ) );

	Block->Style = Style_Inline;
	if ( JsonConfig->HasTypedField< EJson::String >( TEXT("style") ) )
	{
		const FString Style = JsonConfig->GetStringField( TEXT("style") );
		if ( Style == TEXT("multiline")  )
		{
			Block->Style = Style_Multiline;
		}
		else if ( Style == TEXT("multiline-text") )
		{
			Block->Style = Style_MultilineText;
		}
		else if ( Style == TEXT("inline-text") )
		{
			Block->Style = Style_InlineText;
		}
		else if ( Style == TEXT("columns") )
		{
			Block->Style = Style_Columns;
		}
	}

	Block->Instructions = FText::GetEmpty();
	if ( JsonConfig->HasTypedField< EJson::String >( TEXT("instructions") ) )
	{
		Block->Instructions = FText::FromString( JsonConfig->GetStringField( TEXT("instructions") ) );
	}

	Block->bIsRequired = true;
	if ( JsonConfig->HasTypedField< EJson::Boolean >( TEXT("required") ) )
	{
		Block->bIsRequired = JsonConfig->GetBoolField( TEXT("required") );
	}

	Block->Response = FQuestionBlock::Response_Single;
	if ( JsonConfig->HasTypedField< EJson::String >( TEXT("response") ) )
	{
		const FString ResponseString = JsonConfig->GetStringField( TEXT("response") );
		if ( ResponseString == TEXT("multi") )
		{
			Block->Response = FQuestionBlock::Response_Multi;
		}
	}
	else if ( JsonConfig->HasTypedField< EJson::Number >( TEXT("response") ) )
	{
		int32 ResponseNumber = (int32)( JsonConfig->GetNumberField( TEXT("response") ) );
		if ( ResponseNumber <= 0 )
		{
			Block->Response = FQuestionBlock::Response_Multi;
		}
		else if ( ResponseNumber == 1 )
		{
			Block->Response = FQuestionBlock::Response_Multi;
		}
		else
		{
			Block->Response = (FQuestionBlock::EResponse)ResponseNumber;
		}
	}

	if ( JsonConfig->HasTypedField< EJson::Array >( TEXT("questions") ) )
	{
		TArray< TSharedPtr< FJsonValue > > QuestionStrings = JsonConfig->GetArrayField( TEXT("questions") );
		for (int Index = 0; Index < QuestionStrings.Num(); Index++)
		{
			const FString Question = QuestionStrings[Index]->AsString();
			if ( !Question.IsEmpty() )
			{
				Block->Questions.Add( FText::FromString( Question ) );
				Block->UserAnswers.Add( TArray< int32 >() );
				Block->UserTextAnswers.Add( TArray< FString >() );
			}
		}
	}

	if ( JsonConfig->HasTypedField< EJson::Array >( TEXT("answers") ) )
	{
		TArray< TSharedPtr< FJsonValue > > AnswerStrings = JsonConfig->GetArrayField( TEXT("answers") );
		for (int AnswerIndex = 0; AnswerIndex < AnswerStrings.Num(); AnswerIndex++)
		{
			for (int QuestionIndex = 0; QuestionIndex < Block->UserTextAnswers.Num(); QuestionIndex++)
			{
				Block->UserTextAnswers[QuestionIndex].Push( FString() );
			}

			FAnswer Answer;
			switch( AnswerStrings[AnswerIndex]->Type  )
			{
			case EJson::String:
				{
					Answer.Text = FText::FromString( AnswerStrings[AnswerIndex]->AsString() );
				}
				break;
			case EJson::Object:
				{
					TSharedPtr<FJsonObject> SubObject = AnswerStrings[AnswerIndex]->AsObject();
					if( SubObject.IsValid() )
					{
						if( SubObject->HasTypedField< EJson::String >( TEXT("text") ) )
						{
							Answer.Text = FText::FromString( SubObject->GetStringField( TEXT("text") ) );
						}
						if( SubObject->HasTypedField< EJson::Array >( TEXT("branch_points") ) )
						{
							TArray< TSharedPtr< FJsonValue > > BranchPointsArray = SubObject->GetArrayField( TEXT("branch_points") );
							for (int BranchIndex = 0; BranchIndex < BranchPointsArray.Num(); ++BranchIndex)
							{
								TSharedPtr<FJsonObject> BranchObject = BranchPointsArray[BranchIndex]->AsObject();

								if( BranchObject.IsValid() )
								{
									FString BranchName;
									int32 BranchPoints = 0;
									if( BranchObject->HasTypedField< EJson::String >("branch") )
									{
										BranchName = BranchObject->GetStringField("branch");
									}
									if( BranchObject->HasTypedField< EJson::Number >("points") )
									{
										BranchPoints = int32(BranchObject->GetNumberField("points"));
									}
									if( !BranchName.IsEmpty() && (BranchPoints > 0) )
									{
										Answer.Branches.Add( BranchName, BranchPoints );
									}
								}
							}
						}
					}
				}
				break;
			}

			Block->Answers.Add( Answer );
		}
	}

	return Block;
}

FQuestionBlock::FQuestionBlock( const TSharedRef< class FEpicSurvey >& InEpicSurvey )
	: EpicSurvey( InEpicSurvey )
	, InitializationState( EContentInitializationState::NotStarted )
{

}

void FQuestionBlock::MarkAnswerByIndex( int32 QuestionIndex, int32 AnswerIndex )
{
	if ( Response == Response_Single )
	{
		if( UserAnswers[QuestionIndex].Num() > 0 )
		{
			const FAnswer& AnswerToRemove = Answers[UserAnswers[QuestionIndex][0]];
			UpdateBranchPoints( AnswerToRemove, false );
		}

		UserAnswers[QuestionIndex].Empty();
		UserAnswers[QuestionIndex].Push( AnswerIndex );

		UpdateBranchPoints( Answers[AnswerIndex], true );
	}
	else if ( Response == Response_Multi || UserAnswers[QuestionIndex].Num() < (int32)Response )
	{
		UserAnswers[QuestionIndex].Push( AnswerIndex );

		UpdateBranchPoints( Answers[AnswerIndex], true );
	}
	else
	{
		auto UserAnswer = UserAnswers[QuestionIndex];
		for( auto AnswerIt=UserAnswer.CreateConstIterator(); AnswerIt; ++AnswerIt )
		{
			UpdateBranchPoints( Answers[*AnswerIt], false );
		}

		UserAnswers[QuestionIndex].RemoveAt( 0 );
		UserAnswers[QuestionIndex].Push( AnswerIndex );

		UpdateBranchPoints( Answers[AnswerIndex], true );
	}
}

void FQuestionBlock::UnmarkAnswerByIndex( int32 QuestionIndex, int32 AnswerIndex )
{
	UserAnswers[QuestionIndex].Remove( AnswerIndex );

	UpdateBranchPoints( Answers[AnswerIndex], false );
}

void FQuestionBlock::UpdateBranchPoints( const FAnswer& Answer, const bool bAdd ) const
{
	const int Sign = bAdd ? 1 : -1;
	for( auto BranchIt=Answer.Branches.CreateConstIterator(); BranchIt; ++BranchIt )
	{
		EpicSurvey->AddToBranchPoints( BranchIt->Key, Sign*BranchIt->Value );
	}
}

bool FQuestionBlock::IsReadyToSubmit() const
{
	if ( !bIsRequired )
	{
		return true;
	}

	bool bIsReadyToSubmit = true;
	if ( Style == Style_InlineText || Style == Style_MultilineText )
	{
		for (int QuestionIndex = 0; bIsReadyToSubmit && QuestionIndex < Questions.Num(); QuestionIndex++)
		{
			for (int AnswerIndex = 0; AnswerIndex < UserTextAnswers[QuestionIndex].Num(); AnswerIndex++)
			{
				bIsReadyToSubmit = !UserTextAnswers[QuestionIndex][AnswerIndex].IsEmpty();

				if ( !bIsReadyToSubmit )
				{
					break;
				}
			}
		}
	}
	else
	{
		if ( Response == Response_Single )
		{
			for (int QuestionIndex = 0; bIsReadyToSubmit && QuestionIndex < Questions.Num(); QuestionIndex++)
			{
				bIsReadyToSubmit = UserAnswers[QuestionIndex].Num() == 1;
			}
		}
		else if ( Response == Response_Multi )
		{
			for (int QuestionIndex = 0; bIsReadyToSubmit && QuestionIndex < Questions.Num(); QuestionIndex++)
			{
				bIsReadyToSubmit = UserAnswers[QuestionIndex].Num() > 0;
			}
		}
		else 
		{
			for (int QuestionIndex = 0; bIsReadyToSubmit && QuestionIndex < Questions.Num(); QuestionIndex++)
			{
				bIsReadyToSubmit = UserAnswers[QuestionIndex].Num() == (int32)Response;
			}
		}
	}

	return bIsReadyToSubmit;
}

void FQuestionBlock::SubmitQuestions( const FGuid& SurveyIdentifier )
{
	if ( FEngineAnalytics::IsAvailable() )
	{
		if ( Style == Style_InlineText || Style == Style_MultilineText )
		{
			FString AnswerName = TEXT("Answer");
			for (int QuestionIndex = 0; QuestionIndex < Questions.Num(); QuestionIndex++)
			{
				TArray< FString > UserTextAnswersForQuestion = UserTextAnswers[QuestionIndex];

				TArray< FAnalyticsEventAttribute > EventAttributes;
				EventAttributes.Add( FAnalyticsEventAttribute( TEXT("SurveyID"), SurveyIdentifier.ToString() ) );
				EventAttributes.Add( FAnalyticsEventAttribute( TEXT("Question"), Questions[QuestionIndex].ToString() ) );

				int32 AnswerFlags = 0; 
				for (int AnswerIndex = 0; AnswerIndex < UserTextAnswersForQuestion.Num(); AnswerIndex++)
				{
					if ( !UserTextAnswersForQuestion[AnswerIndex].IsEmpty() )
					{
						AnswerFlags |= ( 1 << AnswerIndex );
					}
				}

				EventAttributes.Add( FAnalyticsEventAttribute( TEXT("AnswerFlags"), AnswerFlags ) );

				for (int AnswerIndex = 0; AnswerIndex < Answers.Num(); AnswerIndex++)
				{
					FString AnswerText = Answers[ AnswerIndex ].Text.ToString();
					if ( AnswerText.IsEmpty() )
					{
						AnswerText = FString::FormatAsNumber( AnswerIndex + 1 );
					}

					EventAttributes.Add( FAnalyticsEventAttribute( AnswerText, UserTextAnswersForQuestion[ AnswerIndex ] ) );
				}

				FEngineAnalytics::GetProvider().RecordEvent( TEXT("Survey"), EventAttributes);
			}
		}
		else
		{
			FString AnswerName = TEXT("Answer");
			for (int QuestionIndex = 0; QuestionIndex < Questions.Num(); QuestionIndex++)
			{
				TArray< int32 > UserAnswersForQuestion = UserAnswers[QuestionIndex];

				TArray< FAnalyticsEventAttribute > EventAttributes;
				EventAttributes.Add( FAnalyticsEventAttribute( TEXT("SurveyID"), SurveyIdentifier.ToString() ) );
				EventAttributes.Add( FAnalyticsEventAttribute( TEXT("Question"), Questions[QuestionIndex].ToString() ) );

				int32 AnswerFlags = 0; 
				for (int AnswerIndex = 0; AnswerIndex < UserAnswersForQuestion.Num(); AnswerIndex++)
				{
					AnswerFlags |= ( 1 << UserAnswersForQuestion[AnswerIndex] );
				}

				EventAttributes.Add( FAnalyticsEventAttribute( TEXT("AnswerFlags"), AnswerFlags ) );

				for (int AnswerIndex = 0; AnswerIndex < Answers.Num(); AnswerIndex++)
				{
					const FString Value = UserAnswersForQuestion.Contains( AnswerIndex ) ? TEXT("true") : TEXT("false");
					EventAttributes.Add( FAnalyticsEventAttribute( Answers[ AnswerIndex ].Text.ToString(), Value ) );
				}

				FEngineAnalytics::GetProvider().RecordEvent( TEXT("Survey"), EventAttributes);
			}
		}
	}
}

void FQuestionBlock::UpdateAllBranchPoints( bool bAdd ) const
{
	for( int32 QuestionIndex=0; QuestionIndex<Questions.Num(); ++QuestionIndex )
	{
		auto UserAnswer = UserAnswers[QuestionIndex];
		for( auto AnswerIt=UserAnswer.CreateConstIterator(); AnswerIt; ++AnswerIt )
		{
			UpdateBranchPoints( Answers[*AnswerIt], bAdd );
		}
	}
}

void FQuestionBlock::Initialize()
{
	InitializationState = EContentInitializationState::Success;
}

EContentInitializationState::Type FQuestionBlock::GetInitializationState()
{
	if ( InitializationState == EContentInitializationState::Working )
	{
		//	bool AllSuccessful = true;
	
		//Insert other dependencies here

		if ( true /* AllSuccessful */)
		{
			InitializationState = EContentInitializationState::Success;
		}
	}

	return InitializationState;
}

void FQuestionBlock::Reset()
{
	// remove all user answers.
	for( int32 UserAnswersIndex=0; UserAnswersIndex<UserAnswers.Num(); ++UserAnswersIndex )
	{
		TArray< int32 >& UserAnswersArray = UserAnswers[UserAnswersIndex];
		for( auto UserAnswersArrayIt=UserAnswersArray.CreateConstIterator(); UserAnswersArrayIt; ++UserAnswersArrayIt )
		{
			UnmarkAnswerByIndex(UserAnswersIndex, *UserAnswersArrayIt);
		}
		UserAnswersArray.Empty();
	}
}

#undef LOCTEXT_NAMESPACE
