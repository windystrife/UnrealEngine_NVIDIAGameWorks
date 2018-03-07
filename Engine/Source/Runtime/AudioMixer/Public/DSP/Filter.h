// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ModulationMatrix.h"
#include "BiQuadFilter.h"

namespace Audio
{
	// Enumeration of filter types
	namespace EBiquadFilter
	{
		enum Type
		{
			Lowpass,
			Highpass,
			Bandpass,
			Notch,
			ParametricEQ,
			AllPass,
		};
	}

	// Biquad filter class which wraps a biquad filter struct
	// Handles multi-channel audio to avoid calculating filter coefficients for multiple channels of audio.
	class AUDIOMIXER_API FBiquadFilter
	{
	public:
		// Constructor
		FBiquadFilter();
		// Destructor
		virtual ~FBiquadFilter();

		// Initialize the filter
		void Init(const float InSampleRate, const int32 InNumChannels, const EBiquadFilter::Type InType, const float InCutoffFrequency = 20000.0f, const float InBandwidth = 2.0f, const float InGain = 0.0f);

		// Resets the filter state
		void Reset();

		// Processes a single frame of audio
		void ProcessAudioFrame(const float* InAudio, float* OutAudio);

		// Sets all filter parameters with one function
		void SetParams(const EBiquadFilter::Type InFilterType, const float InCutoffFrequency, const float InBandwidth, const float InGainDB);

		// Sets the type of the filter to use
		void SetType(const EBiquadFilter::Type InType);

		// Sets the filter frequency
		void SetFrequency(const float InCutoffFrequency);

		// Sets the bandwidth (octaves) of the filter
		void SetBandwidth(const float InBandwidth);

		// Sets the gain of the filter in decibels
		void SetGainDB(const float InGainDB);

		// Sets whether or no this filter is enabled (if disabled audio is passed through)
		void SetEnabled(const bool bInEnabled);

	protected:

		// Function computes biquad coefficients based on current filter settings
		void CalculateBiquadCoefficients();

		// What kind of filter to use when computing coefficients
		EBiquadFilter::Type FilterType;

		// Biquad filter objects for each channel
		FBiquad* Biquad;

		// The sample rate of the filter
		float SampleRate;

		// Number of channels in the filter
		int32 NumChannels;

		// Current frequency of the filter
		float Frequency;

		// Current bandwidth/resonance of the filter
		float Bandwidth;

		// The gain of the filter in DB (for filters that use gain)
		float GainDB;

		// Whether or not this filter is enabled (will bypass otherwise)
		bool bEnabled;
	};

	namespace EFilter
	{
		enum Type
		{
			LowPass,
			HighPass,
			BandPass,
			BandStop,

			NumFilterTypes
		};
	}

	static const int32 MaxFilterChannels = 8;

	// Base class for filters usable in synthesis
	class AUDIOMIXER_API IFilter
	{
	public:
		IFilter();
		virtual ~IFilter();

		// Initialize the filter
		virtual void Init(const float InSampleRate, const int32 InNumChannels, const int32 InVoiceId, FModulationMatrix* InModMatrix = nullptr);

		// Sets the cutoff frequency of the filter.
		virtual void SetFrequency(const float InCutoffFrequency);

		// Sets an external modulated frequency
		virtual void SetFrequencyMod(const float InModFrequency);

		// Sets the quality/resonance of the filter
		virtual void SetQ(const float InQ);

		// Sets an external modulated quality/resonance of the filter
		virtual void SetQMod(const float InModQ);

		// Sets the filter saturation (not used on all filters)
		virtual void SetSaturation(const float InSaturation) {}

		// Sets the band stop control param (not used on all filters)
		virtual void SetBandStopControl(const float InBandStop) {}

		// Sets the band pass gain compensation (not used on all filters)
		virtual void SetPassBandGainCompensation(const float InPassBandGainCompensation) {}

		// Sets the filter type
		virtual void SetFilterType(const EFilter::Type InFilterType);

		// Reset the filter
		virtual void Reset() {}

		// Updates the filter
		virtual void Update();

		// Processes a single frame of audio. Number of channels MUST be what was used during filter initialization.
		virtual void ProcessAudio(const float* InSamples, float* OutSamples) = 0;

		// Filter patch destinations
		FPatchDestination GetModDestCutoffFrequency() const { return ModCutoffFrequencyDest; }
		FPatchDestination GetModDestQ() const { return ModQDest; }

	protected:

		FORCEINLINE float GetGCoefficient() const
		{
			const float OmegaDigital = 2.0f * PI * Frequency;
			const float OmegaAnalog = 2.0f * SampleRate * Audio::FastTan(0.5f * OmegaDigital / SampleRate);
			const float G = 0.5f * OmegaAnalog / SampleRate;
			return G;
		}

		// The voice id of the owner of the filter (for use with mod matrix)
		int32 VoiceId;

		// Sample rate of the filter
		float SampleRate;

		// Number of channels of the sound
		int32 NumChannels;

		// The cutoff frequency currently used by the filter (can be modulated)
		float Frequency;

		// The base filter cutoff frequency
		float BaseFrequency;

		// A modulated frequency
		float ModFrequency;

		// A modulatied frequency driven externally
		float ExternalModFrequency;

		// The current Q
		float Q;

		// The modulated Q
		float ModQ;

		// The base 
		float BaseQ;

		// An external modulation of Q
		float ExternalModQ;

		// The filter type of the filter
		EFilter::Type FilterType;

		FModulationMatrix* ModMatrix;

		// Mod matrix patchable destinations
		FPatchDestination ModCutoffFrequencyDest;
		FPatchDestination ModQDest;

		bool bChanged;
	};

	// A virtual analog one-pole filter.
	// Defaults to a low-pass mode, but can be switched to high-pass
	class AUDIOMIXER_API FOnePoleFilter : public IFilter
	{
	public:
		FOnePoleFilter();
		virtual ~FOnePoleFilter();

		virtual void Init(const float InSampleRate, const int32 InNumChannels, const int32 InVoiceId = 0, FModulationMatrix* InModMatrix = nullptr) override;
		virtual void Reset() override;
		virtual void Update() override;
		virtual void ProcessAudio(const float* InSamples, float* OutSamples) override;

		void SetCoefficient(const float InCoefficient) { A0 = InCoefficient; }
		float GetCoefficient() const { return A0; }

		float GetState(const int32 InChannel) const { return Z1[InChannel]; }

	protected:
		float A0;
		float* Z1;
	};

	class AUDIOMIXER_API FStateVariableFilter : public IFilter
	{
	public:
		FStateVariableFilter();
		virtual ~FStateVariableFilter();

		virtual void Init(const float InSampleRate, const int32 InNumChannels, const int32 InVoiceId = 0, FModulationMatrix* InModMatrix = nullptr) override;
		virtual void SetBandStopControl(const float InBandStop) override;
		virtual void Reset() override;
		virtual void Update() override;
		virtual void ProcessAudio(const float* InSamples, float* OutSamples) override;

	protected:
		float InputScale;
		float A0;
		float Feedback;
		float BandStopParam;

		struct FFilterState
		{
			float Z1_1;
			float Z1_2;

			FFilterState()
				: Z1_1(0.0f)
				, Z1_2(0.0f)
			{}
		};

		TArray<FFilterState> FilterState;
	};

	class AUDIOMIXER_API FLadderFilter : public IFilter
	{
	public:
		FLadderFilter();
		virtual ~FLadderFilter();

		virtual void Init(const float InSampleRate, const int32 InNumChannels, const int32 InVoiceId = 0, FModulationMatrix* InModMatrix = nullptr) override;
		virtual void Reset() override;
		virtual void Update() override;
		virtual void SetQ(const float InQ) override;
		virtual void SetPassBandGainCompensation(const float InPassBandGainCompensation) override;
		virtual void ProcessAudio(const float* InSamples, float* OutSamples) override;

	protected:

		// Ladder filter is made of 4 LPF filters
		FOnePoleFilter OnePoleFilters[4];
		float Beta[4];
		float K;
		float Gamma;
		float Alpha;
		float Factors[5];
		float PassBandGainCompensation;
	};

}
