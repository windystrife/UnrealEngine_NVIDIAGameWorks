// Copyright 2017 Google Inc.

#include "Classes/GoogleVRMotionControllerComponent.h"
#include "GoogleVRController.h"
#include "GoogleVRLaserPlaneComponent.h"
#include "Classes/GoogleVRPointerInputComponent.h"
#include "Classes/GoogleVRLaserVisualComponent.h"
#include "Classes/GoogleVRControllerFunctionLibrary.h"
#include "MotionControllerComponent.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemComponent.h"
#include "Components/MaterialBillboardComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/WorldSettings.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogGoogleVRMotionController, Log, All);

const FVector UGoogleVRMotionControllerComponent::TOUCHPAD_POINT_DIMENSIONS = FVector(0.01f, 0.01f, 0.0004f);
const FVector UGoogleVRMotionControllerComponent::BATTERY_INDICATOR_TRANSLATION = FVector(-3.0f, 0.0f, 0.001f);
const FVector UGoogleVRMotionControllerComponent::BATTERY_INDICATOR_SCALE = FVector(0.032f, 0.015f, 1.0f);
const FQuat UGoogleVRMotionControllerComponent::BATTERY_INDICATOR_ROTATION = FQuat(FVector(0.0f, 0.0f, 1.0f), PI/2.0f);

UGoogleVRMotionControllerComponent::UGoogleVRMotionControllerComponent()
: ControllerMesh(nullptr)
, ControllerTouchPointMesh(nullptr)
, IdleMaterial(nullptr)
, TouchpadMaterial(nullptr)
, AppMaterial(nullptr)
, SystemMaterial(nullptr)
, ControllerTouchPointMaterial(nullptr)
, ParameterCollection(nullptr)
, ControllerBatteryMesh(nullptr)
, BatteryTextureParameterName("Texture")
, BatteryUnknownTexture(nullptr)
, BatteryFullTexture(nullptr)
, BatteryAlmostFullTexture(nullptr)
, BatteryMediumTexture(nullptr)
, BatteryLowTexture(nullptr)
, BatteryCriticalLowTexture(nullptr)
, BatteryChargingTexture(nullptr)
, EnterRadiusCoeff(0.1f)
, ExitRadiusCoeff(0.2f)
, RequireInputComponent(true)
, IsLockedToHead(false)
, TranslucentSortPriority(1)
, PlayerController(nullptr)
, MotionControllerComponent(nullptr)
, ControllerMeshComponent(nullptr)
, ControllerTouchPointMeshComponent(nullptr)
, ControllerBatteryMeshComponent(nullptr)
, ControllerBatteryStaticMaterial(nullptr)
, ControllerBatteryMaterial(nullptr)
, LaserVisualComponent(nullptr)
, TouchMeshScale(FVector::ZeroVector)
, bAreSubComponentsEnabled(false)
, LastKnownBatteryState(EGoogleVRControllerBatteryLevel::Unknown)
, bBatteryWasCharging(false)
{
	PrimaryComponentTick.bCanEverTick = true;
	bAutoActivate = true;

	if (FModuleManager::Get().IsModuleLoaded("GoogleVRController"))
	{
		ControllerMesh = Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), NULL, TEXT("/GoogleVRController/ControllerMesh")));
		ControllerTouchPointMesh = Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), NULL, TEXT("/Engine/BasicShapes/Cylinder")));
		IdleMaterial = Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), NULL, TEXT("/GoogleVRController/ControllerIdleMaterial")));
		TouchpadMaterial = Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), NULL, TEXT("/GoogleVRController/ControllerPadMaterial")));
		AppMaterial = Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), NULL, TEXT("/GoogleVRController/ControllerAppMaterial")));
		SystemMaterial = Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), NULL, TEXT("/GoogleVRController/ControllerSysMaterial")));
		ControllerTouchPointMaterial = Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), NULL, TEXT("/GoogleVRController/TouchMaterial")));
		ControllerBatteryMesh = Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), NULL, TEXT("/Engine/BasicShapes/Plane")));
		ControllerBatteryStaticMaterial = Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), NULL, TEXT("/GoogleVRController/BatteryIndicatorMaterial")));
		BatteryUnknownTexture = Cast<UTexture2D>(StaticLoadObject(UTexture2D::StaticClass(), NULL, TEXT("/GoogleVRController/BatteryIndicatorUnknown")));
		BatteryFullTexture = Cast<UTexture2D>(StaticLoadObject(UTexture2D::StaticClass(), NULL, TEXT("/GoogleVRController/BatteryIndicatorFull")));
		BatteryAlmostFullTexture = Cast<UTexture2D>(StaticLoadObject(UTexture2D::StaticClass(), NULL, TEXT("/GoogleVRController/BatteryIndicatorAlmostFull")));
		BatteryMediumTexture = Cast<UTexture2D>(StaticLoadObject(UTexture2D::StaticClass(), NULL, TEXT("/GoogleVRController/BatteryIndicatorMedium")));
		BatteryLowTexture = Cast<UTexture2D>(StaticLoadObject(UTexture2D::StaticClass(), NULL, TEXT("/GoogleVRController/BatteryIndicatorLow")));
		BatteryCriticalLowTexture = Cast<UTexture2D>(StaticLoadObject(UTexture2D::StaticClass(), NULL, TEXT("/GoogleVRController/BatteryIndicatorCriticalLow")));
		BatteryChargingTexture = Cast<UTexture2D>(StaticLoadObject(UTexture2D::StaticClass(), NULL, TEXT("/GoogleVRController/BatteryIndicatorCharging")));
		ParameterCollection = Cast<UMaterialParameterCollection>(StaticLoadObject(UMaterialParameterCollection::StaticClass(), NULL, TEXT("/GoogleVRController/ControllerParameters")));
	}
}

UMotionControllerComponent* UGoogleVRMotionControllerComponent::GetMotionController() const
{
	return MotionControllerComponent;
}

UStaticMeshComponent* UGoogleVRMotionControllerComponent::GetControllerMesh() const
{
	return ControllerMeshComponent;
}

UMaterialInstanceDynamic* UGoogleVRMotionControllerComponent::GetLaserMaterial() const
{
	return LaserVisualComponent != nullptr ? LaserVisualComponent->GetLaserMaterial() : nullptr;
}

void UGoogleVRMotionControllerComponent::SetPointerDistance(float Distance)
{
	if (LaserVisualComponent != nullptr)
	{
		LaserVisualComponent->SetPointerDistance(Distance, GetWorldToMetersScale(), PlayerController->PlayerCameraManager->GetCameraLocation());
	}
}

void UGoogleVRMotionControllerComponent::OnRegister()
{
	Super::OnRegister();

	// Check that required UPROPERTIES are set.
	check(ControllerMesh != nullptr);
	check(ControllerTouchPointMesh != nullptr);
	check(IdleMaterial != nullptr);
	check(TouchpadMaterial != nullptr);
	check(AppMaterial != nullptr);
	check(SystemMaterial != nullptr);
	check(ControllerTouchPointMaterial != nullptr);
	check(ControllerBatteryMesh != nullptr);
	check(ControllerBatteryStaticMaterial != nullptr);
	check(BatteryUnknownTexture != nullptr);
	check(BatteryFullTexture != nullptr);
	check(BatteryAlmostFullTexture != nullptr);
	check(BatteryMediumTexture != nullptr);
	check(BatteryLowTexture != nullptr);
	check(BatteryCriticalLowTexture != nullptr);
	check(BatteryChargingTexture != nullptr);
	check(ParameterCollection != nullptr);

	// Get the world to meters scale.
	const float WorldToMetersScale = GetWorldToMetersScale();

	// Create the MotionController and attach it to ourselves.
	MotionControllerComponent = NewObject<UMotionControllerComponent>(this, TEXT("MotionController"));
	MotionControllerComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MotionControllerComponent->SetupAttachment(this);
	MotionControllerComponent->RegisterComponent();

	// Create the Controller mesh and attach it to the MotionController.
	ControllerMeshComponent = NewObject<UStaticMeshComponent>(this, TEXT("ControllerMesh"));
	ControllerMeshComponent->SetStaticMesh(ControllerMesh);
	ControllerMeshComponent->SetTranslucentSortPriority(TranslucentSortPriority);
	ControllerMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ControllerMeshComponent->SetupAttachment(MotionControllerComponent);
	ControllerMeshComponent->RegisterComponent();

	// Position the ControllerMesh so that the back of the model is at the pivot point.
	// This code assumes that the 3d model was exported with the mesh centered.
	FVector BoundsMin, BoundsMax;
	ControllerMeshComponent->GetLocalBounds(BoundsMin, BoundsMax);
	float ControllerLength = BoundsMax.X - BoundsMin.X;
	float ControllerHalfLength = ControllerLength * 0.5f * CONTROLLER_OFFSET_RATIO;
	ControllerMeshComponent->SetRelativeLocation(FVector(ControllerHalfLength, 0.0f, 0.0f));

	// Create the Controller Touch Point Mesh and attach it to the ControllerMesh.
	{
		ControllerTouchPointMeshComponent = NewObject<UStaticMeshComponent>(this, TEXT("ControllerTouchPointMesh"));
		ControllerTouchPointMeshComponent->SetStaticMesh(ControllerTouchPointMesh);
		ControllerTouchPointMeshComponent->SetTranslucentSortPriority(TranslucentSortPriority + 1);
		ControllerTouchPointMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		ControllerTouchPointMeshComponent->SetMaterial(0, ControllerTouchPointMaterial);

		// Determine what the scale of the mesh should be based on the
		// Size of the mesh and the desired size of the touch point.
		// This assumes that the controller is a certain size.
		FVector TouchMeshSize;
		ControllerTouchPointMeshComponent->GetLocalBounds(BoundsMin, BoundsMax);
		TouchMeshSize = BoundsMax - BoundsMin;
		TouchMeshScale = TOUCHPAD_POINT_DIMENSIONS * WorldToMetersScale;
		TouchMeshScale.X /= TouchMeshSize.X;
		TouchMeshScale.Y /= TouchMeshSize.Y;
		TouchMeshScale.Z /= TouchMeshSize.Z;

		ControllerTouchPointMeshComponent->SetRelativeScale3D(TouchMeshScale);
		ControllerTouchPointMeshComponent->SetupAttachment(ControllerMeshComponent);
		ControllerTouchPointMeshComponent->RegisterComponent();
	}

	// Create the Controller Battery Mesh and attach it to the ControllerMesh.
	{
		ControllerBatteryMeshComponent = NewObject<UStaticMeshComponent>(this, TEXT("ControllerBatteryMesh"));
		ControllerBatteryMeshComponent->SetStaticMesh(ControllerBatteryMesh);
		ControllerBatteryMeshComponent->SetTranslucentSortPriority(TranslucentSortPriority + 1);
		ControllerBatteryMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

		// Create the dynamic material that will hold the current battery level texture.
		ControllerBatteryMaterial = UMaterialInstanceDynamic::Create(ControllerBatteryStaticMaterial->GetMaterial(), this);
		ControllerBatteryMaterial->SetTextureParameterValue(BatteryTextureParameterName, BatteryUnknownTexture);
		ControllerBatteryMeshComponent->SetMaterial(0, ControllerBatteryMaterial);

		// Determine the size and position of the mesh.  This assumes that
		// the controller is a certain size.
		FTransform BatteryTransform;
		BatteryTransform.SetTranslation(BATTERY_INDICATOR_TRANSLATION);
		BatteryTransform.SetScale3D(BATTERY_INDICATOR_SCALE);
		BatteryTransform.SetRotation(BATTERY_INDICATOR_ROTATION);
		ControllerBatteryMeshComponent->SetRelativeTransform(BatteryTransform);
		ControllerBatteryMeshComponent->SetupAttachment(ControllerMeshComponent);
		ControllerBatteryMeshComponent->RegisterComponent();
	}

	// Now that everything is created, set the visibility based on the active status.
	// Set bAreSubComponentsEnabled to prevent SetSubComponentsEnabled from returning early since
	// The components have only just been created and may not be set properly yet.
	SetSubComponentsEnabled(true);
}

void UGoogleVRMotionControllerComponent::BeginPlay()
{
	Super::BeginPlay();

	TArray<UGoogleVRPointerInputComponent*> Components;
	UGoogleVRPointerInputComponent* InputComponent = nullptr;

	GetOwner()->GetComponents(Components);
	if(Components.Num() == 0)
	{
		if (RequireInputComponent)
		{
			UE_LOG(LogGoogleVRMotionController, Warning, TEXT("GoogleVRMotionControllerComponent has RequireInputComponent set to true, but the actor does not have a GoogleVRPointerInputComponent. Creating GoogleVRPointerInputComponent."));

			InputComponent = NewObject<UGoogleVRPointerInputComponent>(GetOwner(), "GoogleVRPointerInputComponent");
			InputComponent->RegisterComponent();
		}
	}
	else
	{
		InputComponent = Components[0];
	}

	TArray<UActorComponent*> LaserVisualComponents = GetOwner()->GetComponentsByTag(UGoogleVRLaserVisual::StaticClass(), LaserVisualComponentTag);
	if (LaserVisualComponents.Num() > 0)
	{
		LaserVisualComponent = (UGoogleVRLaserVisual*)LaserVisualComponents[0];
		LaserVisualComponent->AttachToComponent(MotionControllerComponent, FAttachmentTransformRules::KeepWorldTransform);
	}
	else
	{
		UE_LOG(LogGoogleVRMotionController, Warning, TEXT("GoogleVRMotionControllerComponent: the actor does not have a GoogleVRLaserVisualComponent."));
	}

	// If we found an InputComponent and it doesn't already have a Pointer, automatically set
	// it to this. If you want to switch between multiple pointers, or have multiple InputComponents,
	// then set the Pointers manually.
	if (InputComponent != nullptr && InputComponent->GetPointer() == nullptr)
	{
		InputComponent->SetPointer(this);
	}

	// Get the Playercontroller to use for input events.
	PlayerController = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	check(PlayerController != nullptr);

	// Set the laser and reticle distances to their default positions.
	if (LaserVisualComponent != nullptr)
	{
		LaserVisualComponent->SetSubComponentsEnabled(bAreSubComponentsEnabled);

		// Get the world to meters scale.
		const float WorldToMetersScale = GetWorldToMetersScale();

		LaserVisualComponent->SetDefaultLaserDistance(WorldToMetersScale);
		LaserVisualComponent->SetDefaultReticleDistance(WorldToMetersScale, PlayerController->PlayerCameraManager->GetCameraLocation());
	}

	UGoogleVRControllerFunctionLibrary::SetArmModelIsLockedToHead(IsLockedToHead);
}

void UGoogleVRMotionControllerComponent::Activate(bool bReset)
{
	Super::Activate(bReset);

	SetSubComponentsEnabled(IsPointerActive());
}

void UGoogleVRMotionControllerComponent::Deactivate()
{
	Super::Deactivate();

	SetSubComponentsEnabled(IsPointerActive());
}

void UGoogleVRMotionControllerComponent::TickComponent( float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction )
{
	Super::TickComponent( DeltaTime, TickType, ThisTickFunction );

	SetSubComponentsEnabled(IsPointerActive());

	if (!bAreSubComponentsEnabled)
	{
		return;
	}

	const float WorldToMetersScale = GetWorldToMetersScale();

	// Update the battery level indicator.
	UpdateBatteryIndicator();

	if (PlayerController->IsInputKeyDown(FGamepadKeyNames::MotionController_Left_Thumbstick)
		|| PlayerController->IsInputKeyDown(FGamepadKeyNames::MotionController_Right_Thumbstick))
	{
		TrySetControllerMaterial(TouchpadMaterial);
		if (ControllerTouchPointMeshComponent != nullptr)
		{
			ControllerTouchPointMeshComponent->SetVisibility(false);
		}
	}
	else
	{
		if (PlayerController->IsInputKeyDown(FGamepadKeyNames::MotionController_Left_Shoulder)
			|| PlayerController->IsInputKeyDown(FGamepadKeyNames::MotionController_Right_Shoulder))
		{
			TrySetControllerMaterial(AppMaterial);
		}
		else if (PlayerController->IsInputKeyDown(FGamepadKeyNames::SpecialLeft)
				 || PlayerController->IsInputKeyDown(FGamepadKeyNames::SpecialRight))
		{
			TrySetControllerMaterial(SystemMaterial);
		}
		else
		{
			TrySetControllerMaterial(IdleMaterial);
		}

		// Update the touch point's scale and position
		if (ControllerTouchPointMeshComponent != nullptr)
		{
			ControllerTouchPointMeshComponent->SetVisibility(true);

			if (PlayerController->IsInputKeyDown(EKeys::Steam_Touch_0))
			{
				ControllerTouchPointMeshComponent->SetRelativeScale3D(ControllerTouchPointMeshComponent->RelativeScale3D * TOUCHPAD_POINT_FILTER_STRENGTH +
																	  TouchMeshScale * (1.0f - TOUCHPAD_POINT_FILTER_STRENGTH));

				float TouchPadX = PlayerController->GetInputAnalogKeyState(FGamepadKeyNames::MotionController_Left_Thumbstick_X);
				float TouchPadY = PlayerController->GetInputAnalogKeyState(FGamepadKeyNames::MotionController_Left_Thumbstick_Y);
				float X = TouchPadX * TOUCHPAD_RADIUS * WorldToMetersScale;
				float Y = TouchPadY * TOUCHPAD_RADIUS * WorldToMetersScale;

				FVector TouchPointRelativeLocation = FVector(TOUCHPAD_POINT_X_OFFSET * WorldToMetersScale  - Y, X, TOUCHPAD_POINT_ELEVATION * WorldToMetersScale);
				ControllerTouchPointMeshComponent->SetRelativeLocation(TouchPointRelativeLocation);
			}
			else
			{
				ControllerTouchPointMeshComponent->SetRelativeScale3D(ControllerTouchPointMeshComponent->RelativeScale3D * TOUCHPAD_POINT_FILTER_STRENGTH);
			}
		}
	}

	// Update the position of the pointer.
	FVector PointerPositionOffset = UGoogleVRControllerFunctionLibrary::GetArmModelPointerPositionOffset();
	float PointerTiltAngle = UGoogleVRControllerFunctionLibrary::GetArmModelPointerTiltAngle();

	if (LaserVisualComponent != nullptr)
	{
		LaserVisualComponent->SetRelativeLocation(PointerPositionOffset);
		LaserVisualComponent->SetRelativeRotation(FRotator(-PointerTiltAngle, 0.0f, 0.0f));
	}

	// Adjust Transparency
	if (ParameterCollection != nullptr)
	{
		float AlphaValue = UGoogleVRControllerFunctionLibrary::GetControllerAlphaValue();
		UMaterialParameterCollectionInstance* ParameterCollectionInstance = GetWorld()->GetParameterCollectionInstance(ParameterCollection);
		const bool bFoundParameter = ParameterCollectionInstance->SetScalarParameterValue("GoogleVRMotionControllerAlpha", AlphaValue);
		if (!bFoundParameter)
		{
			UE_LOG(LogGoogleVRMotionController, Warning, TEXT("Unable to find GoogleVRMotionControllerAlpha parameter in Material Collection."));
		}
	}
}

void UGoogleVRMotionControllerComponent::TrySetControllerMaterial(UMaterialInterface* NewMaterial)
{
	if (NewMaterial != nullptr)
	{
		ControllerMeshComponent->SetMaterial(0, NewMaterial);
	}
	else
	{
		ControllerMeshComponent->SetMaterial(0, IdleMaterial);
	}
}

void UGoogleVRMotionControllerComponent::UpdateBatteryIndicator()
{
	UTexture2D* NewTexture = nullptr;

	// Charging overrides other state options.
	if (UGoogleVRControllerFunctionLibrary::GetBatteryCharging())
	{
		if (!bBatteryWasCharging)
		{
			NewTexture = BatteryChargingTexture;
			bBatteryWasCharging = true;
		}
	}
	else
	{
		EGoogleVRControllerBatteryLevel BatteryLevel = UGoogleVRControllerFunctionLibrary::GetBatteryLevel();

		if (BatteryLevel != LastKnownBatteryState || bBatteryWasCharging)
		{
			switch (BatteryLevel)
			{
				case EGoogleVRControllerBatteryLevel::CriticalLow:
					NewTexture = BatteryCriticalLowTexture;
					break;

				case EGoogleVRControllerBatteryLevel::Low:
					NewTexture = BatteryLowTexture;
					break;

				case EGoogleVRControllerBatteryLevel::Medium:
					NewTexture = BatteryMediumTexture;
					break;

				case EGoogleVRControllerBatteryLevel::AlmostFull:
					NewTexture = BatteryAlmostFullTexture;
					break;

				case EGoogleVRControllerBatteryLevel::Full:
					NewTexture = BatteryFullTexture;
					break;

				default:
					NewTexture = BatteryUnknownTexture;
					break;
			}

			LastKnownBatteryState = BatteryLevel;
			bBatteryWasCharging = false;
		}
	}

	if (NewTexture != nullptr && ControllerBatteryMaterial != nullptr)
	{
		ControllerBatteryMaterial->SetTextureParameterValue(BatteryTextureParameterName, NewTexture);
	}
}

void UGoogleVRMotionControllerComponent::SetSubComponentsEnabled(bool bNewEnabled)
{
	if (bNewEnabled == bAreSubComponentsEnabled)
	{
		return;
	}

	bAreSubComponentsEnabled = bNewEnabled;

	// Explicitly set the visibility of each elements instead of propagating recursively.
	// If we do it recursively, then we might change the visisiblity of something unintentionally.
	// I.E. An Object that is being "grabbed" with the controller.

	if (MotionControllerComponent != nullptr)
	{
		MotionControllerComponent->SetActive(bNewEnabled);
		MotionControllerComponent->SetVisibility(bNewEnabled);
	}

	if (ControllerMeshComponent != nullptr)
	{
		ControllerMeshComponent->SetActive(bNewEnabled);
		ControllerMeshComponent->SetVisibility(bNewEnabled);
	}

	if (ControllerTouchPointMeshComponent != nullptr)
	{
		ControllerTouchPointMeshComponent->SetActive(bNewEnabled);
		ControllerTouchPointMeshComponent->SetVisibility(bNewEnabled);
	}

	if (ControllerBatteryMeshComponent != nullptr)
	{
		ControllerBatteryMeshComponent->SetActive(bNewEnabled);
		ControllerBatteryMeshComponent->SetVisibility(bNewEnabled);
	}

	if (LaserVisualComponent != nullptr)
	{
		LaserVisualComponent->SetSubComponentsEnabled(bNewEnabled);
	}
}

bool UGoogleVRMotionControllerComponent::IsControllerConnected() const
{
	return UGoogleVRControllerFunctionLibrary::GetGoogleVRControllerState() == EGoogleVRControllerState::Connected;
}

float UGoogleVRMotionControllerComponent::GetWorldToMetersScale() const
{
	if (GWorld != nullptr)
	{
		return GWorld->GetWorldSettings()->WorldToMeters;
	}

	// Default value, assume Unreal units are in centimeters
	return 100.0f;
}

void UGoogleVRMotionControllerComponent::OnPointerEnter(const FHitResult& HitResult, bool IsHitInteractive)
{
	OnPointerHover(HitResult, IsHitInteractive);
}

void UGoogleVRMotionControllerComponent::OnPointerHover(const FHitResult& HitResult, bool IsHitInteractive)
{
	if (LaserVisualComponent != nullptr)
	{
		FVector Location = HitResult.Location;
		FVector OriginLocation = HitResult.TraceStart;
		LaserVisualComponent->UpdateReticleLocation(Location, OriginLocation, GetWorldToMetersScale(), PlayerController->PlayerCameraManager->GetCameraLocation());

		FTransform PointerContainerTransform = LaserVisualComponent->GetComponentTransform();
		FVector Difference = Location - PointerContainerTransform.GetLocation();
		float Distance = Difference.Size();
		LaserVisualComponent->UpdateLaserDistance(Distance);

		FVector UncorrectedLaserEndpoint = PointerContainerTransform.GetLocation() + PointerContainerTransform.GetUnitAxis(EAxis::X) * Distance;
		LaserVisualComponent->UpdateLaserCorrection(Location - UncorrectedLaserEndpoint);
	}
}

void UGoogleVRMotionControllerComponent::OnPointerExit(const FHitResult& HitResult)
{
	if (LaserVisualComponent != nullptr)
	{
		const float WorldToMetersScale = GetWorldToMetersScale();
		LaserVisualComponent->SetDefaultLaserDistance(WorldToMetersScale);
		LaserVisualComponent->SetDefaultReticleDistance(WorldToMetersScale, PlayerController->PlayerCameraManager->GetCameraLocation());
		LaserVisualComponent->UpdateLaserCorrection(FVector(0, 0, 0));
	}
}

FVector UGoogleVRMotionControllerComponent::GetOrigin() const
{
	if (LaserVisualComponent != nullptr)
	{
		return LaserVisualComponent->GetComponentLocation();
	}

	return FVector::ZeroVector;
}

FVector UGoogleVRMotionControllerComponent::GetDirection() const
{
	if (LaserVisualComponent != nullptr)
	{
		return LaserVisualComponent->GetForwardVector();
	}

	return FVector::ZeroVector;
}

void UGoogleVRMotionControllerComponent::GetRadius(float& OutEnterRadius, float& OutExitRadius) const
{
	if (LaserVisualComponent != nullptr)
	{
		FMaterialSpriteElement& Sprite = *(LaserVisualComponent->GetReticleSprite());
		float SpriteSize = Sprite.BaseSizeX;

		// Fixed size for enter radius to avoid flickering.
		// TODO: This will cause some slight variability based on the distance of the object from the camera,
		// and is optimized for the average case. For this to be fixed, the hit test must be done via a cone instead
		// of the spherecast that is currently used.
		const float WorldToMetersScale = GetWorldToMetersScale();
		OutEnterRadius = LaserVisualComponent->GetReticleSize() * WorldToMetersScale * EnterRadiusCoeff;

		// Dynamic size for exit radius.
		// Will always be correct because we know the intersection point of the object and are therefore using
		// the correct radius based on the object's distance from the camera.
		OutExitRadius = SpriteSize * ExitRadiusCoeff;
	}
	else
	{
		OutEnterRadius = 0.0f;
		OutExitRadius = 0.0f;
	}
}

float UGoogleVRMotionControllerComponent::GetMaxPointerDistance() const
{
	float WorldToMetersScale = GetWorldToMetersScale();
	if (LaserVisualComponent != nullptr)
	{
		return LaserVisualComponent->GetMaxPointerDistance(WorldToMetersScale);
	}
	return 2.5f * WorldToMetersScale;
}

bool UGoogleVRMotionControllerComponent::IsPointerActive() const
{
	return IsActive() && IsControllerConnected();
}
