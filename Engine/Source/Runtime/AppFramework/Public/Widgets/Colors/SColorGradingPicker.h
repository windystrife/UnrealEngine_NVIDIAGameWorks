// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Input/Reply.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SLeafWidget.h"
#include "Framework/SlateDelegates.h"
#include "SCompoundWidget.h"
#include "SNumericEntryBox.h"

/** Callback to get the current FVector4 value */
DECLARE_DELEGATE_RetVal_OneParam(bool, FOnGetCurrentVector4Value, FVector4&)

/**
* Enumerates color picker modes.
*/
enum class EColorGradingModes
{
	Saturation,
	Contrast,
	Gamma,
	Gain,
	Offset,
	Invalid
};


/**
 * Class for placing a color picker. If all you need is a standalone color picker,
 * use the functions OpenColorGradingWheel and DestroyColorGradingWheel, since they hold a static
 * instance of the color picker.
 */
class APPFRAMEWORK_API SColorGradingPicker
	: public SCompoundWidget
{
public:
	/** Notification when the max/min spinner values are changed (only apply if SupportDynamicSliderMaxValue or SupportDynamicSliderMinValue are true) */
	DECLARE_MULTICAST_DELEGATE_FourParams(FOnNumericEntryBoxDynamicSliderMinMaxValueChanged, float, TWeakPtr<SWidget>, bool, bool);

	// Delegate called when the widget Color Data changed
	DECLARE_DELEGATE_TwoParams(FOnColorGradingPickerValueChanged, FVector4&, bool);


	SLATE_BEGIN_ARGS(SColorGradingPicker)
		: _AllowSpin(true)
		, _SupportDynamicSliderMaxValue(false)
		, _SupportDynamicSliderMinValue(false)
		, _MainDelta(0.01f)
		, _MainShiftMouseMovePixelPerDelta(10)
		, _ColorGradingModes(EColorGradingModes::Saturation)
		, _OnColorCommitted()
		, _OnQueryCurrentColor()
	{ }
		
		SLATE_ARGUMENT(TOptional<float>, ValueMin )
		SLATE_ARGUMENT(TOptional<float>, ValueMax )
		SLATE_ARGUMENT(TOptional<float>, SliderValueMin)
		SLATE_ARGUMENT(TOptional<float>, SliderValueMax)
		SLATE_ATTRIBUTE(bool, AllowSpin)

		/** Tell us if we want to support dynamically changing of the max value using ctrl */
		SLATE_ATTRIBUTE(bool, SupportDynamicSliderMaxValue)
		/** Tell us if we want to support dynamically changing of the min value using ctrl */
		SLATE_ATTRIBUTE(bool, SupportDynamicSliderMinValue)

		SLATE_ARGUMENT( float, MainDelta )

		SLATE_ARGUMENT( int32, MainShiftMouseMovePixelPerDelta)

		SLATE_ARGUMENT( EColorGradingModes, ColorGradingModes)

		/** The event called when the color is committed */
		SLATE_EVENT(FOnColorGradingPickerValueChanged, OnColorCommitted )

		/** Callback to get the current FVector4 value */
		SLATE_EVENT(FOnGetCurrentVector4Value, OnQueryCurrentColor)

		/** Called right before the slider begins to move */
		SLATE_EVENT(FSimpleDelegate, OnBeginSliderMovement)

		/** Called right after the slider handle is released by the user */
		SLATE_EVENT(FSimpleDelegate, OnEndSliderMovement)

	SLATE_END_ARGS()

	/**	Destructor. */
	~SColorGradingPicker();

public:

	/**
	 * Construct the widget
	 *
	 * @param InArgs Declaration from which to construct the widget.
	 */
	void Construct(const FArguments& InArgs );

	FOnNumericEntryBoxDynamicSliderMinMaxValueChanged& GetOnNumericEntryBoxDynamicSliderMaxValueChangedDelegate() { return OnNumericEntryBoxDynamicSliderMaxValueChanged; }
	FOnNumericEntryBoxDynamicSliderMinMaxValueChanged& GetOnNumericEntryBoxDynamicSliderMinValueChangedDelegate() { return OnNumericEntryBoxDynamicSliderMinValueChanged; }

	/** Callback when the max/min spinner value are changed (only apply if SupportDynamicSliderMaxValue or SupportDynamicSliderMinValue are true) */
	void OnDynamicSliderMaxValueChanged(float NewMaxSliderValue, TWeakPtr<SWidget> InValueChangedSourceWidget, bool IsOriginator, bool UpdateOnlyIfHigher);
	void OnDynamicSliderMinValueChanged(float NewMinSliderValue, TWeakPtr<SWidget> InValueChangedSourceWidget, bool IsOriginator, bool UpdateOnlyIfLower);

protected:

	void TransformLinearColorRangeToColorGradingRange(FVector4 &VectorValue) const;
	void TransformColorGradingRangeToLinearColorRange(FVector4 &VectorValue) const;
	void TransformColorGradingRangeToLinearColorRange(float &FloatValue);

	TOptional<float> OnGetMainValue() const;
	void OnMainValueChanged(float InValue, bool ShouldCommitValueChanges);
	void OnMainValueCommitted(float InValue, ETextCommit::Type CommitType);

	FLinearColor GetCurrentLinearColor();

	bool IsEntryBoxEnabled() const;

	// Callback for value changes in the color spectrum picker.
	void HandleCurrentColorValueChanged(const FLinearColor& NewValue, bool ShouldCommitValueChanges);
	void HandleColorWheelMouseCaptureEnd();

	void OnBeginSliderMovement();
	void OnEndSliderMovement(float NewValue);
	void AdjustRatioValue(FVector4 &NewValue);

	bool bIsMouseDragging;
	FVector4 StartDragRatio;

	float SliderValueMin;
	float SliderValueMax;
	float MainDelta;
	int32 MainShiftMouseMovePixelPerDelta;
	EColorGradingModes ColorGradingModes;

	TSharedPtr<SNumericEntryBox<float>> NumericEntryBoxWidget;

	/** Invoked when a new value is selected on the color wheel */
	FOnColorGradingPickerValueChanged OnColorCommitted;

	FOnGetCurrentVector4Value OnQueryCurrentColor;
	FOnNumericEntryBoxDynamicSliderMinMaxValueChanged OnNumericEntryBoxDynamicSliderMaxValueChanged;
	FOnNumericEntryBoxDynamicSliderMinMaxValueChanged OnNumericEntryBoxDynamicSliderMinValueChanged;

	FSimpleDelegate ExternalBeginSliderMovementDelegate;
	FSimpleDelegate ExternalEndSliderMovementDelegate;
};
