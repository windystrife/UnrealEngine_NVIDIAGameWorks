// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Widgets/SWidget.h"
#include "Widgets/SOverlay.h"
#include "Components/PanelWidget.h"
#include "Overlay.generated.h"

class UOverlaySlot;

/**
 * Allows widgets to be stacked on top of each other, uses simple flow layout for content on each layer.
 */
UCLASS()
class UMG_API UOverlay : public UPanelWidget
{
	GENERATED_UCLASS_BODY()

	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

	/**  */
	UFUNCTION(BlueprintCallable, Category="Widget")
	UOverlaySlot* AddChildToOverlay(UWidget* Content);

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

	TSharedPtr<class SOverlay> MyOverlay;

protected:
	// UWidget interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	// End of UWidget interface
};
