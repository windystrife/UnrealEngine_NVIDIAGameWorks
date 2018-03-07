// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/SlateTypes.h"
#include "Widgets/SWidget.h"
#include "Components/TextWidgetTypes.h"
#include "MultiLineEditableText.generated.h"

class SMultiLineEditableText;

/**
 * Editable text box widget
 */
UCLASS(meta=( DisplayName="Editable Text (Multi-Line)" ))
class UMG_API UMultiLineEditableText : public UTextLayoutWidget
{
	GENERATED_UCLASS_BODY()

public:

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMultiLineEditableTextChangedEvent, const FText&, Text);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMultiLineEditableTextCommittedEvent, const FText&, Text, ETextCommit::Type, CommitMethod);

public:
	/** The text content for this editable text box widget */
	UPROPERTY(EditAnywhere, Category=Content, meta=(MultiLine="true"))
	FText Text;

	/** Hint text that appears when there is no text in the text box */
	UPROPERTY(EditAnywhere, Category=Content, meta=(MultiLine="true"))
	FText HintText;

	/** A bindable delegate to allow logic to drive the hint text of the widget */
	UPROPERTY()
	FGetText HintTextDelegate;
public:

	/** The style */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Style", meta=(ShowOnlyInnerProperties))
	FTextBlockStyle WidgetStyle;

	/** Sets whether this text block can be modified interactively by the user */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance")
	bool bIsReadOnly;

	/** Font color and opacity (overrides Style) */
	UPROPERTY()
	FSlateFontInfo Font_DEPRECATED;

	/** Whether the context menu can be opened */
	UPROPERTY(EditAnywhere, Category = Behavior, AdvancedDisplay)
	bool AllowContextMenu;

	/** Called whenever the text is changed interactively by the user */
	UPROPERTY(BlueprintAssignable, Category="Widget Event", meta=(DisplayName="OnTextChanged (Multi-Line Editable Text)"))
	FOnMultiLineEditableTextChangedEvent OnTextChanged;

	/** Called whenever the text is committed.  This happens when the user presses enter or the text box loses focus. */
	UPROPERTY(BlueprintAssignable, Category="Widget Event", meta=(DisplayName="OnTextCommitted (Multi-Line Editable Text)"))
	FOnMultiLineEditableTextCommittedEvent OnTextCommitted;

public:

	/**  */
	UFUNCTION(BlueprintCallable, Category="Widget", meta=(DisplayName="GetText (Multi-Line Editable Text)"))
	FText GetText() const;

	/**  */
	UFUNCTION(BlueprintCallable, Category="Widget", meta=(DisplayName="SetText (Multi-Line Editable Text)"))
	void SetText(FText InText);

	UFUNCTION(BlueprintCallable, Category="Widget", meta=(DisplayName="SetIsReadOnly (Multi-Line Editable Text"))
	void SetIsReadOnly(bool bReadOnly);
	
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

	void HandleOnTextChanged(const FText& Text);
	void HandleOnTextCommitted(const FText& Text, ETextCommit::Type CommitMethod);

protected:
	TSharedPtr<SMultiLineEditableText> MyMultiLineEditableText;

	PROPERTY_BINDING_IMPLEMENTATION(FText, HintText);
};
