// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "KismetNodes/SGraphNodeK2Terminator.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Images/SImage.h"
#include "K2Node.h"


void SGraphNodeK2Terminator::Construct( const FArguments& InArgs, UK2Node* InNode )
{
	this->GraphNode = InNode;

	this->SetCursor( EMouseCursor::CardinalCross );

	this->UpdateGraphNode();
}

void SGraphNodeK2Terminator::UpdateGraphNode()
{
	check(GraphNode != NULL);

	InputPins.Empty();
	OutputPins.Empty();

	// Reset variables that are going to be exposed, in case we are refreshing an already setup node.
	RightNodeBox.Reset();
	LeftNodeBox.Reset();
	
	UK2Node* K2Node = CastChecked<UK2Node>(GraphNode);
	const FSlateBrush* TopBrush = NULL;
	const FSlateBrush* BottomBrush = NULL;

	if(K2Node->DrawNodeAsEntry())
	{
		TopBrush = FEditorStyle::GetBrush(TEXT("Graph.Node.NodeEntryTop"));
		BottomBrush = FEditorStyle::GetBrush(TEXT("Graph.Node.NodeEntryBottom"));
	}
	else
	{
		TopBrush = FEditorStyle::GetBrush(TEXT("Graph.Node.NodeExitTop"));
		BottomBrush = FEditorStyle::GetBrush(TEXT("Graph.Node.NodeExitBottom"));
	}


	//
	//             ______________________
	//            |      TITLE AREA      |
	//            +-------+------+-------+
	//            | (>) L |      | R (>) |
	//            | (>) E |      | I (>) |
	//            | (>) F |      | G (>) |
	//            | (>) T |      | H (>) |
	//            |       |      | T (>) |
	//            |_______|______|_______|
	//
	this->ContentScale.Bind( this, &SGraphNode::GetContentScale );
	this->GetOrAddSlot( ENodeZone::Center )
	.HAlign(HAlign_Center)
	.VAlign(VAlign_Center)
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		[
			SNew(SImage)
			.ColorAndOpacity( this, &SGraphNode::GetNodeTitleColor )
			.Image(TopBrush)
		]
		+SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Top)
		[
			SNew(SBorder)
			. Padding( 3 )
			. BorderImage( FEditorStyle::GetBrush(TEXT("WhiteTexture")) )
			. HAlign(HAlign_Center)
			. BorderBackgroundColor( this, &SGraphNode::GetNodeTitleColor )
			[
				SNew(SNodeTitle, GraphNode)
			]
		]
		+SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Top)
		[
			// NODE CONTENT AREA
			SNew(SBorder)
			.BorderImage( FEditorStyle::GetBrush(TEXT("Graph.Node.NodeBackground")) )
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			.Padding( FMargin(0,3) )
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Left)
				[
					// LEFT
					SAssignNew(LeftNodeBox, SVerticalBox)
				]
				+SHorizontalBox::Slot()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Fill)
				.FillWidth(1.0f)
				[
					// MIDDLE
					SNew(SImage)
					.Image( FEditorStyle::GetBrush(TEXT("WhiteTexture")) )
					.ColorAndOpacity(FLinearColor(1.f,1.f,1.f,0.f))
				]
				+SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Right)
				[
					// RIGHT
					SAssignNew(RightNodeBox, SVerticalBox)
				]
			]
		]
		+SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		[
			SNew(SImage)
			.ColorAndOpacity( this, &SGraphNode::GetNodeTitleColor )
			.Image(BottomBrush)
		]
	];

	CreatePinWidgets();
}

const FSlateBrush* SGraphNodeK2Terminator::GetShadowBrush(bool bSelected) const
{
	UK2Node* K2Node = CastChecked<UK2Node>(GraphNode);

	if(K2Node->DrawNodeAsEntry())
	{
		return bSelected ? FEditorStyle::GetBrush(TEXT("Graph.Node.NodeEntryShadowSelected")) : FEditorStyle::GetBrush(TEXT("Graph.Node.NodeEntryShadow"));
	}
	else
	{
		return bSelected ? FEditorStyle::GetBrush(TEXT("Graph.Node.NodeExitShadowSelected")) : FEditorStyle::GetBrush(TEXT("Graph.Node.NodeExitShadow"));
	}
}
