// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "UI/SynthSlateStyle.h"

#include "UObject/ObjectMacros.h"
#include "Styling/SlateTypes.h"
#include "Widgets/SWidget.h"
#include "Components/Widget.h"
#include "Styling/SlateWidgetStyle.h"
#include "Brushes/SlateImageBrush.h"
#include "Synth2DSliderStyle.generated.h"

/**
* Represents the appearance of an SSynth2DSlider
*/
USTRUCT(BlueprintType)
struct SYNTHESIS_API FSynth2DSliderStyle : public FSlateWidgetStyle
{
	GENERATED_USTRUCT_BODY()

	FSynth2DSliderStyle();

	virtual ~FSynth2DSliderStyle();

	// Image to use for the 2D handle
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FSlateBrush NormalThumbImage;

		// Image to use for the 2D handle
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FSlateBrush DisabledThumbImage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FSlateBrush NormalBarImage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FSlateBrush DisabledBarImage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FSlateBrush BackgroundImage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	float BarThickness;

	static const FName TypeName;
	virtual const FName GetTypeName() const override { return TypeName; };

	virtual void GetResources(TArray< const FSlateBrush* >& OutBrushes) const override;

	static const FSynth2DSliderStyle& GetDefault();

};

