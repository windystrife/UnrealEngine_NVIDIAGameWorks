// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	HeightFogComponent.cpp: Height fog implementation.
=============================================================================*/

#include "CoreMinimal.h"
#include "UObject/CoreNet.h"
#include "UObject/ConstructorHelpers.h"
#include "EngineDefines.h"
#include "Engine/World.h"
#include "SceneInterface.h"
#include "Engine/Texture2D.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Engine/ExponentialHeightFog.h"
#include "Net/UnrealNetwork.h"
#include "Components/BillboardComponent.h"

UExponentialHeightFogComponent::UExponentialHeightFogComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	FogInscatteringColor = FLinearColor(0.447f, 0.638f, 1.0f);

	DirectionalInscatteringExponent = 4.0f;
	DirectionalInscatteringStartDistance = 10000.0f;
	DirectionalInscatteringColor = FLinearColor(0.25f, 0.25f, 0.125f);

	InscatteringTextureTint = FLinearColor::White;
	FullyDirectionalInscatteringColorDistance = 100000.0f;
	NonDirectionalInscatteringColorDistance = 1000.0f;

	FogDensity = 0.02f;
	FogHeightFalloff = 0.2f;
	FogMaxOpacity = 1.0f;
	StartDistance = 0.0f;

	// disabled by default
	FogCutoffDistance = 0;

	VolumetricFogScatteringDistribution = .2f;
	VolumetricFogAlbedo = FColor::White;
	VolumetricFogExtinctionScale = 1.0f;
	VolumetricFogDistance = 6000.0f;
	VolumetricFogStaticLightingScatteringIntensity = 1;
}

void UExponentialHeightFogComponent::AddFogIfNeeded()
{
	if (ShouldComponentAddToScene() && ShouldRender() && IsRegistered() && FogDensity * 1000 > DELTA && FogMaxOpacity > DELTA
		&& (GetOuter() == NULL || !GetOuter()->HasAnyFlags(RF_ClassDefaultObject)))
	{
		GetWorld()->Scene->AddExponentialHeightFog(this);
	}
}

void UExponentialHeightFogComponent::CreateRenderState_Concurrent()
{
	Super::CreateRenderState_Concurrent();
	AddFogIfNeeded();
}

void UExponentialHeightFogComponent::SendRenderTransform_Concurrent()
{
	GetWorld()->Scene->RemoveExponentialHeightFog(this);
	AddFogIfNeeded();
	Super::SendRenderTransform_Concurrent();
}

void UExponentialHeightFogComponent::DestroyRenderState_Concurrent()
{
	Super::DestroyRenderState_Concurrent();
	GetWorld()->Scene->RemoveExponentialHeightFog(this);
}

#if WITH_EDITOR

bool UExponentialHeightFogComponent::CanEditChange(const UProperty* InProperty) const
{
	if (InProperty)
	{
		FString PropertyName = InProperty->GetName();

		if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UExponentialHeightFogComponent, DirectionalInscatteringExponent) ||
			PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UExponentialHeightFogComponent, DirectionalInscatteringStartDistance) ||
			PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UExponentialHeightFogComponent, DirectionalInscatteringColor) ||
			PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UExponentialHeightFogComponent, FogInscatteringColor))
		{
			return !InscatteringColorCubemap;
		}

		if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UExponentialHeightFogComponent, FullyDirectionalInscatteringColorDistance) ||
			PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UExponentialHeightFogComponent, NonDirectionalInscatteringColorDistance) ||
			PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UExponentialHeightFogComponent, InscatteringTextureTint) ||
			PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UExponentialHeightFogComponent, InscatteringColorCubemapAngle))
		{
			return InscatteringColorCubemap != NULL;
		}
	}

	return Super::CanEditChange(InProperty);
}

void UExponentialHeightFogComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	FogDensity = FMath::Clamp(FogDensity, 0.0f, 10.0f);
	FogHeightFalloff = FMath::Clamp(FogHeightFalloff, 0.0f, 2.0f);
	FogMaxOpacity = FMath::Clamp(FogMaxOpacity, 0.0f, 1.0f);
	StartDistance = FMath::Clamp(StartDistance, 0.0f, (float)WORLD_MAX);
	FogCutoffDistance = FMath::Clamp(FogCutoffDistance, 0.0f, (float)(10 * WORLD_MAX));
	FullyDirectionalInscatteringColorDistance = FMath::Clamp(FullyDirectionalInscatteringColorDistance, 0.0f, (float)WORLD_MAX);
	NonDirectionalInscatteringColorDistance = FMath::Clamp(NonDirectionalInscatteringColorDistance, 0.0f, FullyDirectionalInscatteringColorDistance);
	InscatteringColorCubemapAngle = FMath::Clamp(InscatteringColorCubemapAngle, 0.0f, 360.0f);

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif // WITH_EDITOR

void UExponentialHeightFogComponent::PostInterpChange(UProperty* PropertyThatChanged)
{
	Super::PostInterpChange(PropertyThatChanged);

	MarkRenderStateDirty();
}

void UExponentialHeightFogComponent::SetFogDensity(float Value)
{
	if(FogDensity != Value)
	{
		FogDensity = Value;
		MarkRenderStateDirty();
	}
}

void UExponentialHeightFogComponent::SetFogInscatteringColor(FLinearColor Value)
{
	if(FogInscatteringColor != Value)
	{
		FogInscatteringColor = Value;
		MarkRenderStateDirty();
	}
}

void UExponentialHeightFogComponent::SetInscatteringColorCubemap(UTextureCube* Value)
{
	if(InscatteringColorCubemap != Value)
	{
		InscatteringColorCubemap = Value;
		MarkRenderStateDirty();
	}
}

void UExponentialHeightFogComponent::SetInscatteringColorCubemapAngle(float Value)
{
	if(InscatteringColorCubemapAngle != Value)
	{
		InscatteringColorCubemapAngle = Value;
		MarkRenderStateDirty();
	}
}

void UExponentialHeightFogComponent::SetFullyDirectionalInscatteringColorDistance(float Value)
{
	if(FullyDirectionalInscatteringColorDistance != Value)
	{
		FullyDirectionalInscatteringColorDistance = Value;
		MarkRenderStateDirty();
	}
}

void UExponentialHeightFogComponent::SetNonDirectionalInscatteringColorDistance(float Value)
{
	if(NonDirectionalInscatteringColorDistance != Value)
	{
		NonDirectionalInscatteringColorDistance = Value;
		MarkRenderStateDirty();
	}
}

void UExponentialHeightFogComponent::SetInscatteringTextureTint(FLinearColor Value)
{
	if(InscatteringTextureTint != Value)
	{
		InscatteringTextureTint = Value;
		MarkRenderStateDirty();
	}
}

void UExponentialHeightFogComponent::SetDirectionalInscatteringExponent(float Value)
{
	if(DirectionalInscatteringExponent != Value)
	{
		DirectionalInscatteringExponent = Value;
		MarkRenderStateDirty();
	}
}

void UExponentialHeightFogComponent::SetDirectionalInscatteringStartDistance(float Value)
{
	if(DirectionalInscatteringStartDistance != Value)
	{
		DirectionalInscatteringStartDistance = Value;
		MarkRenderStateDirty();
	}
}

void UExponentialHeightFogComponent::SetDirectionalInscatteringColor(FLinearColor Value)
{
	if(DirectionalInscatteringColor != Value)
	{
		DirectionalInscatteringColor = Value;
		MarkRenderStateDirty();
	}
}

void UExponentialHeightFogComponent::SetFogHeightFalloff(float Value)
{
	if(FogHeightFalloff != Value)
	{
		FogHeightFalloff = Value;
		MarkRenderStateDirty();
	}
}

void UExponentialHeightFogComponent::SetFogMaxOpacity(float Value)
{
	if(FogMaxOpacity != Value)
	{
		FogMaxOpacity = Value;
		MarkRenderStateDirty();
	}
}

void UExponentialHeightFogComponent::SetStartDistance(float Value)
{
	if(StartDistance != Value)
	{
		StartDistance = Value;
		MarkRenderStateDirty();
	}
}

void UExponentialHeightFogComponent::SetFogCutoffDistance(float Value)
{
	if(FogCutoffDistance != Value)
	{
		FogCutoffDistance = Value;
		MarkRenderStateDirty();
	}
}

void UExponentialHeightFogComponent::SetVolumetricFog(bool bNewValue)
{
	if(bEnableVolumetricFog != bNewValue)
	{
		bEnableVolumetricFog = bNewValue;
		MarkRenderStateDirty();
	}
}

void UExponentialHeightFogComponent::SetVolumetricFogScatteringDistribution(float NewValue)
{
	if(VolumetricFogScatteringDistribution != NewValue)
	{
		VolumetricFogScatteringDistribution = NewValue;
		MarkRenderStateDirty();
	}
}

void UExponentialHeightFogComponent::SetVolumetricFogExtinctionScale(float NewValue)
{
	if (VolumetricFogExtinctionScale != NewValue)
	{
		VolumetricFogExtinctionScale = NewValue;
		MarkRenderStateDirty();
	}
}

void UExponentialHeightFogComponent::SetVolumetricFogAlbedo(FColor NewValue)
{
	if (VolumetricFogAlbedo != NewValue)
	{
		VolumetricFogAlbedo = NewValue;
		MarkRenderStateDirty();
	}
}

void UExponentialHeightFogComponent::SetVolumetricFogEmissive(FLinearColor NewValue)
{
	if (VolumetricFogEmissive != NewValue)
	{
		VolumetricFogEmissive = NewValue;
		MarkRenderStateDirty();
	}
}

void UExponentialHeightFogComponent::SetVolumetricFogDistance(float NewValue)
{
	if(VolumetricFogDistance != NewValue)
	{
		VolumetricFogDistance = NewValue;
		MarkRenderStateDirty();
	}
}

//////////////////////////////////////////////////////////////////////////
// AExponentialHeightFog

AExponentialHeightFog::AExponentialHeightFog(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Component = CreateDefaultSubobject<UExponentialHeightFogComponent>(TEXT("HeightFogComponent0"));
	RootComponent = Component;

	bHidden = false;

#if WITH_EDITORONLY_DATA
	if (!IsRunningCommandlet() && (GetSpriteComponent() != NULL))
	{
		// Structure to hold one-time initialization
		struct FConstructorStatics
		{
			ConstructorHelpers::FObjectFinderOptional<UTexture2D> FogTextureObject;
			FName ID_Fog;
			FText NAME_Fog;
			FConstructorStatics()
				: FogTextureObject(TEXT("/Engine/EditorResources/S_ExpoHeightFog"))
				, ID_Fog(TEXT("Fog"))
				, NAME_Fog(NSLOCTEXT("SpriteCategory", "Fog", "Fog"))
			{
			}
		};
		static FConstructorStatics ConstructorStatics;

		GetSpriteComponent()->Sprite = ConstructorStatics.FogTextureObject.Get();
		GetSpriteComponent()->RelativeScale3D = FVector(0.5f, 0.5f, 0.5f);
		GetSpriteComponent()->SpriteInfo.Category = ConstructorStatics.ID_Fog;
		GetSpriteComponent()->SpriteInfo.DisplayName = ConstructorStatics.NAME_Fog;
		GetSpriteComponent()->SetupAttachment(Component);
	}
#endif // WITH_EDITORONLY_DATA
}

void AExponentialHeightFog::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	bEnabled = Component->bVisible;
}

void AExponentialHeightFog::GetLifetimeReplicatedProps( TArray< FLifetimeProperty > & OutLifetimeProps ) const
{
	Super::GetLifetimeReplicatedProps( OutLifetimeProps );
	
	DOREPLIFETIME( AExponentialHeightFog, bEnabled );
}

void AExponentialHeightFog::OnRep_bEnabled()
{
	Component->SetVisibility(bEnabled);
}

