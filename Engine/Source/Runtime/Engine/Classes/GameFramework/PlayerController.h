// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SlateFwd.h"
#include "UObject/ObjectMacros.h"
#include "Misc/Guid.h"
#include "InputCoreTypes.h"
#include "Templates/SubclassOf.h"
#include "Engine/NetSerialization.h"
#include "Engine/EngineTypes.h"
#include "Engine/EngineBaseTypes.h"
#include "Widgets/SWidget.h"
#include "Engine/LatentActionManager.h"
#include "SceneTypes.h"
#include "GameFramework/Controller.h"
#include "UObject/TextProperty.h"
#include "GameFramework/OnlineReplStructs.h"
#include "GameFramework/PlayerMuteList.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/InputComponent.h"
#include "GameFramework/ForceFeedbackEffect.h"
#include "GenericPlatform/IInputInterface.h"
#include "PlayerController.generated.h"

class ACameraActor;
class APawn;
class ASpectatorPawn;
class FDebugDisplayInfo;
class UActorChannel;
class UGameViewportClient;
class UInterpTrackInstDirector;
class ULocalMessage;
class UPlayer;
class UPrimitiveComponent;
struct FActiveHapticFeedbackEffect;
struct FCollisionQueryParams;

/** Default delegate that provides an implementation for those that don't have special needs other than a toggle */
DECLARE_DELEGATE_RetVal(bool, FCanUnpause);

/** delegate used to override default viewport audio listener position calculated from camera */
DECLARE_DELEGATE_ThreeParams(FGetAudioListenerPos, FVector& /*Location*/, FVector& /*ProjFront*/, FVector& /*ProjRight*/);

DECLARE_LOG_CATEGORY_EXTERN(LogPlayerController, Log, All);

UENUM()
namespace EDynamicForceFeedbackAction
{
	enum Type
	{
		Start,
		Update,
		Stop,
	};
}

struct FDynamicForceFeedbackDetails
{
	uint32 bAffectsLeftLarge:1;
	uint32 bAffectsLeftSmall:1;
	uint32 bAffectsRightLarge:1;
	uint32 bAffectsRightSmall:1;

	float Intensity;

	FDynamicForceFeedbackDetails()
		: bAffectsLeftLarge(true)
		, bAffectsLeftSmall(true)
		, bAffectsRightLarge(true)
		, bAffectsRightSmall(true)
		, Intensity(0.f)
	{}

	void Update(FForceFeedbackValues& Values) const;
};

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
/** Used to display the force feedback history of what was played most recently. */
struct FForceFeedbackEffectHistoryEntry
{
	FActiveForceFeedbackEffect LastActiveForceFeedbackEffect;
	float TimeShown;

	FForceFeedbackEffectHistoryEntry(FActiveForceFeedbackEffect LastActiveFFE, float Time)
	{
		LastActiveForceFeedbackEffect = LastActiveFFE;
		TimeShown = Time;
	}
};
#endif

/** Abstract base class for Input Mode structures */
struct ENGINE_API FInputModeDataBase
{
protected:
	virtual ~FInputModeDataBase() { }

	/** Derived classes override this function to apply the necessary settings for the desired input mode */
	virtual void ApplyInputMode(class FReply& SlateOperations, class UGameViewportClient& GameViewportClient) const = 0;

	/** Utility functions for derived classes. */
	void SetFocusAndLocking(FReply& SlateOperations, TSharedPtr<class SWidget> InWidgetToFocus, bool bLockMouseToViewport, TSharedRef<class SViewport> InViewportWidget) const;

	friend class APlayerController;
};

/** Data structure used to setup an input mode that allows only the UI to respond to user input. */
struct ENGINE_API FInputModeUIOnly : public FInputModeDataBase
{
	/** Widget to focus */
	FInputModeUIOnly& SetWidgetToFocus(TSharedPtr<SWidget> InWidgetToFocus) { WidgetToFocus = InWidgetToFocus; return *this; }

	/** Whether to lock the mouse to the viewport */
	DEPRECATED(4.13, "Mouse locking behavior is now controlled by an enum. Please use SetLockMouseToViewportBehavior(...) instead.")
	FInputModeUIOnly& SetLockMouseToViewport(bool InLockMouseToViewport) { return SetLockMouseToViewportBehavior( InLockMouseToViewport ? EMouseLockMode::LockOnCapture : EMouseLockMode::DoNotLock ); }

	/** Sets the mouse locking behavior of the viewport */
	FInputModeUIOnly& SetLockMouseToViewportBehavior(EMouseLockMode InMouseLockMode) { MouseLockMode = InMouseLockMode; return *this; }

	FInputModeUIOnly()
		: WidgetToFocus()
		, MouseLockMode(EMouseLockMode::DoNotLock)
	{}

protected:
	TSharedPtr<SWidget> WidgetToFocus;
	EMouseLockMode MouseLockMode;

	virtual void ApplyInputMode(FReply& SlateOperations, class UGameViewportClient& GameViewportClient) const override;
};

/** Data structure used to setup an input mode that allows the UI to respond to user input, and if the UI doesn't handle it player input / player controller gets a chance. */
struct ENGINE_API FInputModeGameAndUI : public FInputModeDataBase
{
	/** Widget to focus */
	FInputModeGameAndUI& SetWidgetToFocus(TSharedPtr<SWidget> InWidgetToFocus) { WidgetToFocus = InWidgetToFocus; return *this; }

	/** Whether to lock the mouse to the viewport */
	DEPRECATED(4.13, "Mouse locking behavior is now controlled by an enum. Please use SetLockMouseToViewportBehavior(...) instead.")
	FInputModeGameAndUI& SetLockMouseToViewport(bool InLockMouseToViewport) { return SetLockMouseToViewportBehavior( InLockMouseToViewport ? EMouseLockMode::LockOnCapture : EMouseLockMode::DoNotLock ); }

	/** Sets the mouse locking behavior of the viewport */
	FInputModeGameAndUI& SetLockMouseToViewportBehavior(EMouseLockMode InMouseLockMode) { MouseLockMode = InMouseLockMode; return *this; }

	/** Whether to hide the cursor during temporary mouse capture caused by a mouse down */
	FInputModeGameAndUI& SetHideCursorDuringCapture(bool InHideCursorDuringCapture) { bHideCursorDuringCapture = InHideCursorDuringCapture; return *this; }

	FInputModeGameAndUI()
		: WidgetToFocus()
		, MouseLockMode(EMouseLockMode::DoNotLock)
		, bHideCursorDuringCapture(true)
	{}

protected:

	TSharedPtr<SWidget> WidgetToFocus;
	EMouseLockMode MouseLockMode;
	bool bHideCursorDuringCapture;

	virtual void ApplyInputMode(FReply& SlateOperations, class UGameViewportClient& GameViewportClient) const override;
};

/** Data structure used to setup an input mode that allows only the player input / player controller to respond to user input. */
struct ENGINE_API FInputModeGameOnly : public FInputModeDataBase
{
	/** Whether the mouse down that causes capture should be consumed, and not passed to player input processing */
	FInputModeGameOnly& SetConsumeCaptureMouseDown(bool InConsumeCaptureMouseDown) { bConsumeCaptureMouseDown = InConsumeCaptureMouseDown; return *this; }

	FInputModeGameOnly()
		: bConsumeCaptureMouseDown(true)
	{}

protected:
	bool bConsumeCaptureMouseDown;

	virtual void ApplyInputMode(FReply& SlateOperations, class UGameViewportClient& GameViewportClient) const override;
};


//~=============================================================================
/**
 * PlayerControllers are used by human players to control Pawns.
 *
 * ControlRotation (accessed via GetControlRotation()), determines the aiming
 * orientation of the controlled Pawn.
 *
 * In networked games, PlayerControllers exist on the server for every player-controlled pawn,
 * and also on the controlling client's machine. They do NOT exist on a client's
 * machine for pawns controlled by remote players elsewhere on the network.
 *
 * @see https://docs.unrealengine.com/latest/INT/Gameplay/Framework/Controller/PlayerController/
 */

UCLASS(config=Game, BlueprintType, Blueprintable, meta=(ShortTooltip="A Player Controller is an actor responsible for controlling a Pawn used by the player."))
class ENGINE_API APlayerController : public AController
{
	GENERATED_UCLASS_BODY()
	
	/** UPlayer associated with this PlayerController.  Could be a local player or a net connection. */
	UPROPERTY()
	class UPlayer* Player;

	/** when true, reduces connect timeout from InitialConnectionTimeOut to ConnectionTimeout.  
		Set once initial level load is complete (client may be unresponsive during level loading). */
	uint32 bShortConnectTimeOut:1;

	/** Used in net games so client can acknowledge it possessed a specific pawn. */
	UPROPERTY()
	class APawn* AcknowledgedPawn;

	/** Director track that's currently possessing this player controller, or none if not possessed. */
	UPROPERTY(transient)
	class UInterpTrackInstDirector* ControllingDirTrackInst;

	/** last used FOV based multiplier to distance to an object when determining if it exceeds the object's cull distance
	 * @note: only valid for local player
	 */
	float LocalPlayerCachedLODDistanceFactor;

	/** Heads up display associated with this PlayerController. */
	UPROPERTY()
	class AHUD* MyHUD;

	// ******************************************************************************
	// Camera/view related variables

	/** Camera manager associated with this Player Controller. */
	UPROPERTY(BlueprintReadOnly, Category=PlayerController)
	class APlayerCameraManager* PlayerCameraManager;

	/** PlayerCamera class should be set for each game, otherwise Engine.PlayerCameraManager is used */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=PlayerController)
	TSubclassOf<class APlayerCameraManager> PlayerCameraManagerClass;

	/** 
	 * True to allow this player controller to manage the camera target for you,
	 * typically by using the possessed pawn as the camera target. Set to false
	 * if you want to manually control the camera target.
	 */
	UPROPERTY(EditAnywhere, Category=PlayerController)
	bool bAutoManageActiveCameraTarget;

	/** Used to replicate the view rotation of targets not owned/possessed by this PlayerController. */ 
	UPROPERTY(replicated)
	FRotator TargetViewRotation; 

	/**  Smoothed version of TargetViewRotation to remove jerkiness from intermittent replication updates. */
	FRotator BlendedTargetViewRotation;

	/** Interp speed for blending remote view rotation for smoother client updates */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=PlayerController)
	float SmoothTargetViewRotationSpeed;

	/** The actors which the camera shouldn't see - e.g. used to hide actors which the camera penetrates */
	UPROPERTY()
	TArray<class AActor*> HiddenActors;

	/** Explicit components the camera shouldn't see (helpful for external systems to hide a component from a single player) */
	UPROPERTY()
	TArray< TWeakObjectPtr<UPrimitiveComponent> > HiddenPrimitiveComponents;

	/** Whether to render primitives component. */
	bool bRenderPrimitiveComponents;

	/** Used to make sure the client is kept synchronized when in a spectator state */
	UPROPERTY()
	float LastSpectatorStateSynchTime;

	/** Last location synced on the server for a spectator. */
	UPROPERTY(Transient)
	FVector LastSpectatorSyncLocation;

	/** Last rotation synced on the server for a spectator. */
	UPROPERTY(Transient)
	FRotator LastSpectatorSyncRotation;

	/** Cap set by server on bandwidth from client to server in bytes/sec (only has impact if >=2600) */
	UPROPERTY()
	int32 ClientCap;
	
	/** Object that manages "cheat" commands.  Not instantiated in shipping builds. */
	UPROPERTY(transient, BlueprintReadOnly, Category="Cheat Manager")
	class UCheatManager* CheatManager;
	
	/** Class of my CheatManager.  The Cheat Manager is not created in shipping builds */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cheat Manager")
	TSubclassOf<class UCheatManager> CheatClass;

	/** Object that manages player input. */
	UPROPERTY(transient)
	class UPlayerInput* PlayerInput;    
	
	UPROPERTY(transient)
	TArray<FActiveForceFeedbackEffect> ActiveForceFeedbackEffects;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	/** For debbugging, shows the last force feeback effects that played */
	TArray<FForceFeedbackEffectHistoryEntry> ForceFeedbackEffectHistoryEntries;
#endif

	TMap<int32, FDynamicForceFeedbackDetails> DynamicForceFeedbacks;

	/** Currently playing haptic effects for both the left and right hand */
	TSharedPtr<struct FActiveHapticFeedbackEffect> ActiveHapticEffect_Left;
	TSharedPtr<struct FActiveHapticFeedbackEffect> ActiveHapticEffect_Right;
	TSharedPtr<struct FActiveHapticFeedbackEffect> ActiveHapticEffect_Gun;

	/** list of names of levels the server is in the middle of sending us for a PrepareMapChange() call */
	TArray<FName> PendingMapChangeLevelNames;

	/** Is this player currently in cinematic mode?  Prevents rotation/movement/firing/etc */
	uint32 bCinematicMode:1;
	
	/** When cinematic mode is true, signifies that this controller's pawn should be hidden */
	uint32 bHidePawnInCinematicMode:1;

	/** Whether this controller is using streaming volumes.  **/
	uint32 bIsUsingStreamingVolumes:1;

	/** True if PlayerController is currently waiting for the match to start or to respawn. Only valid in Spectating state. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category=PlayerController)
	uint32 bPlayerIsWaiting:1;

	/** Indicate that the Spectator is waiting to join/respawn. */
	UFUNCTION(server, reliable, WithValidation, Category=PlayerController)
	void ServerSetSpectatorWaiting(bool bWaiting);

	/** Indicate that the Spectator is waiting to join/respawn. */
	UFUNCTION(client, reliable, Category=PlayerController)
	void ClientSetSpectatorWaiting(bool bWaiting);

	/** index identifying players using the same base connection (splitscreen clients)
	 * Used by netcode to match replicated PlayerControllers to the correct splitscreen viewport and child connection
	 * replicated via special internal code, not through normal variable replication
	 */
	UPROPERTY(DuplicateTransient)
	uint8 NetPlayerIndex;

	/** List of muted players in various categories */
	FPlayerMuteList MuteList;

	/** this is set on the OLD PlayerController when performing a swap over a network connection
	 * so we know what connection we're waiting on acknowledgment from to finish destroying this PC
	 * (or when the connection is closed)
	 * @see GameModeBase::SwapPlayerControllers()
	 */
	UPROPERTY(DuplicateTransient)
	class UNetConnection* PendingSwapConnection;

	/** The net connection this controller is communicating on, NULL for local players on server */
	UPROPERTY(DuplicateTransient)
	class UNetConnection* NetConnection;

	// Input axes values, accumulated each tick.
	FRotator RotationInput;

	/** Yaw input speed scaling */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, config, Category=PlayerController)
	float InputYawScale;

	/** Pitch input speed scaling */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, config, Category=PlayerController)
	float InputPitchScale;

	/** Roll input speed scaling */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, config, Category=PlayerController)
	float InputRollScale;

	/** Whether the mouse cursor should be displayed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=MouseInterface)
	uint32 bShowMouseCursor:1;

	/** Whether actor/component click events should be generated. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=MouseInterface)
	uint32 bEnableClickEvents:1;

	/** Whether actor/component touch events should be generated. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=MouseInterface)
	uint32 bEnableTouchEvents:1;

	/** Whether actor/component mouse over events should be generated. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=MouseInterface)
	uint32 bEnableMouseOverEvents:1;

	/** Whether actor/component touch over events should be generated. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=MouseInterface)
	uint32 bEnableTouchOverEvents:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Game|Feedback")
	uint32 bForceFeedbackEnabled:1;

	/** Scale applied to force feedback values */
	UPROPERTY(config)
	float ForceFeedbackScale;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=MouseInterface, meta=(EditCondition="bEnableClickEvents"))
	TArray<FKey> ClickEventKeys;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=MouseInterface)
	TEnumAsByte<EMouseCursor::Type> DefaultMouseCursor;

	UPROPERTY(BlueprintReadWrite, Category=MouseInterface)
	TEnumAsByte<EMouseCursor::Type> CurrentMouseCursor;

	/** Default trace channel used for determining what world object was clicked on. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=MouseInterface)
	TEnumAsByte<ECollisionChannel> DefaultClickTraceChannel;

	/** Trace channel currently being used for determining what world object was clicked on. */
	UPROPERTY(BlueprintReadWrite, Category=MouseInterface)
	TEnumAsByte<ECollisionChannel> CurrentClickTraceChannel;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=MouseInterface, meta=(DisplayName="Trace Distance"))
	float HitResultTraceDistance;

	FForceFeedbackValues ForceFeedbackValues;

public:
	/**  Enables cheats within the game */
	UFUNCTION(exec)
	virtual void EnableCheats();

	/** Timer used by RoundEnded and Inactive states to accept player input again */
	virtual void UnFreeze();

	/** Calculate minimal respawn delay */
	virtual float GetMinRespawnDelay();

	/** Set the field of view to NewFOV */ 
	UFUNCTION(exec)
	virtual void FOV(float NewFOV);

	/** Restarts the current level */
	UFUNCTION(exec)
	virtual void RestartLevel();

	/** Causes the client to travel to the given URL */
	UFUNCTION(exec)
	virtual void LocalTravel(const FString& URL);

	/** Return the client to the main menu gracefully */
	UFUNCTION(Reliable, Client)
	virtual void ClientReturnToMainMenu(const FString& ReturnReason);

	/** Development RPC for testing object reference replication */
	UFUNCTION(Reliable, Client)
	virtual void ClientRepObjRef(UObject* Object);

	/**
	 * Locally try to pause game (call serverpause to pause network game); returns success indicator.  Calls GameModeBase's SetPause().
	 * @return true if succeeded to pause
	 */
	virtual bool SetPause(bool bPause, FCanUnpause CanUnpauseDelegate = FCanUnpause());

	/** Command to try to pause the game. */
	UFUNCTION(exec)
	virtual void Pause();

	/** Trys to set the player's name to the given name. */
	UFUNCTION(exec)
	virtual void SetName(const FString& S);

	/** SwitchLevel to the given MapURL. */
	UFUNCTION(exec)
	virtual void SwitchLevel(const FString& URL);

	/** 
	 * called to notify the server when the client has loaded a new world via seamless traveling
	 * @param WorldPackageName the name of the world package that was loaded
	 * @param bFinalDest whether this world is the destination map for the travel (i.e. not the transition level)
	 */
	virtual void NotifyLoadedWorld(FName WorldPackageName, bool bFinalDest);

	/**
	 * Processes player input (immediately after PlayerInput gets ticked) and calls UpdateRotation().
	 * PlayerTick is only called if the PlayerController has a PlayerInput object. Therefore, it will only be called for locally controlled PlayerControllers.
	 */
	virtual void PlayerTick(float DeltaTime);

	/** Method called prior to processing input */
	virtual void PreProcessInput(const float DeltaTime, const bool bGamePaused);

	/** Method called after processing input */
	virtual void PostProcessInput(const float DeltaTime, const bool bGamePaused);

	/** Adjust input based on cinematic mode 
	  * @param	bInCinematicMode	specify true if the player is entering cinematic mode; false if the player is leaving cinematic mode.
	  * @param	bAffectsMovement	specify true to disable movement in cinematic mode, enable it when leaving
	  * @param	bAffectsTurning		specify true to disable turning in cinematic mode or enable it when leaving
	  */
	virtual void SetCinematicMode( bool bInCinematicMode, bool bAffectsMovement, bool bAffectsTurning);

	/** Reset move and look input ignore flags to defaults */
	virtual void ResetIgnoreInputFlags() override;

	bool GetHitResultAtScreenPosition(const FVector2D ScreenPosition, const ECollisionChannel TraceChannel, const FCollisionQueryParams& CollisionQueryParams, FHitResult& HitResult) const;
	bool GetHitResultAtScreenPosition(const FVector2D ScreenPosition, const ECollisionChannel TraceChannel, bool bTraceComplex, FHitResult& HitResult) const;
	bool GetHitResultAtScreenPosition(const FVector2D ScreenPosition, const ETraceTypeQuery TraceChannel, bool bTraceComplex, FHitResult& HitResult) const;
	bool GetHitResultAtScreenPosition(const FVector2D ScreenPosition, const TArray<TEnumAsByte<EObjectTypeQuery> > & ObjectTypes, bool bTraceComplex, FHitResult& HitResult) const;

	UFUNCTION(BlueprintCallable, Category="Game|Player", meta=(DeprecatedFunction, DeprecationMessage = "Use new GetHitResultUnderCursorByChannel or GetHitResultUnderCursorForObject", TraceChannel=ECC_Visibility, bTraceComplex=true))
	bool GetHitResultUnderCursor(ECollisionChannel TraceChannel, bool bTraceComplex, FHitResult& HitResult) const;

	UFUNCTION(BlueprintCallable, Category="Game|Player", meta=(bTraceComplex=true))
	bool GetHitResultUnderCursorByChannel(ETraceTypeQuery TraceChannel, bool bTraceComplex, FHitResult& HitResult) const;

	UFUNCTION(BlueprintCallable, Category="Game|Player", meta=(bTraceComplex=true))
	bool GetHitResultUnderCursorForObjects(const TArray<TEnumAsByte<EObjectTypeQuery> > & ObjectTypes, bool bTraceComplex, FHitResult& HitResult) const;

	UFUNCTION(BlueprintCallable, Category="Game|Player", meta=(DeprecatedFunction, DeprecationMessage = "Use new GetHitResultUnderFingerByChannel or GetHitResultUnderFingerForObject", TraceChannel=ECC_Visibility, bTraceComplex=true))
	bool GetHitResultUnderFinger(ETouchIndex::Type FingerIndex, ECollisionChannel TraceChannel, bool bTraceComplex, FHitResult& HitResult) const;

	UFUNCTION(BlueprintCallable, Category="Game|Player", meta=(bTraceComplex=true))
	bool GetHitResultUnderFingerByChannel(ETouchIndex::Type FingerIndex, ETraceTypeQuery TraceChannel, bool bTraceComplex, FHitResult& HitResult) const;

	UFUNCTION(BlueprintCallable, Category="Game|Player", meta=(bTraceComplex=true))
	bool GetHitResultUnderFingerForObjects(ETouchIndex::Type FingerIndex, const  TArray<TEnumAsByte<EObjectTypeQuery> > & ObjectTypes, bool bTraceComplex, FHitResult& HitResult) const;

	/** Convert current mouse 2D position to World Space 3D position and direction. Returns false if unable to determine value. **/
	UFUNCTION(BlueprintCallable, Category = "Game|Player", meta = (DisplayName = "ConvertMouseLocationToWorldSpace", Keywords = "deproject"))
	bool DeprojectMousePositionToWorld(FVector& WorldLocation, FVector& WorldDirection) const;

	/** Convert 2D screen position to World Space 3D position and direction. Returns false if unable to determine value. **/
	UFUNCTION(BlueprintCallable, Category = "Game|Player", meta = (DisplayName = "ConvertScreenLocationToWorldSpace", Keywords = "deproject"))
	bool DeprojectScreenPositionToWorld(float ScreenX, float ScreenY, FVector& WorldLocation, FVector& WorldDirection) const;

	/**
	 * Convert a World Space 3D position into a 2D Screen Space position.
	 * @return true if the world coordinate was successfully projected to the screen.
	 */
	UFUNCTION(BlueprintCallable, Category = "Game|Player", meta = (DisplayName = "ConvertWorldLocationToScreenLocation", Keywords = "project"))
	bool ProjectWorldLocationToScreen(FVector WorldLocation, FVector2D& ScreenLocation, bool bPlayerViewportRelative = false) const;

	/**
	 * Convert a World Space 3D position into a 3D Screen Space position.
	 * @return true if the world coordinate was successfully projected to the screen.
	 */
	bool ProjectWorldLocationToScreenWithDistance(FVector WorldLocation, FVector& ScreenLocation, bool bPlayerViewportRelative = false) const;
	
	/** Positions the mouse cursor in screen space, in pixels. */
	UFUNCTION( BlueprintCallable, Category="Game|Player", meta = (DisplayName = "SetMousePosition", Keywords = "mouse" ))
	void SetMouseLocation( const int X, const int Y );
	/**
	  * Updates the rotation of player, based on ControlRotation after RotationInput has been applied.
	  * This may then be modified by the PlayerCamera, and is passed to Pawn->FaceRotation().
	  */
	virtual void UpdateRotation(float DeltaTime);

protected:
	/** Pawn has been possessed, so changing state to NAME_Playing. Start it walking and begin playing with it. */
	virtual void BeginPlayingState();

	// Leave playing state. */ 
	virtual void EndPlayingState();

	/** overridden to return that player controllers are capable of RPCs */
	virtual bool HasNetOwner() const override;

public:
	/** Fire the player's currently selected weapon with the optional firemode. */
	UFUNCTION(exec)
	virtual void StartFire(uint8 FireModeNum = 0);

	/** Notify player of change to level */
	void LevelStreamingStatusChanged(class ULevelStreaming* LevelObject, bool bNewShouldBeLoaded, bool bNewShouldBeVisible, bool bNewShouldBlockOnLoad, int32 LODIndex);

	/** used to wait until a map change can be prepared when one was already in progress */
	virtual void DelayedPrepareMapChange();

	/** Called on client during seamless level transitions to get the list of Actors that should be moved into the new level
	 * PlayerControllers, Role < ROLE_Authority Actors, and any non-Actors that are inside an Actor that is in the list
	 * (i.e. Object.Outer == Actor in the list)
	 * are all automatically moved regardless of whether they're included here
	 * only dynamic actors in the PersistentLevel may be moved (this includes all actors spawned during gameplay)
	 * this is called for both parts of the transition because actors might change while in the middle (e.g. players might join or leave the game)
	 * @see also GameModeBase::GetSeamlessTravelActorList() (the function that's called on servers)
	 * @param bToEntry true if we are going from old level -> entry, false if we are going from entry -> new level
	 * @param ActorList (out) list of actors to maintain
	 */
	virtual void GetSeamlessTravelActorList(bool bToEntry, TArray<class AActor*>& ActorList);

	/** Called when seamless traveling and we are being replaced by the specified PC
	 * clean up any persistent state (post process chains on LocalPlayers, for example)
	 * (not called if PlayerController is the same for the from and to GameModes)
	 */
	virtual void SeamlessTravelTo(class APlayerController* NewPC);

	/** Called when seamless traveling and the specified PC is being replaced by this one
	 * copy over data that should persist
	 * (not called if PlayerController is the same for the from and to GameModes)
	 */
	virtual void SeamlessTravelFrom(class APlayerController* OldPC);

	/** 
	 * Called after this player controller has transitioned through seamless travel, but before that player is initialized
	 * This is called both when a new player controller is created, and when it is maintained
	 */
	virtual void PostSeamlessTravel();

	/** 
	 * Tell the client to enable or disable voice chat (not muting)
	 * @param bEnable enable or disable voice chat
	 */
	UFUNCTION(Reliable, Client)
	virtual void ClientEnableNetworkVoice(bool bEnable);

	/** Enable voice chat transmission */
	void StartTalking();

	/** Disable voice chat transmission */
	void StopTalking();

	/** 
	 * Toggle voice chat on and off
	 * @param bSpeaking enable or disable voice chat
	 */
	UFUNCTION(exec)
	virtual void ToggleSpeaking(bool bInSpeaking);

	/**
	 * Tells the client that the server has all the information it needs and that it
	 * is ok to start sending voice packets. The server will already send voice packets
	 * when this function is called, since it is set server side and then forwarded
	 *
	 * NOTE: This is done as an RPC instead of variable replication because ordering matters
	 */
	UFUNCTION(Reliable, Client)
	virtual void ClientVoiceHandshakeComplete();

	/**
	 * Tell the server to mute a player for this controller
	 * @param PlayerId player id to mute
	 */
	UFUNCTION(server, reliable, WithValidation)
	virtual void ServerMutePlayer(FUniqueNetIdRepl PlayerId);

	/**
	 * Tell the server to unmute a player for this controller
	 * @param PlayerId player id to unmute
	 */
	UFUNCTION(server, reliable, WithValidation )
	virtual void ServerUnmutePlayer(FUniqueNetIdRepl PlayerId);

	/**
	 * Tell the client to mute a player for this controller
	 * @param PlayerId player id to mute
	 */
	UFUNCTION(Reliable, Client)
	virtual void ClientMutePlayer(FUniqueNetIdRepl PlayerId);

	/**
	 * Tell the client to unmute a player for this controller
	 * @param PlayerId player id to unmute
	 */
	UFUNCTION(Reliable, Client)
	virtual void ClientUnmutePlayer(FUniqueNetIdRepl PlayerId);

	/**
	 * Mutes a remote player on the server and then tells the client to mute
	 *
	 * @param PlayerNetId the remote player to mute
	 */
	void GameplayMutePlayer(const FUniqueNetIdRepl& PlayerNetId);

	/**
	 * Unmutes a remote player on the server and then tells the client to unmute
	 *
	 * @param PlayerNetId the remote player to unmute
	 */
	void GameplayUnmutePlayer(const FUniqueNetIdRepl& PlayerNetId);

	/**
	 * Is the specified player muted by this controlling player
	 * for any reason (gameplay, system, etc), check voice interface IsMuted() for system mutes
	 *
	 * @param PlayerId potentially muted player
	 * @return true if player is muted, false otherwise
	 */
	virtual bool IsPlayerMuted(const class FUniqueNetId& PlayerId);

	/** Notification when a matinee director track starts or stops controlling the ViewTarget of this PlayerController */
	virtual void NotifyDirectorControl(bool bNowControlling, class AMatineeActor* CurrentMatinee);

	/** Console control commands, useful when remote debugging so you can't touch the console the normal way */
	UFUNCTION(exec)
	virtual void ConsoleKey(FKey Key);

	/** Sends a command to the console to execute if not shipping version */
	UFUNCTION(exec)
	virtual void SendToConsole(const FString& Command);

	/** Adds a location to the texture streaming system for the specified duration. */
	UFUNCTION(reliable, client, SealedEvent)
	void ClientAddTextureStreamingLoc(FVector InLoc, float Duration, bool bOverrideLocation);

	/** Tells client to cancel any pending map change. */
	UFUNCTION(Reliable, Client)
	void ClientCancelPendingMapChange();

	/** Set CurrentNetSpeed to the lower of its current value and Cap. */
	UFUNCTION(Reliable, Client)
	void ClientCapBandwidth(int32 Cap);

	/** Actually performs the level transition prepared by PrepareMapChange(). */
	UFUNCTION(Reliable, Client)	
	void ClientCommitMapChange();

	/** Tells the client to block until all pending level streaming actions are complete
	 * happens at the end of the tick
	 * primarily used to force update the client ASAP at join time
	 */
	UFUNCTION(reliable, client, SealedEvent)
	void ClientFlushLevelStreaming();

	/** Forces GC at the end of the tick on the client */
	UFUNCTION(Reliable, Client)
	void ClientForceGarbageCollection();

	/** 
	 * Replicated function called by GameHasEnded().
	 * @param	EndGameFocus - actor to view with camera
	 * @param	bIsWinner - true if this controller is on winning team
	 */
	UFUNCTION(Reliable, Client)
	void ClientGameEnded(class AActor* EndGameFocus, bool bIsWinner);

	/** 
	 * Server uses this to force client into NewState .
	 * @Note ALL STATE NAMES NEED TO BE DEFINED IN name table in UnrealNames.h to be correctly replicated (so they are mapped to the same thing on client and server).
	 */
	UFUNCTION(Reliable, Client)
	void ClientGotoState(FName NewState);

	/** calls IgnoreLookInput on client */
	UFUNCTION(Reliable, Client)
	void ClientIgnoreLookInput(bool bIgnore);

	/** calls IgnoreMoveInput on client */
	UFUNCTION(Reliable, Client)
	void ClientIgnoreMoveInput(bool bIgnore);

	/**
	 * Outputs a message to HUD
	 * @param S - message to display
	 * @param Type - @todo document
	 * @param MsgLifeTime - Optional length of time to display 0 = default time
	 */
	UFUNCTION(Reliable, Client)
	void ClientMessage(const FString& S, FName Type = NAME_None, float MsgLifeTime = 0.f);

	/** Play the indicated CameraAnim on this camera.
	 * @param AnimToPlay - Camera animation to play
	 * @param Scale - "Intensity" scalar.  This is the scale at which the anim was first played.
	 * @param Rate -  Multiplier for playback rate.  1.0 = normal.
	 * @param BlendInTime - Time to interpolate in from zero, for smooth starts
	 * @param BlendOutTime - Time to interpolate out to zero, for smooth finishes
	 * @param bLoop - True if the animation should loop, false otherwise
	 * @param bRandomStartTime - Whether or not to choose a random time to start playing.  Only really makes sense for bLoop = true
	 * @param Space - Animation play area
	 * @param CustomPlaySpace - Matrix used when Space = CAPS_UserDefined
	 */
	UFUNCTION(unreliable, client, BlueprintCallable, Category = "Game|Feedback")
	void ClientPlayCameraAnim(class UCameraAnim* AnimToPlay, float Scale=1.f, float Rate=1.f, float BlendInTime=0.f, float BlendOutTime=0.f, bool bLoop=false, bool bRandomStartTime=false, ECameraAnimPlaySpace::Type Space=ECameraAnimPlaySpace::CameraLocal, FRotator CustomPlaySpace=FRotator::ZeroRotator);

	/** 
	 * Play Camera Shake 
	 * @param Shake - Camera shake animation to play
	 * @param Scale - Scalar defining how "intense" to play the anim
	 * @param PlaySpace - Which coordinate system to play the shake in (used for CameraAnims within the shake).
	 * @param UserPlaySpaceRot - Matrix used when PlaySpace = CAPS_UserDefined
	 */
	UFUNCTION(unreliable, client, BlueprintCallable, Category="Game|Feedback")
	void ClientPlayCameraShake(TSubclassOf<class UCameraShake> Shake, float Scale = 1.f, ECameraAnimPlaySpace::Type PlaySpace = ECameraAnimPlaySpace::CameraLocal, FRotator UserPlaySpaceRot = FRotator::ZeroRotator);

	/**
	 * Play sound client-side (so only the client will hear it)
	 * @param Sound	- Sound to play
	 * @param VolumeMultiplier - Volume multiplier to apply to the sound
	 * @param PitchMultiplier - Pitch multiplier to apply to the sound
	 */
	UFUNCTION(unreliable, client)
	void ClientPlaySound(class USoundBase* Sound, float VolumeMultiplier = 1.f, float PitchMultiplier = 1.f );

	/**
	 * Play sound client-side at the specified location
	 * @param Sound	- Sound to play
	 * @param Location - Location to play the sound at
	 * @param VolumeMultiplier - Volume multiplier to apply to the sound
	 * @param PitchMultiplier - Pitch multiplier to apply to the sound
	 */
	UFUNCTION(unreliable, client)
	void ClientPlaySoundAtLocation(class USoundBase* Sound, FVector Location, float VolumeMultiplier = 1.f, float PitchMultiplier = 1.f);

	/** Asynchronously loads the given level in preparation for a streaming map transition.
	 * the server sends one function per level name since dynamic arrays can't be replicated
	 * @param LevelNames - the names of the level packages to load. LevelNames[0] will be the new persistent (primary) level
	 * @param bFirst - whether this is the first item in the list (so clear the list first)
	 * @param bLast - whether this is the last item in the list (so start preparing the change after receiving it)
	 */
	UFUNCTION(Reliable, Client)
	void ClientPrepareMapChange(FName LevelName, bool bFirst, bool bLast);

	/**
	 * Forces the streaming system to disregard the normal logic for the specified duration and
	 * instead always load all mip-levels for all textures used by the specified actor.
	 * @param ForcedActor		- The actor whose textures should be forced into memory.
	 * @param ForceDuration		- Number of seconds to keep all mip-levels in memory, disregarding the normal priority logic.
	 * @param bEnableStreaming	- Whether to start (true) or stop (false) streaming
	 * @param CinematicTextureGroups	- Bitfield indicating which texture groups that use extra high-resolution mips
	 */
	UFUNCTION(Reliable, Client)
	void ClientPrestreamTextures(class AActor* ForcedActor, float ForceDuration, bool bEnableStreaming, int32 CinematicTextureGroups = 0);

	/** Tell client to reset the PlayerController */
	UFUNCTION(Reliable, Client)
	void ClientReset();

	/** Tell client to restart the level */
	UFUNCTION(Reliable, Client)
	void ClientRestart(class APawn* NewPawn);

	/** 
	 * Tells the client to block until all pending level streaming actions are complete.
	 * Happens at the end of the tick primarily used to force update the client ASAP at join time.
	 */
	UFUNCTION(Reliable, Client)
	void ClientSetBlockOnAsyncLoading();

	/** Tell client to fade camera
	 * @Param bEnableFading - true if we should apply FadeColor/FadeAmount to the screen
	 * @Param FadeColor - Color to fade to
	 * @Param FadeAlpha - Amount of fading to apply
	 * @Param FadeTime - length of time for fade to occur over
	 * @Param bFadeAudio - true to apply fading of audio alongside the video
	 */
	UFUNCTION(Reliable, Client)
	void ClientSetCameraFade(bool bEnableFading, FColor FadeColor = FColor(ForceInit), FVector2D FadeAlpha = FVector2D(ForceInit), float FadeTime = 0, bool bFadeAudio = false);

	/**
	 * Replicated function to set camera style on client
	 * @param	NewCamMode, name defining the new camera mode
	 */
	UFUNCTION(Reliable, Client)
	void ClientSetCameraMode(FName NewCamMode);

	/** Called by the server to synchronize cinematic transitions with the client */
	UFUNCTION(Reliable, Client)
	void ClientSetCinematicMode(bool bInCinematicMode, bool bAffectsMovement, bool bAffectsTurning, bool bAffectsHUD);

	/**
	 * Forces the streaming system to disregard the normal logic for the specified duration and
	 * instead always load all mip-levels for all textures used by the specified material.
	 *
	 * @param Material		- The material whose textures should be forced into memory.
	 * @param ForceDuration	- Number of seconds to keep all mip-levels in memory, disregarding the normal priority logic.
	 * @param CinematicTextureGroups	- Bitfield indicating which texture groups that use extra high-resolution mips
	 */
	UFUNCTION(Reliable, Client)
	void ClientSetForceMipLevelsToBeResident(class UMaterialInterface* Material, float ForceDuration, int32 CinematicTextureGroups = 0);

	/** Set the client's class of HUD and spawns a new instance of it. If there was already a HUD active, it is destroyed. */
	UFUNCTION(BlueprintCallable, Category="HUD", Reliable, Client)
	void ClientSetHUD(TSubclassOf<class AHUD> NewHUDClass);

	/** Helper to get the size of the HUD canvas for this player controller.  Returns 0 if there is no HUD */
	UFUNCTION(BlueprintCallable, Category="HUD")
	void GetViewportSize(int32& SizeX, int32& SizeY) const;

	/** Gets the HUD currently being used by this player controller */
	UFUNCTION(BlueprintCallable, Category="HUD")
	class AHUD* GetHUD() const;

	/**
	 * Sets the Widget for the Mouse Cursor to display 
	 * @param Cursor - the cursor to set the widget for
	 * @param CursorWidget - the widget to set the cursor to
	 */
	UFUNCTION(BlueprintCallable, Category = "UI")
	void SetMouseCursorWidget(EMouseCursor::Type Cursor, class UUserWidget* CursorWidget);

	/** Set the view target
	 * @param A - new actor to set as view target
	 * @param TransitionParams - parameters to use for controlling the transition
	 */
	UFUNCTION(Reliable, Client)
	void ClientSetViewTarget(class AActor* A, struct FViewTargetTransitionParams TransitionParams = FViewTargetTransitionParams());

	/** Spawn a camera lens effect (e.g. blood). */
	UFUNCTION(unreliable, client, BlueprintCallable, Category="Game|Feedback")
	void ClientSpawnCameraLensEffect(TSubclassOf<class AEmitterCameraLensEffectBase>  LensEffectEmitterClass);

	/** Removes all Camera Lens Effects. */
	UFUNCTION(reliable, client, BlueprintCallable, Category="Game|Feedback")
	virtual void ClientClearCameraLensEffects();

	/** Stop camera animation on client. */
	UFUNCTION(Reliable, Client)
	void ClientStopCameraAnim(class UCameraAnim* AnimToStop);

	/** Stop camera shake on client.  */
	UFUNCTION(reliable, client, BlueprintCallable, Category="Game|Feedback")
	void ClientStopCameraShake(TSubclassOf<class UCameraShake> Shake, bool bImmediately = true);

	/** 
	 * Play a force feedback pattern on the player's controller
	 * @param	ForceFeedbackEffect		The force feedback pattern to play
	 * @param	bLooping				Whether the pattern should be played repeatedly or be a single one shot
	 * @param	bIgnoreTimeDilation		Whether the pattern should ignore time dilation
	 * @param	Tag						A tag that allows stopping of an effect.  If another effect with this Tag is playing, it will be stopped and replaced
	 */
	UFUNCTION(unreliable, client, BlueprintCallable, Category="Game|Feedback")
	void ClientPlayForceFeedback(class UForceFeedbackEffect* ForceFeedbackEffect, bool bLooping, bool bIgnoreTimeDilation, FName Tag);

	DEPRECATED(4.18, "Use version that specifies whether to ignore time dilation or not")
	void ClientPlayForceFeedback(class UForceFeedbackEffect* ForceFeedbackEffect, bool bLooping, FName Tag)
	{
		ClientPlayForceFeedback(ForceFeedbackEffect, bLooping, false, Tag);
	}

	/** 
	 * Stops a playing force feedback pattern
	 * @param	ForceFeedbackEffect		If set only patterns from that effect will be stopped
	 * @param	Tag						If not none only the pattern with this tag will be stopped
	 */
	UFUNCTION(reliable, client, BlueprintCallable, Category="Game|Feedback")
	void ClientStopForceFeedback(class UForceFeedbackEffect* ForceFeedbackEffect, FName Tag);

	/** 
	 * Latent action that controls the playing of force feedback 
	 * Begins playing when Start is called.  Calling Update or Stop if the feedback is not active will have no effect.
	 * Completed will execute when Stop is called or the duration ends.
	 * When Update is called the Intensity, Duration, and affect values will be updated with the current inputs
	 * @param	Intensity				How strong the feedback should be.  Valid values are between 0.0 and 1.0
	 * @param	Duration				How long the feedback should play for.  If the value is negative it will play until stopped
	 * @param   bAffectsLeftLarge		Whether the intensity should be applied to the large left servo
	 * @param   bAffectsLeftSmall		Whether the intensity should be applied to the small left servo
	 * @param   bAffectsRightLarge		Whether the intensity should be applied to the large right servo
	 * @param   bAffectsRightSmall		Whether the intensity should be applied to the small right servo
	 */
	UFUNCTION(BlueprintCallable, meta=(Latent, LatentInfo="LatentInfo", ExpandEnumAsExecs="Action", Duration="-1", bAffectsLeftLarge="true", bAffectsLeftSmall="true", bAffectsRightLarge="true", bAffectsRightSmall="true", AdvancedDisplay="bAffectsLeftLarge,bAffectsLeftSmall,bAffectsRightLarge,bAffectsRightSmall"), Category="Game|Feedback")
	void PlayDynamicForceFeedback(float Intensity, float Duration, bool bAffectsLeftLarge, bool bAffectsLeftSmall, bool bAffectsRightLarge, bool bAffectsRightSmall, TEnumAsByte<EDynamicForceFeedbackAction::Type> Action, FLatentActionInfo LatentInfo);

	/**
	* Play a haptic feedback curve on the player's controller
	* @param	HapticEffect			The haptic effect to play
	* @param	Hand					Which hand to play the effect on
	* @param	Scale					Scale between 0.0 and 1.0 on the intensity of playback
	*/
	UFUNCTION(BlueprintCallable, Category = "Game|Feedback")
	void PlayHapticEffect(class UHapticFeedbackEffect_Base* HapticEffect, EControllerHand Hand, float Scale = 1.f,  bool bLoop = false);

	/**
	* Stops a playing haptic feedback curve
	* @param	HapticEffect			The haptic effect to stop
	* @param	Hand					Which hand to stop the effect for
	*/
	UFUNCTION(BlueprintCallable, Category = "Game|Feedback")
	void StopHapticEffect(EControllerHand Hand);

	/**
	* Sets the value of the haptics for the specified hand directly, using frequency and amplitude.  NOTE:  If a curve is already
	* playing for this hand, it will be cancelled in favour of the specified values.
	*
	* @param	Frequency				The normalized frequency [0.0, 1.0] to play through the haptics system
	* @param	Amplitude				The normalized amplitude [0.0, 1.0] to set the haptic feedback to
	* @param	Hand					Which hand to play the effect on
	*/
	UFUNCTION(BlueprintCallable, Category = "Game|Feedback")
	void SetHapticsByValue(const float Frequency, const float Amplitude, EControllerHand Hand);
	
	/**
	* Sets the light color of the player's controller
	* @param	Color					The color for the light to be
	*/
	UFUNCTION(BlueprintCallable, Category = "Game|Feedback")
	void SetControllerLightColor(FColor Color);

	/**
	 * Travel to a different map or IP address. Calls the PreClientTravel event before doing anything.
	 * NOTE: This is implemented as a locally executed wrapper for ClientTravelInternal, to avoid API compatability breakage
	 *
	 * @param URL				A string containing the mapname (or IP address) to travel to, along with option key/value pairs
	 * @param TravelType 		specifies whether the client should append URL options used in previous travels; if true is specified
	 *							for the bSeamlesss parameter, this value must be TRAVEL_Relative.
	 * @param bSeamless			Indicates whether to use seamless travel (requires TravelType of TRAVEL_Relative)
	 * @param MapPackageGuid	The GUID of the map package to travel to - this is used to find the file when it has been autodownloaded,
	 * 							so it is only needed for clients
	 */
	UFUNCTION()
	void ClientTravel(const FString& URL, enum ETravelType TravelType, bool bSeamless = false, FGuid MapPackageGuid = FGuid());

	/**
	 * Internal clientside implementation of ClientTravel - use ClientTravel to call this
	 *
	 * @param URL				A string containing the mapname (or IP address) to travel to, along with option key/value pairs
	 * @param TravelType 		specifies whether the client should append URL options used in previous travels; if true is specified
	 *							for the bSeamlesss parameter, this value must be TRAVEL_Relative.
	 * @param bSeamless			Indicates whether to use seamless travel (requires TravelType of TRAVEL_Relative)
	 * @param MapPackageGuid	The GUID of the map package to travel to - this is used to find the file when it has been autodownloaded,
	 * 							so it is only needed for clients
	 */
	UFUNCTION(Reliable, Client)
	void ClientTravelInternal(const FString& URL, enum ETravelType TravelType, bool bSeamless = false, FGuid MapPackageGuid = FGuid());

	/**
	 * Replicated Update streaming status
	 * @param PackageName - Name of the level package name used for loading.
	 * @param bNewShouldBeLoaded - Whether the level should be loaded
	 * @param bNewShouldBeVisible - Whether the level should be visible if it is loaded	
	 * @param bNewShouldBlockOnLoad - Whether we want to force a blocking load
	 * @param LODIndex				- Current LOD index for a streaming level
	 */
	UFUNCTION(Reliable, Client)
	void ClientUpdateLevelStreamingStatus(FName PackageName, bool bNewShouldBeLoaded, bool bNewShouldBeVisible, bool bNewShouldBlockOnLoad, int32 LODIndex);

	/** Notify client they were kicked from the server */
	UFUNCTION(Reliable, Client)
	void ClientWasKicked(const FText& KickReason);

	/** Notify client that the session is starting */
	UFUNCTION(Reliable, Client)
	void ClientStartOnlineSession();

	/** Notify client that the session is about to start */
	UFUNCTION(Reliable, Client)
	void ClientEndOnlineSession();

	/** Assign Pawn to player, but avoid calling ClientRestart if we have already accepted this pawn */
	UFUNCTION(Reliable, Client)
	void ClientRetryClientRestart(class APawn* NewPawn);

	/** Call ClientRetryClientRestart, but only if the current pawn is not the currently acknowledged pawn (and throttled to avoid saturating the network). */
	virtual void SafeRetryClientRestart();

	/** send client localized message id */
	UFUNCTION(Reliable, Client)
	void ClientReceiveLocalizedMessage(TSubclassOf<ULocalMessage> Message, int32 Switch = 0, class APlayerState* RelatedPlayerState_1 = NULL, class APlayerState* RelatedPlayerState_2 = NULL, class UObject* OptionalObject = NULL);

	/** acknowledge possession of pawn */
	UFUNCTION(reliable, server, WithValidation)
	void ServerAcknowledgePossession(class APawn* P);

	/** change mode of camera */
	UFUNCTION(reliable, server, WithValidation)
	void ServerCamera(FName NewMode);

	/** Change name of server */
	UFUNCTION(reliable, server, WithValidation)
	void ServerChangeName(const FString& S);

	/** 
	 * Called to notify the server when the client has loaded a new world via seamless traveling
	 * @param WorldPackageName the name of the world package that was loaded
	 */
	UFUNCTION(reliable, server, WithValidation, SealedEvent)
	void ServerNotifyLoadedWorld(FName WorldPackageName);

	/** Replicate pause request to the server */
	UFUNCTION(reliable, server, WithValidation)
	void ServerPause();

	/** Attempts to restart this player, generally called from the client upon respawn request. */
	UFUNCTION(reliable, server, WithValidation)
	void ServerRestartPlayer();

	/** When spectating, updates spectator location/rotation and pings the server to make sure spectating should continue. */
	UFUNCTION(unreliable, server, WithValidation)
	void ServerSetSpectatorLocation(FVector NewLoc, FRotator NewRot);

	/** Calls ServerSetSpectatorLocation but throttles it to reduce bandwidth and only calls it when necessary. */
	void SafeServerUpdateSpectatorState();

	/** Tells the server to make sure the possessed pawn is in sync with the client. */
	UFUNCTION(unreliable, server, WithValidation)
	void ServerCheckClientPossession();

	/** Reliable version of ServerCheckClientPossession to be used when there is no likely danger of spamming the network. */
	UFUNCTION(reliable, server, WithValidation)
	void ServerCheckClientPossessionReliable();

	/** Call ServerCheckClientPossession on the server, but only if the current pawn is not the acknowledged pawn (and throttled to avoid saturating the network). */
	virtual void SafeServerCheckClientPossession();

	/** Notifies the server that the client has ticked gameplay code, and should no longer get the extended "still loading" timeout grace period */
	UFUNCTION(reliable, server, WithValidation)
	void ServerShortTimeout();

	/** If PlayerCamera.bUseClientSideCameraUpdates is set, client will replicate camera positions to the server. */
	// @TODO - combine pitch/yaw into one int, maybe also send location compressed
	UFUNCTION(unreliable, server, WithValidation)
	void ServerUpdateCamera(FVector_NetQuantize CamLoc, int32 CamPitchAndYaw);

	/** 
	 * Called when the client adds/removes a streamed level
	 * the server will only replicate references to Actors in visible levels so that it's impossible to send references to
	 * Actors the client has not initialized
	 * @param PackageName the name of the package for the level whose status changed
	 */
	UFUNCTION(reliable, server, WithValidation, SealedEvent)
	void ServerUpdateLevelVisibility(FName PackageName, bool bIsVisible);

	/** Used by client to request server to confirm current viewtarget (server will respond with ClientSetViewTarget() ). */
	UFUNCTION(reliable, server, WithValidation)
	void ServerVerifyViewTarget();

	/** Move camera to next player on round ended or spectating*/
	UFUNCTION(unreliable, server, WithValidation)
	void ServerViewNextPlayer();

	/** Move camera to previous player on round ended or spectating */
	UFUNCTION(unreliable, server, WithValidation)
	void ServerViewPrevPlayer();

	/** Move camera to current user */
	UFUNCTION(unreliable, server, WithValidation)
	void ServerViewSelf(struct FViewTargetTransitionParams TransitionParams = FViewTargetTransitionParams());

	/** @todo document */
	UFUNCTION(Reliable, Client)
	void ClientTeamMessage(class APlayerState* SenderPlayerState, const FString& S, FName Type, float MsgLifeTime = 0);

	/** Used by UGameplayDebuggingControllerComponent to replicate messages for AI debugging in network games. */
	UFUNCTION(reliable, server, WithValidation)
	void ServerToggleAILogging();

	/**
	 * Add Pitch (look up) input. This value is multiplied by InputPitchScale.
	 * @param Val Amount to add to Pitch. This value is multiplied by InputPitchScale.
	 */
	UFUNCTION(BlueprintCallable, Category="Game|Player", meta=(Keywords="up down"))
	virtual void AddPitchInput(float Val);

	/**
	 * Add Yaw (turn) input. This value is multiplied by InputYawScale.
	 * @param Val Amount to add to Yaw. This value is multiplied by InputYawScale.
	 */
	UFUNCTION(BlueprintCallable, Category="Game|Player", meta=(Keywords="left right turn"))
	virtual void AddYawInput(float Val);

	/**
	 * Add Roll input. This value is multiplied by InputRollScale.
	 * @param Val Amount to add to Roll. This value is multiplied by InputRollScale.
	 */
	UFUNCTION(BlueprintCallable, Category="Game|Player")
	virtual void AddRollInput(float Val);

	/** Returns true if the given key/button is pressed on the input of the controller (if present) */
	UFUNCTION(BlueprintCallable, Category="Game|Player")
	bool IsInputKeyDown(FKey Key) const;

	/** Returns true if the given key/button was up last frame and down this frame. */
	UFUNCTION(BlueprintCallable, Category="Game|Player")
	bool WasInputKeyJustPressed(FKey Key) const;

	/** Returns true if the given key/button was down last frame and up this frame. */
	UFUNCTION(BlueprintCallable, Category="Game|Player")
	bool WasInputKeyJustReleased(FKey Key) const;

	/** Returns the analog value for the given key/button.  If analog isn't supported, returns 1 for down and 0 for up. */
	UFUNCTION(BlueprintCallable, Category="Game|Player")
	float GetInputAnalogKeyState(FKey Key) const;

	/** Returns the vector value for the given key/button. */
	UFUNCTION(BlueprintCallable, Category="Game|Player")
	FVector GetInputVectorKeyState(FKey Key) const;

	/** Retrieves the X and Y screen coordinates of the specified touch key. Returns false if the touch index is not down */
	UFUNCTION(BlueprintCallable, Category="Game|Player")
	void GetInputTouchState(ETouchIndex::Type FingerIndex, float& LocationX, float& LocationY, bool& bIsCurrentlyPressed) const;

	/** Retrieves the current motion state of the player's input device */
	UFUNCTION(BlueprintCallable, Category="Game|Player")
	void GetInputMotionState(FVector& Tilt, FVector& RotationRate, FVector& Gravity, FVector& Acceleration) const;

	/** Retrieves the X and Y screen coordinates of the mouse cursor. Returns false if there is no associated mouse device */
	UFUNCTION(BlueprintCallable, Category="Game|Player")
	bool GetMousePosition(float& LocationX, float& LocationY) const;

	/** Returns how long the given key/button has been down.  Returns 0 if it's up or it just went down this frame. */
	UFUNCTION(BlueprintCallable, Category="Game|Player")
	float GetInputKeyTimeDown(FKey Key) const;

	/** Retrieves how far the mouse moved this frame. */
	UFUNCTION(BlueprintCallable, Category="Game|Player")
	void GetInputMouseDelta(float& DeltaX, float& DeltaY) const;

	/** Retrieves the X and Y displacement of the given analog stick. */
	UFUNCTION(BlueprintCallable, Category="Game|Player")
	void GetInputAnalogStickState(EControllerAnalogStick::Type WhichStick, float& StickX, float& StickY) const;

	/** Activates a new touch interface for this player controller */
	UFUNCTION(BlueprintCallable, Category="Game|Player")
	virtual void ActivateTouchInterface(class UTouchInterface* NewTouchInterface);

	/** Set the virtual joystick visibility. */
	UFUNCTION(BlueprintCallable, Category = "Game|Player")
	virtual void SetVirtualJoystickVisibility(bool bVisible);

public:

	/** Setup an input mode. */
	virtual void SetInputMode(const FInputModeDataBase& InData);

public:
	/**
	 * Change Camera mode
	 * @param	New camera mode to set
	 */
	UFUNCTION(exec)
	virtual void Camera(FName NewMode);

	/** Set the view target blending with variable control
	 * @param NewViewTarget - new actor to set as view target
	 * @param BlendTime - time taken to blend
	 * @param BlendFunc - Cubic, Linear etc functions for blending
	 * @param BlendExp -  Exponent, used by certain blend functions to control the shape of the curve. 
	 * @param bLockOutgoing - If true, lock outgoing viewtarget to last frame's camera position for the remainder of the blend.
	 */
	UFUNCTION(BlueprintCallable, Category="Game|Player", meta=(Keywords = "Camera"))
	virtual void SetViewTargetWithBlend(class AActor* NewViewTarget, float BlendTime = 0, enum EViewTargetBlendFunction BlendFunc = VTBlend_Linear, float BlendExp = 0, bool bLockOutgoing = false);

protected:
	/** Clickable object currently under the mouse cursor. */
	TWeakObjectPtr<UPrimitiveComponent> CurrentClickablePrimitive;

	/** Touchable objects currently under fingers. */
	TWeakObjectPtr<UPrimitiveComponent> CurrentTouchablePrimitives[EKeys::NUM_TOUCH_KEYS];

	/** Internal.  Current stack of InputComponents. */
	TArray< TWeakObjectPtr<UInputComponent> > CurrentInputStack;
	
	/** InputComponent we use when player is in Inactive state. */
	UPROPERTY()
	UInputComponent* InactiveStateInputComponent;

	/** Sets up input bindings for the input component pushed on the stack in the inactive state. */
	virtual void SetupInactiveStateInputComponent(UInputComponent* InComponent);

	/** Refresh state specific input components */
	virtual void UpdateStateInputComponents();

	/** The state of the inputs from cinematic mode */
	uint32 bCinemaDisableInputMove:1;
	uint32 bCinemaDisableInputLook:1;

private:
	/* Whether the PlayerController's input handling is enabled. */
	uint32 bInputEnabled:1;

protected:

	/** Whether we fully tick when the game is paused, if our tick function is allowed to do so. If false, we do a minimal update during the tick. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=PlayerController)
	uint32 bShouldPerformFullTickWhenPaused:1;

	/** The virtual touch interface */
	TSharedPtr<class SVirtualJoystick> VirtualJoystick;

	/** The currently set touch interface */
	UPROPERTY()
	class UTouchInterface* CurrentTouchInterface;

	/** Handle for efficient management of UnFreeze timer */
	FTimerHandle TimerHandle_UnFreeze;

private:
	/** Handle for efficient management of DelayedPrepareMapChange timer */
	FTimerHandle TimerHandle_DelayedPrepareMapChange;

	/** Handle for efficient management of ClientCommitMapChange timer */
	FTimerHandle TimerHandle_ClientCommitMapChange;

public:
	/** Adds an inputcomponent to the top of the input stack. */
	void PushInputComponent(UInputComponent* Input);

	/** Removes given inputcomponent from the input stack (regardless of if it's the top, actually). */
	bool PopInputComponent(UInputComponent* Input);

	virtual void FlushPressedKeys();

	virtual bool InputKey(FKey Key, EInputEvent EventType, float AmountDepressed, bool bGamepad);
	virtual bool InputTouch(uint32 Handle, ETouchType::Type Type, const FVector2D& TouchLocation, FDateTime DeviceTimestamp, uint32 TouchpadIndex);
	virtual bool InputAxis(FKey Key, float Delta, float DeltaTime, int32 NumSamples, bool bGamepad);
	virtual bool InputMotion(const FVector& Tilt, const FVector& RotationRate, const FVector& Gravity, const FVector& Acceleration);

	/** Associate a new UPlayer with this PlayerController. */
	virtual void SetPlayer(UPlayer* InPlayer);

	/** Returns the ULocalPlayer for this controller if it exists, or null otherwise */
	class ULocalPlayer* GetLocalPlayer() const;

	/**
	 * Called client-side to smoothly interpolate received TargetViewRotation (result is in BlendedTargetViewRotation)
	 * @param TargetPawn   is the pawn which is the current ViewTarget
	 * @param DeltaSeconds is the time interval since the last smoothing update
	 */
	virtual void SmoothTargetViewRotation(APawn* TargetPawn, float DeltaSeconds);

	/**
	 * Executes the Exec() command on the UPlayer object
	 *
	 * @param Command command to execute (string of commands optionally separated by a | (pipe))
	 * @param bWriteToLog write out to the log
	 */
	virtual FString ConsoleCommand(const FString& Command, bool bWriteToLog = true);

	//~ Begin UObject Interface
	virtual void PostLoad() override;
	//~ End UObject Interface

	//~ Begin AActor Interface
	virtual void GetActorEyesViewPoint(FVector& Location, FRotator& Rotation) const override;
	virtual void CalcCamera(float DeltaTime, struct FMinimalViewInfo& OutResult) override;
	virtual void TickActor(float DeltaTime, enum ELevelTick TickType, FActorTickFunction& ThisTickFunction) override;
	virtual bool IsNetRelevantFor(const AActor* RealViewer, const AActor* ViewTarget, const FVector& SrcLocation) const override;
	virtual void FellOutOfWorld(const class UDamageType& dmgType) override;
	virtual void Reset() override;
	virtual void Possess(APawn* aPawn) override;
	virtual void UnPossess() override;
	virtual void CleanupPlayerState() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Destroyed() override;
	virtual void OnActorChannelOpen(class FInBunch& InBunch, class UNetConnection* Connection) override;
	virtual bool UseShortConnectTimeout() const override;
	virtual void OnSerializeNewActor(class FOutBunch& OutBunch) override;
	virtual void OnNetCleanup(class UNetConnection* Connection) override;
	virtual float GetNetPriority(const FVector& ViewPos, const FVector& ViewDir, AActor* Viewer, AActor* ViewTarget, UActorChannel* InChannel, float Time, bool bLowBandwidth) override;
	virtual const AActor* GetNetOwner() const override;
	virtual class UPlayer* GetNetOwningPlayer() override;
	virtual class UNetConnection* GetNetConnection() const override;
	virtual bool DestroyNetworkActorHandled() override;
	virtual void DisplayDebug(class UCanvas* Canvas, const FDebugDisplayInfo& DebugDisplay, float& YL, float& YPos) override;
	virtual void PostInitializeComponents() override;
	virtual void EnableInput(class APlayerController* PlayerController) override;
	virtual void DisableInput(class APlayerController* PlayerController) override;
protected:
	virtual void BeginPlay() override;
	//~ End AActor Interface

public:
	//~ Begin AController Interface
	virtual void GameHasEnded(class AActor* EndGameFocus = NULL, bool bIsWinner = false) override;
	virtual bool IsLocalController() const override;
	virtual void GetPlayerViewPoint(FVector& out_Location, FRotator& out_Rotation) const override;
	virtual void SetInitialLocationAndRotation(const FVector& NewLocation, const FRotator& NewRotation) override;
	virtual void ChangeState(FName NewState) override;
	virtual class AActor* GetViewTarget() const override;
	virtual void BeginInactiveState() override;
	virtual void EndInactiveState() override;
	virtual void FailedToSpawnPawn() override;
	virtual void SetPawn(APawn* InPawn) override;
	//~ End AController Interface

	virtual void GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const override;

	/** called on the server when the client sends a message indicating it was unable to initialize an Actor channel,
	 * most commonly because the desired Actor's archetype couldn't be serialized
	 * the default is to do nothing (Actor simply won't exist on the client), but this function gives the game code
	 * an opportunity to try to correct the problem
	 */
	virtual void NotifyActorChannelFailure(UActorChannel* ActorChan) {}

	/**
	 * Builds a list of actors that are hidden based upon gameplay
	 * @param ViewLocation the view point to hide/unhide from
	 */
	virtual void UpdateHiddenActors(const FVector& ViewLocation) {}

	/**
	 * Builds a list of components that are hidden based upon gameplay
	 * @param ViewLocation the view point to hide/unhide from
	 * @param HiddenComponents the list to add to/remove from
	 */
	virtual void UpdateHiddenComponents(const FVector& ViewLocation, TSet<FPrimitiveComponentId>& /*HiddenComponents*/) {}

	/**
	 * Builds a list of components that are hidden based upon gameplay.
	 * This calls both UpdateHiddenActors and UpdateHiddenComponents, merging the two lists.
	 * @param ViewLocation the view point to hide/unhide from
	 * @param HiddenComponents this list will have all components that should be hidden added to it
	 */
	void BuildHiddenComponentList(const FVector& ViewLocation, TSet<FPrimitiveComponentId>& HiddenComponentsOut);

	/**
	 * Sets the Matinee director track instance that's currently possessing this player controller
	 * @param   NewControllingDirector    The director track instance that's now controlling this player controller (or NULL for none)
	 * @param	bClientSimulatingViewTarget	True to allow clients to simulate their own camera cuts (ignored if NewControllingDirector is NULL).
	 */
	void SetControllingDirector(UInterpTrackInstDirector* NewControllingDirector, bool bClientSimulatingViewTarget);

	/** Returns the Matinee director track that's currently possessing this player controller, or NULL for none */
	UInterpTrackInstDirector* GetControllingDirector();

	/** spawn cameras for servers and owning players */
	virtual void SpawnPlayerCameraManager();

	/** get audio listener position and orientation */
	virtual void GetAudioListenerPosition(FVector& OutLocation, FVector& OutFrontDir, FVector& OutRightDir);

	/**
	 * Used to override the default positioning of the audio listener
	 * 
	 * @param AttachToComponent Optional component to attach the audio listener to
	 * @param Location Depending on whether Component is attached this is either an offset from its location or an absolute position
	 * @param Rotation Depending on whether Component is attached this is either an offset from its rotation or an absolute rotation
	 */
	UFUNCTION(BlueprintCallable, Category="Game|Audio")
	void SetAudioListenerOverride(USceneComponent* AttachToComponent, FVector Location, FRotator Rotation);

	/**
	 * Clear any overrides that have been applied to audio listener
	 */
	UFUNCTION(BlueprintCallable, Category = "Game|Audio")
	void ClearAudioListenerOverride();

protected:
	/** Whether to override the normal audio listener positioning method */
	uint32 bOverrideAudioListener:1;
	/** Component that is currently driving the audio listener position/orientation */
	TWeakObjectPtr<USceneComponent> AudioListenerComponent;
	/** Currently overridden location of audio listener */
	FVector AudioListenerLocationOverride;
	/** Currently overridden rotation of audio listener */
	FRotator AudioListenerRotationOverride;

protected:
	/** Internal. */
	void TickPlayerInput(const float DeltaSeconds, const bool bGamePaused);
	virtual void ProcessPlayerInput(const float DeltaTime, const bool bGamePaused);
	virtual void BuildInputStack(TArray<UInputComponent*>& InputStack);
	void ProcessForceFeedbackAndHaptics(const float DeltaTime, const bool bGamePaused);
	virtual bool IsInViewportClient(UGameViewportClient* ViewportClient) const;

	virtual int32 GetInputIndex() const;

	/** Allows the PlayerController to set up custom input bindings. */
	virtual void SetupInputComponent();

public:
	/**
	 * Store the net speed 
	 * @param NewSpeed current speed of network
	 */
	void SetNetSpeed(int32 NewSpeed);

	/** 
	 * Get the local players network address
	 * @return the address
	 */
	FString GetPlayerNetworkAddress();

	/** 
	 * Get the server network address
	 * @return the adress
	 */
	FString GetServerNetworkAddress();

	/** Handles remapping a package name for networking, call on both the client and server when sending package names manually for RPCs */
	FName NetworkRemapPath(FName InPackageName, bool bReading);

	/** Clears out 'left-over' audio components. */
	virtual void CleanUpAudioComponents();

	/** Notifies the server that the client has ticked gameplay code, and should no longer get the extended "still loading" timeout grace period */
	virtual void AddCheats(bool bForce = false);

	/** Spawn a HUD (make sure that PlayerController always has valid HUD, even if ClientSetHUD() hasn't been called */
	virtual void SpawnDefaultHUD();

	/** Create the touch interface, and activate an initial touch interface (if touch interface is desired) */
	virtual void CreateTouchInterface();

	/** Gives the PlayerController an opportunity to cleanup any changes it applied to the game viewport, primarily for the touch interface */
	virtual void CleanupGameViewport();

	/** @Returns true if input should be frozen (whether UnFreeze timer is active) */
	virtual void AcknowledgePossession(class APawn* P);

	/** Clean up when a Pawn's player is leaving a game. Base implementation destroys the pawn. */
	virtual void PawnLeavingGame();

	/** Takes ping updates from the net driver (both clientside and serverside), and passes them on to PlayerState::UpdatePing */
	virtual void UpdatePing(float InPing);

	/**
	 * Get next active viewable player in PlayerArray.
	 * @param dir is the direction to go in the array
	 */
	virtual class APlayerState* GetNextViewablePlayer(int32 dir);

	/**
	 * View next active player in PlayerArray.
	 * @param dir is the direction to go in the array
	 */
	virtual void ViewAPlayer(int32 dir);

	/** @return true if this controller thinks it's able to restart. Called from GameModeBase::PlayerCanRestart */
	virtual bool CanRestartPlayer();

	/**
	 * Server/SP only function for changing whether the player is in cinematic mode.  Updates values of various state variables, then replicates the call to the client
	 * to sync the current cinematic mode.
	 * @param	bInCinematicMode	specify true if the player is entering cinematic mode; false if the player is leaving cinematic mode.
	 * @param	bHidePlayer			specify true to hide the player's pawn (only relevant if bInCinematicMode is true)
	 * @param	bAffectsHUD			specify true if we should show/hide the HUD to match the value of bCinematicMode
	 * @param	bAffectsMovement	specify true to disable movement in cinematic mode, enable it when leaving
	 * @param	bAffectsTurning		specify true to disable turning in cinematic mode or enable it when leaving
	 */
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic", meta=(bHidePlayer="true", bAffectsHUD="true"))
	virtual void SetCinematicMode(bool bInCinematicMode, bool bHidePlayer, bool bAffectsHUD, bool bAffectsMovement, bool bAffectsTurning);

	/**
	 * Determines whether this player is playing split-screen.
	 * @param	OutSplitscreenPlayerIndex	receives the index [into the player's local GamePlayers array] for this player, if playing splitscreen.
	 * @return	true if this player is playing splitscreen.
	 */
	bool IsSplitscreenPlayer(int32* OutSplitscreenPlayerIndex = NULL) const;

	/**
	 * Wrapper for determining whether this player is the first player on their console.
	 * @return	true if this player is not using splitscreen, or is the first player in the split-screen layout.
	 */
	bool IsPrimaryPlayer() const;

	/**
	 * Returns the PlayerState associated with the player at the specified index.
	 * @param	PlayerIndex		the index [into the local player's GamePlayers array] for the player PlayerState to find
	 * @return	the PlayerState associated with the player at the specified index, or None if the player is not a split-screen player or
	 *			the index was out of range.
	 */
	class APlayerState* GetSplitscreenPlayerByIndex(int32 PlayerIndex = 1) const;

	/**
	 * Returns the number of split-screen players playing on this player's machine.
	 * @return	the total number of players on the player's local machine, or 0 if this player isn't playing split-screen.
	 */
	int32 GetSplitscreenPlayerCount() const;

	/** Update the camera manager; this is called after all actors have been ticked.	 */ 
	virtual void UpdateCameraManager(float DeltaSeconds);

public:
	/**
	 * This function will be called to notify the player controller that the world has received its game class. In the case of a client
	 * we need to initialize the Input System here.
	 *
	 * @Param GameModeClass - The Class of the game that was replicated
	 */
	virtual void ReceivedGameModeClass(TSubclassOf<class AGameModeBase> GameModeClass);

	/** Notify the server that client data was received on the Pawn.
	 * @return true if InPawn is acknowledged on the server, false otherwise. */
	virtual bool NotifyServerReceivedClientData(APawn* InPawn, float TimeStamp);

	/** Start spectating mode, as the only mode allowed. */
	virtual void StartSpectatingOnly();

	/**
	 * Default implementation of pausing check for 'CanUnpause' delegates
	 * @return True if pausing is allowed
	 */
	virtual bool DefaultCanUnpause();

	/** @return true if game is currently paused. */
	bool IsPaused() const;

	bool InputEnabled() const { return bInputEnabled; }

	/** @return true if we fully tick when paused (and if our tick function is enabled when paused).	 */
	bool ShouldPerformFullTickWhenPaused() const;

	/** returns whether the client has completely loaded the server's current world (valid on server only) */
	bool HasClientLoadedCurrentWorld();

	/** forces a full replication check of the specified Actor on only the client that owns this PlayerController
	 * this function has no effect if this PC is not a remote client or if the Actor is not relevant to that client
	 */
	void ForceSingleNetUpdateFor(class AActor* Target);

	/** Set the view target
	 * @param A - new actor to set as view target
	 * @param TransitionParams - parameters to use for controlling the transition
	 */
	virtual void SetViewTarget(class AActor* NewViewTarget, FViewTargetTransitionParams TransitionParams = FViewTargetTransitionParams());

	/**
	 * If bAutoManageActiveCameraTarget is true, then automatically manage the active camera target.
	 * If there a CameraActor placed in the level with an auto-activate player assigned to it, that will be preferred, otherwise SuggestedTarget will be used.
	 */
	virtual void AutoManageActiveCameraTarget(AActor* SuggestedTarget);

	/**
	 * Notify from server that Visual Logger is recording, to show that information on client about possible performance issues 
	 */
	UFUNCTION(Reliable, Client)
	void OnServerStartedVisualLogger(bool bIsLogging);

protected:

	virtual ACameraActor* GetAutoActivateCameraForPlayer() const;

public:

	virtual bool ShouldShowMouseCursor() const;
	virtual EMouseCursor::Type GetMouseCursor() const;

public:
	// Spectating

	/** Get the Pawn used when spectating. NULL when not spectating. */
	UFUNCTION(BlueprintCallable, Category = "Pawn")
	class ASpectatorPawn* GetSpectatorPawn() const { return SpectatorPawn; }

	/** Returns the first of GetPawn() or GetSpectatorPawn() that is not NULL, or NULL otherwise. */
	class APawn* GetPawnOrSpectator() const;

	/** Called to notify the controller that the spectator class has been received. */
	virtual void ReceivedSpectatorClass(TSubclassOf<class ASpectatorPawn> SpectatorClass);

	/** Returns the location the PlayerController is focused on.
	 *  If there is a possessed Pawn, returns the Pawn's location.
	 *  If there is a spectator Pawn, returns that Pawn's location.
	 *  Otherwise, returns the PlayerController's spawn location (usually the last known Pawn location after it has died).
	 */
	UFUNCTION(BlueprintCallable, Category = "Pawn")
	virtual FVector GetFocalLocation() const;

protected:
	/** Event when spectating begins. */
	virtual void BeginSpectatingState();

	/** Event when no longer spectating. */
	virtual void EndSpectatingState();

	/** Set the spectator pawn. Will also call AttachToPawn() using the new spectator. */
	virtual void SetSpectatorPawn(class ASpectatorPawn* NewSpectatorPawn);

	/** Spawn a SpectatorPawn to use as a spectator and initialize it. By default it is spawned at the PC's current location and rotation. */
	virtual ASpectatorPawn* SpawnSpectatorPawn();

	/** Destroys the SpectatorPawn and sets it to NULL. */
	virtual void DestroySpectatorPawn();

private:
	/** The pawn used when spectating (NULL if not spectating). */
	UPROPERTY()
	class ASpectatorPawn* SpectatorPawn;

protected:
	/** The location used internally when there is no pawn or spectator, to know where to spawn the spectator or focus the camera on death. */
	UPROPERTY(Replicated)
	FVector SpawnLocation;

	/** Set the SpawnLocation for use when changing states or when there is no pawn or spectator. */
	virtual void SetSpawnLocation(const FVector& NewLocation);

public:

	// Get the location used when initially created, or when changing states when there is no pawn or spectator.
	FVector GetSpawnLocation() const { return SpawnLocation; }

public:
	/** Called after this PlayerController's viewport/net connection is associated with this player controller. */
	virtual void ReceivedPlayer();

	/** 
	 * Spawn the appropriate class of PlayerInput.
	 * Only called for playercontrollers that belong to local players.
	 */
	virtual void InitInputSystem();

	/** @returns true if input should be frozen (whether UnFreeze timer is active) */
	virtual bool IsFrozen();

	/**
	 * Called when the local player is about to travel to a new map or IP address.  Provides subclass with an opportunity
	 * to perform cleanup or other tasks prior to the travel.
	 */
	virtual void PreClientTravel(const FString& PendingURL, ETravelType TravelType, bool bIsSeamlessTravel);

	/** Set new camera mode */
	virtual void SetCameraMode(FName NewCamMode);

	/** Reset Camera Mode to default. */
	virtual void ResetCameraMode();

	/**
	 * Called on server at end of tick, to let client Pawns handle updates from the server.
	 * Done this way to avoid ever sending more than one ClientAdjustment per server tick.
	 */
	virtual void SendClientAdjustment();

	/**
	 * Designate this player controller as local (public for GameModeBase to use, not expected to be called anywhere else)
	 */
	void SetAsLocalPlayerController() { bIsLocalPlayerController = true; }

private:
	/** Used to delay calling ClientRestart() again when it hasn't been appropriately acknowledged. */
	float		LastRetryPlayerTime;

	/** Set during SpawnActor once and never again to indicate the intent of this controller instance (SERVER ONLY) */
	UPROPERTY()
	mutable bool bIsLocalPlayerController;

public:
	/** Counter for this players seamless travels (used along with the below value, to restrict ServerNotifyLoadedWorld) */
	UPROPERTY()
	uint16		SeamlessTravelCount;

	/** The value of SeamlessTravelCount, upon the last call to GameModeBase::HandleSeamlessTravelPlayer; used to detect seamless travel */
	UPROPERTY()
	uint16		LastCompletedSeamlessTravelCount;

};
