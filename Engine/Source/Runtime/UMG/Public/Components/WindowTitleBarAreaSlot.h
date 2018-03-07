// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Layout/Margin.h"
#include "Components/PanelSlot.h"
#include "Components/WindowTitleBarArea.h"

#include "WindowTitleBarAreaSlot.generated.h"

class SWindowTitleBarArea;

/** The Slot for the UWindowTitleBarArea */
UCLASS()
class UMG_API UWindowTitleBarAreaSlot : public UPanelSlot
{
	GENERATED_UCLASS_BODY()

public:

	UFUNCTION(BlueprintCallable, Category="Layout|WindowTitleBarArea Slot")
	void SetPadding(FMargin InPadding);

	UFUNCTION(BlueprintCallable, Category="Layout|WindowTitleBarArea Slot")
	void SetHorizontalAlignment(EHorizontalAlignment InHorizontalAlignment);

	UFUNCTION(BlueprintCallable, Category = "Layout|WindowTitleBarArea Slot")
	void SetVerticalAlignment(EVerticalAlignment InVerticalAlignment);

protected:

	/** The padding area between the slot and the content it contains. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout|WindowTitleBarArea Slot")
	FMargin Padding;

	/** The alignment of the object horizontally. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout|WindowTitleBarArea Slot")
	TEnumAsByte<EHorizontalAlignment> HorizontalAlignment;

	/** The alignment of the object vertically. */
	UPROPERTY(EditAnywhere, Category = "Layout|WindowTitleBarArea Slot")
	TEnumAsByte<EVerticalAlignment> VerticalAlignment;

public:

	// UPanelSlot interface
	virtual void SynchronizeProperties() override;
	// End of UPanelSlot interface

	/** Builds the underlying FSlot for the Slate layout panel. */
	void BuildSlot(TSharedRef<SWindowTitleBarArea> WindowTitleBarArea);

	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

private:

	/** A pointer to the window zone to allow us to adjust the size, padding...etc at runtime. */
	TSharedPtr<SWindowTitleBarArea> WindowTitleBarArea;

	friend UWindowTitleBarArea;
};
