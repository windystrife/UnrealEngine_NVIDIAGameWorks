// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SoundVisualizationStatics.h"
#include "Audio.h"
#include "Sound/SoundWave.h"
#include "tools/kiss_fftnd.h"

/////////////////////////////////////////////////////
// USoundVisualizationStatics

DEFINE_LOG_CATEGORY_STATIC(LogSoundVisualization, Log, All);

USoundVisualizationStatics::USoundVisualizationStatics(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void USoundVisualizationStatics::GetAmplitude(USoundWave* SoundWave, int32 Channel, float StartTime, float TimeLength, int32 AmplitudeBuckets, TArray<float>& OutAmplitudes)
{
	OutAmplitudes.Empty();

#if WITH_EDITORONLY_DATA
	if (SoundWave)
	{
		if (Channel >= 0)
		{
			TArray< TArray<float> > Amplitudes;

			GetAmplitude(SoundWave, (Channel != 0), StartTime, TimeLength, AmplitudeBuckets, Amplitudes);

			if(Channel == 0)
			{
				OutAmplitudes = Amplitudes[0];
			}
			else if (Channel <= Amplitudes.Num())
			{
				OutAmplitudes = Amplitudes[Channel-1];
			}
			else
			{
				UE_LOG(LogSoundVisualization, Warning, TEXT("Requested channel %d, sound only has %d channels"), SoundWave->NumChannels);
			}
		}
		else
		{
			UE_LOG(LogSoundVisualization, Warning, TEXT("Invalid Channel (%d)"), Channel);
		}
	}
#else
	UE_LOG(LogSoundVisualization, Warning, TEXT("Get Amplitude does not work for cooked builds yet."));
#endif
}

void USoundVisualizationStatics::GetAmplitude(USoundWave* SoundWave, const bool bSplitChannels, const float StartTime, const float TimeLength, const int32 AmplitudeBuckets, TArray< TArray<float> >& OutAmplitudes)
{

	OutAmplitudes.Empty();

#if WITH_EDITORONLY_DATA
	const int32 NumChannels = SoundWave->NumChannels;
	if (AmplitudeBuckets > 0 && NumChannels > 0)
	{
		// Setup the output data
		OutAmplitudes.AddZeroed((bSplitChannels ? NumChannels : 1));
		for (int32 ChannelIndex = 0; ChannelIndex < OutAmplitudes.Num(); ++ChannelIndex)
		{
			OutAmplitudes[ChannelIndex].AddZeroed(AmplitudeBuckets);
		}

		// check if there is any raw sound data
		if( SoundWave->RawData.GetBulkDataSize() > 0 )
		{
			// Lock raw wave data.
			uint8* RawWaveData = ( uint8* )SoundWave->RawData.Lock( LOCK_READ_ONLY );
			int32 RawDataSize = SoundWave->RawData.GetBulkDataSize();
			FWaveModInfo WaveInfo;

			// parse the wave data
			if( WaveInfo.ReadWaveHeader( RawWaveData, RawDataSize, 0 ) )
			{
				uint32 SampleCount = 0;
				uint32 SampleCounts[10] = {0};

				uint32 FirstSample = *WaveInfo.pSamplesPerSec * StartTime;
				uint32 LastSample = *WaveInfo.pSamplesPerSec * (StartTime + TimeLength);

				if (NumChannels <= 2)
				{
					SampleCount = WaveInfo.SampleDataSize / (2 * NumChannels);
				}
				else
				{
					for (int32 ChannelIndex = 0; ChannelIndex < NumChannels; ++ChannelIndex)
					{
						SampleCounts[ChannelIndex] = (SoundWave->ChannelSizes[ChannelIndex] / 2);
						SampleCount = FMath::Max(SampleCount, SampleCounts[ChannelIndex]);
						SampleCounts[ChannelIndex] -= FirstSample;
					}
				}

				FirstSample = FMath::Min(SampleCount, FirstSample);
				LastSample = FMath::Min(SampleCount, LastSample);

				int16* SamplePtr = reinterpret_cast<int16*>(WaveInfo.SampleDataStart);
				if (NumChannels <= 2)
				{
					SamplePtr += FirstSample;
				}

				uint32 SamplesPerAmplitude = (LastSample - FirstSample) / AmplitudeBuckets;
				uint32 ExcessSamples = (LastSample - FirstSample) % AmplitudeBuckets;

				for (int32 AmplitudeIndex = 0; AmplitudeIndex < AmplitudeBuckets; ++AmplitudeIndex)
				{
					if (NumChannels <= 2)
					{
						int64 SampleSum[2] = {0};
						uint32 SamplesToRead = SamplesPerAmplitude + (ExcessSamples-- > 0 ? 1 : 0);
						for (uint32 SampleIndex = 0; SampleIndex < SamplesToRead; ++SampleIndex)
						{
							for (int32 ChannelIndex = 0; ChannelIndex < NumChannels; ++ChannelIndex)
							{
								SampleSum[ChannelIndex] += FMath::Abs(*SamplePtr);
								SamplePtr++;
							}
						}
						for (int32 ChannelIndex = 0; ChannelIndex < NumChannels; ++ChannelIndex)
						{
							OutAmplitudes[(bSplitChannels ? ChannelIndex : 0)][AmplitudeIndex] = SampleSum[ChannelIndex] / (float)SamplesToRead;
						}
					}
					else
					{
						uint32 SamplesRead = 0;
						int64 SampleSum = 0;
						uint32 SamplesToRead = SamplesPerAmplitude + (ExcessSamples-- > 0 ? 1 : 0);
						for (int32 ChannelIndex = 0; ChannelIndex < NumChannels; ++ChannelIndex)
						{
							uint32 SamplesToReadForChannel = FMath::Min(SamplesToRead, SampleCounts[ChannelIndex]);

							if (SamplesToReadForChannel > 0)
							{
								if (bSplitChannels)
								{
									SampleSum = 0;
								}

								for (uint32 SampleIndex = 0; SampleIndex < SamplesToReadForChannel; ++SampleIndex)
								{
									SampleSum += FMath::Abs(*(SamplePtr + FirstSample + SampleIndex + SoundWave->ChannelOffsets[ChannelIndex] / 2));
								}

								if (bSplitChannels)
								{
									OutAmplitudes[ChannelIndex][AmplitudeIndex] = SampleSum / (float)SamplesToReadForChannel;
								}
								SamplesRead += SamplesToReadForChannel;
								SampleCounts[ChannelIndex] -= SamplesToReadForChannel;
							}
						}

						if (!bSplitChannels)
						{
							OutAmplitudes[0][AmplitudeIndex] = SampleSum / (float)SamplesRead;
						}

						FirstSample += SamplesToRead;
					}
				}
			}

			SoundWave->RawData.Unlock();
		}
	}
#else
	UE_LOG(LogSoundVisualization, Warning, TEXT("Get Amplitude does not work for cooked builds yet."));
#endif
}

void USoundVisualizationStatics::CalculateFrequencySpectrum(USoundWave* SoundWave, int32 Channel, float StartTime, float TimeLength, int32 SpectrumWidth, TArray<float>& OutSpectrum)
{
	OutSpectrum.Empty();

#if WITH_EDITORONLY_DATA
	if (SoundWave)
	{
		if (SpectrumWidth <= 0)
		{
			UE_LOG(LogSoundVisualization, Warning, TEXT("Invalid SpectrumWidth (%d)"), SpectrumWidth);
		}
		else if (Channel < 0)
		{
			UE_LOG(LogSoundVisualization, Warning, TEXT("Invalid Channel (%d)"), Channel);
		}
		else
		{
			TArray< TArray<float> > Spectrums;

			CalculateFrequencySpectrum(SoundWave, (Channel != 0), StartTime, TimeLength, SpectrumWidth, Spectrums);

			if(Channel == 0)
			{
				OutSpectrum = Spectrums[0];
			}
			else if (Channel <= Spectrums.Num())
			{
				OutSpectrum = Spectrums[Channel-1];
			}
			else
			{
				UE_LOG(LogSoundVisualization, Warning, TEXT("Requested channel %d, sound only has %d channels"), SoundWave->NumChannels);
			}
		}
	}
#else	
	UE_LOG(LogSoundVisualization, Warning, TEXT("Calculate Frequency Spectrum does not work for cooked builds yet."));
#endif
}

float GetFFTInValue(const int16 SampleValue, const int16 SampleIndex, const int16 SampleCount)
{
	float FFTValue = SampleValue;

	// Apply the Hann window
	FFTValue *= 0.5f * (1 - FMath::Cos(2 * PI * SampleIndex / (SampleCount - 1)));

	return FFTValue;
}

void USoundVisualizationStatics::CalculateFrequencySpectrum(USoundWave* SoundWave, const bool bSplitChannels, const float StartTime, const float TimeLength, const int32 SpectrumWidth, TArray< TArray<float> >& OutSpectrums)
{

	OutSpectrums.Empty();

#if WITH_EDITORONLY_DATA
	const int32 NumChannels = SoundWave->NumChannels;
	if (SpectrumWidth > 0 && NumChannels > 0)
	{
		// Setup the output data
		OutSpectrums.AddZeroed((bSplitChannels ? NumChannels : 1));
		for (int32 ChannelIndex = 0; ChannelIndex < OutSpectrums.Num(); ++ChannelIndex)
		{
			OutSpectrums[ChannelIndex].AddZeroed(SpectrumWidth);
		}

		// check if there is any raw sound data
		if( SoundWave->RawData.GetBulkDataSize() > 0 )
		{
			// Lock raw wave data.
			uint8* RawWaveData = ( uint8* )SoundWave->RawData.Lock( LOCK_READ_ONLY );
			int32 RawDataSize = SoundWave->RawData.GetBulkDataSize();
			FWaveModInfo WaveInfo;

			// parse the wave data
			if( WaveInfo.ReadWaveHeader( RawWaveData, RawDataSize, 0 ) )
			{
				int32 SampleCount = 0;
				int32 SampleCounts[10] = {0};

				int32 FirstSample = *WaveInfo.pSamplesPerSec * StartTime;
				int32 LastSample = *WaveInfo.pSamplesPerSec * (StartTime + TimeLength);

				if (NumChannels <= 2)
				{
					SampleCount = WaveInfo.SampleDataSize / (2 * NumChannels);
				}
				else
				{
					for (int32 ChannelIndex = 0; ChannelIndex < NumChannels; ++ChannelIndex)
					{
						SampleCounts[ChannelIndex] = (SoundWave->ChannelSizes[ChannelIndex] / 2);
						SampleCount = FMath::Max(SampleCount, SampleCounts[ChannelIndex]);
						SampleCounts[ChannelIndex] -= FirstSample;
					}
				}

				FirstSample = FMath::Min(SampleCount, FirstSample);
				LastSample = FMath::Min(SampleCount, LastSample);

				int32 SamplesToRead = LastSample - FirstSample;

				if (SamplesToRead > 0)
				{
					// Shift the window enough so that we get a power of 2
					int32 PoT = 2;
					while (SamplesToRead > PoT) PoT *= 2;
					FirstSample = FMath::Max(0, FirstSample - (PoT - SamplesToRead) / 2);
					SamplesToRead = PoT;
					LastSample = FirstSample + SamplesToRead;
					if (LastSample > SampleCount)
					{
						FirstSample = LastSample - SamplesToRead;
					}
					if (FirstSample < 0)
					{
						// If we get to this point we can't create a reasonable window so just give up
						SoundWave->RawData.Unlock();
						return;
					}

					kiss_fft_cpx* buf[10] = { 0 }; 
					kiss_fft_cpx* out[10] = { 0 };

					int32 Dims[1] = { SamplesToRead };
					kiss_fftnd_cfg stf = kiss_fftnd_alloc(Dims, 1, 0, NULL, NULL);


					int16* SamplePtr = reinterpret_cast<int16*>(WaveInfo.SampleDataStart);
					if (NumChannels <= 2)
					{
						for (int32 ChannelIndex = 0; ChannelIndex < NumChannels; ++ChannelIndex)
						{
							buf[ChannelIndex] = (kiss_fft_cpx *)KISS_FFT_MALLOC(sizeof(kiss_fft_cpx) * SamplesToRead);
							out[ChannelIndex] = (kiss_fft_cpx *)KISS_FFT_MALLOC(sizeof(kiss_fft_cpx) * SamplesToRead);
						}

						SamplePtr += (FirstSample * NumChannels);

						for (int32 SampleIndex = 0; SampleIndex < SamplesToRead; ++SampleIndex)
						{
							for (int32 ChannelIndex = 0; ChannelIndex < NumChannels; ++ChannelIndex)
							{
								buf[ChannelIndex][SampleIndex].r = GetFFTInValue(*SamplePtr, SampleIndex, SamplesToRead);
								buf[ChannelIndex][SampleIndex].i = 0.f;

								SamplePtr++;
							}
						}
					}
					else
					{
						for (int32 ChannelIndex = 0; ChannelIndex < NumChannels; ++ChannelIndex)
						{
							// Drop this channel out if there isn't the power of 2 number of samples available
							if (SampleCounts[ChannelIndex] >= SamplesToRead)
							{
								buf[ChannelIndex] = (kiss_fft_cpx *)KISS_FFT_MALLOC(sizeof(kiss_fft_cpx) * SamplesToRead);
								out[ChannelIndex] = (kiss_fft_cpx *)KISS_FFT_MALLOC(sizeof(kiss_fft_cpx) * SamplesToRead);

								for (int32 SampleIndex = 0; SampleIndex < SamplesToRead; ++SampleIndex)
								{
									buf[ChannelIndex][SampleIndex].r = GetFFTInValue(*(SamplePtr + FirstSample + SampleIndex + SoundWave->ChannelOffsets[ChannelIndex] / 2), SampleIndex, SamplesToRead);
									buf[ChannelIndex][SampleIndex].i = 0.f;
								}
							}
						}
					}

					for (int32 ChannelIndex = 0; ChannelIndex < NumChannels; ++ChannelIndex)
					{
						if (buf[ChannelIndex])
						{
							kiss_fftnd(stf, buf[ChannelIndex], out[ChannelIndex]);
						}
					}

					int32 SamplesPerSpectrum = SamplesToRead / (2 * SpectrumWidth);
					int32 ExcessSamples = SamplesToRead % (2 * SpectrumWidth);

					int32 FirstSampleForSpectrum = 1;
					for (int32 SpectrumIndex = 0; SpectrumIndex < SpectrumWidth; ++SpectrumIndex)
					{
						static bool doLog = false;

						int32 SamplesRead = 0;
						double SampleSum = 0;
						int32 SamplesForSpectrum = SamplesPerSpectrum + (ExcessSamples-- > 0 ? 1 : 0);
						if (doLog) UE_LOG(LogSoundVisualization, Log, TEXT("----"));
						for (int32 ChannelIndex = 0; ChannelIndex < NumChannels; ++ChannelIndex)
						{
							if (out[ChannelIndex])
							{
								if (bSplitChannels)
								{
									SampleSum = 0;
								}

								for (int32 SampleIndex = 0; SampleIndex < SamplesForSpectrum; ++SampleIndex)
								{
									float PostScaledR = out[ChannelIndex][FirstSampleForSpectrum + SampleIndex].r * 2.f / SamplesToRead;
									float PostScaledI = out[ChannelIndex][FirstSampleForSpectrum + SampleIndex].i * 2.f / SamplesToRead;
									//float Val = FMath::Sqrt(FMath::Square(PostScaledR) + FMath::Square(PostScaledI));
									float Val = 10.f * FMath::LogX(10.f, FMath::Square(PostScaledR) + FMath::Square(PostScaledI));
									if (doLog) UE_LOG(LogSoundVisualization, Log, TEXT("%.2f"), Val);
									SampleSum += Val;
								}

								if (bSplitChannels)
								{
									OutSpectrums[ChannelIndex][SpectrumIndex] = (float)(SampleSum / SamplesForSpectrum);
								}
								SamplesRead += SamplesForSpectrum;
							}
						}

						if (!bSplitChannels)
						{
							OutSpectrums[0][SpectrumIndex] = (float)(SampleSum / SamplesRead);
						}

						FirstSampleForSpectrum += SamplesForSpectrum;
					}

					KISS_FFT_FREE(stf);
					for (int32 ChannelIndex = 0; ChannelIndex < NumChannels; ++ChannelIndex)
					{
						if (buf[ChannelIndex])
						{
							KISS_FFT_FREE(buf[ChannelIndex]);
							KISS_FFT_FREE(out[ChannelIndex]);
						}
					}
				}
			}

			SoundWave->RawData.Unlock();
		}
	}
#else
	UE_LOG(LogSoundVisualization, Warning, TEXT("Calculate Frequency Spectrum does not work for cooked builds yet."));
#endif
}


