// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Widgets/SWidget.h"
#include "Components/ContentWidget.h"
#include "NamedSlot.generated.h"

class SBox;

/**
 * Allows you to expose an external slot for your user widget.  When others reuse your user control, 
 * they can put whatever they want in this named slot.
 */
UCLASS()
class UMG_API UNamedSlot : public UContentWidget
{
	GENERATED_UCLASS_BODY()

public:

	// UPanelWidget interface
	virtual void OnSlotAdded(UPanelSlot* Slot) override;
	virtual void OnSlotRemoved(UPanelSlot* Slot) override;
	// End of UPanelWidget interface

	// UVisual interface
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	// End of UVisual interface

#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override;
#endif

protected:
	// UWidget interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	// End of UWidget interface

protected:
	TSharedPtr<SBox> MyBox;
};
