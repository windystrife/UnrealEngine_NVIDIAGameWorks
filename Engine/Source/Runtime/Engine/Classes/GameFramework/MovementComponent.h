// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/**
 * Movement component updates position of associated PrimitiveComponent during its tick.
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/EngineTypes.h"
#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"
#include "MovementComponent.generated.h"

class AActor;
class APhysicsVolume;
class UPrimitiveComponent;
struct FCollisionQueryParams;
struct FCollisionResponseParams;
struct FCollisionShape;

/**
 * Setting that controls behavior when movement is restricted to a 2D plane defined by a specific axis/normal,
 * so that movement along the locked axis is not be possible.
 */
UENUM(BlueprintType)
enum class EPlaneConstraintAxisSetting : uint8
{
	/** Lock movement to a user-defined axis. */
	Custom,
	/** Lock movement in the X axis. */
	X,
	/** Lock movement in the Y axis. */
	Y,
	/** Lock movement in the Z axis. */
	Z,
	/** Use the global physics project setting. */
	UseGlobalPhysicsSetting
};


/**
 * MovementComponent is an abstract component class that defines functionality for moving a PrimitiveComponent (our UpdatedComponent) each tick.
 * Base functionality includes:
 *    - Restricting movement to a plane or axis.
 *    - Utility functions for special handling of collision results (SlideAlongSurface(), ComputeSlideVector(), TwoWallAdjust()).
 *    - Utility functions for moving when there may be initial penetration (SafeMoveUpdatedComponent(), ResolvePenetration()).
 *    - Automatically registering the component tick and finding a component to move on the owning Actor.
 * Normally the root component of the owning actor is moved, however another component may be selected (see SetUpdatedComponent()).
 * During swept (non-teleporting) movement only collision of UpdatedComponent is considered, attached components will teleport to the end location ignoring collision.
 */
UCLASS(ClassGroup=Movement, abstract, BlueprintType)
class ENGINE_API UMovementComponent : public UActorComponent
{
	GENERATED_UCLASS_BODY()

	/**
	 * The component we move and update.
	 * If this is null at startup and bAutoRegisterUpdatedComponent is true, the owning Actor's root component will automatically be set as our UpdatedComponent at startup.
	 * @see bAutoRegisterUpdatedComponent, SetUpdatedComponent(), UpdatedPrimitive
	 */
	UPROPERTY(BlueprintReadOnly, Transient, DuplicateTransient, Category=MovementComponent)
	USceneComponent* UpdatedComponent;

	/**
	 * UpdatedComponent, cast as a UPrimitiveComponent. May be invalid if UpdatedComponent was null or not a UPrimitiveComponent.
	 */
	UPROPERTY(BlueprintReadOnly, Transient, DuplicateTransient, Category=MovementComponent)
	UPrimitiveComponent* UpdatedPrimitive;

	/**
	 * Flags that control the behavior of calls to MoveComponent() on our UpdatedComponent.
	 * @see EMoveComponentFlags
	 */
	EMoveComponentFlags MoveComponentFlags;

	/** Current velocity of updated component. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Velocity)
	FVector Velocity;

	/**
	 * If true, movement will be constrained to a plane.
	 * @see PlaneConstraintNormal, PlaneConstraintOrigin, PlaneConstraintAxisSetting
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=PlanarMovement)
	uint32 bConstrainToPlane:1;

	/** If true and plane constraints are enabled, then the updated component will be snapped to the plane when first attached. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=PlanarMovement, meta=(editcondition=bConstrainToPlane))
	uint32 bSnapToPlaneAtStart:1;

private:

#if WITH_EDITOR
	// Warned about trying to move something with static mobility.
	uint32 bEditorWarnedStaticMobilityMove:1;
#endif

	/**
	 * Setting that controls behavior when movement is restricted to a 2D plane defined by a specific axis/normal,
	 * so that movement along the locked axis is not be possible.
	 * @see SetPlaneConstraintAxisSetting
	 */
	UPROPERTY(EditAnywhere, Category=PlanarMovement, meta=(editcondition=bConstrainToPlane))
	EPlaneConstraintAxisSetting PlaneConstraintAxisSetting;

protected:

	/**
	 * The normal or axis of the plane that constrains movement, if bConstrainToPlane is enabled.
	 * If for example you wanted to constrain movement to the X-Z plane (so that Y cannot change), the normal would be set to X=0 Y=1 Z=0.
	 * This is recalculated whenever PlaneConstraintAxisSetting changes. It is normalized once the component is registered with the game world.
	 * @see bConstrainToPlane, SetPlaneConstraintNormal(), SetPlaneConstraintFromVectors()
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=PlanarMovement, meta=(editcondition=bConstrainToPlane))
	FVector PlaneConstraintNormal;

	/**
	 * The origin of the plane that constrains movement, if plane constraint is enabled. 
	 * This defines the behavior of snapping a position to the plane, such as by SnapUpdatedComponentToPlane().
	 * @see bConstrainToPlane, SetPlaneConstraintOrigin().
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=PlanarMovement, meta=(editcondition=bConstrainToPlane))
	FVector PlaneConstraintOrigin;

	/**
	 * Helper to compute the plane constraint axis from the current setting.
	 * 
	 * @param  AxisSetting Setting to use when computing the axis.
	 * @return Plane constraint axis/normal.
	 */
	FVector GetPlaneConstraintNormalFromAxisSetting(EPlaneConstraintAxisSetting AxisSetting) const;

public:

	/** If true, skips TickComponent() if UpdatedComponent was not recently rendered. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=MovementComponent)
	uint32 bUpdateOnlyIfRendered:1;

	/**
	 * If true, whenever the updated component is changed, this component will enable or disable its tick dependent on whether it has something to update.
	 * This will NOT enable tick at startup if bAutoActivate is false, because presumably you have a good reason for not wanting it to start ticking initially.
	 **/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=MovementComponent)
	uint32 bAutoUpdateTickRegistration:1;

	/**
	 * If true, after registration we will add a tick dependency to tick before our owner (if we can both tick).
	 * This is important when our tick causes an update in the owner's position, so that when the owner ticks it uses the most recent position without lag.
	 * Disabling this can improve performance if both objects tick but the order of ticks doesn't matter.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=MovementComponent)
	uint32 bTickBeforeOwner:1;

	/** If true, registers the owner's Root component as the UpdatedComponent if there is not one currently assigned. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=MovementComponent)
	uint32 bAutoRegisterUpdatedComponent:1;

	//~ Begin ActorComponent Interface 
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;
	virtual void RegisterComponentTickFunctions(bool bRegister) override;
	virtual void PostLoad() override;
	virtual void Deactivate() override;
	virtual void Serialize(FArchive& Ar) override;

	/** Overridden to auto-register the updated component if it starts NULL, and we can find a root component on our owner. */
	virtual void InitializeComponent() override;

	/** Overridden to update component properties that should be updated while being edited. */	
	virtual void OnRegister() override;


#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	static void PhysicsLockedAxisSettingChanged();
#endif // WITH_EDITOR

	//~ End ActorComponent Interface

	/** @return gravity that affects this component */
	UFUNCTION(BlueprintCallable, Category="Components|Movement")
	virtual float GetGravityZ() const;

	/** @return Maximum speed of component in current movement mode. */
	UFUNCTION(BlueprintCallable, Category="Components|Movement")
	virtual float GetMaxSpeed() const;

	/** @return a scalar applied to the maximum velocity that the component can currently move. */
	DEPRECATED(4.3, "GetMaxSpeedModifier() is deprecated, apply your own modifiers to GetMaxSpeed() if desired.")
	virtual float GetMaxSpeedModifier() const;
	
	/** @return a scalar applied to the maximum velocity that the component can currently move. */
	UFUNCTION(BlueprintCallable, Category="Components|Movement", meta=(DeprecatedFunction, DisplayName="GetMaxSpeedModifier", DeprecationMessage="GetMaxSpeedModifier() is deprecated, apply your own modifiers to GetMaxSpeed() if desired."))
	virtual float K2_GetMaxSpeedModifier() const;

	/** @return the result of GetMaxSpeed() * GetMaxSpeedModifier(). */
	DEPRECATED(4.3, "GetModifiedMaxSpeed() is deprecated, use GetMaxSpeed() instead.")
	virtual float GetModifiedMaxSpeed() const;

	/** @return the result of GetMaxSpeed() * GetMaxSpeedModifier(). */
	UFUNCTION(BlueprintCallable, Category="Components|Movement", meta=(DisplayName="GetModifiedMaxSpeed", DeprecationMessage="GetModifiedMaxSpeed() is deprecated, apply your own modifiers to GetMaxSpeed() if desired."))
	virtual float K2_GetModifiedMaxSpeed() const;

	/**
	 * Returns true if the current velocity is exceeding the given max speed (usually the result of GetMaxSpeed()), within a small error tolerance.
	 * Note that under normal circumstances updates cause by acceleration will not cause this to be true, however external forces or changes in the max speed limit
	 * can cause the max speed to be violated.
	 */
	UFUNCTION(BlueprintCallable, Category="Components|Movement")
	virtual bool IsExceedingMaxSpeed(float MaxSpeed) const;

	/** Stops movement immediately (zeroes velocity, usually zeros acceleration for components with acceleration). */
	UFUNCTION(BlueprintCallable, Category="Components|Movement")
	virtual void StopMovementImmediately();

	/**
	 * Possibly skip update if moved component is not rendered or can't move.
	 * @param DeltaTime @todo this parameter is not used in the function.
	 * @return true if component movement update should be skipped
	 */
	virtual bool ShouldSkipUpdate(float DeltaTime) const;

	/** @return PhysicsVolume this MovementComponent is using, or the world's default physics volume if none. **/
	UFUNCTION(BlueprintCallable, Category="Components|Movement")
	virtual APhysicsVolume* GetPhysicsVolume() const;

	/** Delegate when PhysicsVolume of UpdatedComponent has been changed **/
	UFUNCTION()
	virtual void PhysicsVolumeChanged(class APhysicsVolume* NewVolume);

	/** Assign the component we move and update. */
	UFUNCTION(BlueprintCallable, Category="Components|Movement")
	virtual void SetUpdatedComponent(USceneComponent* NewUpdatedComponent);

	/** return true if it's in PhysicsVolume with water flag **/
	virtual bool IsInWater() const;

	/** Update tick registration state, determined by bAutoUpdateTickRegistration. Called by SetUpdatedComponent. */
	virtual void UpdateTickRegistration();

	/** 
	 * Called for Blocking impact
	 * @param Hit: Describes the collision.
	 * @param TimeSlice: Time period for the simulation that produced this hit.  Useful for
	 *		  putting Hit.Time in context.  Can be zero in certain situations where it's not appropriate, 
	 *		  be sure to handle that.
	 * @param MoveDelta: Attempted move that resulted in the hit.
	 */
	virtual void HandleImpact(const FHitResult& Hit, float TimeSlice=0.f, const FVector& MoveDelta = FVector::ZeroVector);

	/** Update ComponentVelocity of UpdatedComponent. This needs to be called by derived classes at the end of an update whenever Velocity has changed.	 */
	virtual void UpdateComponentVelocity();

	/** Initialize collision params appropriately based on our collision settings. Use this before any Line, Overlap, or Sweep tests. */
	virtual void InitCollisionParams(FCollisionQueryParams &OutParams, FCollisionResponseParams& OutResponseParam) const;

	/** Return true if the given collision shape overlaps other geometry at the given location and rotation. The collision params are set by InitCollisionParams(). */
	virtual bool OverlapTest(const FVector& Location, const FQuat& RotationQuat, const ECollisionChannel CollisionChannel, const FCollisionShape& CollisionShape, const AActor* IgnoreActor) const;

	/**
	 * Moves our UpdatedComponent by the given Delta, and sets rotation to NewRotation. Respects the plane constraint, if enabled.
	 * @note This simply calls the virtual MoveUpdatedComponentImpl() which can be overridden to implement custom behavior.
	 * @note The overload taking rotation as an FQuat is slightly faster than the version using FRotator (which will be converted to an FQuat).
	 * @note The 'Teleport' flag is currently always treated as 'None' (not teleporting) when used in an active FScopedMovementUpdate.
	 * @return True if some movement occurred, false if no movement occurred. Result of any impact will be stored in OutHit.
	 */
	bool MoveUpdatedComponent(const FVector& Delta, const FQuat& NewRotation,    bool bSweep, FHitResult* OutHit = NULL, ETeleportType Teleport = ETeleportType::None);
	bool MoveUpdatedComponent(const FVector& Delta, const FRotator& NewRotation, bool bSweep, FHitResult* OutHit = NULL, ETeleportType Teleport = ETeleportType::None);

protected:

	virtual bool MoveUpdatedComponentImpl(const FVector& Delta, const FQuat& NewRotation, bool bSweep, FHitResult* OutHit = NULL, ETeleportType Teleport = ETeleportType::None);

public:
	
	/**
	 * Moves our UpdatedComponent by the given Delta, and sets rotation to NewRotation.
	 * Respects the plane constraint, if enabled.
	 * @return True if some movement occurred, false if no movement occurred. Result of any impact will be stored in OutHit.
	 */
	UFUNCTION(BlueprintCallable, Category="Components|Movement", meta=(DisplayName = "MoveUpdatedComponent", AdvancedDisplay="bTeleport"))
	bool K2_MoveUpdatedComponent(FVector Delta, FRotator NewRotation, FHitResult& OutHit, bool bSweep = true, bool bTeleport = false);

	/**
	 * Calls MoveUpdatedComponent(), handling initial penetrations by calling ResolvePenetration().
	 * If this adjustment succeeds, the original movement will be attempted again.
	 * @note The overload taking rotation as an FQuat is slightly faster than the version using FRotator (which will be converted to an FQuat).
	 * @note The 'Teleport' flag is currently always treated as 'None' (not teleporting) when used in an active FScopedMovementUpdate.
	 * @return result of the final MoveUpdatedComponent() call.
	 */
	bool SafeMoveUpdatedComponent(const FVector& Delta, const FQuat& NewRotation,    bool bSweep, FHitResult& OutHit, ETeleportType Teleport = ETeleportType::None);
	bool SafeMoveUpdatedComponent(const FVector& Delta, const FRotator& NewRotation, bool bSweep, FHitResult& OutHit, ETeleportType Teleport = ETeleportType::None);

	/**
	 * Calculate a movement adjustment to try to move out of a penetration from a failed move.
	 * @param Hit the result of the failed move
	 * @return The adjustment to use after a failed move, or a zero vector if no attempt should be made.
	 */
	virtual FVector GetPenetrationAdjustment(const FHitResult& Hit) const;
	
	/**
	 * Try to move out of penetration in an object after a failed move. This function should respect the plane constraint if applicable.
	 * @note This simply calls the virtual ResolvePenetrationImpl() which can be overridden to implement custom behavior.
	 * @note The overload taking rotation as an FQuat is slightly faster than the version using FRotator (which will be converted to an FQuat)..
	 * @param Adjustment	The requested adjustment, usually from GetPenetrationAdjustment()
	 * @param Hit			The result of the failed move
	 * @return True if the adjustment was successful and the original move should be retried, or false if no repeated attempt should be made.
	 */
	bool ResolvePenetration(const FVector& Adjustment, const FHitResult& Hit, const FQuat& NewRotation);
	bool ResolvePenetration(const FVector& Adjustment, const FHitResult& Hit, const FRotator& NewRotation);

protected:

	virtual bool ResolvePenetrationImpl(const FVector& Adjustment, const FHitResult& Hit, const FQuat& NewRotation);

public:

	/**
	 * Compute a vector to slide along a surface, given an attempted move, time, and normal.
	 * @param Delta:	Attempted move.
	 * @param Time:		Amount of move to apply (between 0 and 1).
	 * @param Normal:	Normal opposed to movement. Not necessarily equal to Hit.Normal.
	 * @param Hit:		HitResult of the move that resulted in the slide.
	 */
	virtual FVector ComputeSlideVector(const FVector& Delta, const float Time, const FVector& Normal, const FHitResult& Hit) const;

	/**
	 * Slide smoothly along a surface, and slide away from multiple impacts using TwoWallAdjust if necessary. Calls HandleImpact for each surface hit, if requested.
	 * Uses SafeMoveUpdatedComponent() for movement, and ComputeSlideVector() to determine the slide direction.
	 * @param Delta:	Attempted movement vector.
	 * @param Time:		Percent of Delta to apply (between 0 and 1). Usually equal to the remaining time after a collision: (1.0 - Hit.Time).
	 * @param Normal:	Normal opposing movement, along which we will slide.
	 * @param Hit:		[In] HitResult of the attempted move that resulted in the impact triggering the slide. [Out] HitResult of last attempted move.
	 * @param bHandleImpact:	Whether to call HandleImpact on each hit.
	 * @return The percentage of requested distance (Delta * Percent) actually applied (between 0 and 1). 0 if no movement occurred, non-zero if movement occurred.
	 */
	virtual float SlideAlongSurface(const FVector& Delta, float Time, const FVector& Normal, FHitResult &Hit, bool bHandleImpact = false);

	/**
	 * Compute a movement direction when contacting two surfaces.
	 * @param Delta:		[In] Amount of move attempted before impact. [Out] Computed adjustment based on impacts.
	 * @param Hit:			Impact from last attempted move
	 * @param OldHitNormal:	Normal of impact before last attempted move
	 * @return Result in Delta that is the direction to move when contacting two surfaces.
	 */
	virtual void TwoWallAdjust(FVector &Delta, const FHitResult& Hit, const FVector &OldHitNormal) const;

	/**
	 * Adds force from radial force components.
	 * Intended to be overridden by subclasses; default implementation does nothing.
	 * @param	Origin		The origin of the force
	 * @param	Radius		The radius in which the force will be applied
	 * @param	Strength	The strength of the force
	 * @param	Falloff		The falloff from the force's origin
	 */
	virtual void AddRadialForce(const FVector& Origin, float Radius, float Strength, ERadialImpulseFalloff Falloff);

	/**
	 * Adds impulse from radial force components.
	 * Intended to be overridden by subclasses; default implementation does nothing.
	 * @param	Origin		The origin of the force
	 * @param	Radius		The radius in which the force will be applied
	 * @param	Strength	The strength of the force
	 * @param	Falloff		The falloff from the force's origin
	 * @param	bVelChange	If true, the Strength is taken as a change in velocity instead of an impulse (ie. mass will have no affect).
	 */
	virtual void AddRadialImpulse(const FVector& Origin, float Radius, float Strength, ERadialImpulseFalloff Falloff, bool bVelChange);

	/**
	 * Set the plane constraint axis setting.
	 * Changing this setting will modify the current value of PlaneConstraintNormal.
	 * 
	 * @param  NewAxisSetting New plane constraint axis setting.
	 */
	UFUNCTION(BlueprintCallable, Category="Components|Movement|Planar")
	virtual void SetPlaneConstraintAxisSetting(EPlaneConstraintAxisSetting NewAxisSetting);

	/**
	 * Get the plane constraint axis setting.
	 */
	UFUNCTION(BlueprintCallable, Category="Components|Movement|Planar")
	EPlaneConstraintAxisSetting GetPlaneConstraintAxisSetting() const;

	/**
	 * Sets the normal of the plane that constrains movement, enforced if the plane constraint is enabled.
	 * Changing the normal automatically sets PlaneConstraintAxisSetting to "Custom".
	 *
	 * @param PlaneNormal	The normal of the plane. If non-zero in length, it will be normalized.
	 */
	UFUNCTION(BlueprintCallable, Category="Components|Movement|Planar")
	virtual void SetPlaneConstraintNormal(FVector PlaneNormal);

	/** Uses the Forward and Up vectors to compute the plane that constrains movement, enforced if the plane constraint is enabled. */
	UFUNCTION(BlueprintCallable, Category="Components|Movement|Planar")
	virtual void SetPlaneConstraintFromVectors(FVector Forward, FVector Up);

	/** Sets the origin of the plane that constrains movement, enforced if the plane constraint is enabled. */
	UFUNCTION(BlueprintCallable, Category="Components|Movement|Planar")
	virtual void SetPlaneConstraintOrigin(FVector PlaneOrigin);
	
	/** Sets whether or not the plane constraint is enabled. */
	UFUNCTION(BlueprintCallable, Category = "Components|Movement|Planar")
	virtual void SetPlaneConstraintEnabled(bool bEnabled);

	/** @return The normal of the plane that constrains movement, enforced if the plane constraint is enabled. */
	UFUNCTION(BlueprintCallable, Category="Components|Movement|Planar")
	const FVector& GetPlaneConstraintNormal() const;

	/**
	 * Get the plane constraint origin. This defines the behavior of snapping a position to the plane, such as by SnapUpdatedComponentToPlane().
	 * @return The origin of the plane that constrains movement, if the plane constraint is enabled.
	 */
	UFUNCTION(BlueprintCallable, Category="Components|Movement|Planar")
	const FVector& GetPlaneConstraintOrigin() const;

	/**
	 * Constrain a direction vector to the plane constraint, if enabled.
	 * @see SetPlaneConstraint
	 */
	UFUNCTION(BlueprintCallable, Category="Components|Movement|Planar")
	virtual FVector ConstrainDirectionToPlane(FVector Direction) const;

	/** Constrain a position vector to the plane constraint, if enabled. */
	UFUNCTION(BlueprintCallable, Category="Components|Movement|Planar")
	virtual FVector ConstrainLocationToPlane(FVector Location) const;

	/** Constrain a normal vector (of unit length) to the plane constraint, if enabled. */
	UFUNCTION(BlueprintCallable, Category="Components|Movement|Planar")
	virtual FVector ConstrainNormalToPlane(FVector Normal) const;

	/** Snap the updated component to the plane constraint, if enabled. */
	UFUNCTION(BlueprintCallable, Category="Components|Movement|Planar")
	virtual void SnapUpdatedComponentToPlane();

	/** Called by owning Actor upon successful teleport from AActor::TeleportTo(). */
	virtual void OnTeleported() {};

private:

	/** Transient flag indicating whether we are executing OnRegister(). */
	bool bInOnRegister;
	
	/** Transient flag indicating whether we are executing InitializeComponent(). */
	bool bInInitializeComponent;
};


//////////////////////////////////////////////////////////////////////////
// Inlines

inline float UMovementComponent::GetMaxSpeed() const
{
	return 0.f;
}

inline void UMovementComponent::StopMovementImmediately()
{
	Velocity = FVector::ZeroVector;
	UpdateComponentVelocity();
}

inline EPlaneConstraintAxisSetting UMovementComponent::GetPlaneConstraintAxisSetting() const
{
	return PlaneConstraintAxisSetting;
}

FORCEINLINE_DEBUGGABLE bool UMovementComponent::SafeMoveUpdatedComponent(const FVector& Delta, const FRotator& NewRotation, bool bSweep, FHitResult& OutHit, ETeleportType Teleport)
{
	return SafeMoveUpdatedComponent(Delta, NewRotation.Quaternion(), bSweep, OutHit, Teleport);
}

FORCEINLINE_DEBUGGABLE bool UMovementComponent::MoveUpdatedComponent(const FVector& Delta, const FQuat& NewRotation, bool bSweep, FHitResult* OutHit, ETeleportType Teleport)
{
	return MoveUpdatedComponentImpl(Delta, NewRotation, bSweep, OutHit, Teleport);
}

FORCEINLINE_DEBUGGABLE bool UMovementComponent::MoveUpdatedComponent(const FVector& Delta, const FRotator& NewRotation, bool bSweep, FHitResult* OutHit, ETeleportType Teleport)
{
	return MoveUpdatedComponentImpl(Delta, NewRotation.Quaternion(), bSweep, OutHit, Teleport);
}

FORCEINLINE_DEBUGGABLE bool UMovementComponent::ResolvePenetration(const FVector& Adjustment, const FHitResult& Hit, const FQuat& NewRotation)
{
	return ResolvePenetrationImpl(Adjustment, Hit, NewRotation);
}

FORCEINLINE_DEBUGGABLE bool UMovementComponent::ResolvePenetration(const FVector& Adjustment, const FHitResult& Hit, const FRotator& NewRotation)
{
	return ResolvePenetrationImpl(Adjustment, Hit, NewRotation.Quaternion());
}
