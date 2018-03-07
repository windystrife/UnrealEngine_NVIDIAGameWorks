// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "KismetNodes/SGraphNodeK2Sequence.h"
#include "EdGraph/EdGraph.h"
#include "Widgets/SBoxPanel.h"
#include "GraphEditorSettings.h"
#include "K2Node_AddPinInterface.h"
#include "K2Node.h"
#include "ScopedTransaction.h"


void SGraphNodeK2Sequence::Construct( const FArguments& InArgs, UK2Node* InNode )
{
	ensure(InNode == nullptr || InNode->GetClass()->ImplementsInterface(UK2Node_AddPinInterface::StaticClass()));
	GraphNode = InNode;

	SetCursor( EMouseCursor::CardinalCross );

	UpdateGraphNode();
}

void SGraphNodeK2Sequence::CreateOutputSideAddButton(TSharedPtr<SVerticalBox> OutputBox)
{
	TSharedRef<SWidget> AddPinButton = AddPinButtonContent(
		NSLOCTEXT("SequencerNode", "SequencerNodeAddPinButton", "Add pin"),
		NSLOCTEXT("SequencerNode", "SequencerNodeAddPinButton_ToolTip", "Add new pin"));

	FMargin AddPinPadding = Settings->GetOutputPinPadding();
	AddPinPadding.Top += 6.0f;

	OutputBox->AddSlot()
	.AutoHeight()
	.VAlign(VAlign_Center)
	.Padding(AddPinPadding)
	[
		AddPinButton
	];
}

FReply SGraphNodeK2Sequence::OnAddPin()
{
	IK2Node_AddPinInterface* AddPinNode = Cast<IK2Node_AddPinInterface>(GraphNode);
	ensure(AddPinNode);
	if (AddPinNode && AddPinNode->CanAddPin())
	{
		FScopedTransaction Transaction(NSLOCTEXT("SequencerNode", "AddPinTransaction", "Add Pin"));

		AddPinNode->AddInputPin();
		UpdateGraphNode();
		GraphNode->GetGraph()->NotifyGraphChanged();
	}
	
	return FReply::Handled();
}
