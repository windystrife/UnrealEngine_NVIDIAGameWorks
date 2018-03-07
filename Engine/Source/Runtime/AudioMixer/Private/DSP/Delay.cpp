// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "DSP/Delay.h"
#include "DSP/Dsp.h"
#include "AudioMixer.h"

namespace Audio
{
	FDelay::FDelay()
		: AudioBuffer(nullptr)
		, AudioBufferSize(0)
		, ReadIndex(0)
		, WriteIndex(0)
		, SampleRate(0)
		, DelayInSamples(0.0f)
		, EaseDelayMsec(0.0f, 0.0001f)
		, OutputAttenuation(1.0f)
		, OutputAttenuationDB(0.0f)
	{
		Reset();
	}

	FDelay::~FDelay()
	{
		if (AudioBuffer)
		{
			delete[] AudioBuffer;
			AudioBuffer = nullptr;
		}
	}

	void FDelay::Init(const float InSampleRate, const float InBufferLengthSec)
	{
		SampleRate = InSampleRate;
		AudioBufferSize = (int32) (InBufferLengthSec * (float)InSampleRate) + 1;

		if (AudioBuffer)
		{
			delete[] AudioBuffer;
		}

		AudioBuffer = new float[AudioBufferSize];

		Reset();
	}

	void FDelay::Reset()
	{
		if (AudioBuffer)
		{
			FMemory::Memzero(AudioBuffer, sizeof(float)*AudioBufferSize);
		}

		WriteIndex = 0;
		ReadIndex = 0;

		Update(true);
	}
	 
	void FDelay::SetDelayMsec(const float InDelayMsec)
	{
		// Directly set the delay
		DelayInSamples = InDelayMsec * SampleRate * 0.001f;
		Update(true);
	}

	void FDelay::SetDelaySamples(const float InDelaySamples)
	{
		DelayInSamples = InDelaySamples;
		Update(true);
	}

	void FDelay::SetEasedDelayMsec(const float InDelayMsec, const bool bIsInit)
	{
		EaseDelayMsec.SetValue(InDelayMsec, bIsInit);
		if (bIsInit)
		{
			DelayInSamples = InDelayMsec * SampleRate * 0.001f;
		}
		Update(bIsInit);
	}

	void FDelay::SetOutputAttenuationDB(const float InDelayAttenDB)
	{
		OutputAttenuationDB = InDelayAttenDB;

		// Compute linear output attenuation based on DB attenuation settings
		OutputAttenuation = FMath::Pow(10.0f, OutputAttenuationDB / 20.0f);
	}

	float FDelay::Read() const
	{
		// Read the output of the delay at ReadIndex
		const float Yn = AudioBuffer[ReadIndex];

		// Read the location ONE BEHIND yn at y(n-1)
		int32 ReadIndexPrev = ReadIndex - 1;
		if (ReadIndexPrev < 0)
		{
			ReadIndexPrev = AudioBufferSize - 1;
		}

		// Set y(n-1)
		const float YnPrev = AudioBuffer[ReadIndexPrev];

		// Get the amount of fractional delay between previous and next read indices
		const float Fraction = DelayInSamples - (int32)DelayInSamples;

		return FMath::Lerp(Yn, YnPrev, Fraction);
	}

	float FDelay::ReadDelayAt(const float InReadMsec) const
	{
		const float ReadAtDelayInSamples = InReadMsec*((float)SampleRate) / 1000.0f;

		// Subtract to make read index
		int32 ReadAtReadIndex = WriteIndex - (int32)ReadAtDelayInSamples;

		if (ReadAtReadIndex < 0)
		{
			ReadAtReadIndex += AudioBufferSize;	// amount of wrap is Read + Length
		}

		// Read the output of the delay at ReadAtReadIndexs
		float Yn = AudioBuffer[ReadAtReadIndex];

		// Read the location ONE BEHIND yn at y(n-1)
		int32 ReadAtReadIndexPrev = ReadAtReadIndex - 1;
		if (ReadAtReadIndexPrev < 0)
		{
			ReadAtReadIndexPrev = AudioBufferSize - 1;
		}

		// get y(n-1)
		const float YnPrev = AudioBuffer[ReadAtReadIndexPrev];

		// interpolate: (0, yn) and (1, yn_1) by the amount fracDelay
		float Fraction = ReadAtDelayInSamples - (int32)ReadAtDelayInSamples;

		return FMath::Lerp(Yn, YnPrev, Fraction);
	}

	void FDelay::WriteDelayAndInc(const float InDelayInput)
	{
		// write to the delay line
		AudioBuffer[WriteIndex] = InDelayInput; // external feedback sample
												// increment the pointers and wrap if necessary
		WriteIndex++;
		if (WriteIndex >= AudioBufferSize)
		{
			WriteIndex = 0;
		}

		ReadIndex++;
		if (ReadIndex >= AudioBufferSize)
		{
			ReadIndex = 0;
		}
	}

	void FDelay::ProcessAudio(const float* InAudio, float* OutAudio)
	{
		Update();

		const float Xn = *InAudio;
		const float Yn = DelayInSamples == 0 ? Xn : Read();
		WriteDelayAndInc(Xn);
		*OutAudio = OutputAttenuation*Yn;
	}
	 
	void FDelay::Update(bool bForce)
	{
		if (!EaseDelayMsec.IsDone() || bForce)
		{
			// Compute the delay in samples based on msec delay line
			// If we're easing, then get the delay based on the current value of the ease
			if (!EaseDelayMsec.IsDone())
			{
				DelayInSamples = EaseDelayMsec.GetValue() * SampleRate * 0.001f;
			}

			DelayInSamples = FMath::Clamp(DelayInSamples, 0.0f, (float)(AudioBufferSize - 1));

			// Subtract from write index the delay in samples (will do interpolation during read)
			ReadIndex = WriteIndex - (int32)(DelayInSamples + 1.0f);

			// If negative, wrap around
			if (ReadIndex < 0)
			{
				ReadIndex += AudioBufferSize;
			}
		}
	}
}
