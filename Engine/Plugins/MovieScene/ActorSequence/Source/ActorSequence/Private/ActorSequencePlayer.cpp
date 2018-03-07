// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "ActorSequencePlayer.h"
#include "Classes/Engine/BlueprintGeneratedClass.h"
#include "Classes/Engine/SimpleConstructionScript.h"


UObject* UActorSequencePlayer::GetPlaybackContext() const
{
	UActorSequence* ActorSequence = CastChecked<UActorSequence>(Sequence);
	if (ActorSequence)
	{
		if (AActor* Actor = ActorSequence->GetTypedOuter<AActor>())
		{
			return Actor;
		}
#if WITH_EDITOR
		else if (UBlueprintGeneratedClass* GeneratedClass = ActorSequence->GetTypedOuter<UBlueprintGeneratedClass>())
		{
			return GeneratedClass->SimpleConstructionScript->GetComponentEditorActorInstance();
		}
#endif
	}

	return nullptr;
}

TArray<UObject*> UActorSequencePlayer::GetEventContexts() const
{
	TArray<UObject*> Contexts;
	if (UObject* PlaybackContext = GetPlaybackContext())
	{
		Contexts.Add(PlaybackContext);
	}
	return Contexts;
}