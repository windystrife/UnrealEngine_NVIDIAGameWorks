// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ActiveSound.h"
#include "EngineDefines.h"
#include "Misc/App.h"
#include "AudioThread.h"
#include "AudioDevice.h"
#include "Sound/SoundCue.h"
#include "Sound/SoundWave.h"
#include "Sound/SoundNodeAttenuation.h"
#include "SubtitleManager.h"
#include "DSP/Dsp.h"

FTraceDelegate FActiveSound::ActiveSoundTraceDelegate;
TMap<FTraceHandle, FActiveSound::FAsyncTraceDetails> FActiveSound::TraceToActiveSoundMap;

FActiveSound::FActiveSound()
	: World(nullptr)
	, WorldID(0)
	, Sound(nullptr)
	, AudioComponentID(0)
	, OwnerID(0)
	, AudioDevice(nullptr)
	, ConcurrencyGroupID(0)
	, ConcurrencyGeneration(0)
	, ConcurrencySettings(nullptr)
	, SoundClassOverride(nullptr)
	, SoundSubmixOverride(nullptr)
	, bHasCheckedOcclusion(false)
	, bAllowSpatialization(true)
	, bHasAttenuationSettings(false)
	, bShouldRemainActiveIfDropped(false)
	, bFadingOut(false)
	, bFinished(false)
	, bIsPaused(false)
	, bShouldStopDueToMaxConcurrency(false)
	, bRadioFilterSelected(false)
	, bApplyRadioFilter(false)
	, bHandleSubtitles(true)
	, bHasExternalSubtitles(false)
	, bLocationDefined(false)
	, bIgnoreForFlushing(false)
	, bEQFilterApplied(false)
	, bAlwaysPlay(false)
	, bIsUISound(false)
	, bIsMusic(false)
	, bReverb(false)
	, bCenterChannelOnly(false)
	, bIsPreviewSound(false)
	, bGotInteriorSettings(false)
	, bApplyInteriorVolumes(false)
#if !(NO_LOGGING || UE_BUILD_SHIPPING || UE_BUILD_TEST)
	, bWarnedAboutOrphanedLooping(false)
#endif
	, bEnableLowPassFilter(false)
	, bUpdatePlayPercentage(false)
	, UserIndex(0)
	, bIsOccluded(false)
	, bAsyncOcclusionPending(false)
	, PlaybackTime(0.f)
	, RequestedStartTime(0.f)
	, CurrentAdjustVolumeMultiplier(1.f)
	, TargetAdjustVolumeMultiplier(1.f)
	, TargetAdjustVolumeStopTime(-1.f)
	, VolumeMultiplier(1.f)
	, PitchMultiplier(1.f)
	, LowPassFilterFrequency(MAX_FILTER_FREQUENCY)
	, CurrentOcclusionFilterFrequency(MAX_FILTER_FREQUENCY)
	, CurrentOcclusionVolumeAttenuation(1.0f)
	, ConcurrencyVolumeScale(1.f)
	, ConcurrencyDuckingVolumeScale(1.f)
	, SubtitlePriority(DEFAULT_SUBTITLE_PRIORITY)
	, Priority(1.0f)
	, FocusPriorityScale(1.0f)
	, FocusDistanceScale(1.0f)
	, VolumeConcurrency(0.0f)
	, OcclusionCheckInterval(0.f)
	, LastOcclusionCheckTime(TNumericLimits<float>::Lowest())
	, MaxDistance(WORLD_MAX)
	, Azimuth(0.0f)
	, AbsoluteAzimuth(0.0f)
	, LastLocation(FVector::ZeroVector)
	, AudioVolumeID(0)
	, LastUpdateTime(0.f)
	, SourceInteriorVolume(1.f)
	, SourceInteriorLPF(MAX_FILTER_FREQUENCY)
	, CurrentInteriorVolume(1.f)
	, CurrentInteriorLPF(MAX_FILTER_FREQUENCY)
	, ClosestListenerPtr(nullptr)
	, InternalFocusFactor(1.0f)
{
	if (!ActiveSoundTraceDelegate.IsBound())
	{
		ActiveSoundTraceDelegate.BindStatic(&OcclusionTraceDone);
	}
}

FActiveSound::~FActiveSound()
{
	ensureMsgf(WaveInstances.Num() == 0, TEXT("Destroyed an active sound that had active wave instances."));
	check(CanDelete());
}

FArchive& operator<<( FArchive& Ar, FActiveSound* ActiveSound )
{
	if( !Ar.IsLoading() && !Ar.IsSaving() )
	{
		Ar << ActiveSound->Sound;
		Ar << ActiveSound->WaveInstances;
		Ar << ActiveSound->SoundNodeOffsetMap;
	}
	return( Ar );
}

void FActiveSound::AddReferencedObjects( FReferenceCollector& Collector)
{
	for (auto WaveInstanceIt(WaveInstances.CreateConstIterator()); WaveInstanceIt; ++WaveInstanceIt)
	{
		FWaveInstance* WaveInstance = WaveInstanceIt.Value();
		// Avoid recursing back to the wave instance that sourced this active sound
		if( WaveInstance )
		{
			WaveInstance->AddReferencedObjects( Collector );
		}
	}

	Collector.AddReferencedObject(Sound);
	Collector.AddReferencedObject(SoundClassOverride);
	Collector.AddReferencedObject(ConcurrencySettings);

	for (FAudioComponentParam& Param : InstanceParameters)
	{
		if (Param.SoundWaveParam)
		{
			Collector.AddReferencedObject(Param.SoundWaveParam);
		}
	}
}

void FActiveSound::SetWorld(UWorld* InWorld)
{
	check(IsInGameThread());

	World = InWorld;
	WorldID = (InWorld ? InWorld->GetUniqueID() : 0);
}

void FActiveSound::SetSound(USoundBase* InSound)
{
	check(IsInGameThread());

	Sound = InSound;
	bApplyInteriorVolumes = (SoundClassOverride && SoundClassOverride->Properties.bApplyAmbientVolumes)
							|| (Sound && Sound->ShouldApplyInteriorVolumes());
}

void FActiveSound::SetSoundClass(USoundClass* SoundClass)
{
	check(IsInGameThread());

	SoundClassOverride = SoundClass;
	bApplyInteriorVolumes = (SoundClassOverride && SoundClassOverride->Properties.bApplyAmbientVolumes)
							|| (Sound && Sound->ShouldApplyInteriorVolumes());
}

void FActiveSound::SetAudioComponent(UAudioComponent* Component)
{
	check(IsInGameThread());

	AActor* Owner = Component->GetOwner();

	AudioComponentID = Component->GetAudioComponentID();
	AudioComponentUserID = Component->GetAudioComponentUserID();
	AudioComponentName = Component->GetFName();

	SetOwner(Owner);
}

void FActiveSound::SetOwner(AActor* Actor)
{
	if (Actor)
	{
		OwnerID = Actor->GetUniqueID();
		OwnerName = Actor->GetFName();
	}
	else
	{
		OwnerID = 0;
		OwnerName = NAME_None;
	}
}

FString FActiveSound::GetAudioComponentName() const
{
	return (AudioComponentID > 0 ? AudioComponentName.ToString() : TEXT("NO COMPONENT"));
}

FString FActiveSound::GetOwnerName() const
{
	return (OwnerID > 0 ? OwnerName.ToString() : TEXT("None"));
}

USoundClass* FActiveSound::GetSoundClass() const
{
	if (SoundClassOverride)
	{
		return SoundClassOverride;
	}
	else if (Sound)
	{
		return Sound->GetSoundClass();
	}

	return nullptr;
}

USoundSubmix* FActiveSound::GetSoundSubmix() const
{
	if (SoundSubmixOverride)
	{
		return SoundSubmixOverride;
	}
	else if (Sound)
	{
		return Sound->GetSoundSubmix();
	}
	return nullptr;
}

void FActiveSound::SetSubmixSend(const FSoundSubmixSendInfo& SubmixSendInfo)
{
	// Override send level if the submix send already included in active sound
	for (FSoundSubmixSendInfo& Info : SoundSubmixSendsOverride)
	{
		if (Info.SoundSubmix == SubmixSendInfo.SoundSubmix)
		{
			Info.SendLevel = SubmixSendInfo.SendLevel;
			return;
		}
	}

	// Otherwise, add it to the submix send overrides
	SoundSubmixSendsOverride.Add(SubmixSendInfo);
}

void FActiveSound::SetSourceBusSend(const FSoundSourceBusSendInfo& SourceBusSendInfo)
{
	// Override send level if the source bus send is already included in active sound
	for (FSoundSourceBusSendInfo& Info : SoundSourceBusSendsOverride)
	{
		if (Info.SoundSourceBus == SourceBusSendInfo.SoundSourceBus)
		{
			Info.SendLevel = SourceBusSendInfo.SendLevel;
			return;
		}
	}

	// Otherwise, add it to the source bus send overrides
	SoundSourceBusSendsOverride.Add(SourceBusSendInfo);
}

void FActiveSound::GetSoundSubmixSends(TArray<FSoundSubmixSendInfo>& OutSends) const
{
	if (Sound)
	{
		// Get the base sends
		Sound->GetSoundSubmixSends(OutSends);

		// Loop through the overrides, which may append or override the existing send
		for (const FSoundSubmixSendInfo& SendInfo : SoundSubmixSendsOverride)
		{
			bool bOverridden = false;
			for (FSoundSubmixSendInfo& OutSendInfo : OutSends)
			{
				if (OutSendInfo.SoundSubmix == SendInfo.SoundSubmix)
				{
					OutSendInfo.SendLevel = SendInfo.SendLevel;
					bOverridden = true;
					break;
				}
			}

			if (!bOverridden)
			{
				OutSends.Add(SendInfo);
			}
		}
	}
}

void FActiveSound::GetSoundSourceBusSends(TArray<FSoundSourceBusSendInfo>& OutSends) const
{
	if (Sound)
	{
		// Get the base sends
		Sound->GetSoundSourceBusSends(OutSends);

		// Loop through the overrides, which may append or override the existing send
		for (const FSoundSourceBusSendInfo& SendInfo : SoundSourceBusSendsOverride)
		{
			bool bOverridden = false;
			for (FSoundSourceBusSendInfo& OutSendInfo : OutSends)
			{
				if (OutSendInfo.SoundSourceBus == SendInfo.SoundSourceBus)
				{
					OutSendInfo.SendLevel = SendInfo.SendLevel;
					bOverridden = true;
					break;
				}
			}

			if (!bOverridden)
			{
				OutSends.Add(SendInfo);
			}
		}
	}
}

int32 FActiveSound::FindClosestListener( const TArray<FListener>& InListeners ) const
{
	return FAudioDevice::FindClosestListenerIndex(Transform, InListeners);
}

const FSoundConcurrencySettings* FActiveSound::GetSoundConcurrencySettingsToApply() const
{
	if (ConcurrencySettings)
	{
		return &ConcurrencySettings->Concurrency;
	}
	else if (Sound)
	{
		return Sound->GetSoundConcurrencySettingsToApply();
	}
	return nullptr;
}

uint32 FActiveSound::GetSoundConcurrencyObjectID() const
{
	if (ConcurrencySettings)
	{
		return ConcurrencySettings->GetUniqueID();
	}
	else if (Sound)
	{
		return Sound->GetSoundConcurrencyObjectID();
	}
	return INDEX_NONE;
}

void FActiveSound::UpdateWaveInstances( TArray<FWaveInstance*> &InWaveInstances, const float DeltaTime )
{
	check(AudioDevice);

	// Early outs.
	if (Sound == nullptr || !Sound->IsPlayable())
	{
		return;
	}

	// splitscreen support:
	// we always pass the 'primary' listener (viewport 0) to the sound nodes and the underlying audio system
	// then move the AudioComponent's CurrentLocation so that its position relative to that Listener is the same as its real position is relative to the closest Listener

	int32 ClosestListenerIndex = 0;

	const TArray<FListener>& Listeners = AudioDevice->GetListeners();

	if (Listeners.Num() > 1)
	{
		SCOPE_CYCLE_COUNTER( STAT_AudioFindNearestLocation );
		ClosestListenerIndex = FindClosestListener(Listeners);
	}

	// Cache the closest listener ptr 
	ClosestListenerPtr = &Listeners[ClosestListenerIndex];

	// The apparent max distance factors the actual max distance of the sound scaled with the distance scale due to focus effects
	float ApparentMaxDistance = MaxDistance * FocusDistanceScale;

	FSoundParseParameters ParseParams;
	ParseParams.Transform = Transform;
	ParseParams.StartTime = RequestedStartTime;

	// Default values.
	// It's all Multiplicative!  So now people are all modifying the multiplier values via various means
	// (even after the Sound has started playing, and this line takes them all into account and gives us
	// final value that is correct
	UpdateAdjustVolumeMultiplier(DeltaTime);

	// If the sound is a preview sound, then ignore the transient master volume and application volume
	if (!bIsPreviewSound)
	{
		ParseParams.VolumeApp = AudioDevice->GetTransientMasterVolume() * FApp::GetVolumeMultiplier();
	}

	ParseParams.VolumeMultiplier = VolumeMultiplier * Sound->GetVolumeMultiplier() * CurrentAdjustVolumeMultiplier * ConcurrencyVolumeScale;

	ParseParams.Priority = Priority;
	ParseParams.Pitch *= PitchMultiplier * Sound->GetPitchMultiplier();
	ParseParams.bEnableLowPassFilter = bEnableLowPassFilter;
	ParseParams.LowPassFilterFrequency = LowPassFilterFrequency;
	ParseParams.SoundClass = GetSoundClass();
	ParseParams.bIsPaused = bIsPaused;

	ParseParams.SoundSubmix = GetSoundSubmix();
	GetSoundSubmixSends(ParseParams.SoundSubmixSends);

	ParseParams.bOutputToBusOnly = Sound->bOutputToBusOnly;
	GetSoundSourceBusSends(ParseParams.SoundSourceBusSends);

	// Set up the base source effect chain. 
	ParseParams.SourceEffectChain = Sound->SourceEffectChain;

	if (bApplyInteriorVolumes)
	{
		// Additional inside/outside processing for ambient sounds
		// If we aren't in a world there is no interior volumes to be handled.
		HandleInteriorVolumes(*ClosestListenerPtr, ParseParams);
	}

	// for velocity-based effects like doppler
	if (DeltaTime > 0.f)
	{
		ParseParams.Velocity = (ParseParams.Transform.GetTranslation() - LastLocation) / DeltaTime;
		LastLocation = ParseParams.Transform.GetTranslation();
	}

	TArray<FWaveInstance*> ThisSoundsWaveInstances;

	// Recurse nodes, have SoundWave's create new wave instances and update bFinished unless we finished fading out.
	bFinished = true;
	if (!bFadingOut || (PlaybackTime <= TargetAdjustVolumeStopTime))
	{
		if (bHasAttenuationSettings)
		{
			ApplyAttenuation(ParseParams, *ClosestListenerPtr);
		}
		else
		{
			// In the case of no attenuation settings, we still want to setup a default send reverb level
			ParseParams.ReverbSendMethod = EReverbSendMethod::Manual;
			ParseParams.ManualReverbSendLevel = AudioDevice->GetDefaultReverbSendLevel();
		}

		// if the closest listener is not the primary one, transform the sound transform so it's panned relative to primary listener position
		if (ClosestListenerIndex != 0)
		{
			const FListener& Listener = Listeners[0];
			ParseParams.Transform = ParseParams.Transform * ClosestListenerPtr->Transform.Inverse() * Listener.Transform;
		}

		Sound->Parse(AudioDevice, 0, *this, ParseParams, ThisSoundsWaveInstances);
	}

	if (bFinished)
	{
		AudioDevice->StopActiveSound(this);
	}
	else if (ThisSoundsWaveInstances.Num() > 0)
	{
		// If this active sound is told to limit concurrency by the quietest sound
		const FSoundConcurrencySettings* ConcurrencySettingsToApply = GetSoundConcurrencySettingsToApply();
		if (ConcurrencySettingsToApply && ConcurrencySettingsToApply->ResolutionRule == EMaxConcurrentResolutionRule::StopQuietest)
		{
			check(ConcurrencyGroupID != 0);
			// Now that we have this sound's active wave instances, lets find the loudest active wave instance to represent the "volume" of this active sound
			VolumeConcurrency = 0.0f;
			for (const FWaveInstance* WaveInstance : ThisSoundsWaveInstances)
			{
				const float WaveInstanceVolume = WaveInstance->GetVolumeWithDistanceAttenuation();
				if (WaveInstanceVolume > VolumeConcurrency)
				{
					VolumeConcurrency = WaveInstanceVolume;
				}
			}
		}
	}

	InWaveInstances.Append(ThisSoundsWaveInstances);
}

void FActiveSound::Stop()
{
	check(AudioDevice);

	if (Sound)
	{
		Sound->CurrentPlayCount = FMath::Max( Sound->CurrentPlayCount - 1, 0 );
	}

	for (auto WaveInstanceIt(WaveInstances.CreateIterator()); WaveInstanceIt; ++WaveInstanceIt)
	{
		FWaveInstance*& WaveInstance = WaveInstanceIt.Value();

		// Stop the owning sound source
		FSoundSource* Source = AudioDevice->GetSoundSource(WaveInstance);
		if( Source )
		{
			Source->Stop();
		}

		// Dequeue subtitles for this sounds on the game thread
		DECLARE_CYCLE_STAT(TEXT("FGameThreadAudioTask.KillSubtitles"), STAT_AudioKillSubtitles, STATGROUP_TaskGraphTasks);
		const PTRINT WaveInstanceID = (PTRINT)WaveInstance;
		FAudioThread::RunCommandOnGameThread([WaveInstanceID]()
		{
			FSubtitleManager::GetSubtitleManager()->KillSubtitles(WaveInstanceID);
		}, GET_STATID(STAT_AudioKillSubtitles));

		delete WaveInstance;

		// Null the entry out temporarily as later Stop calls could try to access this structure
		WaveInstance = nullptr;
	}
	WaveInstances.Empty();

	AudioDevice->RemoveActiveSound(this);
}

FWaveInstance* FActiveSound::FindWaveInstance( const UPTRINT WaveInstanceHash )
{
	FWaveInstance** WaveInstance = WaveInstances.Find(WaveInstanceHash);
	return (WaveInstance ? *WaveInstance : nullptr);
}

void FActiveSound::UpdateAdjustVolumeMultiplier(const float DeltaTime)
{
	// keep stepping towards our target until we hit our stop time
	if (PlaybackTime < TargetAdjustVolumeStopTime)
	{
		CurrentAdjustVolumeMultiplier += (TargetAdjustVolumeMultiplier - CurrentAdjustVolumeMultiplier) * DeltaTime / (TargetAdjustVolumeStopTime - PlaybackTime);
	}
	else
	{
		CurrentAdjustVolumeMultiplier = TargetAdjustVolumeMultiplier;
	}
} 

void FActiveSound::OcclusionTraceDone(const FTraceHandle& TraceHandle, FTraceDatum& TraceDatum)
{
	// Look for any results that resulted in a blocking hit
	bool bFoundBlockingHit = false;
	for (const FHitResult& HitResult : TraceDatum.OutHits)
	{
		if (HitResult.bBlockingHit)
		{
			bFoundBlockingHit = true;
			break;
		}
	}

	FAsyncTraceDetails TraceDetails;
	if (TraceToActiveSoundMap.RemoveAndCopyValue(TraceHandle, TraceDetails))
	{
		if (FAudioDeviceManager* AudioDeviceManager = GEngine->GetAudioDeviceManager())
		{
			if (FAudioDevice* AudioDevice = AudioDeviceManager->GetAudioDevice(TraceDetails.AudioDeviceID))
			{
				FActiveSound* ActiveSound = TraceDetails.ActiveSound;

				FAudioThread::RunCommandOnAudioThread([AudioDevice, ActiveSound, bFoundBlockingHit]()
				{
					if (AudioDevice->GetActiveSounds().Contains(ActiveSound))
					{
						ActiveSound->bIsOccluded = bFoundBlockingHit;
						ActiveSound->bAsyncOcclusionPending = false;
					}
				});
			}
		}
	}
}

void FActiveSound::CheckOcclusion(const FVector ListenerLocation, const FVector SoundLocation, const FSoundAttenuationSettings* AttenuationSettingsPtr)
{
	check(AttenuationSettingsPtr);
	check(AttenuationSettingsPtr->bEnableOcclusion);

	if (!bAsyncOcclusionPending && (PlaybackTime - LastOcclusionCheckTime) > OcclusionCheckInterval)
	{
		LastOcclusionCheckTime = PlaybackTime;

		const bool bUseComplexCollisionForOcclusion = AttenuationSettingsPtr->bUseComplexCollisionForOcclusion;
		const ECollisionChannel OcclusionTraceChannel = AttenuationSettingsPtr->OcclusionTraceChannel;

		if (!bHasCheckedOcclusion)
		{
			FCollisionQueryParams Params(SCENE_QUERY_STAT(SoundOcclusion), bUseComplexCollisionForOcclusion);
			if (OwnerID > 0)
			{
				Params.AddIgnoredActor(OwnerID);
			}

			if (UWorld* WorldPtr = World.Get())
			{
				// LineTraceTestByChannel is generally threadsafe, but there is a very narrow race condition here 
				// if World goes invalid before the scene lock and queries begin.
				bIsOccluded = WorldPtr->LineTraceTestByChannel(SoundLocation, ListenerLocation, OcclusionTraceChannel, Params);
			}
		}
		else
		{
			bAsyncOcclusionPending = true;

			const uint32 SoundOwnerID = OwnerID;
			TWeakObjectPtr<UWorld> SoundWorld = World;
			FAsyncTraceDetails TraceDetails;
			TraceDetails.AudioDeviceID = AudioDevice->DeviceHandle;
			TraceDetails.ActiveSound = this;

			FAudioThread::RunCommandOnGameThread([SoundWorld, SoundLocation, ListenerLocation, OcclusionTraceChannel, SoundOwnerID, bUseComplexCollisionForOcclusion, TraceDetails]
			{
				if (UWorld* WorldPtr = SoundWorld.Get())
				{
					FCollisionQueryParams Params(SCENE_QUERY_STAT(SoundOcclusion), bUseComplexCollisionForOcclusion);
					if (SoundOwnerID > 0)
					{
						Params.AddIgnoredActor(SoundOwnerID);
					}

					FTraceHandle TraceHandle = WorldPtr->AsyncLineTraceByChannel(EAsyncTraceType::Test, SoundLocation, ListenerLocation, OcclusionTraceChannel, Params, FCollisionResponseParams::DefaultResponseParam, &ActiveSoundTraceDelegate);
					TraceToActiveSoundMap.Add(TraceHandle, TraceDetails);
				}
			});
		}
	}

	// Update the occlusion values
	const float InterpolationTime = bHasCheckedOcclusion ? AttenuationSettingsPtr->OcclusionInterpolationTime : 0.0f;
	bHasCheckedOcclusion = true;

	if (bIsOccluded)
	{
		if (CurrentOcclusionFilterFrequency.GetTargetValue() > AttenuationSettingsPtr->OcclusionLowPassFilterFrequency)
		{
			CurrentOcclusionFilterFrequency.Set(AttenuationSettingsPtr->OcclusionLowPassFilterFrequency, InterpolationTime);
		}

		if (CurrentOcclusionVolumeAttenuation.GetTargetValue() > AttenuationSettingsPtr->OcclusionVolumeAttenuation)
		{
			CurrentOcclusionVolumeAttenuation.Set(AttenuationSettingsPtr->OcclusionVolumeAttenuation, InterpolationTime);
		}
	}
	else
	{
		CurrentOcclusionFilterFrequency.Set(MAX_FILTER_FREQUENCY, InterpolationTime);
		CurrentOcclusionVolumeAttenuation.Set(1.0f, InterpolationTime);
	}

	const float DeltaTime = FApp::GetDeltaTime();
	CurrentOcclusionFilterFrequency.Update(DeltaTime);
	CurrentOcclusionVolumeAttenuation.Update(DeltaTime);
}

void FActiveSound::HandleInteriorVolumes( const FListener& Listener, FSoundParseParameters& ParseParams )
{
	// Get the settings of the ambient sound
	if (!bGotInteriorSettings || (ParseParams.Transform.GetTranslation() - LastLocation).SizeSquared() > KINDA_SMALL_NUMBER)
	{
		FAudioDevice::FAudioVolumeSettings AudioVolumeSettings;
		AudioDevice->GetAudioVolumeSettings(WorldID, ParseParams.Transform.GetTranslation(), AudioVolumeSettings);

		InteriorSettings = AudioVolumeSettings.InteriorSettings;
		AudioVolumeID = AudioVolumeSettings.AudioVolumeID;
		bGotInteriorSettings = true;
	}

	// Check to see if we've moved to a new audio volume
	if (LastUpdateTime < Listener.InteriorStartTime)
	{
		SourceInteriorVolume = CurrentInteriorVolume;
		SourceInteriorLPF = CurrentInteriorLPF;
		LastUpdateTime = FApp::GetCurrentTime();
	}

	if (Listener.AudioVolumeID == AudioVolumeID || !bAllowSpatialization)
	{
		// Ambient and listener in same ambient zone
		CurrentInteriorVolume = FMath::Lerp(SourceInteriorVolume, 1.0f, Listener.InteriorVolumeInterp);
		ParseParams.InteriorVolumeMultiplier = CurrentInteriorVolume;

		CurrentInteriorLPF = FMath::Lerp(SourceInteriorLPF, MAX_FILTER_FREQUENCY, Listener.InteriorLPFInterp);
		ParseParams.AmbientZoneFilterFrequency = CurrentInteriorLPF;
	}
	else
	{
		// Ambient and listener in different ambient zone
		if( InteriorSettings.bIsWorldSettings )
		{
			// The ambient sound is 'outside' - use the listener's exterior volume
			CurrentInteriorVolume = FMath::Lerp(SourceInteriorVolume, Listener.InteriorSettings.ExteriorVolume, Listener.ExteriorVolumeInterp);
			ParseParams.InteriorVolumeMultiplier = CurrentInteriorVolume;

			CurrentInteriorLPF = FMath::Lerp(SourceInteriorLPF, Listener.InteriorSettings.ExteriorLPF, Listener.ExteriorLPFInterp);
			ParseParams.AmbientZoneFilterFrequency = CurrentInteriorLPF;
		}
		else
		{
			// The ambient sound is 'inside' - use the ambient sound's interior volume multiplied with the listeners exterior volume
			CurrentInteriorVolume = FMath::Lerp(SourceInteriorVolume, InteriorSettings.InteriorVolume, Listener.InteriorVolumeInterp);
			CurrentInteriorVolume *= FMath::Lerp(SourceInteriorVolume, Listener.InteriorSettings.ExteriorVolume, Listener.ExteriorVolumeInterp);
			ParseParams.InteriorVolumeMultiplier = CurrentInteriorVolume;

			float AmbientLPFValue = FMath::Lerp(SourceInteriorLPF, InteriorSettings.InteriorLPF, Listener.InteriorLPFInterp);
			float ListenerLPFValue = FMath::Lerp(SourceInteriorLPF, Listener.InteriorSettings.ExteriorLPF, Listener.ExteriorLPFInterp);

			// The current interior LPF value is the less of the LPF due to ambient zone and LPF due to listener settings
			if (AmbientLPFValue < ListenerLPFValue)
			{
				CurrentInteriorLPF = AmbientLPFValue;
				ParseParams.AmbientZoneFilterFrequency = AmbientLPFValue;
			}
			else
			{
				CurrentInteriorLPF = ListenerLPFValue;
				ParseParams.AmbientZoneFilterFrequency = ListenerLPFValue;
			}
		}
	}
}

void FActiveSound::ApplyRadioFilter(const FSoundParseParameters& ParseParams )
{
	check(AudioDevice);
	if( AudioDevice->GetMixDebugState() != DEBUGSTATE_DisableRadio )
	{
		// Make sure the radio filter is requested
		if( ParseParams.SoundClass)
		{
			const float RadioFilterVolumeThreshold = ParseParams.VolumeMultiplier * ParseParams.SoundClass->Properties.RadioFilterVolumeThreshold;
			if (RadioFilterVolumeThreshold > KINDA_SMALL_NUMBER )
			{
				bApplyRadioFilter = ( ParseParams.Volume < RadioFilterVolumeThreshold );
			}
		}
	}
	else
	{
		bApplyRadioFilter = false;
	}

	bRadioFilterSelected = true;
}

bool FActiveSound::GetFloatParameter( const FName InName, float& OutFloat ) const
{
	// Always fail if we pass in no name.
	if( InName != NAME_None )
	{
		for( const FAudioComponentParam& P : InstanceParameters )
		{
			if( P.ParamName == InName )
			{
				OutFloat = P.FloatParam;
				return true;
			}
		}
	}

	return false;
}

void FActiveSound::SetFloatParameter( const FName InName, const float InFloat )
{
	if( InName != NAME_None )
	{
		// First see if an entry for this name already exists
		for( FAudioComponentParam& P : InstanceParameters )
		{
			if( P.ParamName == InName )
			{
				P.FloatParam = InFloat;
				return;
			}
		}

		// We didn't find one, so create a new one.
		const int32 NewParamIndex = InstanceParameters.AddDefaulted();
		InstanceParameters[ NewParamIndex ].ParamName = InName;
		InstanceParameters[ NewParamIndex ].FloatParam = InFloat;
	}
}

bool FActiveSound::GetWaveParameter( const FName InName, USoundWave*& OutWave ) const
{
	// Always fail if we pass in no name.
	if( InName != NAME_None )
	{
		for( const FAudioComponentParam& P : InstanceParameters )
		{
			if( P.ParamName == InName )
			{
				OutWave = P.SoundWaveParam;
				return true;
			}
		}
	}

	return false;
}

void FActiveSound::SetWaveParameter( const FName InName, USoundWave* InWave )
{
	if( InName != NAME_None )
	{
		// First see if an entry for this name already exists
		for( FAudioComponentParam& P : InstanceParameters )
		{
			if( P.ParamName == InName )
			{
				P.SoundWaveParam = InWave;
				return;
			}
		}

		// We didn't find one, so create a new one.
		const int32 NewParamIndex = InstanceParameters.AddDefaulted();
		InstanceParameters[ NewParamIndex ].ParamName = InName;
		InstanceParameters[ NewParamIndex ].SoundWaveParam = InWave;
	}
}

bool FActiveSound::GetBoolParameter( const FName InName, bool& OutBool ) const
{
	// Always fail if we pass in no name.
	if( InName != NAME_None )
	{
		for( const FAudioComponentParam& P : InstanceParameters )
		{
			if( P.ParamName == InName )
			{
				OutBool = P.BoolParam;
				return true;
			}
		}
	}

	return false;
}

void FActiveSound::SetBoolParameter( const FName InName, const bool InBool )
{
	if( InName != NAME_None )
	{
		// First see if an entry for this name already exists
		for( FAudioComponentParam& P : InstanceParameters )
		{
			if( P.ParamName == InName )
			{
				P.BoolParam = InBool;
				return;
			}
		}

		// We didn't find one, so create a new one.
		const int32 NewParamIndex = InstanceParameters.AddDefaulted();
		InstanceParameters[ NewParamIndex ].ParamName = InName;
		InstanceParameters[ NewParamIndex ].BoolParam = InBool;
	}
}

bool FActiveSound::GetIntParameter( const FName InName, int32& OutInt ) const
{
	// Always fail if we pass in no name.
	if( InName != NAME_None )
	{
		for( const FAudioComponentParam& P : InstanceParameters )
		{
			if( P.ParamName == InName )
			{
				OutInt = P.IntParam;
				return true;
			}
		}
	}

	return false;
}

void FActiveSound::SetIntParameter( const FName InName, const int32 InInt )
{
	if( InName != NAME_None )
	{
		// First see if an entry for this name already exists
		for( FAudioComponentParam& P : InstanceParameters )
		{
			if( P.ParamName == InName )
			{
				P.IntParam = InInt;
				return;
			}
		}

		// We didn't find one, so create a new one.
		const int32 NewParamIndex = InstanceParameters.AddDefaulted();
		InstanceParameters[ NewParamIndex ].ParamName = InName;
		InstanceParameters[ NewParamIndex ].IntParam = InInt;
	}
}

void FActiveSound::SetSoundParameter(const FAudioComponentParam& Param)
{
	if (Param.ParamName != NAME_None)
	{
		// First see if an entry for this name already exists
		for( FAudioComponentParam& P : InstanceParameters )
		{
			if (P.ParamName == Param.ParamName)
			{
				P = Param;
				return;
			}
		}

		// We didn't find one, so create a new one.
		const int32 NewParamIndex = InstanceParameters.Add(Param);
	}
}

void FActiveSound::CollectAttenuationShapesForVisualization(TMultiMap<EAttenuationShape::Type, FBaseAttenuationSettings::AttenuationShapeDetails>& ShapeDetailsMap) const
{
	bool bFoundAttenuationSettings = false;

	if (bHasAttenuationSettings)
	{
		AttenuationSettings.CollectAttenuationShapesForVisualization(ShapeDetailsMap);
	}

	// For sound cues we'll dig in and see if we can find any attenuation sound nodes that will affect the settings
	USoundCue* SoundCue = Cast<USoundCue>(Sound);
	if (SoundCue)
	{
		TArray<USoundNodeAttenuation*> AttenuationNodes;
		SoundCue->RecursiveFindAttenuation( SoundCue->FirstNode, AttenuationNodes );
		for (int32 NodeIndex = 0; NodeIndex < AttenuationNodes.Num(); ++NodeIndex)
		{
			FSoundAttenuationSettings* AttenuationSettingsToApply = AttenuationNodes[NodeIndex]->GetAttenuationSettingsToApply();
			if (AttenuationSettingsToApply)
			{
				AttenuationSettingsToApply->CollectAttenuationShapesForVisualization(ShapeDetailsMap);
			}
		}
	}
}

float FActiveSound::GetAttenuationFrequency(const FSoundAttenuationSettings* Settings, const FAttenuationListenerData& ListenerData, const FVector2D& FrequencyRange, const FRuntimeFloatCurve& CustomCurve)
{
	float OutputFrequency = 0.0f;

	// If the frequency mapping is the same no matter what, no need to do any mapping
	if (FrequencyRange.X == FrequencyRange.Y)
	{
		OutputFrequency = FrequencyRange.X;
	}
	// If the transition band is instantaneous, just set it to before/after frequency value
	else if (Settings->LPFRadiusMin == Settings->LPFRadiusMax)
	{
		if (ListenerData.AttenuationDistance > Settings->LPFRadiusMin)
		{
			OutputFrequency = FrequencyRange.Y;
		}
		else
		{
			OutputFrequency = FrequencyRange.X;
		}
	}
	else if (Settings->AbsorptionMethod == EAirAbsorptionMethod::Linear)
	{
		FVector2D AbsorptionDistanceRange = { Settings->LPFRadiusMin, Settings->LPFRadiusMax };

		// Do log-scaling if we've been told to do so. This applies a log function to perceptually smooth filter frequency between target frequency ranges
		if (Settings->bEnableLogFrequencyScaling)
		{
			OutputFrequency = Audio::GetLogFrequencyClamped(ListenerData.AttenuationDistance, AbsorptionDistanceRange, FrequencyRange);
		}
		else
		{
			OutputFrequency = FMath::GetMappedRangeValueClamped(AbsorptionDistanceRange, FrequencyRange, ListenerData.AttenuationDistance);
		}
	}
	else
	{
		// In manual absorption mode, the frequency ranges are interpreted as a true "range"
		FVector2D ActualFreqRange(FMath::Min(FrequencyRange.X, FrequencyRange.Y), FMath::Max(FrequencyRange.X, FrequencyRange.Y));

		// Normalize the distance values to a value between 0 and 1
		FVector2D AbsorptionDistanceRange = { Settings->LPFRadiusMin, Settings->LPFRadiusMax };
		check(AbsorptionDistanceRange.Y != AbsorptionDistanceRange.X);
		const float Alpha = FMath::Clamp<float>((ListenerData.AttenuationDistance - AbsorptionDistanceRange.X) / (AbsorptionDistanceRange.Y - AbsorptionDistanceRange.X), 0.0f, 1.0f);

		// Perform the curve mapping
		const float MappedFrequencyValue = FMath::Clamp<float>(CustomCurve.GetRichCurveConst()->Eval(Alpha), 0.0f, 1.0f);

		if (Settings->bEnableLogFrequencyScaling)
		{
			// Use the mapped value in the log scale mapping
			OutputFrequency = Audio::GetLogFrequencyClamped(MappedFrequencyValue, FVector2D(0.0f, 1.0f), ActualFreqRange);
		}
		else
		{
			// Do a straight linear interpolation between the absorption frequency ranges
			OutputFrequency = FMath::GetMappedRangeValueClamped(FVector2D(0.0f, 1.0f), ActualFreqRange, MappedFrequencyValue);
		}
	}

	return FMath::Clamp<float>(OutputFrequency, MIN_FILTER_FREQUENCY, MAX_FILTER_FREQUENCY);
}

void FActiveSound::ApplyAttenuation(FSoundParseParameters& ParseParams, const FListener& Listener, const FSoundAttenuationSettings* SettingsAttenuationNode)
{
	float& Volume = ParseParams.Volume;
	const FTransform& SoundTransform = ParseParams.Transform;
	const FVector& ListenerLocation = Listener.Transform.GetTranslation();

	// Get the attenuation settings to use for this application to the active sound
	const FSoundAttenuationSettings* Settings = SettingsAttenuationNode ? SettingsAttenuationNode : &AttenuationSettings;

	FAttenuationListenerData ListenerData;

	// Reset distance and priority scale to 1.0 in case changed in editor
	FocusDistanceScale = 1.0f;
	FocusPriorityScale = 1.0f;

	check(Sound);

	if (Settings->bEnableReverbSend)
	{
		ParseParams.ReverbSendMethod = Settings->ReverbSendMethod;
		ParseParams.ManualReverbSendLevel = Settings->ManualReverbSendLevel;
		ParseParams.CustomReverbSendCurve = Settings->CustomReverbSendCurve;
		ParseParams.ReverbSendLevelRange = { Settings->ReverbWetLevelMin, Settings->ReverbWetLevelMax };
		ParseParams.ReverbSendLevelDistanceRange = { Settings->ReverbDistanceMin, Settings->ReverbDistanceMax };
	}

	if (Settings->bSpatialize || Settings->bEnableListenerFocus)
	{
		AudioDevice->GetAzimuth(ListenerData, Sound, SoundTransform, *Settings, Listener.Transform, Azimuth, AbsoluteAzimuth);

		if (Settings->bSpatialize)
		{
			ParseParams.AttenuationDistance = ListenerData.AttenuationDistance;

			ParseParams.ListenerToSoundDistance = ListenerData.ListenerToSoundDistance;

			ParseParams.AbsoluteAzimuth = AbsoluteAzimuth;
		}

		if (Settings->bEnableListenerFocus)
		{
			// Compute the azimuth of the active sound
			const FGlobalFocusSettings& FocusSettings = AudioDevice->GetGlobalFocusSettings();

			// Get the current target focus factor
			const float TargetFocusFactor = AudioDevice->GetFocusFactor(ListenerData, Sound, Azimuth, *Settings);

			// User opt-in for focus interpolation
			if (Settings->bEnableFocusInterpolation)
			{
				// Determine which interpolation speed to use (attack/release)
				float InterpSpeed;
				if (TargetFocusFactor <= InternalFocusFactor)
				{
					InterpSpeed = Settings->FocusAttackInterpSpeed;
				}
				else
				{
					InterpSpeed = Settings->FocusReleaseInterpSpeed;
				}

				// Interpolate the internal focus factor to the target value
				const float DeviceDeltaTime = AudioDevice->GetDeviceDeltaTime();
				InternalFocusFactor = FMath::FInterpTo(InternalFocusFactor, TargetFocusFactor, DeviceDeltaTime, InterpSpeed);
			}
			else
			{
				// Set focus directly to target value
				InternalFocusFactor = TargetFocusFactor;
			}

			// Get the volume scale to apply the volume calculation based on the focus factor
			const float FocusVolumeAttenuation = Settings->GetFocusAttenuation(FocusSettings, InternalFocusFactor);
			Volume *= FocusVolumeAttenuation;

			// Scale the volume-weighted priority scale value we use for sorting this sound for voice-stealing
			FocusPriorityScale = Settings->GetFocusPriorityScale(FocusSettings, InternalFocusFactor);
			ParseParams.Priority *= FocusPriorityScale;

			// Get the distance scale to use when computing distance-calculations for 3d attenuation
			FocusDistanceScale = Settings->GetFocusDistanceScale(FocusSettings, InternalFocusFactor);
		}
	}

	// Attenuate the volume based on the model. Note we don't apply the distance attenuation immediately to the sound.
	// The audio mixer applies distance-based attenuation as a separate stage to feed source audio through source effects and buses.
	// The old audio engine will scale this together when the wave instance is queried for GetActualVolume.
	if (Settings->bAttenuate)
	{
		if (Settings->AttenuationShape == EAttenuationShape::Sphere)
		{
			// Update attenuation data in-case it hasn't been updated
			AudioDevice->GetAttenuationListenerData(ListenerData, SoundTransform, *Settings, &Listener.Transform);
			ParseParams.DistanceAttenuation = Settings->AttenuationEval(ListenerData.AttenuationDistance, Settings->FalloffDistance, FocusDistanceScale);
		}
		else
		{
			ParseParams.DistanceAttenuation = Settings->Evaluate(SoundTransform, ListenerLocation, FocusDistanceScale);
		}
	}

	// Only do occlusion traces if the sound is audible and we're not using a occlusion plugin
	if (Settings->bEnableOcclusion)
	{
		// If we've got a occlusion plugin settings, then the plugin will handle occlusion calculations
		if (Settings->OcclusionPluginSettings)
		{
			ParseParams.OcclusionPluginSettings = Settings->OcclusionPluginSettings;
		}
		else if (Volume > 0.0f && !AudioDevice->IsAudioDeviceMuted())
		{
			check(ClosestListenerPtr);
			CheckOcclusion(ClosestListenerPtr->Transform.GetTranslation(), ParseParams.Transform.GetTranslation(), Settings);

			// Apply the volume attenuation due to occlusion (using the interpolating dynamic parameter)
			ParseParams.VolumeMultiplier *= CurrentOcclusionVolumeAttenuation.GetValue();

			ParseParams.bIsOccluded = bIsOccluded;
			ParseParams.OcclusionFilterFrequency = CurrentOcclusionFilterFrequency.GetValue();
		}
	}

	ParseParams.SpatializationPluginSettings = Settings->SpatializationPluginSettings;
	ParseParams.ReverbPluginSettings = Settings->ReverbPluginSettings;

	// Attenuate with the absorption filter if necessary
	if (Settings->bAttenuateWithLPF)
	{
		AudioDevice->GetAttenuationListenerData(ListenerData, SoundTransform, *Settings, &Listener.Transform);

		FVector2D AbsorptionLowPassFrequencyRange = { Settings->LPFFrequencyAtMin, Settings->LPFFrequencyAtMax };
		FVector2D AbsorptionHighPassFrequencyRange = { Settings->HPFFrequencyAtMin, Settings->HPFFrequencyAtMax };
		const float AttenuationLowpassFilterFrequency = GetAttenuationFrequency(Settings, ListenerData, AbsorptionLowPassFrequencyRange, Settings->CustomLowpassAirAbsorptionCurve);
		const float AttenuationHighPassFilterFrequency = GetAttenuationFrequency(Settings, ListenerData, AbsorptionHighPassFrequencyRange, Settings->CustomHighpassAirAbsorptionCurve);

		// Only apply the attenuation filter frequency if it results in a lower attenuation filter frequency than is already being used by ParseParams (the struct pass into the sound cue node tree)
		// This way, subsequently chained attenuation nodes in a sound cue will only result in the lowest frequency of the set.
		if (AttenuationLowpassFilterFrequency < ParseParams.AttenuationLowpassFilterFrequency)
		{
			ParseParams.AttenuationLowpassFilterFrequency = AttenuationLowpassFilterFrequency;
		}

		// Same with high pass filter frequency
		if (AttenuationHighPassFilterFrequency > ParseParams.AttenuationHighpassFilterFrequency)
		{
			ParseParams.AttenuationHighpassFilterFrequency = AttenuationHighPassFilterFrequency;
		}
	}

	ParseParams.OmniRadius = Settings->OmniRadius;
	ParseParams.StereoSpread = Settings->StereoSpread;
	ParseParams.bApplyNormalizationToStereoSounds = Settings->bApplyNormalizationToStereoSounds;
	ParseParams.bUseSpatialization |= Settings->bSpatialize;

	if (Settings->SpatializationAlgorithm == ESoundSpatializationAlgorithm::SPATIALIZATION_Default && AudioDevice->IsHRTFEnabledForAll())
	{
		ParseParams.SpatializationMethod = ESoundSpatializationAlgorithm::SPATIALIZATION_HRTF;
	}
	else
	{
		ParseParams.SpatializationMethod = Settings->SpatializationAlgorithm;
	}
}
