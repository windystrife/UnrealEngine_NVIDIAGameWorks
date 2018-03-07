// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"
#include "UnrealAudioTypes.h"
#include "UnrealAudioVoiceInternal.h"

#if ENABLE_UNREAL_AUDIO

namespace UAudio
{
	/**
	 * A struct containing a index (into voice entity array) and a computed volume product.
	 */

	struct FSortedVoiceEntry
	{
		/**
		 * Constructor.
		 *
		 * @param	Index   	Zero-based index of the.
		 * @param	InVolume	The in volume.
		 */

		explicit FSortedVoiceEntry(uint32 InIndex, float InPriorityWeightedVolume)
			: Index(InIndex)
			, PriorityWeightedVolume(InPriorityWeightedVolume)
		{}

		/** Zero-based index of the object. */
		uint32 Index;

		/** The priority-weighted volume. */
		float PriorityWeightedVolume;
	};

	struct FVolumeInitParam
	{
		FEmitterHandle EmitterHandle;
		float BaseVolume;
		float VolumeScale;
		float VolumeScaleDeltaTime;
		float VolumeProduct;
		float VolumeAttenuation;
		float PriorityWeight;
	};

	/** Manager for volumes. */
	class FVolumeManager
	{
	public:
		FVolumeManager(FUnrealAudioModule* InAudioModule);

		void Init(uint32 NumElements);
		void Update();
		
		void InitializeEntry(uint32 VoiceDataIndex, const FVolumeInitParam& Params);
		void ReleaseEntry(uint32 VoiceDataIndex);

		void SetAttenuation(uint32 VoiceDataIndex, float Attenuation);
		void SetDynamicVolumeScale(uint32 VoiceDataIndex, float VolumeScale, float DeltaTimeSec);
		void SetFadeOut(uint32 VoiceDataIndex, float FadeTimeSec);

		bool IsFadedOut(uint32 VoiceDataIndex) const;
		float GetVolumeScale(uint32 VoiceDataIndex) const;
		float GetVolumeFade(uint32 VoiceDataIndex) const;
		float GetVolumeProduct(uint32 VoiceDataIndex) const;
		float GetVolumeAttenuation(uint32 VoiceDataIndex) const;
		const TArray<FSortedVoiceEntry>& GetSortedVoices() const;

	private:

		/** The audio module. */
		FUnrealAudioModule* AudioModule;

		/** Number of entries. */
		int32 EntryCount;

		/** The last time the object was updated. */
		double LastTimeSec;

		/** The baseline volume. */
		TArray<float> Baseline;

		/** The attenuation. */
		TArray<float> Attenuation;

		/** The volume product. */
		TArray<float> VolumeProduct;

		/** The volume scale. */
		FDynamicParamData DynamicScale;

		/** The volume fade scale. */
		FDynamicParamData FadeScale;

		/** The priority weight of the voice, used to sort voices. */
		TArray<float> PriorityWeight;

		TArray<float> VolumeWeightedPriority;

		TArray<FEmitterHandle> EmitterHandle;

		/** A volume compare predicate. */
		struct VoiceComparePredicate
		{
			bool operator()(const FSortedVoiceEntry& A, const FSortedVoiceEntry& B) const
			{
				return A.PriorityWeightedVolume > B.PriorityWeightedVolume;
			}
		};

		/** Array of sorted indices by volume. */
		TArray<FSortedVoiceEntry> SortedVoices;
	};


}

#endif // #if ENABLE_UNREAL_AUDIO
