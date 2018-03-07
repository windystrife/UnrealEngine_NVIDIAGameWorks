// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Sound/SoundNodeDialoguePlayer.h"
#include "Audio.h"
#include "ActiveSound.h"
#include "Sound/SoundBase.h"
#include "Sound/DialogueWave.h"

#define LOCTEXT_NAMESPACE "SoundNodeDialoguePlayer"

USoundNodeDialoguePlayer::USoundNodeDialoguePlayer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void USoundNodeDialoguePlayer::ParseNodes( FAudioDevice* AudioDevice, const UPTRINT NodeWaveInstanceHash, FActiveSound& ActiveSound, const FSoundParseParameters& ParseParams, TArray<FWaveInstance*>& WaveInstances )
{
	USoundBase* SoundBase = GetDialogueWave() ? GetDialogueWave()->GetWaveFromContext(DialogueWaveParameter.Context) : NULL;
	if (SoundBase)
	{
		if (bLooping)
		{
			FSoundParseParameters UpdatedParams = ParseParams;
			UpdatedParams.bLooping = true;
			SoundBase->Parse(AudioDevice, NodeWaveInstanceHash, ActiveSound, UpdatedParams, WaveInstances);
		}
		else
		{
			SoundBase->Parse(AudioDevice, NodeWaveInstanceHash, ActiveSound, ParseParams, WaveInstances);
		}
	}
}

float USoundNodeDialoguePlayer::GetDuration()
{
	USoundBase* SoundBase = GetDialogueWave() ? GetDialogueWave()->GetWaveFromContext(DialogueWaveParameter.Context) : NULL;
	float Duration = 0.f;
	if (SoundBase)
	{
		if (bLooping)
		{
			Duration = INDEFINITELY_LOOPING_DURATION;
		}
		else
		{
			Duration = SoundBase->GetDuration();
		}
	}
	return Duration;
}

// A Wave Player is the end of the chain and has no children
int32 USoundNodeDialoguePlayer::GetMaxChildNodes() const
{
	return 0;
}

#if WITH_EDITOR
FText USoundNodeDialoguePlayer::GetTitle() const
{
	FText DialogueWaveName;
	if (GetDialogueWave())
	{
		DialogueWaveName = FText::FromString(GetDialogueWave()->GetFName().ToString());
	}
	else
	{
		DialogueWaveName = LOCTEXT("NoDialogueWave", "NONE");
	}

	FText Title;

	FFormatNamedArguments Arguments;
	Arguments.Add(TEXT("Description"), Super::GetTitle());
	Arguments.Add(TEXT("DialogueWaveName"), DialogueWaveName);
	if (bLooping)
	{
		Title = FText::Format(LOCTEXT("LoopingDialogueWaveDescription", "Looping {Description} : {DialogueWaveName}"), Arguments);
	}
	else
	{
		Title = FText::Format(LOCTEXT("NonLoopingDialogueWaveDescription", "{Description} : {DialogueWaveName}"), Arguments);
	}

	return Title;
}
#endif

void USoundNodeDialoguePlayer::SetDialogueWave(UDialogueWave* Value)
{
	DialogueWaveParameter.DialogueWave = Value;
}

UDialogueWave* USoundNodeDialoguePlayer::GetDialogueWave() const
{
	return DialogueWaveParameter.DialogueWave;
}

#undef LOCTEXT_NAMESPACE
