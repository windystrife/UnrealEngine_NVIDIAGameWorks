// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "DSP/RingModulation.h"
#include "DSP/Dsp.h"

namespace Audio
{
	FRingModulation::FRingModulation()
		: ModulationFrequency(800.0f)
		, ModulationDepth(0.5f)
	{

	}

	FRingModulation::~FRingModulation()
	{

	}

	void FRingModulation::Init(const float InSampleRate)
	{
		Osc.Init(InSampleRate);
		Osc.SetFrequency(ModulationFrequency);
		Osc.Update();
		Osc.Start();
	}

	void FRingModulation::SetModulatorWaveType(const EOsc::Type InType)
	{
		Osc.SetType(InType);
	}

	void FRingModulation::SetModulationFrequency(const float InModulationFrequency)
	{
		Osc.SetFrequency(FMath::Clamp(InModulationFrequency, 10.0f, 10000.0f));
		Osc.Update();
	}

	void FRingModulation::SetModulationDepth(const float InModulationDepth)
	{
		ModulationDepth = FMath::Clamp(InModulationDepth, -1.0f, 1.0f);
	}

	void FRingModulation::ProcessAudio(const float InLeftSample, const float InRightSample, float& OutLeftSample, float& OutRightSample)
	{
		float OscOut = Osc.Generate();
		OutLeftSample = InLeftSample * OscOut * ModulationDepth;
		OutRightSample = InRightSample * OscOut * ModulationDepth;
	}

}
