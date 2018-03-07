// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SequencerDisplayNodeDragDropOp.h"
#include "MovieSceneSequence.h"
#include "MovieScene.h"
#include "Sequencer.h"
#include "SequencerDisplayNode.h"
#include "SequencerObjectBindingNode.h"
#include "K2Node_GetSequenceBinding.h"
#include "SBorder.h"
#include "SImage.h"
#include "STextBlock.h"
#include "EditorStyleSet.h"
#include "EdGraph/EdGraph.h"

#define LOCTEXT_NAMESPACE "SequencerDisplayNodeDragDropOp"

TSharedRef<FSequencerDisplayNodeDragDropOp> FSequencerDisplayNodeDragDropOp::New(TArray<TSharedRef<FSequencerDisplayNode>>& InDraggedNodes, FText InDefaultText, const FSlateBrush* InDefaultIcon)
{
	TSharedRef<FSequencerDisplayNodeDragDropOp> NewOp = MakeShareable(new FSequencerDisplayNodeDragDropOp);

	NewOp->DraggedNodes = InDraggedNodes;
	NewOp->DefaultHoverText = NewOp->CurrentHoverText = InDefaultText;
	NewOp->DefaultHoverIcon = NewOp->CurrentIconBrush = InDefaultIcon;

	NewOp->Construct();
	return NewOp;
}

void FSequencerDisplayNodeDragDropOp::ResetToDefaultToolTip()
{
	CurrentHoverText = DefaultHoverText;
	CurrentIconBrush = DefaultHoverIcon;
}

void FSequencerDisplayNodeDragDropOp::Construct()
{
	FGraphEditorDragDropAction::Construct();

	SetFeedbackMessage(
		SNew(SBorder)
		.BorderImage(FEditorStyle::GetBrush("Graph.ConnectorFeedback.Border"))
		.Content()
		[
			SNew(SHorizontalBox)
			
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f, 0.0f, 3.0f, 0.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SImage)
				.Image(this, &FSequencerDisplayNodeDragDropOp::GetDecoratorIcon)
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock) 
				.Text(this, &FSequencerDisplayNodeDragDropOp::GetDecoratorText)
			]
		]
	);
}

FGuid GetBindingID(TSharedRef<FSequencerDisplayNode> InNode, UMovieSceneSequence* Sequence)
{
	if (InNode->GetType() != ESequencerNode::Object)
	{
		return FGuid();
	}

	TSharedRef<FSequencerObjectBindingNode> ObjectBinding = StaticCastSharedRef<FSequencerObjectBindingNode>(InNode);
	UMovieScene* MovieScene = Sequence->GetMovieScene();

	const FMovieScenePossessable* Possessable = MovieScene->FindPossessable(ObjectBinding->GetObjectBinding());
	if (Possessable && !Sequence->CanRebindPossessable(*Possessable))
	{
		return FGuid();
	}

	return ObjectBinding->GetObjectBinding();
}

FSequencer* FSequencerDisplayNodeDragDropOp::GetSequencer() const
{
	for (TSharedRef<FSequencerDisplayNode> Node : DraggedNodes)
	{
		return &Node->GetSequencer();
	}
	return nullptr;
}

TArray<FMovieSceneObjectBindingID> FSequencerDisplayNodeDragDropOp::GetDraggedBindings() const
{
	TArray<FMovieSceneObjectBindingID> Bindings;

	FSequencer* Sequencer = GetSequencer();
	if (!Sequencer)
	{
		return Bindings;
	}

	// Binding IDs always resolve from the root sequence
	UMovieSceneSequence* Sequence = Sequencer->GetRootMovieSceneSequence();
	FMovieSceneSequenceID SequenceID = Sequencer->GetFocusedTemplateID();

	for (TSharedRef<FSequencerDisplayNode> Node : DraggedNodes)
	{
		FGuid ObjectBindingID = GetBindingID(Node, Sequence);
		if (!ObjectBindingID.IsValid())
		{
			// To avoid confusion over what is overridable, if anything is invalid, the entire drag is invalid
			return TArray<FMovieSceneObjectBindingID>();
		}

		Bindings.Emplace(ObjectBindingID, SequenceID);
	}

	return Bindings;
}

void FSequencerDisplayNodeDragDropOp::HoverTargetChanged()
{
	if (GetHoveredGraph() && GetDraggedBindings().Num() > 0)
	{
		CurrentHoverText = LOCTEXT("CreateNode", "Add binding ID to graph");
		CurrentIconBrush = FEditorStyle::GetBrush( TEXT( "Graph.ConnectorFeedback.NewNode" ) );
	}
	else
	{
		ResetToDefaultToolTip();
	}
}

FReply FSequencerDisplayNodeDragDropOp::DroppedOnPanel( const TSharedRef< class SWidget >& Panel, FVector2D ScreenPosition, FVector2D GraphPosition, UEdGraph& Graph)
{
	FSequencer* Sequencer = GetSequencer();
	if (!Sequencer || GetDraggedBindings().Num() == 0)
	{
		return FReply::Unhandled();
	}

	// Binding IDs always resolve from the root sequence
	UMovieSceneSequence* Sequence = Sequencer->GetRootMovieSceneSequence();

	const UEdGraphSchema* Schema = Graph.GetSchema();
	if (Schema)
	{
		UK2Node_GetSequenceBinding* Template = NewObject<UK2Node_GetSequenceBinding>(GetTransientPackage());
		Template->SourceSequence = Sequence;

		FEdGraphSchemaAction_NewNode Action;
		Action.NodeTemplate = Template;

		FMovieSceneSequenceID SequenceID = Sequencer->GetFocusedTemplateID();

		for (TSharedRef<FSequencerDisplayNode> Node : DraggedNodes)
		{
			FGuid ObjectBindingID = GetBindingID(Node, Sequence);
			if (ObjectBindingID.IsValid())
			{
				Template->Binding = FMovieSceneObjectBindingID(ObjectBindingID, SequenceID);
				UEdGraphNode* NewNode = Action.PerformAction(&Graph, GetHoveredPin(), GraphPosition, false);

				int32 Offset = FMath::Max(NewNode->NodeHeight, 100);
				Offset += Offset % 16;
				GraphPosition.Y += Offset;
			}
		}
	}
	
	return FReply::Handled();
}

TArray<TSharedRef<FSequencerDisplayNode>>& FSequencerDisplayNodeDragDropOp::GetDraggedNodes()
{
	return DraggedNodes;
}

#undef LOCTEXT_NAMESPACE
