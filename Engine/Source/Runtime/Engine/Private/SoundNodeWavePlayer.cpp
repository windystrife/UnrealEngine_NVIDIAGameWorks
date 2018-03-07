// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Sound/SoundNodeWavePlayer.h"
#include "Audio.h"
#include "ActiveSound.h"
#include "Sound/SoundWave.h"
#include "FrameworkObjectVersion.h"

#define LOCTEXT_NAMESPACE "SoundNodeWavePlayer"

void USoundNodeWavePlayer::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FFrameworkObjectVersion::GUID);

	if (Ar.CustomVer(FFrameworkObjectVersion::GUID) >= FFrameworkObjectVersion::HardSoundReferences)
	{
		if (Ar.IsLoading())
		{
			Ar << SoundWave;
		}
		else if (Ar.IsSaving())
		{
			USoundWave* HardReference = (ShouldHardReferenceAsset() ? SoundWave : nullptr);
			Ar << HardReference;
		}
	}
}

void USoundNodeWavePlayer::LoadAsset(bool bAddToRoot)
{
	if (IsAsyncLoading())
	{
		SoundWave = SoundWaveAssetPtr.Get();
		if (SoundWave == nullptr)
		{
			const FString LongPackageName = SoundWaveAssetPtr.GetLongPackageName();
			if (!LongPackageName.IsEmpty())
			{
				bAsyncLoading = true;
				LoadPackageAsync(LongPackageName, FLoadPackageAsyncDelegate::CreateUObject(this, &USoundNodeWavePlayer::OnSoundWaveLoaded, bAddToRoot));
			}
		}
		else if (bAddToRoot)
		{
			SoundWave->AddToRoot();
		}
		if (SoundWave)
		{
			SoundWave->AddToCluster(this);
		}
	}
	else
	{
		SoundWave = SoundWaveAssetPtr.LoadSynchronous();
		if (SoundWave)
		{
			if (bAddToRoot)
			{
				SoundWave->AddToRoot();
			}
			SoundWave->AddToCluster(this);
		}
	}
}

void USoundNodeWavePlayer::ClearAssetReferences()
{
	SoundWave = nullptr;
}

void USoundNodeWavePlayer::OnSoundWaveLoaded(const FName& PackageName, UPackage* Package, EAsyncLoadingResult::Type Result, bool bAddToRoot)
{
	if (Result == EAsyncLoadingResult::Succeeded)
	{
		SoundWave = SoundWaveAssetPtr.Get();
		if (SoundWave)
		{
			if (bAddToRoot)
			{
				SoundWave->AddToRoot();
			}
			SoundWave->AddToCluster(this);
		}
	}
	bAsyncLoading = false;
}

void USoundNodeWavePlayer::SetSoundWave(USoundWave* InSoundWave)
{
	SoundWave = InSoundWave;
	SoundWaveAssetPtr = InSoundWave;
}

#if WITH_EDITOR
void USoundNodeWavePlayer::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(USoundNodeWavePlayer, SoundWaveAssetPtr))
	{
		LoadAsset();
	}
}
#endif

void USoundNodeWavePlayer::ParseNodes( FAudioDevice* AudioDevice, const UPTRINT NodeWaveInstanceHash, FActiveSound& ActiveSound, const FSoundParseParameters& ParseParams, TArray<FWaveInstance*>& WaveInstances )
{
	if (bAsyncLoading)
	{
		UE_LOG(LogAudio, Verbose, TEXT("Asynchronous load of %s not complete in USoundNodeWavePlayer::ParseNodes, will attempt to play later."), *GetFullNameSafe(this));
		// We're still loading so don't stop this active sound yet
		ActiveSound.bFinished = false;
		return;
	}

	if (SoundWave)
	{
		// The SoundWave's bLooping is only for if it is directly referenced, so clear it
		// in the case that it is being played from a player
		bool bWaveIsLooping = SoundWave->bLooping;
		SoundWave->bLooping = false;

		if (bLooping)
		{
			FSoundParseParameters UpdatedParams = ParseParams;
			UpdatedParams.bLooping = true;
			SoundWave->Parse(AudioDevice, NodeWaveInstanceHash, ActiveSound, UpdatedParams, WaveInstances);
		}
		else
		{
			SoundWave->Parse(AudioDevice, NodeWaveInstanceHash, ActiveSound, ParseParams, WaveInstances);
		}

		SoundWave->bLooping = bWaveIsLooping;
	}
}

float USoundNodeWavePlayer::GetDuration()
{
	float Duration = 0.f;
	if (SoundWave)
	{
		if (bLooping)
		{
			Duration = INDEFINITELY_LOOPING_DURATION;
		}
		else
		{
			Duration = SoundWave->Duration;
		}
	}
	return Duration;
}

#if WITH_EDITOR
FText USoundNodeWavePlayer::GetTitle() const
{
	FText SoundWaveName;
	if (SoundWave)
	{
		SoundWaveName = FText::FromString(SoundWave->GetFName().ToString());
	}
	else
	{
		SoundWaveName = LOCTEXT("NoSoundWave", "NONE");
	}

	FText Title;

	FFormatNamedArguments Arguments;
	Arguments.Add(TEXT("Description"), Super::GetTitle());
	Arguments.Add(TEXT("SoundWaveName"), SoundWaveName);
	if (bLooping)
	{
		Title = FText::Format(LOCTEXT("LoopingSoundWaveDescription", "Looping {Description} : {SoundWaveName}"), Arguments);
	}
	else
	{
		Title = FText::Format(LOCTEXT("NonLoopingSoundWaveDescription", "{Description} : {SoundWaveName}"), Arguments);
	}

	return Title;
}
#endif

// A Wave Player is the end of the chain and has no children
int32 USoundNodeWavePlayer::GetMaxChildNodes() const
{
	return 0;
}


#undef LOCTEXT_NAMESPACE
