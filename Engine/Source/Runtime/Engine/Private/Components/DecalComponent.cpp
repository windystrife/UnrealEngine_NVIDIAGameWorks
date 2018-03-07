// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DecalComponent.cpp: Decal component implementation.
=============================================================================*/

#include "Components/DecalComponent.h"
#include "Materials/Material.h"
#include "TimerManager.h"
#include "SceneManagement.h"
#include "Materials/MaterialInstanceDynamic.h"

static TAutoConsoleVariable<float> CVarDecalFadeDurationScale(
	TEXT("r.Decal.FadeDurationScale"),
	1.0f,
	TEXT("Scales the per decal fade durations. Lower values shortens lifetime and fade duration. Default is 1.0f.")
	);

FDeferredDecalProxy::FDeferredDecalProxy(const UDecalComponent* InComponent)
	: DrawInGame( InComponent->bVisible && !InComponent->bHiddenInGame )
	, DrawInEditor( InComponent->bVisible )
	, InvFadeDuration(-1.0f)
	, FadeStartDelayNormalized(1.0f)
	, FadeScreenSize( InComponent->FadeScreenSize )
{
	UMaterialInterface* EffectiveMaterial = UMaterial::GetDefaultMaterial(MD_DeferredDecal);

	if(InComponent->DecalMaterial)
	{
		UMaterial* BaseMaterial = InComponent->DecalMaterial->GetMaterial();

		if(BaseMaterial->MaterialDomain == MD_DeferredDecal)
		{
			EffectiveMaterial = InComponent->DecalMaterial;
		}
	}

	Component = InComponent;
	DecalMaterial = EffectiveMaterial;
	SetTransformIncludingDecalSize(InComponent->GetTransformIncludingDecalSize());
	bOwnerSelected = InComponent->IsOwnerSelected();
	SortOrder = InComponent->SortOrder;

#if WITH_EDITOR
	// We don't want to fade when we're editing, only in Simulate/PIE/Game
	if (!GIsEditor || GIsPlayInEditorWorld)
#endif
	{
		InitializeFadingParameters(InComponent->GetWorld()->GetTimeSeconds(), InComponent->GetFadeDuration(), InComponent->GetFadeStartDelay());
	}
	
	if ( InComponent->GetOwner() )
	{
		DrawInGame &= !( InComponent->GetOwner()->bHidden );
#if WITH_EDITOR
		DrawInEditor &= !InComponent->GetOwner()->IsHiddenEd();
#endif
	}
}

void FDeferredDecalProxy::SetTransformIncludingDecalSize(const FTransform& InComponentToWorldIncludingDecalSize)
{
	ComponentTrans = InComponentToWorldIncludingDecalSize;
}

void FDeferredDecalProxy::InitializeFadingParameters(float AbsSpawnTime, float FadeDuration, float FadeStartDelay)
{
	if (FadeDuration > 0.0f)
	{
		InvFadeDuration = 1.0f / FadeDuration;
		FadeStartDelayNormalized = (AbsSpawnTime + FadeStartDelay + FadeDuration) * InvFadeDuration;
	}
}

bool FDeferredDecalProxy::IsShown( const FSceneView* View ) const
{
	// Logic here should match FPrimitiveSceneProxy::IsShown for consistent behavior in editor and at runtime.
#if WITH_EDITOR
	if ( View->Family->EngineShowFlags.Editor )
	{
		if ( !DrawInEditor )
		{
			return false;
		}
	}
	else
#endif
	{
		if ( !DrawInGame
#if WITH_EDITOR
			|| ( !View->bIsGameView && View->Family->EngineShowFlags.Game && !DrawInEditor )
#endif
			)
		{
			return false;
		}
	}
	return true;
}

UDecalComponent::UDecalComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, FadeScreenSize(0.01)
	, FadeStartDelay(0.0f)
	, FadeDuration(0.0f)
	, bDestroyOwnerAfterFade(true)
	, DecalSize(128.0f, 256.0f, 256.0f)
{
}

void UDecalComponent::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if (Ar.UE4Ver() < VER_UE4_DECAL_SIZE)
	{
		DecalSize = FVector(1.0f, 1.0f, 1.0f);
	}
}

void UDecalComponent::SetLifeSpan(const float LifeSpan)
{
	if (LifeSpan > 0.f)
	{
		GetWorld()->GetTimerManager().SetTimer(TimerHandle_DestroyDecalComponent, this, &UDecalComponent::LifeSpanCallback, LifeSpan, false);
	}
	else
	{
		GetWorld()->GetTimerManager().ClearTimer(TimerHandle_DestroyDecalComponent);
	}
}

void UDecalComponent::LifeSpanCallback()
{
	DestroyComponent();

	auto* Owner = GetOwner();

	if (bDestroyOwnerAfterFade && Owner && (FadeDuration > 0.0f || FadeStartDelay > 0.0f))
	{
		Owner->Destroy();
	}
}

float UDecalComponent::GetFadeStartDelay() const
{
	return FadeStartDelay;
}

float UDecalComponent::GetFadeDuration() const
{
	return FadeDuration;
}

void UDecalComponent::SetFadeOut(float StartDelay, float Duration, bool DestroyOwnerAfterFade /*= true*/)
{
	float FadeDurationScale = CVarDecalFadeDurationScale.GetValueOnGameThread();
	FadeDurationScale = (FadeDurationScale <= SMALL_NUMBER) ? 0.0f : FadeDurationScale;

	FadeStartDelay = StartDelay * FadeDurationScale;
	FadeDuration = Duration * FadeDurationScale;
	bDestroyOwnerAfterFade = DestroyOwnerAfterFade;
	SetLifeSpan(FadeStartDelay + FadeDuration);

	MarkRenderStateDirty();
}

void UDecalComponent::SetFadeScreenSize(float NewFadeScreenSize)
{
	FadeScreenSize = NewFadeScreenSize;

	MarkRenderStateDirty();
}


void UDecalComponent::SetSortOrder(int32 Value)
{
	SortOrder = Value;

	MarkRenderStateDirty();
}

void UDecalComponent::SetDecalMaterial(class UMaterialInterface* NewDecalMaterial)
{
	DecalMaterial = NewDecalMaterial;

	MarkRenderStateDirty();	
}

void UDecalComponent::PushSelectionToProxy()
{
	MarkRenderStateDirty();	
}

class UMaterialInterface* UDecalComponent::GetDecalMaterial() const
{
	return DecalMaterial;
}

class UMaterialInstanceDynamic* UDecalComponent::CreateDynamicMaterialInstance()
{
	// Create the MID
	UMaterialInstanceDynamic* Instance = UMaterialInstanceDynamic::Create(DecalMaterial, this);

	// Assign it, once parent is set
	SetDecalMaterial(Instance);

	return Instance;
}

void UDecalComponent::GetUsedMaterials( TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials ) const
{
	OutMaterials.Add( GetDecalMaterial() );
}


FDeferredDecalProxy* UDecalComponent::CreateSceneProxy()
{
	return new FDeferredDecalProxy(this);
}

FBoxSphereBounds UDecalComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	return FBoxSphereBounds(FVector(0, 0, 0), DecalSize, DecalSize.Size()).TransformBy(LocalToWorld);
}

void UDecalComponent::BeginPlay()
{
	Super::BeginPlay();

	SetLifeSpan(FadeStartDelay + FadeDuration);
}

void UDecalComponent::CreateRenderState_Concurrent()
{
	Super::CreateRenderState_Concurrent();

	// Mimics UPrimitiveComponent's visibility logic, although without the UPrimitiveCompoent visibility flags
	if ( ShouldComponentAddToScene() && ShouldRender() )
	{
		GetWorld()->Scene->AddDecal(this);
	}
}

void UDecalComponent::SendRenderTransform_Concurrent()
{	
	//If Decal isn't hidden update its transform.
	if ( ShouldComponentAddToScene() && ShouldRender() )
	{
		GetWorld()->Scene->UpdateDecalTransform(this);
	}

	Super::SendRenderTransform_Concurrent();
}

const UObject* UDecalComponent::AdditionalStatObject() const
{
	return DecalMaterial;
}

void UDecalComponent::DestroyRenderState_Concurrent()
{
	Super::DestroyRenderState_Concurrent();
	GetWorld()->Scene->RemoveDecal(this);
}

