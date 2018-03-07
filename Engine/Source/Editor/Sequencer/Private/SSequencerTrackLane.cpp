// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SSequencerTrackLane.h"
#include "Rendering/DrawElements.h"
#include "EditorStyleSet.h"
#include "SSequencerTreeView.h"

class SResizeArea : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SResizeArea){}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TWeakPtr<FSequencerDisplayNode> InResizeTarget)
	{
		WeakResizeTarget = InResizeTarget;
		ChildSlot
		[
			SNew(SBox).HeightOverride(5.f)
		];
	}

	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		TSharedPtr<FSequencerDisplayNode> ResizeTarget = WeakResizeTarget.Pin();
		if (ResizeTarget.IsValid() && MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
		{
			DragParameters = FDragParameters(ResizeTarget->GetNodeHeight(), MouseEvent.GetScreenSpacePosition().Y);
			return FReply::Handled().CaptureMouse(AsShared());
		}
		return FReply::Handled();
	}

	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		DragParameters.Reset();
		return FReply::Handled().ReleaseMouseCapture();
	}

	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		if (DragParameters.IsSet() && HasMouseCapture())
		{
			float NewHeight = DragParameters->OriginalHeight + (MouseEvent.GetScreenSpacePosition().Y - DragParameters->DragStartY);

			TSharedPtr<FSequencerDisplayNode> ResizeTarget = WeakResizeTarget.Pin();
			if (ResizeTarget.IsValid() && FMath::RoundToInt(NewHeight) != FMath::RoundToInt(DragParameters->OriginalHeight))
			{
				ResizeTarget->Resize(NewHeight);
			}
		}

		return FReply::Handled();
	}

	virtual FCursorReply OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const
	{
		return FCursorReply::Cursor(EMouseCursor::ResizeUpDown);
	}

private:

	struct FDragParameters
	{
		FDragParameters(float InOriginalHeight, float InDragStartY) : OriginalHeight(InOriginalHeight), DragStartY(InDragStartY) {}

		float OriginalHeight;
		float DragStartY;
	};
	TOptional<FDragParameters> DragParameters;
	TWeakPtr<FSequencerDisplayNode> WeakResizeTarget;
};


void SSequencerTrackLane::Construct(const FArguments& InArgs, const TSharedRef<FSequencerDisplayNode>& InDisplayNode, const TSharedRef<SSequencerTreeView>& InTreeView)
{
	DisplayNode = InDisplayNode;
	TreeView = InTreeView;

	TSharedRef<SWidget> Widget = InArgs._Content.Widget;

	if (DisplayNode->IsResizable())
	{
		Widget = SNew(SOverlay)
			+ SOverlay::Slot()
			[
				InArgs._Content.Widget
			]

			+ SOverlay::Slot()
			.VAlign(VAlign_Bottom)
			[
				SNew(SResizeArea, DisplayNode)
			];
	}

	SetVisibility(EVisibility::SelfHitTestInvisible);

	ChildSlot
	.HAlign(HAlign_Fill)
	.Padding(0)
	[
		Widget
	];
}


void DrawLaneRecursive(const TSharedRef<FSequencerDisplayNode>& DisplayNode, const FGeometry& AllottedGeometry, float& YOffs, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle)
{
	static const FName BorderName("Sequencer.AnimationOutliner.DefaultBorder");
	static const FName SelectionColorName("SelectionColor");

	if (DisplayNode->IsHidden())
	{
		return;
	}
	
	float TotalNodeHeight = DisplayNode->GetNodeHeight() + DisplayNode->GetNodePadding().Combined();

	// draw selection border
	FSequencerSelection& SequencerSelection = DisplayNode->GetSequencer().GetSelection();

	if (SequencerSelection.IsSelected(DisplayNode))
	{
		FLinearColor SelectionColor = FEditorStyle::GetSlateColor(SelectionColorName).GetColor(InWidgetStyle);

		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId,
			AllottedGeometry.ToPaintGeometry(
				FVector2D(0, YOffs),
				FVector2D(AllottedGeometry.GetLocalSize().X, TotalNodeHeight)
			),
			FEditorStyle::GetBrush(BorderName),
			ESlateDrawEffect::None,
			SelectionColor
		);
	}

	// draw hovered or node has keys or sections selected border
	else
	{
		FLinearColor HighlightColor;
		bool bDrawHighlight = false;
		if (SequencerSelection.NodeHasSelectedKeysOrSections(DisplayNode))
		{
			bDrawHighlight = true;
			HighlightColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.15f);
		}
		else if (DisplayNode->IsHovered())
		{
			bDrawHighlight = true;
			HighlightColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.05f);
		}

		if (bDrawHighlight)
		{
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				LayerId,
				AllottedGeometry.ToPaintGeometry(
					FVector2D(0, YOffs),
					FVector2D(AllottedGeometry.GetLocalSize().X, TotalNodeHeight)
				),
				FEditorStyle::GetBrush(BorderName),
				ESlateDrawEffect::None,
				HighlightColor
			);
		}
	}

	YOffs += TotalNodeHeight;

	if (DisplayNode->IsExpanded())
	{
		for (const auto& ChildNode : DisplayNode->GetChildNodes())
		{
			DrawLaneRecursive(ChildNode, AllottedGeometry, YOffs, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle);
		}
	}
}

int32 SSequencerTrackLane::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	float YOffs = 0;
	DrawLaneRecursive(DisplayNode.ToSharedRef(), AllottedGeometry, YOffs, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle);

	return SCompoundWidget::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId + 1, InWidgetStyle, bParentEnabled);
}

void SSequencerTrackLane::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	FVector2D ThisFrameDesiredSize = GetDesiredSize();

	if (LastDesiredSize.IsSet() && ThisFrameDesiredSize.Y != LastDesiredSize.GetValue().Y)
	{
		TSharedPtr<SSequencerTreeView> PinnedTree = TreeView.Pin();
		if (PinnedTree.IsValid())
		{
			PinnedTree->RequestTreeRefresh();
		}
	}

	LastDesiredSize = ThisFrameDesiredSize;
}

FVector2D SSequencerTrackLane::ComputeDesiredSize(float LayoutScale) const
{
	float Height = DisplayNode->GetNodeHeight() + DisplayNode->GetNodePadding().Combined();
	
	
	if (DisplayNode->GetType() == ESequencerNode::Track || DisplayNode->GetType() == ESequencerNode::Category)
	{
		const bool bIncludeThisNode = false;
		
		// These types of nodes need to consider the entire visible hierarchy for their desired size
		DisplayNode->TraverseVisible_ParentFirst([&](FSequencerDisplayNode& InNode){
			Height += InNode.GetNodeHeight() + InNode.GetNodePadding().Combined();
			return true;
		}, bIncludeThisNode);
	}

	return FVector2D(100.f, Height);
}

float SSequencerTrackLane::GetPhysicalPosition() const
{
	return TreeView.Pin()->ComputeNodePosition(DisplayNode.ToSharedRef()).Get(0.f);
}
