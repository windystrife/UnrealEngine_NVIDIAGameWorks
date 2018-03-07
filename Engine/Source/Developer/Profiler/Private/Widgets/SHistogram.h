// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "ProfilerSample.h"

class FFPSAnalyzer;
class FPaintArgs;
class FSlateWindowElementList;

/** Type definition for shared pointers to instances of FGraphDataSource. */
typedef TSharedPtr<class IHistogramDataSource> FHistogramDataSourcePtr;

/** Type definition for shared references to instances of FGraphDataSource. */
typedef TSharedRef<class IHistogramDataSource> FHistogramDataSourceRef;


class FHistogramDescription
{
public:
	FHistogramDescription()
	{}

	/**
	 * Create and initialize a new instance.
	 *
	 * @param InDataSource Data source for the histogram.
	 * @param InBinInterval Interval for each bin.
	 * @param InMinValue Minimum value to use for the bins.
	 * @param InMaxValue Maximum value to use for the bins.
	 * @param InBinNormalize Whether or not to normalize the bins to the total data count.
	 *
	 */
	FHistogramDescription(const FHistogramDataSourceRef& InDataSource, float InBinInterval, float InMinValue, float InMaxValue, bool InBinNormalize = false)
		: HistogramDataSource(InDataSource)
		, Interval(InBinInterval)
		, MinValue(InMinValue)
		, MaxValue(InMaxValue)
		, Normalize(InBinNormalize)
	{
		BinCount = FPlatformMath::CeilToInt( (MaxValue - MinValue) / Interval )+1;	// +1 for data beyond the max
	}

	/** Retrieves the bin count */
	int32 GetBinCount() const
	{
		return BinCount;
	}

	/** Retrieves the count for the specified bin */
	int32 GetCount(int32 Bin) const
	{
		float MinVal = MinValue + Bin * Interval;
		float MaxVal = MinValue + (Bin+1) * Interval;
		return HistogramDataSource->GetCount(MinVal, MaxVal);
	}

	/** Retrieves the total count */
	int32 GetTotalCount() const
	{
		return HistogramDataSource->GetTotalCount();
	}

	/** Data source for the histogram */
	FHistogramDataSourcePtr HistogramDataSource;

	/** Bin interval */
	float Interval;

	/** Min value of the graph */
	float MinValue;

	/** Max value of the graph */
	float MaxValue;

	/** Normalize the bins */
	bool Normalize;

	int32 BinCount;
};

/*-----------------------------------------------------------------------------
	Declarations
-----------------------------------------------------------------------------*/

/** A custom widget used to display a histogram. */
class SHistogram : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SHistogram )
		: _Description()
		{}

		/** Analyzer reference */
		SLATE_ATTRIBUTE( FHistogramDescription, Description )

	SLATE_END_ARGS()

	/**
	 * Construct this widget
	 *
	 * @param	InArgs	The declaration data for this widget
	 */
	void Construct( const FArguments& InArgs );

	virtual int32 OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const override;

	virtual FVector2D ComputeDesiredSize(float) const override
	{
		//return SCompoundWidget::ComputeDesiredSize();
		return FVector2D(16.0f,16.0f);
	}

	/** Sets a new Analyzer for the Description panel */
	void SetFPSAnalyzer(const TSharedPtr<FFPSAnalyzer>& InAnalyzer);

protected:
	FHistogramDescription Description;
};
