// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "GameFramework/ProjectileMovementComponent.h"
#include "EngineDefines.h"
#include "GameFramework/DamageType.h"
#include "Engine/World.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/WorldSettings.h"

DEFINE_LOG_CATEGORY_STATIC(LogProjectileMovement, Log, All);

const float UProjectileMovementComponent::MIN_TICK_TIME = 1e-6f;

UProjectileMovementComponent::UProjectileMovementComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bUpdateOnlyIfRendered = false;
	bInitialVelocityInLocalSpace = true;
	bForceSubStepping = false;

	Velocity = FVector(1.f,0.f,0.f);

	ProjectileGravityScale = 1.f;

	Bounciness = 0.6f;
	Friction = 0.2f;
	BounceVelocityStopSimulatingThreshold = 5.f;

	HomingAccelerationMagnitude = 0.f;

	bWantsInitializeComponent = true;

	MaxSimulationTimeStep = 0.05f;
	MaxSimulationIterations = 8;

	bBounceAngleAffectsFriction = false;
	bIsSliding = false;
	PreviousHitTime = 1.f;
	PreviousHitNormal = FVector::UpVector;
}

void UProjectileMovementComponent::PostLoad()
{
	Super::PostLoad();

	const int32 LinkerUE4Ver = GetLinkerUE4Version();

	if (LinkerUE4Ver < VER_UE4_REFACTOR_PROJECTILE_MOVEMENT)
	{
		// Old code used to treat Bounciness as Friction as well.
		Friction = FMath::Clamp(1.f - Bounciness, 0.f, 1.f);

		// Old projectiles probably don't want to use this behavior by default.
		bInitialVelocityInLocalSpace = false;
	}
}


void UProjectileMovementComponent::InitializeComponent()
{
	Super::InitializeComponent();

	if (Velocity.SizeSquared() > 0.f)
	{
		// InitialSpeed > 0 overrides initial velocity magnitude.
		if (InitialSpeed > 0.f)
		{
			Velocity = Velocity.GetSafeNormal() * InitialSpeed;
		}

		if (bInitialVelocityInLocalSpace)
		{
			SetVelocityInLocalSpace(Velocity);
		}

		if (bRotationFollowsVelocity)
		{
			if (UpdatedComponent)
			{
				UpdatedComponent->SetWorldRotation(Velocity.Rotation());
			}
		}

		UpdateComponentVelocity();
		
		if (UpdatedPrimitive && UpdatedPrimitive->IsSimulatingPhysics())
		{
			UpdatedPrimitive->SetPhysicsLinearVelocity(Velocity);
		}
	}
}


void UProjectileMovementComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	QUICK_SCOPE_CYCLE_COUNTER( STAT_ProjectileMovementComponent_TickComponent );

	// skip if don't want component updated when not rendered or updated component can't move
	if (HasStoppedSimulation() || ShouldSkipUpdate(DeltaTime))
	{
		return;
	}

	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!IsValid(UpdatedComponent))
	{
		return;
	}

	AActor* ActorOwner = UpdatedComponent->GetOwner();
	if ( !ActorOwner || !CheckStillInWorld() )
	{
		return;
	}

	if (UpdatedComponent->IsSimulatingPhysics())
	{
		return;
	}

	float RemainingTime	= DeltaTime;
	uint32 NumBounces = 0;
	int32 Iterations = 0;
	FHitResult Hit(1.f);
	
	while( RemainingTime >= MIN_TICK_TIME && (Iterations < MaxSimulationIterations) && !ActorOwner->IsPendingKill() && !HasStoppedSimulation() )
	{
		Iterations++;

		// subdivide long ticks to more closely follow parabolic trajectory
		const float TimeTick = ShouldUseSubStepping() ? GetSimulationTimeStep(RemainingTime, Iterations) : RemainingTime;
		RemainingTime -= TimeTick;

		Hit.Time = 1.f;
		const FVector OldVelocity = Velocity;
		const FVector MoveDelta = ComputeMoveDelta(OldVelocity, TimeTick);

		const FQuat NewRotation = (bRotationFollowsVelocity && !OldVelocity.IsNearlyZero(0.01f)) ? OldVelocity.ToOrientationQuat() : UpdatedComponent->GetComponentQuat();

		// Move the component
		if (bShouldBounce)
		{
			// If we can bounce, we are allowed to move out of penetrations, so use SafeMoveUpdatedComponent which does that automatically.
			SafeMoveUpdatedComponent( MoveDelta, NewRotation, true, Hit );
		}
		else
		{
			// If we can't bounce, then we shouldn't adjust if initially penetrating, because that should be a blocking hit that causes a hit event and stop simulation.
			TGuardValue<EMoveComponentFlags> ScopedFlagRestore(MoveComponentFlags, MoveComponentFlags | MOVECOMP_NeverIgnoreBlockingOverlaps);
			MoveUpdatedComponent(MoveDelta, NewRotation, true, &Hit );
		}
		
		// If we hit a trigger that destroyed us, abort.
		if( ActorOwner->IsPendingKill() || HasStoppedSimulation() )
		{
			return;
		}

		// Handle hit result after movement
		if( !Hit.bBlockingHit )
		{
			PreviousHitTime = 1.f;
			bIsSliding = false;

			// Only calculate new velocity if events didn't change it during the movement update.
			if (Velocity == OldVelocity)
			{
				Velocity = ComputeVelocity(Velocity, TimeTick);				
			}
		}
		else
		{
			// Only calculate new velocity if events didn't change it during the movement update.
			if (Velocity == OldVelocity)
			{
				// re-calculate end velocity for partial time
				Velocity = (Hit.Time > KINDA_SMALL_NUMBER) ? ComputeVelocity(OldVelocity, TimeTick * Hit.Time) : OldVelocity;
			}

			// Handle blocking hit
			float SubTickTimeRemaining = TimeTick * (1.f - Hit.Time);
			const EHandleBlockingHitResult HandleBlockingResult = HandleBlockingHit(Hit, TimeTick, MoveDelta, SubTickTimeRemaining);
			if (HandleBlockingResult == EHandleBlockingHitResult::Abort || HasStoppedSimulation())
			{
				break;
			}
			else if (HandleBlockingResult == EHandleBlockingHitResult::Deflect)
			{
				NumBounces++;
				HandleDeflection(Hit, OldVelocity, NumBounces, SubTickTimeRemaining);
				PreviousHitTime = Hit.Time;
				PreviousHitNormal = ConstrainNormalToPlane(Hit.Normal);
			}
			else if (HandleBlockingResult == EHandleBlockingHitResult::AdvanceNextSubstep)
			{
				// Reset deflection logic to ignore this hit
				PreviousHitTime = 1.f;
			}
			else
			{
				// Unhandled EHandleBlockingHitResult
				checkNoEntry();
			}
			
			
			// A few initial bounces should add more time and iterations to complete most of the simulation.
			if (NumBounces <= 2 && SubTickTimeRemaining >= MIN_TICK_TIME)
			{
				RemainingTime += SubTickTimeRemaining;
				Iterations--;
			}
		}
	}

	UpdateComponentVelocity();
}


bool UProjectileMovementComponent::HandleDeflection(FHitResult& Hit, const FVector& OldVelocity, const uint32 NumBounces, float& SubTickTimeRemaining)
{
	const FVector Normal = ConstrainNormalToPlane(Hit.Normal);

	// Multiple hits within very short time period?
	const bool bMultiHit = (PreviousHitTime < 1.f && Hit.Time <= KINDA_SMALL_NUMBER);

	// if velocity still into wall (after HandleBlockingHit() had a chance to adjust), slide along wall
	const float DotTolerance = 0.01f;
	bIsSliding = (bMultiHit && FVector::Coincident(PreviousHitNormal, Normal)) ||
					((Velocity.GetSafeNormal() | Normal) <= DotTolerance);

	if (bIsSliding)
	{
		if (bMultiHit && (PreviousHitNormal | Normal) <= 0.f)
		{
			//90 degree or less corner, so use cross product for direction
			FVector NewDir = (Normal ^ PreviousHitNormal);
			NewDir = NewDir.GetSafeNormal();
			Velocity = Velocity.ProjectOnToNormal(NewDir);
			if ((OldVelocity | Velocity) < 0.f)
			{
				Velocity *= -1.f;
			}
			Velocity = ConstrainDirectionToPlane(Velocity);
		}
		else
		{
			//adjust to move along new wall
			Velocity = ComputeSlideVector(Velocity, 1.f, Normal, Hit);
		}

		// Check min velocity.
		if (Velocity.SizeSquared() < FMath::Square(BounceVelocityStopSimulatingThreshold))
		{
			StopSimulating(Hit);
			return false;
		}

		// Velocity is now parallel to the impact surface.
		if (SubTickTimeRemaining > KINDA_SMALL_NUMBER)
		{
			if (!HandleSliding(Hit, SubTickTimeRemaining))
			{
				return false;
			}
		}
	}

	return true;
}


bool UProjectileMovementComponent::HandleSliding(FHitResult& Hit, float& SubTickTimeRemaining)
{
	FHitResult InitialHit(Hit);
	const FVector OldHitNormal = ConstrainDirectionToPlane(Hit.Normal);

	// Velocity is now parallel to the impact surface.
	// Perform the move now, before adding gravity/accel again, so we don't just keep hitting the surface.
	SafeMoveUpdatedComponent(Velocity * SubTickTimeRemaining, UpdatedComponent->GetComponentQuat(), true, Hit);

	if (HasStoppedSimulation())
	{
		return false;
	}

	// A second hit can deflect the velocity (through the normal bounce code), for the next iteration.
	if (Hit.bBlockingHit)
	{
		const float TimeTick = SubTickTimeRemaining;
		SubTickTimeRemaining = TimeTick * (1.f - Hit.Time);
		
		if (HandleBlockingHit(Hit, TimeTick, Velocity * TimeTick, SubTickTimeRemaining) == EHandleBlockingHitResult::Abort ||
			HasStoppedSimulation())
		{
			return false;
		}
	}
	else
	{
		// Find velocity after elapsed time
		const FVector PostTickVelocity = ComputeVelocity(Velocity, SubTickTimeRemaining);

		// If pointing back into surface, apply friction and acceleration.
		const FVector Force = (PostTickVelocity - Velocity);
		const float ForceDotN = (Force | OldHitNormal);
		if (ForceDotN < 0.f)
		{
			const FVector ProjectedForce = FVector::VectorPlaneProject(Force, OldHitNormal);
			const FVector NewVelocity = Velocity + ProjectedForce;

			const FVector FrictionForce = -NewVelocity.GetSafeNormal() * FMath::Min(-ForceDotN * Friction, NewVelocity.Size());
			Velocity = ConstrainDirectionToPlane(NewVelocity + FrictionForce);
		}
		else
		{
			Velocity = PostTickVelocity;
		}

		// Check min velocity
		if (Velocity.SizeSquared() < FMath::Square(BounceVelocityStopSimulatingThreshold))
		{
			StopSimulating(InitialHit);
			return false;
		}

		SubTickTimeRemaining = 0.f;
	}

	return true;
}


void UProjectileMovementComponent::SetVelocityInLocalSpace(FVector NewVelocity)
{
	if (UpdatedComponent)
	{
		Velocity = UpdatedComponent->GetComponentToWorld().TransformVectorNoScale(NewVelocity);
	}
}


FVector UProjectileMovementComponent::ComputeVelocity(FVector InitialVelocity, float DeltaTime) const
{
	// v = v0 + a*t
	const FVector Acceleration = ComputeAcceleration(InitialVelocity, DeltaTime);
	FVector NewVelocity = InitialVelocity + (Acceleration * DeltaTime);

	return LimitVelocity(NewVelocity);
}


FVector UProjectileMovementComponent::LimitVelocity(FVector NewVelocity) const
{
	const float CurrentMaxSpeed = GetMaxSpeed();
	if (CurrentMaxSpeed > 0.f)
	{
		NewVelocity = NewVelocity.GetClampedToMaxSize(CurrentMaxSpeed);
	}

	return ConstrainDirectionToPlane(NewVelocity);
}

FVector UProjectileMovementComponent::ComputeMoveDelta(const FVector& InVelocity, float DeltaTime) const
{
	// Velocity Verlet integration (http://en.wikipedia.org/wiki/Verlet_integration#Velocity_Verlet)
	// The addition of p0 is done outside this method, we are just computing the delta.
	// p = p0 + v0*t + 1/2*a*t^2

	// We use ComputeVelocity() here to infer the acceleration, to make it easier to apply custom velocities.
	// p = p0 + v0*t + 1/2*((v1-v0)/t)*t^2
	// p = p0 + v0*t + 1/2*((v1-v0))*t

	const FVector NewVelocity = ComputeVelocity(InVelocity, DeltaTime);
	const FVector Delta = (InVelocity * DeltaTime) + (NewVelocity - InVelocity) * (0.5f * DeltaTime);
	return Delta;
}

FVector UProjectileMovementComponent::ComputeAcceleration(const FVector& InVelocity, float DeltaTime) const
{
	FVector Acceleration(FVector::ZeroVector);

	Acceleration.Z += GetGravityZ();

	if (bIsHomingProjectile && HomingTargetComponent.IsValid())
	{
		Acceleration += ComputeHomingAcceleration(InVelocity, DeltaTime);
	}

	return Acceleration;
}

// Allow the projectile to track towards its homing target.
FVector UProjectileMovementComponent::ComputeHomingAcceleration(const FVector& InVelocity, float DeltaTime) const
{
	FVector HomingAcceleration = ((HomingTargetComponent->GetComponentLocation() - UpdatedComponent->GetComponentLocation()).GetSafeNormal() * HomingAccelerationMagnitude);
	return HomingAcceleration;
}


float UProjectileMovementComponent::GetGravityZ() const
{
	// TODO: apply buoyancy if in water
	return ShouldApplyGravity() ? Super::GetGravityZ() * ProjectileGravityScale : 0.f;
}


void UProjectileMovementComponent::StopSimulating(const FHitResult& HitResult)
{
	SetUpdatedComponent(NULL);
	Velocity = FVector::ZeroVector;
	OnProjectileStop.Broadcast(HitResult);
}


UProjectileMovementComponent::EHandleBlockingHitResult UProjectileMovementComponent::HandleBlockingHit(const FHitResult& Hit, float TimeTick, const FVector& MoveDelta, float& SubTickTimeRemaining)
{
	AActor* ActorOwner = UpdatedComponent ? UpdatedComponent->GetOwner() : NULL;
	if (!CheckStillInWorld() || !ActorOwner || ActorOwner->IsPendingKill())
	{
		return EHandleBlockingHitResult::Abort;
	}
	
	HandleImpact(Hit, TimeTick, MoveDelta);
	
	if (ActorOwner->IsPendingKill() || HasStoppedSimulation())
	{
		return EHandleBlockingHitResult::Abort;
	}

	SubTickTimeRemaining = TimeTick * (1.f - Hit.Time);
	return EHandleBlockingHitResult::Deflect;
}
 
FVector UProjectileMovementComponent::ComputeBounceResult(const FHitResult& Hit, float TimeSlice, const FVector& MoveDelta)
{
	FVector TempVelocity = Velocity;
	const FVector Normal = ConstrainNormalToPlane(Hit.Normal);
	const float VDotNormal = (TempVelocity | Normal);

	// Only if velocity is opposed by normal
	if (VDotNormal < 0.f)
	{
		// Project velocity onto normal in reflected direction.
		const FVector ProjectedNormal = Normal * -VDotNormal;

		// Point velocity in direction parallel to surface
		TempVelocity += ProjectedNormal;

		// Only tangential velocity should be affected by friction.
		const float ScaledFriction = (bBounceAngleAffectsFriction || bIsSliding) ? FMath::Clamp(-VDotNormal / TempVelocity.Size(), 0.f, 1.f) * Friction : Friction;
		TempVelocity *= FMath::Clamp(1.f - ScaledFriction, 0.f, 1.f);

		// Coefficient of restitution only applies perpendicular to impact.
		TempVelocity += (ProjectedNormal * FMath::Max(Bounciness, 0.f));

		// Bounciness could cause us to exceed max speed.
		TempVelocity = LimitVelocity(TempVelocity);
	}

	return TempVelocity;
}

void UProjectileMovementComponent::HandleImpact(const FHitResult& Hit, float TimeSlice, const FVector& MoveDelta)
{
	bool bStopSimulating = false;

	if (bShouldBounce)
	{
		const FVector OldVelocity = Velocity;
		Velocity = ComputeBounceResult(Hit, TimeSlice, MoveDelta);

		// Trigger bounce events
		OnProjectileBounce.Broadcast(Hit, OldVelocity);

		// Event may modify velocity or threshold, so check velocity threshold now.
		Velocity = LimitVelocity(Velocity);
		if (Velocity.SizeSquared() < FMath::Square(BounceVelocityStopSimulatingThreshold))
		{
			bStopSimulating = true;
		}
	}
	else
	{
		bStopSimulating = true;
	}


	if (bStopSimulating)
	{
		StopSimulating(Hit);
	}
}

bool UProjectileMovementComponent::CheckStillInWorld()
{
	if ( !UpdatedComponent )
	{
		return false;
	}

	const UWorld* MyWorld = GetWorld();
	if (!MyWorld)
	{
		return false;
	}

	// check the variations of KillZ
	AWorldSettings* WorldSettings = MyWorld->GetWorldSettings( true );
	if (!WorldSettings->bEnableWorldBoundsChecks)
	{
		return true;
	}
	AActor* ActorOwner = UpdatedComponent->GetOwner();
	if (!IsValid(ActorOwner))
	{
		return false;
	}
	if( ActorOwner->GetActorLocation().Z < WorldSettings->KillZ )
	{
		UDamageType const* DmgType = WorldSettings->KillZDamageType ? WorldSettings->KillZDamageType->GetDefaultObject<UDamageType>() : GetDefault<UDamageType>();
		ActorOwner->FellOutOfWorld(*DmgType);
		return false;
	}
	// Check if box has poked outside the world
	else if( UpdatedComponent && UpdatedComponent->IsRegistered() )
	{
		const FBox&	Box = UpdatedComponent->Bounds.GetBox();
		if(	Box.Min.X < -HALF_WORLD_MAX || Box.Max.X > HALF_WORLD_MAX ||
			Box.Min.Y < -HALF_WORLD_MAX || Box.Max.Y > HALF_WORLD_MAX ||
			Box.Min.Z < -HALF_WORLD_MAX || Box.Max.Z > HALF_WORLD_MAX )
		{
			UE_LOG(LogProjectileMovement, Warning, TEXT("%s is outside the world bounds!"), *ActorOwner->GetName());
			ActorOwner->OutsideWorldBounds();
			// not safe to use physics or collision at this point
			ActorOwner->SetActorEnableCollision(false);
			FHitResult Hit(1.f);
			StopSimulating(Hit);
			return false;
		}
	}
	return true;
}


bool UProjectileMovementComponent::ShouldUseSubStepping() const
{
	return bForceSubStepping || (GetGravityZ() != 0.f) || (bIsHomingProjectile && HomingTargetComponent.IsValid());
}


float UProjectileMovementComponent::GetSimulationTimeStep(float RemainingTime, int32 Iterations) const
{
	if (RemainingTime > MaxSimulationTimeStep)
	{
		if (Iterations < MaxSimulationIterations)
		{
			// Subdivide moves to be no longer than MaxSimulationTimeStep seconds
			RemainingTime = FMath::Min(MaxSimulationTimeStep, RemainingTime * 0.5f);
		}
		else
		{
			// If this is the last iteration, just use all the remaining time. This is usually better than cutting things short, as the simulation won't move far enough otherwise.
			// Print a throttled warning.
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			static uint32 s_WarningCount = 0;
			if ((s_WarningCount++ < 100) || (GFrameCounter & 15) == 0)
			{
				UE_LOG(LogProjectileMovement, Warning, TEXT("GetSimulationTimeStep() - Max iterations %d hit while remaining time %.6f > MaxSimulationTimeStep (%.3f) for '%s'"), MaxSimulationIterations, RemainingTime, MaxSimulationTimeStep, *GetPathNameSafe(UpdatedComponent));
			}
#endif
		}
	}

	// no less than MIN_TICK_TIME (to avoid potential divide-by-zero during simulation).
	return FMath::Max(MIN_TICK_TIME, RemainingTime);
}
