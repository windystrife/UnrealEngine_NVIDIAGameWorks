// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ScriptMacros.h"
#include "Components/PanelSlot.h"
#include "Widgets/Layout/SUniformGridPanel.h"

#include "UniformGridSlot.generated.h"

/**
 * A slot for UUniformGridPanel, these slots all share the same size as the largest slot
 * in the grid.
 */
UCLASS()
class UMG_API UUniformGridSlot : public UPanelSlot
{
	GENERATED_UCLASS_BODY()

public:

	/** The alignment of the object horizontally. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Uniform Grid Slot")
	TEnumAsByte<EHorizontalAlignment> HorizontalAlignment;

	/** The alignment of the object vertically. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Uniform Grid Slot")
	TEnumAsByte<EVerticalAlignment> VerticalAlignment;
	
	/** The row index of the cell this slot is in */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=( UIMin = "0" ), Category="Layout|Uniform Grid Slot")
	int32 Row;
	
	/** The column index of the cell this slot is in */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=( UIMin = "0" ), Category="Layout|Uniform Grid Slot")
	int32 Column;

public:

	/** Sets the row index of the slot, this determines what cell the slot is in the panel */
	UFUNCTION(BlueprintCallable, Category="Layout|Uniform Grid Slot")
	void SetRow(int32 InRow);

	/** Sets the column index of the slot, this determines what cell the slot is in the panel */
	UFUNCTION(BlueprintCallable, Category="Layout|Uniform Grid Slot")
	void SetColumn(int32 InColumn);

	/**  */
	UFUNCTION(BlueprintCallable, Category="Layout|Uniform Grid Slot")
	void SetHorizontalAlignment(EHorizontalAlignment InHorizontalAlignment);

	/**  */
	UFUNCTION(BlueprintCallable, Category="Layout|Uniform Grid Slot")
	void SetVerticalAlignment(EVerticalAlignment InVerticalAlignment);

public:

	// UPanelSlot interface
	virtual void SynchronizeProperties() override;
	// End of UPanelSlot interface

	/** Builds the underlying FSlot for the Slate layout panel. */
	void BuildSlot(TSharedRef<SUniformGridPanel> GridPanel);

	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

private:
	//TODO UMG Slots should hold weak or shared refs to slots.

	/** A raw pointer to the slot to allow us to adjust the size, padding...etc at runtime. */
	SUniformGridPanel::FSlot* Slot;
};
