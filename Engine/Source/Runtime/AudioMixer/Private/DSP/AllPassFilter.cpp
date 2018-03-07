// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "DSP/AllPassFilter.h"

namespace Audio
{
	FDelayAPF::FDelayAPF()
		: G(0.0f)
	{
	}

	FDelayAPF::~FDelayAPF()
	{
	}

	void FDelayAPF::ProcessAudio(const float* InputSample, float* OutputSample)
	{
		// Read the delay line to get w(n-D);
		const float WnD = this->Read();

		// For the APF if the delay is 0.0 we just need to pass input -> output
		if (ReadIndex == WriteIndex)
		{
			this->WriteDelayAndInc(*InputSample);
			*OutputSample = *InputSample;
			return;
		}

		// Form w(n) = x(n) + gw(n-D)
		const float Wn = *InputSample + G*WnD;

		// form y(n) = -gw(n) + w(n-D)
		float Yn = -G*Wn + WnD;

		Yn = UnderflowClamp(Yn);
		this->WriteDelayAndInc(Wn);
		*OutputSample = Yn;
	}

}
