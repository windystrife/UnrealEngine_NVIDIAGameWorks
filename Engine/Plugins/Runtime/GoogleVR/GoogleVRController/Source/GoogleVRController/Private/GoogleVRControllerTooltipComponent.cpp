// Copyright 2017 Google Inc.

#include "Classes/GoogleVRControllerTooltipComponent.h"
#include "GoogleVRController.h"
#include "Classes/GoogleVRControllerFunctionLibrary.h"
#include "MotionControllerComponent.h"
#include "Materials/MaterialParameterCollectionInstance.h"
#include "GameFramework/WorldSettings.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"

DEFINE_LOG_CATEGORY_STATIC(LogGoogleVRControllerTooltip, Log, All);

UGoogleVRControllerTooltipComponent::UGoogleVRControllerTooltipComponent(const FObjectInitializer& ObjectInitializer)
: TooltipLocation(EGoogleVRControllerTooltipLocation::TouchPadOutside)
, TextRenderComponent(nullptr)
, ParameterCollection(nullptr)
, MotionController(nullptr)
, IsOnLeft(false)
{
	PrimaryComponentTick.bCanEverTick = true;

	// Create Text Render Component with default properties.
	TextRenderComponent = ObjectInitializer.CreateDefaultSubobject<UTextRenderComponent>(this, TEXT("TooltipText"));
	TextRenderComponent->SetTextMaterial(Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), NULL, TEXT("/GoogleVRController/TooltipTextMaterial"))));
	const float WorldToMetersScale = GetWorldToMetersScale();
	TextRenderComponent->WorldSize = 0.02f * WorldToMetersScale;
	TextRenderComponent->SetVerticalAlignment(EVerticalTextAligment::EVRTA_TextCenter);

	// Load default parameter collection.
	ParameterCollection = Cast<UMaterialParameterCollection>(StaticLoadObject(UMaterialParameterCollection::StaticClass(), NULL, TEXT("/GoogleVRController/ControllerParameters")));
}

void UGoogleVRControllerTooltipComponent::BeginPlay()
{
	Super::BeginPlay();

	// Attach the Text Render Component.
	if (TextRenderComponent != nullptr)
	{
		TextRenderComponent->AttachToComponent(this, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		FQuat Rotation = FQuat::MakeFromEuler(FVector(180.0f, 90.0f, 0.0f));
		TextRenderComponent->SetRelativeRotation(Rotation.Rotator());
	}

	// Attempt to attach this tooltip to the motion controller and set it's position.
	TArray<UMotionControllerComponent*> Components;
	GetOwner()->GetComponents(Components);
	if(Components.Num() == 0)
	{
		UE_LOG(LogGoogleVRControllerTooltip, Warning, TEXT("Cannot find MotionControllerComponent."));
	}
	else
	{
		MotionController = Components[0];
		AttachToComponent(MotionController, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		IsOnLeft = IsTooltipOnLeft();
		RefreshTooltipLocation();
	}
}

void UGoogleVRControllerTooltipComponent::TickComponent( float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction )
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (TextRenderComponent != nullptr && MotionController != nullptr)
	{
		TextRenderComponent->SetVisibility(MotionController->IsActive());
	}

	// If handedness changes, the tooltip will switch sides.
	bool NewIsOnLeft = IsTooltipOnLeft();
	if (NewIsOnLeft != IsOnLeft)
	{
		IsOnLeft = NewIsOnLeft;
		RefreshTooltipLocation();
	}

	// Adjust Transparency
	if (ParameterCollection != nullptr)
	{
		float AlphaValue = UGoogleVRControllerFunctionLibrary::GetTooltipAlphaValue();
		UMaterialParameterCollectionInstance* ParameterCollectionInstance = GetWorld()->GetParameterCollectionInstance(ParameterCollection);
		const bool bFoundParameter = ParameterCollectionInstance->SetScalarParameterValue("GoogleVRControllerTooltipAlpha", AlphaValue);
		if (!bFoundParameter)
		{
			UE_LOG(LogGoogleVRControllerTooltip, Warning, TEXT("Unable to find GoogleVRControllerTooltipAlpha parameter in Material Collection."));
		}
	}
}

void UGoogleVRControllerTooltipComponent::OnSideChanged(bool OnLeftSide)
{
	if (TextRenderComponent != nullptr)
	{
		if (OnLeftSide)
		{
			TextRenderComponent->SetHorizontalAlignment(EHorizTextAligment::EHTA_Right);
		}
		else
		{
			TextRenderComponent->SetHorizontalAlignment(EHorizTextAligment::EHTA_Left);
		}
	}
}

void UGoogleVRControllerTooltipComponent::RefreshTooltipLocation()
{
	FVector NewRelativeLocation = GetRelativeLocation();

	if (IsOnLeft)
	{
		NewRelativeLocation.Y *= -1.0f;
	}

	SetRelativeLocation(NewRelativeLocation);

	OnSideChanged(IsOnLeft);
	ReceiveOnSideChanged(IsOnLeft);
}

FVector UGoogleVRControllerTooltipComponent::GetRelativeLocation() const
{
	const float WorldToMetersScale = GetWorldToMetersScale();

	switch (TooltipLocation)
	{
		case EGoogleVRControllerTooltipLocation::TouchPadOutside:
		case EGoogleVRControllerTooltipLocation::TouchPadInside:
			return FVector(0.09f, 0.02f, 0.0f) * WorldToMetersScale;
		case EGoogleVRControllerTooltipLocation::AppButtonOutside:
		case EGoogleVRControllerTooltipLocation::AppButtonInside:
			return FVector(0.06f, 0.02f, 0.0f) * WorldToMetersScale;
		default:
			return FVector::ZeroVector;
	}
}

bool UGoogleVRControllerTooltipComponent::IsTooltipInside() const
{
	switch (TooltipLocation)
	{
		case EGoogleVRControllerTooltipLocation::TouchPadInside:
		case EGoogleVRControllerTooltipLocation::AppButtonInside:
			return true;
		case EGoogleVRControllerTooltipLocation::TouchPadOutside:
		case EGoogleVRControllerTooltipLocation::AppButtonOutside:
		default:
			return false;
	}
}

bool UGoogleVRControllerTooltipComponent::IsTooltipOnLeft() const
{
	bool IsInside = IsTooltipInside();

	EGoogleVRControllerHandedness Handedness = UGoogleVRControllerFunctionLibrary::GetGoogleVRControllerHandedness();

	if (Handedness == EGoogleVRControllerHandedness::LeftHanded)
	{
		return !IsInside;
	}
	else
	{
		return IsInside;
	}
}

float UGoogleVRControllerTooltipComponent::GetWorldToMetersScale() const
{
	if (GWorld != nullptr)
	{
		return GWorld->GetWorldSettings()->WorldToMeters;
	}

	// Default value, assume Unreal units are in centimeters
	return 100.0f;
}
