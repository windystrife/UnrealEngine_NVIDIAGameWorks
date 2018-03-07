// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "InputCoreTypes.h"
#include "Engine/EngineTypes.h"
#include "ViewportInteractor.h"
#include "VREditorInteractor.generated.h"

class AActor;
class UWidgetComponent;

enum class EControllerType : uint8
{
	Laser,
	AssistingLaser,
	UI,
	Unknown
};


/**
 * VREditor default interactor
 */
UCLASS()
class VREDITOR_API UVREditorInteractor : public UViewportInteractor
{
	GENERATED_BODY()

public:

	/** Default constructor */
	UVREditorInteractor();

	/** Initialize default values */
	virtual void Init( class UVREditorMode* InVRMode );

	/** Gets the owner of this system */
	class UVREditorMode& GetVRMode()
	{
		return *VRMode;
	}

	/** Gets the owner of this system (const) */
	const class UVREditorMode& GetVRMode() const
	{
		return *VRMode;
	}

	// ViewportInteractorInterface overrides
	virtual void Shutdown() override;
	virtual void Tick( const float DeltaTime ) override;
	virtual FHitResult GetHitResultFromLaserPointer( TArray<AActor*>* OptionalListOfIgnoredActors = nullptr, const bool bIgnoreGizmos = false,
		TArray<UClass*>* ObjectsInFrontOfGizmo = nullptr, const bool bEvenIfBlocked = false, const float LaserLengthOverride = 0.0f ) override;
	virtual void ResetHoverState() override;
	virtual bool IsModifierPressed() const override;
	virtual void PreviewInputKey( class FEditorViewportClient& ViewportClient, FViewportActionKeyInput& Action, const FKey Key, const EInputEvent Event, bool& bOutWasHandled ) override;

	/** Returns the slide delta for pushing and pulling objects. Needs to be implemented by derived classes (e.g. touchpad for vive controller or scrollweel for mouse ) */
	virtual float GetSlideDelta();

	//
	// Getters and setters
	//

	/** Returns what controller type this is for asymmetric control schemes */
	EControllerType GetControllerType() const
	{
		return ControllerType;
	};

	/** Set what controller type this is for asymmetric control schemes */
	void SetControllerType( const EControllerType& InControllerType );

	/** Gets if this interactor is hovering over UI */
	bool IsHoveringOverUI() const;

	/** Sets if the quick menu is on this interactor */
	void SetHasUIOnForearm( const bool bInHasUIOnForearm );
	
	/** Check if the quick menu is on this interactor */
	bool HasUIOnForearm() const;

	/** Gets the current hovered widget component if any */
	UWidgetComponent* GetLastHoveredWidgetComponent() const;

	/** Sets the current hovered widget component */
	void SetLastHoveredWidgetComponent( UWidgetComponent* NewHoveringOverWidgetComponent );

	/** Sets if the interactor is clicking on any UI */
	void SetIsClickingOnUI( const bool bInIsClickingOnUI );
	
	/** Gets if the interactor is clicking on any UI */
	bool IsClickingOnUI() const;

	/** Sets if the interactor is hovering over any UI */
	void SetIsHoveringOverUI( const bool bInIsHoveringOverUI );

	/** Sets if the interactor is right  hover over any UI */
	void SetIsRightClickingOnUI( const bool bInIsRightClickingOnUI );

	/** Gets if the interactor is right clicking on UI */
	bool IsRightClickingOnUI() const;

	/** Sets the time the interactor last pressed on UI */
	void SetLastUIPressTime( const double InLastUIPressTime );

	/** Gets last time the interactor pressed on UI */
	double GetLastUIPressTime() const;

	/** Sets the UI scroll velocity */
	void SetUIScrollVelocity( const float InUIScrollVelocity );

	/** Gets the UI scroll velocity */
	float GetUIScrollVelocity() const;

	/** Gets the trigger value */
	float GetSelectAndMoveTriggerValue() const;
	
	/* ViewportInteractor overrides, checks if the laser is blocked by UI */
	virtual bool GetIsLaserBlocked() const override;
	
protected:

	/** The mode that owns this interactor */
	UPROPERTY()
	class UVREditorMode* VRMode;

	//
	// General input @todo: VREditor: Should this be general (non-UI) in interactordata ?
	//

	/** For asymmetrical systems - what type of controller this is */
	EControllerType ControllerType;

	/** Is the Modifier button held down? */
	bool bIsModifierPressed;

	/** Current trigger pressed amount for 'select and move' (0.0 - 1.0) */
	float SelectAndMoveTriggerValue;

private:

	//
	// UI
	//

	/** True if a floating UI panel is attached to the front of the hand, and we shouldn't bother drawing a laser
	pointer or enabling certain other features */
	bool bHasUIInFront;

	/** True if a floating UI panel is attached to our forearm, so we shouldn't bother drawing help labels */
	bool bHasUIOnForearm;

	/** True if we're currently holding the 'SelectAndMove' button down after clicking on UI */
	bool bIsClickingOnUI;

	/** When bIsClickingOnUI is true, this will be true if we're "right clicking".  That is, the Modifier key was held down at the time that the user clicked */
	bool bIsRightClickingOnUI;

	 /** True if we're hovering over UI right now.  When hovering over UI, we don't bother drawing a see-thru laser pointer */
	bool bIsHoveringOverUI;
	
	/** Inertial scrolling -- how fast to scroll the mousewheel over UI */
	float UIScrollVelocity;	

	/** Last real time that we pressed the 'SelectAndMove' button on UI.  This is used to detect double-clicks. */
	double LastUIPressTime;

protected:

	//
	// Help
	//

	/** True if we want help labels to be visible right now, otherwise false */
	bool bWantHelpLabels;

	/** Help labels for buttons on the motion controllers */
	TMap< FKey, class AFloatingText* > HelpLabels;

	/** Time that we either started showing or hiding help labels (for fade transitions) */
	FTimespan HelpLabelShowOrHideStartTime;
};