// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/SlateColor.h"
#include "Styling/SlateTypes.h"
#include "Widgets/SWidget.h"
#include "Components/Widget.h"
#include "Components/TextWidgetTypes.h"
#include "EditableText.generated.h"

class SEditableText;
class USlateBrushAsset;
class USlateWidgetStyleAsset;

/**
 * Editable text box widget
 */
UCLASS()
class UMG_API UEditableText : public UWidget
{
	GENERATED_UCLASS_BODY()

public:

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEditableTextChangedEvent, const FText&, Text);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEditableTextCommittedEvent, const FText&, Text, ETextCommit::Type, CommitMethod);

public:
	/** The text content for this editable text box widget */
	UPROPERTY(EditAnywhere, Category=Content)
	FText Text;

	/** A bindable delegate to allow logic to drive the text of the widget */
	UPROPERTY()
	FGetText TextDelegate;

public:

	/** Hint text that appears when there is no text in the text box */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Content)
	FText HintText;

	/** A bindable delegate to allow logic to drive the hint text of the widget */
	UPROPERTY()
	FGetText HintTextDelegate;

	/** The style */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Style", meta=( DisplayName="Style" ))
	FEditableTextStyle WidgetStyle;

	/** Text style */
	UPROPERTY()
	USlateWidgetStyleAsset* Style_DEPRECATED;

	/** Background image for the selected text (overrides Style) */
	UPROPERTY()
	USlateBrushAsset* BackgroundImageSelected_DEPRECATED;

	/** Background image for the composing text (overrides Style) */
	UPROPERTY()
	USlateBrushAsset* BackgroundImageComposing_DEPRECATED;

	/** Image brush used for the caret (overrides Style) */
	UPROPERTY()
	USlateBrushAsset* CaretImage_DEPRECATED;

	/** Font color and opacity (overrides Style) */
	UPROPERTY()
	FSlateFontInfo Font_DEPRECATED;

	/** Text color and opacity (overrides Style) */
	UPROPERTY()
	FSlateColor ColorAndOpacity_DEPRECATED;

	/** Sets whether this text box can actually be modified interactively by the user */
	UPROPERTY(EditAnywhere, Category=Appearance)
	bool IsReadOnly;

	/** Sets whether this text box is for storing a password */
	UPROPERTY(EditAnywhere, Category=Appearance)
	bool IsPassword;

	/** Minimum width that a text block should be */
	UPROPERTY(EditAnywhere, Category=Appearance)
	float MinimumDesiredWidth;

	/** Workaround as we lose focus when the auto completion closes. */
	UPROPERTY(EditAnywhere, Category=Behavior, AdvancedDisplay)
	bool IsCaretMovedWhenGainFocus;

	/** Whether to select all text when the user clicks to give focus on the widget */
	UPROPERTY(EditAnywhere, Category=Behavior, AdvancedDisplay)
	bool SelectAllTextWhenFocused;

	/** Whether to allow the user to back out of changes when they press the escape key */
	UPROPERTY(EditAnywhere, Category=Behavior, AdvancedDisplay)
	bool RevertTextOnEscape;

	/** Whether to clear keyboard focus when pressing enter to commit changes */
	UPROPERTY(EditAnywhere, Category=Behavior, AdvancedDisplay)
	bool ClearKeyboardFocusOnCommit;

	/** Whether to select all text when pressing enter to commit changes */
	UPROPERTY(EditAnywhere, Category=Behavior, AdvancedDisplay)
	bool SelectAllTextOnCommit;

	/** Whether the context menu can be opened */
	UPROPERTY(EditAnywhere, Category=Behavior, AdvancedDisplay)
	bool AllowContextMenu;

	/** If we're on a platform that requires a virtual keyboard, what kind of keyboard should this widget use? */
	UPROPERTY(EditAnywhere, Category=Behavior, AdvancedDisplay)
	TEnumAsByte<EVirtualKeyboardType::Type> KeyboardType;

	/** Controls how the text within this widget should be shaped. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Localization, AdvancedDisplay, meta=(ShowOnlyInnerProperties))
	FShapedTextOptions ShapedTextOptions;

public:

	/** Called whenever the text is changed interactively by the user */
	UPROPERTY(BlueprintAssignable, Category="Widget Event", meta=(DisplayName="OnTextChanged (Editable Text)"))
	FOnEditableTextChangedEvent OnTextChanged;

	/** Called whenever the text is committed.  This happens when the user presses enter or the text box loses focus. */
	UPROPERTY(BlueprintAssignable, Category="Widget Event", meta=(DisplayName="OnTextCommitted (Editable Text)"))
	FOnEditableTextCommittedEvent OnTextCommitted;

	/**
	 * Gets the widget text
	 * @return The widget text
	 */
	UFUNCTION(BlueprintCallable, Category = "Widget", meta=(DisplayName="GetText (Editable Text)"))
	FText GetText() const;

	/**
	 * Directly sets the widget text.
	 * Warning: This will wipe any binding created for the Text property!
	 * @param InText The text to assign to the widget
	 */
	UFUNCTION(BlueprintCallable, Category = "Widget", meta=(DisplayName="SetText (Editable Text)"))
	void SetText(FText InText);

	UFUNCTION(BlueprintCallable, Category = "Widget")
	void SetIsPassword(bool InbIsPassword);

	UFUNCTION(BlueprintCallable, Category = "Widget")
	void SetHintText(FText InHintText);

	UFUNCTION(BlueprintCallable, Category = "Widget", meta=(DisplayName="SetIsReadOnly (Editable Text)"))
	void SetIsReadOnly(bool InbIsReadyOnly);

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
	//~ End UWidget Interface

protected:
	//~ Begin UWidget Interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	// End of UWidget

	void HandleOnTextChanged(const FText& Text);
	void HandleOnTextCommitted(const FText& Text, ETextCommit::Type CommitMethod);

protected:
	TSharedPtr<SEditableText> MyEditableText;

	PROPERTY_BINDING_IMPLEMENTATION(FText, Text);
	PROPERTY_BINDING_IMPLEMENTATION(FText, HintText);
};
