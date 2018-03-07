// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Input/NumericTypeInterface.h"
#include "ITimeSlider.h"

class STimeRange : public ITimeSlider
{
public:
	SLATE_BEGIN_ARGS(STimeRange)
		: _ShowWorkingRange(true), _ShowViewRange(false), _ShowPlaybackRange(false)
	{}
		/* If we should show frame numbers on the timeline */
		SLATE_ARGUMENT( TAttribute<bool>, ShowFrameNumbers )
		/** Whether to show the working range */
		SLATE_ARGUMENT( bool, ShowWorkingRange )
		/** Whether to show the view range */
		SLATE_ARGUMENT( bool, ShowViewRange )
		/** Whether to show the playback range */
		SLATE_ARGUMENT( bool, ShowPlaybackRange )
		/* Content to display inside the time range */
		SLATE_DEFAULT_SLOT( FArguments, CenterContent )
	SLATE_END_ARGS()

	/**
	 * Construct the widget
	 * 
	 * @param InArgs   A declaration from which to construct the widget
	 */
	void Construct( const FArguments& InArgs, TSharedRef<ITimeSliderController> InTimeSliderController, TSharedRef<INumericTypeInterface<float>> NumericTypeInterface );
	
protected:

	float PlayStartTime() const;
	float PlayEndTime() const;

	TOptional<float> MinPlayStartTime() const;
	TOptional<float> MaxPlayStartTime() const;
	TOptional<float> MinPlayEndTime() const;
	TOptional<float> MaxPlayEndTime() const;

	void OnPlayStartTimeCommitted(float NewValue, ETextCommit::Type InTextCommit);
	void OnPlayEndTimeCommitted(float NewValue, ETextCommit::Type InTextCommit);

	void OnPlayStartTimeChanged(float NewValue);
	void OnPlayEndTimeChanged(float NewValue);

	FText PlayStartTimeTooltip() const;
	FText PlayEndTimeTooltip() const;

protected:

	float ViewStartTime() const;
	float ViewEndTime() const;

	TOptional<float> MaxViewStartTime() const;
	TOptional<float> MinViewEndTime() const;

	void OnViewStartTimeCommitted(float NewValue, ETextCommit::Type InTextCommit);
	void OnViewEndTimeCommitted(float NewValue, ETextCommit::Type InTextCommit);

	void OnViewStartTimeChanged(float NewValue);
	void OnViewEndTimeChanged(float NewValue);

	FText ViewStartTimeTooltip() const;
	FText ViewEndTimeTooltip() const;

protected:

	float WorkingStartTime() const;
	float WorkingEndTime() const;

	TOptional<float> MaxWorkingStartTime() const;
	TOptional<float> MinWorkingEndTime() const;

	void OnWorkingStartTimeCommitted(float NewValue, ETextCommit::Type InTextCommit);
	void OnWorkingEndTimeCommitted(float NewValue, ETextCommit::Type InTextCommit);

	void OnWorkingStartTimeChanged(float NewValue);
	void OnWorkingEndTimeChanged(float NewValue);

private:
	TSharedPtr<ITimeSliderController> TimeSliderController;
	TAttribute<bool> ShowFrameNumbers;
};
