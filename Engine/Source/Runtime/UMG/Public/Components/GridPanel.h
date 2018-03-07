// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Widgets/SWidget.h"
#include "Components/PanelWidget.h"
#include "GridPanel.generated.h"

class SGridPanel;
class UGridSlot;

/**
 * A panel that evenly divides up available space between all of its children.
 * 
 * * Many Children
 */
UCLASS()
class UMG_API UGridPanel : public UPanelWidget
{
	GENERATED_UCLASS_BODY()

public:

	/** The column fill rules */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fill Rules")
	TArray<float> ColumnFill;

	/** The row fill rules */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fill Rules")
	TArray<float> RowFill;

public:

	/**  */
	UFUNCTION(BlueprintCallable, Category="Widget")
	UGridSlot* AddChildToGrid(UWidget* Content);

public:

	// UWidget interface
	virtual void SynchronizeProperties() override;
	// End of UWidget interface

	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

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

	TSharedPtr<SGridPanel> MyGridPanel;

protected:
	// UWidget interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	// End of UWidget interface
};
