// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Widgets/SWidget.h"
#include "Widgets/SBoxPanel.h"
#include "Components/PanelWidget.h"
#include "VerticalBox.generated.h"

class UVerticalBoxSlot;

/**
 * A vertical box widget is a layout panel allowing child widgets to be automatically laid out
 * vertically.
 *
 * * Many Children
 * * Flows Vertical
 */
UCLASS()
class UMG_API UVerticalBox : public UPanelWidget
{
	GENERATED_UCLASS_BODY()

public:

	/**  */
	UFUNCTION(BlueprintCallable, Category="Panel")
	UVerticalBoxSlot* AddChildToVerticalBox(UWidget* Content);

#if WITH_EDITOR
	// UWidget interface
	virtual const FText GetPaletteCategory() override;
	// End UWidget interface
#endif

protected:

	// UPanelWidget
	virtual UClass* GetSlotClass() const override;
	virtual void OnSlotAdded(UPanelSlot* Slot) override;
	virtual void OnSlotRemoved(UPanelSlot* Slot) override;
	// End UPanelWidget

	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

protected:

	TSharedPtr<class SVerticalBox> MyVerticalBox;

protected:
	// UWidget interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	// End of UWidget interface
};
