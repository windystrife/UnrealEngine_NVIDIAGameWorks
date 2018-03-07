// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Layout/Margin.h"
#include "Widgets/SWidget.h"
#include "Components/PanelWidget.h"
#include "UniformGridPanel.generated.h"

class SUniformGridPanel;
class UUniformGridSlot;

/**
 * A panel that evenly divides up available space between all of its children.
 */
UCLASS()
class UMG_API UUniformGridPanel : public UPanelWidget
{
	GENERATED_UCLASS_BODY()

public:

	/** Padding given to each slot */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Child Layout")
	FMargin SlotPadding;

	/** The minimum desired width of the slots */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Child Layout")
	float MinDesiredSlotWidth;

	/** The minimum desired height of the slots */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Child Layout")
	float MinDesiredSlotHeight;

public:

	/**  */
	UFUNCTION(BlueprintCallable, Category="Child Layout")
	void SetSlotPadding(FMargin InSlotPadding);

	/**  */
	UFUNCTION(BlueprintCallable, Category="Child Layout")
	void SetMinDesiredSlotWidth(float InMinDesiredSlotWidth);

	/**  */
	UFUNCTION(BlueprintCallable, Category="Child Layout")
	void SetMinDesiredSlotHeight(float InMinDesiredSlotHeight);

public:

	/**  */
	UFUNCTION(BlueprintCallable, Category="Widget")
	UUniformGridSlot* AddChildToUniformGrid(UWidget* Content);

public:

	// UWidget interface
	virtual void SynchronizeProperties() override;
	// End of UWidget interface

	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

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

	TSharedPtr<SUniformGridPanel> MyUniformGridPanel;

protected:
	// UWidget interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	// End of UWidget interface
};
