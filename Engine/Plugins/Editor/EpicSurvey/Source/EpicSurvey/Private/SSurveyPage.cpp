// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SSurveyPage.h"
#include "Widgets/SBoxPanel.h"
#include "SQuestionBlock.h"

void SSurveyPage::Construct( const FArguments& Args, const TSharedRef< FSurveyPage >& Page )
{
	TArray< TSharedRef< FQuestionBlock > > Blocks = Page->GetBlocks();

	TSharedPtr<SVerticalBox> TargetBox;
	this->ChildSlot
		[
			SAssignNew(TargetBox,SVerticalBox)
		];

	for (int BlockIndex = 0; BlockIndex < Blocks.Num(); BlockIndex++)
	{
		const TSharedRef< FQuestionBlock > Block = Blocks[BlockIndex];

		TargetBox->AddSlot()
		.AutoHeight()
		[
			SNew(SQuestionBlock, Block )
		];
	}
}
