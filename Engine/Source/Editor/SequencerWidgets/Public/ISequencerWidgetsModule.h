// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/EnumClassFlags.h"
#include "Misc/Attribute.h"
#include "Layout/Visibility.h"
#include "Widgets/SWidget.h"
#include "ITimeSlider.h"
#include "Widgets/Input/NumericTypeInterface.h"
#include "Modules/ModuleInterface.h"

/** Enum denoting which time ranges to display on a time range */
enum class EShowRange
{
	None			= 0,
	WorkingRange	= 1 << 0,
	ViewRange		= 1 << 1,
	PlaybackRange	= 1 << 2,
};
ENUM_CLASS_FLAGS(EShowRange);

/** Time range construction arguments */
struct FTimeRangeArgs
{
	FTimeRangeArgs(
		EShowRange InShowRanges,
		TSharedRef<ITimeSliderController> InController,
		TAttribute<EVisibility> InVisibilityDelegate,
		TAttribute<bool> InShowFrameNumbersDelegate,
		TSharedRef<INumericTypeInterface<float>> InNumericTypeInterface
		)
		: ShowRanges(InShowRanges)
		, Controller(InController)
		, VisibilityDelegate(InVisibilityDelegate)
		, ShowFrameNumbersDelegate(InShowFrameNumbersDelegate)
		, NumericTypeInterface(InNumericTypeInterface)
	{
	}
	
	/** Which ranges to show */
	EShowRange ShowRanges;
	/** A time slider controller */
	TSharedRef<ITimeSliderController> Controller;
	/** Visibility delegate */
	TAttribute<EVisibility> VisibilityDelegate;
	/** Whether to show frame numbers */
	TAttribute<bool> ShowFrameNumbersDelegate;
	/** Numeric type interface to use for frame<->time conversion */
	TSharedRef<INumericTypeInterface<float>> NumericTypeInterface;
};

/**
 * The public interface of SequencerModule
 */
class ISequencerWidgetsModule
	: public IModuleInterface
{
public:

	virtual TSharedRef<ITimeSlider> CreateTimeSlider( const TSharedRef<ITimeSliderController>& InController, bool bMirrorLabels  ) = 0;
	virtual TSharedRef<ITimeSlider> CreateTimeSlider( const TSharedRef<ITimeSliderController>& InController, const TAttribute<EVisibility>& VisibilityDelegate, bool bMirrorLabels  ) = 0;
	virtual TSharedRef<SWidget> CreateTimeRangeSlider( const TSharedRef<class ITimeSliderController>& InController, const TAttribute<float>& InTimeSnapIntervalDelegate ) = 0;
	virtual TSharedRef<ITimeSlider> CreateTimeRange( const FTimeRangeArgs& InArgs, const TSharedRef<SWidget>& Content ) = 0;
};
