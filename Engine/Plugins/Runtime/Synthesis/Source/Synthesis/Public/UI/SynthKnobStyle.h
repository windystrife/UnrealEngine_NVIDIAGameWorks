// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "UObject/ObjectMacros.h"
#include "Styling/SlateTypes.h"
#include "Widgets/SWidget.h"
#include "Components/Widget.h"
#include "Styling/SlateWidgetStyle.h"
#include "Brushes/SlateImageBrush.h"
#include "SynthKnobStyle.generated.h"

UENUM(BlueprintType)
enum class ESynthKnobSize : uint8
{
	Medium,
	Large,
	Count UMETA(Hidden)
};

/**
 * Represents the appearance of an SSynthKnob
 */
USTRUCT(BlueprintType)
struct SYNTHESIS_API FSynthKnobStyle : public FSlateWidgetStyle
{
	GENERATED_USTRUCT_BODY()

	FSynthKnobStyle();

	virtual ~FSynthKnobStyle();

	static const FName TypeName;
	virtual const FName GetTypeName() const override { return TypeName; };

	virtual void GetResources(TArray< const FSlateBrush* >& OutBrushes) const override;

	static const FSynthKnobStyle& GetDefault();

	// Returns the base brush to use
	const FSlateBrush* GetBaseBrush() const;

	// Returns the overlay brush to represent the given value
	const FSlateBrush* GetOverlayBrush() const;

	// Image to use for the large knob
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FSlateBrush LargeKnob;

	// Image to use for the dot handle
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FSlateBrush LargeKnobOverlay;

	// Image to use for the medium large knob
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FSlateBrush MediumKnob;

	// Image to use for the mediaum knob dot handle
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FSlateBrush MediumKnobOverlay;

	// Image to use for the mediaum knob dot handle
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	float MinValueAngle;

	// Image to use for the mediaum knob dot handle
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	float MaxValueAngle;

	/** The size of the knobs to use. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Appearance)
	ESynthKnobSize KnobSize;
	FSynthKnobStyle& SetKnobSize(const ESynthKnobSize& InKnobSize){ KnobSize = InKnobSize; return *this; }

};

