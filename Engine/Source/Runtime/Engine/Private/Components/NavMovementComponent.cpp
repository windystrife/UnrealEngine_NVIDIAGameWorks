// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "GameFramework/NavMovementComponent.h"
#include "AI/Navigation/NavigationSystem.h"
#include "Components/CapsuleComponent.h"
#include "Navigation/PathFollowingComponent.h"

//----------------------------------------------------------------------//
// FNavAgentProperties
//----------------------------------------------------------------------//
const FNavAgentProperties FNavAgentProperties::DefaultProperties;

void FNavAgentProperties::UpdateWithCollisionComponent(UShapeComponent* CollisionComponent)
{
	check( CollisionComponent != NULL);
	AgentRadius = CollisionComponent->Bounds.SphereRadius;
}

bool FNavAgentProperties::IsNavDataMatching(const FNavAgentProperties& Other) const
{
	return (PreferredNavData == Other.PreferredNavData || PreferredNavData == nullptr || Other.PreferredNavData == nullptr);
}

//----------------------------------------------------------------------//
// UMovementComponent
//----------------------------------------------------------------------//
UNavMovementComponent::UNavMovementComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bUpdateNavAgentWithOwnersCollision(true)
	, bUseAccelerationForPaths(false)
	, bUseFixedBrakingDistanceForPaths(false)
	, bStopMovementAbortPaths(true)
{
}

FBasedPosition UNavMovementComponent::GetActorFeetLocationBased() const
{
	return FBasedPosition(NULL, GetActorFeetLocation());
}

void UNavMovementComponent::RequestDirectMove(const FVector& MoveVelocity, bool bForceMaxSpeed)
{
	Velocity = MoveVelocity;
}

void UNavMovementComponent::RequestPathMove(const FVector& MoveInput)
{
	// empty in base class, requires at least PawnMovementComponent for input related operations
}

bool UNavMovementComponent::CanStopPathFollowing() const
{
	return true;
}

float UNavMovementComponent::GetPathFollowingBrakingDistance(float MaxSpeed) const
{
	return bUseFixedBrakingDistanceForPaths ? FixedPathBrakingDistance : MaxSpeed;
}

void UNavMovementComponent::SetFixedBrakingDistance(float DistanceToEndOfPath)
{
	if (DistanceToEndOfPath > KINDA_SMALL_NUMBER)
	{
		bUseFixedBrakingDistanceForPaths = true;
		FixedPathBrakingDistance = DistanceToEndOfPath;
	}
}

void UNavMovementComponent::ClearFixedBrakingDistance()
{
	bUseFixedBrakingDistanceForPaths = false;
}

void UNavMovementComponent::StopActiveMovement()
{
	if (PathFollowingComp.IsValid() && bStopMovementAbortPaths)
	{
		PathFollowingComp->AbortMove(*this, FPathFollowingResultFlags::MovementStop);
	}
}

void UNavMovementComponent::UpdateNavAgent(const AActor& Owner)
{
	ensure(&Owner == GetOwner());
	if (ShouldUpdateNavAgentWithOwnersCollision() == false)
	{
		return;
	}

	// initialize properties from navigation system
	UNavigationSystem* NavSys = UNavigationSystem::GetCurrent(GetWorld());
	if (NavSys != nullptr)
	{
		NavAgentProps.NavWalkingSearchHeightScale = NavSys->GetDefaultSupportedAgentConfig().NavWalkingSearchHeightScale;
	}

	// Can't call GetSimpleCollisionCylinder(), because no components will be registered.
	float BoundRadius, BoundHalfHeight;	
	Owner.GetSimpleCollisionCylinder(BoundRadius, BoundHalfHeight);
	NavAgentProps.AgentRadius = BoundRadius;
	NavAgentProps.AgentHeight = BoundHalfHeight * 2.f;
}

void UNavMovementComponent::UpdateNavAgent(const UCapsuleComponent& CapsuleComponent)
{
	if (ShouldUpdateNavAgentWithOwnersCollision() == false)
	{
		return;
	}

	// initialize properties from navigation system
	UNavigationSystem* NavSys = UNavigationSystem::GetCurrent(GetWorld());
	if (NavSys != nullptr)
	{
		NavAgentProps.NavWalkingSearchHeightScale = NavSys->GetDefaultSupportedAgentConfig().NavWalkingSearchHeightScale;
	}

	NavAgentProps.AgentRadius = CapsuleComponent.GetScaledCapsuleRadius();
	NavAgentProps.AgentHeight = CapsuleComponent.GetScaledCapsuleHalfHeight() * 2.f;
}

void UNavMovementComponent::SetUpdateNavAgentWithOwnersCollisions(bool bUpdateWithOwner)
{
	bUpdateNavAgentWithOwnersCollision = bUpdateWithOwner;
}
