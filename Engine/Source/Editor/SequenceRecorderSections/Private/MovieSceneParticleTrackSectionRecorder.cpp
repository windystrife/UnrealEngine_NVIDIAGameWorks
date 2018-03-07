// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneParticleTrackSectionRecorder.h"
#include "Tracks/MovieSceneParticleTrack.h"
#include "MovieScene.h"

TSharedPtr<IMovieSceneSectionRecorder> FMovieSceneParticleTrackSectionRecorderFactory::CreateSectionRecorder(const struct FActorRecordingSettings& InActorRecordingSettings) const
{
	return MakeShareable(new FMovieSceneParticleTrackSectionRecorder);
}

bool FMovieSceneParticleTrackSectionRecorderFactory::CanRecordObject(UObject* InObjectToRecord) const
{
	return InObjectToRecord->IsA<UParticleSystemComponent>();
}

FMovieSceneParticleTrackSectionRecorder::~FMovieSceneParticleTrackSectionRecorder()
{
	if(DelegateProxy.IsValid())
	{
		DelegateProxy->SectionRecorder = nullptr;
		DelegateProxy->RemoveFromRoot();
		DelegateProxy.Reset();
	}
}

void FMovieSceneParticleTrackSectionRecorder::CreateSection(UObject* InObjectToRecord, UMovieScene* MovieScene, const FGuid& Guid, float Time)
{
	SystemToRecord = CastChecked<UParticleSystemComponent>(InObjectToRecord);

	UMovieSceneParticleTrack* ParticleTrack = MovieScene->AddTrack<UMovieSceneParticleTrack>(Guid);
	if(ParticleTrack)
	{
		MovieSceneSection = Cast<UMovieSceneParticleSection>(ParticleTrack->CreateNewSection());

		ParticleTrack->AddSection(*MovieSceneSection);

		MovieSceneSection->SetStartTime(Time);

		bWasTriggered = false;

		DelegateProxy = NewObject<UMovieSceneParticleTrackSectionRecorder>();
		DelegateProxy->SectionRecorder = this;
		DelegateProxy->AddToRoot();
		UParticleSystemComponent::OnSystemPreActivationChange.AddUObject(DelegateProxy.Get(), &UMovieSceneParticleTrackSectionRecorder::OnTriggered);
	}

	PreviousState = EParticleKey::Deactivate;
}

void FMovieSceneParticleTrackSectionRecorder::FinalizeSection()
{
}

void FMovieSceneParticleTrackSectionRecorder::Record(float CurrentTime)
{
	if(SystemToRecord.IsValid())
	{
		MovieSceneSection->SetEndTime(CurrentTime);

		EParticleKey::Type NewState = EParticleKey::Deactivate;
		if(SystemToRecord->IsRegistered() && SystemToRecord->IsActive() && !SystemToRecord->bWasDeactivated)
		{
			if(bWasTriggered)
			{
				NewState = EParticleKey::Trigger;
				bWasTriggered = false;
			}
			else
			{
				NewState = EParticleKey::Activate;
			}
		}
		else
		{
			NewState = EParticleKey::Deactivate;
		}

		if(NewState != PreviousState)
		{
			MovieSceneSection->AddKey(CurrentTime, NewState);
		}

		if(NewState == EParticleKey::Trigger)
		{
			NewState = EParticleKey::Activate;
		}
		PreviousState = NewState;
	}
}

void UMovieSceneParticleTrackSectionRecorder::OnTriggered(UParticleSystemComponent* Component, bool bActivating)
{ 
	if(SectionRecorder && SectionRecorder->SystemToRecord.Get() == Component)
	{
		SectionRecorder->bWasTriggered = bActivating;
	}
}
