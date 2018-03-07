// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/ScriptMacros.h"
#include "SlateWrapperTypes.h"
#include "PanelSlot.h"
#include "Layout/Margin.h"
#include "BackgroundBlurSlot.generated.h"


class SBackgroundBlur;
class UBackgroundBlur;

/**
 * The Slot for the UBackgroundBlurSlot, contains the widget displayed in a BackgroundBlur's single slot
 */
UCLASS()
class UMG_API UBackgroundBlurSlot : public UPanelSlot
{
	GENERATED_UCLASS_BODY()

public:

	UFUNCTION(BlueprintCallable, Category="Layout|Background Blur Slot")
	void SetPadding(FMargin InPadding);

	UFUNCTION(BlueprintCallable, Category="Layout|Background Blur Slot")
	void SetHorizontalAlignment(EHorizontalAlignment InHorizontalAlignment);

	UFUNCTION(BlueprintCallable, Category="Layout|Background Blur Slot")
	void SetVerticalAlignment(EVerticalAlignment InVerticalAlignment);

protected:
	/** The padding area between the slot and the content it contains. */
	UPROPERTY(EditAnywhere, Category="Layout|Background Blur Slot")
	FMargin Padding;

	/** The alignment of the object horizontally. */
	UPROPERTY(EditAnywhere, Category="Layout|Background Blur Slot")
	TEnumAsByte<EHorizontalAlignment> HorizontalAlignment;

	/** The alignment of the object vertically. */
	UPROPERTY(EditAnywhere, Category="Layout|Background Blur Slot")
	TEnumAsByte<EVerticalAlignment> VerticalAlignment;

public:

	// UPanelSlot interface
	virtual void SynchronizeProperties() override;
	// End of UPanelSlot interface

	/** Builds the underlying slot for the slate BackgroundBlur. */
	void BuildSlot(TSharedRef<SBackgroundBlur> InBackgroundBlur);

	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

public:

#if WITH_EDITOR

	// UObject interface
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	// End of UObject interface

#endif

private:

	/** A pointer to the BackgroundBlur to allow us to adjust the size, padding...etc at runtime. */
	TSharedPtr<SBackgroundBlur> BackgroundBlur;

	friend UBackgroundBlur;
};
