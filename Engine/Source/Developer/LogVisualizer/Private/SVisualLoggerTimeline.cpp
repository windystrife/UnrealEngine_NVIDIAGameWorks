// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SVisualLoggerTimeline.h"
#include "Widgets/SBoxPanel.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SMenuAnchor.h"
#include "LogVisualizerSettings.h"
#include "VisualLoggerDatabase.h"
#include "LogVisualizerStyle.h"
#include "LogVisualizerPrivate.h"
#include "SVisualLoggerTimelinesContainer.h"
#include "SVisualLoggerTimelineBar.h"

class STimelineLabelAnchor : public SMenuAnchor
{
public:
	void Construct(const FArguments& InArgs, TSharedPtr<class SLogVisualizerTimeline> InTimelineOwner)
	{
		SMenuAnchor::Construct(InArgs);
		TimelineOwner = InTimelineOwner;
	}

private:
	/** SWidget Interface */
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	TWeakPtr<class SLogVisualizerTimeline> TimelineOwner;
};

FReply STimelineLabelAnchor::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton && TimelineOwner.IsValid() && TimelineOwner.Pin()->IsSelected())
	{
		SetIsOpen(!IsOpen());
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

FReply SLogVisualizerTimeline::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	Owner->ChangeSelection(SharedThis(this), MouseEvent);

	return FReply::Unhandled();
}

FReply SLogVisualizerTimeline::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	FReply ReturnValue = FReply::Unhandled();
	if (IsSelected() == false)
	{
		return ReturnValue;
	}

	return FLogVisualizer::Get().GetEvents().OnKeyboardEvent.Execute(MyGeometry, InKeyEvent);
}

bool SLogVisualizerTimeline::IsSelected() const
{
	// Ask the tree if we are selected
	return FVisualLoggerDatabase::Get().IsRowSelected(GetName());
}

void SLogVisualizerTimeline::UpdateVisibility()
{
	ULogVisualizerSettings* Settings = ULogVisualizerSettings::StaticClass()->GetDefaultObject<ULogVisualizerSettings>();

	if (FVisualLoggerDatabase::Get().ContainsRowByName(GetName()))
	{
		FVisualLoggerDBRow& DataRow = FVisualLoggerDatabase::Get().GetRowByName(GetName());
		const TArray<FVisualLogDevice::FVisualLogEntryItem>& Entries = DataRow.GetItems();

		const bool bJustIgnore = Settings->bIgnoreTrivialLogs && Entries.Num() <= Settings->TrivialLogsThreshold;
		const bool bVisibleByOwnerClasses = FVisualLoggerFilters::Get().MatchObjectName(OwnerClassName.ToString());
		const bool bIsCollapsed = bJustIgnore || DataRow.GetNumberOfHiddenItems() == Entries.Num() || (SearchFilter.Len() > 0 && OwnerName.ToString().Find(SearchFilter) == INDEX_NONE);

		const bool bIsHidden = bIsCollapsed || !bVisibleByOwnerClasses;
		SetVisibility(bIsHidden ? EVisibility::Collapsed : EVisibility::Visible);
		if (bIsCollapsed)
		{
			Owner->SetSelectionState(SharedThis(this), false, false);
		}

		FVisualLoggerDatabase::Get().SetRowVisibility(GetName(), !bIsHidden);
	}
	else
	{
		Owner->SetSelectionState(SharedThis(this), false, false);
		FVisualLoggerDatabase::Get().SetRowVisibility(GetName(), false);
	}
}

void SLogVisualizerTimeline::OnFiltersSearchChanged(const FText& Filter)
{
	OnFiltersChanged();
}

void SLogVisualizerTimeline::OnFiltersChanged()
{
	UpdateVisibility();
}

void SLogVisualizerTimeline::OnSearchChanged(const FText& Filter)
{
	SearchFilter = Filter.ToString();
	UpdateVisibility();
}

void SLogVisualizerTimeline::HandleLogVisualizerSettingChanged(FName InName)
{
	UpdateVisibility();
}

const TArray<FVisualLogDevice::FVisualLogEntryItem>& SLogVisualizerTimeline::GetEntries()
{
	FVisualLoggerDBRow& DataRow = FVisualLoggerDatabase::Get().GetRowByName(GetName());
	return DataRow.GetItems();
}

void SLogVisualizerTimeline::AddEntry(const FVisualLogDevice::FVisualLogEntryItem& Entry) 
{ 
	//Entries.Add(Entry); 

	ULogVisualizerSettings* Settings = ULogVisualizerSettings::StaticClass()->GetDefaultObject<ULogVisualizerSettings>();
	
	FVisualLoggerDBRow& DataRow = FVisualLoggerDatabase::Get().GetRowByName(GetName());
	const TArray<FVisualLogDevice::FVisualLogEntryItem>& Entries = DataRow.GetItems();

	UpdateVisibility();

}

void SLogVisualizerTimeline::Construct(const FArguments& InArgs, TSharedPtr<FVisualLoggerTimeSliderController> TimeSliderController, TSharedPtr<SVisualLoggerTimelinesContainer> InContainer, FName InName, FName InOwnerClassName)
{
	OnGetMenuContent = InArgs._OnGetMenuContent;

	Owner = InContainer;
	OwnerName = InName;
	OwnerClassName = InOwnerClassName;
	
	ChildSlot
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(FMargin(0, 4, 0, 0))
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			.FillWidth(TAttribute<float>::Create(TAttribute<float>::FGetter::CreateLambda([=] { return FLogVisualizer::Get().GetAnimationOutlinerFillPercentage(); })))
			[
				SAssignNew(PopupAnchor, STimelineLabelAnchor, SharedThis(this))
				.OnGetMenuContent(OnGetMenuContent)
				[
					SNew(SBorder)
					.HAlign(HAlign_Fill)
					.Padding(FMargin(0, 0, 4, 0))
					.BorderImage(FCoreStyle::Get().GetBrush("NoBorder"))
					[
						SNew(SBorder)
						.VAlign(VAlign_Center)
						.BorderImage(this, &SLogVisualizerTimeline::GetBorder)
						.Padding(FMargin(4, 0, 2, 0))
						[
							// Search box for searching through the outliner
							SNew(SVerticalBox)
							+ SVerticalBox::Slot()
							.Padding(FMargin(0, 0, 0, 0))
							.HAlign(HAlign_Fill)
							.VAlign(VAlign_Fill)
							[
								SNew(STextBlock)
								.Text(FText::FromName(OwnerName))
								.ShadowOffset(FVector2D(1.f, 1.f))
							]
							+ SVerticalBox::Slot()
							.Padding(FMargin(0, 0, 0, 0))
							.HAlign(HAlign_Fill)
							.VAlign(VAlign_Fill)
							[
								SNew(STextBlock)
								.Text(FText::FromName(OwnerClassName))
								.TextStyle(FLogVisualizerStyle::Get(), TEXT("Sequencer.ClassNAme"))
							]
						]
					]
				]
			]
			+ SHorizontalBox::Slot()
			.Padding(FMargin(0, 4, 0, 0))
			.HAlign(HAlign_Left)
			[
				SNew(SBox)
				.Padding(FMargin(0, 0, 0, 0))
				.HAlign(HAlign_Left)
				[
					// Search box for searching through the outliner
					SAssignNew(TimelineBar, SVisualLoggerTimelineBar, TimeSliderController, SharedThis(this))
				]
			]
		];

	ULogVisualizerSettings::StaticClass()->GetDefaultObject<ULogVisualizerSettings>()->OnSettingChanged().AddRaw(this, &SLogVisualizerTimeline::HandleLogVisualizerSettingChanged);
	FVisualLoggerDatabase::Get().GetEvents().OnNewItem.AddRaw(this, &SLogVisualizerTimeline::OnNewItemHandler);
	FVisualLoggerDatabase::Get().GetEvents().OnRowSelectionChanged.AddRaw(this, &SLogVisualizerTimeline::OnRowSelectionChanged);
}

SLogVisualizerTimeline::~SLogVisualizerTimeline()
{
	ULogVisualizerSettings::StaticClass()->GetDefaultObject<ULogVisualizerSettings>()->OnSettingChanged().RemoveAll(this);
	FVisualLoggerDatabase::Get().GetEvents().OnNewItem.RemoveAll(this);
	FVisualLoggerDatabase::Get().GetEvents().OnRowSelectionChanged.RemoveAll(this);
}

void SLogVisualizerTimeline::OnRowSelectionChanged(const TArray<FName>& RowNames)
{
	if (RowNames.Num() > 0)
	{
		const FName RowName = RowNames[RowNames.Num() - 1];
	}
}

void SLogVisualizerTimeline::OnNewItemHandler(const FVisualLoggerDBRow& BDRow, int32 ItemIndex)
{
	const FVisualLogDevice::FVisualLogEntryItem& Entry = BDRow.GetItems()[ItemIndex];
	if (GetName() == Entry.OwnerName)
	{
		AddEntry(Entry);
	}
}

const FSlateBrush* SLogVisualizerTimeline::GetBorder() const
{
	if (IsSelected())
	{
		return FLogVisualizerStyle::Get().GetBrush("ToolBar.Button.Hovered");
	}
	else
	{
		return FLogVisualizerStyle::Get().GetBrush("ToolBar.Button.Normal");
	}
}

void SLogVisualizerTimeline::Goto(float ScrubPosition) 
{ 

}

void SLogVisualizerTimeline::GotoNextItem() 
{ 
	FLogVisualizer::Get().GotoNextItem(GetName());
}

void SLogVisualizerTimeline::GotoPreviousItem() 
{ 
	FLogVisualizer::Get().GotoPreviousItem(GetName());
}

void SLogVisualizerTimeline::MoveCursorByDistance(int32 Distance)
{
	if (Distance > 0)
	{
		FLogVisualizer::Get().GotoNextItem(GetName(), FMath::Abs(Distance));
	}
	else
	{
		FLogVisualizer::Get().GotoPreviousItem(GetName(), FMath::Abs(Distance));
	}
	
}
