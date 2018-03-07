// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SlateFwd.h"
#include "UObject/ObjectMacros.h"
#include "Layout/Margin.h"
#include "Styling/SlateColor.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"
#include "Widgets/SWidget.h"
#include "Components/Widget.h"
#include "Components/NamedSlotInterface.h"
#include "ExpandableArea.generated.h"

class UExpandableArea;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnExpandableAreaExpansionChanged, UExpandableArea*, Area, bool, bIsExpanded);

/**
 * 
 */
UCLASS()
class UMG_API UExpandableArea : public UWidget, public INamedSlotInterface
{
	GENERATED_UCLASS_BODY()

public:

	UPROPERTY(EditAnywhere, Category = "Style")
	FExpandableAreaStyle Style;

	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Style" )
	FSlateBrush BorderBrush;

	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Style" )
	FSlateColor BorderColor;

	/**  */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Expansion")
	bool bIsExpanded;

	/** The maximum height of the area */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Expansion")
	float MaxHeight;
	
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Expansion" )
	FMargin HeaderPadding;
	
	/**  */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Expansion")
	FMargin AreaPadding;

	/** A bindable delegate for the IsChecked. */
	UPROPERTY(BlueprintAssignable, Category="ExpandableArea|Event")
	FOnExpandableAreaExpansionChanged OnExpansionChanged;

public:

	UFUNCTION(BlueprintCallable, Category="Expansion")
	bool GetIsExpanded() const;

	UFUNCTION(BlueprintCallable, Category="Expansion")
	void SetIsExpanded(bool IsExpanded);

	UFUNCTION(BlueprintCallable, Category = "Expansion")
	void SetIsExpanded_Animated(bool IsExpanded);
	
	// Begin INamedSlotInterface
	virtual void GetSlotNames(TArray<FName>& SlotNames) const override;
	virtual UWidget* GetContentForSlot(FName SlotName) const override;
	virtual void SetContentForSlot(FName SlotName, UWidget* Content) override;
	// End INamedSlotInterface

public:
	
	// UWidget interface
	virtual void SynchronizeProperties() override;
	// End of UWidget interface

	// UVisual interface
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	// End of UVisual interface

#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override;
	virtual void OnDescendantSelectedByDesigner(UWidget* DescendantWidget) override;
	virtual void OnDescendantDeselectedByDesigner(UWidget* DescendantWidget) override;
#endif

protected:
	// UWidget interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	// End of UWidget interface

	void SlateExpansionChanged(bool NewState);

protected:
	UPROPERTY()
	UWidget* HeaderContent;

	UPROPERTY()
	UWidget* BodyContent;

	TSharedPtr<SExpandableArea> MyExpandableArea;
};
