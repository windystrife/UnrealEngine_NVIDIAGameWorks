#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "SoundUtilities.generated.h"

/** Sound Utilities Blueprint Function Library
*  A library of Sound related functions for use in Blueprints
*/
UCLASS()
class SOUNDUTILITIES_API USoundUtilitiesBPFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

	/** Calculates a beat time in seconds from the given BPM, beat multipler and divisions of a whole note. */
	UFUNCTION(BlueprintCallable, Category = "SoundUtilitiesBPLibrary")
	static float GetBeatTempo(float BeatsPerMinute = 120.0f, int32 BeatMultiplier = 1, int32 DivisionsOfWholeNote = 4);

	/** Calculates Frequency values based on MIDI Pitch input */
	UFUNCTION(BlueprintCallable, Category = "SoundUtilitiesBPLibrary")
	static float GetFrequencyFromMIDIPitch(int32 MidiNote = 69);

	/** Calculates MIDI Pitch values based on frequency input*/
	UFUNCTION(BlueprintCallable, Category = "SoundUtilitiesBPLibrary")
	static int32 GetMIDIPitchFromFrequency(float Frequency = 440.0f);

	/** Calculates Pitch Scalar based on starting frequency and desired MIDI Pitch output */
	UFUNCTION(BlueprintCallable, Category = "SoundUtilitiesBPLibrary")
	static float GetPitchScaleFromMIDIPitch(int32 BaseMidiNote = 69, int32 TargetMidiNote = 69);
};