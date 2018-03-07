// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SequencerMeshTrail.h"
#include "SequencerKeyActor.h"
#include "EditorWorldExtension.h"
#include "ViewportWorldInteraction.h"
#include "Materials/MaterialInstance.h" 
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/BillboardComponent.h"
#include "Components/StaticMeshComponent.h"
#include "MovieScene3DTransformSection.h"
#include "TimerManager.h"
#include "Editor.h"

ASequencerMeshTrail::ASequencerMeshTrail()
	: Super()
{
	TrailTime = 0.0f;
	MaxTrailTime = 1.0f;
	UBillboardComponent* PrimitiveRootComponent = CreateDefaultSubobject<UBillboardComponent>(TEXT("RootComponent"));
	PrimitiveRootComponent->bSelectable = false;
	PrimitiveRootComponent->SetVisibility(false, false);
	SetRootComponent(PrimitiveRootComponent);

	if (!IsTemplate() && GEditor)
	{
		GEditor->GetTimerManager()->SetTimer(TrailUpdate, FTimerDelegate::CreateUObject(this, &ASequencerMeshTrail::UpdateTrailAppearance, 0.0005f), 0.0005f, true);
	}
}

void ASequencerMeshTrail::Cleanup()
{
	UEditorWorldExtensionCollection* ExtensionCollection = GEditor->GetEditorWorldExtensionsManager()->GetEditorWorldExtensions(GetWorld());
	if (ExtensionCollection != nullptr)
	{
		UViewportWorldInteraction* WorldInteraction = Cast<UViewportWorldInteraction>(ExtensionCollection->FindExtension(UViewportWorldInteraction::StaticClass()));
		if (WorldInteraction != nullptr)
		{
			// Destroy all the key actors this trail created
			for (FKeyActorData KeyMesh : KeyMeshActors)
			{
				WorldInteraction->DestroyTransientActor(KeyMesh.KeyActor);
			}
			WorldInteraction->DestroyTransientActor(this);
		}
	}
	else
	{
		// Destroy all the key actors this trail created
		for (FKeyActorData KeyMesh : KeyMeshActors)
		{
			KeyMesh.KeyActor->Destroy();
		}
		this->Destroy();
	}
}

void ASequencerMeshTrail::AddKeyMeshActor(float KeyTime, const FTransform KeyTransform, class UMovieScene3DTransformSection* TrackSection)
{
	ASequencerKeyActor* KeyMeshActor = nullptr;
	UStaticMesh* KeyEditorMesh = nullptr;
	UMaterialInstance* KeyEditorMaterial = nullptr;
	MaxTrailTime = FMath::Max(KeyTime, MaxTrailTime);
	FKeyActorData* KeyPtr = KeyMeshActors.FindByPredicate([KeyTime](const FKeyActorData InKey)
	{
		return FMath::IsNearlyEqual(KeyTime, InKey.Time);
	});

	// If we don't currently have an actor for this time, create one
	if (KeyPtr == nullptr)
	{
		UViewportWorldInteraction* WorldInteraction = Cast<UViewportWorldInteraction>( GEditor->GetEditorWorldExtensionsManager()->GetEditorWorldExtensions( GetWorld() )->FindExtension( UViewportWorldInteraction::StaticClass() ) );
		if( WorldInteraction != nullptr )
		{
			KeyMeshActor = WorldInteraction->SpawnTransientSceneActor<ASequencerKeyActor>( TEXT( "KeyMesh" ), false );
			KeyMeshActor->SetActorTransform(KeyTransform);
			// Destroy all the frame components this trail created
			for (FFrameComponentData FrameMesh : FrameMeshComponents)
			{
				FrameMesh.FrameComponent->DestroyComponent();
			}
			FrameMeshComponents.Reset();
			KeyMeshActor->SetKeyData(TrackSection, KeyTime);
			KeyMeshActor->SetOwner(this);
			FKeyActorData NewKey = FKeyActorData(KeyTime, KeyMeshActor);
			KeyMeshActors.Add(NewKey);
		}
	}
	else
	{
		// Just update the transform
		KeyPtr->KeyActor->SetActorTransform(KeyTransform);
	}
}

void ASequencerMeshTrail::AddFrameMeshComponent(const float FrameTime, const FTransform FrameTransform)
{
	UStaticMeshComponent* FrameMeshComponent = nullptr;
	UStaticMesh* FrameEditorMesh = nullptr;
	UMaterial* FrameEditorMaterial = nullptr;

	FFrameComponentData* FramePtr = FrameMeshComponents.FindByPredicate([FrameTime](const FFrameComponentData InFrame)
	{
		return FMath::IsNearlyEqual(FrameTime, InFrame.Time);
	});

	// If we don't currently have a component for this time, create one
	if (FramePtr == nullptr)
	{
		FrameMeshComponent = NewObject<UStaticMeshComponent>(this);
		this->AddOwnedComponent(FrameMeshComponent);
		FrameMeshComponent->RegisterComponent();
		FrameEditorMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/VREditor/TransformGizmo/SM_Sequencer_Node"));
		check(FrameEditorMesh != nullptr);
		FrameEditorMaterial = LoadObject<UMaterial>(nullptr, TEXT("/Engine/VREditor/TransformGizmo/Main"));
		check(FrameEditorMaterial != nullptr);
		FrameMeshComponent->SetStaticMesh(FrameEditorMesh);
		FrameMeshComponent->CreateAndSetMaterialInstanceDynamicFromMaterial(0, FrameEditorMaterial);
		FrameMeshComponent->SetMobility(EComponentMobility::Movable);
		FrameMeshComponent->SetWorldTransform(FrameTransform);
		FrameMeshComponent->SetCastShadow(false);
		FrameMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		FrameMeshComponent->bSelectable = false;
		FFrameComponentData NewFrame = FFrameComponentData(FrameTime, FrameMeshComponent);
		FrameMeshComponents.Add(NewFrame);
	}
	else
	{
		// Just update the transform
		FramePtr->FrameComponent->SetWorldTransform(FrameTransform);
	}
}

void ASequencerMeshTrail::UpdateTrailAppearance(float UpdateTime)
{
	if (FMath::IsNearlyEqual(MaxTrailTime, 0.0f))
	{
		MaxTrailTime = 1.0f;
	}
	// TODO: No hardcoded glow values
	TrailTime = FMath::Fmod(TrailTime + UpdateTime, MaxTrailTime);
	for (FFrameComponentData Frame : FrameMeshComponents)
	{
		UMaterialInstanceDynamic* FrameMaterial = Cast<UMaterialInstanceDynamic>(Frame.FrameComponent->GetMaterial(0));
		if (FrameMaterial != nullptr)
		{
			float GlowValue = 0.0f;
			if (FMath::IsNearlyEqual(TrailTime, Frame.Time, 0.1f))
			{
				GlowValue = 12.0f;
			}
			else
			{
				GlowValue = 3.0f;
			}
			FrameMaterial->SetScalarParameterValue("GlowAmount", GlowValue);
		}
	}
	for (FKeyActorData Key : KeyMeshActors)
	{
		UMaterialInstanceDynamic* KeyMaterial = Cast<UMaterialInstanceDynamic>(Key.KeyActor->GetMeshComponent()->GetMaterial(0));
		if (KeyMaterial != nullptr)
		{
			float GlowValue = 0.0f;
			if (FMath::IsNearlyEqual(TrailTime, Key.Time, 0.1f))
			{
				GlowValue = 20.0f;
			}
			else
			{
				GlowValue = 5.0f;
			}
			KeyMaterial->SetScalarParameterValue("GlowAmount", GlowValue);
		}
	}
}
