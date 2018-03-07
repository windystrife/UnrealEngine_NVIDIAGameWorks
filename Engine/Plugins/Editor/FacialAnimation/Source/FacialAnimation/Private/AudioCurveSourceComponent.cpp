// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AudioCurveSourceComponent.h"
#include "Sound/SoundWave.h"
#include "Engine/CurveTable.h"

UAudioCurveSourceComponent::UAudioCurveSourceComponent()
{
	OnAudioPlaybackPercentNative.AddUObject(this, &UAudioCurveSourceComponent::HandlePlaybackPercent);

	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;

	static FName DefaultSourceBindingName(TEXT("Default"));
	CurveSourceBindingName = DefaultSourceBindingName;

	CachedCurveEvalTime = 0.0f;
	CachedSyncPreRoll = 0.0f;
	CachedStartTime = 0.0f;
	CachedFadeInDuration = 0.0f;
	CachedFadeVolumeLevel = 1.0f;
	CachedDuration = 0.0f;
	bCachedLooping = false;

#if WITH_EDITORONLY_DATA
	bVisualizeComponent = false;
#endif
}

void UAudioCurveSourceComponent::CacheCurveData()
{
	CachedCurveTable = nullptr;
	CachedCurveEvalTime = 0.0f;
	CachedSyncPreRoll = 0.0f;
	CachedDuration = 0.0f;
	bCachedLooping = false;

	// we only support preroll with sound waves (and derived classes) as these are the
	// only types where we can properly determine correct sound waves up-front (sound 
	// cues etc. can be randomized etc.)
	if (USoundWave* SoundWave = Cast<USoundWave>(Sound))
	{
		if (SoundWave->Curves != nullptr)
		{
			CachedCurveTable = SoundWave->Curves;

			// cache audio sync curve
			static const FName AudioSyncCurve(TEXT("Audio"));
			FRichCurve** RichCurvePtr = SoundWave->Curves->RowMap.Find(AudioSyncCurve);
			if (RichCurvePtr != nullptr && *RichCurvePtr != nullptr)
			{
				CachedSyncPreRoll = (*RichCurvePtr)->GetFirstKey().Time;
			}
		}

		CachedDuration = SoundWave->Duration;
		bCachedLooping = SoundWave->IsLooping();
	}
}

void UAudioCurveSourceComponent::FadeIn(float FadeInDuration, float FadeVolumeLevel, float StartTime)
{
	CacheCurveData();

	if (CachedSyncPreRoll <= 0.0f)
	{
		PlayInternal(StartTime, FadeInDuration, FadeVolumeLevel);
	}
	else
	{
		CachedFadeInDuration = FadeInDuration;
		CachedFadeVolumeLevel = FadeVolumeLevel;
		CachedStartTime = StartTime;
		Delay = 0.0f;
	}
}

void UAudioCurveSourceComponent::FadeOut(float FadeOutDuration, float FadeVolumeLevel)
{
	if (Delay < CachedSyncPreRoll)
	{
		CachedCurveTable = nullptr;
		CachedCurveEvalTime = 0.0f;
		CachedSyncPreRoll = 0.0f;
		Delay = 0.0f;
	}
	else
	{
		Super::FadeOut(FadeOutDuration, FadeVolumeLevel);
	}
}

void UAudioCurveSourceComponent::Play(float StartTime)
{
	CacheCurveData();

	if (CachedSyncPreRoll <= 0.0f)
	{
		PlayInternal(StartTime);
	}
	else
	{
		CachedFadeInDuration = 0.0f;
		CachedFadeVolumeLevel = 1.0f;
		CachedStartTime = 0.0f;
		Delay = 0.0f;
	}
}

void UAudioCurveSourceComponent::Stop()
{
	if (Delay < CachedSyncPreRoll)
	{
		CachedCurveTable = nullptr;
		CachedCurveEvalTime = 0.0f;
		CachedSyncPreRoll = 0.0f;
		Delay = 0.0f;
	}
	else
	{
		Super::Stop();
	}
}

bool UAudioCurveSourceComponent::IsPlaying() const
{
	return Delay < CachedSyncPreRoll || Super::IsPlaying();
}

void UAudioCurveSourceComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	const float OldDelay = Delay;
	Delay = FMath::Min(Delay + DeltaTime, CachedSyncPreRoll);
	if (OldDelay < CachedSyncPreRoll && Delay >= CachedSyncPreRoll)
	{
		PlayInternal(CachedStartTime, CachedFadeInDuration, CachedFadeVolumeLevel);
	}
	else if(Delay < CachedSyncPreRoll)
	{
		CachedCurveEvalTime = Delay;
	}
}

void UAudioCurveSourceComponent::HandlePlaybackPercent(const UAudioComponent* InComponent, const USoundWave* InSoundWave, const float InPlaybackPercentage)
{
	CachedCurveTable = InSoundWave->Curves;
	CachedDuration = InSoundWave->Duration;
	CachedCurveEvalTime = CurveSyncOffset + Delay + (InPlaybackPercentage * InSoundWave->Duration);
	bCachedLooping = const_cast<USoundWave*>(InSoundWave)->IsLooping();
}

FName UAudioCurveSourceComponent::GetBindingName_Implementation() const
{
	return CurveSourceBindingName;
}

float UAudioCurveSourceComponent::GetCurveValue_Implementation(FName CurveName) const
{
	if (IsPlaying())
	{
		UCurveTable* CurveTable = CachedCurveTable.Get();
		if (CurveTable)
		{
			FRichCurve** Curve = CurveTable->RowMap.Find(CurveName);
			if (Curve != nullptr && *Curve != nullptr)
			{
				return (*Curve)->Eval(CachedCurveEvalTime);
			}
		}
	}

	return 0.0f;
}

void UAudioCurveSourceComponent::GetCurves_Implementation(TArray<FNamedCurveValue>& OutCurve) const
{
	if (IsPlaying())
	{
		UCurveTable* CurveTable = CachedCurveTable.Get();
		if (CurveTable)
		{
			OutCurve.Reset(CurveTable->RowMap.Num());

			if (bCachedLooping && CachedSyncPreRoll > 0.0f && Delay >= CachedSyncPreRoll && CachedCurveEvalTime >= CachedDuration - CachedSyncPreRoll)
			{
				// if we are looping and we have some preroll delay, we need to evaluate the curve twice
				// as we need to incorporate the preroll in the loop
				for (auto Iter = CurveTable->RowMap.CreateConstIterator(); Iter; ++Iter)
				{
					FRichCurve* Curve = Iter.Value();

					float StandardValue = Curve->Eval(CachedCurveEvalTime);
					float LoopedValue = Curve->Eval(FMath::Fmod(CachedCurveEvalTime, CachedDuration));

					OutCurve.Add({ Iter.Key(), FMath::Max(StandardValue, LoopedValue) });
				}
			}
			else
			{
				for (auto Iter = CurveTable->RowMap.CreateConstIterator(); Iter; ++Iter)
				{
					OutCurve.Add({ Iter.Key(), Iter.Value()->Eval(CachedCurveEvalTime) });
				}
			}
		}
	}
}
