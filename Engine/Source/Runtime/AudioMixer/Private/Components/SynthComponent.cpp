// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SynthComponent.h"
#include "AudioDevice.h"
#include "AudioMixerLog.h"
#include "Components/BillboardComponent.h"

USynthSound::USynthSound(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, OwningSynthComponent(nullptr)
{
}

void USynthSound::Init(USynthComponent* InSynthComponent, int32 InNumChannels)
{
	OwningSynthComponent = InSynthComponent;
	bVirtualizeWhenSilent = true;
	NumChannels = InNumChannels;

	// Turn off async generation in old audio engine on mac.
#if PLATFORM_MAC
	if (!InSynthComponent->GetAudioDevice()->IsAudioMixerEnabled())
	{
		bCanProcessAsync = false;
	}
	else
#endif // #if PLATFORM_MAC
	{
		bCanProcessAsync = true;
	}

	Duration = INDEFINITELY_LOOPING_DURATION;
	bLooping = true;
	SampleRate = InSynthComponent->GetAudioDevice()->SampleRate;
	bAudioMixer = InSynthComponent->GetAudioDevice()->IsAudioMixerEnabled();
}

bool USynthSound::OnGeneratePCMAudio(TArray<uint8>& OutAudio, int32 NumSamples)
{
	OutAudio.Reset();

	if (bAudioMixer)
	{
		// If running with audio mixer, the output audio buffer will be in floats already
		OutAudio.AddZeroed(NumSamples * sizeof(float));
		OwningSynthComponent->OnGeneratePCMAudio((float*)OutAudio.GetData(), NumSamples);
	}
	else
	{
		// Use the float scratch buffer instead of the out buffer directly
		FloatBuffer.Reset();
		FloatBuffer.AddZeroed(NumSamples * sizeof(float));
		
		float* FloatBufferDataPtr = FloatBuffer.GetData();
		OwningSynthComponent->OnGeneratePCMAudio(FloatBufferDataPtr, NumSamples);

		// Convert the float buffer to int16 data
		OutAudio.AddZeroed(NumSamples * sizeof(int16));
		int16* OutAudioBuffer = (int16*)OutAudio.GetData();
		for (int32 i = 0; i < NumSamples; ++i)
		{
			OutAudioBuffer[i] = (int16)(32767.0f * FloatBufferDataPtr[i]);
		}
	}

	return true;
}

Audio::EAudioMixerStreamDataFormat::Type USynthSound::GetGeneratedPCMDataFormat() const 
{ 
	// Only audio mixer supports return float buffers
	return bAudioMixer ? Audio::EAudioMixerStreamDataFormat::Float : Audio::EAudioMixerStreamDataFormat::Int16;
}

USynthComponent::USynthComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

	bAutoActivate = false;
	bStopWhenOwnerDestroyed = true;

	bNeverNeedsRenderUpdate = true;
	bUseAttachParentBound = true; // Avoid CalcBounds() when transform changes.

	bIsSynthPlaying = false;
	bIsInitialized = false;
	bIsUISound = false;

	// Set the default sound class
	SoundClass = USoundBase::DefaultSoundClassObject;

#if WITH_EDITORONLY_DATA
	bVisualizeComponent = false;
#endif
}

void USynthComponent::Activate(bool bReset)
{
	if (bReset || ShouldActivate() == true)
	{
		Start();
		if (bIsActive)
		{
			OnComponentActivated.Broadcast(this, bReset);
		}
	}
}

void USynthComponent::Deactivate()
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

void USynthComponent::Initialize()
{
	FAudioDevice* AudioDevice = GetAudioDevice();
	if (!bIsInitialized && AudioDevice)
	{
		bIsInitialized = true;

		const int32 SampleRate = AudioDevice->SampleRate;

#if SYNTH_GENERATOR_TEST_TONE
		NumChannels = 2;
		TestSineLeft.Init(SampleRate, 440.0f, 0.5f);
		TestSineRight.Init(SampleRate, 220.0f, 0.5f);
#else	
		// Initialize the synth component
		this->Init(SampleRate);

		if (NumChannels < 0 || NumChannels > 2)
		{
			UE_LOG(LogAudioMixer, Error, TEXT("Synthesis component '%s' has set an invalid channel count '%d' (only mono and stereo currently supported)."), *GetName(), NumChannels);
		}

		NumChannels = FMath::Clamp(NumChannels, 1, 2);
#endif

		Synth = NewObject<USynthSound>(this, TEXT("Synth"));

		// Copy sound base data to the sound
		Synth->SourceEffectChain = SourceEffectChain;
		Synth->SoundSubmixObject = SoundSubmix;
		Synth->SoundSubmixSends = SoundSubmixSends;

		Synth->Init(this, NumChannels);
	}
}

UAudioComponent* USynthComponent::GetAudioComponent()
{
	return AudioComponent;
}

void USynthComponent::CreateAudioComponent()
{
	if (!AudioComponent)
	{
		// Create the audio component which will be used to play the procedural sound wave
		AudioComponent = NewObject<UAudioComponent>(this);

		if (AudioComponent)
		{
			AudioComponent->bAutoActivate = false;
			AudioComponent->bStopWhenOwnerDestroyed = true;
			AudioComponent->bShouldRemainActiveIfDropped = true;
			AudioComponent->Mobility = EComponentMobility::Movable;

#if WITH_EDITORONLY_DATA
			AudioComponent->bVisualizeComponent = false;
#endif
			if (AudioComponent->GetAttachParent() == nullptr && !AudioComponent->IsAttachedTo(this))
			{
				AudioComponent->SetupAttachment(this);
			}

			Initialize();
		}
	}
}


void USynthComponent::OnRegister()
{
	CreateAudioComponent();

	Super::OnRegister();
}

void USynthComponent::OnUnregister()
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

	// Make sure the audio component is destroyed during unregister
	if (AudioComponent)
	{
		AudioComponent->DestroyComponent();
		AudioComponent = nullptr;
	}
}

bool USynthComponent::IsReadyForOwnerToAutoDestroy() const
{
	return !AudioComponent || (AudioComponent && !AudioComponent->IsPlaying());
}

#if WITH_EDITOR
void USynthComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (bIsActive)
	{
		// If this is an auto destroy component we need to prevent it from being auto-destroyed since we're really just restarting it
		const bool bWasAutoDestroy = bAutoDestroy;
		bAutoDestroy = false;
		Stop();
		bAutoDestroy = bWasAutoDestroy;
		Start();
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

void USynthComponent::PumpPendingMessages()
{
	TFunction<void()> Command;
	while (CommandQueue.Dequeue(Command))
	{
		Command();
	}

	ESynthEvent SynthEvent;
	while (PendingSynthEvents.Dequeue(SynthEvent))
	{
		switch (SynthEvent)
		{
			case ESynthEvent::Start:
				bIsSynthPlaying = true;
				this->OnStart();
				break;

			case ESynthEvent::Stop:
				bIsSynthPlaying = false;
				this->OnStop();
				break;

			default:
				break;
		}
	}
}

void USynthComponent::OnGeneratePCMAudio(float* GeneratedPCMData, int32 NumSamples)
{
	PumpPendingMessages();

	check(NumSamples > 0);

	// Only call into the synth if we're actually playing, otherwise, we'll write out zero's
	if (bIsSynthPlaying)
	{
		this->OnGenerateAudio(GeneratedPCMData, NumSamples);
	}
}

void USynthComponent::Start()
{
	// This will try to create the audio component if it hasn't yet been created
	CreateAudioComponent();

	if (AudioComponent)
	{
		// Copy the attenuation and concurrency data from the synth component to the audio component
		AudioComponent->AttenuationSettings = AttenuationSettings;
		AudioComponent->bOverrideAttenuation = bOverrideAttenuation;
		AudioComponent->bIsUISound = bIsUISound;
		AudioComponent->ConcurrencySettings = ConcurrencySettings;
		AudioComponent->AttenuationOverrides = AttenuationOverrides;
		AudioComponent->SoundClassOverride = SoundClass;

		// Set the audio component's sound to be our procedural sound wave
		AudioComponent->SetSound(Synth);
		AudioComponent->Play(0);

		bIsActive = AudioComponent->IsActive();

		if (bIsActive)
		{
			PendingSynthEvents.Enqueue(ESynthEvent::Start);
		}
	}
}

void USynthComponent::Stop()
{
	if (bIsActive)
	{
		PendingSynthEvents.Enqueue(ESynthEvent::Stop);

		if (AudioComponent)
		{
			AudioComponent->Stop();
		}

		bIsActive = false;
	}
}

bool USynthComponent::IsPlaying() const
{
	return AudioComponent && AudioComponent->IsPlaying();
}

void USynthComponent::SetSubmixSend(USoundSubmix* Submix, float SendLevel)
{
	if (AudioComponent)
	{
		AudioComponent->SetSubmixSend(Submix, SendLevel);
	}
}


void USynthComponent::SynthCommand(TFunction<void()> Command)
{
	CommandQueue.Enqueue(MoveTemp(Command));
}