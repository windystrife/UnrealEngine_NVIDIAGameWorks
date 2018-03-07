// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "GameFramework/MovementComponent.h"
#include "ProjectileMovementComponent.generated.h"

/**
 * ProjectileMovementComponent updates the position of another component during its tick.
 *
 * Behavior such as bouncing after impacts and homing toward a target are supported.
 *
 * Normally the root component of the owning actor is moved, however another component may be selected (see SetUpdatedComponent()).
 * If the updated component is simulating physics, only the initial launch parameters (when initial velocity is non-zero)
 * will affect the projectile, and the physics sim will take over from there.
 *
 * @see UMovementComponent
 */
UCLASS(ClassGroup=Movement, meta=(BlueprintSpawnableComponent), ShowCategories=(Velocity))
class ENGINE_API UProjectileMovementComponent : public UMovementComponent
{
	GENERATED_UCLASS_BODY()

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams( FOnProjectileBounceDelegate, const FHitResult&, ImpactResult, const FVector&, ImpactVelocity );
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam( FOnProjectileStopDelegate, const FHitResult&, ImpactResult );

	/** Initial speed of projectile. If greater than zero, this will override the initial Velocity value and instead treat Velocity as a direction. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Projectile)
	float InitialSpeed;

	/** Limit on speed of projectile (0 means no limit). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Projectile)
	float MaxSpeed;

	/** If true, this projectile will have its rotation updated each frame to match the direction of its velocity. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Projectile)
	uint32 bRotationFollowsVelocity:1;

	/** If true, simple bounces will be simulated. Set this to false to stop simulating on contact. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=ProjectileBounces)
	uint32 bShouldBounce:1;

	/**
	 * If true, the initial Velocity is interpreted as being in local space upon startup.
	 * @see SetVelocityInLocalSpace()
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Projectile)
	uint32 bInitialVelocityInLocalSpace:1;

	/**
	 * If true, forces sub-stepping to break up movement into discrete smaller steps to improve accuracy of the trajectory.
	 * Objects that move in a straight line typically do *not* need to set this, as movement always uses continuous collision detection (sweeps) so collision is not missed.
	 * Sub-stepping is automatically enabled when under the effects of gravity or when homing towards a target.
	 * @see MaxSimulationTimeStep, MaxSimulationIterations
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=ProjectileSimulation)
	uint32 bForceSubStepping:1;

	/**
	 * If true, we will accelerate toward our homing target. HomingTargetComponent must be set after the projectile is spawned.
	 * @see HomingTargetComponent, HomingAccelerationMagnitude
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Homing)
	uint32 bIsHomingProjectile:1;

	/**
	 * Controls the effects of friction on velocity parallel to the impact surface when bouncing.
	 * If true, friction will be modified based on the angle of impact, making friction higher for perpendicular impacts and lower for glancing impacts.
	 * If false, a bounce will retain a proportion of tangential velocity equal to (1.0 - Friction), acting as a "horizontal restitution".
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=ProjectileBounces)
	uint32 bBounceAngleAffectsFriction:1;

	/**
	 * If true, projectile is sliding / rolling along a surface.
	 */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category=ProjectileBounces)
	uint32 bIsSliding:1;

	/** Saved HitResult Time (0 to 1) from previous simulation step. Equal to 1.0 when there was no impact. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category=ProjectileBounces)
	float PreviousHitTime;

	/** Saved HitResult Normal from previous simulation step that resulted in an impact. If PreviousHitTime is 1.0, then the hit was not in the last step. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category=ProjectileBounces)
	FVector PreviousHitNormal;

	/** Custom gravity scale for this projectile. Set to 0 for no gravity. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Projectile)
	float ProjectileGravityScale;

	/** Buoyancy of UpdatedComponent in fluid. 0.0=sinks as fast as in air, 1.0=neutral buoyancy */
	UPROPERTY()
	float Buoyancy;

	/**
	 * Percentage of velocity maintained after the bounce in the direction of the normal of impact (coefficient of restitution).
	 * 1.0 = no velocity lost, 0.0 = no bounce. Ignored if bShouldBounce is false.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=ProjectileBounces, meta=(ClampMin="0", UIMin="0"))
	float Bounciness;

	/**
	 * Coefficient of friction, affecting the resistance to sliding along a surface.
	 * Normal range is [0,1] : 0.0 = no friction, 1.0+ = very high friction.
	 * Also affects the percentage of velocity maintained after the bounce in the direction tangent to the normal of impact.
	 * Ignored if bShouldBounce is false.
	 * @see bBounceAngleAffectsFriction
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=ProjectileBounces, meta=(ClampMin="0", UIMin="0"))
	float Friction;

	/**
	 * If velocity is below this threshold after a bounce, stops simulating and triggers the OnProjectileStop event.
	 * Ignored if bShouldBounce is false, in which case the projectile stops simulating on the first impact.
	 * @see StopSimulating(), OnProjectileStop
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=ProjectileBounces)
	float BounceVelocityStopSimulatingThreshold;

	/** Called when projectile impacts something and bounces are enabled. */
	UPROPERTY(BlueprintAssignable)
	FOnProjectileBounceDelegate OnProjectileBounce;

	/** Called when projectile has come to a stop (velocity is below simulation threshold, bounces are disabled, or it is forcibly stopped). */
	UPROPERTY(BlueprintAssignable)
	FOnProjectileStopDelegate OnProjectileStop;

	/** The magnitude of our acceleration towards the homing target. Overall velocity magnitude will still be limited by MaxSpeed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Homing)
	float HomingAccelerationMagnitude;

	/**
	 * The current target we are homing towards. Can only be set at runtime (when projectile is spawned or updating).
	 * @see bIsHomingProjectile
	 */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadWrite, Category=Homing)
	TWeakObjectPtr<USceneComponent> HomingTargetComponent;

	/** Sets the velocity to the new value, rotated into Actor space. */
	UFUNCTION(BlueprintCallable, Category="Game|Components|ProjectileMovement")
	virtual void SetVelocityInLocalSpace(FVector NewVelocity);

	//Begin UActorComponent Interface
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;
	virtual void PostLoad() override;
	//End UActorComponent Interface

	//Begin UMovementComponent Interface
	virtual float GetMaxSpeed() const override { return MaxSpeed; }
	virtual void InitializeComponent() override;
	//End UMovementComponent Interface

	/**
	 * This will check to see if the projectile is still in the world.  It will check things like
	 * the KillZ, outside world bounds, etc. and handle the situation.
	 */
	virtual bool CheckStillInWorld();

	/** @return Buoyancy of UpdatedComponent in fluid.  0.0=sinks as fast as in air, 1.0=neutral buoyancy*/
	float GetBuoyancy() const { return Buoyancy; };	

	bool ShouldApplyGravity() const { return ProjectileGravityScale != 0.f; }

	/**
	 * Given an initial velocity and a time step, compute a new velocity.
	 * Default implementation applies the result of ComputeAcceleration() to velocity.
	 * 
	 * @param  InitialVelocity Initial velocity.
	 * @param  DeltaTime Time step of the update.
	 * @return Velocity after DeltaTime time step.
	 */
	virtual FVector ComputeVelocity(FVector InitialVelocity, float DeltaTime) const;

	/** Clears the reference to UpdatedComponent, fires stop event (OnProjectileStop), and stops ticking (if bAutoUpdateTickRegistration is true). */
	UFUNCTION(BlueprintCallable, Category="Game|Components|ProjectileMovement")
	virtual void StopSimulating(const FHitResult& HitResult);

	bool HasStoppedSimulation() { return (UpdatedComponent == nullptr) || (bIsActive == false); }

	/**
	 * Compute remaining time step given remaining time and current iterations.
	 * The last iteration (limited by MaxSimulationIterations) always returns the remaining time, which may violate MaxSimulationTimeStep.
	 *
	 * @param RemainingTime		Remaining time in the tick.
	 * @param Iterations		Current iteration of the tick (starting at 1).
	 * @return The remaining time step to use for the next sub-step of iteration.
	 * @see MaxSimulationTimeStep, MaxSimulationIterations
	 * @see ShouldUseSubStepping()
	 */
	float GetSimulationTimeStep(float RemainingTime, int32 Iterations) const;

	/**
	 * Determine whether or not to use substepping in the projectile motion update.
	 * If true, GetSimulationTimeStep() will be used to time-slice the update. If false, all remaining time will be used during the tick.
	 * @return Whether or not to use substepping in the projectile motion update.
	 * @see GetSimulationTimeStep()
	 */
	virtual bool ShouldUseSubStepping() const;

	/**
	 * Max time delta for each discrete simulation step.
	 * Lowering this value can address issues with fast-moving objects or complex collision scenarios, at the cost of performance.
	 *
	 * WARNING: if (MaxSimulationTimeStep * MaxSimulationIterations) is too low for the min framerate, the last simulation step may exceed MaxSimulationTimeStep to complete the simulation.
	 * @see MaxSimulationIterations, bForceSubStepping
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0.0166", ClampMax="0.50", UIMin="0.0166", UIMax="0.50"), Category=ProjectileSimulation)
	float MaxSimulationTimeStep;

	/**
	 * Max number of iterations used for each discrete simulation step.
	 * Increasing this value can address issues with fast-moving objects or complex collision scenarios, at the cost of performance.
	 *
	 * WARNING: if (MaxSimulationTimeStep * MaxSimulationIterations) is too low for the min framerate, the last simulation step may exceed MaxSimulationTimeStep to complete the simulation.
	 * @see MaxSimulationTimeStep, bForceSubStepping
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="1", ClampMax="25", UIMin="1", UIMax="25"), Category=ProjectileSimulation)
	int32 MaxSimulationIterations;

protected:

	// Enum indicating how simulation should proceed after HandleBlockingHit() is called.
	enum class EHandleBlockingHitResult
	{
		Deflect,				/** Assume velocity has been deflected, and trigger HandleDeflection(). This is the default return value of HandleBlockingHit(). */
		AdvanceNextSubstep,		/** Advance to the next simulation update. Typically used when additional slide/multi-bounce logic can be ignored,
								    such as when an object that blocked the projectile is destroyed and movement should continue. */
		Abort,					/** Abort all further simulation. Typically used when components have been invalidated or simulation should stop. */
	};

	/**
	 * Handle blocking hit during simulation update. Checks that simulation remains valid after collision.
	 * If simulating then calls HandleImpact(), and returns EHandleHitWallResult::Deflect by default to enable multi-bounce and sliding support through HandleDeflection().
	 * If no longer simulating then returns EHandleHitWallResult::Abort, which aborts attempts at further simulation.
	 *
	 * @param  Hit						Blocking hit that occurred.
	 * @param  TimeTick					Time delta of last move that resulted in the blocking hit.
	 * @param  MoveDelta				Movement delta for the current sub-step.
	 * @param  SubTickTimeRemaining		How much time to continue simulating in the current sub-step, which may change as a result of this function.
	 *									Initial default value is: TimeTick * (1.f - Hit.Time)
	 * @return Result indicating how simulation should proceed.
	 * @see EHandleHitWallResult, HandleImpact()
	 */
	 virtual EHandleBlockingHitResult HandleBlockingHit(const FHitResult& Hit, float TimeTick, const FVector& MoveDelta, float& SubTickTimeRemaining);

	/**
	 * Applies bounce logic if enabled to affect velocity upon impact (using ComputeBounceResult()),
	 * or stops the projectile if bounces are not enabled or velocity is below BounceVelocityStopSimulatingThreshold.
	 * Triggers applicable events (OnProjectileBounce).
	 */
	virtual void HandleImpact(const FHitResult& Hit, float TimeSlice=0.f, const FVector& MoveDelta = FVector::ZeroVector) override;

	/**
	 * Handle a blocking hit after HandleBlockingHit() returns a result indicating that deflection occured.
	 * Default implementation increments NumBounces, checks conditions that could indicate a slide, and calls HandleSliding() if necessary.
	 * 
	 * @param  Hit					Blocking hit that occurred. May be changed to indicate the last hit result of further movement.
	 * @param  OldVelocity			Velocity at the start of the simulation update sub-step. Current Velocity may differ (as a result of a bounce).
	 * @param  NumBounces			Number of bounces that have occurred thus far in the tick.
	 * @param  SubTickTimeRemaining	Time remaining in the simulation sub-step. May be changed to indicate change to remaining time.
	 * @return True if simulation of the projectile should continue, false otherwise.
	 * @see HandleSliding()
	 */
	 virtual bool HandleDeflection(FHitResult& Hit, const FVector& OldVelocity, const uint32 NumBounces, float& SubTickTimeRemaining);

	/**
	 * Handle case where projectile is sliding along a surface.
	 * Velocity will be parallel to the impact surface upon entry to this method.
	 * 
	 * @param  InitialHit				Hit result of impact causing slide. May be modified by this function to reflect any subsequent movement.
	 * @param  SubTickTimeRemaining		Time remaining in the tick. This function may update this time with any reduction to the simulation time requested.
	 * @return True if simulation of the projectile should continue, false otherwise.
	 */
	virtual bool HandleSliding(FHitResult& Hit, float& SubTickTimeRemaining);

	/** Computes result of a bounce and returns the new velocity. */
	virtual FVector ComputeBounceResult(const FHitResult& Hit, float TimeSlice, const FVector& MoveDelta);

	/** Don't allow velocity magnitude to exceed MaxSpeed, if MaxSpeed is non-zero. */
	UFUNCTION(BlueprintCallable, Category="Game|Components|ProjectileMovement")
	FVector LimitVelocity(FVector NewVelocity) const;

	/** Compute the distance we should move in the given time, at a given a velocity. */
	virtual FVector ComputeMoveDelta(const FVector& InVelocity, float DeltaTime) const;

	/** Compute the acceleration that will be applied */
	virtual FVector ComputeAcceleration(const FVector& InVelocity, float DeltaTime) const;

	/** Allow the projectile to track towards its homing target. */
	virtual FVector ComputeHomingAcceleration(const FVector& InVelocity, float DeltaTime) const;

public:
	/** Compute gravity effect given current physics volume, projectile gravity scale, etc. */
	virtual float GetGravityZ() const override;

protected:
	/** Minimum delta time considered when ticking. Delta times below this are not considered. This is a very small non-zero positive value to avoid potential divide-by-zero in simulation code. */
	static const float MIN_TICK_TIME;
};



