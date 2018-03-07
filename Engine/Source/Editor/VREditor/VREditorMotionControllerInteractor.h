// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "InputCoreTypes.h"
#include "HeadMountedDisplayTypes.h"
#include "VREditorInteractor.h"
#include "VREditorMotionControllerInteractor.generated.h"

class AActor;
class UStaticMesh;
class UStaticMeshSocket;

/**
 * Represents the interactor in the world
 */
UCLASS()
class UVREditorMotionControllerInteractor : public UVREditorInteractor
{
	GENERATED_BODY()

public:

	/** Default constructor */
	UVREditorMotionControllerInteractor();
	
	// UVREditorInteractor overrides
	virtual void Init( class UVREditorMode* InVRMode ) override;
	/** Gets the trackpad slide delta */
	virtual float GetSlideDelta() override;

	/** Sets up all components */
	void SetupComponent( AActor* OwningActor );

	/** Sets the EControllerHand for this motioncontroller */
	void SetControllerHandSide( const EControllerHand InControllerHandSide );

	// IViewportInteractorInterface overrides
	virtual void Shutdown() override;
	virtual void Tick( const float DeltaTime ) override;
	virtual void CalculateDragRay( float& InOutDragRayLength, float& InOutDragRayVelocity ) override;

	/** @return Returns the type of HMD we're dealing with */
	EHMDDeviceType::Type GetHMDDeviceType() const;

	// UViewportInteractor
	virtual void PreviewInputKey( class FEditorViewportClient& ViewportClient, FViewportActionKeyInput& Action, const FKey Key, const EInputEvent Event, bool& bOutWasHandled ) override;
	virtual void HandleInputKey( class FEditorViewportClient& ViewportClient, FViewportActionKeyInput& Action, const FKey Key, const EInputEvent Event, bool& bOutWasHandled ) override;
	virtual bool GetTransformAndForwardVector( FTransform& OutHandTransform, FVector& OutForwardVector ) const override;
	virtual bool GetIsLaserBlocked() const override;

	/** 
	 * Gets the trackpad delta of the axis passed. 
	 * 
	 * @param Axis	The axis of which we want the slide delta. 0 is X axis and 1 is Y axis.Default is axis Y
	 */
	float GetTrackpadSlideDelta( const bool Axis = 1 );

	/** Starts haptic feedback for physical motion controller */
	virtual void PlayHapticEffect( const float Strength ) override;
	
	/** Get the side of the controller */
	EControllerHand GetControllerSide() const;

	/** Get the motioncontroller component of this interactor */
	class UMotionControllerComponent* GetMotionControllerComponent() const;
	
	/** Resets all the trackpad related values to default. */
	void ResetTrackpad();

	/** Check if the touchpad is currently touched */
	bool IsTouchingTrackpad() const;

	/** Get the current position of the trackpad or analog stick */
	FVector2D GetTrackpadPosition() const;

	/** Get the last position of the trackpad or analog stick */
	FVector2D GetLastTrackpadPosition() const;

	/** If the trackpad values are valid */
	bool IsTrackpadPositionValid( const int32 AxisIndex ) const;

	/** Get when the last time the trackpad position was updated */
	FTimespan& GetLastTrackpadPositionUpdateTime();

	/** Get when the last time the trackpad position was updated */
	FTimespan& GetLastActiveTrackpadUpdateTime();

	/** Set if we want to force to show the laser*/
	void SetForceShowLaser(const bool bInForceShow);

	/** Next frame this will be used as color for the laser */
	void SetForceLaserColor(const FLinearColor& InColor);

	/** Toggles whether or not this controller is being used to scrub sequencer */
	void ToggleSequencerScrubbingMode()
	{
		bIsScrubbingSequence = !bIsScrubbingSequence;
	};

	/** Returns whether or not this controller is being used to scrub sequencer */
	bool IsScrubbingSequencer()
	{
		return bIsScrubbingSequence;
	}

protected:

	// ViewportInteractor
	virtual void HandleInputAxis( class FEditorViewportClient& ViewportClient, FViewportActionKeyInput& Action, const FKey Key, const float Delta, const float DeltaTime, bool& bOutWasHandled ) override;

	/** Polls input for the motion controllers transforms */
	virtual void PollInput() override;

private:

	/** Changes the color of the buttons on the handmesh */
	void ApplyButtonPressColors( const FViewportActionKeyInput& Action );

	/** Set the visuals for a button on the motion controller */
	void SetMotionControllerButtonPressedVisuals( const EInputEvent Event, const FName& ParameterName, const float PressStrength );

	/** Pops up some help text labels for the controller in the specified hand, or hides it, if requested */
	void ShowHelpForHand( const bool bShowIt );

	/** Called every frame to update the position of any floating help labels */
	void UpdateHelpLabels();

	/** Given a mesh and a key name, tries to find a socket on the mesh that matches a supported key */
	UStaticMeshSocket* FindMeshSocketForKey( UStaticMesh* StaticMesh, const FKey Key );

	/** Updates all the segments of the curved laser */
	void UpdateSplineLaser(const FVector& InStartLocation, const FVector& InEndLocation, const FVector& InForward);
	
	/** Sets the visibility on all curved laser segments */
	void SetLaserVisibility(const bool bVisible);

	/** Sets the visuals of the LaserPointer */
	void SetLaserVisuals( const FLinearColor& NewColor, const float CrawlFade, const float CrawlSpeed );

	/** Updates the radial menu */
	void UpdateRadialMenuInput( const float DeltaTime );

	/** Directions the trackpad can be swiped to */
	enum ETouchSwipeDirection
	{
		None = 0,
		Left = 1,
		Right = 2,
		Up = 3,
		Down = 4
	};

	/** Start undo or redo from swipe for the Vive */
	void UndoRedoFromSwipe(const ETouchSwipeDirection InSwipeDirection);

protected:
	
	/** Motion controller component which handles late-frame transform updates of all parented sub-components */
	UPROPERTY()
	class UMotionControllerComponent* MotionControllerComponent;

	//
	// Graphics
	//

	/** Mesh for this hand */
	UPROPERTY()
	class UStaticMeshComponent* HandMeshComponent;

	/** Spline for this hand's laser pointer */
	UPROPERTY()
	class USplineComponent* LaserSplineComponent;

	/** Spline meshes for curved laser */
	UPROPERTY()
	TArray<class USplineMeshComponent*> LaserSplineMeshComponents;

	/** MID for laser pointer material (opaque parts) */
	UPROPERTY()
	class UMaterialInstanceDynamic* LaserPointerMID;

	/** MID for laser pointer material (translucent parts) */
	UPROPERTY()
	class UMaterialInstanceDynamic* TranslucentLaserPointerMID;

	/** Hover impact indicator mesh */
	UPROPERTY()
	class UStaticMeshComponent* HoverMeshComponent;

	/** Hover point light */
	UPROPERTY()
	class UPointLightComponent* HoverPointLightComponent;

	/** MID for hand mesh */
	UPROPERTY()
	class UMaterialInstanceDynamic* HandMeshMID;

	/** Right or left hand */
	EControllerHand ControllerHandSide;

	/** True if this hand has a motion controller (or both!) */
	bool bHaveMotionController;

	// Special key action names for motion controllers
	static const FName TrackpadPositionX;
	static const FName TrackpadPositionY;
	static const FName TriggerAxis;
	static const FName MotionController_Left_FullyPressedTriggerAxis;
	static const FName MotionController_Right_FullyPressedTriggerAxis;
	static const FName MotionController_Left_PressedTriggerAxis;
	static const FName MotionController_Right_PressedTriggerAxis;

private:

	//
	// Trigger axis state
	//

	/** True if trigger is fully pressed right now (or within some small threshold) */
	bool bIsTriggerFullyPressed;

	/** True if the trigger is currently pulled far enough that we consider it in a "half pressed" state */
	bool bIsTriggerPressed;

	/** True if trigger has been fully released since the last press */
	bool bHasTriggerBeenReleasedSinceLastPress;

	//
	// Trackpad support
	//

	/** True if the trackpad is actively being touched */
	bool bIsTouchingTrackpad;

	/** True if pressing trackpad button (or analog stick button is down) */
	bool bIsPressingTrackpad;

	/** Position of the touched trackpad */
	FVector2D TrackpadPosition;

	/** Last position of the touched trackpad */
	FVector2D LastTrackpadPosition;

	/** True if we have a valid trackpad position (for each axis) */
	bool bIsTrackpadPositionValid[ 2 ];

	/** Real time that the last trackpad position was last updated.  Used to filter out stale previous data. */
	FTimespan LastTrackpadPositionUpdateTime;

	/** Real time that the last trackpad position was over the dead zone threshold. */
	FTimespan LastActiveTrackpadUpdateTime;

	/** Forcing to show laser */
	bool bForceShowLaser;

	/** The color that will be used for one frame */
	TOptional<FLinearColor> ForceLaserColor;

	/**Whether a flick action was executed */
	bool bFlickActionExecuted;

	/** whether or not this controller is being used to scrub sequencer */
	bool bIsScrubbingSequence;

	//
	// Swipe
	//

	/** Latest swipe direction on the trackpad */
	ETouchSwipeDirection LastSwipe;

	/** Initial position when starting to touch the trackpad */
	FVector2D InitialTouchPosition;
};
