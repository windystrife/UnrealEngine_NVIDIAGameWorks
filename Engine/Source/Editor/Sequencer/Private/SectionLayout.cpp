// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SectionLayout.h"
#include "GroupedKeyArea.h"

FSectionLayoutElement FSectionLayoutElement::FromGroup(const TSharedRef<FSequencerDisplayNode>& InNode, const TSharedRef<FGroupedKeyArea>& InKeyAreaGroup, float InOffset)
{
	FSectionLayoutElement Tmp;
	Tmp.Type = Group;
	Tmp.KeyArea = InKeyAreaGroup;
	Tmp.LocalOffset = InOffset;
	Tmp.Height = InNode->GetNodeHeight();
	Tmp.DisplayNode = InNode;
	return Tmp;
}

FSectionLayoutElement FSectionLayoutElement::FromKeyAreaNode(const TSharedRef<FSequencerSectionKeyAreaNode>& InKeyAreaNode, UMovieSceneSection* InSection, float InOffset)
{
	FSectionLayoutElement Tmp;
	Tmp.Type = Single;
	Tmp.KeyArea = InKeyAreaNode->GetKeyArea(InSection);
	Tmp.LocalOffset = InOffset;
	Tmp.DisplayNode = InKeyAreaNode;
	Tmp.Height = InKeyAreaNode->GetNodeHeight();
	return Tmp;
}

FSectionLayoutElement FSectionLayoutElement::FromTrack(const TSharedRef<FSequencerTrackNode>& InTrackNode, UMovieSceneSection* InSection, float InOffset)
{
	FSectionLayoutElement Tmp;
	Tmp.Type = Single;
	Tmp.KeyArea = InTrackNode->GetTopLevelKeyNode()->GetKeyArea(InSection);
	Tmp.LocalOffset = InOffset;
	Tmp.DisplayNode = InTrackNode;
	Tmp.Height = InTrackNode->GetNodeHeight();
	return Tmp;
}

FSectionLayoutElement FSectionLayoutElement::EmptySpace(const TSharedRef<FSequencerDisplayNode>& InNode, float InOffset)
{
	FSectionLayoutElement Tmp;
	Tmp.Type = Single;
	Tmp.LocalOffset = InOffset;
	Tmp.DisplayNode = InNode;
	Tmp.Height = InNode->GetNodeHeight();
	return Tmp;
}

FSectionLayoutElement::EType FSectionLayoutElement::GetType() const
{
	return Type;
}

float FSectionLayoutElement::GetOffset() const
{
	return LocalOffset;
}

float FSectionLayoutElement::GetHeight() const
{
	return Height;
}

TSharedPtr<IKeyArea> FSectionLayoutElement::GetKeyArea() const
{
	return KeyArea;
}

TSharedPtr<FSequencerDisplayNode> FSectionLayoutElement::GetDisplayNode() const
{
	return DisplayNode;
}

FSectionLayout::FSectionLayout(FSequencerTrackNode& TrackNode, int32 InSectionIndex)
{
	UMovieSceneSection* Section = TrackNode.GetSections()[InSectionIndex]->GetSectionObject();

	auto SetupKeyArea = [this, Section](FSequencerDisplayNode& Node, float Offset){

		if (Node.GetType() == ESequencerNode::KeyArea && static_cast<FSequencerSectionKeyAreaNode&>(Node).GetKeyArea(Section).IsValid())
		{
			Elements.Add(FSectionLayoutElement::FromKeyAreaNode(
				StaticCastSharedRef<FSequencerSectionKeyAreaNode>(Node.AsShared()),
				Section,
				Offset
			));
		}
		else if (Node.GetType() == ESequencerNode::Track && static_cast<FSequencerTrackNode&>(Node).GetTopLevelKeyNode().IsValid())
		{
			Elements.Add(FSectionLayoutElement::FromTrack(
				StaticCastSharedRef<FSequencerTrackNode>(Node.AsShared()),
				Section,
				Offset
			));
		}
		else if (Node.GetChildNodes().Num() && !Node.IsExpanded())
		{
			Elements.Add(FSectionLayoutElement::FromGroup(
				Node.AsShared(),
				Node.GetKeyGrouping(Section),
				Offset
			));
		}
		else
		{
			// It's benign space
			Elements.Add(FSectionLayoutElement::EmptySpace(
				Node.AsShared(),
				Offset
			));
		}

	};

	float VerticalOffset = 0.f;

	// First, layout the parent
	{
		VerticalOffset += TrackNode.GetNodePadding().Top;

		SetupKeyArea(TrackNode, VerticalOffset);

		VerticalOffset += TrackNode.GetNodeHeight() + TrackNode.GetNodePadding().Bottom;
	}

	// Then any children
	TrackNode.TraverseVisible_ParentFirst([&](FSequencerDisplayNode& Node){
		
		VerticalOffset += Node.GetNodePadding().Top;

		SetupKeyArea(Node, VerticalOffset);

		VerticalOffset += Node.GetNodeHeight() + Node.GetNodePadding().Bottom;
		return true;

	}, false);
}

const TArray<FSectionLayoutElement>& FSectionLayout::GetElements() const
{
	return Elements;
}

float FSectionLayout::GetTotalHeight() const
{
	if (Elements.Num())
	{
		return Elements.Last().GetOffset() + Elements.Last().GetDisplayNode()->GetNodePadding().Combined() + Elements.Last().GetHeight();
	}
	return 0.f;
}
