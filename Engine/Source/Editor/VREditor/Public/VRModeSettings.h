// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "VRModeSettings.generated.h"

UENUM()
enum class EInteractorHand : uint8
{
	/** Right hand */
	Right,

	/** Left hand */
	Left,
};

/**
* Implements the settings for VR Mode.
*/
UCLASS(config = EditorSettings)
class VREDITOR_API UVRModeSettings : public UObject
{
	GENERATED_BODY()

public:

	/** Default constructor that sets up CDO properties */
	UVRModeSettings();

	/**If true, wearing a Vive or Oculus Rift headset will automatically enter VR Editing mode */
	UPROPERTY(EditAnywhere, config, Category = "General", meta = (DisplayName = "Enable VR Mode Auto-Entry"))
	uint32 bEnableAutoVREditMode : 1;

	// Which hand should have the primary interactor laser on it
	UPROPERTY(EditAnywhere, config, Category = "General")
	EInteractorHand InteractorHand;

	/** Show the movement grid for orientation while moving through the world */
	UPROPERTY(EditAnywhere, config, Category = "UI Customization")
	uint32 bShowWorldMovementGrid : 1;

	/** Dim the surroundings while moving through the world */
	UPROPERTY(EditAnywhere, config, Category = "UI Customization")
	uint32 bShowWorldMovementPostProcess : 1;

	/** Display a progress bar while scaling that shows your current scale */
	UPROPERTY(EditAnywhere, config, Category = "UI Customization")
	uint32 bShowWorldScaleProgressBar : 1;

	/** Adjusts the brightness of the UI panels */
	UPROPERTY(EditAnywhere, config, Category = "UI Customization", meta = (DisplayName = "UI Panel Brightness", ClampMin = 0.01, ClampMax = 10.0))
	float UIBrightness;

	/** The size of the transform gizmo */
	UPROPERTY(EditAnywhere, config, Category = "UI Customization", meta = (ClampMin = 0.1, ClampMax = 2.0))
	float GizmoScale;

	/** The maximum time in seconds between two clicks for a double-click to register */
	UPROPERTY(EditAnywhere, config, Category = "Motion Controllers", meta = (ClampMin = 0.01, ClampMax = 1.0))
	float DoubleClickTime;

	/** The amount (between 0-1) you have to depress the Vive controller trigger to register a press */
	UPROPERTY(EditAnywhere, config, Category = "Motion Controllers", meta = (DisplayName = "Trigger Pressed Threshold (Vive)", ClampMin = 0.01, ClampMax = 1.0))
	float TriggerPressedThreshold_Vive;

	/** The amount (between 0-1) you have to depress the Oculus Touch controller trigger to register a press */
	UPROPERTY(EditAnywhere, config, Category = "Motion Controllers", meta = (DisplayName = "Trigger Pressed Threshold (Oculus Touch)", ClampMin = 0.01, ClampMax = 1.0))
	float TriggerPressedThreshold_Rift;

private:
#if WITH_EDITOR
	void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent);
#endif
};
