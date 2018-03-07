// Copyright 2017 Google Inc.


#include "GoogleVRPointerInputComponent.h"
#include "GoogleVRController.h"
#include "GoogleVRActorPointerResponder.h"
#include "GoogleVRComponentPointerResponder.h"
#include "GoogleVRWidgetInteractionComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogGoogleVRPointerInput, Log, All);

UGoogleVRPointerInputComponent::UGoogleVRPointerInputComponent(const FObjectInitializer& ObjectInitializer)
: PointerInputMode(EGoogleVRPointerInputMode::Camera)
, FarClippingDistance(1000.0f)
, NearClippingDistance(30.0f)
, UseControllerClick(true)
, UseTouchClick(false)
, WidgetInteraction(nullptr)
, Pointer(nullptr)
, LatestHitResult(ForceInit)
, PendingClickActor(nullptr)
, PendingClickComponent(nullptr)
{
	PrimaryComponentTick.bCanEverTick = true;

	WidgetInteraction = ObjectInitializer.CreateDefaultSubobject<UGoogleVRWidgetInteractionComponent>(this, TEXT("GoogleVRWidgetInteraction"));
}

void UGoogleVRPointerInputComponent::SetPointer(TScriptInterface<IGoogleVRPointer> NewPointer)
{
	Pointer = NewPointer;
}

TScriptInterface<IGoogleVRPointer> UGoogleVRPointerInputComponent::GetPointer() const
{
	return Pointer;
}

bool UGoogleVRPointerInputComponent::IsBlockingHit() const
{
	return LatestHitResult.IsValidBlockingHit();
}

AActor* UGoogleVRPointerInputComponent::GetHitActor() const
{
	return LatestHitResult.GetActor();
}

UPrimitiveComponent* UGoogleVRPointerInputComponent::GetHitComponent() const
{
	return LatestHitResult.GetComponent();
}

FVector UGoogleVRPointerInputComponent::GetIntersectionLocation() const
{
	if (IsBlockingHit())
	{
		return LatestHitResult.Location;
	}

	return FVector::ZeroVector;
}

FHitResult UGoogleVRPointerInputComponent::GetLatestHitResult() const
{
	return LatestHitResult;
}

void UGoogleVRPointerInputComponent::OnRegister()
{
	Super::OnRegister();
}

void UGoogleVRPointerInputComponent::BeginPlay()
{
	Super::BeginPlay();

	APlayerController* PlayerController = UGameplayStatics::GetPlayerController(GetOwner(), 0);

	if (UseControllerClick)
	{
		// Bind Controller click events.
		PlayerController->InputComponent->BindKey(FGamepadKeyNames::MotionController_Right_Thumbstick, EInputEvent::IE_Pressed, this, &UGoogleVRPointerInputComponent::ClickButtonPressed);
		PlayerController->InputComponent->BindKey(FGamepadKeyNames::MotionController_Right_Thumbstick, EInputEvent::IE_Released, this, &UGoogleVRPointerInputComponent::ClickButtonReleased);
	}

	if (UseTouchClick)
	{
		// Bind touch events.
		FInputTouchBinding& TouchStartedBinding = PlayerController->InputComponent->BindTouch(EInputEvent::IE_Pressed, this, &UGoogleVRPointerInputComponent::TouchPressed);
		TouchStartedBinding.bConsumeInput = false;
		FInputTouchBinding& TouchStoppedBinding = PlayerController->InputComponent->BindTouch(EInputEvent::IE_Released, this, &UGoogleVRPointerInputComponent::TouchReleased);
		TouchStoppedBinding.bConsumeInput = false;

		// Bind Mouse events for the editor so that we do not need to rely on "Use Mouse for Touch" enabled which is not working when the mouse moves outside the window.
		PlayerController->InputComponent->BindAction("EditorTouchTrigger", EInputEvent::IE_Pressed, this, &UGoogleVRPointerInputComponent::ClickButtonPressed);
		PlayerController->InputComponent->BindAction("EditorTouchTrigger", EInputEvent::IE_Released, this, &UGoogleVRPointerInputComponent::ClickButtonReleased);
	}
}

void UGoogleVRPointerInputComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Since it isn't possible for us to store the Pointer as a weak reference,
	// we explicitly check to see if it is marked to be killed and null it out.
	if (Pointer != nullptr && Pointer.GetObject()->IsPendingKill())
	{
		Pointer = nullptr;
	}

	if (Pointer == nullptr)
	{
		UE_LOG(LogGoogleVRPointerInput, Warning, TEXT("Pointer must be set for GoogleVRPointerInputComponent to function"));
		return;
	}

	AActor* PreviousHitActor = GetHitActor();
	UPrimitiveComponent* PreviousHitComponent = GetHitComponent();

	if (Pointer->IsPointerActive())
	{
		// Determine the pointer start and end locations.
		FVector PointerStart, PointerEnd;
		GetPointerStartAndEnd(PointerStart, PointerEnd);

		// Determine what the pointer is currently hitting
		LatestHitResult = PerformHitDetection(PointerStart, PointerEnd);
	}
	else
	{
		LatestHitResult.Reset();
	}

	if (WidgetInteraction != nullptr)
	{
		WidgetInteraction->UpdateState(LatestHitResult);
	}

	AActor* HitActor = GetHitActor();
	UPrimitiveComponent* HitComponent = GetHitComponent();

	bool bActorChanged = HitActor != PreviousHitActor;
	bool bComponentChanged = HitComponent != PreviousHitComponent;
	bool bIsHitInteractive =  HitActor != nullptr && HitActor->Implements<UGoogleVRActorPointerResponder>();
	bIsHitInteractive = bIsHitInteractive || (HitComponent != nullptr && HitComponent->Implements<UGoogleVRComponentPointerResponder>());

	if (WidgetInteraction != nullptr)
	{
		bIsHitInteractive = bIsHitInteractive || WidgetInteraction->IsOverInteractableWidget();
	}

	// Comoponent Exit
	if (bComponentChanged && PreviousHitComponent != nullptr)
	{
		OnPointerExitComponentEvent.Broadcast(PreviousHitComponent, LatestHitResult);

		if (PreviousHitComponent->Implements<UGoogleVRComponentPointerResponder>())
		{
			IGoogleVRComponentPointerResponder::Execute_OnPointerExit(PreviousHitComponent, PreviousHitComponent, LatestHitResult, this);
		}
	}

	// Actor Exit
	if (bActorChanged && PreviousHitActor != nullptr)
	{
		OnPointerExitActorEvent.Broadcast(PreviousHitActor, LatestHitResult);

		if (PreviousHitActor->Implements<UGoogleVRActorPointerResponder>())
		{
			IGoogleVRActorPointerResponder::Execute_OnPointerExit(PreviousHitActor, PreviousHitActor, LatestHitResult, this);
		}

		Pointer->OnPointerExit(LatestHitResult);
	}

	// Actor Enter
	if (bActorChanged && HitActor != nullptr)
	{
		OnPointerEnterActorEvent.Broadcast(LatestHitResult);

		if (HitActor->Implements<UGoogleVRActorPointerResponder>())
		{
			IGoogleVRActorPointerResponder::Execute_OnPointerEnter(HitActor, LatestHitResult, this);
		}

		Pointer->OnPointerEnter(LatestHitResult, bIsHitInteractive);
	}

	// Component Enter
	if (bComponentChanged && HitComponent != nullptr)
	{
		OnPointerEnterComponentEvent.Broadcast(LatestHitResult);

		if (HitComponent->Implements<UGoogleVRComponentPointerResponder>())
		{
			IGoogleVRComponentPointerResponder::Execute_OnPointerEnter(HitComponent, LatestHitResult, this);
		}
	}

	// Component Changed
	if (!bActorChanged && bComponentChanged && HitActor != nullptr && HitActor->Implements<UGoogleVRActorPointerResponder>())
	{
		IGoogleVRActorPointerResponder::Execute_OnPointerComponentChanged(PreviousHitActor, PreviousHitComponent, LatestHitResult, this);
	}

	// Actor Hover
	if (!bActorChanged && HitActor != nullptr)
	{
		OnPointerHoverActorEvent.Broadcast(LatestHitResult);

		if (HitActor->Implements<UGoogleVRActorPointerResponder>())
		{
			IGoogleVRActorPointerResponder::Execute_OnPointerHover(HitActor, LatestHitResult, this);
		}

		Pointer->OnPointerHover(LatestHitResult, bIsHitInteractive);
	}

	// Component Hover
	if (!bComponentChanged && HitComponent != nullptr)
	{
		OnPointerHoverComponentEvent.Broadcast(LatestHitResult);

		if (HitComponent->Implements<UGoogleVRComponentPointerResponder>())
		{
			IGoogleVRComponentPointerResponder::Execute_OnPointerHover(HitComponent, LatestHitResult, this);
		}
	}

	PostHitDetection();
}

FHitResult UGoogleVRPointerInputComponent::PerformHitDetection(FVector PointerStart, FVector PointerEnd)
{
	FHitResult HitResult = FHitResult(ForceInit);
	FCollisionObjectQueryParams ObjectParams(FCollisionObjectQueryParams::AllObjects);
	FCollisionQueryParams Params = FCollisionQueryParams::DefaultQueryParam;
	Params.AddIgnoredActor(GetOwner());

	float EnterRadius, ExitRadius;
	Pointer->GetRadius(EnterRadius, ExitRadius);
	GetWorld()->SweepSingleByObjectType(HitResult, PointerStart, PointerEnd, FQuat(), ObjectParams, FCollisionShape::MakeSphere(EnterRadius), Params);

	// If we were already pointing at an object we must check that object against the exit radius
	// to make sure we are no longer pointing at it to prevent flicker.
	if (LatestHitResult.GetComponent() != nullptr && HitResult.GetComponent() != LatestHitResult.GetComponent())
	{
		FHitResult ExitHitResult = FHitResult(ForceInit);
		GetWorld()->SweepSingleByObjectType(ExitHitResult, PointerStart, PointerEnd, FQuat(), ObjectParams, FCollisionShape::MakeSphere(ExitRadius), Params);

		if (ExitHitResult.GetComponent() == LatestHitResult.GetComponent())
		{
			HitResult = ExitHitResult;
		}
	}

	return HitResult;
}

void UGoogleVRPointerInputComponent::PostHitDetection()
{
	// Override me.
}

void UGoogleVRPointerInputComponent::GetPointerStartAndEnd(FVector& OutPointerStart, FVector& OutPointerEnd) const
{
	switch (PointerInputMode)
	{
		case EGoogleVRPointerInputMode::Camera:
		{
			FVector RealPointerStart = Pointer->GetOrigin();
			FVector RealPointerEnd = RealPointerStart + (Pointer->GetDirection() * Pointer->GetMaxPointerDistance());

			FVector CameraLocation = UGameplayStatics::GetPlayerCameraManager(GetWorld(), 0)->GetCameraLocation();
			FVector Direction = RealPointerEnd - CameraLocation;
			Direction.Normalize();

			OutPointerStart = CameraLocation + (Direction * NearClippingDistance);
			OutPointerEnd = CameraLocation + (Direction * FarClippingDistance);

			break;
		}
		case EGoogleVRPointerInputMode::Direct:
		{
			OutPointerStart = Pointer->GetOrigin();
			OutPointerEnd = OutPointerStart + (Pointer->GetDirection() * FarClippingDistance);
			break;
		}
		default:
		check(false);
	}
}

void UGoogleVRPointerInputComponent::ClickButtonPressed()
{
	if (WidgetInteraction != nullptr)
	{
		WidgetInteraction->PressPointerKey(EKeys::LeftMouseButton);
	}

	OnPointerPressedEvent.Broadcast(LatestHitResult);

	PendingClickActor = GetHitActor();
	PendingClickComponent = GetHitComponent();

	AActor* HitActor = GetHitActor();
	UPrimitiveComponent* HitComponent = GetHitComponent();

	if (HitActor != nullptr && HitActor->Implements<UGoogleVRActorPointerResponder>())
	{
		IGoogleVRActorPointerResponder::Execute_OnPointerPressed(HitActor, LatestHitResult, this);
	}

	if (HitComponent != nullptr && HitComponent->Implements<UGoogleVRComponentPointerResponder>())
	{
		IGoogleVRComponentPointerResponder::Execute_OnPointerPressed(HitComponent, LatestHitResult, this);
	}

	PendingClickActor = HitActor;
	PendingClickComponent = HitComponent;
}

void UGoogleVRPointerInputComponent::ClickButtonReleased()
{
	if (WidgetInteraction != nullptr)
	{
		WidgetInteraction->ReleasePointerKey(EKeys::LeftMouseButton);
	}

	OnPointerReleasedEvent.Broadcast(LatestHitResult);

	AActor* HitActor = GetHitActor();
	UPrimitiveComponent* HitComponent = GetHitComponent();

	if (PendingClickActor != nullptr)
	{
		if (PendingClickActor->Implements<UGoogleVRActorPointerResponder>())
		{
			IGoogleVRActorPointerResponder::Execute_OnPointerReleased(PendingClickActor, LatestHitResult, this);
		}

		if (PendingClickActor == HitActor)
		{
			OnPointerClickActorEvent.Broadcast(LatestHitResult);

			if (HitActor != nullptr && HitActor->Implements<UGoogleVRActorPointerResponder>())
			{
				IGoogleVRActorPointerResponder::Execute_OnPointerClick(HitActor, LatestHitResult, this);
			}
		}
	}

	if (PendingClickComponent != nullptr)
	{
		if (PendingClickComponent->Implements<UGoogleVRComponentPointerResponder>())
		{
			IGoogleVRComponentPointerResponder::Execute_OnPointerReleased(PendingClickComponent, LatestHitResult, this);
		}

		if (PendingClickComponent == HitComponent)
		{
			OnPointerClickComponentEvent.Broadcast(LatestHitResult);

			if (HitComponent != nullptr && HitComponent->Implements<UGoogleVRComponentPointerResponder>())
			{
				IGoogleVRComponentPointerResponder::Execute_OnPointerClick(HitComponent, LatestHitResult, this);
			}
		}
	}

	PendingClickActor = nullptr;
	PendingClickComponent = nullptr;
}

void UGoogleVRPointerInputComponent::TouchPressed(ETouchIndex::Type FingerIndex, FVector Location)
{
	ClickButtonPressed();
}

void UGoogleVRPointerInputComponent::TouchReleased(ETouchIndex::Type FingerIndex, FVector Location)
{
	ClickButtonReleased();
}
