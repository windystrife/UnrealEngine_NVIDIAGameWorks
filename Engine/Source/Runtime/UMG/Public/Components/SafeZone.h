// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Widgets/SWidget.h"
#include "Components/ContentWidget.h"
#include "Widgets/Layout/SSafeZone.h"
#include "SafeZone.generated.h"

/**
 * The Safe-Zone widget is an essential part of developing a game UI that can run on lots of different non-PC platforms.
 * While a modern flat panel computer monitor may not have over scan issues, this is a common occurrence for Consoles.  
 * It's common for TVs to have extra pixels under the bezel, in addition to projectors and projection TVs having potentially
 * several vertical and horizontal columns of pixels hidden behind or against a black border of the projection screen.
 * 
 * Useful testing console commands to help, simulate the safe zone on PC,
 *   r.DebugSafeZone.TitleRatio 0.96
 *   r.DebugActionZone.ActionRatio 0.96
 * 
 * To enable a red band to visualize the safe zone, use this console command,
 * r.DebugSafeZone.Mode controls the debug visualization overlay (0..2, default 0).
 *   0: Do not display the safe zone overlay.
 *   1: Display the overlay for the title safe zone.
 *   2: Display the overlay for the action safe zone.
 */
UCLASS()
class UMG_API USafeZone : public UContentWidget
{
	GENERATED_BODY()
public:
	USafeZone();

#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override;

	virtual void OnDesignerChanged(const FDesignerChangedEventArgs& EventArgs) override;
#endif

	virtual void OnSlotAdded( UPanelSlot* Slot ) override;
	virtual void OnSlotRemoved( UPanelSlot* Slot ) override;
	virtual UClass* GetSlotClass() const override;

	void UpdateWidgetProperties();

	UFUNCTION(BlueprintCallable, Category = "SafeZone")
	void SetSidesToPad(bool InPadLeft, bool InPadRight, bool InPadTop, bool InPadBottom);

public:

	/** If this safe zone should pad for the left side of the screen's safe zone */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SafeZone")
	bool PadLeft;

	/** If this safe zone should pad for the right side of the screen's safe zone */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SafeZone")
	bool PadRight;

	/** If this safe zone should pad for the top side of the screen's safe zone */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SafeZone")
	bool PadTop;

	/** If this safe zone should pad for the bottom side of the screen's safe zone */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SafeZone")
	bool PadBottom;

protected:
	// UWidget interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	// End of UWidget interface

	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

	TSharedPtr< class SSafeZone > MySafeZone;

#if WITH_EDITOR
	TOptional<FVector2D> DesignerSize;
	TOptional<float> DesignerDpi;
#endif
};
