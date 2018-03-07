// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "UnrealAudioPitchManager.h"
#include "UnrealAudioPrivate.h"

#if ENABLE_UNREAL_AUDIO

namespace UAudio
{
	FPitchManager::FPitchManager(FUnrealAudioModule* InAudioModule)
		: AudioModule(InAudioModule)
		, EntryCount(0)
		, LastTimeSeconds(0.0)
	{
		check(AudioModule);
	}

	void FPitchManager::Init(uint32 NumElements)
	{
		Baselines.Init(1.0f, NumElements);
		PitchScaleData.Init(NumElements);
		PitchProducts.Init(1.0f, NumElements);
		DurationSeconds.Init(0.0f, NumElements);
		CurrentTimeSeconds.Init(0.0f, NumElements);
	}

	void FPitchManager::Update()
	{
		double CurrentTimeSec = AudioModule->GetCurrentTimeSec();
		LastTimeSeconds = CurrentTimeSec;

		// If there are no pitches to update, then no need to update anything
		if (EntryCount == 0)
		{
			return;
		}

		float DeltaTime = (float)(CurrentTimeSec - LastTimeSeconds);
		float Fraction = 0.0f;
		float Result = 0.0f;

		int32 TotalCount = Baselines.Num();
		int32 UpdateCount = 0;
		
		// Update all dynamic volume parameters in one parallelized loop.
		// TODO: implement SSE

		for (int32 Index = 0; Index < TotalCount && UpdateCount < EntryCount; ++Index)
		{
			// If the value of baseline is less than 0.0f, it's been a released volume entry
			if (Baselines[Index] < 0.0f)
			{
				continue;
			}

			++UpdateCount;

			float Product = Baselines[Index] * PitchScaleData.Compute(Index, CurrentTimeSec);
			PitchProducts[Index] = Product;

			// increment the current time by the delta time
			// Note: this is not 100% accurate for highly dynamic pitches but is a close approximation and "good enough"
			CurrentTimeSeconds[Index] += (DeltaTime * Product);
		}

		// Make sure we updated everything!
		DEBUG_AUDIO_CHECK(UpdateCount == EntryCount);
	}

	void FPitchManager::InitializeEntry(uint32 Index, const FPitchInitParam& PitchParams)
	{
		++EntryCount;

		DEBUG_AUDIO_CHECK(EntryCount <= Baselines.Num());

		PitchScaleData.InitEntry(Index);
		PitchScaleData.SetValue(Index, PitchParams.PitchScale, LastTimeSeconds, PitchParams.PitchScaleTime);
		Baselines[Index] = PitchParams.BaselinePitch;

		float PitchProduct = PitchParams.BaselinePitch;
		if (PitchParams.PitchScaleTime == 0.0f)
		{
			PitchProduct *= PitchParams.PitchScale;
		}
		PitchProducts[Index] = PitchProduct;
		DurationSeconds[Index] = PitchParams.DurationSeconds;
		CurrentTimeSeconds[Index] = 0.0f;
	}

	void FPitchManager::ReleaseEntry(uint32 Index)
	{
		DEBUG_AUDIO_CHECK(Index < (uint32)Baselines.Num());

		--EntryCount;
		DEBUG_AUDIO_CHECK(EntryCount >= 0);

		// Setting baseline to -1.0f is releasing the volume data at that index
		Baselines[Index] = -1.0f;
	}

	void FPitchManager::SetDynamicPitchScale(uint32 Index, float InPitch, float InDeltaTimeSeconds)
	{
		DEBUG_AUDIO_CHECK(Index < (uint32)Baselines.Num());
		DEBUG_AUDIO_CHECK(Baselines[Index] > 0.0f);
		PitchScaleData.SetValue(Index, InPitch, LastTimeSeconds, InDeltaTimeSeconds);
	}

	float FPitchManager::GetPitchScale(uint32 Index) const
	{
		DEBUG_AUDIO_CHECK(Index < (uint32)Baselines.Num());
		DEBUG_AUDIO_CHECK(Baselines[Index] > 0.0f);
		return PitchScaleData.CurrentValue[Index];
	}

	float FPitchManager::GetPitchProduct(uint32 Index) const
	{
		DEBUG_AUDIO_CHECK(Index < (uint32)Baselines.Num());
		DEBUG_AUDIO_CHECK(Baselines[Index] > 0.0f);
		return PitchProducts[Index];
	}

	float FPitchManager::GetPlayPercentage(uint32 Index) const
	{
		DEBUG_AUDIO_CHECK(Index < (uint32)Baselines.Num());
		DEBUG_AUDIO_CHECK(Baselines[Index] > 0.0f);
		DEBUG_AUDIO_CHECK(DurationSeconds[Index] > 0.0f);
		return CurrentTimeSeconds[Index] / DurationSeconds[Index];
	}

}

#endif // #if ENABLE_UNREAL_AUDIO

