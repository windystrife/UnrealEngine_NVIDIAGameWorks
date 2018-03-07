// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ScriptMacros.h"
#include "Layout/Margin.h"
#include "Widgets/SBoxPanel.h"
#include "Components/PanelSlot.h"
#include "Components/SlateWrapperTypes.h"

#include "HorizontalBoxSlot.generated.h"

UCLASS()
class UMG_API UHorizontalBoxSlot : public UPanelSlot
{
	GENERATED_UCLASS_BODY()

public:
	
	/** The amount of padding between the slots parent and the content. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Horizontal Box Slot")
	FMargin Padding;

	/** How much space this slot should occupy in the direction of the panel. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Horizontal Box Slot")
	FSlateChildSize Size;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Horizontal Box Slot")
	TEnumAsByte<EHorizontalAlignment> HorizontalAlignment;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout|Horizontal Box Slot")
	TEnumAsByte<EVerticalAlignment> VerticalAlignment;

public:

	UFUNCTION(BlueprintCallable, Category="Layout|Horizontal Box Slot")
	void SetPadding(FMargin InPadding);

	UFUNCTION(BlueprintCallable, Category="Layout|Horizontal Box Slot")
	void SetSize(FSlateChildSize InSize);

	UFUNCTION(BlueprintCallable, Category="Layout|Horizontal Box Slot")
	void SetHorizontalAlignment(EHorizontalAlignment InHorizontalAlignment);

	UFUNCTION(BlueprintCallable, Category="Layout|Horizontal Box Slot")
	void SetVerticalAlignment(EVerticalAlignment InVerticalAlignment);

	void BuildSlot(TSharedRef<SHorizontalBox> HorizontalBox);

	// UPanelSlot interface
	virtual void SynchronizeProperties() override;
	// End of UPanelSlot interface

	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

private:
	SHorizontalBox::FSlot* Slot;
};
