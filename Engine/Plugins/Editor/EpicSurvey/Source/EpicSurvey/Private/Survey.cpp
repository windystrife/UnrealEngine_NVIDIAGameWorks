// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Survey.h"
#include "EngineAnalytics.h"
#include "QuestionBlock.h"
#include "SurveyPage.h"

#define LOCTEXT_NAMESPACE "EpicSurvey"

int32 FSurvey::CurrentSurveyVersion = 2;

TSharedPtr< FSurvey > FSurvey::Create( const TSharedRef< class FEpicSurvey >& InEpicSurvey, const TSharedRef< FJsonObject >& JsonConfig )
{
	FGuid Identifier;
	if (!FGuid::ParseExact(JsonConfig->GetStringField( TEXT("id") ), EGuidFormats::DigitsWithHyphens, Identifier))
	{
		return nullptr;
	}

	TSharedPtr< FSurvey > NewSurvey( new FSurvey( InEpicSurvey ) );

	NewSurvey->Identifier = Identifier;
	NewSurvey->BannerBrushPath = JsonConfig->GetStringField( TEXT("banner") );
	NewSurvey->DisplayName = FText::FromString( JsonConfig->GetStringField( TEXT("name") ) );
	NewSurvey->Instructions = FText::FromString( JsonConfig->GetStringField( TEXT("instructions") ) );
	NewSurvey->SurveyType = ESurveyType::Normal;
	NewSurvey->AutoPrompt = true;
	NewSurvey->SurveyVersion = 1;
	NewSurvey->MinEngineVersion.Empty();
	NewSurvey->MaxEngineVersion.Empty();
	if ( JsonConfig->HasTypedField<EJson::Boolean>( TEXT("auto_prompt") ) )
	{
		NewSurvey->AutoPrompt = int32(JsonConfig->GetBoolField( TEXT("auto_prompt") ) );
	}
	if( JsonConfig->HasTypedField<EJson::Number>( TEXT("survey_version") ) )
	{
		NewSurvey->SurveyVersion = int32(JsonConfig->GetNumberField( TEXT("survey_version") ) );
	}
	if( JsonConfig->HasTypedField<EJson::String>( TEXT("min_engine_version") ) )
	{
		FEngineVersion::Parse( JsonConfig->GetStringField( TEXT("min_engine_version") ), NewSurvey->MinEngineVersion );
	}
	if (JsonConfig->HasTypedField<EJson::String>(TEXT("max_engine_version")))
	{
		FEngineVersion::Parse( JsonConfig->GetStringField( TEXT("max_engine_version") ), NewSurvey->MaxEngineVersion );
	}
	if( (NewSurvey->SurveyVersion != CurrentSurveyVersion) 
		|| (!NewSurvey->MinEngineVersion.IsEmpty() && FEngineVersion::Current().IsCompatibleWith(NewSurvey->MinEngineVersion))
		|| (!NewSurvey->MaxEngineVersion.IsEmpty() && NewSurvey->MaxEngineVersion.IsCompatibleWith(FEngineVersion::Current())) )
	{
		return NULL;
	}

	if( JsonConfig->HasTypedField<EJson::String>( "type" ) )
	{
		FString SurveyType = JsonConfig->GetStringField( TEXT("type") );
		if( !SurveyType.IsEmpty() )
		{
			if( SurveyType == TEXT( "branch" ) )
			{
				NewSurvey->SurveyType = ESurveyType::Branch;
			}
		}
	}

	if( JsonConfig->HasTypedField< EJson::Array >( "branches" ) )
	{
		TArray< TSharedPtr< FJsonValue > > JsonSurveyBranches = JsonConfig->GetArrayField( "branches" );
		for (int Index = 0; Index < JsonSurveyBranches.Num(); Index++)
		{
			TSharedPtr< FSurveyBranch > Branch = FSurveyBranch::Create( InEpicSurvey, JsonSurveyBranches[Index]->AsObject().ToSharedRef() );
			if ( Branch.IsValid() )
			{
				NewSurvey->Branches.Add( Branch.ToSharedRef() );
				InEpicSurvey->AddBranch( Branch->GetBranchName() );
			}
		}
	}

	if( JsonConfig->HasTypedField< EJson::Array >( "pages" ) )
	{
		TArray< TSharedPtr< FJsonValue > > JsonSurveyPages = JsonConfig->GetArrayField( "pages" );
		for (int Index = 0; Index < JsonSurveyPages.Num(); Index++)
		{
			TSharedPtr< FJsonObject > PageObject = JsonSurveyPages[Index]->AsObject();
			
			if( PageObject.IsValid() )
			{
				TSharedPtr<FSurveyPage> Page = FSurveyPage::Create( InEpicSurvey, PageObject.ToSharedRef() );

				if( Page.IsValid() )
				{
					NewSurvey->Pages.Add( Page.ToSharedRef() );

					if( NewSurvey->SurveyType == ESurveyType::Branch )
					{
						Page->SetBranchSurvey( NewSurvey );
					}
				}
			}
		}
	}
	else
	{
		TSharedPtr<FSurveyPage> Page = FSurveyPage::Create( InEpicSurvey, JsonConfig );

		if( Page.IsValid() )
		{
			NewSurvey->Pages.Add( Page.ToSharedRef() );
		}
	}

	return NewSurvey;
}

FSurvey::FSurvey( const TSharedRef< class FEpicSurvey >& InEpicSurvey )
	: EpicSurvey( InEpicSurvey )
	, InitializationState( EContentInitializationState::NotStarted )
	, BannerState( EContentInitializationState::NotStarted )
	, CurrentPageIndex(0)
	, BranchUsed(true)
{

}

void FSurvey::Initialize()
{
	InitializationState = EContentInitializationState::Working;

	if ( !BannerBrushPath.IsEmpty() )
	{
		BannerState = EContentInitializationState::Working;
		EpicSurvey->LoadCloudFileAsBrush( BannerBrushPath, FOnBrushLoaded::CreateSP( SharedThis( this ), &FSurvey::HandleBannerLoaded ) );

		for( int PageIndex=0; PageIndex<Pages.Num(); ++PageIndex )
		{
			TArray< TSharedRef< FQuestionBlock > > Blocks = Pages[PageIndex]->GetBlocks();
			for (int Index = 0; Index < Blocks.Num(); Index++)
			{
				Blocks[Index]->Initialize();
			}
		}
	} 
	else
	{
		InitializationState = EContentInitializationState::Failure;
	}
}

EContentInitializationState::Type FSurvey::GetInitializationState()
{
	if ( InitializationState == EContentInitializationState::Working )
	{
		bool AllSuccessful = true;
		if ( BannerState != EContentInitializationState::Success )
		{
			AllSuccessful = false;
			if ( BannerState == EContentInitializationState::Failure )
			{
				InitializationState = EContentInitializationState::Failure;
			}
		}

		for( int PageIndex=0; PageIndex<Pages.Num(); ++PageIndex )
		{
			TArray< TSharedRef< FQuestionBlock > > Blocks = Pages[PageIndex]->GetBlocks();
			for (int Index = 0; Index < Blocks.Num(); Index++)
			{
				EContentInitializationState::Type BlockState = Blocks[Index]->GetInitializationState();

				if ( BlockState != EContentInitializationState::Success )
				{
					AllSuccessful = false;
					if ( BlockState == EContentInitializationState::Failure )
					{
						InitializationState = EContentInitializationState::Failure;
					}
				}
			}
		}
		if ( InitializationState == EContentInitializationState::Working && AllSuccessful )
		{
			InitializationState = EContentInitializationState::Success;
		}
	}

	return InitializationState;
}

void FSurvey::HandleBannerLoaded( const TSharedPtr< FSlateDynamicImageBrush >& Brush )
{
	BannerBrush = Brush;
	check( BannerState == EContentInitializationState::Working );
	BannerState = ( Brush.IsValid() ) ? EContentInitializationState::Success : EContentInitializationState::Failure;
}

bool FSurvey::IsReadyToSubmit() const
{
	bool ReadyToSubmit = (CurrentPageIndex+1) == Pages.Num();

	for( int PageIndex=0; ReadyToSubmit && (PageIndex<Pages.Num()); ++PageIndex )
	{
		ReadyToSubmit &= Pages[PageIndex]->IsReadyToSubmit();
	}

	return ReadyToSubmit;
}

void FSurvey::Submit()
{
	if ( FEngineAnalytics::IsAvailable() )
	{
		for( int PageIndex=0; PageIndex<Pages.Num(); ++PageIndex )
		{
			TArray< TSharedRef< FQuestionBlock > > Blocks = Pages[PageIndex]->GetBlocks();

			for (int BlockIndex = 0; BlockIndex < Blocks.Num(); BlockIndex++)
			{
				Blocks[BlockIndex]->SubmitQuestions( Identifier );
			}
		}
	}
}

TSharedPtr<FSurveyBranch> FSurvey::TestForBranch() const
{
	for( auto BranchIt=Branches.CreateConstIterator(); BranchIt; ++BranchIt )
	{
		const FString& BranchName = (*BranchIt)->GetBranchName();
		const int32 BranchPoints = EpicSurvey->GetBranchPoints( BranchName );
		if( BranchPoints >= (*BranchIt)->GetBranchPointsThreshold() )
		{
			TSharedPtr<FSurvey> BranchSurvey = (*BranchIt)->GetBranchSurvey();
			if( BranchSurvey.IsValid() && !BranchSurvey->GetBranchUsed() )
			{
				return (*BranchIt); 
			}
		}
	}
	return NULL;
}

void FSurvey::EvaluateBranches()
{
	// find a potential branch
	TSharedPtr<FSurveyBranch> NewBranch = TestForBranch();

	const int NextPageIndex = CurrentPageIndex+1;

	// check the page page if there is one
	if( NextPageIndex < Pages.Num() )
	{
		// clean up old next branch if necessary - i.e. if we have an old branch and a different or invalid next branch
		TSharedPtr< FSurvey > OldBranchSurvey = Pages[NextPageIndex]->GetBranchSurvey();
		if( OldBranchSurvey.IsValid() && (!NewBranch.IsValid() || (NewBranch->GetBranchSurvey() != OldBranchSurvey)) )
		{
			// Mark the branch as not completed.
			OldBranchSurvey->SetBranchUsed( false );

			// Remove the branch survey pages from the survey.
			struct FRemovePred
			{
				FRemovePred( TSharedPtr< FSurvey > InOldBranchSurvey )
					: Pages(InOldBranchSurvey->GetPages())
				{
				}
				bool operator()( const TSharedPtr<FSurveyPage>& InPage ) const
				{
					return Pages.Contains( InPage.ToSharedRef() );
				}
			private:
				const TArray< TSharedRef< FSurveyPage > >& Pages;
			};
			Pages.RemoveAll( FRemovePred(OldBranchSurvey) );

			// remove the branch points for any of the branch survey answers.
			OldBranchSurvey->UpdateAllBranchPoints( false );
		}

	}

	// if the new branch is valid, insert it
	if( NewBranch.IsValid() )
	{
		TSharedPtr<FSurvey> NewBranchSurvey = NewBranch->GetBranchSurvey();

		if( NewBranchSurvey.IsValid() )
		{
			// find the next free page (i.e. not a branch).
			int32 PageIndex = Pages.Num();
			for( PageIndex=CurrentPageIndex; PageIndex<Pages.Num(); ++PageIndex )
			{
				if( !Pages[PageIndex]->GetBranchSurvey().IsValid() )
				{
					PageIndex += 1;
					break;
				}
			}

			// mark the branch as taken.
			NewBranchSurvey->SetBranchUsed( true );

			// insert the new pages into the survey.
			Pages.Insert( NewBranchSurvey->GetPages(), PageIndex );

			// Add branch points back into the survey
			NewBranchSurvey->UpdateAllBranchPoints( true );
		}
	}
}

void FSurvey::OnPageNext()
{
	if( Pages.Num() > (CurrentPageIndex + 1) )
	{
		// the next page is just the next page in this survey, so add back any branch points if necessary.
		Pages[CurrentPageIndex+1]->UpdateAllBranchPoints( true );
	}
}

void FSurvey::OnPageBack()
{
	// Need to remove all the branch points from this page.
	if( CurrentPageIndex > 0 )
	{
		TSharedPtr< FSurvey > BranchSurvey = Pages[CurrentPageIndex-1]->GetBranchSurvey();
		if( BranchSurvey.IsValid() )
		{
			BranchSurvey->UpdateAllBranchPoints( false );
		}
		else
		{
			// this is just a regular page, so remove those points!
			Pages[CurrentPageIndex]->UpdateAllBranchPoints( false );
		}
	}
}

void FSurvey::UpdateAllBranchPoints( bool bAdd ) const
{
	Pages[CurrentPageIndex]->UpdateAllBranchPoints( bAdd );
}

void FSurvey::Reset()
{
	for( int PageIndex=0; PageIndex<Pages.Num(); ++PageIndex )
	{
		TArray< TSharedRef< FQuestionBlock > > Blocks = Pages[PageIndex]->GetBlocks();

		for (int BlockIndex = 0; BlockIndex < Blocks.Num(); BlockIndex++)
		{
			Blocks[BlockIndex]->Reset();
		}
	}
}

bool FSurvey::CanPageNext() const
{
	if ( Pages[ CurrentPageIndex ]->IsReadyToSubmit() )
	{
		// if we have another page or a pending branch
		if( Pages.Num() > (CurrentPageIndex+1) )
		{
			return true;
		}
	}

	return false;
}

bool FSurvey::CanPageBack() const
{
	if ( CurrentPageIndex > 0 )
	{
		return true;
	}

	return false;
}

#undef LOCTEXT_NAMESPACE
