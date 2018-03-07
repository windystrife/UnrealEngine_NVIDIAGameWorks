// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "LevelSequenceActorSpawner.h"
#include "MovieSceneSpawnable.h"
#include "IMovieScenePlayer.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "Particles/ParticleSystemComponent.h"
#include "Engine/Engine.h"

static const FName SequencerActorTag(TEXT("SequencerActor"));

TSharedRef<IMovieSceneObjectSpawner> FLevelSequenceActorSpawner::CreateObjectSpawner()
{
	return MakeShareable(new FLevelSequenceActorSpawner);
}

UClass* FLevelSequenceActorSpawner::GetSupportedTemplateType() const
{
	return AActor::StaticClass();
}

UObject* FLevelSequenceActorSpawner::SpawnObject(FMovieSceneSpawnable& Spawnable, FMovieSceneSequenceIDRef TemplateID, IMovieScenePlayer& Player)
{
	AActor* ObjectTemplate = Cast<AActor>(Spawnable.GetObjectTemplate());
	if (!ObjectTemplate)
	{
		return nullptr;
	}

	// @todo sequencer: We should probably spawn these in a specific sub-level!
	// World->CurrentLevel = ???;

	const EObjectFlags ObjectFlags = RF_Transient;

	// @todo sequencer livecapture: Consider using SetPlayInEditorWorld() and RestoreEditorWorld() here instead
	
	// @todo sequencer actors: We need to make sure puppet objects aren't copied into PIE/SIE sessions!  They should be omitted from that duplication!

	UWorld* WorldContext = Cast<UWorld>(Player.GetPlaybackContext());
	if(WorldContext == nullptr)
	{
		WorldContext = GWorld;
	}

	// Spawn the puppet actor
	FActorSpawnParameters SpawnInfo;
	{
		SpawnInfo.Name = NAME_None;
		SpawnInfo.ObjectFlags = ObjectFlags;
		SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		// @todo: Spawning with a non-CDO template is fraught with issues
		//SpawnInfo.Template = ObjectTemplate;
		// allow pre-construction variables to be set.
		SpawnInfo.bDeferConstruction = true;
		SpawnInfo.OverrideLevel = WorldContext->PersistentLevel;
	}

	FTransform SpawnTransform;

	if (USceneComponent* RootComponent = ObjectTemplate->GetRootComponent())
	{
		SpawnTransform.SetTranslation(RootComponent->RelativeLocation);
		SpawnTransform.SetRotation(RootComponent->RelativeRotation.Quaternion());
	}

	{
		// Disable all particle components so that they don't auto fire as soon as the actor is spawned. The particles should be triggered through the particle track.
		TArray<UActorComponent*> ParticleComponents = ObjectTemplate->GetComponentsByClass(UParticleSystemComponent::StaticClass());
		for (int32 ComponentIdx = 0; ComponentIdx < ParticleComponents.Num(); ++ComponentIdx)
		{
			ParticleComponents[ComponentIdx]->bAutoActivate = false;
		}
	}

	AActor* SpawnedActor = WorldContext->SpawnActorAbsolute(ObjectTemplate->GetClass(), SpawnTransform, SpawnInfo);
	if (!SpawnedActor)
	{
		return nullptr;
	}
	
	UEngine::FCopyPropertiesForUnrelatedObjectsParams CopyParams;
	CopyParams.bPreserveRootComponent = false;
	CopyParams.bNotifyObjectReplacement = false;
	SpawnedActor->UnregisterAllComponents();
	UEngine::CopyPropertiesForUnrelatedObjects(ObjectTemplate, SpawnedActor, CopyParams);
	SpawnedActor->RegisterAllComponents();

	// Ensure this spawnable is not a preview actor. Preview actors will not have BeginPlay() called on them.
#if WITH_EDITOR
	SpawnedActor->bIsEditorPreviewActor = false;
#endif

	// tag this actor so we know it was spawned by sequencer
	SpawnedActor->Tags.AddUnique(SequencerActorTag);

#if WITH_EDITOR
	if (GIsEditor)
	{
		// Explicitly set RF_Transactional on spawned actors so we can undo/redo properties on them. We don't add this as a spawn flag since we don't want to transact spawn/destroy events.
		SpawnedActor->SetFlags(RF_Transactional);

		for (UActorComponent* Component : TInlineComponentArray<UActorComponent*>(SpawnedActor))
		{
			Component->SetFlags(RF_Transactional);
		}
	}

	SpawnedActor->SetActorLabel(Spawnable.GetName());
#endif

	const bool bIsDefaultTransform = true;
	SpawnedActor->FinishSpawning(SpawnTransform, bIsDefaultTransform);

	return SpawnedActor;
}

void FLevelSequenceActorSpawner::DestroySpawnedObject(UObject& Object)
{
	AActor* Actor = Cast<AActor>(&Object);
	if (!ensure(Actor))
	{
		return;
	}

#if WITH_EDITOR
	if (GIsEditor)
	{
		// Explicitly remove RF_Transactional on spawned actors since we don't want to trasact spawn/destroy events
		Actor->ClearFlags(RF_Transactional);
		for (UActorComponent* Component : TInlineComponentArray<UActorComponent*>(Actor))
		{
			Component->ClearFlags(RF_Transactional);
		}
	}
#endif

	UWorld* World = Actor->GetWorld();
	if (ensure(World))
	{
		const bool bNetForce = false;
		const bool bShouldModifyLevel = false;
		World->DestroyActor(Actor, bNetForce, bShouldModifyLevel);
	}
}
