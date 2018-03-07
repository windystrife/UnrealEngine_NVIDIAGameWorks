// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Layout/Margin.h"
#include "Styling/SlateColor.h"
#include "Styling/SlateTypes.h"
#include "Widgets/SWidget.h"
#include "Components/ContentWidget.h"

#include "CheckBox.generated.h"

class SCheckBox;
class USlateBrushAsset;
class USlateWidgetStyleAsset;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam( FOnCheckBoxComponentStateChanged, bool, bIsChecked );

/**
 * The checkbox widget allows you to display a toggled state of 'unchecked', 'checked' and 
 * 'indeterminable.  You can use the checkbox for a classic checkbox, or as a toggle button,
 * or as radio buttons.
 * 
 * * Single Child
 * * Toggle
 */
UCLASS()
class UMG_API UCheckBox : public UContentWidget
{
	GENERATED_UCLASS_BODY()

public:
	/** Whether the check box is currently in a checked state */
	UPROPERTY(EditAnywhere, Category=Appearance)
	ECheckBoxState CheckedState;

	/** A bindable delegate for the IsChecked. */
	UPROPERTY()
	FGetCheckBoxState CheckedStateDelegate;

public:
	/** The checkbox bar style */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Style", meta=( DisplayName="Style" ))
	FCheckBoxStyle WidgetStyle;

	/** Style of the check box */
	UPROPERTY()
	USlateWidgetStyleAsset* Style_DEPRECATED;

	/** Image to use when the checkbox is unchecked */
	UPROPERTY()
	USlateBrushAsset* UncheckedImage_DEPRECATED;
	
	/** Image to use when the checkbox is unchecked and hovered */
	UPROPERTY()
	USlateBrushAsset* UncheckedHoveredImage_DEPRECATED;
	
	/** Image to use when the checkbox is unchecked and pressed */
	UPROPERTY()
	USlateBrushAsset* UncheckedPressedImage_DEPRECATED;
	
	/** Image to use when the checkbox is checked */
	UPROPERTY()
	USlateBrushAsset* CheckedImage_DEPRECATED;
	
	/** Image to use when the checkbox is checked and hovered */
	UPROPERTY()
	USlateBrushAsset* CheckedHoveredImage_DEPRECATED;
	
	/** Image to use when the checkbox is checked and pressed */
	UPROPERTY()
	USlateBrushAsset* CheckedPressedImage_DEPRECATED;
	
	/** Image to use when the checkbox is in an ambiguous state and hovered */
	UPROPERTY()
	USlateBrushAsset* UndeterminedImage_DEPRECATED;
	
	/** Image to use when the checkbox is checked and hovered */
	UPROPERTY()
	USlateBrushAsset* UndeterminedHoveredImage_DEPRECATED;
	
	/** Image to use when the checkbox is in an ambiguous state and pressed */
	UPROPERTY()
	USlateBrushAsset* UndeterminedPressedImage_DEPRECATED;

	/** How the content of the toggle button should align within the given space */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Appearance)
	TEnumAsByte<EHorizontalAlignment> HorizontalAlignment;

	/** Spacing between the check box image and its content */
	UPROPERTY()
	FMargin Padding_DEPRECATED;

	/** The color of the background border */
	UPROPERTY()
	FSlateColor BorderBackgroundColor_DEPRECATED;

	/** Sometimes a button should only be mouse-clickable and never keyboard focusable. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	bool IsFocusable;

public:

	/** Called when the checked state has changed */
	UPROPERTY(BlueprintAssignable, Category="CheckBox|Event")
	FOnCheckBoxComponentStateChanged OnCheckStateChanged;

public:

	/** Returns true if this button is currently pressed */
	UFUNCTION(BlueprintCallable, Category="Widget")
	bool IsPressed() const;
	
	/** Returns true if the checkbox is currently checked */
	UFUNCTION(BlueprintCallable, Category="Widget")
	bool IsChecked() const;

	/** @return the full current checked state. */
	UFUNCTION(BlueprintCallable, Category="Widget")
	ECheckBoxState GetCheckedState() const;

	/** Sets the checked state. */
	UFUNCTION(BlueprintCallable, Category="Widget")
	void SetIsChecked(bool InIsChecked);

	/** Sets the checked state. */
	UFUNCTION(BlueprintCallable, Category="Widget")
	void SetCheckedState(ECheckBoxState InCheckedState);

public:
	
	//~ Begin UWidget Interface
	virtual void SynchronizeProperties() override;
	//~ End UWidget Interface

	//~ Begin UVisual Interface
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	//~ End UVisual Interface

	//~ Begin UObject Interface
	virtual void PostLoad() override;
	//~ End UObject Interface

#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override;
#endif

protected:

	// UPanelWidget
	virtual void OnSlotAdded(UPanelSlot* Slot) override;
	virtual void OnSlotRemoved(UPanelSlot* Slot) override;
	// End UPanelWidget

protected:
	//~ Begin UWidget Interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
#if WITH_EDITOR
	virtual TSharedRef<SWidget> RebuildDesignWidget(TSharedRef<SWidget> Content) override { return Content; }
#endif
	//~ End UWidget Interface

	void SlateOnCheckStateChangedCallback(ECheckBoxState NewState);
	
protected:
	TSharedPtr<SCheckBox> MyCheckbox;

	PROPERTY_BINDING_IMPLEMENTATION(ECheckBoxState, CheckedState)
};
