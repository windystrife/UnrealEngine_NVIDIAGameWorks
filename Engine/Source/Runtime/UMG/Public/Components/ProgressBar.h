// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Styling/SlateTypes.h"
#include "Widgets/SWidget.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Components/Widget.h"
#include "ProgressBar.generated.h"

class USlateBrushAsset;

/**
 * The progress bar widget is a simple bar that fills up that can be restyled to fit any number of uses.
 *
 * * No Children
 */
UCLASS()
class UMG_API UProgressBar : public UWidget
{
	GENERATED_UCLASS_BODY()
	
public:

	/** The progress bar style */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Style", meta=( DisplayName="Style" ))
	FProgressBarStyle WidgetStyle;

	/** Style used for the progress bar */
	UPROPERTY()
	USlateWidgetStyleAsset* Style_DEPRECATED;

	/** The brush to use as the background of the progress bar */
	UPROPERTY()
	USlateBrushAsset* BackgroundImage_DEPRECATED;
	
	/** The brush to use as the fill image */
	UPROPERTY()
	USlateBrushAsset* FillImage_DEPRECATED;
	
	/** The brush to use as the marquee image */
	UPROPERTY()
	USlateBrushAsset* MarqueeImage_DEPRECATED;

	/** Used to determine the fill position of the progress bar ranging 0..1 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Progress, meta=( UIMin = "0", UIMax = "1" ))
	float Percent;

	/** Defines if this progress bar fills Left to right or right to left */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Progress)
	TEnumAsByte<EProgressBarFillType::Type> BarFillType;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Progress)
	bool bIsMarquee;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Progress)
	FVector2D BorderPadding;

	/** A bindable delegate to allow logic to drive the text of the widget */
	UPROPERTY()
	FGetFloat PercentDelegate;

	/** Fill Color and Opacity */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Appearance)
	FLinearColor FillColorAndOpacity;

	/** */
	UPROPERTY()
	FGetLinearColor FillColorAndOpacityDelegate;

public:
	
	/** Sets the current value of the ProgressBar. */
	UFUNCTION(BlueprintCallable, Category="Progress")
	void SetPercent(float InPercent);

	/** Sets the fill color of the progress bar. */
	UFUNCTION(BlueprintCallable, Category="Progress")
	void SetFillColorAndOpacity(FLinearColor InColor);

	/** Sets the progress bar to show as a marquee. */
	UFUNCTION(BlueprintCallable, Category="Progress")
	void SetIsMarquee(bool InbIsMarquee);

	//TODO UMG Add Set BarFillType.

public:
	
	//~ Begin UWidget Interface
	virtual void SynchronizeProperties() override;
	//~ End UWidget Interface

	//~ Begin UVisual Interface
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	//~ End UVisual Interface

	//~ Begin UObject Interface
	virtual void PostLoad() override;
	//~ End UObject Interface

#if WITH_EDITOR
	//~ Begin UWidget Interface
	virtual const FText GetPaletteCategory() override;
	virtual void OnCreationFromPalette() override;
	//~ End UWidget Interface
#endif

protected:
	/** Native Slate Widget */
	TSharedPtr<SProgressBar> MyProgressBar;

	//~ Begin UWidget Interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	//~ End UWidget Interface

	PROPERTY_BINDING_IMPLEMENTATION(FSlateColor, FillColorAndOpacity);
};
