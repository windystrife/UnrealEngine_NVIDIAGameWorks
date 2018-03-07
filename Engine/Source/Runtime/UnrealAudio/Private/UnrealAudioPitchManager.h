// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"
#include "UnrealAudioTypes.h"
#include "UnrealAudioVoiceInternal.h"

#if ENABLE_UNREAL_AUDIO

namespace UAudio
{
	struct FPitchInitParam
	{
		float BaselinePitch;
		float PitchScale;
		float PitchScaleTime;
		float DurationSeconds;
	};

	/** Manager for pitch processing. */
	class FPitchManager
	{
	public:

		/**
		 * Constructor.
		 *
		 * @param [int]	InAudioModule	The parent audio module.
		 */

		FPitchManager(FUnrealAudioModule* InAudioModule);

		/**
		 * Initializes this object.
		 *
		 * @param	ReserveCount	Number of objects entries to reserve.
		 */

		void Init(uint32 ReserveCount);

		/** Updates this object. */
		void Update();

		/**
		 * Initializes the entry.
		 *
		 * @param	Index   	Zero-based index of the.
		 * @param	Baseline	The baseline.
		 */

		void InitializeEntry(uint32 Index, const FPitchInitParam& PitchParams);

		/**
		 * Releases the entry described by Index.
		 *
		 * @param	Index	Zero-based index of the.
		 *
		 */

		void ReleaseEntry(uint32 Index);

		/**
		 * Sets dynamic pitch.
		 *
		 * @param	EntityIndex			Zero-based index of the entity.
		 * @param	PitchScale			The pitch scale.
		 * @param	DeltaTimeSeconds	The delta time in seconds.
		 */

		void SetDynamicPitchScale(uint32 Index, float PitchScale, float DeltaTimeSeconds);

		/**
		 * Gets pitch product.
		 *
		 * @param	EntityIndex	Zero-based index of the entity.
		 *
		 * @return	The pitch product.
		 */

		float GetPitchScale(uint32 Index) const;

		float GetPitchProduct(uint32 Index) const;

		float GetPlayPercentage(uint32 Index) const;

	private:

		/** The audio module. */
		FUnrealAudioModule* AudioModule;
		int32 EntryCount;
		double LastTimeSeconds;
		TArray<float> Baselines;
		FDynamicParamData PitchScaleData;
		TArray<float> PitchProducts;
		TArray<float> CurrentTimeSeconds;
		TArray<float> DurationSeconds;
	};


}

#endif // #if ENABLE_UNREAL_AUDIO
