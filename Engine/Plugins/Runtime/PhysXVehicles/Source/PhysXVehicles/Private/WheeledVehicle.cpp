// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	Vehicle.cpp: AWheeledVehicle implementation
	TODO: Put description here
=============================================================================*/

#include "WheeledVehicle.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "WheeledVehicleMovementComponent.h"
#include "WheeledVehicleMovementComponent4W.h"
#include "DisplayDebugHelpers.h"

FName AWheeledVehicle::VehicleMovementComponentName(TEXT("MovementComp"));
FName AWheeledVehicle::VehicleMeshComponentName(TEXT("VehicleMesh"));

AWheeledVehicle::AWheeledVehicle(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Mesh = CreateDefaultSubobject<USkeletalMeshComponent>(VehicleMeshComponentName);
	Mesh->SetCollisionProfileName(UCollisionProfile::Vehicle_ProfileName);
	Mesh->BodyInstance.bSimulatePhysics = true;
	Mesh->BodyInstance.bNotifyRigidBodyCollision = true;
	Mesh->BodyInstance.bUseCCD = true;
	Mesh->bBlendPhysics = true;
	Mesh->bGenerateOverlapEvents = true;
	Mesh->SetCanEverAffectNavigation(false);
	RootComponent = Mesh;

	VehicleMovement = CreateDefaultSubobject<UWheeledVehicleMovementComponent, UWheeledVehicleMovementComponent4W>(VehicleMovementComponentName);
	VehicleMovement->SetIsReplicated(true); // Enable replication by default
	VehicleMovement->UpdatedComponent = Mesh;
}

void AWheeledVehicle::DisplayDebug(UCanvas* Canvas, const FDebugDisplayInfo& DebugDisplay, float& YL, float& YPos)
{
	static FName NAME_Vehicle = FName(TEXT("Vehicle"));

	Super::DisplayDebug(Canvas, DebugDisplay, YL, YPos);

	if (DebugDisplay.IsDisplayOn(NAME_Vehicle))
	{
		GetVehicleMovementComponent()->DrawDebug(Canvas, YL, YPos);
	}
}

class UWheeledVehicleMovementComponent* AWheeledVehicle::GetVehicleMovementComponent() const
{
	return VehicleMovement;
}

