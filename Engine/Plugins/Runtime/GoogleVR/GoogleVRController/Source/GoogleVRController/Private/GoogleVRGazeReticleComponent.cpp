// Copyright 2017 Google Inc.


#include "Classes/GoogleVRGazeReticleComponent.h"
#include "GoogleVRController.h"
#include "Classes/GoogleVRPointerInputComponent.h"
#include "Classes/GoogleVRControllerFunctionLibrary.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/WorldSettings.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogGoogleVRGazeReticle, Log, All);

UGoogleVRGazeReticleComponent::UGoogleVRGazeReticleComponent()
: Mesh(nullptr)
, Material(nullptr)
, ReticleDistanceMin(0.45f)
, ReticleDistanceMax(2.5f)
, ReticleSize(1.0f)
, ReticleInnerAngleMin(0.0f)
, ReticleOuterAngleMin(0.6f)
, ReticleGrowAngle(1.5f)
, ReticleGrowSpeed(10.0f)
, RequireInputComponent(true)
, ReticleMeshComponent(nullptr)
, CameraComponent(nullptr)
, ReticleInnerDiameter(0.0f)
, ReticleOuterDiameter(0.0f)
, TargetReticleInnerDiameter(0.0f)
, TargetReticleOuterDiameter(0.0f)
, CurrentReticleDistance(0.0f)
, IsReticleExpanded(false)
{
	PrimaryComponentTick.bCanEverTick = true;
	bAutoActivate = true;

	Mesh = Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), NULL, TEXT("/GoogleVRController/GazeReticleMesh")));
	Material = Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), NULL, TEXT("/GoogleVRController/GazeReticleMaterial")));
}
void UGoogleVRGazeReticleComponent::OnRegister()
{
	Super::OnRegister();

	// Create the Controller reticle mesh and attach it.
	ReticleMeshComponent = NewObject<UStaticMeshComponent>(this, TEXT("ReticleMesh"));
	ReticleMeshComponent->SetStaticMesh(Mesh);
	ReticleMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (Material != nullptr)
	{
		ReticleMeshComponent->SetMaterial(0, Material);
	}
	ReticleMeshComponent->SetupAttachment(this);
	ReticleMeshComponent->RegisterComponent();

	SetReticleEnabled(IsActive());
	IsReticleExpanded = true; // Force this to true so we don't return early the first time we do this.
	SetReticleExpanded(false);
	CurrentReticleDistance = GetMaxPointerDistance();
}

void UGoogleVRGazeReticleComponent::BeginPlay()
{
	Super::BeginPlay();

	TArray<UGoogleVRPointerInputComponent*> Components;
	UGoogleVRPointerInputComponent* InputComponent = nullptr;

	GetOwner()->GetComponents(Components);
	if(Components.Num() == 0)
	{
		if (RequireInputComponent)
		{
			UE_LOG(LogGoogleVRGazeReticle, Warning, TEXT("GoogleVRGazeReticleComponent has RequireInputComponent set to true, but the actor does not have a GoogleVRPointerInputComponent. Creating GoogleVRPointerInputComponent."));

			InputComponent = NewObject<UGoogleVRPointerInputComponent>(GetOwner(), "GoogleVRPointerInputComponent");
			InputComponent->RegisterComponent();
		}
	}
	else
	{
		InputComponent = Components[0];
	}

	// If we found an InputComponent and it doesn't already have a Pointer, automatically set
	// it to this. If you want to switch between multiple pointers, or have multiple InputComponents,
	// then set the Pointers manually.
	if (InputComponent != nullptr && InputComponent->GetPointer() == nullptr)
	{
		InputComponent->SetPointer(this);
	}

	TArray<UCameraComponent*> CameraComponents;
	GetOwner()->GetComponents(CameraComponents);
	if (CameraComponents.Num() == 0)
	{
		UE_LOG(LogGoogleVRGazeReticle, Warning, TEXT("GoogleVRGazeReticleComponent is unable to find a CameraComponent on it's owner."));
	}
	else
	{
		CameraComponent = CameraComponents[0];
	}
}

void UGoogleVRGazeReticleComponent::Activate(bool bReset)
{
	Super::Activate(bReset);

	SetReticleEnabled(true);
}

void UGoogleVRGazeReticleComponent::Deactivate()
{
	Super::Deactivate();

	SetReticleEnabled(false);
}

void UGoogleVRGazeReticleComponent::TickComponent( float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction )
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UpdateReticleDiameter(DeltaTime);

	SetWorldLocationAndRotation(CameraComponent->GetComponentLocation(), CameraComponent->GetComponentQuat());

	ReticleMeshComponent->SetScalarParameterValueOnMaterials("ReticleDistance", CurrentReticleDistance);

	float WorldToMetersScale = GetWorldToMetersScale();
	float CurrentReticleDistanceMeters = CurrentReticleDistance / WorldToMetersScale;
	ReticleMeshComponent->SetScalarParameterValueOnMaterials("InnerDiameter", CurrentReticleDistanceMeters * ReticleInnerDiameter * ReticleSize);
	ReticleMeshComponent->SetScalarParameterValueOnMaterials("OuterDiameter", CurrentReticleDistanceMeters * ReticleOuterDiameter * ReticleSize);
}

void UGoogleVRGazeReticleComponent::SetReticleExpanded(bool NewIsReticleExpanded)
{
	if (IsReticleExpanded == NewIsReticleExpanded)
	{
		return;
	}

	if (NewIsReticleExpanded)
	{
		TargetReticleInnerDiameter = CalculateReticleDiameter(ReticleInnerAngleMin + ReticleGrowAngle);
		TargetReticleOuterDiameter = CalculateReticleDiameter(ReticleOuterAngleMin + ReticleGrowAngle);
	}
	else
	{
		TargetReticleInnerDiameter = CalculateReticleDiameter(ReticleInnerAngleMin);
		TargetReticleOuterDiameter = CalculateReticleDiameter(ReticleOuterAngleMin);
	}

	IsReticleExpanded = NewIsReticleExpanded;
}

float UGoogleVRGazeReticleComponent::CalculateReticleDiameter(float ReticleAngleDegrees)
{
	float HalfAngleDegrees = ReticleAngleDegrees * 0.5f;
	return FMath::Tan(PI/(180.f) * HalfAngleDegrees) * 2.0f;
}

void UGoogleVRGazeReticleComponent::UpdateReticleDiameter(float DeltaTime)
{
	if (ReticleInnerDiameter - TargetReticleInnerDiameter > 0.0f && ReticleInnerDiameter <= 0.1f)
	{
		ReticleInnerDiameter = TargetReticleInnerDiameter;
	}
	else
	{
		ReticleInnerDiameter = FMath::Lerp(ReticleInnerDiameter, TargetReticleInnerDiameter, DeltaTime * ReticleGrowSpeed);
	}

	ReticleOuterDiameter = FMath::Lerp(ReticleOuterDiameter, TargetReticleOuterDiameter, DeltaTime * ReticleGrowSpeed);
}

void UGoogleVRGazeReticleComponent::SetReticleEnabled(bool NewEnabled)
{
	if (ReticleMeshComponent != nullptr)
	{
		ReticleMeshComponent->SetActive(NewEnabled);
		ReticleMeshComponent->SetVisibility(NewEnabled);
	}
}

float UGoogleVRGazeReticleComponent::GetWorldToMetersScale() const
{
	if (GWorld != nullptr)
	{
		return GWorld->GetWorldSettings()->WorldToMeters;
	}

	// Default value, assume Unreal units are in centimeters
	return 100.0f;
}

void UGoogleVRGazeReticleComponent::OnPointerEnter(const FHitResult& HitResult, bool IsHitInteractive)
{
	SetReticleExpanded(IsHitInteractive);
	OnPointerHover(HitResult, IsHitInteractive);
}

void UGoogleVRGazeReticleComponent::OnPointerHover(const FHitResult& HitResult, bool IsHitInteractive)
{
	SetReticleExpanded(IsHitInteractive);
	
	if (CameraComponent != nullptr)
	{
		CurrentReticleDistance = CameraComponent->GetComponentTransform().InverseTransformPosition(HitResult.Location).X;
		float WorldToMetersScale = GetWorldToMetersScale();
		CurrentReticleDistance = FMath::Clamp(CurrentReticleDistance, ReticleDistanceMin * WorldToMetersScale, ReticleDistanceMax * WorldToMetersScale);
	}
	else
	{
		CurrentReticleDistance = GetMaxPointerDistance();
	}
}

void UGoogleVRGazeReticleComponent::OnPointerExit(const FHitResult& HitResult)
{
	SetReticleExpanded(false);
	CurrentReticleDistance = GetMaxPointerDistance();
}

FVector UGoogleVRGazeReticleComponent::GetOrigin() const
{
	if (CameraComponent != nullptr)
	{
		return CameraComponent->GetComponentLocation();
	}
	
	return FVector::ZeroVector;
}

FVector UGoogleVRGazeReticleComponent::GetDirection() const
{
	if (CameraComponent != nullptr)
	{
		return CameraComponent->GetForwardVector();
	}

	return FVector::ZeroVector;
}

void UGoogleVRGazeReticleComponent::GetRadius(float& OutEnterRadius, float& OutExitRadius) const
{
	float DegreesToRadians = PI / (180.f);
	float InnerAngleMinRadians = DegreesToRadians *  ReticleInnerAngleMin;
	float InnerAngleMaxRadians = DegreesToRadians * (ReticleInnerAngleMin + ReticleGrowAngle);

	float WorldToMetersScale = GetWorldToMetersScale();
	OutEnterRadius = WorldToMetersScale * 2.0f * FMath::Tan(InnerAngleMinRadians);
	OutExitRadius = WorldToMetersScale * 2.0f * FMath::Tan(InnerAngleMaxRadians);
}

float UGoogleVRGazeReticleComponent::GetMaxPointerDistance() const
{
	float WorldToMetersScale = GetWorldToMetersScale();
	return ReticleDistanceMax * WorldToMetersScale;
}

bool UGoogleVRGazeReticleComponent::IsPointerActive() const
{
	return IsActive();
}
