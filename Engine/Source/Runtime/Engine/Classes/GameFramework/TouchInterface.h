// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SlateFwd.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "InputCoreTypes.h"
#include "TouchInterface.generated.h"

class UTexture2D;

USTRUCT()
struct FTouchInputControl
{
	GENERATED_USTRUCT_BODY()

	// basically mirroring SVirtualJoystick::FControlInfo but as an editable class
	UPROPERTY(EditAnywhere, Category="Control", meta=(ToolTip="For sticks, this is the Thumb"))
	UTexture2D* Image1;
	UPROPERTY(EditAnywhere, Category="Control", meta=(ToolTip="For sticks, this is the Background"))
	UTexture2D* Image2;
	UPROPERTY(EditAnywhere, Category="Control", meta=(ToolTip="The center point of the control (if <= 1.0, it's relative to screen, > 1.0 is absolute)"))
	FVector2D Center;
	UPROPERTY(EditAnywhere, Category="Control", meta=(ToolTip="The size of the control (if <= 1.0, it's relative to screen, > 1.0 is absolute)"))
	FVector2D VisualSize;
	UPROPERTY(EditAnywhere, Category="Control", meta=(ToolTip="For sticks, the size of the thumb (if <= 1.0, it's relative to screen, > 1.0 is absolute)"))
	FVector2D ThumbSize;
	UPROPERTY(EditAnywhere, Category="Control", meta=(ToolTip="The interactive size of the control (if <= 1.0, it's relative to screen, > 1.0 is absolute)"))
	FVector2D InteractionSize;
	UPROPERTY(EditAnywhere, Category = "Control", meta = (ToolTip = "The scale for control input"))
	FVector2D InputScale;
	UPROPERTY(EditAnywhere, Category="Control", meta=(ToolTip="The main input to send from this control (for sticks, this is the horizontal axis)"))
	FKey MainInputKey;
	UPROPERTY(EditAnywhere, Category="Control", meta=(ToolTip="The alternate input to send from this control (for sticks, this is the vertical axis)"))
	FKey AltInputKey;

	FTouchInputControl()
		: InputScale(1.f, 1.f)
	{
	}
};


/**
 * Defines an interface by which touch input can be controlled using any number of buttons and virtual joysticks
 */
UCLASS(Blueprintable, BlueprintType)
class ENGINE_API UTouchInterface : public UObject
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category="TouchInterface")
	TArray<FTouchInputControl> Controls;

	UPROPERTY(EditAnywhere, Category="TouchInterface", meta=(ToolTip="Opacity (0.0 - 1.0) of all controls while any control is active"))
	float ActiveOpacity;

	UPROPERTY(EditAnywhere, Category="TouchInterface", meta=(ToolTip="Opacity (0.0 - 1.0) of all controls while no controls are active"))
	float InactiveOpacity;

	UPROPERTY(EditAnywhere, Category="TouchInterface", meta=(ToolTip="How long after user interaction will all controls fade out to Inactive Opacity"))
	float TimeUntilDeactive;

	UPROPERTY(EditAnywhere, Category="TouchInterface", meta=(ToolTip="How long after going inactive will controls reset/recenter themselves (0.0 will disable this feature)"))
	float TimeUntilReset;

	UPROPERTY(EditAnywhere, Category="TouchInterface", meta=(ToolTip="How long after joystick enabled for touch (0.0 will disable this feature)"))
	float ActivationDelay;

	UPROPERTY(EditAnywhere, Category="TouchInterface", meta=(ToolTip="Whether to prevent joystick re-center"))
	bool bPreventRecenter;

	UPROPERTY(EditAnywhere, Category = "TouchInterface", meta = (ToolTip = "Delay at startup before virtual joystick is drawn"))
	float StartupDelay;

	/** Make this the active set of touch controls */
	void Activate(TSharedPtr<SVirtualJoystick> VirtualJoystick);
};
