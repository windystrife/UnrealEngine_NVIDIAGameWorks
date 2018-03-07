// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AnimationNodes/SAnimationGraphNode.h"
#include "Widgets/SBoxPanel.h"
#include "Layout/WidgetPath.h"
#include "Framework/Application/MenuStack.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "AnimGraphNode_Base.h"
#include "IDocumentation.h"
#include "AnimationEditorUtils.h"

class SPoseViewColourPickerPopup : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SPoseViewColourPickerPopup)
	{}
	SLATE_ARGUMENT(TWeakObjectPtr< UPoseWatch >, PoseWatch)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		PoseWatch = InArgs._PoseWatch;

		static FColor PoseWatchColours[] = { FColor::Red, FColor::Green, FColor::Blue, FColor::Cyan, FColor::Orange, FColor::Purple, FColor::Yellow, FColor::Black };

		const int32 Rows = 2;
		const int32 Columns = 4;

		TSharedPtr<SVerticalBox> Layout = SNew(SVerticalBox);

		for (int32 RowIndex = 0; RowIndex < Rows; ++RowIndex)
		{
			TSharedPtr<SHorizontalBox> Row = SNew(SHorizontalBox);

			for (int32 RowItem = 0; RowItem < Columns; ++RowItem)
			{
				int32 ColourIndex = RowItem + (RowIndex * Columns);
				FColor Colour = PoseWatchColours[ColourIndex];

				Row->AddSlot()
				.Padding(5.f, 2.f)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.OnClicked(this, &SPoseViewColourPickerPopup::NewPoseWatchColourPicked, Colour)
					.ButtonColorAndOpacity(Colour)
				];

			}

			Layout->AddSlot()
			[
				Row.ToSharedRef()
			];
		}

		Layout->AddSlot()
			.AutoHeight()
			.Padding(5.f, 2.f)
			[
				SNew(SButton)
				.Text(NSLOCTEXT("AnimationGraphNode", "RemovePoseWatch", "Remove Pose Watch"))
				.OnClicked(this, &SPoseViewColourPickerPopup::RemovePoseWatch)
			];

		this->ChildSlot
			[
				SNew(SBorder)
				.BorderImage(FEditorStyle::GetBrush(TEXT("Menu.Background")))
				.Padding(10)
				[
					Layout->AsShared()
				]
			];
	}

private:
	FReply NewPoseWatchColourPicked(FColor NewColour)
	{
		if (UPoseWatch* CurPoseWatch = PoseWatch.Get())
		{
			AnimationEditorUtils::UpdatePoseWatchColour(CurPoseWatch, NewColour);
		}
		FSlateApplication::Get().DismissAllMenus();
		return FReply::Handled();
	}

	FReply RemovePoseWatch()
	{
		if (UPoseWatch* CurPoseWatch = PoseWatch.Get())
		{
			AnimationEditorUtils::RemovePoseWatch(CurPoseWatch);
		}
		FSlateApplication::Get().DismissAllMenus();
		return FReply::Handled();
	}

	TWeakObjectPtr<UPoseWatch> PoseWatch;
};

void SAnimationGraphNode::Construct(const FArguments& InArgs, UAnimGraphNode_Base* InNode)
{
	this->GraphNode = InNode;

	this->SetCursor(EMouseCursor::CardinalCross);

	this->UpdateGraphNode();

	const FSlateBrush* ImageBrush = FEditorStyle::Get().GetBrush(TEXT("Graph.AnimationFastPathIndicator"));

	IndicatorWidget =
		SNew(SImage)
		.Image(ImageBrush)
		.ToolTip(IDocumentation::Get()->CreateToolTip(NSLOCTEXT("AnimationGraphNode", "AnimGraphNodeIndicatorTooltip", "Fast path enabled: This node is not using any Blueprint calls to update its data."), NULL, TEXT("Shared/GraphNodes/Animation"), TEXT("GraphNode_FastPathInfo")))
		.Visibility(EVisibility::Visible);

	PoseViewWidget =
		SNew(SButton)
		.ToolTipText(NSLOCTEXT("AnimationGraphNode", "SpawnColourPicker", "Pose watch active. Click to spawn the pose watch colour picker"))
		.OnClicked(this, &SAnimationGraphNode::SpawnColourPicker)
		.ButtonColorAndOpacity(this, &SAnimationGraphNode::GetPoseViewColour)
		[
			SNew(SImage).Image(FEditorStyle::GetBrush("GenericViewButton"))
		];
}

void SAnimationGraphNode::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SGraphNodeK2Base::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	if (UAnimGraphNode_Base* AnimNode = CastChecked<UAnimGraphNode_Base>(GraphNode, ECastCheckedType::NullAllowed))
	{
		// Search for an enabled or disabled breakpoint on this node
		PoseWatch = AnimationEditorUtils::FindPoseWatchForNode(GraphNode);
	}
}

TArray<FOverlayWidgetInfo> SAnimationGraphNode::GetOverlayWidgets(bool bSelected, const FVector2D& WidgetSize) const
{
	TArray<FOverlayWidgetInfo> Widgets;

	if (UAnimGraphNode_Base* AnimNode = CastChecked<UAnimGraphNode_Base>(GraphNode, ECastCheckedType::NullAllowed))
	{
		if (AnimNode->BlueprintUsage == EBlueprintUsage::DoesNotUseBlueprint)
		{
			const FSlateBrush* ImageBrush = FEditorStyle::Get().GetBrush(TEXT("Graph.AnimationFastPathIndicator"));

			FOverlayWidgetInfo Info;
			Info.OverlayOffset = FVector2D(WidgetSize.X - (ImageBrush->ImageSize.X * 0.5f), -(ImageBrush->ImageSize.Y * 0.5f));
			Info.Widget = IndicatorWidget;

			Widgets.Add(Info);
		}

		if (PoseWatch.IsValid())
		{
			const FSlateBrush* ImageBrush = FEditorStyle::GetBrush("GenericViewButton");

			FOverlayWidgetInfo Info;
			Info.OverlayOffset = FVector2D(0 - (ImageBrush->ImageSize.X * 0.5f), -(ImageBrush->ImageSize.Y * 0.5f));
			Info.Widget = PoseViewWidget;
			
			Widgets.Add(Info);
		}
	}

	return Widgets;
}

FSlateColor SAnimationGraphNode::GetPoseViewColour() const
{
	UPoseWatch* CurPoseWatch = PoseWatch.Get();
	if (CurPoseWatch)
	{
		return FSlateColor(CurPoseWatch->PoseWatchColour);
	}
	return FSlateColor(FColor::White); //Need a return value but should never actually get here
}

FReply SAnimationGraphNode::SpawnColourPicker()
{
	FSlateApplication::Get().PushMenu(
		SharedThis(this),
		FWidgetPath(),
		SNew(SPoseViewColourPickerPopup).PoseWatch(PoseWatch),
		FSlateApplication::Get().GetCursorPos(),
		FPopupTransitionEffect(FPopupTransitionEffect::TypeInPopup)
		);

	return FReply::Handled();
}
