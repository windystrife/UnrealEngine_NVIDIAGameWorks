// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "GameFramework/RotatingMovementComponent.h"

URotatingMovementComponent::URotatingMovementComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	RotationRate.Yaw = 180.0f;
	bRotationInLocalSpace = true;
}


void URotatingMovementComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	// skip if we don't want component updated when not rendered or if updated component can't move
	if (ShouldSkipUpdate(DeltaTime))
	{
		return;
	}

	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!IsValid(UpdatedComponent))
	{
		return;
	}

	// Compute new rotation
	const FQuat OldRotation = UpdatedComponent->GetComponentQuat();
	const FQuat DeltaRotation = (RotationRate * DeltaTime).Quaternion();
	const FQuat NewRotation = bRotationInLocalSpace ? (OldRotation * DeltaRotation) : (DeltaRotation * OldRotation);

	// Compute new location
	FVector DeltaLocation = FVector::ZeroVector;
	if (!PivotTranslation.IsZero())
	{
		const FVector OldPivot = OldRotation.RotateVector(PivotTranslation);
		const FVector NewPivot = NewRotation.RotateVector(PivotTranslation);
		DeltaLocation = (OldPivot - NewPivot); // ConstrainDirectionToPlane() not necessary because it's done by MoveUpdatedComponent() below.
	}

	const bool bEnableCollision = false;
	MoveUpdatedComponent(DeltaLocation, NewRotation, bEnableCollision);
}
