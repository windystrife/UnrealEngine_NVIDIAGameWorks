// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "Templates/SubclassOf.h"
#include "UObject/CoreNet.h"
#include "Engine/NetSerialization.h"
#include "Engine/EngineTypes.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "Animation/AnimationAsset.h"
#include "GameFramework/RootMotionSource.h"
#include "Character.generated.h"

class AController;
class FDebugDisplayInfo;
class UAnimMontage;
class UArrowComponent;
class UCapsuleComponent;
class UCharacterMovementComponent;
class UPawnMovementComponent;
class UPrimitiveComponent;
class USkeletalMeshComponent;
struct FAnimMontageInstance;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FMovementModeChangedSignature, class ACharacter*, Character, EMovementMode, PrevMovementMode, uint8, PreviousCustomMode);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FCharacterMovementUpdatedSignature, float, DeltaSeconds, FVector, OldLocation, FVector, OldVelocity);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FCharacterReachedApexSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FLandedSignature, const FHitResult&, Hit);

//
// Forward declarations
//
class UAnimMontage;
class UPrimitiveComponent;
struct FAnimMontageInstance;

/** Replicated data when playing a root motion montage. */
USTRUCT()
struct FRepRootMotionMontage
{
	GENERATED_USTRUCT_BODY()

	/** Whether this has useful/active data. */
	UPROPERTY()
	bool bIsActive;

	/** AnimMontage providing Root Motion */
	UPROPERTY()
	UAnimMontage* AnimMontage;

	/** Track position of Montage */
	UPROPERTY()
	float Position;

	/** Location */
	UPROPERTY()
	FVector_NetQuantize100 Location;

	/** Rotation */
	UPROPERTY()
	FRotator Rotation;

	/** Movement Relative to Base */
	UPROPERTY()
	UPrimitiveComponent* MovementBase;

	/** Bone on the MovementBase, if a skeletal mesh. */
	UPROPERTY()
	FName MovementBaseBoneName;

	/** Additional replicated flag, if MovementBase can't be resolved on the client. So we don't use wrong data. */
	UPROPERTY()
	bool bRelativePosition;

	/** Whether rotation is relative or absolute. */
	UPROPERTY()
	bool bRelativeRotation;

	/** State of Root Motion Sources on Authority */
	UPROPERTY()
	FRootMotionSourceGroup AuthoritativeRootMotion;

	/** Acceleration */
	UPROPERTY()
	FVector_NetQuantize10 Acceleration;

	/** Velocity */
	UPROPERTY()
	FVector_NetQuantize10 LinearVelocity;

	/** Clear root motion sources and root motion montage */
	void Clear()
	{
		bIsActive = false;
		AnimMontage = NULL;
		AuthoritativeRootMotion.Clear();
	}

	/** Is Valid - animation root motion only */
	bool HasRootMotion() const
	{
		return (AnimMontage != NULL);
	}
};

USTRUCT()
struct FSimulatedRootMotionReplicatedMove
{
	GENERATED_USTRUCT_BODY()

	/** Local time when move was received on client and saved. */
	UPROPERTY()
	float Time;

	/** Root Motion information */
	UPROPERTY()
	FRepRootMotionMontage RootMotion;
};


/** MovementBaseUtility provides utilities for working with movement bases, for which we may need relative positioning info. */
namespace MovementBaseUtility
{
	/** Determine whether MovementBase can possibly move. */
	ENGINE_API bool IsDynamicBase(const UPrimitiveComponent* MovementBase);

	/** Determine if we should use relative positioning when based on a component (because it may move). */
	FORCEINLINE bool UseRelativeLocation(const UPrimitiveComponent* MovementBase)
	{
		return IsDynamicBase(MovementBase);
	}

	/** Ensure that BasedObjectTick ticks after NewBase */
	ENGINE_API void AddTickDependency(FTickFunction& BasedObjectTick, UPrimitiveComponent* NewBase);

	/** Remove tick dependency of BasedObjectTick on OldBase */
	ENGINE_API void RemoveTickDependency(FTickFunction& BasedObjectTick, UPrimitiveComponent* OldBase);

	/** Get the velocity of the given component, first checking the ComponentVelocity and falling back to the physics velocity if necessary. */
	ENGINE_API FVector GetMovementBaseVelocity(const UPrimitiveComponent* MovementBase, const FName BoneName);

	/** Get the tangential velocity at WorldLocation for the given component. */
	ENGINE_API FVector GetMovementBaseTangentialVelocity(const UPrimitiveComponent* MovementBase, const FName BoneName, const FVector& WorldLocation);

	/** Get the transforms for the given MovementBase, optionally at the location of a bone. Returns false if MovementBase is NULL, or if BoneName is not a valid bone. */
	ENGINE_API bool GetMovementBaseTransform(const UPrimitiveComponent* MovementBase, const FName BoneName, FVector& OutLocation, FQuat& OutQuat);
}

/** Struct to hold information about the "base" object the character is standing on. */
USTRUCT()
struct FBasedMovementInfo
{
	GENERATED_USTRUCT_BODY()

	/** Component we are based on */
	UPROPERTY()
	UPrimitiveComponent* MovementBase;

	/** Bone name on component, for skeletal meshes. NAME_None if not a skeletal mesh or if bone is invalid. */
	UPROPERTY()
	FName BoneName;

	/** Location relative to MovementBase. Only valid if HasRelativeLocation() is true. */
	UPROPERTY()
	FVector_NetQuantize100 Location;

	/** Rotation: relative to MovementBase if HasRelativeRotation() is true, absolute otherwise. */
	UPROPERTY()
	FRotator Rotation;

	/** Whether the server says that there is a base. On clients, the component may not have resolved yet. */
	UPROPERTY()
	bool bServerHasBaseComponent;

	/** Whether rotation is relative to the base or absolute. It can only be relative if location is also relative. */
	UPROPERTY()
	bool bRelativeRotation;

	/** Whether there is a velocity on the server. Used for forcing replication when velocity goes to zero. */
	UPROPERTY()
	bool bServerHasVelocity;

	/** Is location relative? */
	FORCEINLINE bool HasRelativeLocation() const
	{
		return MovementBaseUtility::UseRelativeLocation(MovementBase);
	}

	/** Is rotation relative or absolute? It can only be relative if location is also relative. */
	FORCEINLINE bool HasRelativeRotation() const
	{
		return bRelativeRotation && HasRelativeLocation();
	}

	/** Return true if the client should have MovementBase, but it hasn't replicated (possibly component has not streamed in). */
	FORCEINLINE bool IsBaseUnresolved() const
	{
		return (MovementBase == NULL) && bServerHasBaseComponent;
	}
};


/**
 * Characters are Pawns that have a mesh, collision, and built-in movement logic.
 * They are responsible for all physical interaction between the player or AI and the world, and also implement basic networking and input models.
 * They are designed for a vertically-oriented player representation that can walk, jump, fly, and swim through the world using CharacterMovementComponent.
 *
 * @see APawn, UCharacterMovementComponent
 * @see https://docs.unrealengine.com/latest/INT/Gameplay/Framework/Pawn/Character/
 */ 

UCLASS(config=Game, BlueprintType, meta=(ShortTooltip="A character is a type of Pawn that includes the ability to walk around."))
class ENGINE_API ACharacter : public APawn
{
	GENERATED_BODY()
public:
	/**
	 * Default UObject constructor.
	 */
	ACharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	/** The main skeletal mesh associated with this Character (optional sub-object). */
	UPROPERTY(Category = Character, VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	class USkeletalMeshComponent* Mesh;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	class UArrowComponent* ArrowComponent;
#endif

	/** Movement component used for movement logic in various movement modes (walking, falling, etc), containing relevant settings and functions to control movement. */
	UPROPERTY(Category = Character, VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	class UCharacterMovementComponent* CharacterMovement;

	/** The CapsuleComponent being used for movement collision (by CharacterMovement). Always treated as being vertically aligned in simple collision check functions. */
	UPROPERTY(Category = Character, VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	class UCapsuleComponent* CapsuleComponent;

public:

	/** Name of the MeshComponent. Use this name if you want to prevent creation of the component (with ObjectInitializer.DoNotCreateDefaultSubobject). */
	static FName MeshComponentName;

	/** Name of the CharacterMovement component. Use this name if you want to use a different class (with ObjectInitializer.SetDefaultSubobjectClass). */
	static FName CharacterMovementComponentName;

	/** Name of the CapsuleComponent. */
	static FName CapsuleComponentName;

	/** Sets the component the Character is walking on, used by CharacterMovement walking movement to be able to follow dynamic objects. */
	virtual void SetBase(UPrimitiveComponent* NewBase, const FName BoneName = NAME_None, bool bNotifyActor=true);
	
	/**
	 * Cache mesh offset from capsule. This is used as the target for network smoothing interpolation, when the mesh is offset with lagged smoothing.
	 * This is automatically called during initialization; call this at runtime if you intend to change the default mesh offset from the capsule.
	 * @see GetBaseTranslationOffset(), GetBaseRotationOffset()
	 */
	UFUNCTION(BlueprintCallable, Category="Pawn|Character")
	virtual void CacheInitialMeshOffset(FVector MeshRelativeLocation, FRotator MeshRelativeRotation);

protected:

	/** Info about our current movement base (object we are standing on). */
	UPROPERTY()
	struct FBasedMovementInfo BasedMovement;

	/** Replicated version of relative movement. Read-only on simulated proxies! */
	UPROPERTY(ReplicatedUsing=OnRep_ReplicatedBasedMovement)
	struct FBasedMovementInfo ReplicatedBasedMovement;

	/** Scale to apply to root motion translation on this Character */
	UPROPERTY(Replicated)
	float AnimRootMotionTranslationScale;

public:
	/** Rep notify for ReplicatedBasedMovement */
	UFUNCTION()
	virtual void OnRep_ReplicatedBasedMovement();

	/** Set whether this actor's movement replicates to network clients. */
	UFUNCTION(BlueprintCallable, Category = "Replication")
	virtual void SetReplicateMovement(bool bInReplicateMovement) override;

protected:
	/** Saved translation offset of mesh. */
	UPROPERTY()
	FVector BaseTranslationOffset;

	/** Saved rotation offset of mesh. */
	UPROPERTY()
	FQuat BaseRotationOffset;

	/** Event called after actor's base changes (if SetBase was requested to notify us with bNotifyPawn). */
	virtual void BaseChange();

	/** CharacterMovement ServerLastTransformUpdateTimeStamp value, replicated to simulated proxies. */
	UPROPERTY(Replicated)
	float ReplicatedServerLastTransformUpdateTimeStamp;

	/** CharacterMovement MovementMode (and custom mode) replicated for simulated proxies. Use CharacterMovementComponent::UnpackNetworkMovementMode() to translate it. */
	UPROPERTY(Replicated)
	uint8 ReplicatedMovementMode;

	/** Flag that we are receiving replication of the based movement. */
	UPROPERTY()
	bool bInBaseReplication;

public:	

	/** Accessor for ReplicatedServerLastTransformUpdateTimeStamp. */
	FORCEINLINE float GetReplicatedServerLastTransformUpdateTimeStamp() const { return ReplicatedServerLastTransformUpdateTimeStamp; }

	/** Accessor for BasedMovement */
	FORCEINLINE const FBasedMovementInfo& GetBasedMovement() const { return BasedMovement; }
	
	/** Accessor for ReplicatedBasedMovement */
	FORCEINLINE const FBasedMovementInfo& GetReplicatedBasedMovement() const { return ReplicatedBasedMovement; }

	/** Save a new relative location in BasedMovement and a new rotation with is either relative or absolute. */
	void SaveRelativeBasedMovement(const FVector& NewRelativeLocation, const FRotator& NewRotation, bool bRelativeRotation);

	/** Returns ReplicatedMovementMode */
	uint8 GetReplicatedMovementMode() const { return ReplicatedMovementMode; }

	/** Get the saved translation offset of mesh. This is how much extra offset is applied from the center of the capsule. */
	UFUNCTION(BlueprintCallable, Category="Pawn|Character")
	FVector GetBaseTranslationOffset() const { return BaseTranslationOffset; }

	/** Get the saved rotation offset of mesh. This is how much extra rotation is applied from the capsule rotation. */
	virtual FQuat GetBaseRotationOffset() const { return BaseRotationOffset; }

	/** Get the saved rotation offset of mesh. This is how much extra rotation is applied from the capsule rotation. */
	UFUNCTION(BlueprintCallable, Category="Pawn|Character", meta=(DisplayName="GetBaseRotationOffset"))
	FRotator GetBaseRotationOffsetRotator() const { return GetBaseRotationOffset().Rotator(); }

	//~ Begin INavAgentInterface Interface
	virtual FVector GetNavAgentLocation() const override;
	//~ End INavAgentInterface Interface

	/** Default crouched eye height */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Camera)
	float CrouchedEyeHeight;

	/** Set by character movement to specify that this Character is currently crouched. */
	UPROPERTY(BlueprintReadOnly, replicatedUsing=OnRep_IsCrouched, Category=Character)
	uint32 bIsCrouched:1;

	/** Handle Crouching replicated from server */
	UFUNCTION()
	virtual void OnRep_IsCrouched();

	/** When true, player wants to jump */
	UPROPERTY(BlueprintReadOnly, Category="Pawn|Character")
	uint32 bPressedJump:1;

	/** When true, applying updates to network client (replaying saved moves for a locally controlled character) */
	UPROPERTY(Transient)
	uint32 bClientUpdating:1;

	/** True if Pawn was initially falling when started to replay network moves. */
	UPROPERTY(Transient)
	uint32 bClientWasFalling:1; 

	/** If server disagrees with root motion track position, client has to resimulate root motion from last AckedMove. */
	UPROPERTY(Transient)
	uint32 bClientResimulateRootMotion:1;

	/** If server disagrees with root motion state, client has to resimulate root motion from last AckedMove. */
	UPROPERTY(Transient)
	uint32 bClientResimulateRootMotionSources:1;

	/** Disable simulated gravity (set when character encroaches geometry on client, to keep him from falling through floors) */
	UPROPERTY()
	uint32 bSimGravityDisabled:1;

	UPROPERTY(Transient)
	uint32 bClientCheckEncroachmentOnNetUpdate:1;

	/** Disable root motion on the server. When receiving a DualServerMove, where the first move is not root motion and the second is. */
	UPROPERTY(Transient)
	uint32 bServerMoveIgnoreRootMotion:1;

	/** 
	 * Jump key Held Time.
	 * This is the time that the player has held the jump key, in seconds.
	 */
	UPROPERTY(Transient, BlueprintReadOnly, VisibleInstanceOnly, Category=Character)
	float JumpKeyHoldTime;

	/** 
	 * The max time the jump key can be held.
	 * Note that if StopJumping() is not called before the max jump hold time is reached,
	 * then the character will carry on receiving vertical velocity. Therefore it is usually 
	 * best to call StopJumping() when jump input has ceased (such as a button up event).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category=Character, Meta=(ClampMin=0.0, UIMin=0.0))
	float JumpMaxHoldTime;

    /**
     * The max number of jumps the character can perform.
     * Note that if JumpMaxHoldTime is non zero and StopJumping is not called, the player
     * may be able to perform and unlimited number of jumps. Therefore it is usually
     * best to call StopJumping() when jump input has ceased (such as a button up event).
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category=Character)
    int32 JumpMaxCount;

    /**
     * Tracks the current number of jumps performed.
     * This is incremented in CheckJumpInput, used in CanJump_Implementation, and reset in OnMovementModeChanged.
     * When providing overrides for these methods, it's recommended to either manually
     * increment / reset this value, or call the Super:: method.
     */
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Character")
    int32 JumpCurrentCount;

	DEPRECATED(4.14, "This value is no longer used.")
	uint32 bJumpMaxCountExceeded:1;

	// Tracks whether or not the character was already jumping last frame.
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Character")
	uint32 bWasJumping:1;

	//~ Begin AActor Interface.
	virtual void BeginPlay() override;
	virtual void ClearCrossLevelReferences() override;
	virtual void PreNetReceive() override;
	virtual void PostNetReceive() override;
	virtual void OnRep_ReplicatedMovement() override;
	virtual void PostNetReceiveLocationAndRotation() override;
	virtual void GetSimpleCollisionCylinder(float& CollisionRadius, float& CollisionHalfHeight) const override;
	virtual UActorComponent* FindComponentByClass(const TSubclassOf<UActorComponent> ComponentClass) const override;
	virtual void TornOff() override;
	//~ End AActor Interface

	template<class T>
	T* FindComponentByClass() const
	{
		return AActor::FindComponentByClass<T>();
	}

	//~ Begin APawn Interface.
	virtual void PostInitializeComponents() override;
	virtual UPawnMovementComponent* GetMovementComponent() const override;
	virtual UPrimitiveComponent* GetMovementBase() const override final { return BasedMovement.MovementBase; }
	virtual float GetDefaultHalfHeight() const override;
	virtual void TurnOff() override;
	virtual void Restart() override;
	virtual void PawnClientRestart() override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void UnPossessed() override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	virtual void DisplayDebug(class UCanvas* Canvas, const FDebugDisplayInfo& DebugDisplay, float& YL, float& YPos) override;
	virtual void RecalculateBaseEyeHeight() override;
	virtual void UpdateNavigationRelevance() override;
	//~ End APawn Interface

	/** Apply momentum caused by damage. */
	virtual void ApplyDamageMomentum(float DamageTaken, FDamageEvent const& DamageEvent, APawn* PawnInstigator, AActor* DamageCauser);

	/** 
	 * Make the character jump on the next update.	 
	 * If you want your character to jump according to the time that the jump key is held,
	 * then you can set JumpKeyHoldTime to some non-zero value. Make sure in this case to
	 * call StopJumping() when you want the jump's z-velocity to stop being applied (such 
	 * as on a button up event), otherwise the character will carry on receiving the 
	 * velocity until JumpKeyHoldTime is reached.
	 */
	UFUNCTION(BlueprintCallable, Category="Pawn|Character")
	virtual void Jump();

	/** 
	 * Stop the character from jumping on the next update. 
	 * Call this from an input event (such as a button 'up' event) to cease applying
	 * jump Z-velocity. If this is not called, then jump z-velocity will be applied
	 * until JumpMaxHoldTime is reached.
	 */
	UFUNCTION(BlueprintCallable, Category="Pawn|Character")
	virtual void StopJumping();

	/**
	 * Check if the character can jump in the current state.
	 *
	 * The default implementation may be overridden or extended by implementing the custom CanJump event in Blueprints. 
	 * 
	 * @Return Whether the character can jump in the current state. 
	 */
	UFUNCTION(BlueprintCallable, Category="Pawn|Character")
	bool CanJump() const;

protected:

	/**
	 * Customizable event to check if the character can jump in the current state.
	 * Default implementation returns true if the character is on the ground and not crouching,
	 * has a valid CharacterMovementComponent and CanEverJump() returns true.
	 * Default implementation also allows for 'hold to jump higher' functionality: 
	 * As well as returning true when on the ground, it also returns true when GetMaxJumpTime is more
	 * than zero and IsJumping returns true.
	 * 
	 *
	 * @Return Whether the character can jump in the current state. 
	 */

	UFUNCTION(BlueprintNativeEvent, Category="Pawn|Character", meta=(DisplayName="CanJump"))
	bool CanJumpInternal() const;
	virtual bool CanJumpInternal_Implementation() const;

	DEPRECATED(4.14, "This function is deprecated. Please use ResetJumpState instead.")
	void CheckResetJumpCount()
	{
		ResetJumpState();
	}

	void ResetJumpState();

public:

	/**
	 * True if jump is actively providing a force, such as when the jump key is held and the time it has been held is less than JumpMaxHoldTime.
	 * @see CharacterMovement->IsFalling
	 */
	UFUNCTION(BlueprintCallable, Category="Pawn|Character")
	virtual bool IsJumpProvidingForce() const;

	/** Play Animation Montage on the character mesh **/
	UFUNCTION(BlueprintCallable, Category=Animation)
	virtual float PlayAnimMontage(class UAnimMontage* AnimMontage, float InPlayRate = 1.f, FName StartSectionName = NAME_None);

	/** Stop Animation Montage. If NULL, it will stop what's currently active. The Blend Out Time is taken from the montage asset that is being stopped. **/
	UFUNCTION(BlueprintCallable, Category=Animation)
	virtual void StopAnimMontage(class UAnimMontage* AnimMontage = NULL);

	/** Return current playing Montage **/
	UFUNCTION(BlueprintCallable, Category=Animation)
	class UAnimMontage* GetCurrentMontage();

public:

	/** Set a pending launch velocity on the Character. This velocity will be processed on the next CharacterMovementComponent tick,
	  * and will set it to the "falling" state. Triggers the OnLaunched event.
	  * @PARAM LaunchVelocity is the velocity to impart to the Character
	  * @PARAM bXYOverride if true replace the XY part of the Character's velocity instead of adding to it.
	  * @PARAM bZOverride if true replace the Z component of the Character's velocity instead of adding to it.
	  */
	UFUNCTION(BlueprintCallable, Category="Pawn|Character")
	virtual void LaunchCharacter(FVector LaunchVelocity, bool bXYOverride, bool bZOverride);

	/** Let blueprint know that we were launched */
	UFUNCTION(BlueprintImplementableEvent)
	void OnLaunched(FVector LaunchVelocity, bool bXYOverride, bool bZOverride);

	/** Event fired when the character has just started jumping */
	UFUNCTION(BlueprintNativeEvent, Category="Pawn|Character")
	void OnJumped();
	virtual void OnJumped_Implementation();

	/** Called when the character's movement enters falling */
	virtual void Falling() {}

	/** Called when character's jump reaches Apex. Needs CharacterMovement->bNotifyApex = true */
	virtual void NotifyJumpApex();

	/** Broadcast when Character's jump reaches its apex. Needs CharacterMovement->bNotifyApex = true */
	UPROPERTY(BlueprintAssignable, Category="Pawn|Character")
	FCharacterReachedApexSignature OnReachedJumpApex;

	/**
	 * Called upon landing when falling, to perform actions based on the Hit result. Triggers the OnLanded event.
	 * Note that movement mode is still "Falling" during this event. Current Velocity value is the velocity at the time of landing.
	 * Consider OnMovementModeChanged() as well, as that can be used once the movement mode changes to the new mode (most likely Walking).
	 *
	 * @param Hit Result describing the landing that resulted in a valid landing spot.
	 * @see OnMovementModeChanged()
	 */
	virtual void Landed(const FHitResult& Hit);

	/**
	* Called upon landing when falling, to perform actions based on the Hit result.
	* Note that movement mode is still "Falling" during this event. Current Velocity value is the velocity at the time of landing.
	* Consider OnMovementModeChanged() as well, as that can be used once the movement mode changes to the new mode (most likely Walking).
	*
	* @param Hit Result describing the landing that resulted in a valid landing spot.
	* @see OnMovementModeChanged()
	*/
	FLandedSignature LandedDelegate;

	/**
	* Called upon landing when falling, to perform actions based on the Hit result.
	* Note that movement mode is still "Falling" during this event. Current Velocity value is the velocity at the time of landing.
	* Consider OnMovementModeChanged() as well, as that can be used once the movement mode changes to the new mode (most likely Walking).
	*
	* @param Hit Result describing the landing that resulted in a valid landing spot.
	* @see OnMovementModeChanged()
	*/
	UFUNCTION(BlueprintImplementableEvent)
	void OnLanded(const FHitResult& Hit);

	/**
	 * Event fired when the Character is walking off a surface and is about to fall because CharacterMovement->CurrentFloor became unwalkable.
	 * If CharacterMovement->MovementMode does not change during this event then the character will automatically start falling afterwards.
	 * @note Z velocity is zero during walking movement, and will be here as well. Another velocity can be computed here if desired and will be used when starting to fall.
	 *
	 * @param  PreviousFloorImpactNormal Normal of the previous walkable floor.
	 * @param  PreviousFloorContactNormal Normal of the contact with the previous walkable floor.
	 * @param  PreviousLocation	Previous character location before movement off the ledge.
	 * @param  TimeTick	Time delta of movement update resulting in moving off the ledge.
	 */
	UFUNCTION(BlueprintNativeEvent, Category="Pawn|Character")
	void OnWalkingOffLedge(const FVector& PreviousFloorImpactNormal, const FVector& PreviousFloorContactNormal, const FVector& PreviousLocation, float TimeDelta);
	virtual void OnWalkingOffLedge_Implementation(const FVector& PreviousFloorImpactNormal, const FVector& PreviousFloorContactNormal, const FVector& PreviousLocation, float TimeDelta);

	/** Called when pawn's movement is blocked
		@PARAM Impact describes the blocking hit. */
	virtual void MoveBlockedBy(const FHitResult& Impact) {};

	/**
	 * Request the character to start crouching. The request is processed on the next update of the CharacterMovementComponent.
	 * @see OnStartCrouch
	 * @see IsCrouched
	 * @see CharacterMovement->WantsToCrouch
	 */
	UFUNCTION(BlueprintCallable, Category="Pawn|Character", meta=(HidePin="bClientSimulation"))
	virtual void Crouch(bool bClientSimulation = false);

	/**
	 * Request the character to stop crouching. The request is processed on the next update of the CharacterMovementComponent.
	 * @see OnEndCrouch
	 * @see IsCrouched
	 * @see CharacterMovement->WantsToCrouch
	 */
	UFUNCTION(BlueprintCallable, Category="Pawn|Character", meta=(HidePin="bClientSimulation"))
	virtual void UnCrouch(bool bClientSimulation = false);

	/** @return true if this character is currently able to crouch (and is not currently crouched) */
	virtual bool CanCrouch();

	/** 
	 * Called when Character stops crouching. Called on non-owned Characters through bIsCrouched replication.
	 * @param	HalfHeightAdjust		difference between default collision half-height, and actual crouched capsule half-height.
	 * @param	ScaledHalfHeightAdjust	difference after component scale is taken in to account.
	 */
	virtual void OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust);

	/** 
	 * Event when Character stops crouching.
	 * @param	HalfHeightAdjust		difference between default collision half-height, and actual crouched capsule half-height.
	 * @param	ScaledHalfHeightAdjust	difference after component scale is taken in to account.
	 */
	UFUNCTION(BlueprintImplementableEvent, meta=(DisplayName = "OnEndCrouch"))
	void K2_OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust);

	/**
	 * Called when Character crouches. Called on non-owned Characters through bIsCrouched replication.
	 * @param	HalfHeightAdjust		difference between default collision half-height, and actual crouched capsule half-height.
	 * @param	ScaledHalfHeightAdjust	difference after component scale is taken in to account.
	 */
	virtual void OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust);

	/**
	 * Event when Character crouches.
	 * @param	HalfHeightAdjust		difference between default collision half-height, and actual crouched capsule half-height.
	 * @param	ScaledHalfHeightAdjust	difference after component scale is taken in to account.
	 */
	UFUNCTION(BlueprintImplementableEvent, meta=(DisplayName = "OnStartCrouch"))
	void K2_OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust);

	/**
	 * Called from CharacterMovementComponent to notify the character that the movement mode has changed.
	 * @param	PrevMovementMode	Movement mode before the change
	 * @param	PrevCustomMode		Custom mode before the change (applicable if PrevMovementMode is Custom)
	 */
	virtual void OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode = 0);

	/** Multicast delegate for MovementMode changing. */
	UPROPERTY(BlueprintAssignable, Category="Pawn|Character")
	FMovementModeChangedSignature MovementModeChangedDelegate;

	/**
	 * Called from CharacterMovementComponent to notify the character that the movement mode has changed.
	 * @param	PrevMovementMode	Movement mode before the change
	 * @param	NewMovementMode		New movement mode
	 * @param	PrevCustomMode		Custom mode before the change (applicable if PrevMovementMode is Custom)
	 * @param	NewCustomMode		New custom mode (applicable if NewMovementMode is Custom)
	 */
	UFUNCTION(BlueprintImplementableEvent, meta=(DisplayName = "OnMovementModeChanged"))
	void K2_OnMovementModeChanged(EMovementMode PrevMovementMode, EMovementMode NewMovementMode, uint8 PrevCustomMode, uint8 NewCustomMode);

	/**
	 * Event for implementing custom character movement mode. Called by CharacterMovement if MovementMode is set to Custom.
	 * @note C++ code should override UCharacterMovementComponent::PhysCustom() instead.
	 * @see UCharacterMovementComponent::PhysCustom()
	 */
	UFUNCTION(BlueprintImplementableEvent, meta=(DisplayName = "UpdateCustomMovement"))
	void K2_UpdateCustomMovement(float DeltaTime);

	/**
	 * Event triggered at the end of a CharacterMovementComponent movement update.
	 * This is the preferred event to use rather than the Tick event when performing custom updates to CharacterMovement properties based on the current state.
	 * This is mainly due to the nature of network updates, where client corrections in position from the server can cause multiple iterations of a movement update,
	 * which allows this event to update as well, while a Tick event would not.
	 *
	 * @param	DeltaSeconds		Delta time in seconds for this update
	 * @param	InitialLocation		Location at the start of the update. May be different than the current location if movement occurred.
	 * @param	InitialVelocity		Velocity at the start of the update. May be different than the current velocity.
	 */
	UPROPERTY(BlueprintAssignable, Category="Pawn|Character")
	FCharacterMovementUpdatedSignature OnCharacterMovementUpdated;

	/** 
	 * Returns true if the Landed() event should be called. Used by CharacterMovement to prevent notifications while playing back network moves.
	 */
	virtual bool ShouldNotifyLanded(const struct FHitResult& Hit);

	/** Trigger jump if jump button has been pressed. */
	virtual void CheckJumpInput(float DeltaTime);

	/** Reset jump input state after having checked input. */
	virtual void ClearJumpInput();

	/**
	 * Get the maximum jump time for the character.
	 * Note that if StopJumping() is not called before the max jump hold time is reached,
	 * then the character will carry on receiving vertical velocity. Therefore it is usually 
	 * best to call StopJumping() when jump input has ceased (such as a button up event).
	 * 
	 * @return Maximum jump time for the character
	 */
	virtual float GetJumpMaxHoldTime() const;

public:

	UFUNCTION(reliable, client)
	void ClientCheatWalk();
	virtual void ClientCheatWalk_Implementation();

	UFUNCTION(reliable, client)
	void ClientCheatFly();
	virtual void ClientCheatFly_Implementation();

	UFUNCTION(reliable, client)
	void ClientCheatGhost();
	virtual void ClientCheatGhost_Implementation();

	UFUNCTION(reliable, client)
	void RootMotionDebugClientPrintOnScreen(const FString& InString);
	virtual void RootMotionDebugClientPrintOnScreen_Implementation(const FString& InString);

	// Root Motion
public:
	/** 
	 *  For LocallyControlled Autonomous clients. 
	 *  During a PerformMovement() after root motion is prepared, we save it off into this and
	 *  then record it into our SavedMoves.
	 *  During SavedMove playback we use it as our "Previous Move" SavedRootMotion which includes
	 *  last received root motion from the Server
	 **/
	UPROPERTY(Transient)
	FRootMotionSourceGroup SavedRootMotion;

	/** For LocallyControlled Autonomous clients. Saved root motion data to be used by SavedMoves. */
	UPROPERTY(Transient)
	FRootMotionMovementParams ClientRootMotionParams;

	/** Array of previously received root motion moves from the server. */
	UPROPERTY(Transient)
	TArray<FSimulatedRootMotionReplicatedMove> RootMotionRepMoves;
	
	/** Find usable root motion replicated move from our buffer.
	 * Goes through the buffer back in time, to find the first move that clears 'CanUseRootMotionRepMove' below.
	 * Returns index of that move or INDEX_NONE otherwise. */
	int32 FindRootMotionRepMove(const FAnimMontageInstance& ClientMontageInstance) const;

	/** true if buffered move is usable to teleport client back to. */
	bool CanUseRootMotionRepMove(const FSimulatedRootMotionReplicatedMove& RootMotionRepMove, const FAnimMontageInstance& ClientMontageInstance) const;

	/** Restore actor to an old buffered move. */
	bool RestoreReplicatedMove(const FSimulatedRootMotionReplicatedMove& RootMotionRepMove);
	
	DEPRECATED(4.11, "UpdateSimulatedPosition() is deprecated and is not used by engine code. Use OnUpdateSimulatedPosition() instead.")
	virtual void UpdateSimulatedPosition(const FVector& Location, const FRotator& NewRotation);

	/**
	 * Called on client after position update is received to respond to the new location and rotation.
	 * Actual change in location is expected to occur in CharacterMovement->SmoothCorrection(), after which this occurs.
	 * Default behavior is to check for penetration in a blocking object if bClientCheckEncroachmentOnNetUpdate is enabled, and set bSimGravityDisabled=true if so.
	 */
	virtual void OnUpdateSimulatedPosition(const FVector& OldLocation, const FQuat& OldRotation);

	/** Replicated Root Motion montage */
	UPROPERTY(ReplicatedUsing=OnRep_RootMotion)
	struct FRepRootMotionMontage RepRootMotion;
	
	/** Handles replicated root motion properties on simulated proxies and position correction. */
	UFUNCTION()
	void OnRep_RootMotion();

	/** Position fix up for Simulated Proxies playing Root Motion */
	void SimulatedRootMotionPositionFixup(float DeltaSeconds);

	/** Get FAnimMontageInstance playing RootMotion */
	FAnimMontageInstance * GetRootMotionAnimMontageInstance() const;

	/** true if we are playing Root Motion right now */
	UFUNCTION(BlueprintCallable, Category=Animation)
	bool IsPlayingRootMotion() const;

	/** true if we are playing Root Motion right now, through a Montage with RootMotionMode == ERootMotionMode::RootMotionFromMontagesOnly.
	 * This means code path for networked root motion is enabled. */
	UFUNCTION(BlueprintCallable, Category = Animation)
	bool IsPlayingNetworkedRootMotionMontage() const;

	/** Sets scale to apply to root motion translation on this Character */
	void SetAnimRootMotionTranslationScale(float InAnimRootMotionTranslationScale = 1.f);

	/** Returns current value of AnimRootMotionScale */
	UFUNCTION(BlueprintCallable, Category = Animation)
	float GetAnimRootMotionTranslationScale() const;

	/** Called on the actor right before replication occurs.
	* Only called on Server, and for autonomous proxies if recording a Client Replay. */
	virtual void PreReplication(IRepChangedPropertyTracker & ChangedPropertyTracker) override;

	/** Called on the actor right before replication occurs.
	* Called for everyone when recording a Client Replay, including Simulated Proxies. */
	virtual void PreReplicationForReplay(IRepChangedPropertyTracker & ChangedPropertyTracker) override;

public:
	/** Returns Mesh subobject **/
	class USkeletalMeshComponent* GetMesh() const;
#if WITH_EDITORONLY_DATA
	/** Returns ArrowComponent subobject **/
	class UArrowComponent* GetArrowComponent() const;
#endif
	/** Returns CharacterMovement subobject **/
	class UCharacterMovementComponent* GetCharacterMovement() const;
	/** Returns CapsuleComponent subobject **/
	class UCapsuleComponent* GetCapsuleComponent() const;
};


//////////////////////////////////////////////////////////////////////////
// Character inlines

/** Returns Mesh subobject **/
FORCEINLINE USkeletalMeshComponent* ACharacter::GetMesh() const { return Mesh; }
#if WITH_EDITORONLY_DATA
/** Returns ArrowComponent subobject **/
FORCEINLINE UArrowComponent* ACharacter::GetArrowComponent() const { return ArrowComponent; }
#endif
/** Returns CharacterMovement subobject **/
FORCEINLINE UCharacterMovementComponent* ACharacter::GetCharacterMovement() const { return CharacterMovement; }
/** Returns CapsuleComponent subobject **/
FORCEINLINE UCapsuleComponent* ACharacter::GetCapsuleComponent() const { return CapsuleComponent; }
