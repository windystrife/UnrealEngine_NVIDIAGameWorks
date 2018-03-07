// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Tools/SequencerEntityVisitor.h"
#include "IKeyArea.h"
#include "DisplayNodes/SequencerTrackNode.h"
#include "GroupedKeyArea.h"
#include "MovieSceneTrack.h"

FSequencerEntityRange::FSequencerEntityRange(const TRange<float>& InRange)
	: StartTime(InRange.GetLowerBoundValue()), EndTime(InRange.GetUpperBoundValue())
{
}

FSequencerEntityRange::FSequencerEntityRange(FVector2D TopLeft, FVector2D BottomRight)
	: StartTime(TopLeft.X), EndTime(BottomRight.X)
	, VerticalTop(TopLeft.Y), VerticalBottom(BottomRight.Y)
{
}

bool FSequencerEntityRange::IntersectSection(const UMovieSceneSection* InSection, const TSharedRef<FSequencerTrackNode>& InTrackNode, int32 MaxRowIndex) const
{
	// Test horizontal bounds
	if (!InSection->IsInfinite() && (InSection->GetStartTime() > EndTime || InSection->GetEndTime() < StartTime))
	{
		return false;
	}
	// Test vertical bounds
	else if (MaxRowIndex > 0 && VerticalTop.IsSet())
	{
		const float NodeTop = InTrackNode->GetVirtualTop();
		const float NodeBottom = InTrackNode->GetVirtualBottom();

		const float RowHeight = (NodeBottom - NodeTop) / (MaxRowIndex + 1);
		const float RowTop = NodeTop + InSection->GetRowIndex() * RowHeight;

		return RowTop <= VerticalBottom.GetValue() && RowTop + RowHeight >= VerticalTop.GetValue();
	}
	else
	{
		return true;
	}
}

bool FSequencerEntityRange::IntersectNode(TSharedRef<FSequencerDisplayNode> InNode) const
{
	if (VerticalTop.IsSet())
	{
		return InNode->GetVirtualTop() <= VerticalBottom.GetValue() && InNode->GetVirtualBottom() >= VerticalTop.GetValue();
	}
	return true;
}

bool FSequencerEntityRange::IntersectKeyArea(TSharedRef<FSequencerDisplayNode> InNode, float VirtualKeyHeight) const
{
	if (VerticalTop.IsSet())
	{
		const float NodeCenter = InNode->GetVirtualTop() + (InNode->GetVirtualBottom() - InNode->GetVirtualTop())/2;
		return NodeCenter + VirtualKeyHeight/2 > VerticalTop.GetValue() && NodeCenter - VirtualKeyHeight/2 < VerticalBottom.GetValue();
	}
	return true;
}

FSequencerEntityWalker::FSequencerEntityWalker(const FSequencerEntityRange& InRange, FVector2D InVirtualKeySize)
	: Range(InRange), VirtualKeySize(InVirtualKeySize)
{}

/* @todo: Could probably optimize this by not walking every single node, and binary searching the begin<->end ranges instead */
void FSequencerEntityWalker::Traverse(const ISequencerEntityVisitor& Visitor, const TArray< TSharedRef<FSequencerDisplayNode> >& Nodes)
{
	for (auto& Child : Nodes)
	{
		if (!Child->IsHidden())
		{
			HandleNode(Visitor, Child);
		}
	}
}

void FSequencerEntityWalker::HandleNode(const ISequencerEntityVisitor& Visitor, const TSharedRef<FSequencerDisplayNode>& InNode)
{
	if (InNode->GetType() == ESequencerNode::Track)
	{
		HandleTrackNode(Visitor, StaticCastSharedRef<FSequencerTrackNode>(InNode));
	}

	if (InNode->IsExpanded())
	{
		for (auto& Child : InNode->GetChildNodes())
		{
			if (!Child->IsHidden())
			{
				HandleNode(Visitor, Child);
			}
		}
	}
}

void FSequencerEntityWalker::HandleTrackNode(const ISequencerEntityVisitor& Visitor, const TSharedRef<FSequencerTrackNode>& InTrackNode)
{
	TArray<TSharedRef<ISequencerSection>> Sections = InTrackNode->GetSections();

	if (Range.IntersectNode(InTrackNode))
	{

		int32 MaxRowIndex;
		if (InTrackNode->GetSubTrackMode() == FSequencerTrackNode::ESubTrackMode::None)
		{
			MaxRowIndex = InTrackNode->GetTrack()->GetMaxRowIndex();
		}
		else
		{
			// When using sub-tracks each section index gets it's own track so the effective max index
			// within the track will always be 0.
			MaxRowIndex = 0;
		}

		// Prune the selections to anything that is in the range, visiting if necessary
		for (int32 SectionIndex = 0; SectionIndex < Sections.Num();)
		{
			UMovieSceneSection* Section = Sections[SectionIndex]->GetSectionObject();
			if (!Range.IntersectSection(Section, InTrackNode, MaxRowIndex))
			{
				Sections.RemoveAtSwap(SectionIndex, 1, false);
				continue;
			}

			if (Visitor.CheckEntityMask(ESequencerEntity::Section))
			{
				Visitor.VisitSection(Section, InTrackNode);
			}

			++SectionIndex;
		}

		HandleSingleNode(Visitor, InTrackNode, Sections);
	}

	if (InTrackNode->IsExpanded())
	{
		// Handle Children
		for (auto& Child : InTrackNode->GetChildNodes())
		{
			if (!Child->IsHidden())
			{
				HandleChildNode(Visitor, Child, Sections);
			}
		}
	}
}

void FSequencerEntityWalker::HandleChildNode(const ISequencerEntityVisitor& Visitor, const TSharedRef<FSequencerDisplayNode>& InNode, const TArray<TSharedRef<ISequencerSection>>& InSections)
{
	if (Range.IntersectNode(InNode))
	{
		HandleSingleNode(Visitor, InNode, InSections);
	}

	if (InNode->IsExpanded())
	{
		// Handle Children
		for (auto& Child : InNode->GetChildNodes())
		{
			if (!Child->IsHidden())
			{
				HandleChildNode(Visitor, Child, InSections);
			}
		}
	}
}

void FSequencerEntityWalker::HandleSingleNode(const ISequencerEntityVisitor& Visitor, const TSharedRef<FSequencerDisplayNode>& InNode, const TArray<TSharedRef<ISequencerSection>>& InSections)
{
	bool bNodeHasKeyArea = false;
	if (InNode->GetType() == ESequencerNode::KeyArea)
	{
		HandleKeyAreaNode(Visitor, StaticCastSharedRef<FSequencerSectionKeyAreaNode>(InNode), InNode, InSections);
		bNodeHasKeyArea = true;
	}
	else if (InNode->GetType() == ESequencerNode::Track)
	{
		TSharedPtr<FSequencerSectionKeyAreaNode> SectionKeyNode = StaticCastSharedRef<FSequencerTrackNode>(InNode)->GetTopLevelKeyNode();
		if (SectionKeyNode.IsValid())
		{
			HandleKeyAreaNode(Visitor, SectionKeyNode.ToSharedRef(), InNode, InSections);
			bNodeHasKeyArea = true;
		}
	}

	// As a fallback, we need to handle:
	//  - Key groupings on collapsed parents
	//  - Sections that have no key areas
	const bool bIterateKeyGroupings = Visitor.CheckEntityMask(ESequencerEntity::Key) &&
		!bNodeHasKeyArea &&
		(!InNode->IsExpanded() || InNode->GetChildNodes().Num() == 0);

	if (bIterateKeyGroupings)
	{
		for (TSharedRef<ISequencerSection> SectionInterface : InSections)
		{
			UMovieSceneSection* Section = SectionInterface->GetSectionObject();

			if (Visitor.CheckEntityMask(ESequencerEntity::Key))
			{
				// Only handle grouped keys if we actually have children
				if (InNode->GetChildNodes().Num() != 0 && Range.IntersectKeyArea(InNode, VirtualKeySize.X))
				{
					TSharedRef<IKeyArea> KeyArea = InNode->GetKeyGrouping(Section);
					HandleKeyArea(Visitor, KeyArea, Section, InNode);
				}
			}
		}
	}
}

void FSequencerEntityWalker::HandleKeyAreaNode(const ISequencerEntityVisitor& Visitor, const TSharedRef<FSequencerSectionKeyAreaNode>& InKeyAreaNode, const TSharedRef<FSequencerDisplayNode>& InOwnerNode, const TArray<TSharedRef<ISequencerSection>>& InSections)
{
	for (TSharedRef<ISequencerSection> SectionInterface : InSections)
	{
		UMovieSceneSection* Section = SectionInterface->GetSectionObject();
		if (Visitor.CheckEntityMask(ESequencerEntity::Key))
		{
			if (Range.IntersectKeyArea(InOwnerNode, VirtualKeySize.X))
			{
				TSharedPtr<IKeyArea> KeyArea = InKeyAreaNode->GetKeyArea(Section);
				if (KeyArea.IsValid())
				{
					HandleKeyArea(Visitor, KeyArea.ToSharedRef(), Section, InOwnerNode);
				}
			}
		}
	}
}

void FSequencerEntityWalker::HandleKeyArea(const ISequencerEntityVisitor& Visitor, const TSharedRef<IKeyArea>& KeyArea, UMovieSceneSection* Section, const TSharedRef<FSequencerDisplayNode>& InNode)
{
	for (FKeyHandle KeyHandle : KeyArea->GetUnsortedKeyHandles())
	{
		float KeyPosition = KeyArea->GetKeyTime(KeyHandle);
		if (KeyPosition + VirtualKeySize.X/2 > Range.StartTime &&
			KeyPosition - VirtualKeySize.X/2 < Range.EndTime)
		{
			Visitor.VisitKey(KeyHandle, KeyPosition, KeyArea, Section, InNode);
		}
	}
}
