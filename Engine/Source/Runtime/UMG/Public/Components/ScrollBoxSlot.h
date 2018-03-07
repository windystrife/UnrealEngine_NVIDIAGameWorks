// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ScriptMacros.h"
#include "Layout/Margin.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Components/PanelSlot.h"

#include "ScrollBoxSlot.generated.h"

/** The Slot for the UScrollBox, contains the widget that are scrollable */
UCLASS()
class UMG_API UScrollBoxSlot : public UPanelSlot
{
	GENERATED_UCLASS_BODY()

public:
	
	/** The padding area between the slot and the content it contains. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|ScrollBox Slot")
	FMargin Padding;

	/** The alignment of the object horizontally. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|ScrollBox Slot")
	TEnumAsByte<EHorizontalAlignment> HorizontalAlignment;

public:

	UFUNCTION(BlueprintCallable, Category="Layout|ScrollBox Slot")
	void SetPadding(FMargin InPadding);

	UFUNCTION(BlueprintCallable, Category="Layout|ScrollBox Slot")
	void SetHorizontalAlignment(EHorizontalAlignment InHorizontalAlignment);

public:

	// UPanelSlot interface
	virtual void SynchronizeProperties() override;
	// End of UPanelSlot interface

	/** Builds the underlying FSlot for the Slate layout panel. */
	void BuildSlot(TSharedRef<SScrollBox> ScrollBox);

	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

private:
	//TODO UMG Slots should hold weak or shared refs to slots.

	/** A raw pointer to the slot to allow us to adjust the size, padding...etc at runtime. */
	SScrollBox::FSlot* Slot;
};
