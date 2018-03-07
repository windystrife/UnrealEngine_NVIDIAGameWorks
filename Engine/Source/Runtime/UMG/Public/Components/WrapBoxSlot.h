// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ScriptMacros.h"
#include "Layout/Margin.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Components/PanelSlot.h"

#include "WrapBoxSlot.generated.h"

/** The Slot for the UWrapBox, contains the widget that is flowed vertically */
UCLASS()
class UMG_API UWrapBoxSlot : public UPanelSlot
{
	GENERATED_UCLASS_BODY()

public:

	/** The padding area between the slot and the content it contains. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Wrap Box Slot")
	FMargin Padding;

	/** Should this slot fill the remaining space on the line? */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Wrap Box Slot")
	bool bFillEmptySpace;

	/**
	 * If the total available space in the wrap panel drops below this threshold, this slot will attempt to fill an entire line.
	 * NOTE: A value of 0, denotes no filling will occur.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Wrap Box Slot")
	float FillSpanWhenLessThan;

	/** The alignment of the object horizontally. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Wrap Box Slot")
	TEnumAsByte<EHorizontalAlignment> HorizontalAlignment;

	/** The alignment of the object vertically. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Wrap Box Slot")
	TEnumAsByte<EVerticalAlignment> VerticalAlignment;

public:

	UFUNCTION(BlueprintCallable, Category="Layout|Wrap Box Slot")
	void SetPadding(FMargin InPadding);

	UFUNCTION(BlueprintCallable, Category="Layout|Wrap Box Slot")
	void SetFillEmptySpace(bool InbFillEmptySpace);

	UFUNCTION(BlueprintCallable, Category="Layout|Wrap Box Slot")
	void SetFillSpanWhenLessThan(float InFillSpanWhenLessThan);

	UFUNCTION(BlueprintCallable, Category="Layout|Wrap Box Slot")
	void SetHorizontalAlignment(EHorizontalAlignment InHorizontalAlignment);

	UFUNCTION(BlueprintCallable, Category="Layout|Wrap Box Slot")
	void SetVerticalAlignment(EVerticalAlignment InVerticalAlignment);

public:

	// UPanelSlot interface
	virtual void SynchronizeProperties() override;
	// End of UPanelSlot interface

	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

	/** Builds the underlying FSlot for the Slate layout panel. */
	void BuildSlot(TSharedRef<SWrapBox> InWrapBox);

private:
	//TODO UMG Slots should hold weak or shared refs to slots.

	/** A raw pointer to the slot to allow us to adjust the size, padding...etc at runtime. */
	SWrapBox::FSlot* Slot;
};
