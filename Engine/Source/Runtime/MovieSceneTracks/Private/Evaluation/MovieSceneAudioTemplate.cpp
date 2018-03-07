// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Evaluation/MovieSceneAudioTemplate.h"


#include "Engine/EngineTypes.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundCue.h"
#include "GameFramework/Actor.h"
#include "AudioThread.h"
#include "AudioDevice.h"
#include "ActiveSound.h"
#include "Sections/MovieSceneAudioSection.h"
#include "Tracks/MovieSceneAudioTrack.h"
#include "MovieScene.h"
#include "MovieSceneEvaluation.h"
#include "IMovieScenePlayer.h"


DECLARE_CYCLE_STAT(TEXT("Audio Track Evaluate"), MovieSceneEval_AudioTrack_Evaluate, STATGROUP_MovieSceneEval);
DECLARE_CYCLE_STAT(TEXT("Audio Track Token Execute"), MovieSceneEval_AudioTrack_TokenExecute, STATGROUP_MovieSceneEval);


/** Stop audio from playing */
struct FStopAudioPreAnimatedToken : IMovieScenePreAnimatedToken
{
	static FMovieSceneAnimTypeID GetAnimTypeID()
	{
		return TMovieSceneAnimTypeID<FStopAudioPreAnimatedToken>();
	}

	virtual void RestoreState(UObject& InObject, IMovieScenePlayer& Player) override
	{
		UAudioComponent* AudioComponent = CastChecked<UAudioComponent>(&InObject);
		if (AudioComponent)
		{
			AudioComponent->Stop();
			AudioComponent->DestroyComponent();
		}
	}

	struct FProducer : IMovieScenePreAnimatedTokenProducer
	{
		virtual IMovieScenePreAnimatedTokenPtr CacheExistingState(UObject& Object) const override
		{
			return FStopAudioPreAnimatedToken();
		}
	};
};

/** Destroy a transient audio component */
struct FDestroyAudioPreAnimatedToken : IMovieScenePreAnimatedToken
{
	static FMovieSceneAnimTypeID GetAnimTypeID()
	{
		return TMovieSceneAnimTypeID<FDestroyAudioPreAnimatedToken>();
	}

	virtual void RestoreState(UObject& InObject, IMovieScenePlayer& Player) override
	{
		UAudioComponent* AudioComponent = CastChecked<UAudioComponent>(&InObject);
		if (AudioComponent)
		{
			AudioComponent->DestroyComponent();
		}
	}

	struct FProducer : IMovieScenePreAnimatedTokenProducer
	{
		virtual IMovieScenePreAnimatedTokenPtr CacheExistingState(UObject& Object) const override
		{
			return FDestroyAudioPreAnimatedToken();
		}
	};
};

struct FCachedAudioTrackData : IPersistentEvaluationData
{
	TArray<TMap<FObjectKey, TWeakObjectPtr<UAudioComponent>>> AudioComponentsByRow;

	UAudioComponent* GetAudioComponentForRow(int32 RowIndex, FObjectKey Key)
	{
		if (AudioComponentsByRow.Num() > RowIndex)
		{
			return AudioComponentsByRow[RowIndex].FindRef(Key).Get();
		}

		return nullptr;
	}

	/** Only to be called on the game thread */
	UAudioComponent* AddAudioComponentForRow(int32 RowIndex, AActor& PrincipalActor, IMovieScenePlayer& Player)
	{
		const int32 NumExtra = RowIndex + 1 - AudioComponentsByRow.Num();
		if (NumExtra > 0)
		{
			AudioComponentsByRow.AddDefaulted(NumExtra);
		}

		FObjectKey ActorKey(&PrincipalActor);
		UAudioComponent* ExistingComponent = AudioComponentsByRow[RowIndex].FindRef(ActorKey).Get();
		if (!ExistingComponent)
		{
			USoundCue* TempPlaybackAudioCue = NewObject<USoundCue>();

			FAudioDevice::FCreateComponentParams Params(PrincipalActor.GetWorld(), &PrincipalActor);
			ExistingComponent = FAudioDevice::CreateComponent(TempPlaybackAudioCue, Params);

			if (!ExistingComponent)
			{
					FString ActorName = 
#if WITH_EDITOR
						PrincipalActor.GetActorLabel();
#else
						PrincipalActor.GetName();
#endif
				UE_LOG(LogMovieScene, Warning, TEXT("Failed to create audio component for spatialized audio track (row %d on %s)."), RowIndex, *ActorName);
				return nullptr;
			}

			Player.SavePreAnimatedState(*ExistingComponent, FMovieSceneAnimTypeID::Unique(), FDestroyAudioPreAnimatedToken::FProducer());

			AudioComponentsByRow[RowIndex].Add(ActorKey, ExistingComponent);

			ExistingComponent->SetFlags(RF_Transient);
			ExistingComponent->AttachToComponent(PrincipalActor.GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
		}

		return ExistingComponent;
	}

	/** Only to be called on the game thread */
	UAudioComponent* AddMasterAudioComponentForRow(int32 RowIndex, UWorld* World, IMovieScenePlayer& Player)
	{
		const int32 NumExtra = RowIndex + 1 - AudioComponentsByRow.Num();
		if (NumExtra > 0)
		{
			AudioComponentsByRow.AddDefaulted(NumExtra);
		}

		UAudioComponent* ExistingComponent = AudioComponentsByRow[RowIndex].FindRef(FObjectKey()).Get();
		if (!ExistingComponent)
		{
			USoundCue* TempPlaybackAudioCue = NewObject<USoundCue>();
			ExistingComponent = FAudioDevice::CreateComponent(TempPlaybackAudioCue, FAudioDevice::FCreateComponentParams(World));

			if (!ExistingComponent)
			{
				UE_LOG(LogMovieScene, Warning, TEXT("Failed to create audio component for master audio track (row %d)."), RowIndex);
				return nullptr;
			}

			Player.SavePreAnimatedState(*ExistingComponent, FMovieSceneAnimTypeID::Unique(), FDestroyAudioPreAnimatedToken::FProducer());
			
			ExistingComponent->SetFlags(RF_Transient);
			AudioComponentsByRow[RowIndex].Add(FObjectKey(), ExistingComponent);
		}

		return ExistingComponent;
	}

	void StopAllSounds()
	{
		for (TMap<FObjectKey, TWeakObjectPtr<UAudioComponent>>& Map : AudioComponentsByRow)
		{
			for (auto& Pair : Map)
			{
				if (UAudioComponent* AudioComponent = Pair.Value.Get())
				{
					AudioComponent->Stop();
				}
			}
		}
	}
};


struct FAudioSectionExecutionToken : IMovieSceneExecutionToken
{
	FAudioSectionExecutionToken(const FMovieSceneAudioSectionTemplateData& InAudioData)
		: AudioData(InAudioData)
	{}

	virtual void Execute(const FMovieSceneContext& Context, const FMovieSceneEvaluationOperand& Operand, FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player)
	{
		FCachedAudioTrackData& TrackData = PersistentData.GetOrAddTrackData<FCachedAudioTrackData>();

		if ((Context.GetStatus() != EMovieScenePlayerStatus::Playing && Context.GetStatus() != EMovieScenePlayerStatus::Scrubbing) || Context.HasJumped())
		{
			// stopped, recording, etc
			TrackData.StopAllSounds();
		}

		// Master audio track
		else if (!Operand.ObjectBindingID.IsValid())
		{
			UObject* PlaybackContext = Player.GetPlaybackContext();

			UAudioComponent* AudioComponent = TrackData.GetAudioComponentForRow(AudioData.RowIndex, FObjectKey());
			if (!AudioComponent)
			{
				// Initialize the sound
				AudioComponent = TrackData.AddMasterAudioComponentForRow(AudioData.RowIndex, PlaybackContext ? PlaybackContext->GetWorld() : nullptr, Player);

				if (AudioComponent)
				{
					if (AudioData.OnQueueSubtitles.IsBound())
					{
						AudioComponent->OnQueueSubtitles = AudioData.OnQueueSubtitles;
					}
					if (AudioData.OnAudioFinished.IsBound())
					{
						AudioComponent->OnAudioFinished = AudioData.OnAudioFinished;
					}
					if(AudioData.OnAudioPlaybackPercent.IsBound())
					{
						AudioComponent->OnAudioPlaybackPercent = AudioData.OnAudioPlaybackPercent;
					}
				}
			}

			if (AudioComponent)
			{
				AudioData.EnsureAudioIsPlaying(*AudioComponent, PersistentData, Context, false, Player);
			}
		}

		// Object binding audio track
		else
		{
			for (TWeakObjectPtr<> Object : Player.FindBoundObjects(Operand))
			{
				AActor* Actor = Cast<AActor>(Object.Get());
				if (!Actor)
				{
					continue;
				}

				UAudioComponent* AudioComponent = TrackData.GetAudioComponentForRow(AudioData.RowIndex, Actor);
				if (!AudioComponent)
				{
					// Initialize the sound
					AudioComponent = TrackData.AddAudioComponentForRow(AudioData.RowIndex, *Actor, Player);

					if (AudioComponent)
					{
						if (AudioData.OnQueueSubtitles.IsBound())
						{
							AudioComponent->OnQueueSubtitles = AudioData.OnQueueSubtitles;
						}
						if (AudioData.OnAudioFinished.IsBound())
						{
							AudioComponent->OnAudioFinished = AudioData.OnAudioFinished;
						}
						if (AudioData.OnAudioPlaybackPercent.IsBound())
						{
							AudioComponent->OnAudioPlaybackPercent = AudioData.OnAudioPlaybackPercent;
						}
					}
				}
				
				if (AudioComponent)
				{
					AudioData.EnsureAudioIsPlaying(*AudioComponent, PersistentData, Context, true, Player);
				}
			}
		}
	}

	FMovieSceneAudioSectionTemplateData AudioData;
};

FMovieSceneAudioSectionTemplateData::FMovieSceneAudioSectionTemplateData(const UMovieSceneAudioSection& Section)
	: Sound(Section.GetSound())
	, AudioStartOffset(Section.GetStartOffset())
	, AudioRange(Section.GetAudioRange())
	, AudioPitchMultiplierCurve(Section.GetPitchMultiplierCurve())
	, AudioVolumeCurve(Section.GetSoundVolumeCurve())
	, RowIndex(Section.GetRowIndex())
	, bOverrideAttenuation(Section.GetOverrideAttenuation())
	, AttenuationSettings(Section.GetAttenuationSettings())
	, OnQueueSubtitles(Section.GetOnQueueSubtitles())
	, OnAudioFinished(Section.GetOnAudioFinished())
	, OnAudioPlaybackPercent(Section.GetOnAudioPlaybackPercent())
{
}

void FMovieSceneAudioSectionTemplateData::EnsureAudioIsPlaying(UAudioComponent& AudioComponent, FPersistentEvaluationData& PersistentData, const FMovieSceneContext& Context, bool bAllowSpatialization, IMovieScenePlayer& Player) const
{
	Player.SavePreAnimatedState(AudioComponent, FStopAudioPreAnimatedToken::GetAnimTypeID(), FStopAudioPreAnimatedToken::FProducer());

	bool bPlaySound = !AudioComponent.IsPlaying() || AudioComponent.Sound != Sound;
	float AudioVolume = AudioVolumeCurve.Eval(Context.GetTime());
	float PitchMultiplier = AudioPitchMultiplierCurve.Eval(Context.GetTime());

	if (AudioComponent.VolumeMultiplier != AudioVolume)
	{
		AudioComponent.SetVolumeMultiplier(AudioVolume);
	}
	
	if (AudioComponent.PitchMultiplier != PitchMultiplier)
	{
		AudioComponent.SetPitchMultiplier(PitchMultiplier);
	}

	if (bPlaySound)
	{
		AudioComponent.bAllowSpatialization = bAllowSpatialization;
	
		if (bOverrideAttenuation)
		{
			AudioComponent.AttenuationSettings = AttenuationSettings;
		}
		
		AudioComponent.Stop();
		AudioComponent.SetSound(Sound);
#if WITH_EDITOR
		UObject* PlaybackContext = Player.GetPlaybackContext();
		UWorld* World = PlaybackContext ? PlaybackContext->GetWorld() : nullptr;
		if (GIsEditor && World != nullptr && !World->IsPlayInEditor())
		{
			AudioComponent.bIsUISound = true;
			AudioComponent.bIsPreviewSound = true;
		}
		else
#endif // WITH_EDITOR
		{
			AudioComponent.bIsUISound = false;
		}

		AudioComponent.Play((Context.GetTime() - AudioRange.GetLowerBoundValue()) + FMath::Max(AudioStartOffset, 0.f));

		if (Context.GetStatus() == EMovieScenePlayerStatus::Scrubbing)
		{
			// Fade out the sound at the same volume in order to simply
			// set a short duration on the sound, far from ideal soln
			AudioComponent.FadeOut(AudioTrackConstants::ScrubDuration, 1.f);
		}
	}

	if (bAllowSpatialization)
	{
		if (FAudioDevice* AudioDevice = AudioComponent.GetAudioDevice())
		{
			DECLARE_CYCLE_STAT(TEXT("FAudioThreadTask.MovieSceneUpdateAudioTransform"), STAT_MovieSceneUpdateAudioTransform, STATGROUP_TaskGraphTasks);

			const FTransform ActorTransform = AudioComponent.GetOwner()->GetTransform();
			const uint64 ActorComponentID = AudioComponent.GetAudioComponentID();
			FAudioThread::RunCommandOnAudioThread([AudioDevice, ActorComponentID, ActorTransform]()
			{
				if (FActiveSound* ActiveSound = AudioDevice->FindActiveSound(ActorComponentID))
				{
					ActiveSound->bLocationDefined = true;
					ActiveSound->Transform = ActorTransform;
				}
			}, GET_STATID(STAT_MovieSceneUpdateAudioTransform));
		}
	}
}

FMovieSceneAudioSectionTemplate::FMovieSceneAudioSectionTemplate(const UMovieSceneAudioSection& Section)
	: AudioData(Section)
{
}


void FMovieSceneAudioSectionTemplate::Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const
{
	MOVIESCENE_DETAILED_SCOPE_CYCLE_COUNTER(MovieSceneEval_AudioTrack_Evaluate)

	if (GEngine && GEngine->UseSound() && Context.GetStatus() != EMovieScenePlayerStatus::Jumping)
	{
		ExecutionTokens.Add(FAudioSectionExecutionToken(AudioData));
	}
}
