// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "ActorSequence.h"
#include "MovieScene.h"
#include "MovieSceneCommonHelpers.h"
#include "Modules/ModuleManager.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/Blueprint.h"
#include "GameFramework/Actor.h"
#include "ActorSequenceComponent.h"

IMPLEMENT_MODULE(FDefaultModuleImpl, ActorSequence);

#if WITH_EDITOR
UActorSequence::FOnInitialize UActorSequence::OnInitializeSequenceEvent;
#endif

UActorSequence::UActorSequence(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, MovieScene(nullptr)
#if WITH_EDITOR
	, bHasBeenInitialized(false)
#endif
{
	bParentContextsAreSignificant = true;

	MovieScene = ObjectInitializer.CreateDefaultSubobject<UMovieScene>(this, "MovieScene");
	MovieScene->SetFlags(RF_Transactional);
}

bool UActorSequence::IsEditable() const
{
	UObject* Template = GetArchetype();

	if (Template == GetDefault<UActorSequence>())
	{
		return false;
	}

	return !Template || Template->GetTypedOuter<UActorSequenceComponent>() == GetDefault<UActorSequenceComponent>();
}

UBlueprint* UActorSequence::GetParentBlueprint() const
{
	if (UBlueprintGeneratedClass* GeneratedClass = GetTypedOuter<UBlueprintGeneratedClass>())
	{
		return Cast<UBlueprint>(GeneratedClass->ClassGeneratedBy);
	}
	return nullptr;
}

void UActorSequence::PostInitProperties()
{
#if WITH_EDITOR && WITH_EDITORONLY_DATA
	UActorComponent* OwnerComponent = Cast<UActorComponent>(GetOuter());
	if (!bHasBeenInitialized && !HasAnyFlags(RF_ClassDefaultObject) && OwnerComponent && !OwnerComponent->HasAnyFlags(RF_ClassDefaultObject))
	{
		AActor* Actor = Cast<AActor>(OwnerComponent->GetOuter());

		FGuid BindingID = MovieScene->AddPossessable(Actor ? Actor->GetActorLabel() : TEXT("Owner"), Actor ? Actor->GetClass() : AActor::StaticClass());
		ObjectReferences.CreateBinding(BindingID, FActorSequenceObjectReference::CreateForContextActor());

		OnInitializeSequenceEvent.Broadcast(this);
		bHasBeenInitialized = true;
	}
#endif

	Super::PostInitProperties();
}

void UActorSequence::BindPossessableObject(const FGuid& ObjectId, UObject& PossessedObject, UObject* Context)
{
	AActor* ActorContext = CastChecked<AActor>(Context);

	if (UActorComponent* Component = Cast<UActorComponent>(&PossessedObject))
	{
		ObjectReferences.CreateBinding(ObjectId, FActorSequenceObjectReference::CreateForComponent(Component));
	}
	else if (AActor* Actor = Cast<AActor>(&PossessedObject))
	{
		ObjectReferences.CreateBinding(ObjectId, FActorSequenceObjectReference::CreateForActor(Actor, ActorContext));
	}
}

bool UActorSequence::CanPossessObject(UObject& Object, UObject* InPlaybackContext) const
{
	AActor* ActorContext = CastChecked<AActor>(InPlaybackContext);

	if (AActor* Actor = Cast<AActor>(&Object))
	{
		return Actor == InPlaybackContext || Actor->GetLevel() == ActorContext->GetLevel();
	}
	else if (UActorComponent* Component = Cast<UActorComponent>(&Object))
	{
		return Component->GetOwner() ? Component->GetOwner()->GetLevel() == ActorContext->GetLevel() : false;
	}
	return false;
}

void UActorSequence::LocateBoundObjects(const FGuid& ObjectId, UObject* Context, TArray<UObject*, TInlineAllocator<1>>& OutObjects) const
{
	if (Context)
	{
		ObjectReferences.ResolveBinding(ObjectId, CastChecked<AActor>(Context), OutObjects);
	}
}

UMovieScene* UActorSequence::GetMovieScene() const
{
	return MovieScene;
}

UObject* UActorSequence::GetParentObject(UObject* Object) const
{
	if (UActorComponent* Component = Cast<UActorComponent>(Object))
	{
		return Component->GetOwner();
	}

	return nullptr;
}

void UActorSequence::UnbindPossessableObjects(const FGuid& ObjectId)
{
	ObjectReferences.RemoveBinding(ObjectId);
}

#if WITH_EDITOR
FText UActorSequence::GetDisplayName() const
{
	UActorSequenceComponent* Component = GetTypedOuter<UActorSequenceComponent>();

	if (Component)
	{
		FString OwnerName;
		
		if (UBlueprint* Blueprint = GetParentBlueprint())
		{
			OwnerName = Blueprint->GetName();
		}
		else if(AActor* Owner = Component->GetOwner())
		{
			OwnerName = Owner->GetActorLabel();
		}

		return OwnerName.IsEmpty()
			? FText::FromName(Component->GetFName())
			: FText::Format(NSLOCTEXT("ActorSequence", "DisplayName", "{0} ({1})"), FText::FromName(Component->GetFName()), FText::FromString(OwnerName));
	}

	return UMovieSceneSequence::GetDisplayName();
}
#endif