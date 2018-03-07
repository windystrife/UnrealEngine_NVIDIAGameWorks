// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Input/Reply.h"
#include "Styling/SlateTypes.h"
#include "Widgets/SWidget.h"
#include "Components/ContentWidget.h"

#include "Button.generated.h"

class SButton;
class USlateWidgetStyleAsset;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnButtonClickedEvent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnButtonPressedEvent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnButtonReleasedEvent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnButtonHoverEvent);

/**
 * The button is a click-able primitive widget to enable basic interaction, you
 * can place any other widget inside a button to make a more complex and 
 * interesting click-able element in your UI.
 *
 * * Single Child
 * * Clickable
 */
UCLASS()
class UMG_API UButton : public UContentWidget
{
	GENERATED_UCLASS_BODY()

public:
	/** The template style asset, used to seed the mutable instance of the style. */
	UPROPERTY()
	USlateWidgetStyleAsset* Style_DEPRECATED;

	/** The button style used at runtime */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance", meta=( DisplayName="Style" ))
	FButtonStyle WidgetStyle;
	
	/** The color multiplier for the button content */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance", meta=( sRGB="true" ))
	FLinearColor ColorAndOpacity;
	
	/** The color multiplier for the button background */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance", meta=( sRGB="true" ))
	FLinearColor BackgroundColor;

	/** The type of mouse action required by the user to trigger the buttons 'Click' */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Interaction", AdvancedDisplay)
	TEnumAsByte<EButtonClickMethod::Type> ClickMethod;

	/** The type of touch action required by the user to trigger the buttons 'Click' */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Interaction", AdvancedDisplay)
	TEnumAsByte<EButtonTouchMethod::Type> TouchMethod;

	/** Sometimes a button should only be mouse-clickable and never keyboard focusable. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Interaction")
	bool IsFocusable;

public:

	/** Called when the button is clicked */
	UPROPERTY(BlueprintAssignable, Category="Button|Event")
	FOnButtonClickedEvent OnClicked;

	/** Called when the button is pressed */
	UPROPERTY(BlueprintAssignable, Category="Button|Event")
	FOnButtonPressedEvent OnPressed;

	/** Called when the button is released */
	UPROPERTY(BlueprintAssignable, Category="Button|Event")
	FOnButtonReleasedEvent OnReleased;

	UPROPERTY( BlueprintAssignable, Category = "Button|Event" )
	FOnButtonHoverEvent OnHovered;

	UPROPERTY( BlueprintAssignable, Category = "Button|Event" )
	FOnButtonHoverEvent OnUnhovered;

public:
	
	/** Sets the color multiplier for the button background */
	UFUNCTION(BlueprintCallable, Category="Button|Appearance")
	void SetStyle(const FButtonStyle& InStyle);

	/** Sets the color multiplier for the button content */
	UFUNCTION(BlueprintCallable, Category="Button|Appearance")
	void SetColorAndOpacity(FLinearColor InColorAndOpacity);

	/** Sets the color multiplier for the button background */
	UFUNCTION(BlueprintCallable, Category="Button|Appearance")
	void SetBackgroundColor(FLinearColor InBackgroundColor);

	/**
	 * Returns true if the user is actively pressing the button.  Do not use this for detecting 'Clicks', use the OnClicked event instead.
	 *
	 * @return true if the user is actively pressing the button otherwise false.
	 */
	UFUNCTION(BlueprintCallable, Category="Button")
	bool IsPressed() const;

	UFUNCTION(BlueprintCallable, Category="Button")
	void SetClickMethod(EButtonClickMethod::Type InClickMethod);

	UFUNCTION(BlueprintCallable, Category="Button")
	void SetTouchMethod(EButtonTouchMethod::Type InTouchMethod);

	//UFUNCTION(BlueprintCallable, Category="Button")
	//void SetPressMethod(EButtonPressMethod::Type InPressMethod);

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
	virtual UClass* GetSlotClass() const override;
	virtual void OnSlotAdded(UPanelSlot* Slot) override;
	virtual void OnSlotRemoved(UPanelSlot* Slot) override;
	// End UPanelWidget

protected:
	/** Handle the actual click event from slate and forward it on */
	FReply SlateHandleClicked();
	void SlateHandlePressed();
	void SlateHandleReleased();
	void SlateHandleHovered();
	void SlateHandleUnhovered();

protected:
	//~ Begin UWidget Interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
#if WITH_EDITOR
	virtual TSharedRef<SWidget> RebuildDesignWidget(TSharedRef<SWidget> Content) override { return Content; }
#endif
	//~ End UWidget Interface

protected:
	/** Cached pointer to the underlying slate button owned by this UWidget */
	TSharedPtr<SButton> MyButton;
};
