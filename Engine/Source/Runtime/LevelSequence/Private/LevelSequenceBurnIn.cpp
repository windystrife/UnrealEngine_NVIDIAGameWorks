// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "LevelSequenceBurnIn.h"

ULevelSequenceBurnIn::ULevelSequenceBurnIn( const FObjectInitializer& ObjectInitializer )
	: Super(ObjectInitializer)
{
}

void ULevelSequenceBurnIn::TakeSnapshotsFrom(ALevelSequenceActor& InActor)
{
	LevelSequenceActor = &InActor;
	if (ensure(InActor.SequencePlayer))
	{
		InActor.SequencePlayer->OnSequenceUpdated().AddUObject(this, &ULevelSequenceBurnIn::OnSequenceUpdated);
		InActor.SequencePlayer->TakeFrameSnapshot(FrameInformation);
	}
}

void ULevelSequenceBurnIn::OnSequenceUpdated(const UMovieSceneSequencePlayer& Player, float CurrentTime, float PreviousTime)
{
	static_cast<const ULevelSequencePlayer&>(Player).TakeFrameSnapshot(FrameInformation);
}
