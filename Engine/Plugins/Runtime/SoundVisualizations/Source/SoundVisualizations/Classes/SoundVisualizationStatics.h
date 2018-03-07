// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SoundVisualizationStatics.generated.h"

class USoundWave;

UCLASS()
class USoundVisualizationStatics : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

	static void CalculateFrequencySpectrum(USoundWave* SoundWave, const bool bSplitChannels, const float StartTime, const float TimeLength, const int32 SpectrumWidth, TArray< TArray<float> >& OutSpectrums);

	/** Calculates the frequency spectrum for a window of time for the SoundWave
	 * @param SoundWave - The wave to generate the spectrum for
	 * @param Channel - The channel of the sound to calculate.  Specify 0 to combine channels together
	 * @param StartTime - The beginning of the window to calculate the spectrum of
	 * @param TimeLength - The duration of the window to calculate the spectrum of
	 * @param SpectrumWidth - How wide the spectrum is.  The total samples in the window are divided evenly across the spectrum width.
	 * @return OutSpectrum - The resulting spectrum
	 */
	UFUNCTION(BlueprintCallable, Category="SoundVisualization")
	static void CalculateFrequencySpectrum(USoundWave* SoundWave, int32 Channel, float StartTime, float TimeLength, int32 SpectrumWidth, TArray<float>& OutSpectrum);

	static void GetAmplitude(USoundWave* SoundWave, const bool bSplitChannels, const float StartTime, const float TimeLength, const int32 AmplitudeBuckets, TArray< TArray<float> >& OutAmplitudes);

	/** Gathers the amplitude of the wave data for a window of time for the SoundWave
	 * @param SoundWave - The wave to get samples from
	 * @param Channel - The channel of the sound to get.  Specify 0 to combine channels together
	 * @param StartTime - The beginning of the window to get the amplitude from
	 * @param TimeLength - The duration of the window to get the amplitude from
	 * @param AmplitudeBuckets - How many samples to divide the data in to.  The amplitude is averaged from the wave samples for each bucket
	 * @return OutAmplitudes - The resulting amplitudes
	 */
	UFUNCTION(BlueprintCallable, Category="SoundVisualization")
	static void GetAmplitude(USoundWave* SoundWave, int32 Channel, float StartTime, float TimeLength, int32 AmplitudeBuckets, TArray<float>& OutAmplitudes);

};


