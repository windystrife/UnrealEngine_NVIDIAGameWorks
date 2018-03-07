// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Fonts/SlateFontInfo.h"
#include "Layout/Margin.h"
#include "Styling/SlateTypes.h"
#include "Widgets/SWidget.h"
#include "Components/Widget.h"
#include "Components/TextWidgetTypes.h"
#include "EditableTextBox.generated.h"

class SEditableTextBox;
class USlateWidgetStyleAsset;

/**
 * Allows the user to type in custom text.  Only permits a single line of text to be entered.
 * 
 * * No Children
 * * Text Entry
 */
UCLASS(meta=(DisplayName="Text Box"))
class UMG_API UEditableTextBox : public UWidget
{
	GENERATED_UCLASS_BODY()

public:

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEditableTextBoxChangedEvent, const FText&, Text);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEditableTextBoxCommittedEvent, const FText&, Text, ETextCommit::Type, CommitMethod);

public:
	/** The text content for this editable text box widget */
	UPROPERTY(EditAnywhere, Category=Content)
	FText Text;

	/** A bindable delegate to allow logic to drive the text of the widget */
	UPROPERTY()
	FGetText TextDelegate;

public:

	/** The style */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Style", meta=( DisplayName="Style" ))
	FEditableTextBoxStyle WidgetStyle;

	/** Style used for the text box */
	UPROPERTY()
	USlateWidgetStyleAsset* Style_DEPRECATED;

	/** Hint text that appears when there is no text in the text box */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Content)
	FText HintText;

	/** A bindable delegate to allow logic to drive the hint text of the widget */
	UPROPERTY()
	FGetText HintTextDelegate;

	/** Font color and opacity (overrides Style) */
	UPROPERTY()
	FSlateFontInfo Font_DEPRECATED;

	/** Text color and opacity (overrides Style) */
	UPROPERTY()
	FLinearColor ForegroundColor_DEPRECATED;

	/** The color of the background/border around the editable text (overrides Style) */
	UPROPERTY()
	FLinearColor BackgroundColor_DEPRECATED;

	/** Text color and opacity when read-only (overrides Style) */
	UPROPERTY()
	FLinearColor ReadOnlyForegroundColor_DEPRECATED;

	/** Sets whether this text box can actually be modified interactively by the user */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Appearance)
	bool IsReadOnly;

	/** Sets whether this text box is for storing a password */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Appearance)
	bool IsPassword;

	/** Minimum width that a text block should be */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Appearance)
	float MinimumDesiredWidth;

	/** Padding between the box/border and the text widget inside (overrides Style) */
	UPROPERTY()
	FMargin Padding_DEPRECATED;

	/** Workaround as we lose focus when the auto completion closes. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Behavior, AdvancedDisplay)
	bool IsCaretMovedWhenGainFocus;

	/** Whether to select all text when the user clicks to give focus on the widget */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Behavior, AdvancedDisplay)
	bool SelectAllTextWhenFocused;

	/** Whether to allow the user to back out of changes when they press the escape key */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Behavior, AdvancedDisplay)
	bool RevertTextOnEscape;

	/** Whether to clear keyboard focus when pressing enter to commit changes */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Behavior, AdvancedDisplay)
	bool ClearKeyboardFocusOnCommit;

	/** Whether to select all text when pressing enter to commit changes */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Behavior, AdvancedDisplay)
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
	UPROPERTY(BlueprintAssignable, Category="TextBox|Event")
	FOnEditableTextBoxChangedEvent OnTextChanged;

	/** Called whenever the text is committed.  This happens when the user presses enter or the text box loses focus. */
	UPROPERTY(BlueprintAssignable, Category="TextBox|Event")
	FOnEditableTextBoxCommittedEvent OnTextCommitted;

	/** Provide a alternative mechanism for error reporting. */
	//SLATE_ARGUMENT(TSharedPtr<class IErrorReportingWidget>, ErrorReporting)

public:

	/**  */
	UFUNCTION(BlueprintCallable, Category="Widget", meta=(DisplayName="GetText (Text Box)"))
	FText GetText() const;

	/**  */
	UFUNCTION(BlueprintCallable, Category="Widget", meta=(DisplayName="SetText (Text Box)"))
	void SetText(FText InText);

	UFUNCTION(BlueprintCallable, Category = "Widget", meta = (DisplayName = "Set Hint Text (Text Box)"))
	void SetHintText(FText InText);

	UFUNCTION(BlueprintCallable, Category="Widget",  meta=(DisplayName="SetError (Text Box)"))
	void SetError(FText InError);

	UFUNCTION(BlueprintCallable, Category="Widget", meta=(DisplayName="SetIsReadOnly (Text Box)"))
	void SetIsReadOnly(bool bReadOnly);

	UFUNCTION(BlueprintCallable, Category="Widget")
	void ClearError();

	UFUNCTION(BlueprintCallable, Category = "Widget")
	bool HasError() const;

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
	//~ Begin UWidget Interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	// End of UWidget

	virtual void HandleOnTextChanged(const FText& Text);
	virtual void HandleOnTextCommitted(const FText& Text, ETextCommit::Type CommitMethod);

protected:
	TSharedPtr<SEditableTextBox> MyEditableTextBlock;

	PROPERTY_BINDING_IMPLEMENTATION(FText, Text);
	PROPERTY_BINDING_IMPLEMENTATION(FText, HintText);
};
