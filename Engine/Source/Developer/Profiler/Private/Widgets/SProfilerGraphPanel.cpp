// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Widgets/SProfilerGraphPanel.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBar.h"
#include "EditorStyleSet.h"
#include "Widgets/SDataGraph.h"
#include "Widgets/SProfilerMiniView.h"
#include "Widgets/SProfilerThreadView.h"

#define LOCTEXT_NAMESPACE "SProfilerGraphPanel"


SProfilerGraphPanel::SProfilerGraphPanel()
	: bLockMiniViewState(false)
	, NumDataPoints(0)
	, NumVisiblePoints(0)
	, GraphOffset(0)
	, ViewMode(EProfilerViewMode::InvalidOrMax)
{ }


SProfilerGraphPanel::~SProfilerGraphPanel()
{
	// Remove ourselves from the profiler manager.
	auto ProfilerManager = FProfilerManager::Get();
	if(ProfilerManager.IsValid())
	{
		ProfilerManager->OnTrackedStatChanged().RemoveAll(this);
		ProfilerManager->OnViewModeChanged().RemoveAll(this);

		DataGraph->OnSelectionChangedForIndex().RemoveAll(ProfilerManager.Get());

		if(ProfilerMiniView.Pin().IsValid())
		{
			ProfilerMiniView.Pin()->OnSelectionBoxChanged().RemoveAll(this);
		}
	}

	ThreadView->OnViewPositionXChanged().RemoveAll(this);
	ThreadView->OnViewPositionYChanged().RemoveAll(this);
}


void SProfilerGraphPanel::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
		.Padding(2.0f)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(SVerticalBox)

				// At this moment only one widget of these two can be visible at once.
				//
				// DataGraph
				+SVerticalBox::Slot()
				.FillHeight(1.0f)
				[
					SAssignNew(DataGraph, SDataGraph)
					.OnGraphOffsetChanged(this, &SProfilerGraphPanel::OnDataGraphGraphOffsetChanged)
				]

				// ThreadView
				+ SVerticalBox::Slot()
				.FillHeight(1.0f)
				[
					SAssignNew(ThreadView, SProfilerThreadView)
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SAssignNew(HorizontalScrollBar, SScrollBar)
					.Orientation(Orient_Horizontal)
					.AlwaysShowScrollbar(true)
					.Visibility(EVisibility::Visible)
					.Thickness(FVector2D(8.0f, 8.0f))
					.OnUserScrolled(this, &SProfilerGraphPanel::HorizontalScrollBar_OnUserScrolled)
				]
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SAssignNew(VerticalScrollBar, SScrollBar)
				.Orientation(Orient_Vertical)
				.AlwaysShowScrollbar(true)
				.Visibility(EVisibility::Visible)
				.Thickness(FVector2D(8.0f, 8.0f))
				.OnUserScrolled(this, &SProfilerGraphPanel::VerticalScrollBar_OnUserScrolled)
			]
		]
	];

	HorizontalScrollBar->SetState(0.0f, 1.0f);
	VerticalScrollBar->SetState(0.0f, 1.0f);

	// Register ourselves with the profiler manager.
	FProfilerManager::Get()->OnTrackedStatChanged().AddSP(this, &SProfilerGraphPanel::ProfilerManager_OnTrackedStatChanged);
	FProfilerManager::Get()->OnViewModeChanged().AddSP(this, &SProfilerGraphPanel::ProfilerManager_OnViewModeChanged);

	DataGraph->OnSelectionChangedForIndex().AddSP(FProfilerManager::Get().ToSharedRef(), &FProfilerManager::DataGraph_OnSelectionChangedForIndex);

	ThreadView->OnViewPositionXChanged().AddSP(this, &SProfilerGraphPanel::ThreadView_OnViewPositionXChanged);
	ThreadView->OnViewPositionYChanged().AddSP(this, &SProfilerGraphPanel::ThreadView_OnViewPositionYChanged);
}


void SProfilerGraphPanel::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	UpdateInternals();
}


void SProfilerGraphPanel::HorizontalScrollBar_OnUserScrolled(float ScrollOffset)
{
	if(ViewMode == EProfilerViewMode::LineIndexBased)
	{
		const float ThumbSizeFraction = FMath::Min((float)NumVisiblePoints / (float)NumDataPoints, 1.0f);
		ScrollOffset = FMath::Min(ScrollOffset, 1.0f - ThumbSizeFraction);

		HorizontalScrollBar->SetState(ScrollOffset, ThumbSizeFraction);

		GraphOffset = FMath::TruncToInt(ScrollOffset * NumDataPoints);
		DataGraph->ScrollTo(GraphOffset);

		ProfilerMiniView.Pin()->MoveWithoutZoomSelectionBox(GraphOffset, GraphOffset + NumVisiblePoints);
	}
	else if(ViewMode == EProfilerViewMode::ThreadViewTimeBased)
	{
		ThreadView->SetPositionXToByScrollBar(ScrollOffset);
	}
}


void SProfilerGraphPanel::VerticalScrollBar_OnUserScrolled(float ScrollOffset)
{
	if(ViewMode == EProfilerViewMode::LineIndexBased)
	{

	}
	else if(ViewMode == EProfilerViewMode::ThreadViewTimeBased)
	{
		ThreadView->SetPositonYTo(ScrollOffset);
	}
}


void SProfilerGraphPanel::ProfilerManager_OnTrackedStatChanged(const TSharedPtr<FTrackedStat>& TrackedStat, bool bIsTracked)
{
	if(bIsTracked)
	{
		DataGraph->AddInnerGraph(TrackedStat);
	}
	else
	{
		DataGraph->RemoveInnerGraph(TrackedStat->StatID);
	}
}


void SProfilerGraphPanel::OnDataGraphGraphOffsetChanged(int32 InGraphOffset)
{
	GraphOffset = InGraphOffset;
	SetScrollBarState();
	ProfilerMiniView.Pin()->MoveWithoutZoomSelectionBox(InGraphOffset, InGraphOffset + NumVisiblePoints);
}


void SProfilerGraphPanel::MiniView_OnSelectionBoxChanged(int32 FrameStart, int32 FrameEnd)
{
	if(ViewMode == EProfilerViewMode::LineIndexBased)
	{
		GraphOffset = FrameStart;
		SetScrollBarState();
		DataGraph->ScrollTo(GraphOffset);
	}
	else if(ViewMode == EProfilerViewMode::ThreadViewTimeBased)
	{
		TGuardValue<bool> LockedMiniViewState(bLockMiniViewState, true);
		// Update thread-view state.
		ThreadView->SetFrameRange(FrameStart, FrameEnd);
	}
}


void SProfilerGraphPanel::ThreadView_OnViewPositionXChanged(double FrameStartMS, double FrameEndMS, double MaxEndTimeMS, int32 FrameStart, int32 FrameEnd)
{
	const double MinStartTimeMS = 0.0f;
	const double TotalRangeMS = MaxEndTimeMS - MinStartTimeMS;
	const double ThisFrameRangeMS = FrameEndMS - FrameStartMS;

	const double ThumbSizeFraction = ThisFrameRangeMS / TotalRangeMS;
	const double OffsetFraction = FrameStartMS / MaxEndTimeMS;

	// Update horizontal scroll bar state.
	HorizontalScrollBar->SetState((float)OffsetFraction, (float)ThumbSizeFraction);

	if(!bLockMiniViewState)
	{
		// Update profiler mini-view state.
		ProfilerMiniView.Pin()->MoveAndZoomSelectionBox(FrameStart, FrameEnd);
	}
}


void SProfilerGraphPanel::ThreadView_OnViewPositionYChanged(double PosStartY, double PosYEnd, double MaxPosY)
{
	const double MinPosY = 0.0f;
	const double TotalRangeY = MaxPosY - MinPosY;
	const double ThisRangeY = PosYEnd - PosStartY;

	const double ThumbSizeFraction = ThisRangeY / TotalRangeY;
	const double OffsetFraction = PosStartY / MaxPosY;

	VerticalScrollBar->SetState((float)OffsetFraction, (float)ThumbSizeFraction);
}


void SProfilerGraphPanel::ProfilerManager_OnViewModeChanged(EProfilerViewMode NewViewMode)
{
	if(NewViewMode == EProfilerViewMode::LineIndexBased)
	{
		VerticalScrollBar->SetVisibility(EVisibility::Collapsed);
		VerticalScrollBar->SetEnabled(false);

		DataGraph->SetVisibility(EVisibility::Visible);
		DataGraph->SetEnabled(true);
		ThreadView->SetVisibility(EVisibility::Collapsed);
		ThreadView->SetEnabled(false);
	}
	else if(NewViewMode == EProfilerViewMode::ThreadViewTimeBased)
	{
		VerticalScrollBar->SetVisibility(EVisibility::Visible);
		VerticalScrollBar->SetEnabled(true);

		DataGraph->SetVisibility(EVisibility::Collapsed);
		DataGraph->SetEnabled(false);
		ThreadView->SetVisibility(EVisibility::Visible);
		ThreadView->SetEnabled(true);
	}

	ViewMode = NewViewMode;
}


void SProfilerGraphPanel::UpdateInternals()
{
	if(ViewMode == EProfilerViewMode::LineIndexBased)
	{
		NumVisiblePoints = DataGraph->GetNumVisiblePoints();
		NumDataPoints = DataGraph->GetNumDataPoints();

		SetScrollBarState();
		ProfilerMiniView.Pin()->MoveWithoutZoomSelectionBox(GraphOffset, GraphOffset + NumVisiblePoints);

		if(FProfilerManager::Get()->IsLivePreview())
		{
			// Scroll to the end.
			HorizontalScrollBar_OnUserScrolled(1.0f);
		}
	}
	else if(ViewMode == EProfilerViewMode::ThreadViewTimeBased)
	{
	}
}


void SProfilerGraphPanel::SetScrollBarState()
{
	// Note that the maximum offset is 1.0-ThumbSizeFraction.
	// If the user can view 1/3 of the items in a single page, the maximum offset will be ~0.667f
	const float ThumbSizeFraction = FMath::Min((float)NumVisiblePoints / (float)NumDataPoints, 1.0f);
	const float OffsetFraction = (float)GraphOffset / (float)NumDataPoints;
	HorizontalScrollBar->SetState(OffsetFraction, ThumbSizeFraction);
}


#undef LOCTEXT_NAMESPACE
