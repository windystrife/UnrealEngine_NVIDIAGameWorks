// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SurveyPage.h"
#include "Serialization/JsonTypes.h"
#include "Dom/JsonObject.h"

TSharedPtr< FSurveyPage > FSurveyPage::Create( const TSharedRef< class FEpicSurvey >& InEpicSurvey, const TSharedRef< FJsonObject >& JsonObject )
{
	TSharedPtr< FSurveyPage > Page = MakeShareable( new FSurveyPage( InEpicSurvey ) );
	if( JsonObject->HasTypedField< EJson::Array >( "blocks" ) )
	{
		TArray< TSharedPtr< FJsonValue > > JsonSurveyBlocks = JsonObject->GetArrayField( "blocks" );
		for (int Index = 0; Index < JsonSurveyBlocks.Num(); Index++)
		{
			TSharedPtr< FQuestionBlock > Block = FQuestionBlock::Create( InEpicSurvey, JsonSurveyBlocks[Index]->AsObject().ToSharedRef() );
			if ( Block.IsValid() )
			{
				Page->Blocks.Add( Block.ToSharedRef() );
			}
		}
	}
	return Page;
}

FSurveyPage::FSurveyPage( const TSharedRef< class FEpicSurvey >& InEpicSurvey )
	: EpicSurvey( InEpicSurvey )
{

}

bool FSurveyPage::IsReadyToSubmit() const
{
	bool ReadyToSubmit = true;
	for (int BlockIndex = 0; ReadyToSubmit && BlockIndex < Blocks.Num(); BlockIndex++)
	{
		ReadyToSubmit &= Blocks[BlockIndex]->IsReadyToSubmit();
	}
	return ReadyToSubmit;
}

void FSurveyPage::UpdateAllBranchPoints( bool bAdd ) const
{
	for( auto ItQuestionBlock=Blocks.CreateConstIterator(); ItQuestionBlock; ++ItQuestionBlock )
	{
		(*ItQuestionBlock)->UpdateAllBranchPoints( bAdd);
	}
}
