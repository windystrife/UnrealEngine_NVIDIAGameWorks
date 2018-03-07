// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "UnrealAudioVolumeManager.h"
#include "UnrealAudioPrivate.h"

#if ENABLE_UNREAL_AUDIO

namespace UAudio
{
	FVolumeManager::FVolumeManager(FUnrealAudioModule* InAudioModule)
		: AudioModule(InAudioModule)
		, EntryCount(0)
		, LastTimeSec(0.0)
	{
		check(AudioModule);
	}

	void FVolumeManager::Init(uint32 NumElements)
	{
		Baseline.Init(-1.0f, NumElements);
		Attenuation.Init(1.0f, NumElements);
		VolumeProduct.Init(1.0f, NumElements);
		FadeScale.Init(NumElements);
		DynamicScale.Init(NumElements);
		PriorityWeight.Init(1.0f, NumElements);
		VolumeWeightedPriority.Init(1.0f, NumElements);
		EmitterHandle.Init(FEmitterHandle(), NumElements);
	}

	void FVolumeManager::Update()
	{
		double CurrentTimeSec = AudioModule->GetCurrentTimeSec();
		LastTimeSec = CurrentTimeSec;

		// If there are no volumes to update, then no need to update anything
		if (EntryCount == 0)
		{
			return;
		}

		int32 UpdateCount = 0;
		float DeltaTime = 0.0f;
		float Fraction = 0.0f;
		float Result = 0.0f;
		int32 TotalCount = Baseline.Num();

		// Update all dynamic volume parameters in one parallelized loop.
		// TODO: implement SSE

		for (int32 Index = 0; Index < TotalCount && UpdateCount < EntryCount; ++Index)
		{
			// If the value of baseline is less than 0.0f, it's been a released volume entry
			if (Baseline[Index] < 0.0f)
			{
				continue;
			}

			++UpdateCount;
			float Product =	Baseline[Index] * 
							Attenuation[Index] * 
							DynamicScale.Compute(Index, CurrentTimeSec) * 
							FadeScale.Compute(Index, CurrentTimeSec);

			DEBUG_AUDIO_CHECK(Product >= 0.0f && Product <= 1.0f);
			VolumeProduct[Index] = Product;
		}

		// Reset the volume priority sorter
		SortedVoices.Reset();
		UpdateCount = 0;
		VoiceComparePredicate Predicate;
		for (int32 Index = 0; Index < TotalCount && UpdateCount < EntryCount; ++Index)
		{
			if (Baseline[Index] < 0.0f)
			{
				continue;
			}
			++UpdateCount;
			float Temp = VolumeProduct[Index] * PriorityWeight[Index];
			VolumeWeightedPriority[Index] = Temp;
			SortedVoices.HeapPush(FSortedVoiceEntry(Index, Temp), Predicate);
		}

		// Make sure we updated everything
		DEBUG_AUDIO_CHECK(UpdateCount == EntryCount);
	}

	void FVolumeManager::InitializeEntry(uint32 VoiceDataIndex, const FVolumeInitParam& Params)
	{
		++EntryCount;
		DEBUG_AUDIO_CHECK(EntryCount< Baseline.Num());
		DEBUG_AUDIO_CHECK(VoiceDataIndex < (uint32)Baseline.Num());
		DEBUG_AUDIO_CHECK(Params.VolumeProduct >= 0.0f && Params.VolumeProduct <= 1.0f);

		Baseline[VoiceDataIndex] = Params.BaseVolume;
		Attenuation[VoiceDataIndex] = Params.VolumeAttenuation;
		VolumeProduct[VoiceDataIndex] = Params.VolumeProduct;
		EmitterHandle[VoiceDataIndex] = Params.EmitterHandle;
		PriorityWeight[VoiceDataIndex] = Params.PriorityWeight;

		DynamicScale.InitEntry(VoiceDataIndex);
		DynamicScale.SetValue(VoiceDataIndex, Params.VolumeScale, LastTimeSec, Params.VolumeScaleDeltaTime);

		FadeScale.InitEntry(VoiceDataIndex);
	}

	void FVolumeManager::ReleaseEntry(uint32 VoiceDataIndex)
	{
		DEBUG_AUDIO_CHECK(VoiceDataIndex < (uint32)Baseline.Num());

		--EntryCount;
		DEBUG_AUDIO_CHECK(EntryCount >= 0);

		// Setting baseline to -1.0f is releasing the volume data at that index
		Baseline[VoiceDataIndex] = -1.0f;
	}

	void FVolumeManager::SetAttenuation(uint32 VoiceDataIndex, float InAttenuation)
	{
		DEBUG_AUDIO_CHECK(VoiceDataIndex < (uint32)Baseline.Num());
		DEBUG_AUDIO_CHECK(Baseline[VoiceDataIndex] > 0.0f);
		Attenuation[VoiceDataIndex] = InAttenuation;
	}

	void FVolumeManager::SetDynamicVolumeScale(uint32 VoiceDataIndex, float InVolume, float DeltaTimeSec)
	{
		DEBUG_AUDIO_CHECK(VoiceDataIndex < (uint32)Baseline.Num());
		DEBUG_AUDIO_CHECK(Baseline[VoiceDataIndex] > 0.0f);
		DEBUG_AUDIO_CHECK(InVolume >= 0.0f && InVolume <= 1.0f);
		DynamicScale.SetValue(VoiceDataIndex, InVolume, LastTimeSec, DeltaTimeSec);
	}

	void FVolumeManager::SetFadeOut(uint32 VoiceDataIndex, float FadeTimeSec)
	{
		DEBUG_AUDIO_CHECK(VoiceDataIndex < (uint32)Baseline.Num());
		DEBUG_AUDIO_CHECK(Baseline[VoiceDataIndex] > 0.0f);
		FadeScale.SetValue(VoiceDataIndex, 0.0f, LastTimeSec, FadeTimeSec);
	}

	bool FVolumeManager::IsFadedOut(uint32 VoiceDataIndex) const
	{
		DEBUG_AUDIO_CHECK(VoiceDataIndex < (uint32)Baseline.Num());
		DEBUG_AUDIO_CHECK(Baseline[VoiceDataIndex] > 0.0f);
		return FadeScale.EndValue[VoiceDataIndex] == 0.0f && FadeScale.bIsDone[VoiceDataIndex];
	}

	float FVolumeManager::GetVolumeScale(uint32 VoiceDataIndex) const
	{
		DEBUG_AUDIO_CHECK(VoiceDataIndex < (uint32)Baseline.Num());
		DEBUG_AUDIO_CHECK(Baseline[VoiceDataIndex] > 0.0f);

		return DynamicScale.CurrentValue[VoiceDataIndex];
	}

	float FVolumeManager::GetVolumeFade(uint32 VoiceDataIndex) const
	{
		DEBUG_AUDIO_CHECK(VoiceDataIndex < (uint32)Baseline.Num());
		DEBUG_AUDIO_CHECK(Baseline[VoiceDataIndex] > 0.0f);

		return FadeScale.CurrentValue[VoiceDataIndex];
	}

	float FVolumeManager::GetVolumeProduct(uint32 VoiceDataIndex) const
	{
		DEBUG_AUDIO_CHECK(VoiceDataIndex < (uint32)Baseline.Num());
		DEBUG_AUDIO_CHECK(Baseline[VoiceDataIndex] > 0.0f);

		return VolumeProduct[VoiceDataIndex];
	}

	float FVolumeManager::GetVolumeAttenuation(uint32 VoiceDataIndex) const
	{
		DEBUG_AUDIO_CHECK(VoiceDataIndex < (uint32)Baseline.Num());
		DEBUG_AUDIO_CHECK(Baseline[VoiceDataIndex] > 0.0f);

		return Attenuation[VoiceDataIndex];
	}

	const TArray<FSortedVoiceEntry>& FVolumeManager::GetSortedVoices() const
	{
		return SortedVoices;
	}

}

#endif // #if ENABLE_UNREAL_AUDIO

