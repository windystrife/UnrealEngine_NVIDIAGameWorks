// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Components/AudioComponent.h"
#include "Audio.h"
#include "Engine/Texture2D.h"
#include "ActiveSound.h"
#include "AudioThread.h"
#include "AudioDevice.h"
#include "Sound/SoundNodeAttenuation.h"
#include "Sound/SoundCue.h"
#include "Components/BillboardComponent.h"
#include "FrameworkObjectVersion.h"

/*-----------------------------------------------------------------------------
UAudioComponent implementation.
-----------------------------------------------------------------------------*/
uint64 UAudioComponent::AudioComponentIDCounter = 0;
TMap<uint64, UAudioComponent*> UAudioComponent::AudioIDToComponentMap;

UAudioComponent::UAudioComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bUseAttachParentBound = true; // Avoid CalcBounds() when transform changes.
	bAutoDestroy = false;
	bAutoActivate = true;
	bAllowSpatialization = true;
	bStopWhenOwnerDestroyed = true;
	bNeverNeedsRenderUpdate = true;
	bWantsOnUpdateTransform = true;
#if WITH_EDITORONLY_DATA
	bVisualizeComponent = true;
#endif
	VolumeMultiplier = 1.f;
	bOverridePriority = false;
	bOverrideSubtitlePriority = false;
	bIsPreviewSound = false;
	bIsPaused = false;
	Priority = 1.f;
	SubtitlePriority = DEFAULT_SUBTITLE_PRIORITY;
	PitchMultiplier = 1.f;
	VolumeModulationMin = 1.f;
	VolumeModulationMax = 1.f;
	PitchModulationMin = 1.f;
	PitchModulationMax = 1.f;
	bEnableLowPassFilter = false;
	LowPassFilterFrequency = MAX_FILTER_FREQUENCY;
	OcclusionCheckInterval = 0.1f;
	ActiveCount = 0;

	AudioDeviceHandle = INDEX_NONE;
	AudioComponentID = ++AudioComponentIDCounter;

	// TODO: Consider only putting played/active components in to the map
	AudioIDToComponentMap.Add(AudioComponentID, this);
}

UAudioComponent* UAudioComponent::GetAudioComponentFromID(uint64 AudioComponentID)
{
	check(IsInGameThread());
	return AudioIDToComponentMap.FindRef(AudioComponentID);
}

void UAudioComponent::BeginDestroy()
{
	Super::BeginDestroy();

	if (bIsActive && Sound && Sound->IsLooping())
	{
		UE_LOG(LogAudio, Warning, TEXT("Audio Component is being destroyed without stopping looping sound '%s'"), *Sound->GetName());
		Stop();
	}

	AudioIDToComponentMap.Remove(AudioComponentID);
}

FString UAudioComponent::GetDetailedInfoInternal( void ) const
{
	FString Result;

	if (Sound != nullptr)
	{
		Result = Sound->GetPathName(nullptr);
	}
	else
	{
		Result = TEXT( "No_Sound" );
	}

	return Result;
}

void UAudioComponent::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FFrameworkObjectVersion::GUID);

	if (Ar.IsLoading() && Ar.CustomVer(FFrameworkObjectVersion::GUID) < FFrameworkObjectVersion::ChangeAudioComponentOverrideSubtitlePriorityDefault)
	{
		// Since the default for overriding the priority changed delta serialize would not have written out anything for true, so if they've changed
		// the priority we'll assume they wanted true, otherwise, we'll leave it with the new false default
		if (SubtitlePriority != DEFAULT_SUBTITLE_PRIORITY)
		{
			bOverrideSubtitlePriority = true;
		}
	}
}

void UAudioComponent::PostLoad()
{
	const int32 LinkerUE4Version = GetLinkerUE4Version();

	// Translate the old HighFrequencyGainMultiplier value to the new LowPassFilterFrequency value
	if (LinkerUE4Version < VER_UE4_USE_LOW_PASS_FILTER_FREQ)
	{
		if (HighFrequencyGainMultiplier_DEPRECATED > 0.0f &&  HighFrequencyGainMultiplier_DEPRECATED < 1.0f)
		{
			bEnableLowPassFilter = true;

			// This seems like it wouldn't make sense, but the original implementation for HighFrequencyGainMultiplier (a number between 0.0 and 1.0).
			// In earlier versions, this was *not* used as a high frequency gain, but instead converted to a frequency value between 0.0 and 6000.0
			// then "converted" to a radian frequency value using an equation taken from XAudio2 documentation. To recover
			// the original intended frequency (approximately), we'll run it through that equation, then scale radian value by the max filter frequency.

			float FilterConstant = 2.0f * FMath::Sin(PI * 6000.0f * HighFrequencyGainMultiplier_DEPRECATED / 48000);
			LowPassFilterFrequency = FilterConstant * MAX_FILTER_FREQUENCY;
		}
	}

	Super::PostLoad();
}

#if WITH_EDITORONLY_DATA
void UAudioComponent::OnRegister()
{
	Super::OnRegister();

	UpdateSpriteTexture();
}
#endif

void UAudioComponent::OnUnregister()
{
	// Route OnUnregister event.
	Super::OnUnregister();

	// Don't stop audio and clean up component if owner has been destroyed (default behaviour). This function gets
	// called from AActor::ClearComponents when an actor gets destroyed which is not usually what we want for one-
	// shot sounds.
	AActor* Owner = GetOwner();
	if (!Owner || bStopWhenOwnerDestroyed)
	{
		Stop();
	}
}

const UObject* UAudioComponent::AdditionalStatObject() const
{
	return Sound;
}

void UAudioComponent::SetSound( USoundBase* NewSound )
{
	const bool bPlay = IsPlaying();

	// If this is an auto destroy component we need to prevent it from being auto-destroyed since we're really just restarting it
	const bool bWasAutoDestroy = bAutoDestroy;
	bAutoDestroy = false;
	Stop();
	bAutoDestroy = bWasAutoDestroy;

	Sound = NewSound;

	if (bPlay)
	{
		Play();
	}
}

bool UAudioComponent::IsReadyForOwnerToAutoDestroy() const
{
	return !IsPlaying();
}

void UAudioComponent::OnUpdateTransform(EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport)
{
	Super::OnUpdateTransform(UpdateTransformFlags, Teleport);

	if (bIsActive && !bPreviewComponent)
	{
		if (FAudioDevice* AudioDevice = GetAudioDevice())
		{
			DECLARE_CYCLE_STAT(TEXT("FAudioThreadTask.UpdateTransform"), STAT_AudioUpdateTransform, STATGROUP_AudioThreadCommands);

			const uint64 MyAudioComponentID = AudioComponentID;
			const FTransform& MyTransform = GetComponentTransform();

			FAudioThread::RunCommandOnAudioThread([AudioDevice, MyAudioComponentID, MyTransform]()
			{
				FActiveSound* ActiveSound = AudioDevice->FindActiveSound(MyAudioComponentID);
				if (ActiveSound)
				{
					ActiveSound->Transform = MyTransform;

				}
			}, GET_STATID(STAT_AudioUpdateTransform));
		}
	}
};

void UAudioComponent::Play(float StartTime)
{
	PlayInternal(StartTime);
}

void UAudioComponent::PlayInternal(const float StartTime, const float FadeInDuration, const float FadeVolumeLevel)
{
	UWorld* World = GetWorld();

	UE_LOG(LogAudio, Verbose, TEXT("%g: Playing AudioComponent : '%s' with Sound: '%s'"), World ? World->GetAudioTimeSeconds() : 0.0f, *GetFullName(), Sound ? *Sound->GetName() : TEXT("nullptr"));

	if (bIsActive)
	{
		// If this is an auto destroy component we need to prevent it from being auto-destroyed since we're really just restarting it
		bool bCurrentAutoDestroy = bAutoDestroy;
		bAutoDestroy = false;
		Stop();
		bAutoDestroy = bCurrentAutoDestroy;
	}

	// Whether or not we managed to actually try to play the sound
	if (Sound && (World == nullptr || World->bAllowAudioPlayback))
	{
		if (FAudioDevice* AudioDevice = GetAudioDevice())
		{
			const FSoundAttenuationSettings* AttenuationSettingsToApply = (bAllowSpatialization ? GetAttenuationSettingsToApply() : nullptr);

			float MaxDistance = 0.0f;
			float FocusFactor = 0.0f;
			FVector Location = GetComponentTransform().GetLocation();

			AudioDevice->GetMaxDistanceAndFocusFactor(Sound, World, Location, AttenuationSettingsToApply, MaxDistance, FocusFactor);

			FActiveSound NewActiveSound;
			NewActiveSound.SetAudioComponent(this);
			NewActiveSound.SetWorld(GetWorld());
			NewActiveSound.SetSound(Sound);
			NewActiveSound.SetSoundClass(SoundClassOverride);
			NewActiveSound.ConcurrencySettings = ConcurrencySettings;

			NewActiveSound.VolumeMultiplier = (VolumeModulationMax + ((VolumeModulationMin - VolumeModulationMax) * FMath::SRand())) * VolumeMultiplier;
			// The priority used for the active sound is the audio component's priority scaled with the sound's priority
			if (bOverridePriority)
			{
				NewActiveSound.Priority = Priority;
			}
			else
			{
				NewActiveSound.Priority = Sound->Priority;
			}

			NewActiveSound.PitchMultiplier = (PitchModulationMax + ((PitchModulationMin - PitchModulationMax) * FMath::SRand())) * PitchMultiplier;
			NewActiveSound.bEnableLowPassFilter = bEnableLowPassFilter;
			NewActiveSound.LowPassFilterFrequency = LowPassFilterFrequency;
			NewActiveSound.RequestedStartTime = FMath::Max(0.f, StartTime);

			if (bOverrideSubtitlePriority)
			{
				NewActiveSound.SubtitlePriority = SubtitlePriority;
			}
			else
			{
				NewActiveSound.SubtitlePriority = Sound->GetSubtitlePriority();
			}

			NewActiveSound.bShouldRemainActiveIfDropped = bShouldRemainActiveIfDropped;
			NewActiveSound.bHandleSubtitles = (!bSuppressSubtitles || OnQueueSubtitles.IsBound());
			NewActiveSound.bIgnoreForFlushing = bIgnoreForFlushing;

			NewActiveSound.bIsUISound = bIsUISound;
			NewActiveSound.bIsMusic = bIsMusic;
			NewActiveSound.bAlwaysPlay = bAlwaysPlay;
			NewActiveSound.bReverb = bReverb;
			NewActiveSound.bCenterChannelOnly = bCenterChannelOnly;
			NewActiveSound.bIsPreviewSound = bIsPreviewSound;
			NewActiveSound.bLocationDefined = !bPreviewComponent;
			NewActiveSound.bIsPaused = bIsPaused;

			if (NewActiveSound.bLocationDefined)
			{
				NewActiveSound.Transform = GetComponentTransform();
			}

			NewActiveSound.bAllowSpatialization = bAllowSpatialization;
			NewActiveSound.bHasAttenuationSettings = (AttenuationSettingsToApply != nullptr);
			if (NewActiveSound.bHasAttenuationSettings)
			{
				NewActiveSound.AttenuationSettings = *AttenuationSettingsToApply;
				NewActiveSound.FocusPriorityScale = AttenuationSettingsToApply->GetFocusPriorityScale(AudioDevice->GetGlobalFocusSettings(), FocusFactor);
			}

			NewActiveSound.bUpdatePlayPercentage = OnAudioPlaybackPercentNative.IsBound() || OnAudioPlaybackPercent.IsBound();

			NewActiveSound.MaxDistance = MaxDistance;

			NewActiveSound.InstanceParameters = InstanceParameters;
			NewActiveSound.TargetAdjustVolumeMultiplier = FadeVolumeLevel;

			if (FadeInDuration > 0.0f)
			{
				NewActiveSound.CurrentAdjustVolumeMultiplier = 0.f;
				NewActiveSound.TargetAdjustVolumeStopTime = FadeInDuration;
			}
			else
			{
				NewActiveSound.CurrentAdjustVolumeMultiplier = FadeVolumeLevel;
			}

			// Bump ActiveCount... this is used to determine if an audio component is still active after "finishing"
			++ActiveCount;

			AudioDevice->AddNewActiveSound(NewActiveSound);
			bIsActive = true;
		}
	}
}

FAudioDevice* UAudioComponent::GetAudioDevice() const
{
	FAudioDevice* AudioDevice = nullptr;

	if (GEngine)
	{
		if (AudioDeviceHandle != INDEX_NONE)
		{
			FAudioDeviceManager* AudioDeviceManager = GEngine->GetAudioDeviceManager();
			AudioDevice = (AudioDeviceManager ? AudioDeviceManager->GetAudioDevice(AudioDeviceHandle) : nullptr);
		}
		else if (UWorld* World = GetWorld())
		{
			AudioDevice = World->GetAudioDevice();
		}
		else
		{
			AudioDevice = GEngine->GetMainAudioDevice();
		}
	}
	return AudioDevice;
}

void UAudioComponent::FadeIn( float FadeInDuration, float FadeVolumeLevel, float StartTime )
{
	PlayInternal(StartTime, FadeInDuration, FadeVolumeLevel);
}

void UAudioComponent::FadeOut( float FadeOutDuration, float FadeVolumeLevel )
{
	if (bIsActive)
	{
		if (FadeOutDuration > 0.0f)
		{
			if (FAudioDevice* AudioDevice = GetAudioDevice())
			{
				DECLARE_CYCLE_STAT(TEXT("FAudioThreadTask.FadeOut"), STAT_AudioFadeOut, STATGROUP_AudioThreadCommands);

				const uint64 MyAudioComponentID = AudioComponentID;
				FAudioThread::RunCommandOnAudioThread([AudioDevice, MyAudioComponentID, FadeOutDuration, FadeVolumeLevel]()
				{
					FActiveSound* ActiveSound = AudioDevice->FindActiveSound(MyAudioComponentID);
					if (ActiveSound)
					{
						ActiveSound->TargetAdjustVolumeMultiplier = FadeVolumeLevel;
						ActiveSound->TargetAdjustVolumeStopTime = ActiveSound->PlaybackTime + FadeOutDuration;
						ActiveSound->bFadingOut = true;
					}
				}, GET_STATID(STAT_AudioFadeOut));
			}
		}
		else
		{
			Stop();
		}
	}
}

void UAudioComponent::AdjustVolume( float AdjustVolumeDuration, float AdjustVolumeLevel )
{
	if (bIsActive)
	{
		if (FAudioDevice* AudioDevice = GetAudioDevice())
		{
			DECLARE_CYCLE_STAT(TEXT("FAudioThreadTask.AdjustVolume"), STAT_AudioAdjustVolume, STATGROUP_AudioThreadCommands);

			const uint64 MyAudioComponentID = AudioComponentID;
			FAudioThread::RunCommandOnAudioThread([AudioDevice, MyAudioComponentID, AdjustVolumeDuration, AdjustVolumeLevel]()
			{
				FActiveSound* ActiveSound = AudioDevice->FindActiveSound(MyAudioComponentID);
				if (ActiveSound)
				{
					ActiveSound->bFadingOut = false;
					ActiveSound->TargetAdjustVolumeMultiplier = AdjustVolumeLevel;

					if (AdjustVolumeDuration > 0.0f)
					{
						ActiveSound->TargetAdjustVolumeStopTime = ActiveSound->PlaybackTime + AdjustVolumeDuration;
					}
					else
					{
						ActiveSound->CurrentAdjustVolumeMultiplier = AdjustVolumeLevel;
						ActiveSound->TargetAdjustVolumeStopTime = -1.0f;
					}
				}
			}, GET_STATID(STAT_AudioAdjustVolume));
		}
	}
}

void UAudioComponent::Stop()
{
	if (bIsActive)
	{
		// Set this to immediately be inactive
		bIsActive = false;

		UE_LOG(LogAudio, Verbose, TEXT( "%g: Stopping AudioComponent : '%s' with Sound: '%s'" ), GetWorld() ? GetWorld()->GetAudioTimeSeconds() : 0.0f, *GetFullName(), Sound ? *Sound->GetName() : TEXT( "nullptr" ) );

		if (FAudioDevice* AudioDevice = GetAudioDevice())
		{
			AudioDevice->StopActiveSound(AudioComponentID);
		}
	}
}

void UAudioComponent::SetPaused(bool bPause)
{
	if (bIsPaused != bPause)
	{
		bIsPaused = bPause;

		if (bIsActive)
		{
			UE_LOG(LogAudio, Verbose, TEXT("%g: Pausing AudioComponent : '%s' with Sound: '%s'"), GetWorld() ? GetWorld()->GetAudioTimeSeconds() : 0.0f, *GetFullName(), Sound ? *Sound->GetName() : TEXT("nullptr"));

			if (FAudioDevice* AudioDevice = GetAudioDevice())
			{
				DECLARE_CYCLE_STAT(TEXT("FAudioThreadTask.PauseActiveSound"), STAT_AudioPauseActiveSound, STATGROUP_AudioThreadCommands);

				const uint64 MyAudioComponentID = AudioComponentID;
				FAudioThread::RunCommandOnAudioThread([AudioDevice, MyAudioComponentID, bPause]()
				{
					AudioDevice->PauseActiveSound(MyAudioComponentID, bPause);
				}, GET_STATID(STAT_AudioPauseActiveSound));
			}
		}
	}
}

void UAudioComponent::PlaybackCompleted(uint64 AudioComponentID, bool bFailedToStart)
{
	check(IsInAudioThread());

	DECLARE_CYCLE_STAT(TEXT("FGameThreadAudioTask.PlaybackCompleted"), STAT_AudioPlaybackCompleted, STATGROUP_TaskGraphTasks);

	FAudioThread::RunCommandOnGameThread([AudioComponentID, bFailedToStart]()
	{
		if (UAudioComponent* AudioComponent = GetAudioComponentFromID(AudioComponentID))
		{
			AudioComponent->PlaybackCompleted(bFailedToStart);
		}
	}, GET_STATID(STAT_AudioPlaybackCompleted));
}

void UAudioComponent::PlaybackCompleted(bool bFailedToStart)
{
	check(ActiveCount > 0);
	--ActiveCount;

	// Mark inactive before calling destroy to avoid recursion
	bIsActive = (ActiveCount > 0);

	if (!bIsActive)
	{
		if (!bFailedToStart && GetWorld() != nullptr && (OnAudioFinished.IsBound() || OnAudioFinishedNative.IsBound()))
		{
			INC_DWORD_STAT(STAT_AudioFinishedDelegatesCalled);
			SCOPE_CYCLE_COUNTER(STAT_AudioFinishedDelegates);

			OnAudioFinished.Broadcast();
			OnAudioFinishedNative.Broadcast(this);
		}

		// Auto destruction is handled via marking object for deletion.
		if (bAutoDestroy)
		{
			DestroyComponent();
		}
	}
}

bool UAudioComponent::IsPlaying() const
{
	return bIsActive;
}

#if WITH_EDITORONLY_DATA
void UAudioComponent::UpdateSpriteTexture()
{
	if (SpriteComponent)
	{
		SpriteComponent->SpriteInfo.Category = TEXT("Sounds");
		SpriteComponent->SpriteInfo.DisplayName = NSLOCTEXT("SpriteCategory", "Sounds", "Sounds");

		if (bAutoActivate)
		{
			SpriteComponent->SetSprite(LoadObject<UTexture2D>(nullptr, TEXT("/Engine/EditorResources/AudioIcons/S_AudioComponent_AutoActivate.S_AudioComponent_AutoActivate")));
		}
		else
		{
			SpriteComponent->SetSprite(LoadObject<UTexture2D>(nullptr, TEXT("/Engine/EditorResources/AudioIcons/S_AudioComponent.S_AudioComponent")));
		}
	}
}
#endif

#if WITH_EDITOR
void UAudioComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (bIsActive)
	{
		// If this is an auto destroy component we need to prevent it from being auto-destroyed since we're really just restarting it
		const bool bWasAutoDestroy = bAutoDestroy;
		bAutoDestroy = false;
		Stop();
		bAutoDestroy = bWasAutoDestroy;
		Play();
	}

#if WITH_EDITORONLY_DATA
	UpdateSpriteTexture();
#endif

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif // WITH_EDITOR

const FSoundAttenuationSettings* UAudioComponent::GetAttenuationSettingsToApply() const
{
	if (bOverrideAttenuation)
	{
		return &AttenuationOverrides;
	}
	else if (AttenuationSettings)
	{
		return &AttenuationSettings->Attenuation;
	}
	else if (Sound)
	{
		return Sound->GetAttenuationSettingsToApply();
	}
	return nullptr;
}

bool UAudioComponent::BP_GetAttenuationSettingsToApply(FSoundAttenuationSettings& OutAttenuationSettings)
{
	if (const FSoundAttenuationSettings* Settings = GetAttenuationSettingsToApply())
	{
		OutAttenuationSettings = *Settings;
		return true;
	}
	return false;
}

void UAudioComponent::CollectAttenuationShapesForVisualization(TMultiMap<EAttenuationShape::Type, FBaseAttenuationSettings::AttenuationShapeDetails>& ShapeDetailsMap) const
{
	const FSoundAttenuationSettings *AttenuationSettingsToApply = GetAttenuationSettingsToApply();

	if (AttenuationSettingsToApply)
	{
		AttenuationSettingsToApply->CollectAttenuationShapesForVisualization(ShapeDetailsMap);
	}

	// For sound cues we'll dig in and see if we can find any attenuation sound nodes that will affect the settings
	USoundCue* SoundCue = Cast<USoundCue>(Sound);
	if (SoundCue)
	{
		TArray<USoundNodeAttenuation*> AttenuationNodes;
		SoundCue->RecursiveFindAttenuation( SoundCue->FirstNode, AttenuationNodes );
		for (int32 NodeIndex = 0; NodeIndex < AttenuationNodes.Num(); ++NodeIndex)
		{
			AttenuationSettingsToApply = AttenuationNodes[NodeIndex]->GetAttenuationSettingsToApply();
			if (AttenuationSettingsToApply)
			{
				AttenuationSettingsToApply->CollectAttenuationShapesForVisualization(ShapeDetailsMap);
			}
		}
	}
}

void UAudioComponent::Activate(bool bReset)
{
	if (bReset || ShouldActivate() == true)
	{
		Play();
		if (bIsActive)
		{
			OnComponentActivated.Broadcast(this, bReset);
		}
	}
}

void UAudioComponent::Deactivate()
{
	if (ShouldActivate() == false)
	{
		Stop();

		if (!bIsActive)
		{
			OnComponentDeactivated.Broadcast(this);
		}
	}
}

void UAudioComponent::SetFloatParameter( const FName InName, const float InFloat )
{
	if (InName != NAME_None)
	{
		bool bFound = false;

		// First see if an entry for this name already exists
		for (FAudioComponentParam& P : InstanceParameters)
		{
			if (P.ParamName == InName)
			{
				P.FloatParam = InFloat;
				bFound = true;
				break;
			}
		}

		// We didn't find one, so create a new one.
		if (!bFound)
		{
			const int32 NewParamIndex = InstanceParameters.AddDefaulted();
			InstanceParameters[ NewParamIndex ].ParamName = InName;
			InstanceParameters[ NewParamIndex ].FloatParam = InFloat;
		}

		// If we're active we need to push this value to the ActiveSound
		if (bIsActive)
		{
			if (FAudioDevice* AudioDevice = GetAudioDevice())
			{
				DECLARE_CYCLE_STAT(TEXT("FAudioThreadTask.SetFloatParameter"), STAT_AudioSetFloatParameter, STATGROUP_AudioThreadCommands);

				const uint64 MyAudioComponentID = AudioComponentID;
				FAudioThread::RunCommandOnAudioThread([AudioDevice, MyAudioComponentID, InName, InFloat]()
				{
					FActiveSound* ActiveSound = AudioDevice->FindActiveSound(MyAudioComponentID);
					if (ActiveSound)
					{
						ActiveSound->SetFloatParameter(InName, InFloat);
					}
				}, GET_STATID(STAT_AudioSetFloatParameter));
			}
		}
	}
}

void UAudioComponent::SetWaveParameter( const FName InName, USoundWave* InWave )
{
	if (InName != NAME_None)
	{
		bool bFound = false;
		// First see if an entry for this name already exists
		for (FAudioComponentParam& P : InstanceParameters)
		{
			if (P.ParamName == InName)
			{
				P.SoundWaveParam = InWave;
				bFound = true;
				break;
			}
		}

		// We didn't find one, so create a new one.
		if (!bFound)
		{
			const int32 NewParamIndex = InstanceParameters.AddDefaulted();
			InstanceParameters[NewParamIndex].ParamName = InName;
			InstanceParameters[NewParamIndex].SoundWaveParam = InWave;
		}

		// If we're active we need to push this value to the ActiveSound
		if (bIsActive)
		{
			if (FAudioDevice* AudioDevice = GetAudioDevice())
			{
				DECLARE_CYCLE_STAT(TEXT("FAudioThreadTask.SetWaveParameter"), STAT_AudioSetWaveParameter, STATGROUP_AudioThreadCommands);

				const uint64 MyAudioComponentID = AudioComponentID;
				FAudioThread::RunCommandOnAudioThread([AudioDevice, MyAudioComponentID, InName, InWave]()
				{
					FActiveSound* ActiveSound = AudioDevice->FindActiveSound(MyAudioComponentID);
					if (ActiveSound)
					{
						ActiveSound->SetWaveParameter(InName, InWave);
					}
				}, GET_STATID(STAT_AudioSetWaveParameter));
			}
		}
	}
}

void UAudioComponent::SetBoolParameter( const FName InName, const bool InBool )
{
	if (InName != NAME_None)
	{
		bool bFound = false;
		// First see if an entry for this name already exists
		for (FAudioComponentParam& P : InstanceParameters)
		{
			if (P.ParamName == InName)
			{
				P.BoolParam = InBool;
				bFound = true;
				break;
			}
		}

		// We didn't find one, so create a new one.
		if (!bFound)
		{
			const int32 NewParamIndex = InstanceParameters.AddDefaulted();
			InstanceParameters[ NewParamIndex ].ParamName = InName;
			InstanceParameters[ NewParamIndex ].BoolParam = InBool;
		}

		// If we're active we need to push this value to the ActiveSound
		if (bIsActive)
		{
			if (FAudioDevice* AudioDevice = GetAudioDevice())
			{
				DECLARE_CYCLE_STAT(TEXT("FAudioThreadTask.SetBoolParameter"), STAT_AudioSetBoolParameter, STATGROUP_AudioThreadCommands);

				const uint64 MyAudioComponentID = AudioComponentID;
				FAudioThread::RunCommandOnAudioThread([AudioDevice, MyAudioComponentID, InName, InBool]()
				{
					FActiveSound* ActiveSound = AudioDevice->FindActiveSound(MyAudioComponentID);
					if (ActiveSound)
					{
						ActiveSound->SetBoolParameter(InName, InBool);
					}
				}, GET_STATID(STAT_AudioSetBoolParameter));
			}
		}
	}
}


void UAudioComponent::SetIntParameter( const FName InName, const int32 InInt )
{
	if (InName != NAME_None)
	{
		bool bFound = false;
		// First see if an entry for this name already exists
		for (FAudioComponentParam& P : InstanceParameters)
		{
			if (P.ParamName == InName)
			{
				P.IntParam = InInt;
				bFound = true;
				break;
			}
		}

		// We didn't find one, so create a new one.
		if (!bFound)
		{
			const int32 NewParamIndex = InstanceParameters.AddDefaulted();
			InstanceParameters[NewParamIndex].ParamName = InName;
			InstanceParameters[NewParamIndex].IntParam = InInt;
		}

		// If we're active we need to push this value to the ActiveSound
		if (bIsActive)
		{
			if (FAudioDevice* AudioDevice = GetAudioDevice())
			{
				DECLARE_CYCLE_STAT(TEXT("FAudioThreadTask.SetIntParameter"), STAT_AudioSetIntParameter, STATGROUP_AudioThreadCommands);

				const uint64 MyAudioComponentID = AudioComponentID;
				FAudioThread::RunCommandOnAudioThread([AudioDevice, MyAudioComponentID, InName, InInt]()
				{
					FActiveSound* ActiveSound = AudioDevice->FindActiveSound(MyAudioComponentID);
					if (ActiveSound)
					{
						ActiveSound->SetIntParameter(InName, InInt);
					}
				}, GET_STATID(STAT_AudioSetIntParameter));
			}
		}
	}
}

void UAudioComponent::SetSoundParameter(const FAudioComponentParam& Param)
{
	if (Param.ParamName != NAME_None)
	{
		bool bFound = false;
		// First see if an entry for this name already exists
		for (FAudioComponentParam& P : InstanceParameters)
		{
			if (P.ParamName == Param.ParamName)
			{
				P = Param;
				bFound = true;
				break;
			}
		}

		// We didn't find one, so create a new one.
		if (!bFound)
		{
			const int32 NewParamIndex = InstanceParameters.Add(Param);
		}

		if (bIsActive)
		{
			if (FAudioDevice* AudioDevice = GetAudioDevice())
			{
				DECLARE_CYCLE_STAT(TEXT("FAudioThreadTask.SetSoundParameter"), STAT_AudioSetSoundParameter, STATGROUP_AudioThreadCommands);

				const uint64 MyAudioComponentID = AudioComponentID;
				FAudioThread::RunCommandOnAudioThread([AudioDevice, MyAudioComponentID, Param]()
				{
					FActiveSound* ActiveSound = AudioDevice->FindActiveSound(MyAudioComponentID);
					if (ActiveSound)
					{
						ActiveSound->SetSoundParameter(Param);
					}
				}, GET_STATID(STAT_AudioSetSoundParameter));
			}
		}
	}
}

void UAudioComponent::SetVolumeMultiplier(const float NewVolumeMultiplier)
{
	VolumeMultiplier = NewVolumeMultiplier;
	VolumeModulationMin = VolumeModulationMax = 1.f;

	if (bIsActive)
	{
		if (FAudioDevice* AudioDevice = GetAudioDevice())
		{
			DECLARE_CYCLE_STAT(TEXT("FAudioThreadTask.SetVolumeMultiplier"), STAT_AudioSetVolumeMultiplier, STATGROUP_AudioThreadCommands);

			const uint64 MyAudioComponentID = AudioComponentID;
			FAudioThread::RunCommandOnAudioThread([AudioDevice, MyAudioComponentID, NewVolumeMultiplier]()
			{
				FActiveSound* ActiveSound = AudioDevice->FindActiveSound(MyAudioComponentID);
				if (ActiveSound)
				{
					ActiveSound->VolumeMultiplier = NewVolumeMultiplier;
				}
			}, GET_STATID(STAT_AudioSetVolumeMultiplier));
		}
	}
}

void UAudioComponent::SetPitchMultiplier(const float NewPitchMultiplier)
{
	PitchMultiplier = NewPitchMultiplier;
	PitchModulationMin = PitchModulationMax = 1.f;

	if (bIsActive)
	{
		if (FAudioDevice* AudioDevice = GetAudioDevice())
		{
			DECLARE_CYCLE_STAT(TEXT("FAudioThreadTask.SetPitchMultiplier"), STAT_AudioSetPitchMultiplier, STATGROUP_AudioThreadCommands);

			const uint64 MyAudioComponentID = AudioComponentID;
			FAudioThread::RunCommandOnAudioThread([AudioDevice, MyAudioComponentID, NewPitchMultiplier]()
			{
				FActiveSound* ActiveSound = AudioDevice->FindActiveSound(MyAudioComponentID);
				if (ActiveSound)
				{
					ActiveSound->PitchMultiplier = NewPitchMultiplier;
				}
			}, GET_STATID(STAT_AudioSetPitchMultiplier));
		}
	}
}

void UAudioComponent::SetUISound(const bool bInIsUISound)
{
	bIsUISound = bInIsUISound;

	if (bIsActive)
	{
		if (FAudioDevice* AudioDevice = GetAudioDevice())
		{
			DECLARE_CYCLE_STAT(TEXT("FAudioThreadTask.SetIsUISound"), STAT_AudioSetIsUISound, STATGROUP_AudioThreadCommands);

			const uint64 MyAudioComponentID = AudioComponentID;
			FAudioThread::RunCommandOnAudioThread([AudioDevice, MyAudioComponentID, bInIsUISound]()
			{
				FActiveSound* ActiveSound = AudioDevice->FindActiveSound(MyAudioComponentID);
				if (ActiveSound)
				{
					ActiveSound->bIsUISound = bInIsUISound;
				}
			}, GET_STATID(STAT_AudioSetIsUISound));
		}
	}
}

void UAudioComponent::AdjustAttenuation(const FSoundAttenuationSettings& InAttenuationSettings)
{
	bOverrideAttenuation = true;
	AttenuationOverrides = InAttenuationSettings;

	if (bIsActive)
	{
		if (FAudioDevice* AudioDevice = GetAudioDevice())
		{
			DECLARE_CYCLE_STAT(TEXT("FAudioThreadTask.AdjustAttenuation"), STAT_AudioAdjustAttenuation, STATGROUP_AudioThreadCommands);

			const uint64 MyAudioComponentID = AudioComponentID;
			FAudioThread::RunCommandOnAudioThread([AudioDevice, MyAudioComponentID, InAttenuationSettings]()
			{
				FActiveSound* ActiveSound = AudioDevice->FindActiveSound(MyAudioComponentID);
				if (ActiveSound)
				{
					ActiveSound->AttenuationSettings = InAttenuationSettings;
				}
			}, GET_STATID(STAT_AudioAdjustAttenuation));
		}
	}
}

void UAudioComponent::SetSubmixSend(USoundSubmix* Submix, float SendLevel)
{
	if (FAudioDevice* AudioDevice = GetAudioDevice())
	{
		const uint64 MyAudioComponentID = AudioComponentID;
		FAudioThread::RunCommandOnAudioThread([AudioDevice, MyAudioComponentID, Submix, SendLevel]()
		{
			FActiveSound* ActiveSound = AudioDevice->FindActiveSound(MyAudioComponentID);
			if (ActiveSound)
			{
				FSoundSubmixSendInfo SendInfo;
				SendInfo.SoundSubmix = Submix;
				SendInfo.SendLevel = SendLevel;
				ActiveSound->SetSubmixSend(SendInfo);
			}
		});
	}
}

void UAudioComponent::SetLowPassFilterEnabled(bool InLowPassFilterEnabled)
{
	if (FAudioDevice* AudioDevice = GetAudioDevice())
	{
		DECLARE_CYCLE_STAT(TEXT("FAudioThreadTask.SetLowPassFilterFrequency"), STAT_AudioSetLowPassFilterEnabled, STATGROUP_AudioThreadCommands);

		const uint64 MyAudioComponentID = AudioComponentID;
		FAudioThread::RunCommandOnAudioThread([AudioDevice, MyAudioComponentID, InLowPassFilterEnabled]()
		{
			FActiveSound* ActiveSound = AudioDevice->FindActiveSound(MyAudioComponentID);
			if (ActiveSound)
			{
				ActiveSound->bEnableLowPassFilter = InLowPassFilterEnabled;
			}
		}, GET_STATID(STAT_AudioSetLowPassFilterEnabled));
	}
}

void UAudioComponent::SetLowPassFilterFrequency(float InLowPassFilterFrequency)
{
	if (FAudioDevice* AudioDevice = GetAudioDevice())
	{
		DECLARE_CYCLE_STAT(TEXT("FAudioThreadTask.SetLowPassFilterFrequency"), STAT_AudioSetLowPassFilterFrequency, STATGROUP_AudioThreadCommands);

		const uint64 MyAudioComponentID = AudioComponentID;
		FAudioThread::RunCommandOnAudioThread([AudioDevice, MyAudioComponentID, InLowPassFilterFrequency]()
		{
			FActiveSound* ActiveSound = AudioDevice->FindActiveSound(MyAudioComponentID);
			if (ActiveSound)
			{
				ActiveSound->LowPassFilterFrequency = InLowPassFilterFrequency;
			}
		}, GET_STATID(STAT_AudioSetLowPassFilterFrequency));
	}
}


