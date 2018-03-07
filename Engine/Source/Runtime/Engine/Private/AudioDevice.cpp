// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AudioDevice.h"
#include "PhysicsEngine/BodyInstance.h"
#include "Sound/SoundEffectPreset.h"
#include "Sound/SoundEffectSubmix.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Misc/OutputDeviceArchiveWrapper.h"
#include "ProfilingDebugging/ProfilingHelpers.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/App.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "UObject/Package.h"
#include "ActiveSound.h"
#include "ContentStreaming.h"
#include "UnrealEngine.h"
#include "Sound/SoundGroups.h"
#include "Sound/SoundEffectSource.h"
#include "Sound/SoundWave.h"
#include "AudioDecompress.h"
#include "AudioEffect.h"
#include "AudioPluginUtilities.h"
#include "Sound/AudioSettings.h"
#include "Sound/SoundCue.h"
#include "Sound/SoundNode.h"
#include "Sound/SoundNodeWavePlayer.h"
#include "GameFramework/GameUserSettings.h"
#include "GameFramework/WorldSettings.h"
#include "GeneralProjectSettings.h"
#include "HAL/LowLevelMemTracker.h"

#if WITH_EDITOR
#include "AssetRegistryModule.h"
#include "Developer/AssetTools/Public/IAssetTools.h"
#include "Developer/AssetTools/Public/AssetToolsModule.h"
#include "Editor/EditorEngine.h"
#include "AudioEditorModule.h"
#endif

static int32 AudioChannelCountCVar = 0;
FAutoConsoleVariableRef CVarSetAudioChannelCount(
	TEXT("au.SetAudioChannelCount"),
	AudioChannelCountCVar,
	TEXT("Changes the audio channel count. Max value is clamped to the MaxChannelCount the audio engine was initialize with.\n")
	TEXT("0: Disable, >0: Enable"),
	ECVF_Default);


/*-----------------------------------------------------------------------------
FDynamicParameter implementation.
-----------------------------------------------------------------------------*/

FDynamicParameter::FDynamicParameter(float Value)
	: CurrValue(Value)
	, StartValue(Value)
	, DeltaValue(0.0f)
	, CurrTimeSec(0.0f)
	, DurationSec(0.0f)
	, LastTime(0.0f)
	, TargetValue(Value)
{}

void FDynamicParameter::Set(float Value, float InDuration)
{
	if (TargetValue != Value || DurationSec != InDuration)
	{
		TargetValue = Value;
		if (InDuration > 0.0f)
		{
			DeltaValue = Value - CurrValue;
			StartValue = CurrValue;
			DurationSec = InDuration;
			CurrTimeSec = 0.0f;
		}
		else
		{
			StartValue = Value;
			DeltaValue = 0.0f;
			DurationSec = 0.0f;
			CurrValue = Value;
		}
	}
}

void FDynamicParameter::Update(float DeltaTime)
{
	if (DurationSec > 0.0f)
	{
		float TimeFraction = CurrTimeSec / DurationSec;
		if (TimeFraction < 1.0f)
		{
			CurrValue = DeltaValue * TimeFraction + StartValue;
		}
		else
		{
			CurrValue = StartValue + DeltaValue;
			DurationSec = 0.0f;
		}
		CurrTimeSec += DeltaTime;
	}
}


/*-----------------------------------------------------------------------------
	FAudioDevice implementation.
-----------------------------------------------------------------------------*/

FAudioDevice::FAudioDevice()
	: CommonAudioPool(nullptr)
	, CommonAudioPoolFreeBytes(0)
	, DeviceHandle(INDEX_NONE)
	, SpatializationPluginInterface(nullptr)
	, ReverbPluginInterface(nullptr)
	, OcclusionInterface(nullptr)
	, PluginListeners()
	, CurrentTick(0)
	, TestAudioComponent(nullptr)
	, DebugState(DEBUGSTATE_None)
	, TransientMasterVolume(1.0f)
	, GlobalPitchScale(1.0f)
	, LastUpdateTime(FPlatformTime::Seconds())
	, NextResourceID(1)
	, BaseSoundMix(nullptr)
	, DefaultBaseSoundMix(nullptr)
	, Effects(nullptr)
	, CurrentReverbEffect(nullptr)
	, PlatformAudioHeadroom(1.0f)
	, DefaultReverbSendLevel(0.2f)
	, bHRTFEnabledForAll_OnGameThread(false)
	, bGameWasTicking(true)
	, bDisableAudioCaching(false)
	, bIsAudioDeviceHardwareInitialized(false)
	, bAudioMixerModuleLoaded(false)
	, bSpatializationIsExternalSend(false)
	, bOcclusionIsExternalSend(false)
	, bReverbIsExternalSend(false)
	, bStartupSoundsPreCached(false)
	, bSpatializationInterfaceEnabled(false)
	, bOcclusionInterfaceEnabled(false)
	, bReverbInterfaceEnabled(false)
	, bPluginListenersInitialized(false)
	, bHRTFEnabledForAll(false)
	, bIsDeviceMuted(false)
	, bIsInitialized(false)
	, AudioClock(0.0)
	, bAllowCenterChannel3DPanning(false)
	, bHasActivatedReverb(false)
	, bAllowVirtualizedSounds(true)
	, bUseAttenuationForNonGameWorlds(false)
#if !UE_BUILD_SHIPPING
	, RequestedAudioStats(0)
#endif
	, DeviceDeltaTime(0.0f)
	, ConcurrencyManager(this)
{
}

FAudioEffectsManager* FAudioDevice::CreateEffectsManager()
{
	return new FAudioEffectsManager(this);
}

FAudioQualitySettings FAudioDevice::GetQualityLevelSettings()
{
	const UAudioSettings* AudioSettings = GetDefault<UAudioSettings>();
	return AudioSettings->GetQualityLevelSettings(GEngine->GetGameUserSettings()->GetAudioQualityLevel());
}

bool FAudioDevice::Init(int32 InMaxChannels)
{
	if (bIsInitialized)
	{
		return true;
	}

	bool bDeferStartupPrecache = false;
	
	PluginListeners.Reset();

	// initialize max channels taking into account platform configurations
	// Get a copy of the platform-specific settings (overriden by platforms)
	PlatformSettings = GetPlatformSettings();

	// MaxChannels is the min of the platform-specific value and the max value in the quality settings (InMaxChannels)
	MaxChannels = PlatformSettings.MaxChannels > 0 ? FMath::Min(PlatformSettings.MaxChannels, InMaxChannels) : InMaxChannels;

	// Mixed sample rate is set by the platform
	SampleRate = PlatformSettings.SampleRate;

	check(MaxChannels != 0);

	verify(GConfig->GetInt(TEXT("Audio"), TEXT("CommonAudioPoolSize"), CommonAudioPoolSize, GEngineIni));

	// If this is true, skip the initial startup precache so we can do it later in the flow
	GConfig->GetBool(TEXT("Audio"), TEXT("DeferStartupPrecache"), bDeferStartupPrecache, GEngineIni);

	// Get an optional engine ini setting for platform headroom. 
	float Headroom = 0.0f; // in dB
	if (GConfig->GetFloat(TEXT("Audio"), TEXT("PlatformHeadroomDB"), Headroom, GEngineIni))
	{
		// Convert dB to linear volume
		PlatformAudioHeadroom = FMath::Pow(10.0f, Headroom / 20.0f);
	}

	const UAudioSettings* AudioSettings = GetDefault<UAudioSettings>();

	bAllowCenterChannel3DPanning = AudioSettings->bAllowCenterChannel3DPanning;
	bAllowVirtualizedSounds = AudioSettings->bAllowVirtualizedSounds;
	DefaultReverbSendLevel = AudioSettings->DefaultReverbSendLevel;

	const FSoftObjectPath DefaultBaseSoundMixName = GetDefault<UAudioSettings>()->DefaultBaseSoundMix;
	if (DefaultBaseSoundMixName.IsValid())
	{
		DefaultBaseSoundMix = LoadObject<USoundMix>(nullptr, *DefaultBaseSoundMixName.ToString());
	}

	GetDefault<USoundGroups>()->Initialize();

	// Parses sound classes.
	InitSoundClasses();
	InitSoundEffectPresets();

	// Audio mixer needs to create effects manager before initializing the plugins.
	if (IsAudioMixerEnabled())
	{
		// create a platform specific effects manager
		Effects = CreateEffectsManager();
	}

	

	//Get the requested spatialization plugin and set it up.
	IAudioSpatializationFactory* SpatializationPluginFactory = AudioPluginUtilities::GetDesiredSpatializationPlugin(AudioPluginUtilities::CurrentPlatform);
	if (SpatializationPluginFactory != nullptr)
	{
		SpatializationPluginInterface = SpatializationPluginFactory->CreateNewSpatializationPlugin(this);
		if (!IsAudioMixerEnabled())
		{
			//Set up initialization parameters for system level effect plugins:
			FAudioPluginInitializationParams PluginInitializationParams;
			PluginInitializationParams.SampleRate = SampleRate;
			PluginInitializationParams.NumSources = MaxChannels;
			PluginInitializationParams.BufferLength = PlatformSettings.CallbackBufferFrameSize;
			PluginInitializationParams.AudioDevicePtr = this;
			
			SpatializationPluginInterface->Initialize(PluginInitializationParams);
		}

		bSpatializationInterfaceEnabled = true;
		bSpatializationIsExternalSend = SpatializationPluginFactory->IsExternalSend();
		UE_LOG(LogAudio, Log, TEXT("Using Audio Spatialization Plugin: %s is external send: %d"), *(SpatializationPluginFactory->GetDisplayName()), bSpatializationIsExternalSend);
	}
	else
	{
		UE_LOG(LogAudio, Log, TEXT("Using built-in audio spatialization."));
	}

	//Get the requested reverb plugin and set it up:
	IAudioReverbFactory* ReverbPluginFactory = AudioPluginUtilities::GetDesiredReverbPlugin(AudioPluginUtilities::CurrentPlatform);
	if (ReverbPluginFactory != nullptr)
	{
		ReverbPluginInterface = ReverbPluginFactory->CreateNewReverbPlugin(this);
		bReverbInterfaceEnabled = true;
		bReverbIsExternalSend = ReverbPluginFactory->IsExternalSend();
		UE_LOG(LogAudio, Log, TEXT("Audio Reverb Plugin: %s"), *(ReverbPluginFactory->GetDisplayName()));

	}
	else
	{
		UE_LOG(LogAudio, Log, TEXT("Using built-in audio reverb."));
	}

	//Get the requested occlusion plugin and set it up.
	IAudioOcclusionFactory* OcclusionPluginFactory = AudioPluginUtilities::GetDesiredOcclusionPlugin(AudioPluginUtilities::CurrentPlatform);
	if (OcclusionPluginFactory != nullptr)
		{
		OcclusionInterface = OcclusionPluginFactory->CreateNewOcclusionPlugin(this);
		bOcclusionInterfaceEnabled = true;
		bOcclusionIsExternalSend = OcclusionPluginFactory->IsExternalSend();
		UE_LOG(LogAudio, Display, TEXT("Audio Occlusion Plugin: %s"), *(OcclusionPluginFactory->GetDisplayName()));
		}
	else
	{
		UE_LOG(LogAudio, Display, TEXT("Using built-in audio occlusion."));
	}

	// allow the platform to startup
	if (InitializeHardware() == false)
	{
		// Could not initialize hardware, teardown anything that was set up during initialization
		Teardown();

		return false;
	}

	// create a platform specific effects manager
	// if this is the audio mixer, we initialized the effects manager before the hardware
 	if (!IsAudioMixerEnabled())
	{
		Effects = CreateEffectsManager();
	}

	InitSoundSources();
	
	// Make sure the Listeners array has at least one entry, so we don't have to check for Listeners.Num() == 0 all the time
	Listeners.Add(FListener(this));
	ListenerTransforms.AddDefaulted();
	InverseListenerTransform.SetIdentity();

	if (!bDeferStartupPrecache)
	{
		PrecacheStartupSounds();
	}

	UE_LOG(LogInit, Log, TEXT("FAudioDevice initialized."));

	bIsInitialized = true;

	return true;
}

float FAudioDevice::GetLowPassFilterResonance() const
{
	return GetDefault<UAudioSettings>()->LowPassFilterResonance;
}

void FAudioDevice::PrecacheStartupSounds()
{
	// Iterate over all already loaded sounds and precache them. This relies on Super::Init in derived classes to be called last.
	if (!GIsEditor && GEngine && GEngine->UseSound() )
	{
		for (TObjectIterator<USoundWave> It; It; ++It)
		{
			USoundWave* SoundWave = *It;
			Precache(SoundWave);
		}

		bStartupSoundsPreCached = true;
	}
}

void FAudioDevice::SetMaxChannels(int32 InMaxChannels)
{
	if (InMaxChannels > Sources.Num())
	{
		UE_LOG(LogAudio, Warning, TEXT("Can't increase channels past starting number!"));
		return;
	}

	MaxChannels = InMaxChannels;
}

int32 FAudioDevice::GetMaxChannels() const
{
	if (AudioChannelCountCVar > 0 && AudioChannelCountCVar < Sources.Num())
	{
		return AudioChannelCountCVar;
	}

	return MaxChannels;
}

void FAudioDevice::Teardown()
{
	// Do a fadeout to prevent clicking on shutdown
	FadeOut();

	// Flush stops all sources so sources can be safely deleted below.
	Flush(nullptr);

	// Clear out the EQ/Reverb/LPF effects
	if (Effects)
	{
		delete Effects;
		Effects = nullptr;
	}

	for (TAudioPluginListenerPtr PluginListener : PluginListeners)
	{
		PluginListener->OnListenerShutdown(this);
	}
	
	
	// let platform shutdown
	TeardownHardware();

	SoundMixClassEffectOverrides.Empty();

	// Note: we don't free audio buffers at this stage since they are managed in the audio device manager

	// Must be after FreeBufferResource as that potentially stops sources
	for (int32 Index = 0; Index < Sources.Num(); Index++)
	{
		Sources[Index]->Stop();
		delete Sources[Index];
	}
	Sources.Reset();
	FreeSources.Reset();

	SpatializationPluginInterface.Reset();
	bSpatializationInterfaceEnabled = false;

	ReverbPluginInterface.Reset();
	bReverbInterfaceEnabled = false;

	OcclusionInterface.Reset();
	bOcclusionInterfaceEnabled = false;

	PluginListeners.Reset();
}

void FAudioDevice::Suspend(bool bGameTicking)
{
	HandlePause(bGameTicking, true);
}

void FAudioDevice::CountBytes(FArchive& Ar)
{
	Sources.CountBytes(Ar);
	// The buffers are stored on the audio device since they are shared amongst all audio devices
	// Though we are going to count them when querying an individual audio device object about its bytes
	GEngine->GetAudioDeviceManager()->Buffers.CountBytes(Ar);
	FreeSources.CountBytes(Ar);
	WaveInstanceSourceMap.CountBytes(Ar);
	Ar.CountBytes(sizeof(FWaveInstance) * WaveInstanceSourceMap.Num(), sizeof(FWaveInstance) * WaveInstanceSourceMap.Num());
	SoundClasses.CountBytes(Ar);
	SoundMixModifiers.CountBytes(Ar);
}

void FAudioDevice::AddReferencedObjects(FReferenceCollector& Collector)
{	
	Collector.AddReferencedObject(DefaultBaseSoundMix);
	Collector.AddReferencedObjects(PrevPassiveSoundMixModifiers);
	Collector.AddReferencedObjects(SoundMixModifiers);

	for (TPair<FName, FActivatedReverb>& ActivatedReverbPair : ActivatedReverbs)
	{
		Collector.AddReferencedObject(ActivatedReverbPair.Value.ReverbSettings.ReverbEffect);
	}

	if (Effects)
	{
		Effects->AddReferencedObjects(Collector);
	}

	for (FActiveSound* ActiveSound : ActiveSounds)
	{
		ActiveSound->AddReferencedObjects(Collector);
	}
}

void FAudioDevice::ResetInterpolation()
{
	check(IsInAudioThread());

	for (FListener& Listener : Listeners)
	{
		Listener.InteriorStartTime = 0.f;
		Listener.InteriorEndTime = 0.f;
		Listener.ExteriorEndTime = 0.f;
		Listener.InteriorLPFEndTime = 0.f;
		Listener.ExteriorLPFEndTime = 0.f;

		Listener.InteriorVolumeInterp = 0.0f;
		Listener.InteriorLPFInterp = 0.0f;
		Listener.ExteriorVolumeInterp = 0.0f;
		Listener.ExteriorLPFInterp = 0.0f;
	}

	// Reset sound class properties to defaults
	for (TMap<USoundClass*, FSoundClassProperties>::TIterator It(SoundClasses); It; ++It)
	{
		USoundClass* SoundClass = It.Key();
		if (SoundClass)
		{
			It.Value() = SoundClass->Properties;
		}
	}

	SoundMixModifiers.Reset();
	PrevPassiveSoundMixModifiers.Reset();
	BaseSoundMix = nullptr;

	// reset audio effects
	if (Effects)
	{
		Effects->ResetInterpolation();
	}
}

void FAudioDevice::EnableRadioEffect(bool bEnable)
{
	if (bEnable)
	{
		SetMixDebugState(DEBUGSTATE_None);
	}
	else
	{
		UE_LOG(LogAudio, Log, TEXT("Radio disabled for all sources"));
		SetMixDebugState(DEBUGSTATE_DisableRadio);
	}
}

#if !UE_BUILD_SHIPPING
bool FAudioDevice::HandleShowSoundClassHierarchyCommand(const TCHAR* Cmd, FOutputDevice& Ar)
{
	FAudioThreadSuspendContext AudioThreadSuspend;

	ShowSoundClassHierarchy(Ar);
	return true;
}

bool FAudioDevice::HandleListWavesCommand(const TCHAR* Cmd, FOutputDevice& Ar)
{
	FAudioThreadSuspendContext AudioThreadSuspend;

	TArray<FWaveInstance*> WaveInstances;
	int32 FirstActiveIndex = GetSortedActiveWaveInstances(WaveInstances, ESortedActiveWaveGetType::QueryOnly);

	for (int32 InstanceIndex = FirstActiveIndex; InstanceIndex < WaveInstances.Num(); InstanceIndex++)
	{
		FWaveInstance* WaveInstance = WaveInstances[ InstanceIndex ];
		FSoundSource* Source = WaveInstanceSourceMap.FindRef(WaveInstance);
		UAudioComponent* AudioComponent = UAudioComponent::GetAudioComponentFromID(WaveInstance->ActiveSound->GetAudioComponentID());
		AActor* SoundOwner = AudioComponent ? AudioComponent->GetOwner() : nullptr;
		Ar.Logf(TEXT("%4i.    %s %6.2f %6.2f  %s   %s"), InstanceIndex, Source ? TEXT("Yes") : TEXT(" No"), WaveInstance->ActiveSound->PlaybackTime, WaveInstance->GetVolume(), *WaveInstance->WaveData->GetPathName(), SoundOwner ? *SoundOwner->GetName() : TEXT("None"));
	}

	Ar.Logf(TEXT("Total: %i"), WaveInstances.Num()-FirstActiveIndex);

	return true;
}

void FAudioDevice::GetSoundClassInfo(TMap<FName, FAudioClassInfo>& AudioClassInfos)
{
	// Iterate over all sound cues to get a unique map of sound node waves to class names
	TMap<USoundWave*, FName> SoundWaveClasses;

	for (TObjectIterator<USoundCue> CueIt; CueIt; ++CueIt)
	{
		TArray<USoundNodeWavePlayer*> WavePlayers;

		USoundCue* SoundCue = *CueIt;
		SoundCue->RecursiveFindNode<USoundNodeWavePlayer>(SoundCue->FirstNode, WavePlayers);

		for (int32 WaveIndex = 0; WaveIndex < WavePlayers.Num(); ++WaveIndex)
		{
			// Presume one class per sound node wave
			USoundWave *SoundWave = WavePlayers[ WaveIndex ]->GetSoundWave();
			if (SoundWave && SoundCue->GetSoundClass())
			{
				SoundWaveClasses.Add(SoundWave, SoundCue->GetSoundClass()->GetFName());
			}
		}
	}

	// Add any sound node waves that are not referenced by sound cues
	for (TObjectIterator<USoundWave> WaveIt; WaveIt; ++WaveIt)
	{
		USoundWave* SoundWave = *WaveIt;
		if (SoundWaveClasses.Find(SoundWave) == nullptr)
		{
			SoundWaveClasses.Add(SoundWave, NAME_UnGrouped);
		}
	}

	// Collate the data into something useful
	for (TMap<USoundWave*, FName>::TIterator MapIter(SoundWaveClasses); MapIter; ++MapIter)
	{
		USoundWave* SoundWave = MapIter.Key();
		FName ClassName = MapIter.Value();

		FAudioClassInfo* AudioClassInfo = AudioClassInfos.Find(ClassName);
		if (AudioClassInfo == nullptr)
		{
			FAudioClassInfo NewAudioClassInfo;

			NewAudioClassInfo.NumResident = 0;
			NewAudioClassInfo.SizeResident = 0;
			NewAudioClassInfo.NumRealTime = 0;
			NewAudioClassInfo.SizeRealTime = 0;

			AudioClassInfos.Add(ClassName, NewAudioClassInfo);

			AudioClassInfo = AudioClassInfos.Find(ClassName);
			check(AudioClassInfo);
		}

#if !WITH_EDITOR
		AudioClassInfo->SizeResident += SoundWave->GetCompressedDataSize(GetRuntimeFormat(SoundWave));
		AudioClassInfo->NumResident++;
#else
		switch(SoundWave->DecompressionType)
		{
		case DTYPE_Native:
		case DTYPE_Preview:
			AudioClassInfo->SizeResident += SoundWave->RawPCMDataSize;
			AudioClassInfo->NumResident++;
			break;

		case DTYPE_RealTime:
			AudioClassInfo->SizeRealTime += SoundWave->GetCompressedDataSize(GetRuntimeFormat(SoundWave));
			AudioClassInfo->NumRealTime++;
			break;

		case DTYPE_Streaming:
			// Add these to real time count for now - eventually compressed data won't be loaded &
			// might have a class info entry of their own
			AudioClassInfo->SizeRealTime += SoundWave->GetCompressedDataSize(GetRuntimeFormat(SoundWave));
			AudioClassInfo->NumRealTime++;
			break;

		case DTYPE_Setup:
		case DTYPE_Invalid:
		default:
			break;
		}
#endif
	}
}

bool FAudioDevice::HandleListSoundClassesCommand(const TCHAR* Cmd, FOutputDevice& Ar)
{
	TMap<FName, FAudioClassInfo> AudioClassInfos;

	GetSoundClassInfo(AudioClassInfos);

	Ar.Logf(TEXT("Listing all sound classes."));

	// Display the collated data
	int32 TotalSounds = 0;
	for (TMap<FName, FAudioClassInfo>::TIterator ACIIter(AudioClassInfos); ACIIter; ++ACIIter)
	{
		FName ClassName = ACIIter.Key();
		FAudioClassInfo* ACI = AudioClassInfos.Find(ClassName);

		FString Line = FString::Printf(TEXT("Class '%s' has %d resident sounds taking %.2f kb"), *ClassName.ToString(), ACI->NumResident, ACI->SizeResident / 1024.0f);
		TotalSounds += ACI->NumResident;
		if (ACI->NumRealTime > 0)
		{
			Line += FString::Printf(TEXT(", and %d real time sounds taking %.2f kb "), ACI->NumRealTime, ACI->SizeRealTime / 1024.0f);
			TotalSounds += ACI->NumRealTime;
		}

		Ar.Logf(TEXT("%s"), *Line);
	}

	Ar.Logf(TEXT("%d total sounds in %d classes"), TotalSounds, AudioClassInfos.Num());
	return true;
}

void FAudioDevice::ShowSoundClassHierarchy(FOutputDevice& Ar, USoundClass* InSoundClass, int32 Indent ) const
{
	TArray<USoundClass*> SoundClassesToShow;
	if (InSoundClass)
	{
		SoundClassesToShow.Add(InSoundClass);
	}
	else
	{
		for (TMap<USoundClass*, FSoundClassProperties>::TConstIterator It(SoundClasses); It; ++It)
		{
			USoundClass* SoundClass = It.Key();
			if (SoundClass && SoundClass->ParentClass == nullptr)
			{
				SoundClassesToShow.Add(SoundClass);
			}
		}
	}

	for (int32 Index=0; Index < SoundClassesToShow.Num(); ++Index)
	{
		USoundClass* SoundClass = SoundClassesToShow[Index];
		if (Indent > 0)
		{
			Ar.Logf(TEXT("%s|- %s"), FCString::Spc(Indent*2), *SoundClass->GetName());
		}
		else
		{
			Ar.Logf(TEXT("%s"), *SoundClass->GetName());
		}
		for (int i = 0; i < SoundClass->ChildClasses.Num(); ++i)
		{
			if (SoundClass->ChildClasses[i])
			{
				ShowSoundClassHierarchy(Ar, SoundClass->ChildClasses[i], Indent+1);
			}
		}
	}
}

int32 PrecachedRealtime = 0;
int32 PrecachedNative = 0;
int32 TotalNativeSize = 0;
float AverageNativeLength = 0.f;
TMap<int32, int32> NativeChannelCount;
TMap<int32, int32> NativeSampleRateCount;

bool FAudioDevice::HandleDumpSoundInfoCommand(const TCHAR* Cmd, FOutputDevice& Ar)
{
	FAudioThreadSuspendContext AudioThreadSuspend;

	Ar.Logf(TEXT("Native Count: %d\nRealtime Count: %d\n"), PrecachedNative, PrecachedRealtime);
	float AverageSize = 0.0f;
	if (PrecachedNative != 0)
	{
		PrecachedNative = TotalNativeSize / PrecachedNative;
	}
	Ar.Logf(TEXT("Average Length: %.3g\nTotal Size: %d\nAverage Size: %.3g\n"), AverageNativeLength, TotalNativeSize, PrecachedNative);
	Ar.Logf(TEXT("Channel counts:\n"));
	for (auto CountIt = NativeChannelCount.CreateConstIterator(); CountIt; ++CountIt)
	{
		Ar.Logf(TEXT("\t%d: %d"),CountIt.Key(), CountIt.Value());
	}
	Ar.Logf(TEXT("Sample rate counts:\n"));
	for (auto SampleRateIt = NativeSampleRateCount.CreateConstIterator(); SampleRateIt; ++SampleRateIt)
	{
		Ar.Logf(TEXT("\t%d: %d"), SampleRateIt.Key(), SampleRateIt.Value());
	}
	return true;
}


bool FAudioDevice::HandleListSoundClassVolumesCommand(const TCHAR* Cmd, FOutputDevice& Ar)
{
	FAudioThreadSuspendContext AudioThreadSuspend;

	Ar.Logf(TEXT("SoundClass Volumes: (Volume, Pitch)"));

	for (TMap<USoundClass*, FSoundClassProperties>::TIterator It(SoundClasses); It; ++It)
	{
		USoundClass* SoundClass = It.Key();
		if (SoundClass)
		{
			const FSoundClassProperties& CurClass = It.Value();

			Ar.Logf(TEXT("Cur (%3.2f, %3.2f) for SoundClass %s"), CurClass.Volume, CurClass.Pitch, *SoundClass->GetName());
		}
	}

	return true;
}

bool FAudioDevice::HandleListAudioComponentsCommand(const TCHAR* Cmd, FOutputDevice& Ar)
{
	FAudioThreadSuspendContext AudioThreadSuspend;

	int32 Count = 0;
	Ar.Logf(TEXT("AudioComponent Dump"));
	for (TObjectIterator<UAudioComponent> It; It; ++It)
	{
		UAudioComponent* AudioComponent = *It;
		UObject* Outer = It->GetOuter();
		UObject* Owner = It->GetOwner();
		Ar.Logf(TEXT("    0x%p: %s, %s, %s, %s"),
			AudioComponent,
			*(It->GetPathName()),
			It->Sound ? *(It->Sound->GetPathName()) : TEXT("NO SOUND"),
			Outer ? *(Outer->GetPathName()) : TEXT("NO OUTER"),
			Owner ? *(Owner->GetPathName()) : TEXT("NO OWNER"));
		Ar.Logf(TEXT("        bAutoDestroy....................%s"), AudioComponent->bAutoDestroy ? TEXT("true") : TEXT("false"));
		Ar.Logf(TEXT("        bStopWhenOwnerDestroyed.........%s"), AudioComponent->bStopWhenOwnerDestroyed ? TEXT("true") : TEXT("false"));
		Ar.Logf(TEXT("        bShouldRemainActiveIfDropped....%s"), AudioComponent->bShouldRemainActiveIfDropped ? TEXT("true") : TEXT("false"));
		Ar.Logf(TEXT("        bIgnoreForFlushing..............%s"), AudioComponent->bIgnoreForFlushing ? TEXT("true") : TEXT("false"));
		Count++;
	}
	Ar.Logf(TEXT("AudioComponent Total = %d"), Count);

	Ar.Logf(TEXT("AudioDevice 0x%p has %d ActiveSounds"),
		this, ActiveSounds.Num());
	for (int32 ASIndex = 0; ASIndex < ActiveSounds.Num(); ASIndex++)
	{
		const FActiveSound* ActiveSound = ActiveSounds[ASIndex];
		UAudioComponent* AComp = UAudioComponent::GetAudioComponentFromID(ActiveSound->GetAudioComponentID());
		if (AComp)
		{
			Ar.Logf(TEXT("    0x%p: %4d - %s, %s, %s, %s"),
				AComp,
				ASIndex,
				*(AComp->GetPathName()),
				ActiveSound->Sound ? *(ActiveSound->Sound->GetPathName()) : TEXT("NO SOUND"),
				AComp->GetOuter() ? *(AComp->GetOuter()->GetPathName()) : TEXT("NO OUTER"),
				AComp->GetOwner() ? *(AComp->GetOwner()->GetPathName()) : TEXT("NO OWNER"));
		}
		else
		{
			Ar.Logf(TEXT("    %4d - %s, %s"), 
				ASIndex, 
				ActiveSound->Sound ? *(ActiveSound->Sound->GetPathName()) : TEXT("NO SOUND"),
				TEXT("NO COMPONENT"));
		}
	}
	return true;
}

bool FAudioDevice::HandleListSoundDurationsCommand(const TCHAR* Cmd, FOutputDevice& Ar)
{
	Ar.Logf(TEXT("Sound,Duration,Channels"));
	for (TObjectIterator<USoundWave> It; It; ++It)
	{
		USoundWave* SoundWave = *It;
		Ar.Logf(TEXT("%s,%f,%i"), *SoundWave->GetPathName(), SoundWave->Duration, SoundWave->NumChannels);
	}
	return true;
}


bool FAudioDevice::HandlePlaySoundCueCommand(const TCHAR* Cmd, FOutputDevice& Ar)
{
	// Stop any existing sound playing
	if (!TestAudioComponent.IsValid())
	{
		TestAudioComponent = NewObject<UAudioComponent>();
	}

	UAudioComponent* AudioComp = TestAudioComponent.Get();
	if (AudioComp != nullptr)
	{
		AudioComp->Stop();

		// Load up an arbitrary cue
		USoundCue* Cue = LoadObject<USoundCue>(nullptr, Cmd, nullptr, LOAD_None, nullptr);
		if (Cue != nullptr)
		{
			AudioComp->Sound = Cue;
			AudioComp->bAllowSpatialization = false;
			AudioComp->bAutoDestroy = true;
			AudioComp->Play();

			TArray<USoundNodeWavePlayer*> WavePlayers;
			Cue->RecursiveFindNode<USoundNodeWavePlayer>(Cue->FirstNode, WavePlayers);
			for (int32 i = 0; i < WavePlayers.Num(); ++i)
			{
				USoundWave* SoundWave = WavePlayers[ i ]->GetSoundWave();
				if (SoundWave)
				{
					SoundWave->LogSubtitle(Ar);
				}
			}
		}
	}
	return true;
}

bool FAudioDevice::HandlePlaySoundWaveCommand(const TCHAR* Cmd, FOutputDevice& Ar)
{
	// Stop any existing sound playing
	if (!TestAudioComponent.IsValid())
	{
		TestAudioComponent = NewObject<UAudioComponent>();
	}

	UAudioComponent* AudioComp = TestAudioComponent.Get();
	if (AudioComp != nullptr)
	{
		AudioComp->Stop();

		// Load up an arbitrary wave
		USoundWave* Wave = LoadObject<USoundWave>(NULL, Cmd, NULL, LOAD_None, NULL);
		if (Wave != NULL)
		{
			AudioComp->Sound = Wave;
			AudioComp->bAllowSpatialization = false;
			AudioComp->bAutoDestroy = true;
			AudioComp->Play();

			Wave->LogSubtitle(Ar);
		}
	}
	return true;
}

bool FAudioDevice::HandleSetBaseSoundMixCommand(const TCHAR* Cmd, FOutputDevice& Ar)
{
	// Ar.Logf(TEXT("Setting base sound mix '%s'"), Cmd);
	const FName NewMix = FName(Cmd);
	USoundMix* SoundMix = nullptr;

	for (TObjectIterator<USoundMix> It; It; ++It)
	{
		if (NewMix == It->GetFName())
		{
			SoundMix = *It;
			break;
		}
	}

	if (SoundMix)
	{
		SetBaseSoundMix(SoundMix);
	}
	else
	{
		Ar.Logf(TEXT("Unknown SoundMix: %s"), *NewMix.ToString());
	}
	return true;
}

bool FAudioDevice::HandleIsolateDryAudioCommand(const TCHAR* Cmd, FOutputDevice& Ar)
{
	Ar.Logf(TEXT("Dry audio isolated"));
	SetMixDebugState(DEBUGSTATE_IsolateDryAudio);
	return true;
}

bool FAudioDevice::HandleIsolateReverbCommand(const TCHAR* Cmd, FOutputDevice& Ar)
{
	Ar.Logf(TEXT("Reverb audio isolated"));
	SetMixDebugState(DEBUGSTATE_IsolateReverb);
	return true;
}

bool FAudioDevice::HandleTestLPFCommand(const TCHAR* Cmd, FOutputDevice& Ar)
{
	Ar.Logf(TEXT("LPF set to max for all sources"));
	SetMixDebugState(DEBUGSTATE_TestLPF);
	return true;
}

bool FAudioDevice::HandleTestStereoBleedCommand(const TCHAR* Cmd, FOutputDevice& Ar)
{
	Ar.Logf(TEXT("StereoBleed set to max for all sources"));
	SetMixDebugState(DEBUGSTATE_TestStereoBleed);
	return true;
}

bool FAudioDevice::HandleTestLFEBleedCommand(const TCHAR* Cmd, FOutputDevice& Ar)
{
	Ar.Logf(TEXT("LFEBleed set to max for all sources"));
	SetMixDebugState(DEBUGSTATE_TestLFEBleed);
	return true;
}

bool FAudioDevice::HandleDisableLPFCommand(const TCHAR* Cmd, FOutputDevice& Ar)
{
	Ar.Logf(TEXT("LPF disabled for all sources"));
	SetMixDebugState(DEBUGSTATE_DisableLPF);
	return true;
}

bool FAudioDevice::HandleDisableRadioCommand(const TCHAR* Cmd, FOutputDevice& Ar)
{
	EnableRadioEffect(false);
	return true;
}

bool FAudioDevice::HandleEnableRadioCommand(const TCHAR* Cmd, FOutputDevice& Ar)
{
	EnableRadioEffect(true);
	return true;
}

bool FAudioDevice::HandleResetSoundStateCommand(const TCHAR* Cmd, FOutputDevice& Ar)
{
	Ar.Logf(TEXT("All volumes reset to their defaults; all test filters removed"));
	SetMixDebugState(DEBUGSTATE_None);
	return true;
}

bool FAudioDevice::HandleToggleSpatializationExtensionCommand(const TCHAR* Cmd, FOutputDevice& Ar)
{
	SetSpatializationInterfaceEnabled(!bSpatializationInterfaceEnabled);

	return true;
}

bool FAudioDevice::HandleEnableHRTFForAllCommand(const TCHAR* Cmd, FOutputDevice& Ar)
{
	SetHRTFEnabledForAll(!bHRTFEnabledForAll_OnGameThread);

	return true;
}

bool FAudioDevice::HandleSoloCommand(const TCHAR* Cmd, FOutputDevice& Ar)
{
	// Apply the solo to the given device
	FAudioDeviceManager* DeviceManager = GEngine->GetAudioDeviceManager();
	if (DeviceManager)
	{
		DeviceManager->SetSoloDevice(DeviceHandle);
	}
	return true;
}

bool FAudioDevice::HandleClearSoloCommand(const TCHAR* Cmd, FOutputDevice& Ar)
{
	FAudioDeviceManager* DeviceManager = GEngine->GetAudioDeviceManager();
	if (DeviceManager)
	{
		DeviceManager->SetSoloDevice(INDEX_NONE);
	}
	return true;
}

bool FAudioDevice::HandlePlayAllPIEAudioCommand(const TCHAR* Cmd, FOutputDevice& Ar)
{
	FAudioDeviceManager* DeviceManager = GEngine->GetAudioDeviceManager();
	if (DeviceManager)
	{
		DeviceManager->TogglePlayAllDeviceAudio();
	}
	return true;
}

bool FAudioDevice::HandleAudio3dVisualizeCommand(const TCHAR* Cmd, FOutputDevice& Ar)
{
	FAudioDeviceManager* DeviceManager = GEngine->GetAudioDeviceManager();
	if (DeviceManager)
	{
		DeviceManager->ToggleVisualize3dDebug();
	}
	return true;
}

bool FAudioDevice::HandleAudioSoloSoundClass(const TCHAR* Cmd, FOutputDevice& Ar)
{
	FAudioDeviceManager* DeviceManager = GEngine->GetAudioDeviceManager();
	if (DeviceManager)
	{
		DeviceManager->SetDebugSoloSoundClass(Cmd);
	}
	return true;
}

bool FAudioDevice::HandleAudioSoloSoundWave(const TCHAR* Cmd, FOutputDevice& Ar)
{
	FAudioDeviceManager* DeviceManager = GEngine->GetAudioDeviceManager();
	if (DeviceManager)
	{
		DeviceManager->SetDebugSoloSoundWave(Cmd);
	}
	return true;
}

bool FAudioDevice::HandleAudioSoloSoundCue(const TCHAR* Cmd, FOutputDevice& Ar)
{
	FAudioDeviceManager* DeviceManager = GEngine->GetAudioDeviceManager();
	if (DeviceManager)
	{
		DeviceManager->SetDebugSoloSoundCue(Cmd);
	}
	return true;
}

bool FAudioDevice::HandleAudioMixerDebugSound(const TCHAR* Cmd, FOutputDevice& Ar)
{
	FAudioDeviceManager* DeviceManager = GEngine->GetAudioDeviceManager();
	if (DeviceManager)
	{
		DeviceManager->SetAudioMixerDebugSound(Cmd);
	}
	return true;
}

bool FAudioDevice::HandleSoundClassFixup(const TCHAR* Cmd, FOutputDevice& Ar)
{
#if WITH_EDITOR
	// Get asset registry module
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

	TArray<FAssetData> AssetDataArray;
	AssetRegistryModule.Get().GetAssetsByClass(USoundClass::StaticClass()->GetFName(), AssetDataArray);

	static const FString EngineDir = TEXT("/Engine/");
	FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
	TArray<FAssetRenameData> RenameData;
	for (FAssetData AssetData : AssetDataArray)
	{
		USoundClass* SoundClass = Cast<USoundClass>(AssetData.GetAsset());
		if (SoundClass != nullptr && !SoundClass->GetPathName().Contains(EngineDir))
		{
			// If this sound class is within another sound class package, create a new uniquely named sound class
			FString OutermostFullName = SoundClass->GetOutermost()->GetName();
			FString ExistingSoundClassFullName = SoundClass->GetPathName();
			int32 CharPos = INDEX_NONE;

			FString OutermostShortName = FPaths::GetCleanFilename(OutermostFullName);

			OutermostShortName = FString::Printf(TEXT("%s.%s"), *OutermostShortName, *OutermostShortName);

			FString ExistingSoundClassShortName = FPaths::GetCleanFilename(ExistingSoundClassFullName);
			if (ExistingSoundClassShortName != OutermostShortName)
			{
				// Construct a proper new asset name/path 
				FString ExistingSoundClassPath = ExistingSoundClassFullName.Left(CharPos);

				ExistingSoundClassShortName.FindLastChar('.', CharPos);

				// Get the name of the new sound class
				FString NewSoundClassName = ExistingSoundClassShortName.RightChop(CharPos + 1);

				const FString PackagePath = FPackageName::GetLongPackagePath(AssetData.GetAsset()->GetOutermost()->GetName());

				// Use the asset tool module to get a unique name based on the existing name
 				FString OutNewPackageName;
				FString OutAssetName;
 				AssetToolsModule.Get().CreateUniqueAssetName(PackagePath + "/" + NewSoundClassName, TEXT(""), OutNewPackageName, OutAssetName);

				const FString LongPackagePath = FPackageName::GetLongPackagePath(OutNewPackageName);

				// Immediately perform the rename since there could be a naming conflict in the list and CreateUniqueAssetName won't be able to resolve
				// unless the assets are renamed immediately
				RenameData.Reset();
				RenameData.Add(FAssetRenameData(AssetData.GetAsset(), LongPackagePath, OutAssetName));
				AssetToolsModule.Get().RenameAssets(RenameData);
			}		
		}
	}
	return true;
#else
	return false;
#endif
}

bool FAudioDevice::HandleAudioMemoryInfo(const TCHAR* Cmd, FOutputDevice& Ar)
{
	struct FSoundWaveInfo
	{
		USoundWave* SoundWave;
		FResourceSizeEx ResourceSize;
		FString SoundGroupName;
		float Duration;
		bool bDecompressed;

		FSoundWaveInfo(USoundWave* InSoundWave, FResourceSizeEx InResourceSize, const FString& InSoundGroupName, float InDuration, bool bInDecompressed)
			: SoundWave(InSoundWave)
			, ResourceSize(InResourceSize)
			, SoundGroupName(InSoundGroupName)
			, Duration(InDuration)
			, bDecompressed(bInDecompressed)
		{}
	};

	struct FSoundWaveGroupInfo
	{
		FResourceSizeEx ResourceSize;
		FResourceSizeEx CompressedResourceSize;

		FSoundWaveGroupInfo()
			: ResourceSize()
			, CompressedResourceSize()
		{}
	};

	// Alpha sort the objects by path name
	struct FCompareSoundWave
	{
		FORCEINLINE bool operator()(const FSoundWaveInfo& A, const FSoundWaveInfo& B) const
		{
			return A.SoundWave->GetPathName() < B.SoundWave->GetPathName();
		}
	};

	const FString PathName = *(FPaths::ProfilingDir() + TEXT("MemReports/"));
	IFileManager::Get().MakeDirectory(*PathName);

	const FString Filename = CreateProfileFilename(TEXT("_audio_memreport.csv"), true);
	FString FilenameFull = PathName + Filename;

	UE_LOG(LogEngine, Log, TEXT("AudioMemReport: saving to %s"), *FilenameFull);

	FArchive* FileAr = IFileManager::Get().CreateDebugFileWriter(*FilenameFull);
	FOutputDeviceArchiveWrapper* FileArWrapper = new FOutputDeviceArchiveWrapper(FileAr);
	FOutputDevice* ReportAr = FileArWrapper;

	// Get the sound wave class
	UClass* SoundWaveClass = nullptr;
	ParseObject<UClass>(TEXT("class=SoundWave"), TEXT("CLASS="), SoundWaveClass, ANY_PACKAGE);

	TArray<FSoundWaveInfo> SoundWaveObjects;
	TMap<FString, FSoundWaveGroupInfo> SoundWaveGroupSizes;
	TArray<FString> SoundWaveGroupFolders;

	// Grab the list of folders to specifically track memory usage for
	FConfigSection* TrackedFolders = GConfig->GetSectionPrivate(TEXT("AudioMemReportFolders"), 0, 1, GEngineIni);
	if (TrackedFolders)
	{
		for (FConfigSectionMap::TIterator It(*TrackedFolders); It; ++It)
		{
			const FString& SoundFolder = *It.Value().GetValue();
			SoundWaveGroupSizes.Add(SoundFolder, FSoundWaveGroupInfo());
			SoundWaveGroupFolders.Add(SoundFolder);
		}
	}

	FResourceSizeEx TotalResourceSize;
	FResourceSizeEx CompressedResourceSize;
	FResourceSizeEx DecompressedResourceSize;
	int32 CompressedResourceCount = 0;

	if (SoundWaveClass != nullptr)
	{
		// Loop through all objects and find only sound wave objects
		for (TObjectIterator<USoundWave> It; It; ++It)
		{
			if (It->IsTemplate(RF_ClassDefaultObject))
			{
				continue;
			}

			// Get the resource size of the sound wave
			FResourceSizeEx TrueResourceSize = FResourceSizeEx(EResourceSizeMode::Exclusive);
			It->GetResourceSizeEx(TrueResourceSize);
			if (TrueResourceSize.GetTotalMemoryBytes() == 0)
			{
				continue;
			}

			USoundWave* SoundWave = *It;

			const FSoundGroup& SoundGroup = GetDefault<USoundGroups>()->GetSoundGroup(SoundWave->SoundGroup);
			float Duration = SoundWave->GetDuration();
			bool bDecompressed = SoundGroup.bAlwaysDecompressOnLoad || Duration < SoundGroup.DecompressedDuration;

			FString SoundGroupName;
			switch (SoundWave->SoundGroup)
			{
				case ESoundGroup::SOUNDGROUP_Default:
					SoundGroupName = TEXT("Default");
					break;

				case ESoundGroup::SOUNDGROUP_Effects:
					SoundGroupName = TEXT("Effects");
					break;

				case ESoundGroup::SOUNDGROUP_UI:
					SoundGroupName = TEXT("UI");
					break;

				case ESoundGroup::SOUNDGROUP_Music:
					SoundGroupName = TEXT("Music");
					break;

				case ESoundGroup::SOUNDGROUP_Voice:
					SoundGroupName = TEXT("Voice");
					break;

				default:
					SoundGroupName = SoundGroup.DisplayName;
					break;
			}

			// Add the info to the SoundWaveObjects array
			SoundWaveObjects.Add(FSoundWaveInfo(SoundWave, TrueResourceSize, SoundGroupName, Duration, bDecompressed));

			// Track total resource usage
			TotalResourceSize += TrueResourceSize;

			if (bDecompressed)
			{
				DecompressedResourceSize += TrueResourceSize;
				++CompressedResourceCount;
			}
			else
			{
				CompressedResourceSize += TrueResourceSize;
			}

			// Get the sound object path
			FString SoundWavePath = SoundWave->GetPathName();

			// Now track the resource size according to all the sub-directories
			FString SubDir;
			int32 Index = 0;

			for (int32 i = 0; i < SoundWavePath.Len(); ++i)
			{
				if (SoundWavePath[i] == '/')
				{
					if (SubDir.Len() > 0)
					{
						FSoundWaveGroupInfo* SubDirSize = SoundWaveGroupSizes.Find(SubDir);
						if (SubDirSize)
						{
							SubDirSize->ResourceSize += TrueResourceSize;
							if (bDecompressed)
							{
								SubDirSize->CompressedResourceSize += TrueResourceSize;
							}
						}
					}
					SubDir = TEXT("");
				}
				else
				{
					SubDir.AppendChar(SoundWavePath[i]);
				}
			}
		}

		ReportAr->Log(TEXT("Sound Wave Memory Report"));
		ReportAr->Log(TEXT(""));

		if (SoundWaveObjects.Num())
		{
			// Alpha sort the sound wave objects
			SoundWaveObjects.Sort(FCompareSoundWave());

			// Log the sound wave objects
			
			ReportAr->Logf(TEXT("Memory (MB),Count"));
			ReportAr->Logf(TEXT("Total,%.3f,%d"), TotalResourceSize.GetTotalMemoryBytes() / 1024.f / 1024.f, SoundWaveObjects.Num());
			ReportAr->Logf(TEXT("Decompressed,%.3f,%d"), DecompressedResourceSize.GetTotalMemoryBytes() / 1024.f / 1024.f, CompressedResourceCount);
			ReportAr->Logf(TEXT("Compressed,%.3f,%d"), CompressedResourceSize.GetTotalMemoryBytes() / 1024.f / 1024.f, SoundWaveObjects.Num() - CompressedResourceCount);

			if (SoundWaveGroupFolders.Num())
			{
				ReportAr->Log(TEXT(""));
				ReportAr->Log(TEXT("Memory Usage and Count for Specified Folders (Folders defined in [AudioMemReportFolders] section in DefaultEngine.ini file):"));
				ReportAr->Log(TEXT(""));
				ReportAr->Logf(TEXT("%s,%s,%s"), TEXT("Directory"), TEXT("Total (MB)"), TEXT("Compressed (MB)"));
				for (const FString& SoundWaveGroupFolder : SoundWaveGroupFolders)
				{
					FSoundWaveGroupInfo* SubDirSize = SoundWaveGroupSizes.Find(SoundWaveGroupFolder);
					check(SubDirSize);
					ReportAr->Logf(TEXT("%s,%10.2f,%10.2f"), *SoundWaveGroupFolder, SubDirSize->ResourceSize.GetTotalMemoryBytes() / 1024.0f / 1024.0f, SubDirSize->CompressedResourceSize.GetTotalMemoryBytes() / 1024.0f / 1024.0f);
				}
			}

			ReportAr->Log(TEXT(""));
			ReportAr->Log(TEXT("All Sound Wave Objects Sorted Alphebetically:"));
			ReportAr->Log(TEXT(""));

			ReportAr->Logf(TEXT("%s,%s,%s,%s,%s,%s"), TEXT("SoundWave"), TEXT("KB"), TEXT("MB"), TEXT("SoundGroup"), TEXT("Duration"), TEXT("CompressionState"));
			for (const FSoundWaveInfo& Info : SoundWaveObjects)
			{
				float Kbytes = Info.ResourceSize.GetTotalMemoryBytes() / 1024.0f;
				ReportAr->Logf(TEXT("%s,%10.2f,%10.2f,%s,%10.2f,%s"), *Info.SoundWave->GetPathName(), Kbytes, Kbytes / 1024.0f, *Info.SoundGroupName, Info.Duration, Info.bDecompressed ? TEXT("Decompressed") : TEXT("Compressed"));
			}
		}

	}

	// Shutdown and free archive resources
	FileArWrapper->TearDown();
	delete FileArWrapper;
	delete FileAr;

	return true;
}

#endif // !UE_BUILD_SHIPPING

bool FAudioDevice::IsHRTFEnabledForAll() const
{
	if (IsInAudioThread())
	{
		return bHRTFEnabledForAll && IsSpatializationPluginEnabled();
	}

	check(IsInGameThread());
	return bHRTFEnabledForAll_OnGameThread && IsSpatializationPluginEnabled();
}

void FAudioDevice::SetMixDebugState(EDebugState InDebugState)
{
	if (!IsInAudioThread())
	{
		DECLARE_CYCLE_STAT(TEXT("FAudioThreadTask.SetMixDebugState"), STAT_AudioSetMixDebugState, STATGROUP_AudioThreadCommands);

		FAudioDevice* AudioDevice = this;
		FAudioThread::RunCommandOnAudioThread([AudioDevice, InDebugState]()
		{
			AudioDevice->SetMixDebugState(InDebugState);

		}, GET_STATID(STAT_AudioSetMixDebugState));

		return;
	}

	DebugState = InDebugState;
}

bool FAudioDevice::Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
{
#if !UE_BUILD_SHIPPING
	if (FParse::Command(&Cmd, TEXT("DumpSoundInfo")))
	{
		HandleDumpSoundInfoCommand(Cmd, Ar);
	}
	if (FParse::Command(&Cmd, TEXT("ListSounds")))
	{
		return HandleListSoundsCommand(Cmd, Ar);
	}
	else if (FParse::Command(&Cmd, TEXT("ListWaves")))
	{
		return HandleListWavesCommand(Cmd, Ar);
	}
	else if (FParse::Command(&Cmd, TEXT("ListSoundClasses")))
	{
		return HandleListSoundClassesCommand(Cmd, Ar);
	}
	else if (FParse::Command(&Cmd, TEXT("ShowSoundClassHierarchy")))
	{
		return HandleShowSoundClassHierarchyCommand(Cmd, Ar);
	}
	else if (FParse::Command(&Cmd, TEXT("ListSoundClassVolumes")))
	{
		return HandleListSoundClassVolumesCommand(Cmd, Ar);
	}
	else if (FParse::Command(&Cmd, TEXT("ListAudioComponents")))
	{
		return HandleListAudioComponentsCommand(Cmd, Ar);
	}
	else if (FParse::Command(&Cmd, TEXT("ListSoundDurations")))
	{
		return HandleListSoundDurationsCommand(Cmd, Ar);
	}
	else if (FParse::Command(&Cmd, TEXT("PlaySoundCue")))
	{
		return HandlePlaySoundCueCommand(Cmd, Ar);
	}
	else if (FParse::Command(&Cmd, TEXT("PlaySoundWave")))
	{
		return HandlePlaySoundWaveCommand(Cmd, Ar);
	}
	else if (FParse::Command(&Cmd, TEXT("SetBaseSoundMix")))
	{
		return HandleSetBaseSoundMixCommand(Cmd, Ar);
	}
	else if (FParse::Command(&Cmd, TEXT("IsolateDryAudio")))
	{
		return HandleIsolateDryAudioCommand(Cmd, Ar);
	}
	else if (FParse::Command(&Cmd, TEXT("IsolateReverb")))
	{
		return HandleIsolateReverbCommand(Cmd, Ar);
	}
	else if (FParse::Command(&Cmd, TEXT("TestLPF")))
	{
		return HandleTestLPFCommand(Cmd, Ar); 
	}
	else if (FParse::Command(&Cmd, TEXT("TestStereoBleed")))
	{
		return HandleTestStereoBleedCommand(Cmd, Ar);
	}
	else if (FParse::Command(&Cmd, TEXT("TestLFEBleed")))
	{
		return HandleTestLPFCommand(Cmd, Ar);
	}
	else if (FParse::Command(&Cmd, TEXT("DisableLPF")))
	{
		return HandleDisableLPFCommand(Cmd, Ar);
	}
	else if (FParse::Command(&Cmd, TEXT("DisableRadio")))
	{
		return HandleDisableRadioCommand(Cmd, Ar);
	}
	else if (FParse::Command(&Cmd, TEXT("EnableRadio")))
	{
		return HandleEnableRadioCommand(Cmd, Ar);
	}
	else if (FParse::Command(&Cmd, TEXT("ResetSoundState")))
	{
		return HandleResetSoundStateCommand(Cmd, Ar);
	}
	else if (FParse::Command(&Cmd, TEXT("ToggleSpatExt")))
	{
		return HandleToggleSpatializationExtensionCommand(Cmd, Ar);
	}
	else if (FParse::Command(&Cmd, TEXT("ToggleHRTFForAll")))
	{
		return HandleEnableHRTFForAllCommand(Cmd, Ar);
	}
	else if (FParse::Command(&Cmd, TEXT("SoloAudio")))
	{
		return HandleSoloCommand(Cmd, Ar);
	}
	else if (FParse::Command(&Cmd, TEXT("ClearSoloAudio")))
	{
		return HandleClearSoloCommand(Cmd, Ar);
	}
	else if (FParse::Command(&Cmd, TEXT("PlayAllPIEAudio")))
	{
		return HandlePlayAllPIEAudioCommand(Cmd, Ar);
	}
	else if (FParse::Command(&Cmd, TEXT("Audio3dVisualize")))
	{
		return HandleAudio3dVisualizeCommand(Cmd, Ar);
	}
	else if (FParse::Command(&Cmd, TEXT("AudioSoloSoundClass")))
	{
		return HandleAudioSoloSoundClass(Cmd, Ar);
	}
	else if (FParse::Command(&Cmd, TEXT("AudioSoloSoundWave")))
	{
		return HandleAudioSoloSoundWave(Cmd, Ar);
	}
	else if (FParse::Command(&Cmd, TEXT("AudioSoloSoundCue")))
	{
		return HandleAudioSoloSoundCue(Cmd, Ar);
	}
	else if (FParse::Command(&Cmd, TEXT("AudioMemReport")))
	{
		return HandleAudioMemoryInfo(Cmd, Ar);
	}
	else if (FParse::Command(&Cmd, TEXT("AudioMixerDebugSound")))
	{
		return HandleAudioMixerDebugSound(Cmd, Ar);
	}
	else if (FParse::Command(&Cmd, TEXT("SoundClassFixup")))
	{
		return HandleSoundClassFixup(Cmd, Ar);
	}
#endif // !UE_BUILD_SHIPPING

	return false;
}

void FAudioDevice::InitSoundClasses()
{
	// Reset the maps of sound class properties
	for (TObjectIterator<USoundClass> It; It; ++It)
	{
		USoundClass* SoundClass = *It;
		FSoundClassProperties& Properties = SoundClasses.Add(SoundClass, SoundClass->Properties);
	}

	// Propagate the properties down the hierarchy 
	ParseSoundClasses();
}

void FAudioDevice::InitSoundSources()
{
	if (Sources.Num() == 0)
	{
		// now create platform specific sources
		const int32 Channels = GetMaxChannels();
		for (int32 SourceIndex = 0; SourceIndex < Channels; SourceIndex++)
		{
			FSoundSource* Source = CreateSoundSource();
			Source->InitializeSourceEffects(SourceIndex);

			Sources.Add(Source);
			FreeSources.Add(Source);
		}
	}
}

void FAudioDevice::SetDefaultBaseSoundMix(USoundMix* SoundMix)
{
	if (IsInGameThread() && SoundMix == nullptr)
	{
		const FSoftObjectPath DefaultBaseSoundMixName = GetDefault<UAudioSettings>()->DefaultBaseSoundMix;
		if (DefaultBaseSoundMixName.IsValid())
		{			
			SoundMix = LoadObject<USoundMix>(nullptr, *DefaultBaseSoundMixName.ToString());
		}
	}

	if (!IsInAudioThread())
	{
		DECLARE_CYCLE_STAT(TEXT("FAudioThreadTask.SetDefaultBaseSoundMix"), STAT_AudioSetDefaultBaseSoundMix, STATGROUP_AudioThreadCommands);

		FAudioDevice* AudioDevice = this;
		FAudioThread::RunCommandOnAudioThread([AudioDevice, SoundMix]()
		{
			AudioDevice->SetDefaultBaseSoundMix(SoundMix);

		}, GET_STATID(STAT_AudioSetDefaultBaseSoundMix));

		return;
	}

	DefaultBaseSoundMix = SoundMix;
	SetBaseSoundMix(SoundMix);
}

void FAudioDevice::RemoveSoundMix(USoundMix* SoundMix)
{
	check(IsInAudioThread());

	if (SoundMix)
	{
		// Not sure if we will ever destroy the default base SoundMix
		if (SoundMix == DefaultBaseSoundMix)
		{
			DefaultBaseSoundMix = nullptr;
		}

		ClearSoundMix(SoundMix);

		// Try setting to global default if base SoundMix has been cleared
		if (BaseSoundMix == nullptr)
		{
			SetBaseSoundMix(DefaultBaseSoundMix);
		}
	}
}


void FAudioDevice::RecurseIntoSoundClasses(USoundClass* CurrentClass, FSoundClassProperties& ParentProperties)
{
	// Iterate over all child nodes and recurse.
	for (USoundClass* ChildClass : CurrentClass->ChildClasses)
	{
		// Look up class and propagated properties.
		FSoundClassProperties* Properties = SoundClasses.Find(ChildClass);

		// Should never be NULL for a properly set up tree.
		if (ChildClass)
		{
			if (Properties)
			{
				Properties->Volume *= ParentProperties.Volume;
				Properties->Pitch *= ParentProperties.Pitch;
				Properties->bIsUISound |= ParentProperties.bIsUISound;
				Properties->bIsMusic |= ParentProperties.bIsMusic;

				// Not all values propagate equally...
				// VoiceCenterChannelVolume, RadioFilterVolume, RadioFilterVolumeThreshold, bApplyEffects, BleedStereo, bReverb, and bCenterChannelOnly do not propagate (sub-classes can be non-zero even if parent class is zero)

				// ... and recurse into child nodes.
				RecurseIntoSoundClasses(ChildClass, *Properties);
			}
			else
			{
				UE_LOG(LogAudio, Warning, TEXT("Couldn't find child class properties - sound class functionality will not work correctly! CurrentClass: %s ChildClass: %s"), *CurrentClass->GetFullName(), *ChildClass->GetFullName());
			}
		}
	}
}

void FAudioDevice::UpdateHighestPriorityReverb()
{
	check(IsInGameThread());

	DECLARE_CYCLE_STAT(TEXT("FAudioThreadTask.UpdateHighestPriorityReverb"), STAT_AudioUpdateHighestPriorityReverb, STATGROUP_AudioThreadCommands);

	FAudioDevice* AudioDevice = this;

	if (ActivatedReverbs.Num() > 0)
	{
		ActivatedReverbs.ValueSort([](const FActivatedReverb& A, const FActivatedReverb& B) { return A.Priority > B.Priority; });

		const FActivatedReverb& NewActiveReverbRef = ActivatedReverbs.CreateConstIterator().Value();
		FAudioThread::RunCommandOnAudioThread([AudioDevice, NewActiveReverbRef]()
		{
			AudioDevice->bHasActivatedReverb = true;
			AudioDevice->HighestPriorityActivatedReverb = NewActiveReverbRef;
		}, GET_STATID(STAT_AudioUpdateHighestPriorityReverb));
	}
	else
	{
		FAudioThread::RunCommandOnAudioThread([AudioDevice]()
		{
			AudioDevice->bHasActivatedReverb = false;
		}, GET_STATID(STAT_AudioUpdateHighestPriorityReverb));
	}
}

void FAudioDevice::ParseSoundClasses()
{
	TArray<USoundClass*> RootSoundClasses;

	// Reset to known state - preadjusted by set class volume calls
	for (TMap<USoundClass*, FSoundClassProperties>::TIterator It(SoundClasses); It; ++It)
	{
		USoundClass* SoundClass = It.Key();
		if (SoundClass)
		{
			It.Value() = SoundClass->Properties;
			if (SoundClass->ParentClass == NULL)
			{
				RootSoundClasses.Add(SoundClass);
			}
		}
	}

	for (int32 RootIndex = 0; RootIndex < RootSoundClasses.Num(); ++RootIndex)
	{
		USoundClass* RootSoundClass = RootSoundClasses[RootIndex];

		FSoundClassProperties* RootSoundClassProperties = SoundClasses.Find(RootSoundClass);
		if (RootSoundClass && RootSoundClassProperties)
		{
			// Follow the tree.
			RecurseIntoSoundClasses(RootSoundClass, *RootSoundClassProperties);
		}
	}
}


void FAudioDevice::RecursiveApplyAdjuster(const FSoundClassAdjuster& InAdjuster, USoundClass* InSoundClass)
{
	// Find the sound class properties so we can apply the adjuster
	// and find the sound class so we can recurse through the children
	FSoundClassProperties* Properties = SoundClasses.Find(InSoundClass);
	if (InSoundClass && Properties)
	{
		// Adjust this class
		Properties->Volume *= InAdjuster.VolumeAdjuster;
		Properties->Pitch *= InAdjuster.PitchAdjuster;
		Properties->VoiceCenterChannelVolume *= InAdjuster.VoiceCenterChannelVolumeAdjuster;

		// Recurse through this classes children
		for (int32 ChildIdx = 0; ChildIdx < InSoundClass->ChildClasses.Num(); ++ChildIdx)
		{
			if (InSoundClass->ChildClasses[ ChildIdx ])
			{
				RecursiveApplyAdjuster(InAdjuster, InSoundClass->ChildClasses[ ChildIdx ]);
			}
		}
	}
	else
	{
		UE_LOG(LogAudio, Warning, TEXT("Sound class '%s' does not exist"), InSoundClass ? *InSoundClass->GetName() : TEXT("<null>"));
	}
}

bool FAudioDevice::ApplySoundMix(USoundMix* NewMix, FSoundMixState* SoundMixState)
{
	if (NewMix && SoundMixState)
	{
		UE_LOG(LogAudio, Log, TEXT("FAudioDevice::ApplySoundMix(): %s"), *NewMix->GetName());

		SoundMixState->StartTime = GetAudioClock();
		SoundMixState->FadeInStartTime = SoundMixState->StartTime + NewMix->InitialDelay;
		SoundMixState->FadeInEndTime = SoundMixState->FadeInStartTime + NewMix->FadeInTime;
		SoundMixState->FadeOutStartTime = -1.0;
		SoundMixState->EndTime = -1.0;
		if (NewMix->Duration >= 0.0f)
		{
			SoundMixState->FadeOutStartTime = SoundMixState->FadeInEndTime + NewMix->Duration;
			SoundMixState->EndTime = SoundMixState->FadeOutStartTime + NewMix->FadeOutTime;
		}
		SoundMixState->InterpValue = 0.0f;

		// On sound mix application, there is no delta time
		const float InitDeltaTime = 0.0f;

		ApplyClassAdjusters(NewMix, SoundMixState->InterpValue, InitDeltaTime);

		return(true);
	}

	return(false);
}

void FAudioDevice::UpdateSoundMix(USoundMix* SoundMix, FSoundMixState* SoundMixState)
{
	// If this SoundMix will automatically end, add some more time
	if (SoundMixState->FadeOutStartTime >= 0.f)
	{
		SoundMixState->StartTime = GetAudioClock();

		// Don't need to reset the fade-in times since we don't want to retrigger fade-ins
		// But we need to update the fade out start and end times
		if (SoundMixState->CurrentState != ESoundMixState::Inactive)
		{
			SoundMixState->FadeOutStartTime = -1.0;
			SoundMixState->EndTime = -1.0;

			if (SoundMix->Duration >= 0.0f)
			{
				if (SoundMixState->CurrentState == ESoundMixState::FadingIn || SoundMixState->CurrentState == ESoundMixState::Active)
				{
					SoundMixState->FadeOutStartTime = SoundMixState->StartTime + SoundMix->FadeInTime + SoundMix->Duration;
					SoundMixState->EndTime = SoundMixState->FadeOutStartTime + SoundMix->FadeOutTime;

				}
				else if (SoundMixState->CurrentState == ESoundMixState::FadingOut)
				{
					// Flip the state to fade in
					SoundMixState->CurrentState = ESoundMixState::FadingIn;

					SoundMixState->InterpValue = 0.0f;

					SoundMixState->FadeInStartTime = GetAudioClock() - SoundMixState->InterpValue * SoundMix->FadeInTime;
					SoundMixState->StartTime = SoundMixState->FadeInStartTime;

					SoundMixState->FadeOutStartTime = GetAudioClock() + SoundMix->FadeInTime + SoundMix->Duration;
					SoundMixState->EndTime = SoundMixState->FadeOutStartTime + SoundMix->FadeOutTime;
				}
			}
		}
	}
}

void FAudioDevice::UpdatePassiveSoundMixModifiers(TArray<FWaveInstance*>& WaveInstances, int32 FirstActiveIndex)
{
	TArray<USoundMix*> CurrPassiveSoundMixModifiers;

	// Find all passive SoundMixes from currently active wave instances
	for (int32 WaveIndex = FirstActiveIndex; WaveIndex < WaveInstances.Num(); WaveIndex++)
	{
		FWaveInstance* WaveInstance = WaveInstances[WaveIndex];
		if (WaveInstance)
		{
			USoundClass* SoundClass = WaveInstance->SoundClass;
			if (SoundClass) 
			{
				const float WaveInstanceActualVolume = WaveInstance->GetVolumeWithDistanceAttenuation();
				// Check each SoundMix individually for volume levels
				for (const FPassiveSoundMixModifier& PassiveSoundMixModifier : SoundClass->PassiveSoundMixModifiers)
				{
					if (WaveInstanceActualVolume >= PassiveSoundMixModifier.MinVolumeThreshold && WaveInstanceActualVolume <= PassiveSoundMixModifier.MaxVolumeThreshold)
					{
						// If the active sound is brand new, add to the new list... 
 						if (WaveInstance->ActiveSound->PlaybackTime == 0.0f && PassiveSoundMixModifier.SoundMix)
 						{
							PushSoundMixModifier(PassiveSoundMixModifier.SoundMix, true, true);
 						}

						// Only add a unique sound mix modifier
						CurrPassiveSoundMixModifiers.AddUnique(PassiveSoundMixModifier.SoundMix);
					}
				}
			}
		}
	}

	// Push SoundMixes that weren't previously active
	for (USoundMix* CurrPassiveSoundMixModifier : CurrPassiveSoundMixModifiers)
	{
		if (PrevPassiveSoundMixModifiers.Find(CurrPassiveSoundMixModifier) == INDEX_NONE)
		{
			PushSoundMixModifier(CurrPassiveSoundMixModifier, true);
		}
	}

	// Pop SoundMixes that are no longer active
	for (int32 MixIdx = PrevPassiveSoundMixModifiers.Num() - 1; MixIdx >= 0; MixIdx--)
	{
		USoundMix* PrevPassiveSoundMixModifier = PrevPassiveSoundMixModifiers[MixIdx];
		if (CurrPassiveSoundMixModifiers.Find(PrevPassiveSoundMixModifier) == INDEX_NONE)
		{
			PopSoundMixModifier(PrevPassiveSoundMixModifier, true);
		}
	}

	PrevPassiveSoundMixModifiers = CurrPassiveSoundMixModifiers;
}

bool FAudioDevice::TryClearingSoundMix(USoundMix* SoundMix, FSoundMixState* SoundMixState)
{
	if (SoundMix && SoundMixState)
	{
		// Only manually clear the sound mix if it's no longer referenced and if the duration was not set.
		// If the duration was set by sound designer, let the sound mix clear itself up automatically.
		if (SoundMix->Duration < 0.0f && SoundMixState->ActiveRefCount == 0 && SoundMixState->PassiveRefCount == 0 && SoundMixState->IsBaseSoundMix == false)
		{
			// do whatever is needed to remove influence of this SoundMix
			if (SoundMix->FadeOutTime > 0.f)
			{
				if (SoundMixState->CurrentState == ESoundMixState::Inactive)
				{
					// Haven't even started fading up, can kill immediately
					ClearSoundMix(SoundMix);
				}
				else if (SoundMixState->CurrentState == ESoundMixState::FadingIn)
				{
					// Currently fading up, force fade in to complete and start fade out from current fade level
					SoundMixState->FadeOutStartTime = GetAudioClock() - ((1.0f - SoundMixState->InterpValue) * SoundMix->FadeOutTime);
					SoundMixState->EndTime = SoundMixState->FadeOutStartTime + SoundMix->FadeOutTime;
					SoundMixState->StartTime = SoundMixState->FadeInStartTime = SoundMixState->FadeInEndTime = SoundMixState->FadeOutStartTime - 1.0;

					TryClearingEQSoundMix(SoundMix);
				}
				else if (SoundMixState->CurrentState == ESoundMixState::Active)
				{
					// SoundMix active, start fade out early
					SoundMixState->FadeOutStartTime = GetAudioClock();
					SoundMixState->EndTime = SoundMixState->FadeOutStartTime + SoundMix->FadeOutTime;

					TryClearingEQSoundMix(SoundMix);
				}
				else
				{
					// Already fading out, do nothing
				}
			}
			else
			{
				ClearSoundMix(SoundMix);
			}
			return true;
		}
	}

	return false;
}

bool FAudioDevice::TryClearingEQSoundMix(USoundMix* SoundMix)
{
	if (SoundMix && Effects && Effects->GetCurrentEQMix() == SoundMix)
	{
		USoundMix* NextEQMix = FindNextHighestEQPrioritySoundMix(SoundMix);
		if (NextEQMix)
		{
			// Need to ignore priority when setting as it will be less than current
			Effects->SetMixSettings(NextEQMix, true);
		}
		else
		{
			Effects->ClearMixSettings();
		}

		return true;
	}

	return false;
}

USoundMix* FAudioDevice::FindNextHighestEQPrioritySoundMix(USoundMix* IgnoredSoundMix)
{
	// find the mix with the next highest priority that was added first
	USoundMix* NextEQMix = NULL;
	FSoundMixState* NextState = NULL;

	for (TMap< USoundMix*, FSoundMixState >::TIterator It(SoundMixModifiers); It; ++It)
	{
		if (It.Key() != IgnoredSoundMix && It.Value().CurrentState < ESoundMixState::FadingOut
			&& (NextEQMix == NULL
				|| (It.Key()->EQPriority > NextEQMix->EQPriority
					|| (It.Key()->EQPriority == NextEQMix->EQPriority
						&& It.Value().StartTime < NextState->StartTime)
					)
				)
			)
		{
			NextEQMix = It.Key();
			NextState = &(It.Value());
		}
	}

	return NextEQMix;
}

void FAudioDevice::ClearSoundMix(USoundMix* SoundMix)
{
	if (SoundMix == nullptr)
	{
		return;
	}

	if (SoundMix == BaseSoundMix)
	{
		BaseSoundMix = nullptr;
	}
	SoundMixModifiers.Remove(SoundMix);
	PrevPassiveSoundMixModifiers.Remove(SoundMix);

	// Check if there are any overrides for this sound mix and if so, reset them so that next time this sound mix is applied, it'll get the new override values
	FSoundMixClassOverrideMap* SoundMixOverrideMap = SoundMixClassEffectOverrides.Find(SoundMix);
	if (SoundMixOverrideMap)
	{
		for (TPair<USoundClass*, FSoundMixClassOverride>& Entry : *SoundMixOverrideMap)
		{
			Entry.Value.bOverrideApplied = false;
		}
	}

	TryClearingEQSoundMix(SoundMix);
}

/** Static helper function which handles setting and updating the sound class adjuster override */
static void UpdateClassAdjustorOverrideEntry(FSoundClassAdjuster& ClassAdjustor, FSoundMixClassOverride& ClassAdjusterOverride, float DeltaTime)
{
	// If we've already applied the override in a previous frame
	if (ClassAdjusterOverride.bOverrideApplied)
	{
		// If we've received a new override value since our last update, then just set the dynamic parameters to the new value
		// The dynamic parameter objects will automatically smoothly travel to the new target value from its current value in the given time
		if (ClassAdjusterOverride.bOverrideChanged)
		{
			ClassAdjusterOverride.PitchOverride.Set(ClassAdjusterOverride.SoundClassAdjustor.PitchAdjuster, ClassAdjusterOverride.FadeInTime);
			ClassAdjusterOverride.VolumeOverride.Set(ClassAdjusterOverride.SoundClassAdjustor.VolumeAdjuster, ClassAdjusterOverride.FadeInTime);
		}
		else
		{
			// We haven't changed so just update the override this frame
			ClassAdjusterOverride.PitchOverride.Update(DeltaTime);
			ClassAdjusterOverride.VolumeOverride.Update(DeltaTime);
		}
	}
	else
	{
		// We haven't yet applied the override to the mix, so set the override dynamic parameters to immediately
		// have the current class adjuster values (0.0 interp-time), then set the dynamic parameters to the new target values in the given fade time

		ClassAdjusterOverride.VolumeOverride.Set(ClassAdjustor.VolumeAdjuster, 0.0f);
		ClassAdjusterOverride.VolumeOverride.Set(ClassAdjusterOverride.SoundClassAdjustor.VolumeAdjuster, ClassAdjusterOverride.FadeInTime);

		ClassAdjusterOverride.PitchOverride.Set(ClassAdjustor.PitchAdjuster, 0.0f);
		ClassAdjusterOverride.PitchOverride.Set(ClassAdjusterOverride.SoundClassAdjustor.PitchAdjuster, ClassAdjusterOverride.FadeInTime);
	}

	if (!ClassAdjustor.SoundClassObject)
	{
		ClassAdjustor.SoundClassObject = ClassAdjusterOverride.SoundClassAdjustor.SoundClassObject;
	}

	check(ClassAdjustor.SoundClassObject == ClassAdjusterOverride.SoundClassAdjustor.SoundClassObject);

	// Get the current value of the dynamic parameters
	ClassAdjustor.PitchAdjuster = ClassAdjusterOverride.PitchOverride.GetValue();
	ClassAdjustor.VolumeAdjuster = ClassAdjusterOverride.VolumeOverride.GetValue();

	// Override the apply to children if applicable
	ClassAdjustor.bApplyToChildren = ClassAdjusterOverride.SoundClassAdjustor.bApplyToChildren;

	// Reset the flags on the override adjuster
	ClassAdjusterOverride.bOverrideApplied = true;
	ClassAdjusterOverride.bOverrideChanged = false;

	// Check if we're clearing and check the terminating condition
	if (ClassAdjusterOverride.bIsClearing)
	{
		// If our override dynamic parameter is done, then we've finished clearing
		if (ClassAdjusterOverride.VolumeOverride.IsDone())
		{
			ClassAdjusterOverride.bIsCleared = true;
		}
	}
}

void FAudioDevice::ApplyClassAdjusters(USoundMix* SoundMix, float InterpValue, float DeltaTime)
{
	if (!SoundMix)
	{
		return;
	}

	InterpValue = FMath::Clamp(InterpValue, 0.0f, 1.0f);

	// Check if there is a sound mix override entry
	FSoundMixClassOverrideMap* SoundMixOverrideMap = SoundMixClassEffectOverrides.Find(SoundMix);

	// Create a ptr to the array of sound class adjusters ers we want to actually use. Default to using the sound class effects adjuster list
	TArray<FSoundClassAdjuster>* SoundClassAdjusters = &SoundMix->SoundClassEffects;

	bool bUsingOverride = false;

	// If we have an override for this sound mix, replace any overrides and/or add to the array if the sound class adjustment entry doesn't exist
	if (SoundMixOverrideMap)
	{
		// If we have an override map, create a copy of the sound class adjusters for the sound mix, then override the sound mix class overrides
		SoundClassAdjustersCopy = SoundMix->SoundClassEffects;

		// Use the copied list 
		SoundClassAdjusters = &SoundClassAdjustersCopy;

		bUsingOverride = true;

		// Get the interpolated values of the vanilla adjusters up-front
		for (FSoundClassAdjuster& Entry : *SoundClassAdjusters)
		{
			if (Entry.SoundClassObject)
			{
				Entry.VolumeAdjuster = InterpolateAdjuster(Entry.VolumeAdjuster, InterpValue);
				Entry.PitchAdjuster = InterpolateAdjuster(Entry.PitchAdjuster, InterpValue);
				Entry.VoiceCenterChannelVolumeAdjuster = InterpolateAdjuster(Entry.VoiceCenterChannelVolumeAdjuster, InterpValue);
			}
		}

		TArray<USoundClass*> SoundClassesToRemove;
		for (TPair<USoundClass*, FSoundMixClassOverride>& SoundMixOverrideEntry : *SoundMixOverrideMap)
		{
			// Get the sound class object of the override
			FSoundMixClassOverride& ClassAdjusterOverride = SoundMixOverrideEntry.Value;
			USoundClass* SoundClassObject = ClassAdjusterOverride.SoundClassAdjustor.SoundClassObject;

			// If the override has successfully cleared, then just remove it and continue iterating
			if (ClassAdjusterOverride.bIsCleared)
			{
				SoundClassesToRemove.Add(SoundClassObject);
				continue;
			}

			// Look for it in the adjusters copy 
			bool bSoundClassAdjustorExisted = false;
			for (FSoundClassAdjuster& Entry : *SoundClassAdjusters)
			{
				// If we found it, then we need to override the volume and pitch values of the adjuster entry
				if (Entry.SoundClassObject == SoundClassObject)
				{
					// Flag that we don't need to add it to the SoundClassAdjustorsCopy
					bSoundClassAdjustorExisted = true;

					UpdateClassAdjustorOverrideEntry(Entry, ClassAdjusterOverride, DeltaTime);
					break;
				}
			}

			// If we didn't find an existing sound class we need to add the override to the adjuster copy
			if (!bSoundClassAdjustorExisted)
			{
				// Create a default sound class adjuster (1.0 values for pitch and volume)
				FSoundClassAdjuster NewEntry;

				// Apply and/or update the override
				UpdateClassAdjustorOverrideEntry(NewEntry, ClassAdjusterOverride, DeltaTime);

				// Add the new sound class adjuster entry to the array
				SoundClassAdjusters->Add(NewEntry);
			}
		}

		for (USoundClass* SoundClassToRemove : SoundClassesToRemove)
		{
			SoundMixOverrideMap->Remove(SoundClassToRemove);

			// If there are no more overrides, remove the sound mix override entry
			if (SoundMixOverrideMap->Num() == 0)
			{
				SoundMixClassEffectOverrides.Remove(SoundMix);
			}
		}
	}

	// Loop through the sound class adjusters, everything should be up-to-date
	for (FSoundClassAdjuster& Entry : *SoundClassAdjusters)
	{
		if (Entry.SoundClassObject)
		{
			if (Entry.bApplyToChildren)
			{
				// If we're using the override, Entry will already have interpolated values
				if (bUsingOverride)
				{
					RecursiveApplyAdjuster(Entry, Entry.SoundClassObject);
				}
				else
				{
					// Copy the entry with the interpolated values before applying it recursively
					FSoundClassAdjuster EntryCopy = Entry;
					EntryCopy.VolumeAdjuster = InterpolateAdjuster(Entry.VolumeAdjuster, InterpValue);
					EntryCopy.PitchAdjuster = InterpolateAdjuster(Entry.PitchAdjuster, InterpValue);
					EntryCopy.VoiceCenterChannelVolumeAdjuster = InterpolateAdjuster(Entry.VoiceCenterChannelVolumeAdjuster, InterpValue);

					RecursiveApplyAdjuster(EntryCopy, Entry.SoundClassObject);
				}
			}
			else
			{
				// Apply the adjuster to only the sound class specified by the adjuster
				FSoundClassProperties* Properties = SoundClasses.Find(Entry.SoundClassObject);
				if (Properties)
				{
					// If we are using an override, we've already interpolated all our dynamic parameters
					if (bUsingOverride)
					{
						Properties->Volume *= Entry.VolumeAdjuster;
						Properties->Pitch *= Entry.PitchAdjuster;
						Properties->VoiceCenterChannelVolume *= Entry.VoiceCenterChannelVolumeAdjuster;
					}
					// Otherwise, we need to use the "static" data and compute the adjustment interpolations now
					else
					{
						Properties->Volume *= InterpolateAdjuster(Entry.VolumeAdjuster, InterpValue);
						Properties->Pitch *= InterpolateAdjuster(Entry.PitchAdjuster, InterpValue);
						Properties->VoiceCenterChannelVolume *= InterpolateAdjuster(Entry.VoiceCenterChannelVolumeAdjuster, InterpValue);
					}
				}
				else
				{
					UE_LOG(LogAudio, Warning, TEXT("Sound class '%s' does not exist"), *Entry.SoundClassObject->GetName());
				}
			}
		}
	}
}

void FAudioDevice::UpdateSoundClassProperties(float DeltaTime)
{
	// Remove SoundMix modifications and propagate the properties down the hierarchy
	ParseSoundClasses();

	for (TMap< USoundMix*, FSoundMixState >::TIterator It(SoundMixModifiers); It; ++It)
	{
		FSoundMixState* SoundMixState = &(It.Value());

		// Initial delay before mix is applied
		const double AudioTime = GetAudioClock();

		if (AudioTime >= SoundMixState->StartTime && AudioTime < SoundMixState->FadeInStartTime)
		{
			SoundMixState->InterpValue = 0.0f;
			SoundMixState->CurrentState = ESoundMixState::Inactive;
		}
		else if (AudioTime >= SoundMixState->FadeInStartTime && AudioTime < SoundMixState->FadeInEndTime)
		{
			// Work out the fade in portion
			SoundMixState->InterpValue = (float)((AudioTime - SoundMixState->FadeInStartTime) / (SoundMixState->FadeInEndTime - SoundMixState->FadeInStartTime));
			SoundMixState->CurrentState = ESoundMixState::FadingIn;
		}
		else if (AudioTime >= SoundMixState->FadeInEndTime
			&& (SoundMixState->IsBaseSoundMix
				|| ((SoundMixState->PassiveRefCount > 0 || SoundMixState->ActiveRefCount > 0) && SoundMixState->FadeOutStartTime < 0.f)
				|| AudioTime < SoundMixState->FadeOutStartTime)) 
		{
			// .. ensure the full mix is applied between the end of the fade in time and the start of the fade out time
			// or if SoundMix is the base or active via a passive push - ignores duration.
			SoundMixState->InterpValue = 1.0f;
			SoundMixState->CurrentState = ESoundMixState::Active;
		}
		else if (AudioTime >= SoundMixState->FadeOutStartTime && AudioTime < SoundMixState->EndTime)
		{
			// Work out the fade out portion
			SoundMixState->InterpValue = 1.0f - (float)((AudioTime - SoundMixState->FadeOutStartTime) / (SoundMixState->EndTime - SoundMixState->FadeOutStartTime));
			if (SoundMixState->CurrentState != ESoundMixState::FadingOut)
			{
				// Start fading EQ at same time
				SoundMixState->CurrentState = ESoundMixState::FadingOut;
				TryClearingEQSoundMix(It.Key());
			}
		}
		else 
		{
			check(SoundMixState->EndTime >= 0.f && AudioTime >= SoundMixState->EndTime);
			// Clear the effect of this SoundMix - may need to revisit for passive
			SoundMixState->InterpValue = 0.0f;
			SoundMixState->CurrentState = ESoundMixState::AwaitingRemoval;
		}

		ApplyClassAdjusters(It.Key(), SoundMixState->InterpValue, DeltaTime);

		if (SoundMixState->CurrentState == ESoundMixState::AwaitingRemoval)
		{
			ClearSoundMix(It.Key());
		}
	}
}

float FListener::Interpolate(const double EndTime)
{
	if (FApp::GetCurrentTime() < InteriorStartTime)
	{
		return 0.0f;
	}

	if (FApp::GetCurrentTime() >= EndTime)
	{
		return 1.0f;
	}

	float InterpValue = (float)((FApp::GetCurrentTime() - InteriorStartTime) / (EndTime - InteriorStartTime));
	return FMath::Clamp(InterpValue, 0.0f, 1.0f);
}

void FListener::UpdateCurrentInteriorSettings()
{
	// Store the interpolation value, not the actual value
	InteriorVolumeInterp = Interpolate(InteriorEndTime);
	ExteriorVolumeInterp = Interpolate(ExteriorEndTime);
	InteriorLPFInterp = Interpolate(InteriorLPFEndTime);
	ExteriorLPFInterp = Interpolate(ExteriorLPFEndTime);
}

void FAudioDevice::InvalidateCachedInteriorVolumes() const
{
	check(IsInAudioThread());

	for (FActiveSound* ActiveSound : ActiveSounds)
	{
		ActiveSound->bGotInteriorSettings = false;
	}
}

void FListener::ApplyInteriorSettings(const uint32 InAudioVolumeID, const FInteriorSettings& Settings)
{
	if (InAudioVolumeID != AudioVolumeID || Settings != InteriorSettings)
	{
		// Use previous/ current interpolation time if we're transitioning to the default worldsettings zone.
		InteriorStartTime = FApp::GetCurrentTime();
		InteriorEndTime = InteriorStartTime + (Settings.bIsWorldSettings ? InteriorSettings.InteriorTime : Settings.InteriorTime);
		ExteriorEndTime = InteriorStartTime + (Settings.bIsWorldSettings ? InteriorSettings.ExteriorTime : Settings.ExteriorTime);
		InteriorLPFEndTime = InteriorStartTime + (Settings.bIsWorldSettings ? InteriorSettings.InteriorLPFTime : Settings.InteriorLPFTime);
		ExteriorLPFEndTime = InteriorStartTime + (Settings.bIsWorldSettings ? InteriorSettings.ExteriorLPFTime : Settings.ExteriorLPFTime);

		AudioVolumeID = InAudioVolumeID;
		InteriorSettings = Settings;
	}
}


void FAudioDevice::SetListener(UWorld* World, const int32 InViewportIndex, const FTransform& ListenerTransform, const float InDeltaSeconds)
{
	check(IsInGameThread());

	// The copy is done because FTransform doesn't work to pass by value on Win32
	FTransform ListenerTransformCopy = ListenerTransform;

	if (!ensureMsgf(ListenerTransformCopy.IsValid(), TEXT("Invalid listener transform provided to AudioDevice")))
	{
		// If we have a bad transform give it something functional if totally wrong
		ListenerTransformCopy = FTransform::Identity;
	}

	if (InViewportIndex >= ListenerTransforms.Num())
	{
		ListenerTransforms.AddDefaulted(InViewportIndex - ListenerTransforms.Num() + 1);
	}

	ListenerTransforms[InViewportIndex] = ListenerTransformCopy;

	DECLARE_CYCLE_STAT(TEXT("FAudioThreadTask.SetListener"), STAT_AudioSetListener, STATGROUP_AudioThreadCommands);

	uint32 WorldID = INDEX_NONE;

	if (World != nullptr)
	{
		WorldID = World->GetUniqueID();
	}

	// Initialize the plugin listeners if we haven't already. This needs to be done here since this is when we're 
	// guaranteed to have a world ptr and we've already initialized the audio device. 
	if (World && !bPluginListenersInitialized)
	{
		InitializePluginListeners(World);
		bPluginListenersInitialized = true;
	}

	FAudioDevice* AudioDevice = this;
	FAudioThread::RunCommandOnAudioThread([AudioDevice, WorldID, InViewportIndex, ListenerTransformCopy, InDeltaSeconds]()
	{
		// Broadcast to a 3rd party plugin listener observer if enabled
		for (TAudioPluginListenerPtr PluginManager : AudioDevice->PluginListeners)
		{
			PluginManager->OnListenerUpdated(AudioDevice, InViewportIndex, ListenerTransformCopy, InDeltaSeconds);
		}

		TArray<FListener>& AudioThreadListeners = AudioDevice->Listeners;
		if (InViewportIndex >= AudioThreadListeners.Num())
		{
			const int32 NumListeners = InViewportIndex - AudioThreadListeners.Num() + 1;
			for (int32 i = 0; i < NumListeners; ++i)
			{
				AudioThreadListeners.Add(FListener(AudioDevice));
			}
		}

		FListener& Listener = AudioThreadListeners[InViewportIndex];
		Listener.Velocity = InDeltaSeconds > 0.f ? 
			(ListenerTransformCopy.GetTranslation() - Listener.Transform.GetTranslation()) / InDeltaSeconds
			: FVector::ZeroVector;

#if ENABLE_NAN_DIAGNOSTIC
		if (Listener.Velocity.ContainsNaN())
		{
			logOrEnsureNanError(TEXT("FAudioDevice::SetListener has detected a NaN in Listener Velocity"));
		}
#endif

		Listener.WorldID = WorldID;
		Listener.Transform = ListenerTransformCopy;

	}, GET_STATID(STAT_AudioSetListener));
}

void FAudioDevice::SetDefaultAudioSettings(UWorld* World, const FReverbSettings& DefaultReverbSettings, const FInteriorSettings& DefaultInteriorSettings)
{
	check(IsInGameThread());

	DECLARE_CYCLE_STAT(TEXT("FAudioThreadTask.SetDefaultAudioSettings"), STAT_AudioSetDefaultAudioSettings, STATGROUP_AudioThreadCommands);

	FAudioDevice* AudioDevice = this;
	const uint32 WorldID = World->GetUniqueID();
	FAudioThread::RunCommandOnAudioThread([AudioDevice, WorldID, DefaultReverbSettings, DefaultInteriorSettings]()
	{
		AudioDevice->WorldIDToDefaultAudioVolumeSettingsMap.Add(WorldID, TPair<FReverbSettings,FInteriorSettings>(DefaultReverbSettings,DefaultInteriorSettings));

	}, GET_STATID(STAT_AudioSetDefaultAudioSettings));
}

void FAudioDevice::GetAudioVolumeSettings(const uint32 WorldID, const FVector& Location, FAudioVolumeSettings& OutSettings) const
{
	check(IsInAudioThread());

	for (const TPair<uint32,FAudioVolumeProxy>& AudioVolumePair : AudioVolumeProxies)
	{
		const FAudioVolumeProxy& Proxy = AudioVolumePair.Value;
		if (Proxy.WorldID == WorldID)
		{
			FVector Dummy;
			float DistanceSqr = 0.f;
			if (Proxy.BodyInstance->GetSquaredDistanceToBody(Location, DistanceSqr, Dummy) && DistanceSqr == 0.f)
			{
				OutSettings.AudioVolumeID = Proxy.AudioVolumeID;
				OutSettings.Priority = Proxy.Priority;
				OutSettings.ReverbSettings = Proxy.ReverbSettings;
				OutSettings.InteriorSettings = Proxy.InteriorSettings;
				return;
			}
		}
	}

	OutSettings.AudioVolumeID = 0;

	const TPair<FReverbSettings, FInteriorSettings>* DefaultAudioVolumeSettings = WorldIDToDefaultAudioVolumeSettingsMap.Find(WorldID);

	if (DefaultAudioVolumeSettings)
	{
		OutSettings.ReverbSettings = DefaultAudioVolumeSettings->Key;
		OutSettings.InteriorSettings = DefaultAudioVolumeSettings->Value;
	}
}

void FAudioDevice::SetBaseSoundMix(USoundMix* NewMix)
{
	if (!IsInAudioThread())
	{
		DECLARE_CYCLE_STAT(TEXT("FAudioThreadTask.SetBaseSoundMix"), STAT_AudioSetBaseSoundMix, STATGROUP_AudioThreadCommands);

		FAudioDevice* AudioDevice = this;
		FAudioThread::RunCommandOnAudioThread([AudioDevice, NewMix]()
		{
			AudioDevice->SetBaseSoundMix(NewMix);

		}, GET_STATID(STAT_AudioSetBaseSoundMix));

		return;
	}

	if (NewMix && NewMix != BaseSoundMix)
	{
		USoundMix* OldBaseSoundMix = BaseSoundMix;
		BaseSoundMix = NewMix;

		if (OldBaseSoundMix)
		{
			FSoundMixState* OldBaseState = SoundMixModifiers.Find(OldBaseSoundMix);
			check(OldBaseState);
			OldBaseState->IsBaseSoundMix = false;
			TryClearingSoundMix(OldBaseSoundMix, OldBaseState);
		}

		// Check whether this SoundMix is already active
		FSoundMixState* ExistingState = SoundMixModifiers.Find(NewMix);
		if (!ExistingState)
		{
			// First time this mix has been set - add it and setup mix modifications
			ExistingState = &SoundMixModifiers.Add(NewMix, FSoundMixState());

			// Setup SoundClass modifications
			ApplySoundMix(NewMix, ExistingState);

			// Use it to set EQ Settings, which will check its priority
			if (Effects)
			{
				Effects->SetMixSettings(NewMix);
			}
		}

		ExistingState->IsBaseSoundMix = true;
	}
}

void FAudioDevice::PushSoundMixModifier(USoundMix* SoundMix, bool bIsPassive, bool bIsRetrigger)
{
	if (SoundMix)
	{
		if (!IsInAudioThread())
		{
			DECLARE_CYCLE_STAT(TEXT("FAudioThreadTask.PushSoundMixModifier"), STAT_AudioPushSoundMixModifier, STATGROUP_AudioThreadCommands);

			FAudioDevice* AudioDevice = this;
			FAudioThread::RunCommandOnAudioThread([AudioDevice, SoundMix, bIsPassive]()
			{
				AudioDevice->PushSoundMixModifier(SoundMix, bIsPassive);

			}, GET_STATID(STAT_AudioPushSoundMixModifier));

			return;
		}

		FSoundMixState* SoundMixState = SoundMixModifiers.Find(SoundMix);

		if (!SoundMixState)
		{
			// First time this mix has been pushed - add it and setup mix modifications
			SoundMixState = &SoundMixModifiers.Add(SoundMix, FSoundMixState());

			// Setup SoundClass modifications
			ApplySoundMix(SoundMix, SoundMixState);

			// Use it to set EQ Settings, which will check its priority
			if (Effects)
			{
				Effects->SetMixSettings(SoundMix);
			}
		}
		else
		{
			UpdateSoundMix(SoundMix, SoundMixState);
		}

		// Increase the relevant ref count - we know pointer exists by this point
		if (!bIsRetrigger)
		{
			if (bIsPassive)
			{
				SoundMixState->PassiveRefCount++;
			}
			else
			{
				SoundMixState->ActiveRefCount++;
			}
		}
	}
}

void FAudioDevice::SetSoundMixClassOverride(USoundMix* InSoundMix, USoundClass* InSoundClass, float Volume, float Pitch, float FadeInTime, bool bApplyToChildren)
{
	if (!InSoundMix || !InSoundClass)
	{
		return;
	}

	if (!IsInAudioThread())
	{
		DECLARE_CYCLE_STAT(TEXT("FAudioThreadTask.SetSoundMixClassOverride"), STAT_AudioSetSoundMixClassOverride, STATGROUP_AudioThreadCommands);

		FAudioDevice* AudioDevice = this;
		FAudioThread::RunCommandOnAudioThread([AudioDevice, InSoundMix, InSoundClass, Volume, Pitch, FadeInTime, bApplyToChildren]()
		{
			AudioDevice->SetSoundMixClassOverride(InSoundMix, InSoundClass, Volume, Pitch, FadeInTime, bApplyToChildren);

		}, GET_STATID(STAT_AudioSetSoundMixClassOverride));

		return;
	}

	FSoundMixClassOverrideMap& SoundMixClassOverrideMap = SoundMixClassEffectOverrides.FindOrAdd(InSoundMix);

	// Check if we've already added this sound class override
	FSoundMixClassOverride* ClassOverride = SoundMixClassOverrideMap.Find(InSoundClass);
	if (ClassOverride)
	{
		// Override the values of the sound class override with the new values
		ClassOverride->SoundClassAdjustor.SoundClassObject = InSoundClass;
		ClassOverride->SoundClassAdjustor.VolumeAdjuster = Volume;
		ClassOverride->SoundClassAdjustor.PitchAdjuster = Pitch;
		ClassOverride->SoundClassAdjustor.bApplyToChildren = bApplyToChildren;

		// Flag that we've changed so that the update will interpolate to new values
		ClassOverride->bOverrideChanged = true;
		ClassOverride->bIsClearing = false;
		ClassOverride->FadeInTime = FadeInTime;
	}
	else
	{
		// Create a new override struct
		FSoundMixClassOverride NewClassOverride;
		NewClassOverride.SoundClassAdjustor.SoundClassObject = InSoundClass;
		NewClassOverride.SoundClassAdjustor.VolumeAdjuster = Volume;
		NewClassOverride.SoundClassAdjustor.PitchAdjuster = Pitch;
		NewClassOverride.SoundClassAdjustor.bApplyToChildren = bApplyToChildren;
		NewClassOverride.FadeInTime = FadeInTime;

		SoundMixClassOverrideMap.Add(InSoundClass, NewClassOverride);
	}
}

void FAudioDevice::ClearSoundMixClassOverride(USoundMix* InSoundMix, USoundClass* InSoundClass, float FadeOutTime)
{
	if (!InSoundMix || !InSoundClass)
	{
		return;
	}

	if (!IsInAudioThread())
	{
		DECLARE_CYCLE_STAT(TEXT("FAudioThreadTask.ClearSoundMixClassOverride"), STAT_AudioClearSoundMixClassOverride, STATGROUP_AudioThreadCommands);

		FAudioDevice* AudioDevice = this;
		FAudioThread::RunCommandOnAudioThread([AudioDevice, InSoundMix, InSoundClass, FadeOutTime]()
		{
			AudioDevice->ClearSoundMixClassOverride(InSoundMix, InSoundClass, FadeOutTime);

		}, GET_STATID(STAT_AudioClearSoundMixClassOverride));

		return;
	}

	// Get the sound mix class override map for the sound mix. If this doesn't exist, then nobody overrode the sound mix
	FSoundMixClassOverrideMap* SoundMixClassOverrideMap = SoundMixClassEffectOverrides.Find(InSoundMix);
	if (!SoundMixClassOverrideMap)
	{
		return;
	}

	// Get the sound class override. If this doesn't exist, then the sound class wasn't previously overridden.
	FSoundMixClassOverride* SoundClassOverride = SoundMixClassOverrideMap->Find(InSoundClass);
	if (!SoundClassOverride)
	{
		return;
	}

	// If the override is currently applied, then we need to "fade out" the override
	if (SoundClassOverride->bOverrideApplied)
	{
		// Get the new target values that sound mix would be if it weren't overridden. 
		// If this was a pure add to the sound mix, then the target values will be 1.0f (i.e. not applied)
		float VolumeAdjuster = 1.0f;
		float PitchAdjuster = 1.0f;

		// Loop through the sound mix class adjusters and set the volume adjuster to the value that would be in the sound mix
		for (const FSoundClassAdjuster& Adjustor : InSoundMix->SoundClassEffects)
		{
			if (Adjustor.SoundClassObject == InSoundClass)
			{
				VolumeAdjuster = Adjustor.VolumeAdjuster;
				PitchAdjuster = Adjustor.PitchAdjuster;
				break;
			}
		}

		SoundClassOverride->bIsClearing = true;
		SoundClassOverride->bIsCleared = false;
		SoundClassOverride->bOverrideChanged = true;
		SoundClassOverride->FadeInTime = FadeOutTime;
		SoundClassOverride->SoundClassAdjustor.VolumeAdjuster = VolumeAdjuster;
		SoundClassOverride->SoundClassAdjustor.PitchAdjuster = PitchAdjuster;
	}
	else
	{
		// Otherwise, we just simply remove the sound class override in the sound class override map
		SoundMixClassOverrideMap->Remove(InSoundClass);

		// If there are no more overrides, remove the sound mix override entry
		if (!SoundMixClassOverrideMap->Num())
		{
			SoundMixClassEffectOverrides.Remove(InSoundMix);
		}
	}

}

void FAudioDevice::PopSoundMixModifier(USoundMix* SoundMix, bool bIsPassive)
{
	if (SoundMix)
	{
		if (!IsInAudioThread())
		{
			DECLARE_CYCLE_STAT(TEXT("FAudioThreadTask.PopSoundMixModifier"), STAT_AudioPopSoundMixModifier, STATGROUP_AudioThreadCommands);

			FAudioDevice* AudioDevice = this;
			FAudioThread::RunCommandOnAudioThread([AudioDevice, SoundMix, bIsPassive]()
			{
				AudioDevice->PopSoundMixModifier(SoundMix, bIsPassive);

			}, GET_STATID(STAT_AudioPopSoundMixModifier));

			return;
		}

		FSoundMixState* SoundMixState = SoundMixModifiers.Find(SoundMix);

		if (SoundMixState)
		{
			if (bIsPassive && SoundMixState->PassiveRefCount > 0)
			{
				SoundMixState->PassiveRefCount--;
			}
			else if (!bIsPassive && SoundMixState->ActiveRefCount > 0)
			{
				SoundMixState->ActiveRefCount--;
			}

			TryClearingSoundMix(SoundMix, SoundMixState);
		}
	}
}

void FAudioDevice::ClearSoundMixModifier(USoundMix* SoundMix)
{
	if (SoundMix)
	{
		if (!IsInAudioThread())
		{
			DECLARE_CYCLE_STAT(TEXT("FAudioThreadTask.ClearSoundMixModifier"), STAT_AudioClearSoundMixModifier, STATGROUP_AudioThreadCommands);

			FAudioDevice* AudioDevice = this;
			FAudioThread::RunCommandOnAudioThread([AudioDevice, SoundMix]()
			{
				AudioDevice->ClearSoundMixModifier(SoundMix);

			}, GET_STATID(STAT_AudioClearSoundMixModifier));

			return;
		}

		FSoundMixState* SoundMixState = SoundMixModifiers.Find(SoundMix);

		if (SoundMixState)
		{
			SoundMixState->ActiveRefCount = 0;

			TryClearingSoundMix(SoundMix, SoundMixState);
		}
	}
}

void FAudioDevice::ClearSoundMixModifiers()
{
	if (!IsInAudioThread())
	{
		DECLARE_CYCLE_STAT(TEXT("FAudioThreadTask.ClearSoundMixModifiers"), STAT_AudioClearSoundMixModifiers, STATGROUP_AudioThreadCommands);

		FAudioDevice* AudioDevice = this;
		FAudioThread::RunCommandOnAudioThread([AudioDevice]()
		{
			AudioDevice->ClearSoundMixModifiers();

		}, GET_STATID(STAT_AudioClearSoundMixModifiers));

		return;
	}

	// Clear all sound mix modifiers
	for (TMap< USoundMix*, FSoundMixState >::TIterator It(SoundMixModifiers); It; ++It)
	{
		ClearSoundMixModifier(It.Key());
	}
}

void FAudioDevice::ActivateReverbEffect(UReverbEffect* ReverbEffect, FName TagName, float Priority, float Volume, float FadeTime)
{
	check(IsInGameThread());

	FActivatedReverb& ActivatedReverb = ActivatedReverbs.FindOrAdd(TagName);

	ActivatedReverb.ReverbSettings.ReverbEffect = ReverbEffect;
	ActivatedReverb.ReverbSettings.Volume = Volume;
	ActivatedReverb.ReverbSettings.FadeTime = FadeTime;
	ActivatedReverb.Priority = Priority;

	UpdateHighestPriorityReverb();
}

void FAudioDevice::DeactivateReverbEffect(FName TagName)
{
	check(IsInGameThread());

	if (ActivatedReverbs.Remove(TagName) > 0)
	{
		UpdateHighestPriorityReverb();
	}
}

void* FAudioDevice::InitEffect(FSoundSource* Source)
{
	check(IsInAudioThread());
	if (Effects)
	{
		return Effects->InitEffect(Source);
	}
	return nullptr;
}


void* FAudioDevice::UpdateEffect(FSoundSource* Source)
{
	SCOPE_CYCLE_COUNTER(STAT_AudioUpdateEffects);

	check(IsInAudioThread());
	if (Effects)
	{
		return Effects->UpdateEffect(Source);
	}
	return nullptr;
}


void FAudioDevice::DestroyEffect(FSoundSource* Source)
{
	check(IsInAudioThread());
	if (Effects)
	{
		Effects->DestroyEffect(Source);
	}
}

void FAudioDevice::HandlePause(bool bGameTicking, bool bGlobalPause)
{
	DECLARE_CYCLE_STAT(TEXT("FAudioThreadTask.HandlePause"), STAT_AudioHandlePause, STATGROUP_AudioThreadCommands);

	// Run this command on the audio thread if this is getting called on game thread
	if (!IsInAudioThread())
	{
		FAudioDevice* AudioDevice = this;
		FAudioThread::RunCommandOnAudioThread([AudioDevice, bGameTicking, bGlobalPause]()
		{
			AudioDevice->HandlePause(bGameTicking, bGlobalPause);
		}, GET_STATID(STAT_AudioHandlePause));

		return;
	}

	// Handles the global pause/unpause feature

	// Pause all sounds if transitioning to pause mode.
	if (!bGameTicking && (bGameWasTicking || bGlobalPause))
	{
		for (int32 i = 0; i < Sources.Num(); i++)
		{
			FSoundSource* Source = Sources[ i ];
			if (!Source->IsPausedByGame() && (bGlobalPause || Source->IsGameOnly()))
			{
				Source->SetPauseByGame(true);
			}
		}
	}
	// Unpause all sounds if transitioning back to game.
	else if (bGameTicking && (!bGameWasTicking || bGlobalPause))
	{
		for (int32 i = 0; i < Sources.Num(); i++)
		{
			FSoundSource* Source = Sources[ i ];
			if (Source->IsPausedByGame() && (bGlobalPause || Source->IsGameOnly()))
			{
				Source->SetPauseByGame(false);
			}
		}
	}

	bGameWasTicking = bGameTicking;
}

int32 FAudioDevice::GetSortedActiveWaveInstances(TArray<FWaveInstance*>& WaveInstances, const ESortedActiveWaveGetType::Type GetType)
{
	check(IsInAudioThread());

	SCOPE_CYCLE_COUNTER(STAT_AudioGatherWaveInstances);

	// Tick all the active audio components.  Use a copy as some operations may remove elements from the list, but we want
	// to evaluate in the order they were added
	TArray<FActiveSound*> ActiveSoundsCopy = ActiveSounds;
	for (int32 i = 0; i < ActiveSoundsCopy.Num(); ++i)
	{
		FActiveSound* ActiveSound = ActiveSoundsCopy[i];

		if (!ActiveSound)
		{
			UE_LOG(LogAudio, Error, TEXT("Null sound at index %d in ActiveSounds Array!"), i);
			continue;
		}
		
		if (!ActiveSound->Sound)
		{
			// No sound - cleanup and remove
			AddSoundToStop(ActiveSound);
		}
		// If the world scene allows audio - tick wave instances.
		else 
		{
			UWorld* ActiveSoundWorldPtr = ActiveSound->World.Get();
			if (ActiveSoundWorldPtr == nullptr || ActiveSoundWorldPtr->AllowAudioPlayback())
			{
				bool bStopped = false;

				// Don't artificially stop a looping active sound nor stop a sound which has has bPlayEffectChainTails and actual effects playing
				if (!ActiveSound->Sound->IsLooping())
				{
					if (!ActiveSound->Sound->SourceEffectChain || !ActiveSound->Sound->SourceEffectChain->bPlayEffectChainTails || ActiveSound->Sound->SourceEffectChain->Chain.Num() == 0)
					{
						const float Duration = ActiveSound->Sound->GetDuration();

						// Divide by minimum pitch for longest possible duration
						if (ActiveSound->PlaybackTime > Duration / MIN_PITCH)
						{
							UE_LOG(LogAudio, Log, TEXT("Sound stopped due to duration: %g > %g : %s %s"),
								ActiveSound->PlaybackTime,
								Duration,
								*ActiveSound->Sound->GetName(),
								*ActiveSound->GetAudioComponentName());
							AddSoundToStop(ActiveSound);
							bStopped = true;
						}
					}
				}

				if (!bStopped)
				{
					// If not in game, do not advance sounds unless they are UI sounds.
					float UsedDeltaTime = GetGameDeltaTime();
					if (GetType == ESortedActiveWaveGetType::QueryOnly || (GetType == ESortedActiveWaveGetType::PausedUpdate && !ActiveSound->bIsUISound))
					{
						UsedDeltaTime = 0.0f;
					}

					ActiveSound->UpdateWaveInstances(WaveInstances, UsedDeltaTime);
				}
			}
		}
	}

	// Now stop any sounds that are active that are in concurrency resolution groups that resolve by stopping quietest
	{
		SCOPE_CYCLE_COUNTER(STAT_AudioEvaluateConcurrency);
		ConcurrencyManager.StopQuietSoundsDueToMaxConcurrency();
	}
	
	// Remove all wave instances from the wave instance list that are stopping due to max concurrency
	for (int32 i = WaveInstances.Num() - 1; i >= 0; --i)
	{
		if (WaveInstances[i]->ShouldStopDueToMaxConcurrency())
		{
			WaveInstances.RemoveAtSwap(i, 1, false);
		}
	}

	int32 FirstActiveIndex = 0;

	if (WaveInstances.Num() >= 0)
	{
		// Helper function for "Sort" (higher priority sorts last).
		struct FCompareFWaveInstanceByPlayPriority
		{
			FORCEINLINE bool operator()(const FWaveInstance& A, const FWaveInstance& B) const
			{
				return A.GetVolumeWeightedPriority() < B.GetVolumeWeightedPriority();
			}
		};

		// Sort by priority (lowest priority first).
		WaveInstances.Sort(FCompareFWaveInstanceByPlayPriority());

		// Get the first index that will result in a active source voice
		FirstActiveIndex = FMath::Max(WaveInstances.Num() - GetMaxChannels(), 0);
	}

	return(FirstActiveIndex);
}

void FAudioDevice::UpdateActiveSoundPlaybackTime(bool bIsGameTicking)
{
	if (bIsGameTicking)
	{
		for (FActiveSound* ActiveSound : ActiveSounds)
		{
			ActiveSound->PlaybackTime += GetDeviceDeltaTime();
		}
	}
	else if (GIsEditor)
	{
		for (FActiveSound* ActiveSound : ActiveSounds)
		{
			if (ActiveSound->bIsPreviewSound)
			{
				ActiveSound->PlaybackTime += GetDeviceDeltaTime();
			}
		}
	}

}

void FAudioDevice::StopSources(TArray<FWaveInstance*>& WaveInstances, int32 FirstActiveIndex)
{
	SCOPED_NAMED_EVENT(FAudioDevice_StopSources, FColor::Blue);

	// Touch sources that are high enough priority to play
	for (int32 InstanceIndex = FirstActiveIndex; InstanceIndex < WaveInstances.Num(); InstanceIndex++)
	{
		FWaveInstance* WaveInstance = WaveInstances[ InstanceIndex ];
		FSoundSource* Source = WaveInstanceSourceMap.FindRef(WaveInstance);
		if (Source)
		{
			Source->LastUpdate = CurrentTick;

			// If they are still audible, mark them as such
			float VolumeWeightedPriority = WaveInstance->GetVolumeWithDistanceAttenuation();
			if (VolumeWeightedPriority > 0.0f)
			{
				Source->LastHeardUpdate = CurrentTick;
			}
		}
	}

	// Stop inactive sources, sources that no longer have a WaveInstance associated
	// or sources that need to be reset because Stop & Play were called in the same frame.
	for (int32 SourceIndex = 0; SourceIndex < Sources.Num(); SourceIndex++)
	{
		FSoundSource* Source = Sources[ SourceIndex ];

		if (Source->WaveInstance)
		{
			// If we need to stop this sound due to max concurrency (i.e. it was quietest in a concurrency group)
			if (Source->WaveInstance->ShouldStopDueToMaxConcurrency())
			{
				Source->Stop();
			}
			// Source was not one of the active sounds this tick so needs to be stopped
			else if (Source->LastUpdate != CurrentTick)
			{
				Source->Stop();
			}
			else
			{
				// Update the pause state of the source.
				Source->SetPauseManually(Source->WaveInstance->bIsPaused);

				// Need to update the source still so that it gets any volume settings applied to
				// otherwise the source may play at a very quiet volume and not actually set to 0.0
				Source->Update();
			}
		}
	}

	// Stop wave instances that are no longer playing due to priority reasons. This needs to happen AFTER
	// stopping sources as calling Stop on a sound source in turn notifies the wave instance of a buffer
	// being finished which might reset it being finished.
	for (int32 InstanceIndex = 0; InstanceIndex < FirstActiveIndex; InstanceIndex++)
	{
		FWaveInstance* WaveInstance = WaveInstances[ InstanceIndex ];
		WaveInstance->StopWithoutNotification();
	}

#if STATS
	uint32 AudibleInactiveSounds = 0;
	// Count how many sounds are not being played but were audible
	for (int32 InstanceIndex = 0; InstanceIndex < FirstActiveIndex; InstanceIndex++)
	{
		FWaveInstance* WaveInstance = WaveInstances[ InstanceIndex ];
		if (WaveInstance->GetVolumeWithDistanceAttenuation() > 0.1f)
		{
			AudibleInactiveSounds++;
		}
	}
	SET_DWORD_STAT(STAT_AudibleWavesDroppedDueToPriority, AudibleInactiveSounds);
#endif
}

void FAudioDevice::StartSources(TArray<FWaveInstance*>& WaveInstances, int32 FirstActiveIndex, bool bGameTicking)
{
	check(IsInAudioThread());

	SCOPE_CYCLE_COUNTER(STAT_AudioStartSources);

	// Start sources as needed.
	for (int32 InstanceIndex = FirstActiveIndex; InstanceIndex < WaveInstances.Num(); InstanceIndex++)
	{
		FWaveInstance* WaveInstance = WaveInstances[InstanceIndex];
		
		// Make sure we've finished precaching the wave instance's wave data before trying to create a source for it
		if (!WaveInstance->WaveData->bIsPrecacheDone)
		{
			continue;
		}
		
		// Editor uses bIsUISound for sounds played in the browser.
		if (!WaveInstance->ShouldStopDueToMaxConcurrency() && (bGameTicking || WaveInstance->bIsUISound))
		{
			FSoundSource* Source = WaveInstanceSourceMap.FindRef(WaveInstance);
			if (!Source &&
				(!WaveInstance->IsStreaming() ||
				IStreamingManager::Get().GetAudioStreamingManager().CanCreateSoundSource(WaveInstance)))
			{
				check(FreeSources.Num());
				Source = FreeSources.Pop();
				check(Source);

				// Prepare for initialization... 
				bool bSuccess = false;
				if (Source->PrepareForInitialization(WaveInstance))
				{
					// We successfully prepared for initialization (though we may not be prepared to actually init yet)
					bSuccess = true;

					// If we are now prepared to init (because the file handle and header synchronously loaded), then init right away
					if (Source->IsPreparedToInit())
					{
						// Init the source, this may result in failure
						bSuccess = Source->Init(WaveInstance);

						// If we succeeded then play and update the source
						if (bSuccess)
						{
							// Set the pause before updating it
							Source->SetPauseManually(Source->WaveInstance->bIsPaused);

							check(Source->IsInitialized());
							Source->Update();

							// If the source didn't get paused while initializing, then play it
							if (!Source->IsPaused())
							{
								Source->Play();
							}
						}
					}
				}

				// If we succeeded above then we need to map the wave instance to the source
				if (bSuccess)
				{
					IStreamingManager::Get().GetAudioStreamingManager().AddStreamingSoundSource(Source);
					// Associate wave instance with it which is used earlier in this function.
					WaveInstanceSourceMap.Add(WaveInstance, Source);
				}
				else
				{
					// If we failed, then we need to stop the wave instance and add the source back to the free list
					// This can happen if e.g. the USoundWave pointed to by the WaveInstance is not a valid sound file.
					// If we don't stop the wave file, it will continue to try initializing the file every frame, which is a perf hit
					UE_LOG(LogAudio, Warning, TEXT("Failed to start sound source for %s"), (WaveInstance->ActiveSound && WaveInstance->ActiveSound->Sound) ? *WaveInstance->ActiveSound->Sound->GetName() : TEXT("UNKNOWN") );
					Source->Stop();
				}
			}
			else if (Source)
			{
				// If we've already been initialized, then just update the voice
				if (Source->IsInitialized())
				{
					Source->NotifyPlaybackPercent();
					Source->Update();
				}
				// Otherwise, we need still need to initialize
				else if (Source->IsPreparedToInit())
				{
					// Try to initialize the source. This may fail if something is wrong with the source.
					if (Source->Init(WaveInstance))
					{
						// Note: if we succeeded in starting to prepare to init, we already added the wave instance map to the source so don't need to add here.
						check(Source->IsInitialized());
						Source->Play();

						Source->Update();
					}
					else
					{
						// Make sure init cleaned up the buffer when it failed
						check(Source->Buffer == nullptr);

						// If were ready to call init but failed, then we need to add the source and stop with notification
						WaveInstance->StopWithoutNotification();
						FreeSources.Add(Source);
					}
				}
			}
			else
			{
				// This can happen if the streaming manager determines that this sound should not be started.
				// We stop the wave instance to prevent it from attempting to initialize every frame
				WaveInstance->StopWithoutNotification();
			}
		}
	}
}

void FAudioDevice::Update(bool bGameTicking)
{
	LLM_SCOPE(ELLMTag::Audio);

	SCOPED_NAMED_EVENT(FAudioDevice_Update, FColor::Blue);
	if (!IsInAudioThread())
	{

		FAudioDevice* AudioDevice = this;
		FAudioThread::RunCommandOnAudioThread([AudioDevice, bGameTicking]()
		{
			AudioDevice->Update(bGameTicking);
		});

		return;
	}

	DECLARE_CYCLE_STAT(TEXT("FAudioThreadTask.AudioUpdateTime"), STAT_AudioUpdateTime, STATGROUP_AudioThreadCommands);
	FScopeCycleCounter AudioUpdateTimeCounter(GET_STATID(STAT_AudioUpdateTime));

	// Updates the audio device delta time
	UpdateDeviceDeltaTime();

	// Update the audio clock, this can be overridden per platform to get a sample-accurate clock
	UpdateAudioClock();

	if (bGameTicking)
	{
		GlobalPitchScale.Update(GetDeviceDeltaTime());
	}

	// Start a new frame
	CurrentTick++;

	// Handle pause/unpause for the game and editor.
	HandlePause(bGameTicking);

	bool bHasVolumeSettings = false;
	float AudioVolumePriority = 0.f;
	FReverbSettings ReverbSettings;

	// Gets the current state of the interior settings
	for (FListener& Listener : Listeners)
	{
		FAudioVolumeSettings PlayerAudioVolumeSettings;
		GetAudioVolumeSettings(Listener.WorldID, Listener.Transform.GetLocation(), PlayerAudioVolumeSettings);

		Listener.ApplyInteriorSettings(PlayerAudioVolumeSettings.AudioVolumeID, PlayerAudioVolumeSettings.InteriorSettings);
		Listener.UpdateCurrentInteriorSettings();

		if (!bHasVolumeSettings || (PlayerAudioVolumeSettings.AudioVolumeID > 0 && PlayerAudioVolumeSettings.Priority > AudioVolumePriority))
		{
			bHasVolumeSettings = true;
			AudioVolumePriority = PlayerAudioVolumeSettings.Priority;
			ReverbSettings = PlayerAudioVolumeSettings.ReverbSettings;
		}
	}

	if (bHasActivatedReverb)
	{
		if (HighestPriorityActivatedReverb.Priority > AudioVolumePriority)
		{
			ReverbSettings = HighestPriorityActivatedReverb.ReverbSettings;
		}
	}

	if (Effects)
	{
		Effects->SetReverbSettings(ReverbSettings);

		// Update the audio effects - reverb, EQ etc
		Effects->Update();
	}

	// Gets the current state of the sound classes accounting for sound mix
	UpdateSoundClassProperties(GetDeviceDeltaTime());

	ProcessingPendingActiveSoundStops();

	// Update listener transform
	if (Listeners.Num() > 0)
	{
		// Caches the matrix used to transform a sounds position into local space so we can just look
		// at the Y component after normalization to determine spatialization.
		const FVector Up = Listeners[0].GetUp();
		const FVector Right = Listeners[0].GetFront();
		InverseListenerTransform = FMatrix(Up, Right, Up ^ Right, Listeners[0].Transform.GetTranslation()).Inverse();
		ensure(!InverseListenerTransform.ContainsNaN());
	}

	int32 FirstActiveIndex = INDEX_NONE;

	if (Sources.Num())
	{
		// Kill any sources that have finished
		for (int32 SourceIndex = 0; SourceIndex < Sources.Num(); SourceIndex++)
		{
			// Source has finished playing (it's one shot)
			if (Sources[ SourceIndex ]->IsFinished())
			{
				Sources[ SourceIndex ]->Stop();
			}
		}

		// Poll audio components for active wave instances (== paths in node tree that end in a USoundWave)
		ActiveWaveInstances.Reset();
		FirstActiveIndex = GetSortedActiveWaveInstances(ActiveWaveInstances, (bGameTicking ? ESortedActiveWaveGetType::FullUpdate : ESortedActiveWaveGetType::PausedUpdate));

		// Stop sources that need to be stopped, and touch the ones that need to be kept alive
		StopSources(ActiveWaveInstances, FirstActiveIndex);

		// Start and/or update any sources that have a high enough priority to play
		StartSources(ActiveWaveInstances, FirstActiveIndex, bGameTicking);

		// Check which sounds are active from these wave instances and update passive SoundMixes
		UpdatePassiveSoundMixModifiers(ActiveWaveInstances, FirstActiveIndex);

		// If not paused, update the playback time of the active sounds after we've processed passive mix modifiers 
		// Note that for sounds which play while paused, this will result in longer active sound playback times, which will be ok. If we update the 
		// active sound is updated while paused (for a long time), most sounds will be stopped when unpaused.
		UpdateActiveSoundPlaybackTime(bGameTicking);

		const int32 Channels = GetMaxChannels();
		INC_DWORD_STAT_BY(STAT_WaveInstances, ActiveWaveInstances.Num());
		INC_DWORD_STAT_BY(STAT_AudioSources, Channels - FreeSources.Num());
		INC_DWORD_STAT_BY(STAT_WavesDroppedDueToPriority, FMath::Max(ActiveWaveInstances.Num() - Channels, 0));
		INC_DWORD_STAT_BY(STAT_ActiveSounds, ActiveSounds.Num());
	}

	// now let the platform perform anything it needs to handle
	UpdateHardware();

	// send any needed information back to the game thread
	SendUpdateResultsToGameThread(FirstActiveIndex);

#if !UE_BUILD_SHIPPING
	// Print statistics for first non initial load allocation.
	static bool bFirstTime = true;
	if (bFirstTime && CommonAudioPoolSize != 0)
	{
		bFirstTime = false;
		if (CommonAudioPoolFreeBytes != 0)
		{
			UE_LOG(LogAudio, Log, TEXT("Audio pool size mismatch by %d bytes. Please update CommonAudioPoolSize ini setting to %d to avoid waste!"),
				CommonAudioPoolFreeBytes, CommonAudioPoolSize - CommonAudioPoolFreeBytes);
		}
	}
#endif
}

void FAudioDevice::SendUpdateResultsToGameThread(const int32 FirstActiveIndex)
{
#if !UE_BUILD_SHIPPING
	TArray<FAudioStats::FStatSoundInfo> StatSoundInfos;
	TArray<FAudioStats::FStatSoundMix> StatSoundMixes;
	const FVector ListenerPosition = Listeners[0].Transform.GetTranslation();
	const bool bStatsStale = (RequestedAudioStats == 0);
	if (RequestedAudioStats != 0)
	{
		TMap<FActiveSound*, int32> ActiveSoundToInfoIndex;

		const bool bDebug = (RequestedAudioStats & ERequestedAudioStats::DebugSounds) != 0;

		for (FActiveSound* ActiveSound : ActiveSounds)
		{
			if (ActiveSound->Sound)
			{
				if (!bDebug || ActiveSound->GetSound()->bDebug)
				{
					ActiveSoundToInfoIndex.Add(ActiveSound, StatSoundInfos.AddDefaulted());
					FAudioStats::FStatSoundInfo& StatSoundInfo = StatSoundInfos.Last();
					StatSoundInfo.SoundName = ActiveSound->GetSound()->GetPathName();
					StatSoundInfo.Distance = (ListenerPosition - ActiveSound->Transform.GetTranslation()).Size();

					if (USoundClass* SoundClass = ActiveSound->GetSoundClass())
					{
						StatSoundInfo.SoundClassName = SoundClass->GetFName();
					}
					else
					{
						StatSoundInfo.SoundClassName = NAME_None;
					}
					StatSoundInfo.Transform = ActiveSound->Transform;
					StatSoundInfo.AudioComponentID = ActiveSound->GetAudioComponentID();

					if (bDebug && ActiveSound->GetSound()->bDebug)
					{
						ActiveSound->CollectAttenuationShapesForVisualization(StatSoundInfo.ShapeDetailsMap);
					}
				}
			}
		}

		// Iterate through all wave instances.
		for (int32 InstanceIndex = FirstActiveIndex; InstanceIndex < ActiveWaveInstances.Num(); ++InstanceIndex)
		{
			FWaveInstance* WaveInstance = ActiveWaveInstances[InstanceIndex];
			int32* SoundInfoIndex = ActiveSoundToInfoIndex.Find(WaveInstance->ActiveSound);
			if (SoundInfoIndex)
			{
				FAudioStats::FStatWaveInstanceInfo WaveInstanceInfo;
				FSoundSource* Source = WaveInstanceSourceMap.FindRef(WaveInstance);
				WaveInstanceInfo.Description = Source ? Source->Describe((RequestedAudioStats & ERequestedAudioStats::LongSoundNames) != 0) : FString(TEXT("No source"));
				WaveInstanceInfo.ActualVolume = WaveInstance->GetVolumeWithDistanceAttenuation();
				WaveInstanceInfo.InstanceIndex = InstanceIndex;
				WaveInstanceInfo.WaveInstanceName = *WaveInstance->GetName();
				StatSoundInfos[*SoundInfoIndex].WaveInstanceInfos.Add(MoveTemp(WaveInstanceInfo));
			}
		}

		USoundMix* CurrentEQMix = Effects->GetCurrentEQMix();

		for (const TPair<USoundMix*, FSoundMixState>& SoundMixPair : SoundMixModifiers)
		{
			StatSoundMixes.AddDefaulted();
			FAudioStats::FStatSoundMix& StatSoundMix = StatSoundMixes.Last();
			StatSoundMix.MixName = SoundMixPair.Key->GetName();
			StatSoundMix.InterpValue = SoundMixPair.Value.InterpValue;
			StatSoundMix.RefCount = SoundMixPair.Value.ActiveRefCount + SoundMixPair.Value.PassiveRefCount;
			StatSoundMix.bIsCurrentEQ = (SoundMixPair.Key == CurrentEQMix);
		}
	}
#endif

	DECLARE_CYCLE_STAT(TEXT("FGameThreadAudioTask.AudioSendResults"), STAT_AudioSendResults, STATGROUP_TaskGraphTasks);

	const uint32 AudioDeviceID = DeviceHandle;
	UReverbEffect* ReverbEffect = nullptr;
	if (Effects)
	{
		ReverbEffect = Effects->GetCurrentReverbEffect();
	}

	FAudioThread::RunCommandOnGameThread([AudioDeviceID, ReverbEffect
#if !UE_BUILD_SHIPPING
											, ListenerPosition, StatSoundInfos, StatSoundMixes, bStatsStale
#endif
													]()
	{
		// At shutdown, GEngine may already be null
		if (GEngine)
		{
			if (FAudioDeviceManager* AudioDeviceManager = GEngine->GetAudioDeviceManager())
			{
				if (FAudioDevice* AudioDevice = AudioDeviceManager->GetAudioDevice(AudioDeviceID))
				{
					AudioDevice->CurrentReverbEffect = ReverbEffect;
#if !UE_BUILD_SHIPPING
					AudioDevice->AudioStats.ListenerLocation = ListenerPosition;
					AudioDevice->AudioStats.StatSoundInfos = StatSoundInfos;
					AudioDevice->AudioStats.StatSoundMixes = StatSoundMixes;
					AudioDevice->AudioStats.bStale = bStatsStale;
#endif
				}
			}
		}
	}, GET_STATID(STAT_AudioSendResults));
}

void FAudioDevice::StopAllSounds(bool bShouldStopUISounds)
{
	if (!IsInAudioThread())
	{
		DECLARE_CYCLE_STAT(TEXT("FAudioThreadTask.StopAllSounds"), STAT_AudioStopAllSounds, STATGROUP_AudioThreadCommands);

		FAudioDevice* AudioDevice = this;
		FAudioThread::RunCommandOnAudioThread([AudioDevice, bShouldStopUISounds]()
		{
			AudioDevice->StopAllSounds(bShouldStopUISounds);
		}, GET_STATID(STAT_AudioStopAllSounds));

		return;		
	}

	for (int32 SoundIndex=ActiveSounds.Num() - 1; SoundIndex >= 0; --SoundIndex)
	{
		FActiveSound* ActiveSound = ActiveSounds[SoundIndex];

		if (bShouldStopUISounds)
		{
			AddSoundToStop(ActiveSound);
		}
		// If we're allowing UI sounds to continue then first filter on the active sounds state
		else if (!ActiveSound->bIsUISound)
		{
			// Then iterate across the wave instances.  If any of the wave instances is not a UI sound
			// then we will stop the entire active sound because it makes less sense to leave it half
			// executing
			for (auto WaveInstanceIt(ActiveSound->WaveInstances.CreateConstIterator()); WaveInstanceIt; ++WaveInstanceIt)
			{
				FWaveInstance* WaveInstance = WaveInstanceIt.Value();
				if (WaveInstance && !WaveInstance->bIsUISound)
				{
					AddSoundToStop(ActiveSound);
					break;
				}
			}
		}
	}

	// Immediately process stopping sounds
	ProcessingPendingActiveSoundStops();
}

void FAudioDevice::InitializePluginListeners(UWorld* World)
{
	check(IsInGameThread());
	check(!bPluginListenersInitialized);

	for (TAudioPluginListenerPtr PluginListener : PluginListeners)
	{
		PluginListener->OnListenerInitialize(this, World);
	}
}

void FAudioDevice::AddNewActiveSound(const FActiveSound& NewActiveSound)
{
	if (NewActiveSound.Sound == nullptr)
	{
		return;
	}

	// Don't allow buses to try to play if we're not using the audio mixer.
	if (!IsAudioMixerEnabled())
	{
		if (NewActiveSound.Sound)
		{
			USoundSourceBus* Bus = Cast<USoundSourceBus>(NewActiveSound.Sound);
			if (Bus)
			{
				return;
			}
		}
	}

	if (!IsInAudioThread())
	{
		DECLARE_CYCLE_STAT(TEXT("FAudioThreadTask.AddNewActiveSound"), STAT_AudioAddNewActiveSound, STATGROUP_AudioThreadCommands);

		FAudioDevice* AudioDevice = this;
		FAudioThread::RunCommandOnAudioThread([AudioDevice, NewActiveSound]()
		{
			AudioDevice->AddNewActiveSound(NewActiveSound);
		}, GET_STATID(STAT_AudioAddNewActiveSound));

		return;		
	}

	// Evaluate concurrency. This will create an ActiveSound ptr which is a copy of NewActiveSound if the sound can play.
	FActiveSound* ActiveSound = nullptr;

	{
		SCOPE_CYCLE_COUNTER(STAT_AudioEvaluateConcurrency);

		// Try to create a new active sound. This returns nullptr if too many sounds are playing with this sound's concurrency setting
		ActiveSound = ConcurrencyManager.CreateNewActiveSound(NewActiveSound);
	}

	if (!ActiveSound)
	{
		return;
	}

	if (GIsEditor)
	{
		// If the sound played on an editor preview world, treat it as a preview sound (unpausable and ignoring the realtime volume slider)
		if (const UWorld* World = NewActiveSound.GetWorld())
		{
			ActiveSound->bIsPreviewSound |= (World->WorldType == EWorldType::EditorPreview);
		}
	}

	++NewActiveSound.Sound->CurrentPlayCount;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	UE_LOG(LogAudio, VeryVerbose, TEXT("New ActiveSound %s Comp: %s Loc: %s"), *NewActiveSound.Sound->GetName(), *NewActiveSound.GetAudioComponentName(), *NewActiveSound.Transform.GetTranslation().ToString());
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

	check(ActiveSound);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (ActiveSound->Sound)
	{
		if (!ensureMsgf(ActiveSound->Sound->GetFName() != NAME_None, TEXT("AddNewActiveSound with DESTROYED sound %s. AudioComponent=%s. IsPendingKill=%d. BeginDestroy=%d"),
			*ActiveSound->Sound->GetPathName(),
			*ActiveSound->GetAudioComponentName(),
			(int32)ActiveSound->Sound->IsPendingKill(),
			(int32)ActiveSound->Sound->HasAnyFlags(RF_BeginDestroyed)))
		{
			static FName InvalidSoundName(TEXT("DESTROYED_Sound"));
			ActiveSound->DebugOriginalSoundName = InvalidSoundName;
		}
		else
		{
			ActiveSound->DebugOriginalSoundName = ActiveSound->Sound->GetFName();
		}
	}
#endif

	ActiveSounds.Add(ActiveSound);
	if (ActiveSound->GetAudioComponentID() > 0)
	{
		AudioComponentIDToActiveSoundMap.Add(ActiveSound->GetAudioComponentID(), ActiveSound);
	}

}

void FAudioDevice::ProcessingPendingActiveSoundStops(bool bForceDelete)
{
	// Process the PendingSoundsToDelete. These may have 
	// had their deletion deferred due to an async operation
	for (int32 i = PendingSoundsToDelete.Num() - 1; i >= 0; --i)
	{
		FActiveSound* ActiveSound = PendingSoundsToDelete[i];
		if (bForceDelete || ActiveSound->CanDelete())
		{
			ActiveSound->bAsyncOcclusionPending = false;
			PendingSoundsToDelete.RemoveAtSwap(i, 1, false);
			delete ActiveSound;
		}
	}

	// Stop any pending active sounds that need to be stopped
	for (FActiveSound* ActiveSound : PendingSoundsToStop)
	{
		check(ActiveSound);
		ActiveSound->Stop();

		// If we can delete the active sound now, then delete it
		if (bForceDelete || ActiveSound->CanDelete())
		{
			ActiveSound->bAsyncOcclusionPending = false;
			delete ActiveSound;
		}
		else
		{
			// There was an async operation pending. We need to defer deleting this sound
			PendingSoundsToDelete.Add(ActiveSound);
		}
	}
	PendingSoundsToStop.Reset();
}


void FAudioDevice::AddSoundToStop(FActiveSound* SoundToStop)
{
	check(IsInAudioThread());

	const uint64 AudioComponentID = SoundToStop->GetAudioComponentID();
	if (AudioComponentID > 0)
	{
		AudioComponentIDToActiveSoundMap.Remove(AudioComponentID);
	}

	check(SoundToStop);
	PendingSoundsToStop.Add(SoundToStop);
}

void FAudioDevice::StopActiveSound(const uint64 AudioComponentID)
{
	if (!IsInAudioThread())
	{
		DECLARE_CYCLE_STAT(TEXT("FAudioThreadTask.StopActiveSound"), STAT_AudioStopActiveSound, STATGROUP_AudioThreadCommands);

		FAudioDevice* AudioDevice = this;
		FAudioThread::RunCommandOnAudioThread([AudioDevice, AudioComponentID]()
		{
			AudioDevice->StopActiveSound(AudioComponentID);
		}, GET_STATID(STAT_AudioStopActiveSound));

		return;
	}

	FActiveSound* ActiveSound = FindActiveSound(AudioComponentID);
	if (ActiveSound)
	{
		StopActiveSound(ActiveSound);
	}
}

void FAudioDevice::StopActiveSound(FActiveSound* ActiveSound)
{
	check(IsInAudioThread());
	AddSoundToStop(ActiveSound);
}

void FAudioDevice::PauseActiveSound(const uint64 AudioComponentID, const bool bInIsPaused)
{
	check(IsInAudioThread());
	FActiveSound* ActiveSound = FindActiveSound(AudioComponentID);
	if (ActiveSound)
	{
		ActiveSound->bIsPaused = bInIsPaused;
	}
}

FActiveSound* FAudioDevice::FindActiveSound(const uint64 AudioComponentID)
{
	check(IsInAudioThread());

	// find the active sound corresponding to this audio component
	return AudioComponentIDToActiveSoundMap.FindRef(AudioComponentID);
}

void FAudioDevice::RemoveActiveSound(FActiveSound* ActiveSound)
{
	check(IsInAudioThread());

	ConcurrencyManager.RemoveActiveSound(ActiveSound);

	// Perform the notification
	if (ActiveSound->GetAudioComponentID() > 0)
	{
		UAudioComponent::PlaybackCompleted(ActiveSound->GetAudioComponentID(), false);
	}

	const int32 NumRemoved = ActiveSounds.Remove(ActiveSound);
	check(NumRemoved == 1);
}

bool FAudioDevice::LocationIsAudible(const FVector& Location, const float MaxDistance) const
{
	if (MaxDistance >= WORLD_MAX)
	{
		return true;
	}

	const bool bInAudioThread = IsInAudioThread();
	const bool bInGameThread = IsInGameThread();

	check(bInAudioThread || bInGameThread);

	if (bInAudioThread)
	{
		for (const FListener& Listener : Listeners)
		{
			if (LocationIsAudible(Location, Listener.Transform, MaxDistance))
			{
				return true;
			}
		}
	}
	else //if (bInGameThread)
	{
		for (const FTransform& ListenerTransform : ListenerTransforms)
		{
			if (LocationIsAudible(Location, ListenerTransform, MaxDistance))
			{
				return true;
			}
		}
	}

	return false;
}

bool FAudioDevice::LocationIsAudible(const FVector& Location, const FTransform& ListenerTransform, float MaxDistance)
{
	if (MaxDistance >= WORLD_MAX)
	{
		return true;
	}

	const float MaxDistanceSquared = MaxDistance * MaxDistance;
	return (ListenerTransform.GetTranslation() - Location).SizeSquared() < MaxDistanceSquared;
}

void FAudioDevice::GetMaxDistanceAndFocusFactor(USoundBase* Sound, const UWorld* World, const FVector& Location, const FSoundAttenuationSettings* AttenuationSettingsToApply, float& OutMaxDistance, float& OutFocusFactor)
{
	check(IsInGameThread());
	check(Sound);

	const bool bHasAttenuationSettings = ShouldUseAttenuation(World) && AttenuationSettingsToApply;

	if (bHasAttenuationSettings)
	{
		FTransform SoundTransform;
		SoundTransform.SetTranslation(Location);

		OutMaxDistance = AttenuationSettingsToApply->GetMaxDimension();

		if (AttenuationSettingsToApply->bSpatialize && AttenuationSettingsToApply->bEnableListenerFocus)
		{
			// Now scale the max distance based on the focus settings in the attenuation settings
			FAttenuationListenerData ListenerData;
			float Azimuth = 0.0f;
			float AbsoluteAzimuth = 0.0f;
			const int32 ClosestListenerIndex = FindClosestListenerIndex(SoundTransform);
			const FTransform& ListenerTransform = ListenerTransforms[ClosestListenerIndex];	
			GetAzimuth(ListenerData, Sound, SoundTransform, *AttenuationSettingsToApply, ListenerTransform, Azimuth, AbsoluteAzimuth);

			OutFocusFactor = GetFocusFactor(ListenerData, Sound, Azimuth, *AttenuationSettingsToApply);
		}
		else
		{
			OutFocusFactor = 1.0f;
		}
	}
	else
	{
		// No need to scale the distance by focus factor since we're not using any attenuation settings
		OutMaxDistance = Sound->GetMaxAudibleDistance();
		OutFocusFactor = 1.0f;
	}
}

bool FAudioDevice::SoundIsAudible(USoundBase* Sound, const UWorld* World, const FVector& Location, const FSoundAttenuationSettings* AttenuationSettingsToApply, float MaxDistance, float FocusFactor)
{
	check(IsInGameThread());

	const bool bHasAttenuationSettings = ShouldUseAttenuation(World) && AttenuationSettingsToApply;
	float DistanceScale = 1.0f;
	if (bHasAttenuationSettings)
	{
		DistanceScale = AttenuationSettingsToApply->GetFocusDistanceScale(GetGlobalFocusSettings(), FocusFactor);
	}

	DistanceScale = FMath::Max(DistanceScale, 0.0001f);
	return LocationIsAudible(Location, MaxDistance / DistanceScale);
}

int32 FAudioDevice::FindClosestListenerIndex(const FTransform& SoundTransform, const TArray<FListener>& InListeners)
{
	int32 ClosestListenerIndex = 0;
	if (InListeners.Num() > 0)
	{
		float ClosestDistSq = FVector::DistSquared(SoundTransform.GetTranslation(), InListeners[0].Transform.GetTranslation());

		for (int32 i = 1; i < InListeners.Num(); i++)
		{
			const float DistSq = FVector::DistSquared(SoundTransform.GetTranslation(), InListeners[i].Transform.GetTranslation());
			if (DistSq < ClosestDistSq)
			{
				ClosestListenerIndex = i;
				ClosestDistSq = DistSq;
			}
		}
	}

	return ClosestListenerIndex;
}

int32 FAudioDevice::FindClosestListenerIndex(const FTransform& SoundTransform) const
{
	if (IsInAudioThread())
	{
		return FindClosestListenerIndex(SoundTransform, Listeners);
	}
	else if (IsInGameThread())
	{
		int32 ClosestListenerIndex = 0;
		if (ListenerTransforms.Num() > 0)
		{
			float ClosestDistSq = FVector::DistSquared(SoundTransform.GetTranslation(), ListenerTransforms[0].GetTranslation());

			for (int32 i = 1; i < ListenerTransforms.Num(); i++)
			{
				const float DistSq = FVector::DistSquared(SoundTransform.GetTranslation(), ListenerTransforms[i].GetTranslation());
				if (DistSq < ClosestDistSq)
				{
					ClosestListenerIndex = i;
					ClosestDistSq = DistSq;
				}
			}
		}

		return ClosestListenerIndex;
	}

	return INDEX_NONE;
}

void FAudioDevice::GetAttenuationListenerData(FAttenuationListenerData& OutListenerData, const FTransform& SoundTransform, const FSoundAttenuationSettings& AttenuationSettings, const FTransform* InListenerTransform) const
{
	// Only compute various components of the listener of it hasn't been computed yet
	if (!OutListenerData.bDataComputed)
	{

		// Use the optional input listener param
		if (InListenerTransform)
		{
			OutListenerData.ListenerTransform = *InListenerTransform;
		}
		// If not set, then we need to find the closest listener
		else
		{
			const int32 ClosestListenerIndex = FindClosestListenerIndex(SoundTransform);
			if (IsInAudioThread())
			{
				OutListenerData.ListenerTransform = Listeners[ClosestListenerIndex].Transform;
			}
			else if (IsInGameThread())
			{
				OutListenerData.ListenerTransform = ListenerTransforms[ClosestListenerIndex];
			}
		}

		const FVector ListenerLocation = OutListenerData.ListenerTransform.GetTranslation();
		FVector ListenerToSound = SoundTransform.GetTranslation() - ListenerLocation;
		ListenerToSound.ToDirectionAndLength(OutListenerData.ListenerToSoundDir, OutListenerData.ListenerToSoundDistance);

		OutListenerData.AttenuationDistance = 0.0f;

		if ((AttenuationSettings.bAttenuate && AttenuationSettings.AttenuationShape == EAttenuationShape::Sphere) || AttenuationSettings.bAttenuateWithLPF)
		{
			OutListenerData.AttenuationDistance = FMath::Max(OutListenerData.ListenerToSoundDistance - AttenuationSettings.AttenuationShapeExtents.X, 0.f);
		}

		OutListenerData.bDataComputed = true;
	}
}

void FAudioDevice::GetAzimuth(FAttenuationListenerData& OutListenerData, const USoundBase* Sound, const FTransform& SoundTransform, const FSoundAttenuationSettings& AttenuationSettings, const FTransform& ListenerTransform, float& OutAzimuth, float& OutAbsoluteAzimuth) const
{
	GetAttenuationListenerData(OutListenerData, SoundTransform, AttenuationSettings, &ListenerTransform);

	const FVector& ListenerForwardDir = OutListenerData.ListenerTransform.GetUnitAxis(EAxis::X);

	const float SoundToListenerForwardDotProduct = FVector::DotProduct(ListenerForwardDir, OutListenerData.ListenerToSoundDir);
	const float SoundListenerAngleRadians = FMath::Acos(SoundToListenerForwardDotProduct);

	// Normal azimuth only goes to 180 (0 is in front, 180 is behind).
	OutAzimuth = FMath::RadiansToDegrees(SoundListenerAngleRadians);

	const FVector& ListenerRightDir = OutListenerData.ListenerTransform.GetUnitAxis(EAxis::Y);
	const float SoundToListenerRightDotProduct = FVector::DotProduct(ListenerRightDir, OutListenerData.ListenerToSoundDir);

	FVector AbsAzimuthVector2D = FVector(SoundToListenerForwardDotProduct, SoundToListenerRightDotProduct, 0.0f);
	AbsAzimuthVector2D.Normalize();

	OutAbsoluteAzimuth = FMath::IsNearlyZero(AbsAzimuthVector2D.X) ? HALF_PI : FMath::Atan(AbsAzimuthVector2D.Y / AbsAzimuthVector2D.X);
	OutAbsoluteAzimuth = FMath::RadiansToDegrees(OutAbsoluteAzimuth);
	OutAbsoluteAzimuth = FMath::Abs(OutAbsoluteAzimuth);

	if (AbsAzimuthVector2D.X > 0.0f && AbsAzimuthVector2D.Y < 0.0f)
	{
		OutAbsoluteAzimuth = 360 - OutAbsoluteAzimuth;
	}
	else if (AbsAzimuthVector2D.X < 0.0f && AbsAzimuthVector2D.Y < 0.0f)
	{
		OutAbsoluteAzimuth += 180.0f;
	}
	else if (AbsAzimuthVector2D.X < 0.0f && AbsAzimuthVector2D.Y > 0.0f)
	{
		OutAbsoluteAzimuth = 180 - OutAbsoluteAzimuth;
	}
}

float FAudioDevice::GetFocusFactor(FAttenuationListenerData& OutListenerData, const USoundBase* Sound, const float Azimuth, const FSoundAttenuationSettings& AttenuationSettings) const
{
	check(Sound);

	// 0.0f means we are in focus, 1.0f means we are out of focus
	float FocusFactor = 0.0f;

	const float FocusAzimuth = FMath::Clamp(GlobalFocusSettings.FocusAzimuthScale * AttenuationSettings.FocusAzimuth, 0.0f, 180.0f);
	const float NonFocusAzimuth = FMath::Clamp(GlobalFocusSettings.NonFocusAzimuthScale * AttenuationSettings.NonFocusAzimuth, 0.0f, 180.0f);

	if (FocusAzimuth != NonFocusAzimuth)
	{
		FocusFactor = (Azimuth - FocusAzimuth) / (NonFocusAzimuth - FocusAzimuth);
		FocusFactor = FMath::Clamp(FocusFactor, 0.0f, 1.0f);
	}
	else if (Azimuth >= FocusAzimuth)
	{
		FocusFactor = 1.0f;
	}

	return FocusFactor;
}

FAudioDevice::FCreateComponentParams::FCreateComponentParams()
	: World(nullptr)
	, Actor(nullptr)
{
	AudioDevice = (GEngine ? GEngine->GetMainAudioDevice() : nullptr);
	CommonInit();
}

FAudioDevice::FCreateComponentParams::FCreateComponentParams(UWorld* InWorld, AActor* InActor)
	: World(InWorld)
{
	if (InActor)
	{
		check(InActor->GetWorld() == InWorld);
		Actor = InActor;
	}
	else
	{
		Actor = (World ? World->GetWorldSettings() : nullptr);
	}
	
	AudioDevice = (World ? World->GetAudioDevice() : nullptr);
	CommonInit();
}

FAudioDevice::FCreateComponentParams::FCreateComponentParams(AActor* InActor)
	: Actor(InActor)
{
	World = (Actor ? Actor->GetWorld() : nullptr);
	AudioDevice = (World ? World->GetAudioDevice() : nullptr);
	CommonInit();
}

FAudioDevice::FCreateComponentParams::FCreateComponentParams(FAudioDevice* InAudioDevice)
	: World(nullptr)
	, Actor(nullptr)
	, AudioDevice(InAudioDevice)
{
	CommonInit();
}

void FAudioDevice::FCreateComponentParams::CommonInit()
{
	bPlay = false;
	bStopWhenOwnerDestroyed = true;
	bLocationSet = false;
	AttenuationSettings = nullptr;
	ConcurrencySettings = nullptr;
	Location = FVector::ZeroVector;
}

void FAudioDevice::FCreateComponentParams::SetLocation(const FVector InLocation)
{
	if (World)
	{
		bLocationSet = true;
		Location = InLocation;
	}
	else
	{
		UE_LOG(LogAudio, Warning, TEXT("AudioComponents created without a World cannot have a location."));
	}
}

UAudioComponent* FAudioDevice::CreateComponent(USoundBase* Sound, UWorld* World, AActor* Actor, bool bPlay, bool bStopWhenOwnerDestroyed, const FVector* Location, USoundAttenuation* AttenuationSettings, USoundConcurrency* ConcurrencySettings)
{
	TUniquePtr<FCreateComponentParams> Params;

	if (Actor)
	{
		Params = MakeUnique<FCreateComponentParams>(Actor);
	}
	else if (World)
	{
		Params = MakeUnique<FCreateComponentParams>(World);
	}
	else
	{
		Params = MakeUnique<FCreateComponentParams>(GEngine->GetMainAudioDevice());
	}

	Params->bPlay = bPlay;
	Params->bStopWhenOwnerDestroyed = bStopWhenOwnerDestroyed;
	Params->AttenuationSettings = AttenuationSettings;
	Params->ConcurrencySettings = ConcurrencySettings;
	if (Location)
	{
		Params->SetLocation(*Location);
	}
	return CreateComponent(Sound, *Params);
}

UAudioComponent* FAudioDevice::CreateComponent(USoundBase* Sound, const FCreateComponentParams& Params)
{
	check(IsInGameThread());

	UAudioComponent* AudioComponent = nullptr;

	if (Sound && Params.AudioDevice && GEngine && GEngine->UseSound())
	{	
		// Avoid creating component if we're trying to play a sound on an already destroyed actor.
		if (Params.Actor == nullptr || !Params.Actor->IsPendingKill())
		{
			// Listener position could change before long sounds finish
			const FSoundAttenuationSettings* AttenuationSettingsToApply = (Params.AttenuationSettings ? &Params.AttenuationSettings->Attenuation : Sound->GetAttenuationSettingsToApply());

			bool bIsAudible = true;
			// If a sound is a long duration, the position might change before sound finishes so assume it's audible
			if (Params.bLocationSet && Sound->GetDuration() <= 1.0f)
			{
				float MaxDistance = 0.0f;
				float FocusFactor = 0.0f;
				Params.AudioDevice->GetMaxDistanceAndFocusFactor(Sound, Params.World, Params.Location, AttenuationSettingsToApply, MaxDistance, FocusFactor);
				bIsAudible = Params.AudioDevice->SoundIsAudible(Sound, Params.World, Params.Location, AttenuationSettingsToApply, MaxDistance, FocusFactor);
			}

			if (bIsAudible)
			{
				// Use actor as outer if we have one.
				if (Params.Actor)
				{
					AudioComponent = NewObject<UAudioComponent>(Params.Actor);
				}
				// Let engine pick the outer (transient package).
				else
				{
					AudioComponent = NewObject<UAudioComponent>();
				}

				check(AudioComponent);

				AudioComponent->Sound = Sound;
				AudioComponent->bAutoActivate = false;
				AudioComponent->bIsUISound = false;
				AudioComponent->bAutoDestroy = Params.bPlay && Params.bAutoDestroy;
				AudioComponent->bStopWhenOwnerDestroyed = Params.bStopWhenOwnerDestroyed;
#if WITH_EDITORONLY_DATA
				AudioComponent->bVisualizeComponent = false;
#endif
				AudioComponent->AttenuationSettings = Params.AttenuationSettings;
				AudioComponent->ConcurrencySettings = Params.ConcurrencySettings;

				if (Params.bLocationSet)
				{
					AudioComponent->SetWorldLocation(Params.Location);
				}

				// AudioComponent used in PlayEditorSound sets World to nullptr to avoid situations where the world becomes invalid
				// and the component is left with invalid pointer.
				if (Params.World)
				{
					AudioComponent->RegisterComponentWithWorld(Params.World);
				}
				else
				{
					AudioComponent->AudioDeviceHandle = Params.AudioDevice->DeviceHandle;
				}

				if (Params.bPlay)
				{
					AudioComponent->Play();
				}
			}
			else
			{
				// Don't create a sound component for short sounds that start out of range of any listener
				UE_LOG(LogAudio, Log, TEXT("AudioComponent not created for out of range Sound %s"), *Sound->GetName());
			}
		}
	}

	return AudioComponent;
}

void FAudioDevice::PlaySoundAtLocation(USoundBase* Sound, UWorld* World, float VolumeMultiplier, float PitchMultiplier, float StartTime, const FVector& Location, const FRotator& Rotation, USoundAttenuation* AttenuationSettings, USoundConcurrency* ConcurrencySettings, const TArray<FAudioComponentParam>* Params, AActor* OwningActor)
{
	check(IsInGameThread());

	if (!Sound || !World)
	{
		return;
	}

	// Not audible if the ticking level collection is not visible
	if (World && World->GetActiveLevelCollection() && !World->GetActiveLevelCollection()->IsVisible())
	{
		return;
	}

	const FSoundAttenuationSettings* AttenuationSettingsToApply = (AttenuationSettings ? &AttenuationSettings->Attenuation : Sound->GetAttenuationSettingsToApply());
	float MaxDistance = 0.0f;
	float FocusFactor = 0.0f;

	GetMaxDistanceAndFocusFactor(Sound, World, Location, AttenuationSettingsToApply, MaxDistance, FocusFactor);

	if (Sound->GetDuration() > 1.0f || SoundIsAudible(Sound, World, Location, AttenuationSettingsToApply, MaxDistance, FocusFactor))
	{
		const bool bIsInGameWorld = World->IsGameWorld();

		FActiveSound NewActiveSound;
		NewActiveSound.SetWorld(World);
		NewActiveSound.SetSound(Sound);
		NewActiveSound.VolumeMultiplier = VolumeMultiplier;
		NewActiveSound.PitchMultiplier = PitchMultiplier;
		NewActiveSound.RequestedStartTime = FMath::Max(0.0f, StartTime);
		NewActiveSound.bLocationDefined = true;
		NewActiveSound.Transform.SetTranslation(Location);
		NewActiveSound.Transform.SetRotation(FQuat(Rotation));
		NewActiveSound.bIsUISound = !bIsInGameWorld;
		NewActiveSound.SubtitlePriority = Sound->GetSubtitlePriority();

		NewActiveSound.bHasAttenuationSettings = (ShouldUseAttenuation(World) && AttenuationSettingsToApply);
		if (NewActiveSound.bHasAttenuationSettings)
		{
			const FGlobalFocusSettings& FocusSettings = GetGlobalFocusSettings();

			NewActiveSound.AttenuationSettings = *AttenuationSettingsToApply;
			NewActiveSound.FocusPriorityScale = AttenuationSettingsToApply->GetFocusPriorityScale(FocusSettings, FocusFactor);
			NewActiveSound.FocusDistanceScale = AttenuationSettingsToApply->GetFocusDistanceScale(FocusSettings, FocusFactor);
		}

		NewActiveSound.MaxDistance = MaxDistance;
		NewActiveSound.ConcurrencySettings = ConcurrencySettings;
		NewActiveSound.Priority = Sound->Priority;

		NewActiveSound.SetOwner(OwningActor);

		// Apply any optional audio component instance params on the sound
		if (Params)
		{
			for (const FAudioComponentParam& Param : *Params)
			{
				NewActiveSound.SetSoundParameter(Param);
			}
		}

		AddNewActiveSound(NewActiveSound);
	}
	else
	{
		// Don't play a sound for short sounds that start out of range of any listener
		UE_LOG(LogAudio, Log, TEXT("Sound not played for out of range Sound %s"), *Sound->GetName());
	}
}

void FAudioDevice::Flush(UWorld* WorldToFlush, bool bClearActivatedReverb)
{
	if (!IsInAudioThread())
	{
		DECLARE_CYCLE_STAT(TEXT("FAudioThreadTask.Flush"), STAT_AudioFlush, STATGROUP_AudioThreadCommands);

		FAudioDevice* AudioDevice = this;
		FAudioThread::RunCommandOnAudioThread([AudioDevice, WorldToFlush]()
		{
			AudioDevice->Flush(WorldToFlush);
		}, GET_STATID(STAT_AudioFlush));

		FAudioCommandFence AudioFence;
		AudioFence.BeginFence();
		AudioFence.Wait();

		// Clear the GameThread cache of the listner
		ListenerTransforms.Reset();
		ListenerTransforms.AddDefaulted();

		return;
	}

	// Do fadeout when flushing the audio device.
	if (WorldToFlush == nullptr || WorldToFlush->bIsTearingDown)
	{
		FadeOut();
	}

	// Stop all audio components attached to the scene
	bool bFoundIgnoredComponent = false;
	for (int32 Index = ActiveSounds.Num() - 1; Index >= 0; --Index)
	{
		FActiveSound* ActiveSound = ActiveSounds[Index];
		// if we are in the editor we want to always flush the ActiveSounds
		if (WorldToFlush && ActiveSound->bIgnoreForFlushing)
		{
			bFoundIgnoredComponent = true;
		}
		else
		{
			if (WorldToFlush == nullptr)
			{
				AddSoundToStop(ActiveSound);
			}
			else
			{
				UWorld* ActiveSoundWorld = ActiveSound->World.Get();
				if (ActiveSoundWorld == nullptr || ActiveSoundWorld == WorldToFlush)
				{
					AddSoundToStop(ActiveSound);
				}
			}
		}
	}

	// Immediately stop all pending active sounds
	ProcessingPendingActiveSoundStops(WorldToFlush == nullptr || WorldToFlush->bIsTearingDown);

	// Anytime we flush, make sure to clear all the listeners.  We'll get the right ones soon enough.
	Listeners.Reset();
	Listeners.Add(FListener(this));

	// Clear all the activated reverb effects
	if (bClearActivatedReverb)
	{
		ActivatedReverbs.Reset();
		bHasActivatedReverb = false;
	}

	if (WorldToFlush == nullptr)
	{
		// Make sure sounds are fully stopped.
		if (bFoundIgnoredComponent)
		{
			// We encountered an ignored component, so address the sounds individually.
			// There's no need to individually clear WaveInstanceSourceMap elements,
			// because FSoundSource::Stop(...) takes care of this.
			for (int32 SourceIndex = 0; SourceIndex < Sources.Num(); SourceIndex++)
			{
				const FWaveInstance* WaveInstance = Sources[SourceIndex]->GetWaveInstance();
				if (WaveInstance == nullptr || !WaveInstance->ActiveSound->bIgnoreForFlushing)
				{
					Sources[ SourceIndex ]->Stop();
				}
			}
		}
		else
		{
			// No components were ignored, so stop all sounds.
			for (int32 SourceIndex = 0; SourceIndex < Sources.Num(); SourceIndex++)
			{
				Sources[ SourceIndex ]->Stop();
			}

			WaveInstanceSourceMap.Reset();
		}
	}

	// Make sure we update any hardware changes that need to happen after flushing
	if (IsAudioMixerEnabled() && (WorldToFlush == nullptr || WorldToFlush->bIsTearingDown))
	{
		UpdateHardware();
	}
}

/**
 * Precaches the passed in sound node wave object.
 *
 * @param	SoundWave	Resource to be precached.
 */

void FAudioDevice::Precache(USoundWave* SoundWave, bool bSynchronous, bool bTrackMemory, bool bForceFullDecompression)
{
	LLM_SCOPE(ELLMTag::Audio);

	if (SoundWave == nullptr)
	{
		return;
	}

	// calculate the decompression type
	// @todo audio: maybe move this into SoundWave?
	if (SoundWave->NumChannels == 0)
	{
		// No channels - no way of knowing what to play back
		SoundWave->DecompressionType = DTYPE_Invalid;
	}
	else if (SoundWave->RawPCMData)
	{
		// Run time created audio; e.g. editor preview data
		SoundWave->DecompressionType = DTYPE_Preview;
	}
	else if (SoundWave->bProcedural)
	{
		// Procedurally created audio
		SoundWave->DecompressionType = DTYPE_Procedural;
	}
	else if (SoundWave->bIsBus)
	{
		// Audio data which will be generated by instanced objects, not from the sound wave asset
		if (IsAudioMixerEnabled())
		{
			// Buses will initialize as procedural, but not actually become a procedural sound wave
			SoundWave->DecompressionType = DTYPE_Procedural;
		}
		else
		{
			// Buses are only supported with audio mixer
			SoundWave->DecompressionType = DTYPE_Invalid;
		}
	}
	else if (HasCompressedAudioInfoClass(SoundWave))
	{
		const FSoundGroup& SoundGroup = GetDefault<USoundGroups>()->GetSoundGroup(SoundWave->SoundGroup);

		float CompressedDurationThreshold = SoundGroup.DecompressedDuration;
		/*
		if (CompressedDurationThreshold > 0.0f)
		{
			CompressedDurationThreshold = 1.0f;
		}
		*/

		// handle audio decompression
		if (FPlatformProperties::SupportsAudioStreaming() && SoundWave->IsStreaming())
		{
			SoundWave->DecompressionType = DTYPE_Streaming;
			SoundWave->bCanProcessAsync = false;
		}
		else if (!bForceFullDecompression && SupportsRealtimeDecompression() &&  (bDisableAudioCaching || (!SoundGroup.bAlwaysDecompressOnLoad && SoundWave->Duration > CompressedDurationThreshold)))
		{
			// Store as compressed data and decompress in realtime
			SoundWave->DecompressionType = DTYPE_RealTime;
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			++PrecachedRealtime;
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		}
		else
		{
			// Fully expand loaded audio data into PCM
			SoundWave->DecompressionType = DTYPE_Native;
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			++PrecachedNative;
			AverageNativeLength = (AverageNativeLength * (PrecachedNative - 1) + SoundWave->Duration) / PrecachedNative;
			NativeSampleRateCount.FindOrAdd(SoundWave->SampleRate)++;
			NativeChannelCount.FindOrAdd(SoundWave->NumChannels)++;
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		}

		// Grab the compressed audio data
		SoundWave->InitAudioResource(GetRuntimeFormat(SoundWave));

		if (SoundWave->AudioDecompressor == nullptr && (SoundWave->DecompressionType == DTYPE_Native || SoundWave->DecompressionType == DTYPE_RealTime))
		{
			// Create a worker to decompress the audio data
			if (bSynchronous)
			{
				// Create a worker to decompress the vorbis data
				FAsyncAudioDecompress TempDecompress(SoundWave);
				TempDecompress.StartSynchronousTask();
			}
			else
			{
				// This should only happen in the game thread.
				ensure(IsInGameThread());
				SoundWave->bIsPrecacheDone = false;
				SoundWave->AudioDecompressor = new FAsyncAudioDecompress(SoundWave);
				SoundWave->AudioDecompressor->StartBackgroundTask();
			}

			static FName NAME_OGG(TEXT("OGG"));
			SoundWave->bDecompressedFromOgg = GetRuntimeFormat(SoundWave) == NAME_OGG;

			// the audio decompressor will track memory
			if (SoundWave->DecompressionType == DTYPE_Native)
			{
				bTrackMemory = false;
			}
		}
	}
	else
	{
		// Preserve old behavior if there is no compressed audio info class for this audio format
		SoundWave->DecompressionType = DTYPE_Native;
	}

	if (bTrackMemory)
	{
		const int32 ResourceSize = SoundWave->GetResourceSizeBytes(EResourceSizeMode::Exclusive);
		SoundWave->TrackedMemoryUsage += ResourceSize;

		// If we aren't decompressing it above, then count the memory
		INC_DWORD_STAT_BY(STAT_AudioMemorySize, ResourceSize);
		INC_DWORD_STAT_BY(STAT_AudioMemory, ResourceSize);
	}
}

void FAudioDevice::StopSourcesUsingBuffer(FSoundBuffer* SoundBuffer)
{
	SCOPED_NAMED_EVENT(FAudioDevice_StopSourcesUsingBuffer, FColor::Blue);

	check(IsInAudioThread());

	if (SoundBuffer)
	{
		for (int32 SrcIndex = 0; SrcIndex < Sources.Num(); SrcIndex++)
		{
			FSoundSource* Src = Sources[SrcIndex];
			if (Src && Src->Buffer == SoundBuffer)
			{
				// Make sure the buffer is no longer referenced by anything
				Src->Stop();
				break;
			}
		}
	}
}

void FAudioDevice::RegisterSoundClass(USoundClass* InSoundClass)
{
	if (InSoundClass)
	{
		if (!IsInAudioThread())
		{
			DECLARE_CYCLE_STAT(TEXT("FAudioThreadTask.RegisterSoundClass"), STAT_AudioRegisterSoundClass, STATGROUP_AudioThreadCommands);

			FAudioDevice* AudioDevice = this;
			FAudioThread::RunCommandOnAudioThread([AudioDevice, InSoundClass]()
			{
				AudioDevice->RegisterSoundClass(InSoundClass);
			}, GET_STATID(STAT_AudioRegisterSoundClass));

			return;
		}

		// If the sound class wasn't already registered get it in to the system.
		if (!SoundClasses.Contains(InSoundClass))
		{
			SoundClasses.Add(InSoundClass, FSoundClassProperties());
		}
	}
}

void FAudioDevice::UnregisterSoundClass(USoundClass* SoundClass)
{
	check(IsInAudioThread());
	if (SoundClass)
	{
		SoundClasses.Remove(SoundClass);
	}
}

FSoundClassProperties* FAudioDevice::GetSoundClassCurrentProperties(USoundClass* InSoundClass)
{
	if (InSoundClass)
	{
		check(IsInAudioThread());

		FSoundClassProperties* Properties = SoundClasses.Find(InSoundClass);
		return Properties;
	}
	return nullptr;
}


#if !UE_BUILD_SHIPPING
/** 
 * Displays debug information about the loaded sounds
 */
bool FAudioDevice::HandleListSoundsCommand(const TCHAR* Cmd, FOutputDevice& Ar)
{
	FAudioThreadSuspendContext AudioThreadSuspend;

	// does the user want to sort by name instead of size?
	const bool bAlphaSort = FParse::Param(Cmd, TEXT("ALPHASORT"));
	const bool bUseLongNames = FParse::Param(Cmd, TEXT("LONGNAMES"));

	int32 TotalResident = 0;
	int32 ResidentCount = 0;

	Ar.Logf(TEXT("Listing all sounds:"));

	// Get audio device manager since thats where sound buffers are stored
	FAudioDeviceManager* AudioDeviceManager = GEngine->GetAudioDeviceManager();
	check(AudioDeviceManager != nullptr);

	TArray<FSoundBuffer*> AllSounds;
	for (int32 BufferIndex = 0; BufferIndex < AudioDeviceManager->Buffers.Num(); BufferIndex++)
	{
		AllSounds.Add(AudioDeviceManager->Buffers[BufferIndex]);
	}

	// sort by name or size, depending on flag
	if (bAlphaSort)
	{
		AllSounds.Sort([](FSoundBuffer& A, FSoundBuffer& B) { return A.ResourceName < B.ResourceName; });
	}
	else
	{
		// sort memory usage from large to small 
		AllSounds.Sort([](FSoundBuffer& A, FSoundBuffer& B) { return B.GetSize() < A.GetSize(); });
	}

	// now list the sorted sounds
	for (int32 BufferIndex = 0; BufferIndex < AllSounds.Num(); BufferIndex++)
	{
		FSoundBuffer* Buffer = AllSounds[BufferIndex];

		// format info string
		Ar.Logf(TEXT("%s"), *Buffer->Describe(bUseLongNames));

		// track memory and count
		TotalResident += Buffer->GetSize();
		ResidentCount++;
	}

	Ar.Logf(TEXT("%8.2f Kb for %d resident sounds"), TotalResident / 1024.0f, ResidentCount);
	return true;
}
#endif // !UE_BUILD_SHIPPING

void FAudioDevice::StopSoundsUsingResource(USoundWave* SoundWave, TArray<UAudioComponent*>* StoppedComponents)
{
	if (StoppedComponents == nullptr && !IsInAudioThread())
	{
		DECLARE_CYCLE_STAT(TEXT("FAudioThreadTask.StopSoundsUsingResource"), STAT_AudioStopSoundsUsingResource, STATGROUP_AudioThreadCommands);

		FAudioDevice* AudioDevice = this;
		FAudioThread::RunCommandOnAudioThread([AudioDevice, SoundWave]()
		{
			AudioDevice->StopSoundsUsingResource(SoundWave);
		}, GET_STATID(STAT_AudioStopSoundsUsingResource));

		return;
	}
	else if (StoppedComponents)
	{
		check(IsInGameThread());
		FAudioCommandFence AudioFence;
		AudioFence.BeginFence();
		AudioFence.Wait();
	}

	bool bStoppedSounds = false;

	for (int32 ActiveSoundIndex = ActiveSounds.Num() - 1; ActiveSoundIndex >= 0; --ActiveSoundIndex)
	{
		FActiveSound* ActiveSound = ActiveSounds[ActiveSoundIndex];
		for (const TPair<UPTRINT, FWaveInstance*>& WaveInstancePair : ActiveSound->WaveInstances)
		{
			// If anything the ActiveSound uses the wave then we stop the sound
			FWaveInstance* WaveInstance = WaveInstancePair.Value;
			if (WaveInstance->WaveData == SoundWave)
			{
				if (StoppedComponents)
				{
					if (UAudioComponent* AudioComponent = UAudioComponent::GetAudioComponentFromID(ActiveSound->GetAudioComponentID()))
					{
						StoppedComponents->Add(AudioComponent);
					}
				}
				AddSoundToStop(ActiveSound);
				bStoppedSounds = true;
				break;
			}
		}
	}

	// Immediately stop all pending active sounds
	ProcessingPendingActiveSoundStops();

	if (!GIsEditor && bStoppedSounds)
	{
		UE_LOG(LogAudio, Warning, TEXT("All Sounds using SoundWave '%s' have been stopped"), *SoundWave->GetName());
	}
}

void FAudioDevice::RegisterPluginListener(const TAudioPluginListenerPtr PluginListener)
{
	PluginListeners.AddUnique(PluginListener);
}

void FAudioDevice::UnregisterPluginListener(const TAudioPluginListenerPtr PluginListener)
{
	PluginListeners.RemoveSingle(PluginListener);
}

bool FAudioDevice::IsAudioDeviceMuted() const
{
	check(IsInAudioThread());

	// First check to see if the device manager has "bPlayAllPIEAudio" enabled
	FAudioDeviceManager* DeviceManager = GEngine->GetAudioDeviceManager();
	if (DeviceManager && DeviceManager->IsPlayAllDeviceAudio())
	{
		return false;
	}

	return bIsDeviceMuted;
}

void FAudioDevice::SetDeviceMuted(const bool bMuted)
{
	if (!IsInAudioThread())
	{
		FAudioDevice* AudioDevice = this;
		FAudioThread::RunCommandOnAudioThread([AudioDevice, bMuted]()
		{
			AudioDevice->SetDeviceMuted(bMuted);

		});

		return;
	}

	bIsDeviceMuted = bMuted;
}

FVector FAudioDevice::GetListenerTransformedDirection(const FVector& Position, float* OutDistance)
{
	check(IsInAudioThread());
	FVector UnnormalizedDirection = InverseListenerTransform.TransformPosition(Position);
	if (OutDistance)
	{
		*OutDistance = UnnormalizedDirection.Size();
	}
	return UnnormalizedDirection.GetSafeNormal();
}

float FAudioDevice::GetDeviceDeltaTime() const
{
	// Clamp the delta time to a reasonable max delta time. 
	return FMath::Min(DeviceDeltaTime, 0.5f);
}

float FAudioDevice::GetGameDeltaTime() const
{
	float DeltaTime = FApp::GetDeltaTime();

	// Clamp the delta time to a reasonable max delta time. 
	return FMath::Min(DeltaTime, 0.5f);
}

#if WITH_EDITOR
void FAudioDevice::OnBeginPIE(const bool bIsSimulating)
{
	for (TObjectIterator<USoundNode> It; It; ++It)
	{
		USoundNode* SoundNode = *It;
		SoundNode->OnBeginPIE(bIsSimulating);
	}
}

void FAudioDevice::OnEndPIE(const bool bIsSimulating)
{
	for (TObjectIterator<USoundNode> It; It; ++It)
	{
		USoundNode* SoundNode = *It;
		SoundNode->OnEndPIE(bIsSimulating);
	}
}
#endif

bool FAudioDevice::CanUseVRAudioDevice()
{
#if WITH_EDITOR
	if (GIsEditor)
	{
		UEditorEngine* EdEngine = Cast<UEditorEngine>(GEngine);
		return EdEngine->bUseVRPreviewForPlayWorld;
	}
	else
#endif
	{
		return FParse::Param(FCommandLine::Get(), TEXT("vr")) || GetDefault<UGeneralProjectSettings>()->bStartInVR;
	}
}

void FAudioDevice::SetTransientMasterVolume(const float InTransientMasterVolume)
{
	if (!IsInAudioThread())
	{
		FAudioDevice* AudioDevice = this;
		FAudioThread::RunCommandOnAudioThread([AudioDevice, InTransientMasterVolume]()
		{
			AudioDevice->SetTransientMasterVolume(InTransientMasterVolume);
		});

		return;
	}

	TransientMasterVolume = InTransientMasterVolume;
}

FSoundSource* FAudioDevice::GetSoundSource(FWaveInstance* WaveInstance) const
{
	check(IsInAudioThread());
	return WaveInstanceSourceMap.FindRef( WaveInstance );
}

const FGlobalFocusSettings& FAudioDevice::GetGlobalFocusSettings() const
{
	if (IsInAudioThread())
	{
		return GlobalFocusSettings;
	}

	check(IsInGameThread());
	return GlobalFocusSettings_GameThread;
}

void FAudioDevice::SetGlobalFocusSettings(const FGlobalFocusSettings& NewFocusSettings)
{
	check(IsInGameThread());

	GlobalFocusSettings_GameThread = NewFocusSettings;

	DECLARE_CYCLE_STAT(TEXT("FAudioThreadTask.SetGlobalListenerFocusParameters"), STAT_AudioSetGlobalListenerFocusParameters, STATGROUP_TaskGraphTasks);
	FAudioDevice* AudioDevice = this;
	FAudioThread::RunCommandOnAudioThread([AudioDevice, NewFocusSettings]()
	{
		AudioDevice->GlobalFocusSettings = NewFocusSettings;
	}, GET_STATID(STAT_AudioSetGlobalListenerFocusParameters));
}

void FAudioDevice::SetGlobalPitchModulation(float PitchModulation, float TimeSec)
{
	if (!IsInAudioThread())
	{
		DECLARE_CYCLE_STAT(TEXT("FAudioThreadTask.SetGlobalPitchModulation"), STAT_AudioSetGlobalPitchModulation, STATGROUP_TaskGraphTasks);

		FAudioDevice* AudioDevice = this;
		FAudioThread::RunCommandOnAudioThread([AudioDevice, PitchModulation, TimeSec]()
		{
			AudioDevice->SetGlobalPitchModulation(PitchModulation, TimeSec);
		}, GET_STATID(STAT_AudioSetGlobalPitchModulation));

		return;
	}

	GlobalPitchScale.Set(PitchModulation, TimeSec);
}

void FAudioDevice::SetPlatformAudioHeadroom(const float InPlatformHeadRoom)
{
	if (!IsInAudioThread())
	{
		FAudioDevice* AudioDevice = this;
		FAudioThread::RunCommandOnAudioThread([AudioDevice, InPlatformHeadRoom]()
		{
			AudioDevice->SetPlatformAudioHeadroom(InPlatformHeadRoom);
		});

		return;
	}

	PlatformAudioHeadroom = InPlatformHeadRoom;
}

#if !UE_BUILD_SHIPPING
void HandleDumpActiveSounds(UWorld* World)
{
	if (FAudioDevice* AudioDevice = (World ? World->GetAudioDevice() : GEngine->GetMainAudioDevice()))
	{
		AudioDevice->DumpActiveSounds();
	}
}

static FAutoConsoleCommandWithWorld DumpActiveSounds(TEXT("Audio.DumpActiveSounds"), TEXT("Outputs data about all the currently active sounds."), FConsoleCommandWithWorldDelegate::CreateStatic(&HandleDumpActiveSounds), ECVF_Cheat);

void FAudioDevice::DumpActiveSounds() const
{
	check(IsInGameThread());

	FAudioThreadSuspendContext SuspendAudio;

	UE_LOG(LogAudio, Display, TEXT("Active Sound Count: %d"), ActiveSounds.Num());
	UE_LOG(LogAudio, Display, TEXT("------------------------"), ActiveSounds.Num());

	for (const FActiveSound* ActiveSound : ActiveSounds)
	{
		if (ActiveSound)
		{
			UE_LOG(LogAudio, Display, TEXT("%s (%.3g) - %s"), *ActiveSound->GetSound()->GetName(), ActiveSound->GetSound()->GetDuration(), *ActiveSound->GetAudioComponentName());

			for (const TPair<UPTRINT, FWaveInstance*>& WaveInstancePair : ActiveSound->WaveInstances)
			{
				const FWaveInstance* WaveInstance = WaveInstancePair.Value;
				UE_LOG(LogAudio, Display, TEXT("   %s (%.3g) (%d) - %.3g"), *WaveInstance->GetName(), WaveInstance->WaveData->GetDuration(), WaveInstance->WaveData->GetResourceSizeBytes(EResourceSizeMode::Inclusive), WaveInstance->GetActualVolume());
			}
		}
	}


}
#endif

bool FAudioDevice::ShouldUseAttenuation(const UWorld* World) const
{
	// We use attenuation settings:
	// - if we don't have a world, or
	// - we have a game world, or
	// - we are forcing the use of attenuation (e.g. for some editors)
	const bool bIsInGameWorld = World ? World->IsGameWorld() : true;
	return (bIsInGameWorld || bUseAttenuationForNonGameWorlds);
}