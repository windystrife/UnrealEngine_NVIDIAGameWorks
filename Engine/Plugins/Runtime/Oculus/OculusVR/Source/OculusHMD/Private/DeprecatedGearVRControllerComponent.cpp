// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "../Public/DeprecatedGearVRControllerComponent.h"
//#include "../Public/GearVRControllerFunctionLibrary.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "MotionControllerComponent.h"
#include "ModuleManager.h"
#include "UObject/ConstructorHelpers.h"

UDEPRECATED_DeprecatedGearVRControllerComponent::UDEPRECATED_DeprecatedGearVRControllerComponent()
	: ControllerMesh(nullptr)
	, MotionControllerComponent(nullptr)
	, ControllerMeshComponent(nullptr)
{
	PrimaryComponentTick.bCanEverTick = false;
	bAutoActivate = true;

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
    ControllerMesh = LoadObject<UStaticMesh>(/*Outer =*/nullptr, TEXT("/OculusVR/Meshes/GearVRController.GearVRController"));
    PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

UMotionControllerComponent* UDEPRECATED_DeprecatedGearVRControllerComponent::GetMotionController() const
{
	return MotionControllerComponent;
}

UStaticMeshComponent* UDEPRECATED_DeprecatedGearVRControllerComponent::GetControllerMesh() const
{
	return ControllerMeshComponent;
}

void UDEPRECATED_DeprecatedGearVRControllerComponent::OnRegister()
{
	Super::OnRegister();

	MotionControllerComponent = NewObject<UMotionControllerComponent>(this, TEXT("MotionController"));
	MotionControllerComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MotionControllerComponent->SetupAttachment(this);
	MotionControllerComponent->RegisterComponent();

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	if (ensure(ControllerMesh != nullptr))
	{
		ControllerMeshComponent = NewObject<UStaticMeshComponent>(this, TEXT("ControllerMesh"));
		ControllerMeshComponent->SetStaticMesh(ControllerMesh);
		ControllerMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		ControllerMeshComponent->SetupAttachment(MotionControllerComponent);
		ControllerMeshComponent->RegisterComponent();
		ControllerMeshComponent->SetRelativeRotation(FQuat::MakeFromEuler(FVector(0.0f, 0.0f, 90.0f)));
	}
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}
