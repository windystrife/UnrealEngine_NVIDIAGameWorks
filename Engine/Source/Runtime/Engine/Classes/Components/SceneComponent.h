// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/CoreNet.h"
#include "Engine/EngineTypes.h"
#include "ComponentInstanceDataCache.h"
#include "Components/ActorComponent.h"
#include "RHIDefinitions.h"
#include "SceneComponent.generated.h"

class AActor;
class APhysicsVolume;
class USceneComponent;
struct FLevelCollection;
struct FMinimalViewInfo;

/** Overlap info consisting of the primitive and the body that is overlapping */
struct ENGINE_API FOverlapInfo
{
	FOverlapInfo()
	{}

	explicit FOverlapInfo(const FHitResult& InSweepResult)
		: bFromSweep(true), OverlapInfo(InSweepResult)
	{
	}

	explicit FOverlapInfo(class UPrimitiveComponent* InComponent, int32 InBodyIndex = INDEX_NONE);
	
	int32 GetBodyIndex() const { return OverlapInfo.Item;  }

	//This function completely ignores SweepResult information. It seems that places that use this function do not care, but it still seems risky
	friend bool operator == (const FOverlapInfo& LHS, const FOverlapInfo& RHS) { return LHS.OverlapInfo.Component == RHS.OverlapInfo.Component && LHS.OverlapInfo.Item == RHS.OverlapInfo.Item; }
	bool bFromSweep;

	/** Information for both sweep and overlap queries. Different parts are valid depending on bFromSweep.
	  * If bFromSweep is true then FHitResult is completely valid just like a regular sweep result.
	  * If bFromSweep is false only FHitResult::Component, FHitResult::Actor, FHitResult::Item are valid as this is really just an FOverlapResult*/
	FHitResult OverlapInfo;
};

// All added members of FOverlapInfo are PODs.
template<> struct TIsPODType<FOverlapInfo> { enum { Value = TIsPODType<FHitResult>::Value }; };

/** Detail mode for scene component rendering. */
UENUM()
enum EDetailMode
{
	DM_Low UMETA(DisplayName="Low"),
	DM_Medium UMETA(DisplayName="Medium"),
	DM_High UMETA(DisplayName="High"),
	DM_MAX,
};

/** The space for the transform */
UENUM()
enum ERelativeTransformSpace
{
	/** World space transform. */
	RTS_World,
	/** Actor space transform. */
	RTS_Actor,
	/** Component space transform. */
	RTS_Component,
	/** Parent bone space transform */
	RTS_ParentBoneSpace,
};

//
// MoveComponent options.
//
enum EMoveComponentFlags
{
	// Bitflags.
	MOVECOMP_NoFlags						= 0x0000,	// no flags
	MOVECOMP_IgnoreBases					= 0x0001,	// ignore collisions with things the Actor is based on
	MOVECOMP_SkipPhysicsMove				= 0x0002,	// when moving this component, do not move the physics representation. Used internally to avoid looping updates when syncing with physics.
	MOVECOMP_NeverIgnoreBlockingOverlaps	= 0x0004,	// never ignore initial blocking overlaps during movement, which are usually ignored when moving out of an object. MOVECOMP_IgnoreBases is still respected.
	MOVECOMP_DisableBlockingOverlapDispatch	= 0x0008,	// avoid dispatching blocking hit events when the hit started in penetration (and is not ignored, see MOVECOMP_NeverIgnoreBlockingOverlaps).
};

#define SCENECOMPONENT_QUAT_TOLERANCE		(1.e-8f) // Comparison tolerance for checking if two FQuats are the same when moving SceneComponents.
#define SCENECOMPONENT_ROTATOR_TOLERANCE	(1.e-4f) // Comparison tolerance for checking if two FRotators are the same when moving SceneComponents.

FORCEINLINE EMoveComponentFlags operator|(EMoveComponentFlags Arg1,EMoveComponentFlags Arg2)	{ return EMoveComponentFlags(uint32(Arg1) | uint32(Arg2)); }
FORCEINLINE EMoveComponentFlags operator&(EMoveComponentFlags Arg1,EMoveComponentFlags Arg2)	{ return EMoveComponentFlags(uint32(Arg1) & uint32(Arg2)); }
FORCEINLINE void operator&=(EMoveComponentFlags& Dest,EMoveComponentFlags Arg)					{ Dest = EMoveComponentFlags(Dest & Arg); }
FORCEINLINE void operator|=(EMoveComponentFlags& Dest,EMoveComponentFlags Arg)					{ Dest = EMoveComponentFlags(Dest | Arg); }

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPhysicsVolumeChanged, class APhysicsVolume*, NewVolume);
DECLARE_EVENT_ThreeParams(USceneComponent, FTransformUpdated, USceneComponent* /*UpdatedComponent*/, EUpdateTransformFlags /*UpdateTransformFlags*/, ETeleportType /*Teleport*/);

/**
 * A SceneComponent has a transform and supports attachment, but has no rendering or collision capabilities.
 * Useful as a 'dummy' component in the hierarchy to offset others.
 * @see [Scene Components](https://docs.unrealengine.com/latest/INT/Programming/UnrealArchitecture/Actors/Components/index.html#scenecomponents)
 */
UCLASS(ClassGroup=(Utility, Common), BlueprintType, hideCategories=(Trigger, PhysicsVolume), meta=(BlueprintSpawnableComponent, IgnoreCategoryKeywordsInSubclasses, ShortTooltip="A Scene Component is a component that has a scene transform and can be attached to other scene components."))
class ENGINE_API USceneComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	/** The name to use for the default scene root variable */
	static FName GetDefaultSceneRootVariableName();

	/**
	 * UObject constructor that takes an ObjectInitializer
	 */
	USceneComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Cached level collection that contains the level this component is registered in, for fast access in IsVisible(). */
	const FLevelCollection* CachedLevelCollection;

private:
	/** Physics Volume in which this SceneComponent is located **/
	UPROPERTY(transient)
	TWeakObjectPtr<class APhysicsVolume> PhysicsVolume;

	/** What we are currently attached to. If valid, RelativeLocation etc. are used relative to this object */
	UPROPERTY(ReplicatedUsing = OnRep_AttachParent)
	USceneComponent* AttachParent;

	/** Optional socket name on AttachParent that we are attached to. */
	UPROPERTY(ReplicatedUsing = OnRep_AttachSocketName)
	FName AttachSocketName;

	/** List of child SceneComponents that are attached to us. */
	UPROPERTY(ReplicatedUsing = OnRep_AttachChildren, Transient)
	TArray<USceneComponent*> AttachChildren;

	/** Set of attached SceneComponents that were attached by the client so we can fix up AttachChildren when it is replicated to us. */
	UPROPERTY(Transient)
	TArray<USceneComponent*> ClientAttachedChildren;

	FName NetOldAttachSocketName;
	USceneComponent* NetOldAttachParent;

public:
	/** Current bounds of the component */
	FBoxSphereBounds Bounds;

	/** Location of the component relative to its parent */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_Transform, Category = Transform)
	FVector RelativeLocation;

	/** Rotation of the component relative to its parent */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_Transform, Category=Transform)
	FRotator RelativeRotation;

	/**
	*	Non-uniform scaling of the component relative to its parent.
	*	Note that scaling is always applied in local space (no shearing etc)
	*/
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_Transform, interp, Category=Transform)
	FVector RelativeScale3D;

private:

	/** Current transform of the component, relative to the world */
	FTransform ComponentToWorld;

public:

	/**
	* Velocity of the component.
	* @see GetComponentVelocity()
	*/
	UPROPERTY()
	FVector ComponentVelocity;

private:
	/** True if we have ever updated ComponentToWorld based on RelativeLocation/Rotation/Scale. Used at startup to make sure it is initialized. */
	UPROPERTY(Transient)
	uint8 bComponentToWorldUpdated : 1;

public:
	/** If RelativeLocation should be considered relative to the world, rather than the parent */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadWrite, ReplicatedUsing=OnRep_Transform, Category=Transform)
	uint8 bAbsoluteLocation:1;

	/** If RelativeRotation should be considered relative to the world, rather than the parent */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadWrite, ReplicatedUsing=OnRep_Transform, Category=Transform)
	uint8 bAbsoluteRotation:1;

	/** If RelativeScale3D should be considered relative to the world, rather than the parent */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadWrite, ReplicatedUsing=OnRep_Transform, Category=Transform)
	uint8 bAbsoluteScale:1;

	/** Whether to completely draw the primitive; if false, the primitive is not drawn, does not cast a shadow. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_Visibility,  Category = Rendering)
	uint8 bVisible:1;

	/** Whether to hide the primitive in game, if the primitive is Visible. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Rendering, meta=(SequencerTrackClass = "MovieSceneVisibilityTrack"))
	uint8 bHiddenInGame:1;

	/**
	 * Whether or not the cached PhysicsVolume this component overlaps should be updated when the component is moved.
	 * @see GetPhysicsVolume()
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category=Physics)
	uint8 bShouldUpdatePhysicsVolume:1;

	/** If true, a change in the bounds of the component will call trigger a streaming data rebuild */
	UPROPERTY()
	uint8 bBoundsChangeTriggersStreamingDataRebuild:1;

	/** If true, this component uses its parents bounds when attached.
	 *  This can be a significant optimization with many components attached together.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category=Rendering)
	uint8 bUseAttachParentBound:1;

protected:

	// Transient flag that temporarily disables UpdateOverlaps within DetachFromParent().
	uint8 bDisableDetachmentUpdateOverlaps:1;

	/** If true, OnUpdateTransform virtual will be called each time this component is moved. */
	uint8 bWantsOnUpdateTransform:1;

private:

	uint8 bNetUpdateTransform : 1;
	uint8 bNetUpdateAttachment : 1;

	// DEPRECATED
	UPROPERTY()
	uint8 bAbsoluteTranslation_DEPRECATED : 1;

	// Appends all descendants (recursively) of this scene component to the list of Children.  NOTE: It does NOT clear the list first.
	void AppendDescendants(TArray<USceneComponent*>& Children) const;

public:

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	uint8 bVisualizeComponent : 1;
#endif

	/** How often this component is allowed to move, used to make various optimizations. Only safe to set in constructor. */
	UPROPERTY(Category = Mobility, EditAnywhere, BlueprintReadOnly)
	TEnumAsByte<EComponentMobility::Type> Mobility;

	/** If detail mode is >= system detail mode, primitive won't be rendered. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category = LOD)
	TEnumAsByte<enum EDetailMode> DetailMode;

private:
	/** Cache that avoids Quat<->Rotator conversions if possible. Only to be used with GetComponentTransform().GetRotation(). */
	FRotationConversionCache WorldRotationCache;

	/** Cache that avoids Quat<->Rotator conversions if possible. Only to be used with RelativeRotation. */
	FRotationConversionCache RelativeRotationCache;

public:

	/** Sets the RelativeRotationCache. Used to ensure component ends up with the same RelativeRotation after calling SetWorldTransform(). */
	void SetRelativeRotationCache(const FRotationConversionCache& InCache);
	/** Get the RelativeRotationCache.  */
	FORCEINLINE const FRotationConversionCache& GetRelativeRotationCache() const { return RelativeRotationCache; }

	/** Delegate that will be called when PhysicsVolume has been changed **/
	UPROPERTY(BlueprintAssignable, Category=PhysicsVolume, meta=(DisplayName="Physics Volume Changed"))
	FPhysicsVolumeChanged PhysicsVolumeChangedDelegate;

	/** Returns the current scoped movement update, or NULL if there is none. @see FScopedMovementUpdate */
	class FScopedMovementUpdate* GetCurrentScopedMovement() const;

private:

	/** Stack of current movement scopes. */
	TArray<class FScopedMovementUpdate*> ScopedMovementStack;

	void BeginScopedMovementUpdate(class FScopedMovementUpdate& ScopedUpdate);
	void EndScopedMovementUpdate(class FScopedMovementUpdate& ScopedUpdate);

	friend class FScopedMovementUpdate;
	friend class FScopedPreventAttachedComponentMove;

public:

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	FVector RelativeTranslation_DEPRECATED;
#endif

private:
	UFUNCTION()
	void OnRep_Transform();

	UFUNCTION()
	void OnRep_AttachParent();

	UFUNCTION()
	void OnRep_AttachChildren();

	UFUNCTION()
	void OnRep_AttachSocketName();

	UFUNCTION()
	void OnRep_Visibility(bool OldValue);

protected:

	virtual void PreNetReceive() override;
	virtual void PostNetReceive() override;
	virtual void PostRepNotifies() override;

public:
	/**
	 * Set the location of the component relative to its parent
	 * @param NewLocation		New location of the component relative to its parent.		
	 * @param SweepHitResult	Hit result from any impact if sweep is true.
	 * @param bSweep			Whether we sweep to the destination location, triggering overlaps along the way and stopping short of the target if blocked by something.
	 *							Only the root component is swept and checked for blocking collision, child components move without sweeping. If collision is off, this has no effect.
	 * @param bTeleport			Whether we teleport the physics state (if physics collision is enabled for this object).
	 *							If true, physics velocity for this object is unchanged (so ragdoll parts are not affected by change in location).
	 *							If false, physics velocity is updated based on the change in position (affecting ragdoll parts).
	 *							If CCD is on and not teleporting, this will affect objects along the entire sweep volume.
	 */
	UFUNCTION(BlueprintCallable, Category="Utilities|Transformation", meta=(DisplayName="SetRelativeLocation"))
	void K2_SetRelativeLocation(FVector NewLocation, bool bSweep, FHitResult& SweepHitResult, bool bTeleport);
	void SetRelativeLocation(FVector NewLocation, bool bSweep=false, FHitResult* OutSweepHitResult=nullptr, ETeleportType Teleport = ETeleportType::None);


	/**
	 * Set the rotation of the component relative to its parent
	 * @param NewRotation		New rotation of the component relative to its parent
	 * @param SweepHitResult	Hit result from any impact if sweep is true.
	 * @param bSweep			Whether we sweep to the destination (currently not supported for rotation).
	 * @param bTeleport			Whether we teleport the physics state (if physics collision is enabled for this object).
	 *							If true, physics velocity for this object is unchanged (so ragdoll parts are not affected by change in location).
	 *							If false, physics velocity is updated based on the change in position (affecting ragdoll parts).
	 */
	UFUNCTION(BlueprintCallable, Category="Utilities|Transformation", meta=(DisplayName="SetRelativeRotation", AdvancedDisplay="bSweep,SweepHitResult,bTeleport"))
	void K2_SetRelativeRotation(FRotator NewRotation, bool bSweep, FHitResult& SweepHitResult, bool bTeleport);
	void SetRelativeRotation(FRotator NewRotation, bool bSweep=false, FHitResult* OutSweepHitResult=nullptr, ETeleportType Teleport = ETeleportType::None);
	void SetRelativeRotation(const FQuat& NewRotation, bool bSweep=false, FHitResult* OutSweepHitResult=nullptr, ETeleportType Teleport = ETeleportType::None);

	/**
	 * Set the transform of the component relative to its parent
	 * @param NewTransform		New transform of the component relative to its parent.
	 * @param SweepHitResult	Hit result from any impact if sweep is true.
	 * @param bSweep			Whether we sweep to the destination (currently not supported for rotation).
	 * @param bTeleport			Whether we teleport the physics state (if physics collision is enabled for this object).
	 *							If true, physics velocity for this object is unchanged (so ragdoll parts are not affected by change in location).
	 *							If false, physics velocity is updated based on the change in position (affecting ragdoll parts).
	 */
	UFUNCTION(BlueprintCallable, Category="Utilities|Transformation", meta=(DisplayName="SetRelativeTransform"))
	void K2_SetRelativeTransform(const FTransform& NewTransform, bool bSweep, FHitResult& SweepHitResult, bool bTeleport);
	void SetRelativeTransform(const FTransform& NewTransform, bool bSweep=false, FHitResult* OutSweepHitResult=nullptr, ETeleportType Teleport = ETeleportType::None);

	/** Returns the transform of the component relative to its parent */
	UFUNCTION(BlueprintCallable, Category="Utilities|Transformation")
	FTransform GetRelativeTransform() const;

	/** Reset the transform of the component relative to its parent. Sets relative location to zero, relative rotation to no rotation, and Scale to 1. */
	UFUNCTION(BlueprintCallable, Category="Utilities|Transformation")
	void ResetRelativeTransform();

	/** Set the non-uniform scale of the component relative to its parent */
	UFUNCTION(BlueprintCallable, Category="Utilities|Transformation")
	virtual void SetRelativeScale3D(FVector NewScale3D);

	/**
	 * Adds a delta to the translation of the component relative to its parent
	 * @param DeltaLocation		Change in location of the component relative to its parent
	 * @param SweepHitResult	Hit result from any impact if sweep is true.
	 * @param bSweep			Whether we sweep to the destination location, triggering overlaps along the way and stopping short of the target if blocked by something.
	 *							Only the root component is swept and checked for blocking collision, child components move without sweeping. If collision is off, this has no effect.
	 * @param bTeleport			Whether we teleport the physics state (if physics collision is enabled for this object).
	 *							If true, physics velocity for this object is unchanged (so ragdoll parts are not affected by change in location).
	 *							If false, physics velocity is updated based on the change in position (affecting ragdoll parts).
	 *							If CCD is on and not teleporting, this will affect objects along the entire sweep volume.
	 */
	UFUNCTION(BlueprintCallable, Category="Utilities|Transformation", meta=(DisplayName="AddRelativeLocation"))
	void K2_AddRelativeLocation(FVector DeltaLocation, bool bSweep, FHitResult& SweepHitResult, bool bTeleport);
	void AddRelativeLocation(FVector DeltaLocation, bool bSweep=false, FHitResult* OutSweepHitResult=nullptr, ETeleportType Teleport = ETeleportType::None);

	/**
	 * Adds a delta the rotation of the component relative to its parent
	 * @param DeltaRotation		Change in rotation of the component relative to is parent.
	 * @param SweepHitResult	Hit result from any impact if sweep is true.
	 * @param bSweep			Whether we sweep to the destination (currently not supported for rotation).
	 * @param bTeleport			Whether we teleport the physics state (if physics collision is enabled for this object).
	 *							If true, physics velocity for this object is unchanged (so ragdoll parts are not affected by change in location).
	 *							If false, physics velocity is updated based on the change in position (affecting ragdoll parts).
	 */
	UFUNCTION(BlueprintCallable, Category="Utilities|Transformation", meta=(DisplayName="AddRelativeRotation", AdvancedDisplay="bSweep,SweepHitResult,bTeleport"))
	void K2_AddRelativeRotation(FRotator DeltaRotation, bool bSweep, FHitResult& SweepHitResult, bool bTeleport);
	void AddRelativeRotation(FRotator DeltaRotation, bool bSweep=false, FHitResult* OutSweepHitResult=nullptr, ETeleportType Teleport = ETeleportType::None);
	void AddRelativeRotation(const FQuat& DeltaRotation, bool bSweep=false, FHitResult* OutSweepHitResult=nullptr, ETeleportType Teleport = ETeleportType::None);

	/**
	 * Adds a delta to the location of the component in its local reference frame
	 * @param DeltaLocation		Change in location of the component in its local reference frame.
	 * @param SweepHitResult	Hit result from any impact if sweep is true.
	 * @param bSweep			Whether we sweep to the destination location, triggering overlaps along the way and stopping short of the target if blocked by something.
	 *							Only the root component is swept and checked for blocking collision, child components move without sweeping. If collision is off, this has no effect.
	 * @param bTeleport			Whether we teleport the physics state (if physics collision is enabled for this object).
	 *							If true, physics velocity for this object is unchanged (so ragdoll parts are not affected by change in location).
	 *							If false, physics velocity is updated based on the change in position (affecting ragdoll parts).
	 *							If CCD is on and not teleporting, this will affect objects along the entire sweep volume.
	 */
	UFUNCTION(BlueprintCallable, Category="Utilities|Transformation", meta=(DisplayName="AddLocalOffset", Keywords="location position"))
	void K2_AddLocalOffset(FVector DeltaLocation, bool bSweep, FHitResult& SweepHitResult, bool bTeleport);
	void AddLocalOffset(FVector DeltaLocation, bool bSweep=false, FHitResult* OutSweepHitResult=nullptr, ETeleportType Teleport = ETeleportType::None);

	
	/**
	 * Adds a delta to the rotation of the component in its local reference frame
	 * @param DeltaRotation		Change in rotation of the component in its local reference frame.
	 * @param SweepHitResult	Hit result from any impact if sweep is true.
	 * @param bSweep			Whether we sweep to the destination (currently not supported for rotation).
	 * @param bTeleport			Whether we teleport the physics state (if physics collision is enabled for this object).
	 *							If true, physics velocity for this object is unchanged (so ragdoll parts are not affected by change in location).
	 *							If false, physics velocity is updated based on the change in position (affecting ragdoll parts).
	 */
	UFUNCTION(BlueprintCallable, Category="Utilities|Transformation", meta=(DisplayName="AddLocalRotation", AdvancedDisplay="bSweep,SweepHitResult,bTeleport"))
	void K2_AddLocalRotation(FRotator DeltaRotation, bool bSweep, FHitResult& SweepHitResult, bool bTeleport);
	void AddLocalRotation(FRotator DeltaRotation, bool bSweep=false, FHitResult* OutSweepHitResult=nullptr, ETeleportType Teleport = ETeleportType::None);
	void AddLocalRotation(const FQuat& DeltaRotation, bool bSweep=false, FHitResult* OutSweepHitResult=nullptr, ETeleportType Teleport = ETeleportType::None);


	/**
	 * Adds a delta to the transform of the component in its local reference frame. Scale is unchanged.
	 * @param DeltaTransform	Change in transform of the component in its local reference frame. Scale is unchanged.
	 * @param SweepHitResult	Hit result from any impact if sweep is true.
	 * @param bSweep			Whether we sweep to the destination location, triggering overlaps along the way and stopping short of the target if blocked by something.
	 *							Only the root component is swept and checked for blocking collision, child components move without sweeping. If collision is off, this has no effect.
	 * @param bTeleport			Whether we teleport the physics state (if physics collision is enabled for this object).
	 *							If true, physics velocity for this object is unchanged (so ragdoll parts are not affected by change in location).
	 *							If false, physics velocity is updated based on the change in position (affecting ragdoll parts).
	 *							If CCD is on and not teleporting, this will affect objects along the entire sweep volume.
	 */
	UFUNCTION(BlueprintCallable, Category="Utilities|Transformation", meta=(DisplayName="AddLocalTransform"))
	void K2_AddLocalTransform(const FTransform& DeltaTransform, bool bSweep, FHitResult& SweepHitResult, bool bTeleport);
	void AddLocalTransform(const FTransform& DeltaTransform, bool bSweep=false, FHitResult* OutSweepHitResult=nullptr, ETeleportType Teleport = ETeleportType::None);


	/**
	 * Put this component at the specified location in world space. Updates relative location to achieve the final world location.
	 * @param NewLocation		New location in world space for the component.
	 * @param SweepHitResult	Hit result from any impact if sweep is true.
	 * @param bSweep			Whether we sweep to the destination location, triggering overlaps along the way and stopping short of the target if blocked by something.
	 *							Only the root component is swept and checked for blocking collision, child components move without sweeping. If collision is off, this has no effect.
	 * @param bTeleport			Whether we teleport the physics state (if physics collision is enabled for this object).
	 *							If true, physics velocity for this object is unchanged (so ragdoll parts are not affected by change in location).
	 *							If false, physics velocity is updated based on the change in position (affecting ragdoll parts).
	 *							If CCD is on and not teleporting, this will affect objects along the entire sweep volume.
	 */
	UFUNCTION(BlueprintCallable, Category="Utilities|Transformation", meta=(DisplayName="SetWorldLocation"))
	void K2_SetWorldLocation(FVector NewLocation, bool bSweep, FHitResult& SweepHitResult, bool bTeleport);
	void SetWorldLocation(FVector NewLocation, bool bSweep=false, FHitResult* OutSweepHitResult=nullptr, ETeleportType Teleport = ETeleportType::None);


	/*
	 * Put this component at the specified rotation in world space. Updates relative rotation to achieve the final world rotation.
	 * @param NewRotation		New rotation in world space for the component.
	 * @param SweepHitResult	Hit result from any impact if sweep is true.
	 * @param bSweep			Whether we sweep to the destination (currently not supported for rotation).
	 * @param bTeleport			Whether we teleport the physics state (if physics collision is enabled for this object).
	 *							If true, physics velocity for this object is unchanged (so ragdoll parts are not affected by change in location).
	 *							If false, physics velocity is updated based on the change in position (affecting ragdoll parts).
	 *							If CCD is on and not teleporting, this will affect objects along the entire sweep volume.
	 */
	UFUNCTION(BlueprintCallable, Category="Utilities|Transformation", meta=(DisplayName="SetWorldRotation", AdvancedDisplay="bSweep,SweepHitResult,bTeleport"))
	void K2_SetWorldRotation(FRotator NewRotation, bool bSweep, FHitResult& SweepHitResult, bool bTeleport);
	void SetWorldRotation(FRotator NewRotation, bool bSweep=false, FHitResult* OutSweepHitResult=nullptr, ETeleportType Teleport = ETeleportType::None);
	void SetWorldRotation(const FQuat& NewRotation, bool bSweep=false, FHitResult* OutSweepHitResult=nullptr, ETeleportType Teleport = ETeleportType::None);


	/**
	 * Set the relative scale of the component to put it at the supplied scale in world space.
	 * @param NewScale		New scale in world space for this component.
	 */
	UFUNCTION(BlueprintCallable, Category="Utilities|Transformation")
	void SetWorldScale3D(FVector NewScale);

	/**
	 * Set the transform of the component in world space.
	 * @param NewTransform		New transform in world space for the component.
	 * @param SweepHitResult	Hit result from any impact if sweep is true.
	 * @param bSweep			Whether we sweep to the destination location, triggering overlaps along the way and stopping short of the target if blocked by something.
	 *							Only the root component is swept and checked for blocking collision, child components move without sweeping. If collision is off, this has no effect.
	 * @param bTeleport			Whether we teleport the physics state (if physics collision is enabled for this object).
	 *							If true, physics velocity for this object is unchanged (so ragdoll parts are not affected by change in location).
	 *							If false, physics velocity is updated based on the change in position (affecting ragdoll parts).
	 *							If CCD is on and not teleporting, this will affect objects along the entire sweep volume.
	 */
	UFUNCTION(BlueprintCallable, Category="Utilities|Transformation", meta=(DisplayName="SetWorldTransform"))
	void K2_SetWorldTransform(const FTransform& NewTransform, bool bSweep, FHitResult& SweepHitResult, bool bTeleport);
	void SetWorldTransform(const FTransform& NewTransform, bool bSweep=false, FHitResult* OutSweepHitResult=nullptr, ETeleportType Teleport = ETeleportType::None);


	/**
	 * Adds a delta to the location of the component in world space.
	 * @param DeltaLocation		Change in location in world space for the component.
	 * @param SweepHitResult	Hit result from any impact if sweep is true.
	 * @param bSweep			Whether we sweep to the destination location, triggering overlaps along the way and stopping short of the target if blocked by something.
	 *							Only the root component is swept and checked for blocking collision, child components move without sweeping. If collision is off, this has no effect.
	 * @param bTeleport			Whether we teleport the physics state (if physics collision is enabled for this object).
	 *							If true, physics velocity for this object is unchanged (so ragdoll parts are not affected by change in location).
	 *							If false, physics velocity is updated based on the change in position (affecting ragdoll parts).
	 *							If CCD is on and not teleporting, this will affect objects along the entire sweep volume.
	 */
	UFUNCTION(BlueprintCallable, Category="Utilities|Transformation", meta=(DisplayName="AddWorldOffset", Keywords="location position"))
	void K2_AddWorldOffset(FVector DeltaLocation, bool bSweep, FHitResult& SweepHitResult, bool bTeleport);
	void AddWorldOffset(FVector DeltaLocation, bool bSweep=false, FHitResult* OutSweepHitResult=nullptr, ETeleportType Teleport = ETeleportType::None);


	/**
	 * Adds a delta to the rotation of the component in world space.
	 * @param DeltaRotation		Change in rotation in world space for the component.
	 * @param SweepHitResult	Hit result from any impact if sweep is true.
	 * @param bSweep			Whether we sweep to the destination (currently not supported for rotation).
	 * @param bTeleport			Whether we teleport the physics state (if physics collision is enabled for this object).
	 *							If true, physics velocity for this object is unchanged (so ragdoll parts are not affected by change in location).
	 *							If false, physics velocity is updated based on the change in position (affecting ragdoll parts).
	 *							If CCD is on and not teleporting, this will affect objects along the entire sweep volume.
	 */
	UFUNCTION(BlueprintCallable, Category="Utilities|Transformation", meta=(DisplayName="AddWorldRotation", AdvancedDisplay="bSweep,SweepHitResult,bTeleport"))
	void K2_AddWorldRotation(FRotator DeltaRotation, bool bSweep, FHitResult& SweepHitResult, bool bTeleport);
	void AddWorldRotation(FRotator DeltaRotation, bool bSweep=false, FHitResult* OutSweepHitResult=nullptr, ETeleportType Teleport = ETeleportType::None);
	void AddWorldRotation(const FQuat& DeltaRotation, bool bSweep=false, FHitResult* OutSweepHitResult=nullptr, ETeleportType Teleport = ETeleportType::None);

	/**
	 * Adds a delta to the transform of the component in world space. Scale is unchanged.
	 * @param DeltaTransform	Change in transform in world space for the component. Scale is unchanged.
	 * @param SweepHitResult	Hit result from any impact if sweep is true.
	 * @param bSweep			Whether we sweep to the destination location, triggering overlaps along the way and stopping short of the target if blocked by something.
	 *							Only the root component is swept and checked for blocking collision, child components move without sweeping. If collision is off, this has no effect.
	 * @param bTeleport			Whether we teleport the physics state (if physics collision is enabled for this object).
	 *							If true, physics velocity for this object is unchanged (so ragdoll parts are not affected by change in location).
	 *							If false, physics velocity is updated based on the change in position (affecting ragdoll parts).
	 *							If CCD is on and not teleporting, this will affect objects along the entire sweep volume.
	 */
	UFUNCTION(BlueprintCallable, Category="Utilities|Transformation", meta=(DisplayName="AddWorldTransform"))
	void K2_AddWorldTransform(const FTransform& DeltaTransform, bool bSweep, FHitResult& SweepHitResult, bool bTeleport);
	void AddWorldTransform(const FTransform& DeltaTransform, bool bSweep=false, FHitResult* OutSweepHitResult=nullptr, ETeleportType Teleport = ETeleportType::None);

	/** Return location of the component, in world space */
	UFUNCTION(BlueprintCallable, meta=(DisplayName = "GetWorldLocation"), Category="Utilities|Transformation")
	FVector K2_GetComponentLocation() const;

	/** Returns rotation of the component, in world space. */
	UFUNCTION(BlueprintCallable, meta=(DisplayName = "GetWorldRotation"), Category="Utilities|Transformation")
	FRotator K2_GetComponentRotation() const;
	
	/** Returns scale of the component, in world space. */
	UFUNCTION(BlueprintCallable, meta=(DisplayName = "GetWorldScale"), Category="Utilities|Transformation")
	FVector K2_GetComponentScale() const;

	/** Get the current component-to-world transform for this component */
	UFUNCTION(BlueprintCallable, meta=(DisplayName = "GetWorldTransform"), Category="Utilities|Transformation")
	FTransform K2_GetComponentToWorld() const;

	/** Get the forward (X) unit direction vector from this component, in world space.  */
	UFUNCTION(BlueprintCallable, Category="Utilities|Transformation")
	FVector GetForwardVector() const;

	/** Get the up (Z) unit direction vector from this component, in world space.  */
	UFUNCTION(BlueprintCallable, Category="Utilities|Transformation")
	FVector GetUpVector() const;

	/** Get the right (Y) unit direction vector from this component, in world space.  */
	UFUNCTION(BlueprintCallable, Category="Utilities|Transformation")
	FVector GetRightVector() const;

	/** Returns whether the specified body is currently using physics simulation */
	UFUNCTION(BlueprintCallable, Category="Physics")
	virtual bool IsSimulatingPhysics(FName BoneName = NAME_None) const;

	/** Returns whether the specified body is currently using physics simulation */
	UFUNCTION(BlueprintCallable, Category="Physics")
	virtual bool IsAnySimulatingPhysics() const;

public:

	/** Get the SceneComponents that are attached to this component. */
	const TArray<USceneComponent*>& GetAttachChildren() const;

	/** Get the SceneComponent we are attached to. */
	UFUNCTION(BlueprintCallable, Category="Utilities|Transformation")
	USceneComponent* GetAttachParent() const;

	/** Get the socket we are attached to. */
	UFUNCTION(BlueprintCallable, Category="Utilities|Transformation")
	FName GetAttachSocketName() const;

	/** Gets all parent components up to and including the root component */
	UFUNCTION(BlueprintCallable, Category="Components")
	void GetParentComponents(TArray<USceneComponent*>& Parents) const;

	/** Gets the number of attached children components */
	UFUNCTION(BlueprintCallable, Category="Components")
	int32 GetNumChildrenComponents() const;

	/** Gets the attached child component at the specified location */
	UFUNCTION(BlueprintCallable, Category="Components")
	USceneComponent* GetChildComponent(int32 ChildIndex) const;

	/** 
	 * Gets all the attached child components
	 * @param bIncludeAllDescendants Whether to include all descendants in the list of children (i.e. grandchildren, great grandchildren, etc.)
	 * @param Children The list of attached child components
	 */
	UFUNCTION(BlueprintCallable, Category="Components")
	void GetChildrenComponents(bool bIncludeAllDescendants, TArray<USceneComponent*>& Children) const;

	/** 
	* Initializes desired Attach Parent and SocketName to be attached to when the component is registered.
	* Generally intended to be called from its Owning Actor's constructor and should be preferred over AttachToComponent when
	* a component is not registered.
	* @param  InParent				Parent to attach to.
	* @param  InSocketName			Optional socket to attach to on the parent.
	*/
	void SetupAttachment(USceneComponent* InParent, FName InSocketName = NAME_None);

	/** Backwards compatibility: Used to convert old-style EAttachLocation to new-style EAttachmentRules */
	static void ConvertAttachLocation(EAttachLocation::Type InAttachLocation, EAttachmentRule& InOutLocationRule, EAttachmentRule& InOutRotationRule, EAttachmentRule& InOutScaleRule);

	/**
	 * Attach this component to another scene component, optionally at a named socket. It is valid to call this on components whether or not they have been Registered.
	 * @param  InParent				Parent to attach to.
	 * @param  InSocketName			Optional socket to attach to on the parent.
	 * @param  AttachType			How to handle transform when attaching (Keep relative offset, keep world position, etc).
	 * @param  bWeldSimulatedBodies Whether to weld together simulated physics bodies.
	 * @return True if attachment is successful (or already attached to requested parent/socket), false if attachment is rejected and there is no change in AttachParent.
	 */
	DEPRECATED(4.12, "This function is deprecated, please use AttachToComponent instead.")
	bool AttachTo(USceneComponent* InParent, FName InSocketName = NAME_None, EAttachLocation::Type AttachType = EAttachLocation::KeepRelativeOffset, bool bWeldSimulatedBodies = false);

	/**
	 * Attach this component to another scene component, optionally at a named socket. It is valid to call this on components whether or not they have been Registered.
	 * @param  InParent				Parent to attach to.
	 * @param  InSocketName			Optional socket to attach to on the parent.
	 * @param  AttachType			How to handle transform when attaching (Keep relative offset, keep world position, etc).
	 * @param  bWeldSimulatedBodies Whether to weld together simulated physics bodies.
	 * @return True if attachment is successful (or already attached to requested parent/socket), false if attachment is rejected and there is no change in AttachParent.
	*/
	DEPRECATED(4.17, "This function is deprecated, please use AttachToComponent instead.")
	UFUNCTION(BlueprintCallable, Category = "Utilities|Transformation", meta = (DisplayName = "AttachTo (Deprecated)", AttachType = "KeepRelativeOffset"))
	bool K2_AttachTo(USceneComponent* InParent, FName InSocketName = NAME_None, EAttachLocation::Type AttachType = EAttachLocation::KeepRelativeOffset, bool bWeldSimulatedBodies = true);

	/**
	* Attach this component to another scene component, optionally at a named socket. It is valid to call this on components whether or not they have been Registered, however from
	* constructor or when not registered it is preferable to use SetupAttachment.
	* @param  Parent				Parent to attach to.
	* @param  AttachmentRules		How to handle transforms & welding when attaching.
	* @param  SocketName			Optional socket to attach to on the parent.
	* @return True if attachment is successful (or already attached to requested parent/socket), false if attachment is rejected and there is no change in AttachParent.
	*/
	bool AttachToComponent(USceneComponent* InParent, const FAttachmentTransformRules& AttachmentRules, FName InSocketName = NAME_None );

	/**
	* Attach this component to another scene component, optionally at a named socket. It is valid to call this on components whether or not they have been Registered.
	* @param  Parent					Parent to attach to.
	* @param  SocketName				Optional socket to attach to on the parent.
	* @param  LocationRule				How to handle translation when attaching.
	* @param  RotationRule				How to handle rotation when attaching.
	* @param  ScaleRule					How to handle scale when attaching.
	* @param  bWeldSimulatedBodies		Whether to weld together simulated physics bodies.
	* @return True if attachment is successful (or already attached to requested parent/socket), false if attachment is rejected and there is no change in AttachParent.
	*/
	UFUNCTION(BlueprintCallable, Category = "Utilities|Transformation", meta = (DisplayName = "AttachToComponent", bWeldSimulatedBodies=true))
	bool K2_AttachToComponent(USceneComponent* Parent, FName SocketName, EAttachmentRule LocationRule, EAttachmentRule RotationRule, EAttachmentRule ScaleRule, bool bWeldSimulatedBodies);

	/** Zeroes out the relative transform of the component, and calls AttachTo(). Useful for attaching directly to a scene component or socket location  */
	DEPRECATED(4.17, "Use AttachToComponent instead.")
	UFUNCTION(BlueprintCallable, meta=(DeprecatedFunction, DeprecationMessage = "Use AttachToComponent instead."), Category="Utilities|Transformation")
	bool SnapTo(USceneComponent* InParent, FName InSocketName = NAME_None);

	/** 
	 * Detach this component from whatever it is attached to. Automatically unwelds components that are welded together (See WeldTo)
	 * @param bMaintainWorldPosition	If true, update the relative location of the component to keep its world position the same
	 * @param bCallModify				If true, call Modify() on the component and the current attach parent component
	 */
	DEPRECATED(4.12, "This function is deprecated, please use DetachFromComponent instead.")
	UFUNCTION(BlueprintCallable, Category="Utilities|Transformation", meta = (DisplayName = "DetachFromParent (Deprecated)"))
	virtual void DetachFromParent(bool bMaintainWorldPosition = false, bool bCallModify = true);

	/** 
	 * Detach this component from whatever it is attached to. Automatically unwelds components that are welded together (See WeldTo)
	 * @param LocationRule				How to handle translations when detaching.
	 * @param RotationRule				How to handle rotation when detaching.
	 * @param ScaleRule					How to handle scales when detaching.
	 * @param bCallModify				If true, call Modify() on the component and the current attach parent component
	 */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "DetachFromComponent"), Category = "Utilities|Transformation")
	void K2_DetachFromComponent(EDetachmentRule LocationRule = EDetachmentRule::KeepRelative, EDetachmentRule RotationRule = EDetachmentRule::KeepRelative, EDetachmentRule ScaleRule = EDetachmentRule::KeepRelative, bool bCallModify = true);

	/** 
	 * Detach this component from whatever it is attached to. Automatically unwelds components that are welded together (See WeldTo)
	 * @param DetachmentRules			How to handle transforms & modification when detaching.
	 */
	virtual void DetachFromComponent(const FDetachmentTransformRules& DetachmentRules);

	/** 
	 * Gets the names of all the sockets on the component.
	 * @return Get the names of all the sockets on the component.
	 */
	UFUNCTION(BlueprintCallable, Category="Utilities|Transformation", meta=(Keywords="Bone"))
	TArray<FName> GetAllSocketNames() const;

	/** 
	 * Get world-space socket transform.
	 * @param InSocketName Name of the socket or the bone to get the transform 
	 * @return Socket transform in world space if socket if found. Otherwise it will return component's transform in world space.
	 */
	UFUNCTION(BlueprintCallable, Category="Utilities|Transformation", meta=(Keywords="Bone"))
	virtual FTransform GetSocketTransform(FName InSocketName, ERelativeTransformSpace TransformSpace = RTS_World) const;

	/** 
	 * Get world-space socket or bone location.
	 * @param InSocketName Name of the socket or the bone to get the transform 
	 * @return Socket transform in world space if socket if found. Otherwise it will return component's transform in world space.
	 */
	UFUNCTION(BlueprintCallable, Category="Utilities|Transformation", meta=(Keywords="Bone"))
	virtual FVector GetSocketLocation(FName InSocketName) const;

	/** 
	 * Get world-space socket or bone  FRotator rotation.
	 * @param InSocketName Name of the socket or the bone to get the transform 
	 * @return Socket transform in world space if socket if found. Otherwise it will return component's transform in world space.
	 */
	UFUNCTION(BlueprintCallable, Category="Utilities|Transformation", meta=(Keywords="Bone"))
	virtual FRotator GetSocketRotation(FName InSocketName) const;

	/** 
	 * Get world-space socket or bone FQuat rotation.
	 * @param InSocketName Name of the socket or the bone to get the transform 
	 * @return Socket transform in world space if socket if found. Otherwise it will return component's transform in world space.
	 */
	UFUNCTION(BlueprintCallable, Category="Utilities|Transformation", meta=(Keywords="Bone", DeprecatedFunction, DeprecationMessage="Use GetSocketRotation instead, Quat is not fully supported in blueprints."))
	virtual FQuat GetSocketQuaternion(FName InSocketName) const;

	/** 
	 * return true if socket with the given name exists
	 * @param InSocketName Name of the socket or the bone to get the transform 
	 * @return true if the socket with the given name exists. Otherwise, return false
	 */
	UFUNCTION(BlueprintCallable, Category="Utilities|Transformation", meta=(Keywords="Bone"))
	virtual bool DoesSocketExist(FName InSocketName) const;

	/**
	 * Returns true if this component has any sockets
	 */
	virtual bool HasAnySockets() const;

	/**
	 * Get a list of sockets this component contains
	 */
	virtual void QuerySupportedSockets(TArray<FComponentSocketDescription>& OutSockets) const;

	/** 
	 * Get velocity of the component: either ComponentVelocity, or the velocity of the physics body if simulating physics.
	 * @return Velocity of the component
	 */
	UFUNCTION(BlueprintCallable, Category="Utilities|Transformation")
	virtual FVector GetComponentVelocity() const;

	/** 
	 * Is this component visible or not in game
	 * @return true if visible
	 */
	UFUNCTION(BlueprintCallable, Category="Rendering")
	virtual bool IsVisible() const;

protected:

	/**
	 * Overridable internal function to respond to changes in the visibility of the component.
	 */
	virtual void OnVisibilityChanged();

	/**
	* Overridable internal function to respond to changes in the hidden in game value of the component.
	*/
	virtual void OnHiddenInGameChanged();

private:

	/** 
	 * Enum that dictates what propagation policy to follow when calling SetVisibility or SetHiddenInGame recursively 
	 */
	enum class EVisibilityPropagation : uint8
	{
		// Only change the visibility if needed
		NoPropagation, 

		// If the visibility changed, mark all attached component's render states as dirty
		DirtyOnly,

		// Call function recursively on attached components and also mark their render state as dirty
		Propagate
	};

	/**
	 * Internal function to set visibility of the component. Enum controls propagation rules.
	 */
	void SetVisibility(bool bNewVisibility, EVisibilityPropagation PropagateToChildren);

	/**
	* Internal function to set hidden in game for the component. Enum controls propagation rules.
	*/
	void SetHiddenInGame(bool bNewHiddenInGame, EVisibilityPropagation PropagateToChildren);

public:

	/** 
	 * Set visibility of the component, if during game use this to turn on/off
	 */
	UFUNCTION(BlueprintCallable, Category="Rendering")
	void SetVisibility(bool bNewVisibility, bool bPropagateToChildren=false)
	{
		SetVisibility(bNewVisibility, bPropagateToChildren ? EVisibilityPropagation::Propagate : EVisibilityPropagation::DirtyOnly);
	}

	/** 
	 * Toggle visibility of the component
	 */
	UFUNCTION(BlueprintCallable, Category="Rendering")
	void ToggleVisibility(bool bPropagateToChildren = false)
	{
		SetVisibility(!bVisible, bPropagateToChildren);
	}

	/**
	 * Changes the value of HiddenGame.
	 * @param NewHidden	- The value to assign to HiddenGame.
	 */
	UFUNCTION(BlueprintCallable, Category="Development")
	void SetHiddenInGame(bool NewHidden, bool bPropagateToChildren=false)
	{
		SetHiddenInGame(NewHidden, bPropagateToChildren ? EVisibilityPropagation::Propagate : EVisibilityPropagation::DirtyOnly);
	}

public:
	FTransformUpdated TransformUpdated;

	//~ Begin ActorComponent Interface
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	/** Return true if CreateRenderState() should be called */
	virtual bool ShouldCreateRenderState() const override
	{
		return true;
	}
	virtual void UpdateComponentToWorld(EUpdateTransformFlags UpdateTransformFlags = EUpdateTransformFlags::None, ETeleportType Teleport = ETeleportType::None) override final
	{
		UpdateComponentToWorldWithParent(GetAttachParent(), GetAttachSocketName(), UpdateTransformFlags, RelativeRotationCache.RotatorToQuat(RelativeRotation), Teleport);
	}
	virtual void DestroyComponent(bool bPromoteChildren = false) override;
	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;
	virtual void ApplyWorldOffset(const FVector& InOffset, bool bWorldShift) override;
	virtual class FActorComponentInstanceData* GetComponentInstanceData() const override;
	//~ End ActorComponent Interface

	// Call UpdateComponentToWorld if bComponentToWorldUpdated is false.
	void ConditionalUpdateComponentToWorld();

	//~ Begin UObject Interface
	virtual void PostInterpChange(UProperty* PropertyThatChanged) override;
	virtual void BeginDestroy() override;
#if WITH_EDITORONLY_DATA
	virtual void Serialize(FArchive& Ar) override;
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);
#endif

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
#endif

	//~ End UObject Interface
protected:
	/**
	 * Internal helper, for use from MoveComponent().  Special codepath since the normal setters call MoveComponent.
	 * @return: true if location or rotation was changed.
	 */
	bool InternalSetWorldLocationAndRotation(FVector NewLocation, const FQuat& NewQuat, bool bNoPhysics = false, ETeleportType Teleport = ETeleportType::None);

	virtual void OnUpdateTransform(EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport = ETeleportType::None);

	/** Check if mobility is set to non-static. If it's static we trigger a PIE warning and return true*/
	bool CheckStaticMobilityAndWarn(const FText& ActionText) const;

private:

	void PropagateTransformUpdate(bool bTransformChanged, EUpdateTransformFlags UpdateTransformFlags = EUpdateTransformFlags::None, ETeleportType Teleport = ETeleportType::None);
	void UpdateComponentToWorldWithParent(USceneComponent* Parent, FName SocketName, EUpdateTransformFlags UpdateTransformFlags, const FQuat& RelativeRotationQuat, ETeleportType Teleport = ETeleportType::None);


public:

	/** Queries world and updates overlap tracking state for this component */
	virtual void UpdateOverlaps(TArray<FOverlapInfo> const* PendingOverlaps=nullptr, bool bDoNotifies=true, const TArray<FOverlapInfo>* OverlapsAtEndLocation=nullptr);

	/**
	 * Tries to move the component by a movement vector (Delta) and sets rotation to NewRotation.
	 * Assumes that the component's current location is valid and that the component does fit in its current Location.
	 * Dispatches blocking hit notifications (if bSweep is true), and calls UpdateOverlaps() after movement to update overlap state.
	 *
	 * @note This simply calls the virtual MoveComponentImpl() which can be overridden to implement custom behavior.
	 * @note The overload taking rotation as an FQuat is slightly faster than the version using FRotator (which will be converted to an FQuat)..
	 * @param Delta			The desired location change in world space.
	 * @param NewRotation	The new desired rotation in world space.
	 * @param bSweep		Whether we sweep to the destination location, triggering overlaps along the way and stopping short of the target if blocked by something.
	 *						Only the root component is swept and checked for blocking collision, child components move without sweeping. If collision is off, this has no effect.
	 * @param Teleport		Whether we teleport the physics state (if physics collision is enabled for this object).
	 *						If TeleportPhysics, physics velocity for this object is unchanged (so ragdoll parts are not affected by change in location).
	 *						If None, physics velocity is updated based on the change in position (affecting ragdoll parts).
	 *						If CCD is on and not teleporting, this will affect objects along the entire swept volume.
	 * @param Hit			Optional output describing the blocking hit that stopped the move, if any.
	 * @param MoveFlags		Flags controlling behavior of the move. @see EMoveComponentFlags
	 * @param Teleport      Determines whether to teleport the physics body or not. Teleporting will maintain constant velocity and avoid collisions along the path
	 * @return				True if some movement occurred, false if no movement occurred.
	 */
	bool MoveComponent( const FVector& Delta, const FQuat& NewRotation,    bool bSweep, FHitResult* Hit=NULL, EMoveComponentFlags MoveFlags = MOVECOMP_NoFlags, ETeleportType Teleport = ETeleportType::None);
	bool MoveComponent( const FVector& Delta, const FRotator& NewRotation, bool bSweep, FHitResult* Hit=NULL, EMoveComponentFlags MoveFlags = MOVECOMP_NoFlags, ETeleportType Teleport = ETeleportType::None);

protected:

	// Override this method for custom behavior.
	virtual bool MoveComponentImpl(const FVector& Delta, const FQuat& NewRotation, bool bSweep, FHitResult* Hit = NULL, EMoveComponentFlags MoveFlags = MOVECOMP_NoFlags, ETeleportType Teleport = ETeleportType::None);

public:


	/** Returns true if movement is currently within the scope of an FScopedMovementUpdate. */
	bool IsDeferringMovementUpdates() const;

#if WITH_EDITORONLY_DATA
protected:
	/** Editor only component used to display the sprite so as to be able to see the location of the Audio Component  */
	class UBillboardComponent* SpriteComponent;
#endif

public:

	/** Called when AttachParent changes, to allow the scene to update its attachment state. */
	virtual void OnAttachmentChanged() {}

	/** Return location of the component, in world space */
	FORCEINLINE FVector GetComponentLocation() const
	{
		return GetComponentTransform().GetLocation();
	}

	/** Return rotation of the component, in world space */
	FORCEINLINE FRotator GetComponentRotation() const
	{
		return WorldRotationCache.NormalizedQuatToRotator(GetComponentTransform().GetRotation());
	}

	/** Return rotation quaternion of the component, in world space */
	FORCEINLINE FQuat GetComponentQuat() const
	{
		return GetComponentTransform().GetRotation();
	}

	/** Return scale of the component, in world space */
	FORCEINLINE FVector GetComponentScale() const
	{
		return GetComponentTransform().GetScale3D();
	}

	/** Sets the cached component to world directly. This should be used very rarely. */
	FORCEINLINE void SetComponentToWorld(const FTransform& NewComponentToWorld)
	{
		bComponentToWorldUpdated = true;
		ComponentToWorld = NewComponentToWorld;
	}

	/** 
	 * Get the current component-to-world transform for this component 
	 * TODO: probably deprecate this in favor of GetComponentTransform
	 */
	FORCEINLINE const FTransform& GetComponentToWorld() const 
	{ 
		return ComponentToWorld;
	}

	/** Get the current component-to-world transform for this component */
	FORCEINLINE const FTransform& GetComponentTransform() const
	{
		return ComponentToWorld;
	}

	/** Update transforms of any components attached to this one. */
	void UpdateChildTransforms(EUpdateTransformFlags UpdateTransformFlags = EUpdateTransformFlags::None, ETeleportType Teleport = ETeleportType::None);

	/** Calculate the bounds of the component. Default behavior is a bounding box/sphere of zero size. */
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const;

	/**
	 * Calculate the axis-aligned bounding cylinder of the component (radius in X-Y, half-height along Z axis).
	 * Default behavior is just a cylinder around the box of the cached BoxSphereBounds.
	 */
	virtual void CalcBoundingCylinder(float& CylinderRadius, float& CylinderHalfHeight) const;

	/** Update the Bounds of the component.*/
	void UpdateBounds();

	/** If true, bounds should be used when placing component/actor in level. Does not affect spawning. */
	virtual bool ShouldCollideWhenPlacing() const
	{
		return false;
	}

	/** 
	 * Updates the PhysicsVolume of this SceneComponent, if bShouldUpdatePhysicsVolume is true.
	 * 
	 * @param bTriggerNotifiers		if true, send zone/volume change events
	 */
	virtual void UpdatePhysicsVolume( bool bTriggerNotifiers );

	/**
	 * Replace current PhysicsVolume to input NewVolume
	 *
	 * @param NewVolume				NewVolume to replace
	 * @param bTriggerNotifiers		if true, send zone/volume change events
	 */
	void SetPhysicsVolume( APhysicsVolume * NewVolume,  bool bTriggerNotifiers );

	/** 
	 * Get the PhysicsVolume overlapping this component.
	 */
	UFUNCTION(BlueprintCallable, Category=PhysicsVolume, meta=(UnsafeDuringActorConstruction="true"))
	APhysicsVolume* GetPhysicsVolume() const;


	/** Return const reference to CollsionResponseContainer */
	virtual const FCollisionResponseContainer& GetCollisionResponseToChannels() const;

	/** Return true if visible in editor **/
	virtual bool IsVisibleInEditor() const;

	/** return true if it should render **/
	bool ShouldRender() const;

	/** return true if it can ever render **/
	bool CanEverRender() const;

	/** 
	 *  Looking at various values of the component, determines if this
	 *  component should be added to the scene
	 * @return true if the component is visible and should be added to the scene, false otherwise
	 */
	bool ShouldComponentAddToScene() const;

#if WITH_EDITOR
	/** Called when this component is moved in the editor */
	virtual void PostEditComponentMove(bool bFinished) {}
	virtual bool CanEditChange( const UProperty* Property ) const override;

	virtual const int32 GetNumUncachedStaticLightingInteractions() const;

	virtual void PreFeatureLevelChange(ERHIFeatureLevel::Type PendingFeatureLevel) {}

	/** 
	 * Supplies the editor with a view specific to this component (think a view 
	 * from a camera components POV, etc.). Used for PIP preview windows.
	 * @return True if the component overrides this, and fills out the view info output.
	 */
	virtual bool GetEditorPreviewInfo(float DeltaTime, FMinimalViewInfo& ViewOut) { return false; }
#endif // WITH_EDITOR

protected:

	/** Calculate the new ComponentToWorld transform for this component.
		Parent is optional and can be used for computing ComponentToWorld based on arbitrary USceneComponent.
		If Parent is not passed in we use the component's AttachParent*/
	FORCEINLINE FTransform CalcNewComponentToWorld(const FTransform& NewRelativeTransform, const USceneComponent* Parent = nullptr, FName SocketName = NAME_None) const
	{
		SocketName = Parent ? SocketName : GetAttachSocketName();
		Parent = Parent ? Parent : GetAttachParent();
		if (Parent)
		{
			const bool bGeneral = bAbsoluteLocation || bAbsoluteRotation || bAbsoluteScale;
			if (!bGeneral)
			{
				return NewRelativeTransform * Parent->GetSocketTransform(SocketName);
			}
			
			return CalcNewComponentToWorld_GeneralCase(NewRelativeTransform, Parent, SocketName);
		}
		else
		{
			return NewRelativeTransform;
		}
	}

	FTransform CalcNewComponentToWorld_GeneralCase(const FTransform& NewRelativeTransform, const USceneComponent* Parent, FName SocketName) const;

	
public:
	/**
	 * Set the location and rotation of the component relative to its parent
	 * @param NewLocation		New location of the component relative to its parent.
	 * @param NewRotation		New rotation of the component relative to its parent.
	 * @param SweepHitResult	Hit result from any impact if sweep is true.
	 * @param bSweep			Whether we sweep to the destination location, triggering overlaps along the way and stopping short of the target if blocked by something.
	 *							Only the root component is swept and checked for blocking collision, child components move without sweeping. If collision is off, this has no effect.
	 * @param bTeleport			Whether we teleport the physics state (if physics collision is enabled for this object).
	 *							If true, physics velocity for this object is unchanged (so ragdoll parts are not affected by change in location).
	 *							If false, physics velocity is updated based on the change in position (affecting ragdoll parts).
	 *							If CCD is on and not teleporting, this will affect objects along the entire sweep volume.
	 */
	UFUNCTION(BlueprintCallable, Category="Utilities|Transformation", meta=(DisplayName="SetRelativeLocationAndRotation"))
	void K2_SetRelativeLocationAndRotation(FVector NewLocation, FRotator NewRotation, bool bSweep, FHitResult& SweepHitResult, bool bTeleport);
	void SetRelativeLocationAndRotation(FVector NewLocation, FRotator NewRotation, bool bSweep=false, FHitResult* OutSweepHitResult=nullptr, ETeleportType Teleport = ETeleportType::None);
	void SetRelativeLocationAndRotation(FVector NewLocation, const FQuat& NewRotation, bool bSweep=false, FHitResult* OutSweepHitResult=nullptr, ETeleportType Teleport = ETeleportType::None);

	/** Set which parts of the relative transform should be relative to parent, and which should be relative to world */
	UFUNCTION(BlueprintCallable, Category="Utilities|Transformation")
	void SetAbsolute(bool bNewAbsoluteLocation = false, bool bNewAbsoluteRotation = false, bool bNewAbsoluteScale = false);

	/**
	 * Set the relative location and rotation of the component to put it at the supplied pose in world space.
	 * @param NewLocation		New location in world space for the component.
	 * @param NewRotation		New rotation in world space for the component.
	 * @param SweepHitResult	Hit result from any impact if sweep is true.
	 * @param bSweep			Whether we sweep to the destination location, triggering overlaps along the way and stopping short of the target if blocked by something.
	 *							Only the root component is swept and checked for blocking collision, child components move without sweeping. If collision is off, this has no effect.
	 * @param bTeleport			Whether we teleport the physics state (if physics collision is enabled for this object).
	 *							If true, physics velocity for this object is unchanged (so ragdoll parts are not affected by change in location).
	 *							If false, physics velocity is updated based on the change in position (affecting ragdoll parts).
	 *							If CCD is on and not teleporting, this will affect objects along the entire sweep volume.
	 */
	UFUNCTION(BlueprintCallable, Category="Utilities|Transformation", meta=(DisplayName="SetWorldLocationAndRotation"))
	void K2_SetWorldLocationAndRotation(FVector NewLocation, FRotator NewRotation, bool bSweep, FHitResult& SweepHitResult, bool bTeleport);
	void SetWorldLocationAndRotation(FVector NewLocation, FRotator NewRotation, bool bSweep=false, FHitResult* OutSweepHitResult=nullptr, ETeleportType Teleport = ETeleportType::None);

	/** Set the relative location and FQuat rotation of the component to put it at the supplied pose in world space. */
	void SetWorldLocationAndRotation(FVector NewLocation, const FQuat& NewRotation, bool bSweep=false, FHitResult* OutSweepHitResult=nullptr, ETeleportType Teleport = ETeleportType::None);

	/** Special version of SetWorldLocationAndRotation that does not affect physics. */
	void SetWorldLocationAndRotationNoPhysics(const FVector& NewLocation, const FRotator& NewRotation);

	/** Is this component considered 'world' geometry */
	virtual bool IsWorldGeometry() const;

	/** Returns the form of collision for this component */
	virtual ECollisionEnabled::Type GetCollisionEnabled() const;

	/** Utility to see if there is any form of collision (query or physics) enabled on this component. */
	FORCEINLINE_DEBUGGABLE bool IsCollisionEnabled() const
	{
		return GetCollisionEnabled() != ECollisionEnabled::NoCollision;
	}

	/** Utility to see if there is any query collision enabled on this component. */
	FORCEINLINE_DEBUGGABLE bool IsQueryCollisionEnabled() const
	{
		return CollisionEnabledHasQuery(GetCollisionEnabled());
	}

	/** Utility to see if there is any physics collision enabled on this component. */
	FORCEINLINE_DEBUGGABLE bool IsPhysicsCollisionEnabled() const
	{
		return CollisionEnabledHasPhysics(GetCollisionEnabled());
	}

	/** Returns the response that this component has to a specific collision channel. */
	virtual ECollisionResponse GetCollisionResponseToChannel(ECollisionChannel Channel) const;

	/** Returns the channel that this component belongs to when it moves. */
	virtual ECollisionChannel GetCollisionObjectType() const;

	/** Compares the CollisionObjectType of each component against the Response of the other, to see what kind of response we should generate */
	ECollisionResponse GetCollisionResponseToComponent(USceneComponent* OtherComponent) const;

	/** Set how often this component is allowed to move during runtime. Causes a component re-register if the component is already registered */
	virtual void SetMobility(EComponentMobility::Type NewMobility);

	/** Walks up the attachment chain from this SceneComponent and returns the SceneComponent at the top. If AttachParent is NULL, returns this. */
	USceneComponent* GetAttachmentRoot() const;
	
	/** Walks up the attachment chain from this SceneComponent and returns the top-level actor it's attached to.  Returns Owner if unattached. */
	AActor* GetAttachmentRootActor() const;

	/** Walks up the attachment chain to see if this component is attached to the supplied component. If TestComp == this, returns false.*/
	bool IsAttachedTo(USceneComponent* TestComp) const;

	/**
	 * Find the world-space location and rotation of the given named socket.
	 * If the socket is not found, then it returns the component's location and rotation in world space.
	 * @param InSocketName the name of the socket to find
	 * @param OutLocation (out) set to the world space location of the socket
	 * @param OutRotation (out) set to the world space rotation of the socket
	 * @return whether or not the socket was found
	 */
	void GetSocketWorldLocationAndRotation(FName InSocketName, FVector& OutLocation, FRotator& OutRotation) const;
	void GetSocketWorldLocationAndRotation(FName InSocketName, FVector& OutLocation, FQuat& OutRotation) const;

	/**
	 * Called to see if it's possible to attach another scene component as a child.
	 * Note: This can be called on template component as well!
	 */
	virtual bool CanAttachAsChild(USceneComponent* ChildComponent, FName SocketName) const { return true; }

	/** Get the extent used when placing this component in the editor, used for 'pulling back' hit. */
	virtual FBoxSphereBounds GetPlacementExtent() const;

protected:
	/**
	 * Called after a child scene component is attached to this component.
	 * Note: Do not change the attachment state of the child during this call.
	 */
	virtual void OnChildAttached(USceneComponent* ChildComponent) {}

	/**
	 * Called after a child scene component is detached from this component.
	 * Note: Do not change the attachment state of the child during this call.
	 */
	virtual void OnChildDetached(USceneComponent* ChildComponent) {}

	/** Called after changing transform, tries to update navigation octree for this component */
	void UpdateNavigationData();

	/** Called after changing transform, tries to update navigation octree for owner */
	void PostUpdateNavigationData();

	/**
	 * Determine if dynamic data is allowed to be changed.
	 * 
	 * @param bIgnoreStationary Whether or not to ignore stationary mobility when checking. Default is true (i.e. - check for static mobility only).
	 * @return Whether or not dynamic data is allowed to be changed.
	 */
	FORCEINLINE bool AreDynamicDataChangesAllowed(bool bIgnoreStationary = true) const
	{
		return (IsOwnerRunningUserConstructionScript()) || !(IsRegistered() && (Mobility == EComponentMobility::Static || (!bIgnoreStationary && Mobility == EComponentMobility::Stationary)));
	}
public:
	/** Determines whether or not the component can have its mobility set to static */
	virtual const bool CanHaveStaticMobility() const { return true; }

	virtual void PropagateLightingScenarioChange() {}

	virtual bool IsPrecomputedLightingValid() const
	{
		return false;
	}

private:

	friend struct FDirectAttachChildrenAccessor;
};

/** 
  * Struct to allow direct access to the AttachChildren array for a handful of cases that will require more work than can be done  
  * immediately to fix up in light of the privatization steps
  */
struct FDirectAttachChildrenAccessor
{
private:
	static TArray<USceneComponent*>& Get(USceneComponent* Component)
	{ 
		return Component->AttachChildren;
	}

	friend class UChildActorComponent;
	friend class FBlueprintThumbnailScene;
	friend class FClassThumbnailScene;
	friend class FComponentEditorUtils;
	friend class FBlueprintCompileReinstancer;
	friend struct FResetSceneComponentAfterCopy;
};


//////////////////////////////////////////////////////////////////////////
// USceneComponent inlines

FORCEINLINE const TArray<USceneComponent*>& USceneComponent::GetAttachChildren() const
{
	return AttachChildren;
}

FORCEINLINE USceneComponent* USceneComponent::GetAttachParent() const
{
	return AttachParent;
}

FORCEINLINE FName USceneComponent::GetAttachSocketName() const
{
	return AttachSocketName;
}

FORCEINLINE_DEBUGGABLE void USceneComponent::ConditionalUpdateComponentToWorld()
{
	if (!bComponentToWorldUpdated)
	{
		UpdateComponentToWorld();
	}
}

FORCEINLINE_DEBUGGABLE bool USceneComponent::MoveComponent(const FVector& Delta, const FQuat& NewRotation, bool bSweep, FHitResult* Hit, EMoveComponentFlags MoveFlags, ETeleportType Teleport)
{
	return MoveComponentImpl(Delta, NewRotation, bSweep, Hit, MoveFlags, Teleport);
}

FORCEINLINE_DEBUGGABLE void USceneComponent::SetRelativeLocation(FVector NewLocation, bool bSweep, FHitResult* OutSweepHitResult, ETeleportType Teleport)
{
	SetRelativeLocationAndRotation(NewLocation, RelativeRotationCache.RotatorToQuat(RelativeRotation), bSweep, OutSweepHitResult, Teleport);
}

FORCEINLINE_DEBUGGABLE void USceneComponent::SetRelativeRotation(const FQuat& NewRotation, bool bSweep, FHitResult* OutSweepHitResult, ETeleportType Teleport)
{
	SetRelativeLocationAndRotation(RelativeLocation, NewRotation, bSweep, OutSweepHitResult, Teleport);
}

FORCEINLINE_DEBUGGABLE void USceneComponent::AddRelativeLocation(FVector DeltaLocation, bool bSweep, FHitResult* OutSweepHitResult, ETeleportType Teleport)
{
	SetRelativeLocationAndRotation(RelativeLocation + DeltaLocation, RelativeRotationCache.RotatorToQuat(RelativeRotation), bSweep, OutSweepHitResult, Teleport);
}

FORCEINLINE_DEBUGGABLE void USceneComponent::AddRelativeRotation(FRotator DeltaRotation, bool bSweep, FHitResult* OutSweepHitResult, ETeleportType Teleport)
{
	SetRelativeRotation(RelativeRotation + DeltaRotation, bSweep, OutSweepHitResult, Teleport);
}

//////////////////////////////////////////////////////////////////////////

/** 
 *  Component instance cached data base class for scene components. 
 *  Stores a list of instance components attached to the 
 */
class ENGINE_API FSceneComponentInstanceData : public FActorComponentInstanceData
{
public:
	FSceneComponentInstanceData(const USceneComponent* SourceComponent);
			
	virtual ~FSceneComponentInstanceData()
	{}

	virtual void ApplyToComponent(UActorComponent* Component, const ECacheApplyPhase CacheApplyPhase) override;
	virtual void FindAndReplaceInstances(const TMap<UObject*, UObject*>& OldToNewInstanceMap) override;
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;

	TArray<TPair<USceneComponent*, FTransform>> AttachedInstanceComponents;
};


//////////////////////////////////////////////////////////////////////////

/**
 * Utility for temporarily changing the behavior of a SceneComponent to use absolute transforms, and then restore it to the behavior at the start of the scope.
 */
class ENGINE_API FScopedPreventAttachedComponentMove : private FNoncopyable
{
public:

	/**
	 * Init scoped behavior for a given Component.
	 * Note that null is perfectly acceptable here (does nothing) as a simple way to toggle behavior at runtime without weird conditional compilation.
	 */
	FScopedPreventAttachedComponentMove(USceneComponent* Component)
	: Owner(Component)
	{
		if (Component)
		{
			// Save old flags
			bSavedAbsoluteLocation = Component->bAbsoluteLocation;
			bSavedAbsoluteRotation = Component->bAbsoluteRotation;
			bSavedAbsoluteScale = Component->bAbsoluteScale;
			bSavedNonAbsoluteComponent = !(bSavedAbsoluteLocation && bSavedAbsoluteRotation && bSavedAbsoluteScale);
		
			// Use absolute (stay in world space no matter what parent does)
			Component->bAbsoluteLocation = true;
			Component->bAbsoluteRotation = true;
			Component->bAbsoluteScale = true;

			if (bSavedNonAbsoluteComponent && Component->GetAttachParent())
			{
				// Make RelativeLocation etc relative to the world.
				Component->ConditionalUpdateComponentToWorld();
				Component->RelativeLocation = Component->GetComponentLocation();
				Component->RelativeRotation = Component->GetComponentRotation();
				Component->RelativeScale3D = Component->GetComponentScale();
			}
		}
		else
		{
			bSavedAbsoluteLocation = false;
			bSavedAbsoluteRotation = false;
			bSavedAbsoluteScale = false;
		}
	}

	~FScopedPreventAttachedComponentMove();

private:

	USceneComponent* Owner;
	uint32 bSavedAbsoluteLocation:1;
	uint32 bSavedAbsoluteRotation:1;
	uint32 bSavedAbsoluteScale:1;
	uint32 bSavedNonAbsoluteComponent:1; // Whether any of the saved location/rotation/scale flags were false (or equivalently: not all were true).

	// This class can only be created on the stack, otherwise the ordering constraints
	// of the constructor and destructor between encapsulated scopes could be violated.
	void*	operator new		(size_t);
	void*	operator new[]		(size_t);
	void	operator delete		(void *);
	void	operator delete[]	(void*);
};

//////////////////////////////////////////////////////////////////////////

/**
 * Enum that controls the scoping behavior of FScopedMovementUpdate.
 * Note that EScopedUpdate::ImmediateUpdates is not allowed within outer scopes that defer updates,
 * and any attempt to do so will change the new inner scope to use deferred updates instead.
 */
namespace EScopedUpdate
{
	enum Type
	{
		ImmediateUpdates,
		DeferredUpdates	
	};
}


/**
 * FScopedMovementUpdate creates a new movement scope, within which propagation of moves may be deferred until the end of the outermost scope that does not defer updates.
 * Moves within this scope will avoid updates such as UpdateBounds(), OnUpdateTransform(), UpdatePhysicsVolume(), UpdateChildTransforms() etc
 * until the move is committed (which happens when the last deferred scope goes out of context).
 *
 * Note that non-deferred scopes are not allowed within outer scopes that defer updates, and any attempt to use one will change the inner scope to use deferred updates.
 */
class ENGINE_API FScopedMovementUpdate : private FNoncopyable
{
public:
	
	typedef TArray<struct FHitResult, TInlineAllocator<2>> TBlockingHitArray;

	FScopedMovementUpdate( USceneComponent* Component, EScopedUpdate::Type ScopeBehavior = EScopedUpdate::DeferredUpdates, bool bRequireOverlapsEventFlagToQueueOverlaps = true );
	~FScopedMovementUpdate();

	enum class EHasMovedTransformOption
	{
		eTestTransform,
		eIgnoreTransform
	};

	enum class EOverlapState
	{
		eUseParent,
		eUnknown,
		eIncludesOverlaps,
		eForceUpdate,
	};

	/** Get the scope containing this scope. A scope only has an outer scope if they both defer updates. */
	const FScopedMovementUpdate* GetOuterDeferredScope() const;

	/** Return true if deferring updates, false if updates are applied immediately. */
	bool IsDeferringUpdates() const;
	
	/** Revert movement to the initial location of the Component at the start of the scoped update. Also clears pending overlaps and sets bHasMoved to false. */
	void RevertMove();

	/** Returns whether movement has occurred at all during this scope, optionally checking if the transform is different (since changing scale does not go through a move). RevertMove() sets this back to false. */
	bool HasMoved(EHasMovedTransformOption CheckTransform) const;

	/** Returns true if the Component's transform differs from that at the start of the scoped update. */
	bool IsTransformDirty() const;

	/** Returns true if there are pending overlaps queued in this scope. */
	bool HasPendingOverlaps() const;

	/**
	* Returns true if we require bGenerateOverlapEvents on both the moving object and the overlapped object to add them to the pending overlaps list.
	* These flags will still be required when dispatching calls to UpdateOverlaps(), but this allows some custom processing of queued overlaps that would be otherwise missed along the way.
	*/
	bool RequiresOverlapsEventFlag() const;

	/** Returns the pending overlaps within this scope. */
	const TArray<FOverlapInfo>& GetPendingOverlaps() const;

	/** Returns the list of pending blocking hits, which will be used for notifications once the move is committed. */
	const TBlockingHitArray& GetPendingBlockingHits() const;

	//--------------------------------------------------------------------------------------------------------//
	// These methods are intended only to be used by SceneComponent and derived classes.

	/** Add overlaps to the queued overlaps array. This is intended for use only by SceneComponent and its derived classes whenever movement is performed. */
	void AppendOverlapsAfterMove(const TArray<FOverlapInfo>& NewPendingOverlaps, bool bSweep, bool bIncludesOverlapsAtEnd);

	/** Keep current pending overlaps after a move but make note that there was movement (just a symmetric rotation). */
	void KeepCurrentOverlapsAfterRotation(bool bSweep);

	/** Add blocking hit that will get processed once the move is committed. This is intended for use only by SceneComponent and its derived classes. */
	void AppendBlockingHitAfterMove(const FHitResult& Hit);

	/** Clear overlap state at current location, we don't know what it is. */
	void InvalidateCurrentOverlaps();

	/** Force full overlap update once this scope finishes. */
	void ForceOverlapUpdate();

	/** Registers that this move is a teleport */
	void SetHasTeleported();

	//--------------------------------------------------------------------------------------------------------//

protected:

	/** Fills in the list of overlaps at the end location (in EndOverlaps). Returns pointer to the list, or null if it can't be computed. */
	const TArray<FOverlapInfo>* GetOverlapsAtEnd(class UPrimitiveComponent& PrimComponent, TArray<FOverlapInfo>& EndOverlaps, bool bTransformChanged) const;

	bool SetWorldLocationAndRotation(FVector NewLocation, const FQuat& NewQuat, bool bNoPhysics = false, ETeleportType Teleport = ETeleportType::None);

private:

	/** Notify this scope that the given inner scope completed its update (ie is going out of scope). Only occurs for deferred updates. */
	void OnInnerScopeComplete(const FScopedMovementUpdate& InnerScope);
	
	// This class can only be created on the stack, otherwise the ordering constraints
	// of the constructor and destructor between encapsulated scopes could be violated.
	void*	operator new		(size_t);
	void*	operator new[]		(size_t);
	void	operator delete		(void *);
	void	operator delete[]	(void*);

protected:

	USceneComponent* Owner;
	FScopedMovementUpdate* OuterDeferredScope;
	uint32 bDeferUpdates:1;
	uint32 bHasMoved:1;
	uint32 bHasTeleported:1;
	uint32 bRequireOverlapsEventFlag:1;
	EOverlapState CurrentOverlapState;

	FTransform InitialTransform;
	FVector InitialRelativeLocation;
	FRotator InitialRelativeRotation;
	FVector InitialRelativeScale;

	int32 FinalOverlapCandidatesIndex;		// If not INDEX_NONE, overlaps at this index and beyond in PendingOverlaps are at the final destination
	TArray<FOverlapInfo> PendingOverlaps;	// All overlaps encountered during the scope of moves.
	TBlockingHitArray BlockingHits;			// All blocking hits encountered during the scope of moves.

	friend class USceneComponent;
};

//////////////////////////////////////////////////////////////////////////
// FScopedMovementUpdate inlines

FORCEINLINE const FScopedMovementUpdate* FScopedMovementUpdate::GetOuterDeferredScope() const
{
	return OuterDeferredScope;
}

FORCEINLINE bool FScopedMovementUpdate::IsDeferringUpdates() const
{
	return bDeferUpdates;
}

FORCEINLINE bool FScopedMovementUpdate::HasMoved(EHasMovedTransformOption CheckTransform) const
{
	return bHasMoved || (CheckTransform == EHasMovedTransformOption::eTestTransform && IsTransformDirty());
}

FORCEINLINE bool FScopedMovementUpdate::HasPendingOverlaps() const
{
	return PendingOverlaps.Num() > 0;
}

FORCEINLINE bool FScopedMovementUpdate::RequiresOverlapsEventFlag() const
{
	return bRequireOverlapsEventFlag;
}

FORCEINLINE const TArray<struct FOverlapInfo>& FScopedMovementUpdate::GetPendingOverlaps() const
{
	return PendingOverlaps;
}

FORCEINLINE const FScopedMovementUpdate::TBlockingHitArray& FScopedMovementUpdate::GetPendingBlockingHits() const
{
	return BlockingHits;
}

FORCEINLINE_DEBUGGABLE void FScopedMovementUpdate::AppendBlockingHitAfterMove(const FHitResult& Hit)
{
	BlockingHits.Add(Hit);
}

FORCEINLINE_DEBUGGABLE void FScopedMovementUpdate::KeepCurrentOverlapsAfterRotation(bool bSweep)
{
	bHasMoved = true;
	// CurrentOverlapState is unchanged
}

FORCEINLINE_DEBUGGABLE void FScopedMovementUpdate::InvalidateCurrentOverlaps()
{
	bHasMoved = true;
	CurrentOverlapState = EOverlapState::eUnknown;
	FinalOverlapCandidatesIndex = INDEX_NONE;
}

FORCEINLINE_DEBUGGABLE void FScopedMovementUpdate::ForceOverlapUpdate()
{
	bHasMoved = true;
	CurrentOverlapState = EOverlapState::eForceUpdate;
	FinalOverlapCandidatesIndex = INDEX_NONE;
}

FORCEINLINE_DEBUGGABLE void FScopedMovementUpdate::SetHasTeleported()
{
	bHasTeleported = true;
}

FORCEINLINE_DEBUGGABLE class FScopedMovementUpdate* USceneComponent::GetCurrentScopedMovement() const
{
	if (ScopedMovementStack.Num() > 0)
	{
		return ScopedMovementStack.Last();
	}
	return nullptr;
}

//////////////////////////////////////////////////////////////////////////
// SceneComponent inlines that depend on FScopedMovementUpdate

FORCEINLINE_DEBUGGABLE bool USceneComponent::IsDeferringMovementUpdates() const
{
	if (ScopedMovementStack.Num() > 0)
	{
		checkSlow(ScopedMovementStack.Last()->IsDeferringUpdates());
		return true;
	}
	return false;
}

FORCEINLINE_DEBUGGABLE void USceneComponent::BeginScopedMovementUpdate(class FScopedMovementUpdate& ScopedUpdate)
{
	checkSlow(IsInGameThread());
	checkSlow(ScopedUpdate.IsDeferringUpdates());
	ScopedMovementStack.Push(&ScopedUpdate);
}

