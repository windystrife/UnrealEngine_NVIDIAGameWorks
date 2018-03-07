// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "GameFramework/Actor.h"
#include "Engine/EngineBaseTypes.h"
#include "VREditorTeleporter.generated.h"

class UViewportInteractor;
class UVREditorMode;

/**
 * VR Editor teleport manager and the visual representation of the teleport
 */
UCLASS()
class AVREditorTeleporter: public AActor
{
	GENERATED_BODY()

public:

	/** Default constructor that sets up CDO properties */
	AVREditorTeleporter();

	/** Initializes the teleporter */
	void Init(class UVREditorMode* InMode);

	/** Shuts down the teleporter */
	void Shutdown();

	/** Whether we are currently aiming to teleport. */
	bool IsAiming() const;

	bool IsTeleporting() const;


private:

	//~ Begin AActor interface
	virtual void Tick(const float DeltaTime) override;
	virtual bool IsEditorOnly() const final
	{
		return true;
	}
	//~ End AActor interface

	/** Start teleporting, does a ray trace with the hand passed and calculates the locations for lerp movement in Teleport */
	void StartTeleport(class UViewportInteractor* Interactor);

	/** Called when the user presses a button on their motion controller device */
	void OnPreviewInputAction(class FEditorViewportClient& ViewportClient, UViewportInteractor* Interactor,
		const struct FViewportActionKeyInput& Action, bool& bOutIsInputCaptured, bool& bWasHandled);

	/** Move the roomspace using lerp towards the new location */
	void Teleport(const float DeltaTime);

	/** Updating aiming with teleport to end of laser and pulling and pushing */
	void UpdateTeleportAim(const float DeltaTime);

	/** Helper function to push and pull the teleporter along the laser */
	FVector UpdatePushPullTeleporter(class UVREditorMotionControllerInteractor* VREditorInteractor, const FVector& LaserPointerStart, const FVector& LaserPointerEnd, const bool bEnablePushPull = true);

	/** Hide or show the teleporter visuals */
	void SetVisibility(const bool bVisible);

	/** Sets the color for the teleporter visuals */
	void SetColor(const FLinearColor& Color);

	/** Figures out the new transforms for all the visuals based on the new location and the transforms of the HMD and motioncontrollers */
	void UpdateVisuals(const FVector& NewLocation);

	/** If we want to start showing or hiding the meshes */
	void Show(const bool bShow);

	void UpdateFadingState(const float DeltaTime);

	/** Calculated the scale for animation */
	float CalculateAnimatedScaleFactor() const;

	/** Get slide delta to push/pull or scale the teleporter */
	float GetSlideDelta(UVREditorMotionControllerInteractor* Interactor, const bool Axis);
	
	/** The owning VR mode */
	UPROPERTY()
	UVREditorMode* VRMode;

	enum EState
	{
		None = 0,				// When not aiming for a teleport
		Aiming = 1,	// Aiming at the end of the laser, user can also scale using touchpad/analog stick
		Teleporting = 3			// Currently teleporting from one location to another with lerp
	};

	/** The current teleport state */
	EState TeleportingState;

	/** The current lerp of the teleport between the TeleportStartLocation and the TeleportGoalLocation */
	float TeleportLerpAlpha;

	/** Set on the current Roomspace location in the world in StartTeleport before doing the actual teleporting */
	FVector TeleportStartLocation;

	/** The calculated goal location in StartTeleport to move the Roomspace to */
	FVector TeleportGoalLocation;

	/** Visuals for the feet location of the teleporter with the same direction of the HMD yaw */
	UPROPERTY()
	UStaticMeshComponent* TeleportDirectionMeshComponent;

	/** Visuals for teleport HMD */
	UPROPERTY()
	UStaticMeshComponent* HMDMeshComponent;

	/** Visuals for teleport left motion controller */
	UPROPERTY()
	UStaticMeshComponent* LeftMotionControllerMeshComponent;

	/** Visuals for teleport right motion controller */
	UPROPERTY()
	UStaticMeshComponent* RightMotionControllerMeshComponent;
	
	/** Dynamic material for teleport visuals */
	UPROPERTY()
	UMaterialInstanceDynamic* TeleportMID;

	/** The interactor that started aiming to teleport */
	UPROPERTY()
	UViewportInteractor* InteractorTryingTeleport;

	/** When offset between the hoverlocation of the laser and the calculated  teleport*/
	FVector OffsetDistance;

	/** The goal world to meters scale. This value is used to scale the visuals and will be used to set world to meters scale after teleporting */
	float TeleportGoalScale;

	/** The current length of the laser where the teleport should be at */
	float DragRayLength;
	
	/** The current drag velocity to push or pull the teleport along the laser */
	float DragRayLengthVelocity;

	/** If the teleporter has been pushed by the trackpad */
	bool bPushedFromEndOfLaser;

	/** Is this the first time aiming for teleporting, we don't have to smooth movement for initial tick */
	bool bInitialTeleportAim;

	/** Fade alpha, for visibility transitions */
	float FadeAlpha;

	/** If the teleporter should fade in */
	TOptional<bool> bShouldBeVisible;

	/** Delay to start the actual moving to the end location */
	uint32 TeleportTickDelay;
};