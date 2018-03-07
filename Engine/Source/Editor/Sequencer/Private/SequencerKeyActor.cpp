// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "SequencerKeyActor.h"
#include "Components/StaticMeshComponent.h"
#include "Sections/MovieScene3DTransformSection.h"
#include "SequencerEdMode.h"
#include "Materials/MaterialInstance.h" 
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/Selection.h"
#include "EditorModeTools.h"
#include "EditorModeManager.h"
#include "ModuleManager.h"
#include "EditorViewportClient.h"
#include "UnrealClient.h"

ASequencerKeyActor::ASequencerKeyActor()
	: Super()
{
	UStaticMesh* KeyEditorMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/VREditor/TransformGizmo/SM_Sequencer_Key"));
	check(KeyEditorMesh != nullptr);
	UMaterial* KeyEditorMaterial = LoadObject<UMaterial>(nullptr, TEXT("/Engine/VREditor/TransformGizmo/Main"));
	check(KeyEditorMaterial != nullptr);

	const bool bTransient = true;
	USceneComponent* SceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("SceneComponent"), bTransient);
	check(SceneComponent != nullptr);
	this->RootComponent = SceneComponent;

	KeyMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("KeyMesh"));
	KeyMeshComponent->SetMobility(EComponentMobility::Movable);
	KeyMeshComponent->SetupAttachment(RootComponent);
	KeyMeshComponent->SetStaticMesh(KeyEditorMesh);
	KeyMeshComponent->CreateAndSetMaterialInstanceDynamicFromMaterial(0, KeyEditorMaterial);

	KeyMeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	KeyMeshComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	KeyMeshComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	
	KeyMeshComponent->bGenerateOverlapEvents = false;
	KeyMeshComponent->SetCanEverAffectNavigation(false);
	KeyMeshComponent->bCastDynamicShadow = false;
	KeyMeshComponent->bCastStaticShadow = false;
	KeyMeshComponent->bAffectDistanceFieldLighting = false;
	KeyMeshComponent->bAffectDynamicIndirectLighting = false;
}


void ASequencerKeyActor::PostEditMove(bool bFinished)
{
	// Push the key's transform to the Sequencer track
	PropagateKeyChange();
	Super::PostEditMove(bFinished);
}

void ASequencerKeyActor::SetKeyData(class UMovieScene3DTransformSection* NewTrackSection, float NewKeyTime)
{
	TrackSection = NewTrackSection;
	KeyTime = NewKeyTime;
	// Associate the currently selected actor with this key
	AssociatedActor = GEditor->GetSelectedActors()->GetTop<AActor>();
	// Draw a single transform track based on the data from this key
	FEditorViewportClient* ViewportClient = StaticCast<FEditorViewportClient*>(GEditor->GetActiveViewport()->GetClient());
	if (ViewportClient != nullptr)
	{
		FSequencerEdMode* SequencerEdMode = (FSequencerEdMode*)(ViewportClient->GetModeTools()->GetActiveMode(FSequencerEdMode::EM_SequencerMode));
		SequencerEdMode->DrawMeshTransformTrailFromKey(this);
	}
}


void ASequencerKeyActor::PropagateKeyChange()
{
	if (TrackSection != nullptr)
	{
		// Mark the track section as dirty
		TrackSection->Modify();

		// Update the translation keys
		FRichCurve& TransXCurve = TrackSection->GetTranslationCurve(EAxis::X);
		FRichCurve& TransYCurve = TrackSection->GetTranslationCurve(EAxis::Y);
		FRichCurve& TransZCurve = TrackSection->GetTranslationCurve(EAxis::Z);
		TransXCurve.UpdateOrAddKey(KeyTime, GetActorTransform().GetLocation().X);
		TransYCurve.UpdateOrAddKey(KeyTime, GetActorTransform().GetLocation().Y);
		TransZCurve.UpdateOrAddKey(KeyTime, GetActorTransform().GetLocation().Z);

		// Draw a single transform track based on the data from this key
		FEditorViewportClient* ViewportClient = StaticCast<FEditorViewportClient*>(GEditor->GetActiveViewport()->GetClient());
		if (ViewportClient != nullptr)
		{
			FSequencerEdMode* SequencerEdMode = (FSequencerEdMode*)(ViewportClient->GetModeTools()->GetActiveMode(FSequencerEdMode::EM_SequencerMode));
			SequencerEdMode->DrawMeshTransformTrailFromKey(this);
		}
	}
}

