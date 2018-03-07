// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Sound/SoundConcurrency.h"
#include "Components/AudioComponent.h"
#include "ActiveSound.h"
#include "AudioDevice.h"
#include "Sound/SoundBase.h"

/************************************************************************/
/* USoundConcurrency													*/
/************************************************************************/

USoundConcurrency::USoundConcurrency(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

/************************************************************************/
/* FConcurrencyActiveSounds												*/
/************************************************************************/

FConcurrencyGroup::FConcurrencyGroup()
	: MaxActiveSounds(16)
	, ConcurrencyGroupID(0)
	, ResolutionRule(EMaxConcurrentResolutionRule::StopFarthestThenPreventNew)
	, Generation(0)
{
}

FConcurrencyGroup::FConcurrencyGroup(FActiveSound* ActiveSound)
{
	static FConcurrencyGroupID ConcurrencyGroupIDs = 1;
	ConcurrencyGroupID = ConcurrencyGroupIDs++;

	check(ActiveSound);
	const FSoundConcurrencySettings* ConcurrencySettings = ActiveSound->GetSoundConcurrencySettingsToApply();
	check(ConcurrencySettings);

	MaxActiveSounds = ConcurrencySettings->MaxCount;
	ResolutionRule = ConcurrencySettings->ResolutionRule;
	Generation = 0;

	ActiveSounds.Add(ActiveSound);
	ActiveSound->ConcurrencyGroupID = ConcurrencyGroupID;
}

TArray<FActiveSound*>& FConcurrencyGroup::GetActiveSounds()
{
	return ActiveSounds;
}

void FConcurrencyGroup::AddActiveSound(FActiveSound* ActiveSound)
{
	check(ConcurrencyGroupID != 0);
	ActiveSounds.Add(ActiveSound);
	ActiveSound->ConcurrencyGroupID = ConcurrencyGroupID;
	ActiveSound->ConcurrencyGeneration = Generation++;
}

void FConcurrencyGroup::StopQuietSoundsDueToMaxConcurrency()
{
	// Nothing to do if our active sound count is less than or equal to our max active sounds
	if (ResolutionRule != EMaxConcurrentResolutionRule::StopQuietest || ActiveSounds.Num() <= MaxActiveSounds)
	{
		return;
	}

	// Helper function for sort this concurrency group's active sounds according to their "volume" concurrency
	// Quieter sounds will be at the front of the array
	struct FCompareActiveSounds
	{
		FORCEINLINE bool operator()(const FActiveSound& A, const FActiveSound& B) const
		{
			return A.VolumeConcurrency < B.VolumeConcurrency;
		}
	};

	ActiveSounds.Sort(FCompareActiveSounds());

	const int32 NumSoundsToStop = ActiveSounds.Num() - MaxActiveSounds;
	check(NumSoundsToStop > 0);

	// Need to make a new list when stopping the sounds since the process of stopping an active sound
	// will remove the sound from this concurrency group's ActiveSounds array.
	int32 i = 0;
	for (; i < NumSoundsToStop; ++i)
	{
		// Flag this active sound as needing to be stopped due to volume-based max concurrency.
		// This will actually be stopped in the audio device update function.
		ActiveSounds[i]->bShouldStopDueToMaxConcurrency = true;
	}

	const int32 NumActiveSounds = ActiveSounds.Num();
	for (; i < NumActiveSounds; ++i)
	{
		ActiveSounds[i]->bShouldStopDueToMaxConcurrency = false;
	}

}


/************************************************************************/
/* FSoundConcurrencyManager												*/
/************************************************************************/

FSoundConcurrencyManager::FSoundConcurrencyManager(class FAudioDevice* InAudioDevice)
	: AudioDevice(InAudioDevice)
{
}

FSoundConcurrencyManager::~FSoundConcurrencyManager()
{
}

FActiveSound* FSoundConcurrencyManager::CreateNewActiveSound(const FActiveSound& NewActiveSound)
{
	check(NewActiveSound.GetSound());
	
	// If there are no concurrency settings associated then there is no limit on this sound
	const FSoundConcurrencySettings* Concurrency = NewActiveSound.GetSoundConcurrencySettingsToApply();

	// If there was no concurrency or the setting was zero, then we will always play this sound.
	if (!Concurrency)
	{
		FActiveSound* ActiveSound = new FActiveSound(NewActiveSound);
		ActiveSound->SetAudioDevice(AudioDevice);
		return ActiveSound;
	}

	check(Concurrency->MaxCount > 0);

	uint32 ConcurrencyObjectID = NewActiveSound.GetSoundConcurrencyObjectID();
	if (ConcurrencyObjectID == 0)
	{
		return HandleConcurrencyEvaluationOverride(NewActiveSound);
	}
	else if (ConcurrencyObjectID != INDEX_NONE)
	{
		return HandleConcurrencyEvaluation(NewActiveSound);
	}
	return nullptr;
}

FActiveSound* FSoundConcurrencyManager::ResolveConcurrency(const FActiveSound& NewActiveSound, FConcurrencyGroup& ConcurrencyGroup)
{
	TArray<FActiveSound*>& ActiveSounds = ConcurrencyGroup.GetActiveSounds();
	const FSoundConcurrencySettings* Concurrency = NewActiveSound.GetSoundConcurrencySettingsToApply();
	check(Concurrency && Concurrency->MaxCount > 0);

	check(AudioDevice);
	TArray<struct FListener>& Listeners = AudioDevice->Listeners;

	bool bCanPlay = true;
	FActiveSound* SoundToStop = nullptr;

	if (ActiveSounds.Num() >= Concurrency->MaxCount && Concurrency->ResolutionRule != EMaxConcurrentResolutionRule::StopQuietest)
	{
		switch (Concurrency->ResolutionRule)
		{
			case EMaxConcurrentResolutionRule::PreventNew:
			bCanPlay = false;
			break;

			case EMaxConcurrentResolutionRule::StopOldest:
			{
				for (int32 SoundIndex = 0; SoundIndex < ActiveSounds.Num(); ++SoundIndex)
				{
					FActiveSound* ActiveSound = ActiveSounds[SoundIndex];
					if (SoundToStop == nullptr || ActiveSound->PlaybackTime > SoundToStop->PlaybackTime)
					{
						SoundToStop = ActiveSound;
					}
				}
			}
			break;

			case EMaxConcurrentResolutionRule::StopFarthestThenPreventNew:
			case EMaxConcurrentResolutionRule::StopFarthestThenOldest:
			{
				int32 ClosestListenerIndex = NewActiveSound.FindClosestListener(Listeners);
				float DistanceToStopSoundSq = FVector::DistSquared(Listeners[ClosestListenerIndex].Transform.GetTranslation(), NewActiveSound.Transform.GetTranslation());

				for (FActiveSound* ActiveSound : ActiveSounds)
				{
					ClosestListenerIndex = ActiveSound->FindClosestListener(Listeners);
					const float DistanceToActiveSoundSq = FVector::DistSquared(Listeners[ClosestListenerIndex].Transform.GetTranslation(), ActiveSound->Transform.GetTranslation());

					if (DistanceToActiveSoundSq > DistanceToStopSoundSq)
					{
						SoundToStop = ActiveSound;
						DistanceToStopSoundSq = DistanceToActiveSoundSq;
					}
					else if (Concurrency->ResolutionRule == EMaxConcurrentResolutionRule::StopFarthestThenOldest
								&& DistanceToActiveSoundSq == DistanceToStopSoundSq
								&& (SoundToStop == nullptr || ActiveSound->PlaybackTime > SoundToStop->PlaybackTime))
					{
						SoundToStop = ActiveSound;
						DistanceToStopSoundSq = DistanceToActiveSoundSq;
					}
				}
			}
			break;

			case EMaxConcurrentResolutionRule::StopLowestPriority:
			case EMaxConcurrentResolutionRule::StopLowestPriorityThenPreventNew:
			{
				for (FActiveSound* CurrSound : ActiveSounds)
				{
					// This will find oldest and oldest lowest priority sound in the group
					if (SoundToStop == nullptr 
						|| (CurrSound->GetPriority() < SoundToStop->GetPriority())
						|| (CurrSound->GetPriority() == SoundToStop->GetPriority() && CurrSound->PlaybackTime > SoundToStop->PlaybackTime))
					{
						SoundToStop = CurrSound;
					}
				}

				if (SoundToStop)
				{
					// Only stop any sounds if the *lowest* priority is lower than the incoming NewActiveSound
					if (SoundToStop->GetPriority() > NewActiveSound.GetPriority())
					{
						SoundToStop = nullptr;
					}
					else if (Concurrency->ResolutionRule == EMaxConcurrentResolutionRule::StopLowestPriorityThenPreventNew 
						&& SoundToStop->GetPriority() == NewActiveSound.GetPriority())
					{
						SoundToStop = nullptr;
					}
				}

			}
			break;

			case EMaxConcurrentResolutionRule::StopQuietest:
			// We won't do anything upfront for StopQuietest resolution rule, but instead resolve it after volumes have been computed
			// and before we stopping sounds that play past the max overall voice count. This is because it is nigh impossible (currently)
			// for UE4 to evaluate sound volumes *before* they play. In addition to the fact that gain-stages aren't evaluated independently, 
			// this is primarily because of the way SoundCue nodes work.
			check(false);
			break;

			default:
			checkf(false, TEXT("Unknown EMaxConcurrentResolutionRule enumeration."));
			break;
		}

		// If we found a sound to stop, then stop it and allow the new sound to play.  
		// Otherwise inform the system that the sound failed to start.
		if (SoundToStop == nullptr)
		{
			// If we didn't find any sound to stop, then we're not going to play this sound, so 
			// immediately do the playback complete notify
			bCanPlay = false;

			if (NewActiveSound.GetAudioComponentID() > 0)
			{
				UAudioComponent::PlaybackCompleted(NewActiveSound.GetAudioComponentID(), true);
			}
		}
	}

	if (bCanPlay)
	{
		// Make a new active sound
		FActiveSound* OutActiveSound = MakeNewActiveSound(NewActiveSound);
		check(OutActiveSound);

		// If we're ducking older sounds in the concurrency group, then loop through each sound in the concurrency group
		// and update their duck amount based on each sound's generation and the next generation count. The older the sound, the more ducking.
		if (Concurrency->VolumeScale < 1.0f)
		{
			check(Concurrency->VolumeScale >= 0.0f);

			ActiveSounds = ConcurrencyGroup.GetActiveSounds();

			int32 NextGeneration = ConcurrencyGroup.GetGeneration() + 1;

			for (FActiveSound* ActiveSound : ActiveSounds)
			{
				const int32 ActiveSoundGeneration = ActiveSound->ConcurrencyGeneration;
				const float GenerationDelta = (float)(NextGeneration - ActiveSoundGeneration);
				ActiveSound->ConcurrencyVolumeScale = FMath::Pow(Concurrency->VolumeScale, GenerationDelta);
			}
		}

		// And add it to to the concurrency group. This automatically updates generation counts.
		ConcurrencyGroup.AddActiveSound(OutActiveSound);

		// Stop any sounds now if needed
		if (SoundToStop)
		{
			check(AudioDevice == SoundToStop->AudioDevice);

			// Remove the active sound from the concurrency manager immediately so it doesn't count towards 
			// subsequent concurrency resolution checks (i.e. if sounds are triggered multiple times in this frame)
			RemoveActiveSound(SoundToStop);

			// Add this sound to list of sounds that need to stop but don't stop it immediately
			AudioDevice->AddSoundToStop(SoundToStop);
		}

		return OutActiveSound;
	}

	return nullptr;
}

FActiveSound* FSoundConcurrencyManager::HandleConcurrencyEvaluationOverride(const FActiveSound& NewActiveSound)
{
	check(NewActiveSound.GetSound());
	const FSoundConcurrencySettings* ConcurrencySettings = NewActiveSound.GetSoundConcurrencySettingsToApply();
	check(ConcurrencySettings);

	const uint32 OwnerObjectID = NewActiveSound.GetOwnerID();
	FSoundObjectID SoundObjectID = NewActiveSound.GetSound()->GetUniqueID();

	FActiveSound* ActiveSound = nullptr;

	// First check to see if we're limiting the sound per-owner (i.e. told to limit per owner and we have a valid object id)
	if (ConcurrencySettings->bLimitToOwner && OwnerObjectID != 0)
	{
		// Get the play count for the owner
		FSoundInstanceEntry* OwnerPerSoundEntry = OwnerPerSoundConcurrencyMap.Find(OwnerObjectID);

		// If we have an entry for this owner
		if (OwnerPerSoundEntry)
		{
			// Check if we already have these sounds playing on this owner
			const FConcurrencyGroupID* ConcurrencyGroupID = OwnerPerSoundEntry->SoundInstanceToConcurrencyGroup.Find(SoundObjectID);
			if (ConcurrencyGroupID)
			{
				// Get the concurrency group
				check(*ConcurrencyGroupID != 0);
				FConcurrencyGroup* ConcurrencyGroup = ConcurrencyGroups.Find(*ConcurrencyGroupID);
				check(ConcurrencyGroup);

				// And perform the concurrency resolution
				ActiveSound = ResolveConcurrency(NewActiveSound, *ConcurrencyGroup);
			}
			else
			{
				// If no sounds are playing on this owner, then make a new group and active sound
				FConcurrencyGroupID NewConcurrencyGroupID = MakeNewConcurrencyGroupAndSound(&ActiveSound, NewActiveSound);

				// Adding the new concurrency group ID to the owner for this sound id
				OwnerPerSoundEntry->SoundInstanceToConcurrencyGroup.Add(SoundObjectID, NewConcurrencyGroupID);
			}
		}
		else
		{
			// If we don't have an entry for this sound owner, then create a new entry with a single count for the instance object id
			FConcurrencyGroupID NewConcurrencyGroupID = MakeNewConcurrencyGroupAndSound(&ActiveSound, NewActiveSound);
			OwnerPerSoundConcurrencyMap.Add(OwnerObjectID, FSoundInstanceEntry(SoundObjectID, NewConcurrencyGroupID));
		}
	}
	else
	{
		// If we're not limiting the concurrency to per-owner, then we need to limit concurrency on sound instances that are playing globally
		FConcurrencyGroupID* ConcurrencyGroupID = SoundObjectToActiveSounds.Find(SoundObjectID);

		if (ConcurrencyGroupID)
		{
			// Get the concurrency group
			check(*ConcurrencyGroupID != 0);
			FConcurrencyGroup* ConcurrencyGroup = ConcurrencyGroups.Find(*ConcurrencyGroupID);
			check(ConcurrencyGroup);

			ActiveSound = ResolveConcurrency(NewActiveSound, *ConcurrencyGroup);
		}
		else
		{
			FConcurrencyGroupID NewConcurrencyGroupID = MakeNewConcurrencyGroupAndSound(&ActiveSound, NewActiveSound);
			SoundObjectToActiveSounds.Add(SoundObjectID, NewConcurrencyGroupID);
		}
	}

	// If the sound wasn't able to play, this will be nullptr
	return ActiveSound;
}

FActiveSound* FSoundConcurrencyManager::HandleConcurrencyEvaluation(const FActiveSound& NewActiveSound)
{
	check(NewActiveSound.GetSound());
	const FSoundConcurrencySettings* ConcurrencySettings = NewActiveSound.GetSoundConcurrencySettingsToApply();
	check(ConcurrencySettings);

	const uint32 OwnerObjectID = NewActiveSound.GetOwnerID();
	FConcurrencyObjectID ConcurrencyObjectID = NewActiveSound.GetSoundConcurrencyObjectID();
	check(ConcurrencyObjectID != 0);

	// If there is no concurrency object ID, then there is no sound to play, so don't try to play
	if (ConcurrencyObjectID == INDEX_NONE)
	{
		return nullptr;
	}

	FActiveSound* ActiveSound = nullptr;

	// Check to see if we're limiting per owner
	if (ConcurrencySettings->bLimitToOwner && OwnerObjectID != 0)
	{
		// Get the map of owner to concurrency map instances
		FOwnerConcurrencyMapEntry* ConcurrencyEntry = OwnerConcurrencyMap.Find(OwnerObjectID);
		if (ConcurrencyEntry)
		{
			// Now get the map of active sounds to the concurrency object
			FConcurrencyGroupID* ConcurrencyGroupID = ConcurrencyEntry->ConcurrencyObjectToConcurrencyGroup.Find(ConcurrencyObjectID);
			if (ConcurrencyGroupID)
			{
				// Get the concurrency group
				check(*ConcurrencyGroupID != 0);
				FConcurrencyGroup* ConcurrencyGroup = ConcurrencyGroups.Find(*ConcurrencyGroupID);
				check(ConcurrencyGroup);

				ActiveSound = ResolveConcurrency(NewActiveSound, *ConcurrencyGroup);
			}
			else
			{
				FConcurrencyGroupID NewConcurrencyGroupID = MakeNewConcurrencyGroupAndSound(&ActiveSound, NewActiveSound);
				ConcurrencyEntry->ConcurrencyObjectToConcurrencyGroup.Add(ConcurrencyObjectID, NewConcurrencyGroupID);
			}
		}
		else
		{
			FConcurrencyGroupID NewConcurrencyGroupID = MakeNewConcurrencyGroupAndSound(&ActiveSound, NewActiveSound);
			OwnerConcurrencyMap.Add(OwnerObjectID, FOwnerConcurrencyMapEntry(ConcurrencyObjectID, NewConcurrencyGroupID));
		}
	}
	else
	{
		// If not limiting per owner, then we need to globally limit concurrency of sounds that are playing with this concurrency object ID
		FConcurrencyGroupID* ConcurrencyGroupID = ConcurrencyMap.Find(ConcurrencyObjectID);
		if (ConcurrencyGroupID)
		{
			check(*ConcurrencyGroupID != 0);
			FConcurrencyGroup* ConcurrencyGroup = ConcurrencyGroups.Find(*ConcurrencyGroupID);
			check(ConcurrencyGroup);

			ActiveSound = ResolveConcurrency(NewActiveSound, *ConcurrencyGroup);
		}
		else
		{
			FConcurrencyGroupID NewConcurrencyGroupID = MakeNewConcurrencyGroupAndSound(&ActiveSound, NewActiveSound);
			ConcurrencyMap.Add(ConcurrencyObjectID, NewConcurrencyGroupID);
		}
	}

	// If the sound wasn't able to play, this will be nullptr
	return ActiveSound;
}

FConcurrencyGroupID FSoundConcurrencyManager::MakeNewConcurrencyGroupAndSound(FActiveSound** OutActiveSound, const FActiveSound& NewActiveSound)
{
	// First make a new active sound
	*OutActiveSound = MakeNewActiveSound(NewActiveSound);
	check(*OutActiveSound);

	const FSoundConcurrencySettings* ConcurrencySettings = NewActiveSound.GetSoundConcurrencySettingsToApply();
	check(ConcurrencySettings);

	// Make the new group
	FConcurrencyGroup ConcurrencyGroup(*OutActiveSound);

	// Add it to the concurrency group map
	ConcurrencyGroups.Add(ConcurrencyGroup.GetID(), MoveTemp(ConcurrencyGroup));

	return ConcurrencyGroup.GetID();
}

void FSoundConcurrencyManager::RemoveActiveSound(FActiveSound* ActiveSound)
{
	if (!ActiveSound->ConcurrencyGroupID)
	{
		return;
	}

	// Remove this sound from it's concurrency list
	FConcurrencyGroup* ConcurrencyGroup = ConcurrencyGroups.Find(ActiveSound->ConcurrencyGroupID);
	check(ConcurrencyGroup);

	TArray<FActiveSound*>& ActiveSounds = ConcurrencyGroup->GetActiveSounds();
	check(ActiveSounds.Num() > 0);
	ActiveSounds.Remove(ActiveSound);

	// If we've no longer got any sounds playing in the concurrency group
	if (!ActiveSounds.Num())
	{
		// Remove the concurrency group id from the list of concurrency groups (no more sounds playing in the concurrency group)
		ConcurrencyGroups.Remove(ActiveSound->ConcurrencyGroupID);

		const uint32 ConcurrencyObjectID = ActiveSound->GetSoundConcurrencyObjectID();
		const uint32 OwnerObjectID = ActiveSound->GetOwnerID();
		const FSoundConcurrencySettings* ConcurrencySettings = ActiveSound->GetSoundConcurrencySettingsToApply();

		if (ConcurrencySettings)
		{
			// If 0, that means we're in override mode (i.e. concurrency is limited per-sound instance, not per-group)
			if (ConcurrencyObjectID == 0)
			{
				// Get the sounds unique ID
				const FSoundObjectID SoundObjectID = ActiveSound->GetSound()->GetUniqueID();

				// If we're limiting to owner, we need to clean up the per-owner record keeping
				if (ConcurrencySettings->bLimitToOwner && OwnerObjectID != 0)
				{
					FSoundInstanceEntry* OwnerPerSoundEntry = OwnerPerSoundConcurrencyMap.Find(OwnerObjectID);
					OwnerPerSoundEntry->SoundInstanceToConcurrencyGroup.Remove(SoundObjectID);

					if (!OwnerPerSoundEntry->SoundInstanceToConcurrencyGroup.Num())
					{
						OwnerPerSoundConcurrencyMap.Remove(OwnerObjectID);
					}
				}
				else
				{
					// If we're not limiting per-owner, we need to clean up the global map of per-sound concurrency
					SoundObjectToActiveSounds.Remove(SoundObjectID);
				}
			}
			else if (ConcurrencyObjectID != INDEX_NONE) // We're limiting concurrency per-group (not per-instance)
			{
				// Check if we're doing a per-owner concurrency group
				if (ConcurrencySettings->bLimitToOwner && OwnerObjectID != 0)
				{
					FOwnerConcurrencyMapEntry* ConcurrencyEntry = OwnerConcurrencyMap.Find(OwnerObjectID);
					ConcurrencyEntry->ConcurrencyObjectToConcurrencyGroup.Remove(ConcurrencyObjectID);

					if (!ConcurrencyEntry->ConcurrencyObjectToConcurrencyGroup.Num())
					{
						OwnerConcurrencyMap.Remove(OwnerObjectID);
					}
				}
				else
				{
					// Just remove the map entry that maps concurrency object ID to concurrency group
					ConcurrencyMap.Remove(ConcurrencyObjectID);
				}
			}
		}
	}
}

FActiveSound* FSoundConcurrencyManager::MakeNewActiveSound(const FActiveSound& NewActiveSound)
{
	FActiveSound* ActiveSound = new FActiveSound(NewActiveSound);
	ActiveSound->SetAudioDevice(AudioDevice);
	check(AudioDevice == ActiveSound->AudioDevice);
	return ActiveSound;
}

void FSoundConcurrencyManager::StopQuietSoundsDueToMaxConcurrency()
{
	for (auto& ConcurrenyGroupEntry : ConcurrencyGroups)
	{
		ConcurrenyGroupEntry.Value.StopQuietSoundsDueToMaxConcurrency();
	}
}
