// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "ActorSequenceComponent.h"
#include "ActorSequence.h"
#include "ActorSequencePlayer.h"

UActorSequenceComponent::UActorSequenceComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bAutoPlay(false)
{
	PrimaryComponentTick.bCanEverTick = true;

	if (HasAnyFlags(RF_ClassDefaultObject) || GetArchetype() == GetDefault<UActorSequenceComponent>())
	{
		Sequence = ObjectInitializer.CreateDefaultSubobject<UActorSequence>(this, "Sequence");
		Sequence->SetFlags(RF_Public | RF_Transactional);
	}
}

void UActorSequenceComponent::PostInitProperties()
{
	Super::PostInitProperties();
}

void UActorSequenceComponent::BeginPlay()
{
	Super::BeginPlay();

	if (Sequence != nullptr)
	{
		SequencePlayer = NewObject<UActorSequencePlayer>(this, "SequencePlayer");
		SequencePlayer->Initialize(Sequence, PlaybackSettings);

		if (bAutoPlay)
		{
			SequencePlayer->Play();
		}
	}
}

void UActorSequenceComponent::TickComponent(float DeltaSeconds, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	Super::TickComponent(DeltaSeconds, TickType, ThisTickFunction);

	if (SequencePlayer)
	{
		SequencePlayer->Update(DeltaSeconds);
	}
}