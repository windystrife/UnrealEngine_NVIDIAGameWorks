// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ScriptMacros.h"
#include "Layout/Margin.h"
#include "Components/PanelSlot.h"
#include "Widgets/Layout/SGridPanel.h"

#include "GridSlot.generated.h"

/**
 * A slot for UGridPanel, these slots all share the same size as the largest slot
 * in the grid.
 */
UCLASS()
class UMG_API UGridSlot : public UPanelSlot
{
	GENERATED_UCLASS_BODY()

public:

	/** The padding area between the slot and the content it contains. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Grid Slot")
	FMargin Padding;

	/** The alignment of the object horizontally. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Grid Slot")
	TEnumAsByte<EHorizontalAlignment> HorizontalAlignment;

	/** The alignment of the object vertically. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Grid Slot")
	TEnumAsByte<EVerticalAlignment> VerticalAlignment;
	
	/** The row index of the cell this slot is in */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=( UIMin = "0" ), Category="Layout|Grid Slot")
	int32 Row;
	
	/**  */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Grid Slot")
	int32 RowSpan;

	/** The column index of the cell this slot is in */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=( UIMin = "0" ), Category="Layout|Grid Slot")
	int32 Column;

	/**  */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Grid Slot")
	int32 ColumnSpan;

	/** Positive values offset this cell to be hit-tested and drawn on top of others. Default is 0; i.e. no offset. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Grid Slot")
	int32 Layer;

	/** Offset this slot's content by some amount; positive values offset to lower right */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Grid Slot")
	FVector2D Nudge;

public:

	UFUNCTION(BlueprintCallable, Category="Layout|Border Slot")
	void SetPadding(FMargin InPadding);

	/** Sets the row index of the slot, this determines what cell the slot is in the panel */
	UFUNCTION(BlueprintCallable, Category="Layout|Grid Slot")
	void SetRow(int32 InRow);

	/** How many rows this this slot spans over */
	UFUNCTION(BlueprintCallable, Category="Layout|Grid Slot")
	void SetRowSpan(int32 InRowSpan);

	/** Sets the column index of the slot, this determines what cell the slot is in the panel */
	UFUNCTION(BlueprintCallable, Category="Layout|Grid Slot")
	void SetColumn(int32 InColumn);

	/** How many columns this slot spans over */
	UFUNCTION(BlueprintCallable, Category="Layout|Grid Slot")
	void SetColumnSpan(int32 InColumnSpan);

	/** Sets positive values offset this cell to be hit-tested and drawn on top of others. */
	UFUNCTION(BlueprintCallable, Category = "Layout|Grid Slot")
	void SetLayer(int32 InLayer);

	/** Sets the offset for this slot's content by some amount; positive values offset to lower right*/
	void SetNudge(FVector2D InNudge);

	/**  */
	UFUNCTION(BlueprintCallable, Category="Layout|Grid Slot")
	void SetHorizontalAlignment(EHorizontalAlignment InHorizontalAlignment);

	/**  */
	UFUNCTION(BlueprintCallable, Category="Layout|Grid Slot")
	void SetVerticalAlignment(EVerticalAlignment InVerticalAlignment);

	// UPanelSlot interface
	virtual void SynchronizeProperties() override;
	// End of UPanelSlot interface

	/** Builds the underlying FSlot for the Slate layout panel. */
	void BuildSlot(TSharedRef<SGridPanel> GridPanel);

	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

private:
	//TODO UMG Slots should hold weak or shared refs to slots.

	/** A raw pointer to the slot to allow us to adjust the size, padding...etc at runtime. */
	SGridPanel::FSlot* Slot;
};
