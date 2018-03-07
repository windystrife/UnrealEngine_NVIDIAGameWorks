// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "InputCoreTypes.h"
#include "Engine/EngineTypes.h"
#include "ViewportInteractionTypes.h"
#include "Editor/ViewportInteraction/ViewportInteractorData.h"
#include "ViewportInteractionUtils.h"
#include "ViewportInteractor.generated.h"

class AActor;

/**
 * Represents the interactor in the world
 */
UCLASS( ABSTRACT )
class VIEWPORTINTERACTION_API UViewportInteractor : public UObject
{
	GENERATED_BODY()

public:

	/** Default constructor */
	UViewportInteractor();

	/** Gets the private data for this interactor */
	struct FViewportInteractorData& GetInteractorData();

	/** Gets the private data for this interactor (const) */
	const struct FViewportInteractorData& GetInteractorData() const;

	/** Sets the world interaction */
	void SetWorldInteraction( class UViewportWorldInteraction* InWorldInteraction );

	/** Sets the other interactor */
	void SetOtherInteractor( UViewportInteractor* InOtherInteractor ); //@todo ViewportInteraction: This should not be public to other modules and should only be called from the world interaction.

	/** Removes the other interactor reference for this interactor */
	void RemoveOtherInteractor();

	/** Gets the paired interactor assigned by the world interaction, can return null when there is no other interactor */
	UViewportInteractor* GetOtherInteractor() const;

	/** Whenever the ViewportWorldInteraction is shut down, the interacts will shut down as well */
	virtual void Shutdown();

	/** Update for this interactor called by the ViewportWorldInteraction */
	virtual void Tick( const float DeltaTime );

	/** Gets the last component hovered on by the interactor laser. */
	virtual class UActorComponent* GetLastHoverComponent();

	/** Adds a new action to the KeyToActionMap */
	void AddKeyAction( const FKey& Key, const FViewportActionKeyInput& Action );

	/** Removes an action from the KeyToActionMap */
	void RemoveKeyAction( const FKey& Key );

	/** Base classes need to implement getting the input for the input devices for that interactor */
	virtual void PollInput() {}

	/** Handles axis input and translates it actions */
	bool HandleInputKey( class FEditorViewportClient& ViewportClient, const FKey Key, const EInputEvent Event );

	/** Handles axis input and translates it to actions */
	bool HandleInputAxis( class FEditorViewportClient& ViewportClient, const FKey Key, const float Delta, const float DeltaTime );

	/** @return	Returns true if this interactor's designated 'modifier button' is currently held down.  Some interactors may not support this */
	virtual bool IsModifierPressed() const
	{
		return false;
	}

	/** Gets the world transform of this interactor */
	FTransform GetTransform() const;

	/** Gets the current interactor data dragging mode */
	EViewportInteractionDraggingMode GetDraggingMode() const;

	/** Gets the interactor data previous dragging mode */
	EViewportInteractionDraggingMode GetLastDraggingMode() const;

	/** Gets the interactor data drag velocity */
	FVector GetDragTranslationVelocity() const;

	/** Sets the hover location */
	void SetHoverLocation(const FVector& InHoverLocation);

	/**
	 * Gets the start and end point of the laser pointer for the specified hand
	 *
	 * @param HandIndex				Index of the hand to use
	 * @param LasertPointerStart	(Out) The start location of the laser pointer in world space
	 * @param LasertPointerEnd		(Out) The end location of the laser pointer in world space
	 * @param bEvenIfBlocked		If true, returns a laser pointer even if the hand has UI in front of it (defaults to false)
	 * @param LaserLengthOverride	If zero the default laser length (VREdMode::GetLaserLength) is used
	 *
	 * @return	True if we have motion controller data for this hand and could return a valid result
	 */
	bool GetLaserPointer( FVector& LaserPointerStart, FVector& LaserPointerEnd, const bool bEvenIfBlocked = false, const float LaserLengthOverride = 0.0f );

	/**
	 * Gets a sphere on this interactor that can be used to interact with objects in close proximity
	 *
	 * @param	OutGrabberSphere	The sphere in world space
	 * @param	bEvenIfBlocked	When set to true, a valid sphere may be returned even if there is UI attached in front of this interactor
	 *
	 * @return	True if the sphere is available and valid, or false is the interactor was busy or we could not determine a valid position for the interactor
	 */
	bool GetGrabberSphere( FSphere& OutGrabberSphere, const bool bEvenIfBlocked = false );

	/** Gets the maximum length of a laser pointer */
	float GetLaserPointerMaxLength() const;

	/** Triggers a force feedback effect on device if possible */
	virtual void PlayHapticEffect( const float Strength ) {};

	/**
	 * Traces along the laser pointer vector and returns what it first hits in the world
	 *
	 * @param OptionalListOfIgnoredActors Actors to exclude from hit testing
	 * @param bIgnoreGizmos True if no gizmo results should be returned, otherwise they are preferred (x-ray)
	 * @param bEvenIfUIIsInFront If true, ignores any UI that might be blocking the ray
	 * @param LaserLengthOverride If zero the default laser length (VREdMode::GetLaserLength) is used
	 *
	 * @return What the laster pointer hit
	 */
	virtual FHitResult GetHitResultFromLaserPointer( TArray<AActor*>* OptionalListOfIgnoredActors = nullptr, const bool bIgnoreGizmos = false,
		TArray<UClass*>* ObjectsInFrontOfGizmo = nullptr, const bool bEvenIfBlocked = false, const float LaserLengthOverride = 0.0f );

	/** Reset the values before checking the hover actions */
	virtual void ResetHoverState();

	/** Needs to be implemented by the base to calculate drag ray length and the velocity for the ray */
	virtual void CalculateDragRay( float& InOutDragRayLength, float& InOutDragRayVelocity ) {};

	/**
	 * Creates a hand transform and forward vector for a laser pointer for a given hand
	 *
	 * @param HandIndex			Index of the hand to use
	 * @param OutHandTransform	The created hand transform
	 * @param OutForwardVector	The forward vector of the hand
	 *
	 * @return	True if we have motion controller data for this hand and could return a valid result
	 */
	virtual bool GetTransformAndForwardVector( FTransform& OutHandTransform, FVector& OutForwardVector ) const;

	/** Called by StartDragging in world interaction to give the interactor a chance of acting upon starting a drag operation */
	virtual void OnStartDragging( const FVector& HitLocation, const bool bIsPlacingNewObjects );

	/** Gets the interactor laser hover location */
	FVector GetHoverLocation();

	/** If the interactor laser is currently hovering */
	bool IsHovering() const;

	/** If the interactor laser is currently hovering over a gizmo handle */
	bool IsHoveringOverGizmo();

	/** Sets the current dragging mode for this interactor */
	void SetDraggingMode( const EViewportInteractionDraggingMode NewDraggingMode );

	/** To be overridden by base class. Called by GetLaserPointer to give the derived interactor a chance to disable the laser. By default it is not blocked */
	virtual bool GetIsLaserBlocked() const { return false; }

	/** Gets a certain action by iterating through the map looking for the same ActionType */
	// @todo ViewportInteractor: This should be changed to return a const pointer, but we need to fix up some dragging code in WorldInteraction first
	FViewportActionKeyInput* GetActionWithName( const FName InActionName );

	/** Gets the drag haptic feedback strength console variable */
	float GetDragHapticFeedbackStrength() const;

	/** If this interactor is hovering over a type that has priority from GetHitResultFromLaserPointer */
	bool IsHoveringOverPriorityType() const;

	/** Returns true if currently hovering over selected actor */
	bool IsHoveringOverSelectedActor() const;

	/** Reset the stored laser end location at the end of tick */
	void ResetLaserEnd();

protected:

	/** To be overridden. Called by HandleInputKey before delegates and default input implementation */
	virtual void PreviewInputKey( class FEditorViewportClient& ViewportClient, FViewportActionKeyInput& Action, const FKey Key, const EInputEvent Event, bool& bOutWasHandled ) {};

	/** To be overridden. Called by HandleInputAxis before delegates and default input implementation */
	virtual void PreviewInputAxis( class FEditorViewportClient& ViewportClient, FViewportActionKeyInput& Action, const FKey Key, const float Delta, const float DeltaTime, bool& bOutWasHandled ) {};

	/** To be overridden. Called by HandleInputKey before delegates and default input implementation */
	virtual void HandleInputKey( class FEditorViewportClient& ViewportClient, FViewportActionKeyInput& Action, const FKey Key, const EInputEvent Event, bool& bOutWasHandled ) {};
	
	/** To be overridden. Called by HandleInputAxis before delegates and default input implementation */
	virtual void HandleInputAxis( class FEditorViewportClient& ViewportClient, FViewportActionKeyInput& Action, const FKey Key, const float Delta, const float DeltaTime, bool& bOutWasHandled ) {};

	/** If this interactors allows to smooth the laser. Default is true. */
	virtual bool AllowLaserSmoothing() const;

protected:

	/** All the private data for the interactor */
	FViewportInteractorData InteractorData;
	
	/** Mapping of raw keys to actions*/
	TMap<FKey, FViewportActionKeyInput> KeyToActionMap;

	/** The owning world interaction */
	UPROPERTY()
	class UViewportWorldInteraction* WorldInteraction;

	/** The paired interactor by the world interaction */
	UPROPERTY()
	UViewportInteractor* OtherInteractor;

	/** True if this interactor supports 'grabber sphere' interaction.  Usually disabled for mouse cursors */
	bool bAllowGrabberSphere;

	/** Store end of the laser pointer. This will be returned when calling GetLaserPointer multiple times a tick */
	TOptional<FVector> SavedLaserPointerEnd;

	/** Store the last hitresult from the laser, to use that when calling GetHitResultFromLaserPointer multiple times in a tick. */
	TOptional<FHitResult> SavedHitResult;

private:

	/** Smoothing filter for laser */
	ViewportInteractionUtils::FOneEuroFilter SmoothingOneEuroFilter;
};