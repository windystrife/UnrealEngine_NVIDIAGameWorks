#include "SoundUtilities.h"
#include "DSP/Dsp.h"

float USoundUtilitiesBPFunctionLibrary::GetBeatTempo(float BeatsPerMinute, int32 BeatMultiplier, int32 DivisionsOfWholeNote)
{
	const float QuarterNoteTime = 60.0f / FMath::Max(1.0f, BeatsPerMinute);
	return 4.0f * (float)BeatMultiplier * QuarterNoteTime / FMath::Max(1, DivisionsOfWholeNote);
}

float USoundUtilitiesBPFunctionLibrary::GetFrequencyFromMIDIPitch(int32 MidiNote)
{
	return Audio::GetFrequencyFromMidi((float)MidiNote);
}

int32 USoundUtilitiesBPFunctionLibrary::GetMIDIPitchFromFrequency(float Frequency)
{
	return Audio::GetMidiFromFrequency(Frequency);
}

float USoundUtilitiesBPFunctionLibrary::GetPitchScaleFromMIDIPitch(int32 BaseMidiNote, int32 TargetMidiNote)
{
	return Audio::GetPitchScaleFromMIDINote(BaseMidiNote, TargetMidiNote);
}