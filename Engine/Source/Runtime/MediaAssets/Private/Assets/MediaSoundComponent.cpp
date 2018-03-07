// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MediaSoundComponent.h"

#include "IMediaAudioSample.h"
#include "IMediaPlayer.h"
#include "MediaAudioResampler.h"
#include "Misc/ScopeLock.h"

#include "MediaPlayer.h"
#include "MediaPlayerFacade.h"


/* UMediaSoundComponent structors
 *****************************************************************************/

UMediaSoundComponent::UMediaSoundComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, Channels(EMediaSoundChannels::Stereo)
	, Resampler(new FMediaAudioResampler)
{
	PrimaryComponentTick.bCanEverTick = true;
	bAutoActivate = true;
}


UMediaSoundComponent::~UMediaSoundComponent()
{
	delete Resampler;
}


/* UMediaSoundComponent interface
 *****************************************************************************/

void UMediaSoundComponent::UpdatePlayer()
{
	if (MediaPlayer == nullptr)
	{
		CurrentPlayerFacade.Reset();
		SampleQueue.Reset();

		return;
	}

	// create a new sample queue if the player changed
	TSharedRef<FMediaPlayerFacade, ESPMode::ThreadSafe> PlayerFacade = MediaPlayer->GetPlayerFacade();

	if (PlayerFacade != CurrentPlayerFacade)
	{
		{
			const auto NewSampleQueue = MakeShared<FMediaAudioSampleQueue, ESPMode::ThreadSafe>();

			FScopeLock Lock(&CriticalSection);
			SampleQueue = NewSampleQueue;
		}

		PlayerFacade->AddAudioSampleSink(SampleQueue.ToSharedRef());
		CurrentPlayerFacade = PlayerFacade;
	}

	check(SampleQueue.IsValid());
}


/* UActorComponent interface
 *****************************************************************************/

void UMediaSoundComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UpdatePlayer();
}


/* USynthComponent interface
 *****************************************************************************/

void UMediaSoundComponent::Activate(bool bReset)
{
	if (bReset || ShouldActivate())
	{
		SetComponentTickEnabled(true);
	}

	Super::Activate(bReset);
}


void UMediaSoundComponent::Deactivate()
{
	if (!ShouldActivate())
	{
		SetComponentTickEnabled(false);
	}

	Super::Deactivate();
}


/* USynthComponent interface
 *****************************************************************************/

void UMediaSoundComponent::Init(const int32 SampleRate)
{
	Super::Init(SampleRate);

	if (Channels == EMediaSoundChannels::Mono)
	{
		NumChannels = 1;
	}
	else //if (Channels == EMediaSoundChannels::Stereo)
	{
		NumChannels = 2;
	}/*
	else
	{
		NumChannels = 8;
	}*/

	Resampler->Initialize(NumChannels, SampleRate);
}


void UMediaSoundComponent::OnGenerateAudio(float* OutAudio, int32 NumSamples)
{
	TSharedPtr<FMediaPlayerFacade, ESPMode::ThreadSafe> PinnedPlayerFacade;
	TSharedPtr<FMediaAudioSampleQueue, ESPMode::ThreadSafe> PinnedSampleQueue;
	{
		FScopeLock Lock(&CriticalSection);

		PinnedPlayerFacade = CurrentPlayerFacade.Pin();
		PinnedSampleQueue = SampleQueue;
	}

	if (PinnedPlayerFacade.IsValid() && PinnedPlayerFacade->IsPlaying() && PinnedSampleQueue.IsValid())
	{
		const uint32 FramesRequested = NumSamples / NumChannels;
		const uint32 FramesWritten = Resampler->Generate(OutAudio, FramesRequested, PinnedPlayerFacade->GetRate(), PinnedPlayerFacade->GetTime(), *PinnedSampleQueue);
	}
	else
	{
		Resampler->Flush();
	}
}
