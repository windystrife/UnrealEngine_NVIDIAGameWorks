// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "InputCoreTypes.h"
#include "Templates/SubclassOf.h"
#include "Editor/UnrealEdTypes.h"
#include "EditorWorldExtension.h"
#include "ShowFlags.h"
#include "Misc/App.h"
#include "Widgets/SWindow.h"
#include "HeadMountedDisplayTypes.h"
#include "UI/VRRadialMenuHandler.h"
#include "VREditorMode.generated.h"

class AActor;
class FEditorViewportClient;
class SLevelViewport;
enum class EAutoChangeMode : uint8;
class UStaticMesh;
class UStaticMeshComponent;
class USoundBase;
class UMaterialInterface;

// Forward declare the GizmoHandleTypes that is defined in VIBaseTransformGizmo.h
enum class EGizmoHandleTypes : uint8;

/**
 * Types of actions that can be performed with VR controller devices
 */
namespace VRActionTypes
{
	static const FName Touch( "Touch" );
	static const FName Modifier( "Modifier" );
	static const FName Modifier2( "Modifier2" ); //Only used by Oculus
	static const FName ConfirmRadialSelection( "ConfirmRadialSelection" );
	static const FName TrackpadPositionX( "TrackpadPositionX" );
	static const FName TrackpadPositionY( "TrackpadPositionY" );
	static const FName TriggerAxis( "TriggerAxis" );
}

/**
 * VR Editor Mode. Extends editor viewports with functionality for VR controls and object manipulation
 */
UCLASS()
class VREDITOR_API UVREditorMode : public UEditorWorldExtension
{
	GENERATED_BODY()

public:

	/** Default constructor */
	UVREditorMode();

	/** Initialize the VREditor */
	virtual void Init() override;

	/** Shutdown the VREditor */
	virtual void Shutdown() override;	
	
	/** When the user actually enters the VR Editor mode */
	void Enter();

	/** When the user leaves the VR Editor mode */
	void Exit( const bool bShouldDisableStereo );

	/** Tick before the ViewportWorldInteraction is ticked */
	void PreTick( const float DeltaTime );

	/** Tick after the ViewportWorldInteraction is ticked */
	void PostTick( const float DeltaTime );

	/** Static: Sets whether we should actually use an HMD.  Call this before activating VR mode */
	void SetActuallyUsingVR( const bool bShouldActuallyUseVR )
	{
		bActuallyUsingVR = bShouldActuallyUseVR;
	}

	/** Returns true if we're actually using VR, or false if we're faking it */
	bool IsActuallyUsingVR()
	{
		return bActuallyUsingVR;
	}

	/** Returns true if the user wants to exit this mode */
	bool WantsToExitMode() const
	{
		return bWantsToExitMode;
	}

	/** Call this to start exiting VR mode */
	void StartExitingVRMode();
	
	/** Gets the world space transform of the calibrated VR room origin.  When using a seated VR device, this will feel like the
	camera's world transform (before any HMD positional or rotation adjustments are applied.) */
	FTransform GetRoomTransform() const;

	/** Sets a new transform for the room, in world space.  This is basically setting the editor's camera transform for the viewport */
	void SetRoomTransform( const FTransform& NewRoomTransform );

	/** Gets the transform of the user's HMD in room space */
	FTransform GetRoomSpaceHeadTransform() const;

	/**
	 * Gets the world space transform of the HMD (head)
	 *
	 * @return	World space space HMD transform
	 */
	FTransform GetHeadTransform() const;

	/** Gets access to the world interaction system (const) */
	const class UViewportWorldInteraction& GetWorldInteraction() const;

	/** Gets access to the world interaction system */
	class UViewportWorldInteraction& GetWorldInteraction();

	/** If the mode was completely initialized */
	bool IsFullyInitialized() const;

	/** * Gets the tick handle to give external systems the change to be ticked right after the ViewportWorldInteraction is ticked */
	DECLARE_EVENT_OneParam(UVREditorMode, FOnVRTickHandle, const float /* DeltaTime */);
	FOnVRTickHandle& OnTickHandle()
	{
		return TickHandle;
	}

	/** Returns the Unreal controller ID for the motion controllers we're using */
	int32 GetMotionControllerID() const
	{
		return MotionControllerID;
	}

	/** Returns whether or not the flashlight is visible */
	bool IsFlashlightOn() const
	{
		return bIsFlashlightOn;
	}

	/** Returns the time since the VR Editor mode was last entered */
	inline FTimespan GetTimeSinceModeEntered() const
	{
		return FTimespan::FromSeconds( FApp::GetCurrentTime() ) - AppTimeModeEntered;
	}

	// @todo vreditor: Move this to FMath so we can use it everywhere
	// NOTE: OvershootAmount is usually between 0.5 and 2.0, but can go lower and higher for extreme overshots
	template<class T>
	static T OvershootEaseOut( T Alpha, const float OvershootAmount = 1.0f )
	{
		Alpha--;
		return 1.0f - ( ( Alpha * ( ( OvershootAmount + 1 ) * Alpha + OvershootAmount ) + 1 ) - 1.0f );
	}

	/** Gets access to the VR UI system (const) */
	const class UVREditorUISystem& GetUISystem() const
	{
		return *UISystem;
	}

	/** Gets access to the VR UI system */
	class UVREditorUISystem& GetUISystem()
	{
		return *UISystem;
	}

	/** Lets other modules know if the radial menu is visible on a given interactor so input should be handled differently */
	bool IsShowingRadialMenu( const class UVREditorInteractor* Interactor ) const;

	/** Gets the viewport that VR Mode is activated in.  Even though editor modes are available in all
	    level viewports simultaneously, only one viewport is "possessed" by the HMD.  Generally try to avoid using
		this function and instead use the ViewportClient that is passed around through various FEdMode overrides */
	const class SLevelViewport& GetLevelViewportPossessedForVR() const;

	/** Mutable version of above. */
	class SLevelViewport& GetLevelViewportPossessedForVR();

	/** Gets the world scale factor, which can be multiplied by a scale vector to convert to room space */
	float GetWorldScaleFactor() const;

	/** Spawns a flashlight on the specified hand */
	void ToggleFlashlight( class UVREditorInteractor* Interactor );

	/** Will update the TransformGizmo Actor with the next Gizmo type  */
	void CycleTransformGizmoHandleType();

	/** Gets the current Gizmo handle type */
	EGizmoHandleTypes GetCurrentGizmoType() const;

	/** @return Returns the type of HMD we're dealing with */
	EHMDDeviceType::Type GetHMDDeviceType() const;

	/** @return Checks to see if the specified interactor is aiming roughly toward the specified capsule */
	bool IsHandAimingTowardsCapsule(class UViewportInteractor* Interactor, const FTransform& CapsuleTransform, const FVector CapsuleStart, const FVector CapsuleEnd, const float CapsuleRadius, const float MinDistanceToCapsule, const FVector CapsuleFrontDirection, const float MinDotForAimingAtCapsule) const;

	/** Gets the hand interactor  */
	class UVREditorInteractor* GetHandInteractor( const EControllerHand ControllerHand ) const;

	/** Snaps the current selected actor to the ground */
	void SnapSelectedActorsToGround();
	
	/** Saved information about the editor and viewport we possessed, so we can restore it after exiting VR mode */
	struct FSavedEditorState
	{
		ELevelViewportType ViewportType;
		FVector ViewLocation;
		FRotator ViewRotation;
		FEngineShowFlags ShowFlags;
		bool bLockedPitch;
		bool bGameView;
		bool bAlwaysShowModeWidgetAfterSelectionChanges;
		float NearClipPlane;
		bool bRealTime;
		float DragTriggerDistance;
		bool bOnScreenMessages;
		float TransformGizmoScale;
		EHMDTrackingOrigin::Type TrackingOrigin;
		float WorldToMetersScale;
		bool bCinematicPreviewViewport;
		bool bKeyAllEnabled;
		EAutoChangeMode AutoChangeMode;

		FSavedEditorState()
			: ViewportType(LVT_Perspective),
			  ViewLocation(FVector::ZeroVector),
			  ViewRotation(FRotator::ZeroRotator),
 			  ShowFlags(ESFIM_Editor),
			  bLockedPitch(false),
			  bGameView(false),
			  bAlwaysShowModeWidgetAfterSelectionChanges(false),
			  NearClipPlane(0.0f),
			  bRealTime(false),
			  DragTriggerDistance(0.0f),
			  bOnScreenMessages(false),
			  TransformGizmoScale( 1.0f ),
			  TrackingOrigin(EHMDTrackingOrigin::Eye),
			  WorldToMetersScale(100.0f),
			  bCinematicPreviewViewport(false),
			  bKeyAllEnabled(false),
			  AutoChangeMode()
		{
		}
	};

	/** Gets the saved editor state from entering the mode */
	const FSavedEditorState& GetSavedEditorState() const;

	DECLARE_DELEGATE(FOnVREditingModeExit);
	/** Used to override dockable area restoration behavior */
	FOnVREditingModeExit OnVREditingModeExit_Handler;

	void SaveSequencerSettings(bool bInKeyAllEnabled, EAutoChangeMode InAutoChangeMode, const class USequencerSettings& InSequencerSettings);

	/** Start or stop simulate-in-editor mode */
	void ToggleSIEAndVREditor();

	/** Start or stop play-in-editor mode */
	void TogglePIEAndVREditor();

	/** Create a static motion controller mesh for the current HMD platform */
	UStaticMeshComponent* CreateMotionControllerMesh( AActor* OwningActor, USceneComponent* AttachmentToComponent );

	/** Helper functions to create a static mesh */
	UStaticMeshComponent* CreateMesh( AActor* OwningActor, const FString& MeshName, USceneComponent* AttachmentToComponent /*= nullptr */ );
	UStaticMeshComponent* CreateMesh(AActor* OwningActor, UStaticMesh* Mesh, USceneComponent* AttachmentToComponent /*= nullptr */);

	/** Sets a delegate for the context-specific actions menu */
	void SetActionsMenuGenerator(const FOnRadialMenuGenerated NewMenuGenerator, const FText NewLabel);

	/** Resets the delegate and button for the context-specific actions menu */
	void ResetActionsMenuGenerator();

	/** Gets access to VREditorWorldInteraction */
	class UVREditorPlacement* GetPlacementSystem()
	{
		return PlacementSystem;
	}

	/** Returns true if we started the play in editor session from this VR Editor */
	bool GetStartedPlayFromVREditor() const;
	
	/** Gets the container for all the assets of VREditor. */
	const class UVREditorAssetContainer& GetAssetContainer() const;
	
	/** Plays sound at location. */
	void PlaySound(USoundBase* SoundBase, const FVector& InWorldLocation, const float InVolume = 1.0f);

	/** Delegate to be called when a material is placed **/
	DECLARE_EVENT_ThreeParams( UVREditorPlacement, FOnPlaceDraggedMaterial, UPrimitiveComponent*, UMaterialInterface*, bool& );
	FOnPlaceDraggedMaterial& OnPlaceDraggedMaterial() { return OnPlaceDraggedMaterialEvent; };

	/** Call this to force the 'Actions' radial menu to refresh.  This is useful if the menu generator that you've bound
	    needs to be re-run (usually because it switches on something that has changed since the last time it ran.) */
	void RefreshRadialMenuActionsSubmenu();

	/** Return true if currently aiming to teleport. */
	bool IsAimingTeleport() const;
	bool IsTeleporting() const;

	/** Toggles the debug mode. */
	static void ToggleDebugMode();

	/** Returns if the VR Mode is in debug mode. */
	static bool IsDebugModeEnabled();

	/** Delegate to be called when the debug mode is toggled. */
	DECLARE_EVENT_OneParam(UVREditorMode, FOnToggleVRModeDebug, bool);
	FOnToggleVRModeDebug& OnToggleDebugMode() { return OnToggleDebugModeEvent; };

protected:
	
	virtual void TransitionWorld(UWorld* NewWorld) override;

private:

	/** Called when the editor is closed */
	void OnEditorClosed();

	/** Called when someone closes a standalone VR Editor window */
	void OnVREditorWindowClosed( const TSharedRef<SWindow>& ClosedWindow );

	/** FEditorDelegates callbacks */
	void PostPIEStarted( bool bIsSimulatingInEditor );
	void PrePIEEnded( bool bWasSimulatingInEditor );
	void OnEndPIE( bool bWasSimulatingInEditor );
	void OnPreSwitchPIEAndSIE(bool bIsSimulatingInEditor);
	void OnSwitchPIEAndSIE(bool bIsSimulatingInEditor);

	/** Start using the viewport passed */
	void StartViewport( TSharedPtr<SLevelViewport> Viewport );
	
	/** Close the current viewport */
	void CloseViewport( const bool bShouldDisableStereo );

	/** Resets all the settings when exiting PIE to VR Editor. */
	void RestoreFromPIE();

	/** Restore the world to meters to the saved one when entering VR Editor */
	void RestoreWorldToMeters();

protected:

	//
	// Startup/Shutdown
	//

	/** The VR editor window, if it's open right now */
	TWeakPtr< class SWindow > VREditorWindowWeakPtr;

	/** The VR level viewport, if we're in VR mode */
	TWeakPtr< class SLevelViewport > VREditorLevelViewportWeakPtr;

	/** Saved information about the editor and viewport we possessed, so we can restore it after exiting VR mode */
	FSavedEditorState SavedEditorState;

	/** True if we're in using an actual HMD in this mode, or false if we're "faking" VR mode for testing */
	bool bActuallyUsingVR;

	/** True if we currently want to exit VR mode.  This is used to defer exiting until it is safe to do that */
	bool bWantsToExitMode;

	/** True if VR mode is fully initialized and ready to render */
	bool bIsFullyInitialized;

	/** App time that we entered this mode */
	FTimespan AppTimeModeEntered;


	//
	// Avatar visuals
	//

	/** Actor with components to represent the VR avatar in the world, including motion controller meshes */
	UPROPERTY()
	class AVREditorAvatarActor* AvatarActor;


	//
	// Flashlight
	//

	/** Spotlight for the flashlight */
	class USpotLightComponent* FlashlightComponent;

	/** If there is currently a flashlight in the scene */
	bool bIsFlashlightOn;

	//
	// Input
	//

	/** The Unreal controller ID for the motion controllers we're using */
	int32 MotionControllerID;


	//
	// Subsystems registered
	//

	FOnVRTickHandle TickHandle;

	/** Event broadcast when a material is placed */
	FOnPlaceDraggedMaterial OnPlaceDraggedMaterialEvent;

	//
	// Subsystems
	//

	/** VR UI system */
	UPROPERTY()
	class UVREditorUISystem* UISystem;

	/** Teleporter system */
	UPROPERTY()
	class AVREditorTeleporter* TeleportActor;

	/** Automatic scale system */
	UPROPERTY()
	class UVREditorAutoScaler* AutoScalerSystem;

	//
	// World interaction
	//

	/** World interaction manager */
	UPROPERTY()
	class UViewportWorldInteraction* WorldInteraction;

	/** The current Gizmo type that is used for the TransformGizmo Actor */
	EGizmoHandleTypes CurrentGizmoType;

	UPROPERTY()
	class UVREditorPlacement* PlacementSystem;

	//
	// Interactors
	//

	/** The right motion controller */
	UPROPERTY()
	class UVREditorMotionControllerInteractor* LeftHandInteractor; //@todo vreditor: Hardcoded interactors
	
	/** The right motion controller */
	UPROPERTY()
	class UVREditorMotionControllerInteractor* RightHandInteractor; 

	//
	// Colors
	//
public:
	// Color types
	enum EColors
	{
		DefaultColor,
		SelectionColor,
		WorldDraggingColor,
		UIColor,
		UISelectionBarColor,
		UISelectionBarHoverColor,
		UICloseButtonColor,
		UICloseButtonHoverColor,
		TotalCount
	};

	// Gets the color
	FLinearColor GetColor( const EColors Color ) const;

	// Get the default near clipping plane for VR editing
	float GetDefaultVRNearClipPlane() const;

	/** Runtime and plugin modules can force VR Editor to refresh using this function */
	void RefreshVREditorSequencer(class ISequencer* InCurrentSequencer);

	/** Refresh the current actor preview widget on an in-world UI panel */
	void RefreshActorPreviewWidget(TSharedRef<SWidget> InWidget);

	/** Returns the currently active sequencer */
	class ISequencer* GetCurrentSequencer();

	/** The asset container path */
	static const FString AssetContainerPath;

private:

	// All the colors for this mode
	TArray<FLinearColor> Colors;

	/** If this is the first tick or before */
	bool bFirstTick;

	/** Pointer to the current Sequencer */
	class ISequencer* CurrentSequencer;

	/** The world to meters scale when leaving PIE simulate to restore when back in the editor world. */
	float SavedWorldToMetersScaleForPIE;

	/** If we started play in editor from the VR Editor*/
	bool bStartedPlayFromVREditor;	

	/** If we started play in editor from the VR Editor while in simulate. */
	bool bStartedPlayFromVREditorSimulate;
	
	/** Container of assets */
	UPROPERTY()
	class UVREditorAssetContainer* AssetContainer;

	/** Whether currently in debug mode or not. */
	static bool bDebugModeEnabled;

	/** Event that gets broadcasted when debug mode is toggled. */
	FOnToggleVRModeDebug OnToggleDebugModeEvent;
};
