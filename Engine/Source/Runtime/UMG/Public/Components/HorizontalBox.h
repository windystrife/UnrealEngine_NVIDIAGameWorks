// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Widgets/SWidget.h"
#include "Widgets/SBoxPanel.h"
#include "Components/PanelWidget.h"
#include "HorizontalBox.generated.h"

class UHorizontalBoxSlot;

/**
 * Allows widgets to be laid out in a flow horizontally.
 *
 * * Many Children
 * * Flow Horizontal
 */
UCLASS()
class UMG_API UHorizontalBox : public UPanelWidget
{
	GENERATED_UCLASS_BODY()

	/**  */
	UFUNCTION(BlueprintCallable, Category="Widget")
	UHorizontalBoxSlot* AddChildToHorizontalBox(UWidget* Content);

#if WITH_EDITOR
	// UWidget interface
	virtual const FText GetPaletteCategory() override;
	// End UWidget interface
#endif

	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

protected:

	// UPanelWidget
	virtual UClass* GetSlotClass() const override;
	virtual void OnSlotAdded(UPanelSlot* Slot) override;
	virtual void OnSlotRemoved(UPanelSlot* Slot) override;
	// End UPanelWidget

protected:

	TSharedPtr<class SHorizontalBox> MyHorizontalBox;

protected:
	// UWidget interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	// End of UWidget interface
};
