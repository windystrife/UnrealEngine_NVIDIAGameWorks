// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ScriptMacros.h"
#include "Layout/Margin.h"
#include "Components/PanelSlot.h"

#include "BorderSlot.generated.h"

class SBorder;
class UBorder;

/**
 * The Slot for the UBorderSlot, contains the widget displayed in a border's single slot
 */
UCLASS()
class UMG_API UBorderSlot : public UPanelSlot
{
	GENERATED_UCLASS_BODY()

public:

	UFUNCTION(BlueprintCallable, Category="Layout|Border Slot")
	void SetPadding(FMargin InPadding);

	UFUNCTION(BlueprintCallable, Category="Layout|Border Slot")
	void SetHorizontalAlignment(EHorizontalAlignment InHorizontalAlignment);

	UFUNCTION(BlueprintCallable, Category="Layout|Border Slot")
	void SetVerticalAlignment(EVerticalAlignment InVerticalAlignment);

protected:
	/** The padding area between the slot and the content it contains. */
	UPROPERTY(EditAnywhere, Category="Layout|Border Slot")
	FMargin Padding;

	/** The alignment of the object horizontally. */
	UPROPERTY(EditAnywhere, Category="Layout|Border Slot")
	TEnumAsByte<EHorizontalAlignment> HorizontalAlignment;

	/** The alignment of the object vertically. */
	UPROPERTY(EditAnywhere, Category="Layout|Border Slot")
	TEnumAsByte<EVerticalAlignment> VerticalAlignment;

public:

	// UPanelSlot interface
	virtual void SynchronizeProperties() override;
	// End of UPanelSlot interface

	/** Builds the underlying slot for the slate border. */
	void BuildSlot(TSharedRef<SBorder> InBorder);

	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

public:

#if WITH_EDITOR

	// UObject interface
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	// End of UObject interface

#endif

private:

	/** A pointer to the border to allow us to adjust the size, padding...etc at runtime. */
	TSharedPtr<SBorder> Border;

	friend UBorder;
};
