// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "ProfilerManager.h"

class FFPSAnalyzer;
class SHistogram;
class SProfilerFPSStatisticsPanel;

/**
 * A custom widget that acts as a container for widgets like SDataGraph or SEventTree.
 */
class SProfilerFPSChartPanel
	: public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS( SProfilerFPSChartPanel )
		{}

		SLATE_ARGUMENT( TSharedPtr<FFPSAnalyzer>, FPSAnalyzer )
	SLATE_END_ARGS()

	/** Virtual destructor. */
	virtual ~SProfilerFPSChartPanel();

	/**
	 * Construct this widget
	 *
	 * @param InArgs The declaration data for this widget
	 */
	void Construct( const FArguments& InArgs );

protected:

	/** Called when the status of specified tracked stat has changed. */
	void ProfilerManager_OnViewModeChanged(EProfilerViewMode NewViewMode);

protected:

	/** The descriptions panel of the chart */
	TSharedPtr<SHistogram> Histogram;

	/** The descriptions panel of the chart */
	TSharedPtr<SProfilerFPSStatisticsPanel> StatisticsPanel;
};
