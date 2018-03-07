// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SGraphNodeSoundResult.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Images/SImage.h"
#include "SoundCueGraph/SoundCueGraphNode_Base.h"

void SGraphNodeSoundResult::Construct(const FArguments& InArgs, USoundCueGraphNode_Base* InNode)
{
	this->GraphNode = InNode;

	this->SetCursor(EMouseCursor::CardinalCross);

	this->UpdateGraphNode();
}

TSharedRef<SWidget> SGraphNodeSoundResult::CreateNodeContentArea()
{
	return SNew(SBorder)
		.BorderImage(FEditorStyle::GetBrush("NoBorder"))
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		.Padding(FMargin(0, 3))
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.FillWidth(1.0f)
			[
				// LEFT
				SAssignNew(LeftNodeBox, SVerticalBox)
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SImage)
				.Image(FEditorStyle::GetBrush("Graph.SoundResultNode.Body"))
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Right)
			[
				// RIGHT
				SAssignNew(RightNodeBox, SVerticalBox)
			]
		];
}
