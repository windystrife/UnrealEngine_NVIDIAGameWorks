// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "ProfilerManager.h"
#include "Widgets/SCompoundWidget.h"

class SDataGraph;
class SProfilerMiniView;
class SProfilerThreadView;
class SScrollBar;

/** A custom widget that acts as a container for widgets like SDataGraph or SEventTree. */
class SProfilerGraphPanel
	: public SCompoundWidget
{
public:

	/** Default constructor. */
	SProfilerGraphPanel();
		
	/** Virtual destructor. */
	virtual ~SProfilerGraphPanel();

	SLATE_BEGIN_ARGS(SProfilerGraphPanel){}
	SLATE_END_ARGS()

	/**
	 * Construct this widget
	 *
	 * @param InArgs The declaration data for this widget
	 */
	void Construct(const FArguments& InArgs);

	TSharedPtr<SDataGraph>& GetMainDataGraph()
	{
		return DataGraph;
	}

public:

	//~ SWidget interface

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

protected:

	/** Called when the status of specified tracked stat has changed. */
	void ProfilerManager_OnTrackedStatChanged(const TSharedPtr<FTrackedStat>& TrackedStat, bool bIsTracked);

	/**
	 * Called when the user scrolls the scrollbar
	 *
	 * @param ScrollOffset Scroll offset as a fraction between 0 and 1.
	 */
	void HorizontalScrollBar_OnUserScrolled(float ScrollOffset);

	void VerticalScrollBar_OnUserScrolled(float ScrollOffset);

	/** Called when the frame offset has been changed in the data graph widget. */
	void OnDataGraphGraphOffsetChanged(int32 InFrameOffset);

	void ProfilerManager_OnViewModeChanged(EProfilerViewMode NewViewMode);

public:

	/** Called when the selection box has been changed in the profiler mini-view widget. */
	void MiniView_OnSelectionBoxChanged(int32 FrameStart, int32 FrameEnd);

	void ThreadView_OnViewPositionXChanged(double FrameStartMS, double FrameEndMS, double MaxEndTimeMS, int32 FrameStart, int32 FrameEnd);
	void ThreadView_OnViewPositionYChanged(double PosYStart, double PosYEnd, double MaxPosY);

protected:

	/** Sets state of the scroll bar. */
	void SetScrollBarState();

	void UpdateInternals();

//protected:
public:

	/** Holds the data graph widget. */
	TSharedPtr<SDataGraph> DataGraph;

	/** Holds the thread view widget. */
	TSharedPtr<SProfilerThreadView> ThreadView;

	/** Weak pointer to the profiler mini-view. */
	TWeakPtr<SProfilerMiniView> ProfilerMiniView;
	
	/** Temporary solution to avoid feedback loop when changing the selection box. */
	bool bLockMiniViewState;

	/** Horizontal scroll bar, used for scrolling graphs. */
	TSharedPtr<SScrollBar> HorizontalScrollBar;

	/** Vertical scroll bar, used for scrolling graphs. */
	TSharedPtr<SScrollBar> VerticalScrollBar;

	/** Number of graph points. */
	int32 NumDataPoints;

	/** Number of graph points that can be displayed at once in this widget. */
	int32 NumVisiblePoints;

	/** Current offset of the graph, index of the first visible graph point. */
	int32 GraphOffset;

	/** Current view mode. */
	EProfilerViewMode ViewMode;
};
