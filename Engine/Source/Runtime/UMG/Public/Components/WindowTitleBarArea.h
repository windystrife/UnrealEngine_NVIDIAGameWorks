// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Layout/Margin.h"
#include "GenericPlatform/GenericWindow.h"
#include "Widgets/SWidget.h"
#include "Components/ContentWidget.h"

#include "WindowTitleBarArea.generated.h"

class SWindowTitleBarArea;

/**
* A panel for defining a region of the UI that should allow users to drag the window on desktop platforms.
*/

UCLASS()
class UMG_API UWindowTitleBarArea : public UContentWidget
{
	GENERATED_UCLASS_BODY()

public:

	/** Should double clicking the title bar area toggle fullscreen instead of maximizing the window. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction", meta=(DisplayName="Toggle Fullscreen"))
	bool bDoubleClickTogglesFullscreen;

public:

	UFUNCTION(BlueprintCallable, Category = "Appearance")
	void SetPadding(FMargin InPadding);

	UFUNCTION(BlueprintCallable, Category = "Appearance")
	void SetHorizontalAlignment(EHorizontalAlignment InHorizontalAlignment);

	UFUNCTION(BlueprintCallable, Category = "Appearance")
	void SetVerticalAlignment(EVerticalAlignment InVerticalAlignment);

public:

	//~ Begin UVisual Interface
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	//~ End UVisual Interface

	//~ Begin UObject Interface
	virtual void PostLoad() override;
	//~ End UObject Interface

protected:
	//~ Begin UWidget Interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	//~ End UWidget Interface

protected:

	// UPanelWidget
	virtual UClass* GetSlotClass() const override;
	virtual void OnSlotAdded(UPanelSlot* Slot) override;
	virtual void OnSlotRemoved(UPanelSlot* Slot) override;
	// End UPanelWidget

protected:
	/** Cached pointer to the underlying slate button owned by this UWidget */
	TSharedPtr<SWindowTitleBarArea> MyWindowTitleBarArea;

private:

	bool HandleWindowAction(const TSharedRef<FGenericWindow>& PlatformWindow, EWindowAction::Type WindowAction);
	void HandleMouseButtonDoubleClick();

private:

	FDelegateHandle WindowActionNotificationHandle;
};
