// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/SSynthKnob.h"
#include "UI/SSynthTooltip.h"
#include "UI/SynthKnobStyle.h"
#include "UI/SynthTypes.h"
#include "UObject/ObjectMacros.h"
#include "Styling/SlateTypes.h"
#include "Widgets/SWidget.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Components/ContentWidget.h"
#include "SynthKnob.generated.h"


DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnMouseCaptureBeginEvent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnMouseCaptureEndEvent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnControllerCaptureBeginEvent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnControllerCaptureEndEvent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFloatValueChangedEvent, float, Value);


/**
 * A simple widget that shows a sliding bar with a handle that allows you to control the value between 0..1.
 *
 * * No Children
 */
UCLASS()
class SYNTHESIS_API USynthKnob : public UWidget
{
	GENERATED_UCLASS_BODY()

public:
	/** The volume value to display. */
	UPROPERTY(EditAnywhere, Category=Appearance, meta=( ClampMin="0", ClampMax="1", UIMin="0", UIMax="1"))
	float Value;

	/** The amount to adjust the value by, when using a controller or keyboard */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Appearance, meta = (ClampMin = "0", ClampMax = "1", UIMin = "0", UIMax = "1"))
	float StepSize;

	/** The speed of the mouse knob control */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Appearance, meta = (ClampMin = "0", ClampMax = "1", UIMin = "0", UIMax = "1"))
	float MouseSpeed;
	
	/** The speed of the mouse knob control when fine-tuning the knob */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Appearance, meta = (ClampMin = "0", ClampMax = "1", UIMin = "0", UIMax = "1"))
	float MouseFineTuneSpeed;

	/** Enable tool tip window to show parameter information while knob turns */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = SynthTooltip)
	uint32 ShowTooltipInfo:1;

	/** The name of the pararameter. Will show when knob turns. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = SynthTooltip)
	FText ParameterName;

	/** The parameter units (e.g. hz). Will append to synth tooltip info. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = SynthTooltip)
	FText ParameterUnits;

	/** A bindable delegate to allow logic to drive the value of the widget */
	UPROPERTY()
	FGetFloat ValueDelegate;

public:
	
	/** The synth knob style */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Style", meta=( DisplayName="Style" ))
	FSynthKnobStyle WidgetStyle;

	/** Whether the handle is interactive or fixed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Appearance, AdvancedDisplay)
	bool Locked;

	/** Should the slider be focusable? */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Interaction")
	bool IsFocusable;

public:

	/** Invoked when the mouse is pressed and a capture begins. */
	UPROPERTY(BlueprintAssignable, Category="Widget Event")
	FOnMouseCaptureBeginEvent OnMouseCaptureBegin;

	/** Invoked when the mouse is released and a capture ends. */
	UPROPERTY(BlueprintAssignable, Category="Widget Event")
	FOnMouseCaptureEndEvent OnMouseCaptureEnd;

	/** Invoked when the controller capture begins. */
	UPROPERTY(BlueprintAssignable, Category = "Widget Event")
	FOnControllerCaptureBeginEvent OnControllerCaptureBegin;

	/** Invoked when the controller capture ends. */
	UPROPERTY(BlueprintAssignable, Category = "Widget Event")
	FOnControllerCaptureEndEvent OnControllerCaptureEnd;

	/** Called when the value is changed by slider or typing. */
	UPROPERTY(BlueprintAssignable, Category="Widget Event")
	FOnFloatValueChangedEvent OnValueChanged;

	/** Gets the current value of the slider. */
	UFUNCTION(BlueprintCallable, Category="Behavior")
	float GetValue() const;

	/** Sets the current value of the slider. */
	UFUNCTION(BlueprintCallable, Category="Behavior")
	void SetValue(float InValue);

	/** Sets the handle to be interactive or fixed */
	UFUNCTION(BlueprintCallable, Category="Behavior")
	void SetLocked(bool InValue);

	/** Sets the amount to adjust the value by, when using a controller or keyboard */
	UFUNCTION(BlueprintCallable, Category="Behavior")
	void SetStepSize(float InValue);

	// UWidget interface
	virtual void SynchronizeProperties() override;
	// End of UWidget interface

	// UVisual interface
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	// End of UVisual interface

#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override;
#endif

protected:
	/** Native Slate Widget */
 	TSharedPtr<SSynthKnob> MySynthKnob;

	// UWidget interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	// End of UWidget interface

	void HandleOnValueChanged(float InValue);
	void HandleOnMouseCaptureBegin();
	void HandleOnMouseCaptureEnd();
	void HandleOnControllerCaptureBegin();
	void HandleOnControllerCaptureEnd();

protected:
	PROPERTY_BINDING_IMPLEMENTATION(float, Value);
};
