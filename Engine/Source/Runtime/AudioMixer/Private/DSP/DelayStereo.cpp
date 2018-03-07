// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "DSP/DelayStereo.h"

namespace Audio
{
	FDelayStereo::FDelayStereo()
		: DelayMode(EStereoDelayMode::Normal)
		, DelayTimeMsec(0.0f)
		, Feedback(0.0f)
		, DelayRatio(0.0f)
		, WetLevel(0.0f)
		, bIsInit(true)
	{
	}

	FDelayStereo::~FDelayStereo()
	{
	}

	void FDelayStereo::SetMode(const EStereoDelayMode::Type InMode)
	{
		DelayMode = InMode;
	}

	void FDelayStereo::SetDelayTimeMsec(const float InDelayTimeMsec)
	{
		DelayTimeMsec = InDelayTimeMsec;
		UpdateDelays();
	}

	void FDelayStereo::SetFeedback(const float InFeedback)
	{
		Feedback = FMath::Clamp(InFeedback, 0.0f, 1.0f);
	}

	void FDelayStereo::SetDelayRatio(const float InDelayRatio)
	{
		DelayRatio = FMath::Clamp(InDelayRatio, -1.0f, 1.0f);
		UpdateDelays();
	}

	void FDelayStereo::SetWetLevel(const float InWetLevel)
	{
		WetLevel = FMath::Clamp(InWetLevel, 0.0f, 1.0f);;
	}

	void FDelayStereo::Init(const float InSampleRate, const float InDelayLengthSec)
	{
		// Init the delay lines
		LeftDelay.Init(InSampleRate, 2.0f * InDelayLengthSec);
		RightDelay.Init(InSampleRate, 2.0f * InDelayLengthSec);

		Reset();
	}

	void FDelayStereo::Reset()
	{
		bIsInit = true;
		LeftDelay.Reset();
		RightDelay.Reset();
	}

	void FDelayStereo::UpdateDelays()
	{
		// As delay ratio goes to zero, the delay times are the same
		LeftDelay.SetEasedDelayMsec(DelayTimeMsec * (1.0f + DelayRatio), bIsInit);
		RightDelay.SetEasedDelayMsec(DelayTimeMsec * (1.0f - DelayRatio), bIsInit);
	}

	void FDelayStereo::ProcessAudio(const float InLeftSample, const float InRightSample, float& OutLeftSample, float& OutRightSample)
	{
		bIsInit = false;

		float LeftDelayOut = LeftDelay.Read();
		float RightDelayOut = RightDelay.Read();

		float LeftDelayIn = 0.0f;
		float RightDelayIn = 0.0f;

		if (DelayMode == EStereoDelayMode::Normal)
		{
			LeftDelayIn = InLeftSample + LeftDelayOut * Feedback;
			RightDelayIn = InRightSample + RightDelayOut * Feedback;
		}
		else if (DelayMode == EStereoDelayMode::Cross)
		{
			LeftDelayIn = InRightSample + LeftDelayOut * Feedback;
			RightDelayIn = InLeftSample + RightDelayOut * Feedback;
		}
		else if (DelayMode == EStereoDelayMode::PingPong)
		{
			LeftDelayIn = InRightSample + RightDelayOut * Feedback;
			RightDelayIn = InLeftSample + LeftDelayOut * Feedback;
		}

		float WetLeftOut = 0.0f;
		float WetRightOut = 0.0f;
		LeftDelay.ProcessAudio(&LeftDelayIn, &WetLeftOut);
		RightDelay.ProcessAudio(&RightDelayIn, &WetRightOut);

		OutLeftSample = InLeftSample + WetLevel * WetLeftOut;
		OutRightSample = InRightSample + WetLevel * WetRightOut;
	}


}
