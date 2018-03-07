// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DSP/Dsp.h"
#include "DSP/Osc.h"
#include "DSP/SampleBuffer.h"
#include "DSP/SampleBufferReader.h"
#include "DSP/Envelope.h"
#include "DSP/Amp.h"
#include "DSP/DynamicsProcesser.h"
#include "AudioDevice.h"

namespace Audio
{
	enum class EGranularSynthMode : uint8
	{
		Synthesis,
		Granulation,

		Count,
	};

	enum class EGrainEnvelopeType
	{
		Rectangular,
		Triangle,
		DownwardTriangle,
		UpwardTriangle,
		ExponentialDecay,
		ExponentialIncrease,
		Gaussian,
		Hanning,
		Lanczos,
		Cosine,
		CosineSquared,
		Welch,
		Blackman,
		BlackmanHarris,

		Count
	};

	// Simple class that generates an envelope and lets you retrieve interpolated values at any given fraction
	class FGrainEnvelope
	{
	public:
		FGrainEnvelope();
		~FGrainEnvelope();

		void GenerateEnvelope(const EGrainEnvelopeType EnvelopeType, const int32 NumFrames);
		float GetValue(const float Fraction);

	private:
		EGrainEnvelopeType CurrentType;
		TArray<float> GrainEnvelope;
	};

	struct FGrainData
	{
		// What oscillator type to use
		EOsc::Type OscType;

		// Where in the buffer to seek the grain playback to
		float BufferSeekTime;

		// Duration of the grain
		float DurationSeconds;

		// Grain pitch scale (if in buffer reading mode)
		float PitchScale;

		// The grain frequency (if in synthesis mode)
		float Frequency;

		// Grain volume
		float Volume;

		// Grain pan
		float Pan;
	};

	class FGranularSynth;

	// Class representing a grain of audio
	class FGrain
	{
	public:
		FGrain(const int32 InGrainId, FGranularSynth* InParent);
		~FGrain();

		// Plays the grain with the supplied grain data
		void Play(const FGrainData& InGrainData);

		// Changes the oscillator type for the grain (if the grain is in synth mode)
		void SetOscType(const EOsc::Type InType);

		// Sets the oscillator frequency
		void SetOscFrequency(const float InFrequency);

		// Sets the oscillator frequency modulation
		void SetOscFrequencyModuation(const float InFrequencyModulation);

		// Sets the grain pitch modulation 
		void SetPitchModulation(const float InPitchModulation);

		// Sets the grain modulation
		void SetVolumeModulation(const float InVolumeModulation);

		// Sets the pan modulation angle
		void SetPanModulation(const float InPanModulation);

		// Changes how quickly the grain reads through envelope
		void SetDurationScale(const float InDurationScale);

		// Queries if this grain is finished playing and needs to be reaped
		bool IsDone() const;

		// Generates the next frame from the grain
		bool GenerateFrame(float* OutStereoFrame);

	protected:
		float GetEnvelopeValue();

		// Grain id
		int32 GrainId;

		// Parent synth
		FGranularSynth* Parent;

		// Grain data struct
		FGrainData GrainData;

		// The sample buffer reader to use for the grain of the mode is granulation
		FSampleBufferReader SampleBufferReader;

		// Oscillator to use for synthesis mode
		FOsc Osc;

		// The current pitch
		float CurrentPitch;

		// Current frequency
		float CurrentFrequency;

		// The current volume scale
		float CurrentVolumeScale;

		// The current pan
		float CurrentPan;

		// How quickly we read through the envelope 
		float DurationScale;

		// The current frame count the grain is on
		float CurrentFrameCount;

		// The end frame count the grain will finish on
		float EndFrameCount;

		// Speaker map based on the current grain azimuth
		TArray<float> SpeakerMap;

		// Scratch buffer for sample reading
		TArray<float> FrameScratch;
	};

	// A stereo granulator 
	class AUDIOMIXER_API FGranularSynth
	{
	public:
		FGranularSynth();
		~FGranularSynth();

		void Init(const int32 InSampleRate, const int32 InNumInitialGrains);
		
		// Loads a sound wave to use for granular synth mode
		void LoadSampleBuffer(const FSampleBuffer& InSampleBuffer);

		// Plays a granular synthesis "Note"
		void NoteOn(const uint32 InMidiNote, const float InVelocity, const float InDurationSec = INDEX_NONE);

		// Note off, triggers release envelope
		void NoteOff(const uint32 InMidiNote, const bool bKill);

		// Sets the granular synth attack time
		void SetAttackTime(const float InAttackTimeMSec);

		// Sets the granular synth decay time
		void SetDecayTime(const float InDecayTimeMSec);

		// Sets the granular synth sustain gain
		void SetSustainGain(const float InSustainGain);

		// Sets the granular synth releas etime
		void SetReleaseTime(const float InReleaseTimeMSec);

		// Seeks the loaded buffer used for granulation. Grains will spawn from this location
		void SeekTime(const float InTimeSec, const float LerpTimeSec = 0.0f, const ESeekType::Type InSeekType = ESeekType::FromBeginning);

		// Sets whether or not the buffer playback advances on its own or if it just sits in one place
		void SetScrubMode(const bool bIsScrubMode);

		// Sets how fast the granular play head for granulation is is played (and in what direction)
		void SetPlaybackSpeed(const float InPlaybackSpeed);

		// The rate at which new grains are attempted to be spawned
		void SetGrainsPerSecond(const float InNumberOfGrainsPerSecond);

		// The probability at which a grain will occur when a grain tries to spawn. Allows for sporatic grain generation.
		void SetGrainProbability(const float InGrainProbability);

		// Sets the envelope type to use for new grains. Will instantly switch all grains to this envelope type so may cause discontinuities if switched while playing.
		void SetGrainEnvelopeType(const EGrainEnvelopeType InGrainEnvelopeType);

		// Sets the grain oscillator type (for use with granular synthesis mode)
		void SetGrainOscType(const EOsc::Type InGrainOscType);

		// Sets the base grain volume and randomization range
		void SetGrainVolume(const float InBaseVolume, const FVector2D InVolumeRange = FVector2D::ZeroVector);

		// Sets the grain modulation -- allows modulating actively playing grain volumes
		void SetGrainVolumeModulation(const float InVolumeModulation);

		// Sets the base grain pitch and randomization range
		void SetGrainPitch(const float InBasePitch, const FVector2D InPitchRange = FVector2D::ZeroVector);

		// Sets the grain frequency
		void SetGrainFrequency(const float InFrequency, const FVector2D InPitchRange = FVector2D::ZeroVector);

		// Sets the grain frequency modulation
		void SetGrainFrequencyModulation(const float InFrequencyModulation);

		// Sets the grain pitch modulation -- allows modulating actively playing grain pitches
		void SetGrainPitchModulation(const float InPitchModulation);

		// Sets the grain azimuth (pan) and randomization range
		void SetGrainPan(const float InBasePan, const FVector2D InPanRange = FVector2D::ZeroVector);

		// Sets the grain azimuth modulation - allows modulating actively playing grain azimuths
		void SetGrainPanModulation(const float InPanModulation);

		// Sets the grain duration. 
		void SetGrainDuration(const float InBaseDuration, const FVector2D InDurationRange = FVector2D::ZeroVector);

		// Sets the grain duration modulation.
		void SetGrainDurationScale(const float InDurationScale);

		// Return the number of currently active grains
		int32 GetNumActiveGrains() const;

		// Get current playback time (in granular mode)
		float GetCurrentPlayheadTime() const;

		// Returns the duration of the internal loaded sample buffer
		float GetSampleDuration() const;

		// Generate the next audio buffer
		void Generate(float* OutAudiobuffer, const int32 NumFrames);

	protected:
		// Spawns grains
		void SpawnGrain();

		// Return wrapped playhead position
		float GetWrappedPlayheadPosition(float PlayheadFrame);


		// Current grain azimuth modulation
		struct FGrainParam
		{
			float Modulation;
			float Base;
			FVector2D Range;

			float GetValue() const
			{
				return Base + FMath::FRandRange(Range.X, Range.Y);
			}

			float GetModulation() const
			{
				return Modulation;
			}

			FGrainParam()
				: Modulation(0.0f)
				, Base(0.0f)
			{}
		};

		int32 SampleRate;
		int32 NumChannels;

		// The single envelope function used by all grains
		FGrainEnvelope GrainEnvelope;

		// What mode the granular synthesizer is in
		EGranularSynthMode Mode;

		// The oscillator type to use if in synthesis mode
		EOsc::Type GrainOscType;

		// Current grain envelope type
		EGrainEnvelopeType GrainEnvelopeType;

		// A pool of free grains. Will dynamically grow to needed grains based on grain density.
		TArray<FGrain> GrainPool;
		TArray<int32> FreeGrains;
		TArray<int32> ActiveGrains;
		TArray<int32> DeadGrains;

		// The rate at which grains are spawned
		float GrainsPerSecond;

		// The probability of a grain occurring when it tries to spawn (based off the GrainsPerSecond)
		float GrainProbability;

		// The current number of frames since last attempt to spawn
		int32 CurrentSpawnFrameCount;

		// The next frame when a grain needs to spawn
		int32 NextSpawnFrame;

		// Counts for overall note duration of the granulator
		int32 NoteDurationFrameCount;
		int32 NoteDurationFrameEnd;

		FGrainParam Pan;
		FGrainParam Volume;
		FGrainParam Pitch;
		FGrainParam Frequency;
		FGrainParam Duration;

		// Overall envelope of the granulator
		FEnvelope GainEnv;
		FAmp Amp;
		FDynamicsProcessor DynamicsProcessor;

		// The buffer which holds the sample to be granulated
		FSampleBuffer SampleBuffer;

		// The current playhead frame
		float CurrentPlayHeadFrame;
		float PlaybackSpeed;
		int32 NumActiveGrains;
		bool bScrubMode;
		FLinearEase SeekingPlayheadTimeFrame;

		friend class FGrain;
	};
}

