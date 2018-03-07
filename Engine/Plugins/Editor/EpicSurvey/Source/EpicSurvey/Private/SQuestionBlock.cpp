// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SQuestionBlock.h"
#include "Widgets/SBoxPanel.h"
#include "Styling/SlateTypes.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Text/SRichTextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "EditorStyleSet.h"

void SQuestionBlock::Construct( const FArguments& Args, const TSharedRef<FQuestionBlock>& Block )
{
	TSharedRef< SGridPanel > Grid = SNew( SGridPanel );

	const float QuestionColumnWidth = 400;

	int32 RowIndex = 0;
	int32 ColumnIndex = 0;

	const TArray< FQuestionBlock::FAnswer > Answers = Block->GetAnswers();
	const TArray< FText > Questions = Block->GetQuestions();

	const FCheckBoxStyle* CheckBoxStyle = &FCoreStyle::Get().GetWidgetStyle< FCheckBoxStyle >("Checkbox");
	FQuestionBlock::EResponse BlockResponse = Block->GetResponse();
	if ( BlockResponse == FQuestionBlock::Response_Single )
	{
		CheckBoxStyle = &FEditorStyle::Get().GetWidgetStyle< FCheckBoxStyle >("RadioButton");
	}

	if ( Block->GetStyle() == FQuestionBlock::Style_Inline )
	{
		for (int QuestionIndex = 0; QuestionIndex < Questions.Num(); QuestionIndex++)
		{
			ColumnIndex = 0;
			++RowIndex;

			Grid->AddSlot( ColumnIndex, RowIndex )
			[
				SNew( SBox )
				.WidthOverride( QuestionColumnWidth )
				.Padding( FMargin( 0, 5 ) )
				.VAlign( VAlign_Center )
				[
					SNew( STextBlock )
					.Text( Questions[QuestionIndex] )
				]
			];

			for (int AnswerIndex = 0; AnswerIndex < Answers.Num(); AnswerIndex++)
			{
				++ColumnIndex;
				Grid->AddSlot( ColumnIndex, RowIndex )
				.VAlign( VAlign_Center )
				[
					SNew( SCheckBox )
					.Style( CheckBoxStyle )
					.IsChecked( this, &SQuestionBlock::IsAnswerChecked, TWeakPtr< FQuestionBlock >( Block ), QuestionIndex, AnswerIndex )
					.OnCheckStateChanged( this, &SQuestionBlock::AnswerCheckStateChanged, TWeakPtr< FQuestionBlock >( Block ), QuestionIndex, AnswerIndex )
					.Padding( FMargin( 0, 0, 10, 0 ) )
					[
						SNew( STextBlock )
						.Text( Answers[AnswerIndex].Text )
					]
				];
				Grid->SetColumnFill( ColumnIndex, 1.0f );
			}
		}
	}
	else if ( Block->GetStyle() == FQuestionBlock::Style_InlineText )
	{
		for (int QuestionIndex = 0; QuestionIndex < Questions.Num(); QuestionIndex++)
		{
			ColumnIndex = 0;
			++RowIndex;

			if ( Answers.Num() > 1 )
			{
				Grid->AddSlot( ColumnIndex, RowIndex )
				[
					SNew( SBox )
					.WidthOverride( QuestionColumnWidth )
					.Padding( FMargin( 0, 5 ) )
					.VAlign( VAlign_Center )
					[
						SNew( STextBlock )
						.Text( Questions[QuestionIndex] )
					]
				];
			}
			else
			{
				Grid->AddSlot( ColumnIndex, RowIndex )
				.VAlign( VAlign_Center )
				.Padding( FMargin( 0, 5 ) )
				[
					SNew( STextBlock )
					.Text( Questions[QuestionIndex] )
				];
			}

			for (int AnswerIndex = 0; AnswerIndex < Answers.Num(); AnswerIndex++)
			{
				++ColumnIndex;
				Grid->AddSlot( ColumnIndex, RowIndex )
				.VAlign( VAlign_Center )
				.Padding( 0, 0, 3, 0 )
				[
					SNew( STextBlock )
					.Text( Answers[AnswerIndex].Text )
				];

				++ColumnIndex;
				Grid->AddSlot( ++ColumnIndex, RowIndex )
				.VAlign( VAlign_Center )
				.Padding( FMargin( 3, 0, 10, 0 ) )
				[
					SNew( SEditableTextBox )
					.OnTextChanged( this, &SQuestionBlock::AnswerTextChanged, TWeakPtr< FQuestionBlock >( Block ), QuestionIndex, AnswerIndex )
					.OnTextCommitted( this, &SQuestionBlock::AnswerTextCommitted, TWeakPtr< FQuestionBlock >( Block ), QuestionIndex, AnswerIndex )
				];
				Grid->SetColumnFill( ColumnIndex, 1.0f );
			}
		}
	}
	else if ( Block->GetStyle() == FQuestionBlock::Style_Multiline )
	{
		for (int QuestionIndex = 0; QuestionIndex < Questions.Num(); QuestionIndex++)
		{
			ColumnIndex = 0;
			++RowIndex;

			Grid->AddSlot( ColumnIndex, RowIndex )
			[
				SNew( SBox )
				.WidthOverride( QuestionColumnWidth )
				.Padding( FMargin( 0, 0, 0, 8 ) )
				[					
					SNew( STextBlock )
					.Text( Questions[QuestionIndex] )
				]
			];

			for (int AnswerIndex = 0; AnswerIndex < Answers.Num(); AnswerIndex++)
			{
				++RowIndex;

				Grid->AddSlot( ColumnIndex, RowIndex )
				[
					SNew( SHorizontalBox )
					+SHorizontalBox::Slot()
					.AutoWidth()
					.Padding( 20, 3 )
					.VAlign( VAlign_Center )
					[
						SNew( SCheckBox )
						.Style( CheckBoxStyle )
						.IsChecked( this, &SQuestionBlock::IsAnswerChecked, TWeakPtr< FQuestionBlock >( Block ), QuestionIndex, AnswerIndex )
						.OnCheckStateChanged( this, &SQuestionBlock::AnswerCheckStateChanged, TWeakPtr< FQuestionBlock >( Block ), QuestionIndex, AnswerIndex )
						[
							SNew( STextBlock )
							.Text( Answers[AnswerIndex].Text )
						]
					]
				];
			}
		}
	}
	else if ( Block->GetStyle() == FQuestionBlock::Style_MultilineText )
	{
		for (int QuestionIndex = 0; QuestionIndex < Questions.Num(); QuestionIndex++)
		{
			ColumnIndex = 0;
			++RowIndex;

			Grid->AddSlot( ColumnIndex, RowIndex )
			[
				SNew( SBox )
				.WidthOverride( QuestionColumnWidth )
				.Padding( FMargin( 0, 0, 0, 8 ) )
				[					
					SNew( STextBlock )
					.Text( Questions[QuestionIndex] )
				]
			];

			for (int AnswerIndex = 0; AnswerIndex < Answers.Num(); AnswerIndex++)
			{
				++RowIndex;

				Grid->AddSlot( ColumnIndex, RowIndex )
				.ColumnSpan(2)
				[
					SNew( SHorizontalBox )
					+SHorizontalBox::Slot()
					.AutoWidth()
					.Padding( 20, 3 )
					.VAlign( VAlign_Center )
					[
						SNew( STextBlock )
						.Text( Answers[AnswerIndex].Text )
					]
					+SHorizontalBox::Slot()
					.Padding( FMargin( 0, 3, 10, 3 ) )
					[
						SNew( SEditableTextBox )
						.OnTextChanged( this, &SQuestionBlock::AnswerTextChanged, TWeakPtr< FQuestionBlock >( Block ), QuestionIndex, AnswerIndex )
						.OnTextCommitted( this, &SQuestionBlock::AnswerTextCommitted, TWeakPtr< FQuestionBlock >( Block ), QuestionIndex, AnswerIndex )
					]
				];
			}
		}

		++ColumnIndex;
		Grid->AddSlot( ColumnIndex, RowIndex );
		Grid->SetColumnFill( ColumnIndex, 1.0f );
	}
	else if ( Block->GetStyle() == FQuestionBlock::Style_Columns )
	{
		for (int AnswerIndex = 0; AnswerIndex < Answers.Num(); AnswerIndex++)
		{
			++ColumnIndex;

			Grid->AddSlot( ColumnIndex, RowIndex )
			.HAlign( HAlign_Center )
			.VAlign( VAlign_Bottom )
			.Padding( FMargin( 10, 0 ) )
			[
				SNew( SRichTextBlock )
				.Justification(ETextJustify::Center)
				.Text( Answers[AnswerIndex].Text )
				.AutoWrapText( true )
			];
		}

		for (int QuestionIndex = 0; QuestionIndex < Questions.Num(); QuestionIndex++)
		{
			ColumnIndex = 0;
			++RowIndex;

			Grid->AddSlot( ColumnIndex, RowIndex )
			[
				SNew( SBox )
				.WidthOverride( QuestionColumnWidth )
				[
					SNew( STextBlock )
					.Text( Questions[QuestionIndex] )
				]
			];

			for (int AnswerIndex = 0; AnswerIndex < Answers.Num(); AnswerIndex++)
			{
				++ColumnIndex;

				Grid->AddSlot( ColumnIndex, RowIndex )
				.HAlign( HAlign_Center )
				.VAlign( VAlign_Center )
				.Padding( FMargin( 0, 5  ) )
				[
					SNew( SCheckBox )
					.Style( CheckBoxStyle )
					.IsChecked( this, &SQuestionBlock::IsAnswerChecked, TWeakPtr< FQuestionBlock >( Block ), QuestionIndex, AnswerIndex )
					.OnCheckStateChanged( this, &SQuestionBlock::AnswerCheckStateChanged, TWeakPtr< FQuestionBlock >( Block ), QuestionIndex, AnswerIndex )
				];
			}
		}
	}

	TSharedPtr< SVerticalBox > QuestionBlockVerticalBox;

	this->ChildSlot
	[
		SNew( SHorizontalBox )
		+SHorizontalBox::Slot()
		.FillWidth( 1.0f )
		[
			SNew( SBorder )
			.BorderImage( FEditorStyle::GetBrush("ContentBrowser.ThumbnailShadow") )
			.Padding( 4 )
			[
				SNew( SBorder )
				.BorderImage( FEditorStyle::GetBrush("ToolPanel.GroupBorder") )
				.Padding( 5 )
				[
					SAssignNew( QuestionBlockVerticalBox, SVerticalBox )
				]
			]
		]
	];

	if ( !Block->GetInstructions().IsEmpty() )
	{
		QuestionBlockVerticalBox->AddSlot()
		.Padding( FMargin( 0, 5, 0, 10 ) )
		.AutoHeight()
		[
			SNew( STextBlock )
			.Text( Block->GetInstructions() )
			.AutoWrapText( true )
		];
	}

	QuestionBlockVerticalBox->AddSlot()
	.AutoHeight()
	[
		Grid
	];
}

ECheckBoxState SQuestionBlock::IsAnswerChecked( TWeakPtr< FQuestionBlock > BlockPtr, int32 QuestionIndex, int32 AnswerIndex ) const
{
	TSharedPtr< FQuestionBlock > Block =  BlockPtr.Pin();

	bool IsChecked = false;
	if ( Block.IsValid() )
	{
		IsChecked = Block->GetUserAnswers( QuestionIndex ).Contains( AnswerIndex );
	}

	return IsChecked ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SQuestionBlock::AnswerCheckStateChanged( ECheckBoxState CheckState, TWeakPtr< FQuestionBlock > BlockPtr, int32 QuestionIndex, int32 AnswerIndex )
{
	TSharedPtr< FQuestionBlock > Block =  BlockPtr.Pin();

	bool IsChecked = false;
	if ( Block.IsValid() )
	{
		IsChecked = Block->GetUserAnswers( QuestionIndex ).Contains( AnswerIndex );

		if ( Block->GetResponse() == FQuestionBlock::Response_Single )
		{
			IsChecked = false;
		}

		if ( IsChecked )
		{
			Block->UnmarkAnswerByIndex( QuestionIndex, AnswerIndex );
		}
		else
		{
			Block->MarkAnswerByIndex( QuestionIndex, AnswerIndex );
		}
	}
}

void SQuestionBlock::AnswerTextChanged( const FText& Text, TWeakPtr< FQuestionBlock > BlockPtr, int32 QuestionIndex, int32 AnswerIndex ) const
{
	TSharedPtr< FQuestionBlock > Block =  BlockPtr.Pin();

	if ( Block.IsValid() )
	{
		Block->SetUserTextAnswer( QuestionIndex, AnswerIndex, Text.ToString() );
	}
}

void SQuestionBlock::AnswerTextCommitted( const FText& Text, ETextCommit::Type CommitType, TWeakPtr< FQuestionBlock > BlockPtr, int32 QuestionIndex, int32 AnswerIndex ) const
{
	TSharedPtr< FQuestionBlock > Block =  BlockPtr.Pin();

	if ( Block.IsValid() )
	{
		Block->SetUserTextAnswer( QuestionIndex, AnswerIndex, Text.ToString() );
	}
}
