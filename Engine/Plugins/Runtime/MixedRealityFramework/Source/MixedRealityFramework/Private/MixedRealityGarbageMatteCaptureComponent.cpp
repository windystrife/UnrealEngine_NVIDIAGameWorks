// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MixedRealityGarbageMatteCaptureComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Materials/Material.h"
#include "UObject/ConstructorHelpers.h"
#include "MixedRealityConfigurationSaveGame.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "MixedRealityUtilLibrary.h"

/* UMixedRealityGarbageMatteCaptureComponent
 *****************************************************************************/

//------------------------------------------------------------------------------
UMixedRealityGarbageMatteCaptureComponent::UMixedRealityGarbageMatteCaptureComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCaptureEveryFrame = true;
	PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	CaptureSource = ESceneCaptureSource::SCS_SceneColorHDR;
	PostProcessBlendWeight = 0.0f;
	ShowFlags.SetAtmosphericFog(false);
	ShowFlags.SetFog(false);
}

//------------------------------------------------------------------------------
void UMixedRealityGarbageMatteCaptureComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{
	if (GarbageMatteActor)
	{
		GarbageMatteActor->Destroy();
		GarbageMatteActor = nullptr;
	}

	Super::OnComponentDestroyed(bDestroyingHierarchy);
}

//------------------------------------------------------------------------------
const AActor* UMixedRealityGarbageMatteCaptureComponent::GetViewOwner() const
{
	// This lets SetOnlyOwnerSee on the garbage matte actor make it visible only to this capture component.
	// Basically the "owner" Actor's pointer is being used as an ID for who renders the actor.
	return GarbageMatteActor;
}

//------------------------------------------------------------------------------
void UMixedRealityGarbageMatteCaptureComponent::ApplyConfiguration(const UMixedRealityConfigurationSaveGame& SaveGameInstance)
{
	// garbage matte actor
	{
		// Spawn or clear out existing garbage matte actor
		if (GarbageMatteActor)
		{
			ShowOnlyActors.Empty();
			GarbageMatteActor->Destroy();
			GarbageMatteActor = nullptr;
		}

		const FVector Location(EForceInit::ForceInitToZero);
		const FRotator Rotation(EForceInit::ForceInitToZero);
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Name = TEXT("GarbageMatteActor");
		GarbageMatteActor = GetWorld()->SpawnActor<AStaticMeshActor>(Location, Rotation, SpawnParameters);
		check(GarbageMatteActor);

		USceneComponent* RootComponent = GarbageMatteActor->GetRootComponent();
		check(RootComponent);
		RootComponent->SetVisibility(false, false);
		RootComponent->SetMobility(EComponentMobility::Movable);

		// Add garbage matte static meshes
		for (const FGarbageMatteSaveData& Data : SaveGameInstance.GarbageMatteSaveDatas)
		{
			UStaticMeshComponent* MeshComponent = NewObject<UStaticMeshComponent>(GarbageMatteActor, NAME_None);
			MeshComponent->SetStaticMesh(GarbageMatteMesh);
			MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			MeshComponent->SetCastShadow(false);
			MeshComponent->SetRelativeTransform(Data.Transform);
			MeshComponent->SetMaterial(0, LoadObject<UMaterial>(nullptr, TEXT("/MixedRealityFramework/GarbageMatteRuntimeMaterial.GarbageMatteRuntimeMaterial")));
			MeshComponent->SetOnlyOwnerSee(true);
			MeshComponent->SetMobility(EComponentMobility::Movable);
			MeshComponent->SetupAttachment(RootComponent);
			MeshComponent->RegisterComponent();
		}
	}
	// GarbageMatteCaptureComponent
	if (ExternalGarbageMatteActor == nullptr)
	{
		FOVAngle = SaveGameInstance.AlignmentData.FOV;
		ShowOnlyActors.Empty();
		ShowOnlyActors.Push(GarbageMatteActor);
	}
}

//------------------------------------------------------------------------------
void UMixedRealityGarbageMatteCaptureComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (GarbageMatteActor)
	{
		USceneComponent* GarbageMatteActorParentComponent = GarbageMatteActor->GetRootComponent()->GetAttachParent();
		USceneComponent* HMDRootComponent = UMixedRealityUtilLibrary::GetHMDRootComponent(GetWorld(), 0);
		if (GarbageMatteActorParentComponent != HMDRootComponent)
		{	
			GarbageMatteActor->AttachToComponent(HMDRootComponent, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		}
	}
}

void UMixedRealityGarbageMatteCaptureComponent::SetExternalGarbageMatteActor(AActor* Actor)
{
	ExternalGarbageMatteActor = Actor;
	ShowOnlyActors.Empty();
	if (ExternalGarbageMatteActor != nullptr)
	{
		ShowOnlyActors.Push(ExternalGarbageMatteActor);
	}
}

void UMixedRealityGarbageMatteCaptureComponent::ClearExternalGarbageMatteActor()
{
	ExternalGarbageMatteActor = nullptr;
	ShowOnlyActors.Empty();
	if (GarbageMatteActor != nullptr)
	{
		ShowOnlyActors.Push(GarbageMatteActor);
	}
}

