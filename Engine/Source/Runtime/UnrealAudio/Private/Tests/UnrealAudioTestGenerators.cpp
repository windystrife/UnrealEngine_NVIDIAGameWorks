// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Tests/UnrealAudioTestGenerators.h"

#if ENABLE_UNREAL_AUDIO & WITH_DEV_AUTOMATION_TESTS

namespace UAudio
{
namespace Test
{
	/************************************************************************/
	/* Static Data															*/
	/************************************************************************/

	struct FCallbackData
	{
		float FrameRate;
		int32 NumFrames;
		int32 NumChannels;
		double Time;
		TArray<ESpeaker::Type> Speakers;
		bool bInitialized;

		FCallbackData()
			: bInitialized(false)
		{
		}

	} CallbackData;

	/************************************************************************/
	/* Static Helper Functions                                              */
	/************************************************************************/

	static float WrapTwoPi(float Value)
	{
		while (Value > 2.0 * PI)
		{
			Value -= 2.0 * PI;
		}

		while (Value < 0)
		{
			Value += 2.0 * PI;
		}
		return Value;
	}

	static float Lerp(float StartX, float EndX, float StartY, float EndY, float Alpha)
	{
		if (EndX - StartX <= 0.0f)
		{
			return StartY;
		}
		float Delta = (Alpha - StartX) / (EndX - StartX);
		return Delta * EndY + (1.0f - Delta)*StartY;
	}

	void UpdateCallbackData(FCallbackInfo& CallbackInfo)
	{
		// Only update static data once
		if (!CallbackData.bInitialized)
		{
			CallbackData.bInitialized = true;
			CallbackData.FrameRate = (float)CallbackInfo.FrameRate;
			CallbackData.NumFrames = CallbackInfo.NumFrames;
			CallbackData.NumChannels = CallbackInfo.NumChannels;
			CallbackData.Speakers = CallbackInfo.OutputSpeakers;
		}

		// Update dynamic data every time
		CallbackData.Time = CallbackInfo.StreamTime;
	}

	/************************************************************************/
	/* FTimer																*/
	/************************************************************************/

	FTimer::FTimer(double InTotalTime)
		: TotalTime(InTotalTime)
		, StartTime(0.0f)
		, CurrentTime(0.0f)
		, LastTime(0.0f)
	{
	}

	FTimer::~FTimer()
	{
	}

	bool FTimer::Update()
	{
		LastTime = CallbackData.Time;
		CurrentTime = LastTime - StartTime;
		return IsDone();
	}

	bool FTimer::IsDone() const
	{
		return TotalTime >= 0.0f && CurrentTime >= TotalTime;;
	}

	void FTimer::Start(double InTotalTime)
	{
		StartTime = CallbackData.Time;
		TotalTime = InTotalTime;
		CurrentTime = 0.0;
	}

	void FTimer::Reset()
	{
		StartTime = CallbackData.Time;
	}

	float FTimer::GetTotalTime() const
	{
		return TotalTime;
	}

	/************************************************************************/
	/* FBiquad																*/
	/************************************************************************/

	FBiquad::FBiquad()
	{
		memset((void *)this, 0, sizeof(FBiquad));
	}

	FBiquad::~FBiquad()
	{
	}

	float FBiquad::Update(float Value)
	{
		// A biquad has difference equation:
		// y(n) = a0 * x(n) + a1 * x(n-1) + a2 * x(n-2) - b1 * y(n-1) - b2 * y(n-2)

		float Output = A0 * Value + A1 * X1 + A2 * X2 - B1 * Y1 - B2 * Y2;
		if ((Output > 0.0f && Output < FLT_MIN) || (Output < 0.0f && Output > -FLT_MIN))
		{
			Output = 0.0f;
		}
		Y2 = Y1;
		Y1 = Output;
		X2 = X1;
		X1 = Value;
		return Output;
	}

	/************************************************************************/
	/* FLowPass																*/
	/************************************************************************/

	FLowPass::FLowPass()
		: Quality(1.0f)
		, Frequency(0.0f)
	{
	}

	FLowPass::~FLowPass()
	{
	}

	void FLowPass::SetFrequency(float InFrequency)
	{
		SetParams(InFrequency, Quality);
	}

	void FLowPass::SetQuality(float InQuality)
	{
		SetParams(Frequency, InQuality);
	}

	void FLowPass::SetParams(float InFrequency, float InQuality)
	{
		Frequency = InFrequency;
		Quality = InQuality;

		check(CallbackData.FrameRate > 0.0f);

		float Theta = 2.0 * PI * Frequency / CallbackData.FrameRate;
		float InverseQ = 0.5f / Quality;

		float Temp = InverseQ * FMath::Sin(Theta);
		float Beta = 0.5f * (1.0f - Temp) / (1.0f + Temp);
		float Gamma = (0.5f + Beta) * FMath::Cos(Theta);
		float Alpha = 0.5f * (0.5f + Beta - Gamma);

		A0 = Alpha;
		A1 = 2.0f * Alpha;
		A2 = A0;
		B1 = -2.0f * Gamma;
		B2 = 2.0f * Beta;
	}

	/************************************************************************/
	/* FLinEnv														*/
	/************************************************************************/

	FLinEnv::FLinEnv()
		: CurrValue(0.0f)
		, EndValue(0.0f)
		, bIsDone(false)
	{
	}

	FLinEnv::~FLinEnv()
	{
	}

	void FLinEnv::Start(float End, float Time)
	{
		if (Time == 0.0f)
		{
			bIsDone = true;
			CurrValue = End;
		}
		else
		{
			bIsDone = false;
			EndValue = End;
			DeltaValue = (End - CurrValue) / (Time * CallbackData.FrameRate);
		}
	}

	float FLinEnv::Update()
	{
		if (FMath::Abs(CurrValue - EndValue) < 0.001f)
		{
			bIsDone = true;
			CurrValue = EndValue;
		}
		else
		{
			CurrValue += DeltaValue;
		}
		return CurrValue;
	}

	/************************************************************************/
	/* FSaw																	*/
	/************************************************************************/

	FSaw::FSaw()
		: CurrValue(0.0f)
		, DeltaValue(0.0f)
		, Frequency(0.0f)
		, Scale(1.0f)
		, Add(0.0f)
	{
	}

	FSaw::~FSaw()
	{
	}

	void FSaw::SetFrequency(float InFrequency)
	{
		check(CallbackData.FrameRate > 0.0f);
		Frequency = InFrequency;
		DeltaValue = Frequency / CallbackData.FrameRate;
	}

	void FSaw::SetScaleAdd(float InScale, float InAdd)
	{
		Scale = InScale;
		Add = InAdd;
	}

	float FSaw::Update()
	{
		float Result = CurrValue;
		CurrValue += DeltaValue;
		while (CurrValue >= 1.0f)
		{
			CurrValue -= 1.0f;
		}
		while (CurrValue <= 0.0f)
		{
			CurrValue += 1.0f;
		}
		return Scale * Result + Add;
	}

	/************************************************************************/
	/* FSineOsc																*/
	/************************************************************************/

	FSineOsc::FSineOsc()
		: Frequency(0.0f)
		, Phase(0.0f)
		, PhaseDelta(0.0f)
		, TargetPhaseDelta(0.0f)
		, Scale(1.0f)
		, Add(0.0f)
		, bNewValue(false)
	{
	}

	FSineOsc::~FSineOsc()
	{
	}

	void FSineOsc::SetFrequency(float InFrequency)
	{
		bool bIsInit = (Frequency == 0.0f);
		Frequency = InFrequency;
		if (bIsInit)
		{
			check(CallbackData.FrameRate > 0.0f);
			PhaseDelta = 2.0 * PI * Frequency / CallbackData.FrameRate;
			TargetPhaseDelta = PhaseDelta;
			PhaseDeltaDelta = 0.0f;
			Phase = 0.0f;
			bNewValue = false;
		}
		else
		{
			TargetPhaseDelta = 2.0 * PI * Frequency / CallbackData.FrameRate;
			PhaseDeltaDelta = (TargetPhaseDelta - PhaseDelta) / 100.0f;
			bNewValue = true;
		}
	}

	void FSineOsc::SetScaleAdd(float InScale, float InAdd)
	{
		Scale = InScale;
		Add = InAdd;
	}

	void FSineOsc::SetOutputRange(float Min, float Max)
	{
		Add = Min + 1.0f;
		Scale = 0.5f * (Max - Min);
	}

	float FSineOsc::Update()
	{
		Phase += PhaseDelta;

		if (bNewValue)
		{
			if (FMath::Abs<float>(PhaseDelta - TargetPhaseDelta) < 0.00001f)
			{
				PhaseDelta = TargetPhaseDelta;
				bNewValue = false;
			}
			else
			{
				PhaseDelta += PhaseDeltaDelta;
			}
		}

		Phase = WrapTwoPi(Phase);
		return Scale * FMath::Sin(Phase) + Add;
	}

	/************************************************************************/
	/* FPan																	*/
	/************************************************************************/

	FPan::FPan()
		: Pan(0.0f)
		, LFEIndex(-1)
		, PrevSpeakerIndex(-1)
		, NumNonLfeSpeakers(0)
	{
	}

	FPan::~FPan()
	{
	}

	void FPan::Init(float InPan)
	{
		Pan = InPan;
		SpeakerMap.Init(0, CallbackData.NumChannels);

		NumNonLfeSpeakers = 0;
		for (int32 Speaker = 0; Speaker < CallbackData.NumChannels; ++Speaker)
		{
			if (CallbackData.Speakers[Speaker] == ESpeaker::LOW_FREQUENCY)
			{
				LFEIndex = Speaker;
			}
			else
			{
				NumNonLfeSpeakers++;
			}
		}
	}

	void FPan::SetPan(float InPan)
	{
		Pan = FMath::Clamp(InPan, 0.0f, 1.0f);
	}

	void FPan::Spatialize(float Value, float* Frame)
	{
		// Scale the pan value by the total number of (non-lfe) speakers
		float SpeakerFractionTotal = Pan * (float)NumNonLfeSpeakers;

		// Cast to integer to get the speaker on the left side of the sound
		int32 SpeakerIndex = (int)SpeakerFractionTotal % NumNonLfeSpeakers;

		// Get fraction of pan from this speaker to next
		float SpeakerFraction = SpeakerFractionTotal - (float)SpeakerIndex;

		// Use simple equal power panning to compute the amount of pan between the speakers
		float LeftAmount = FMath::Cos(SpeakerFraction * PI_OVER_TWO);
		float RightAmount = FMath::Sin(SpeakerFraction * PI_OVER_TWO);

		// Now Build a speaker map

		// Zero out the current speaker map if the speaker indices have changed
		if (PrevSpeakerIndex != SpeakerIndex)
		{
			if (SpeakerMap.Num() == 0)
			{
				SpeakerMap.Init(0, NumNonLfeSpeakers);
			}
			else
			{
				// Zero out the speaker map if the speaker indices changed
				memset((void*)SpeakerMap.GetData(), 0, sizeof(float)*NumNonLfeSpeakers);
			}
		}
		PrevSpeakerIndex = SpeakerIndex;

		// Now map the values to the correct index
		SpeakerMap[SpeakerIndex] = LeftAmount;

		int32 NextIndex = (SpeakerIndex + 1) % NumNonLfeSpeakers;
		SpeakerMap[NextIndex] = RightAmount;

		// Use speaker map to mix in the value of the synth
		int32 MapIndex = 0;
		for (int32 Channel = 0; Channel < CallbackData.NumChannels; ++Channel)
		{
			if (Channel == LFEIndex)
			{
				Frame[Channel] += Value;
			}
			else
			{
				float PanScale = SpeakerMap[MapIndex];
				if (PanScale > 0.0f)
				{
					// Get the actual output speaker for the given map index
					int32 ChannelIndex = GetOutputSpeaker(MapIndex);
					Frame[ChannelIndex] += PanScale * Value;
				}
				++MapIndex;
			}
		}
	}

	int32 FPan::GetOutputSpeaker(int32 MapIndex)
	{
		if (CallbackData.NumChannels == 2)
		{
			return StereoSpeakerIndexMap[MapIndex];
		}
		else if (CallbackData.NumChannels == 4)
		{
			return QuadSpeakerIndexMap[MapIndex];
		}
		else if (CallbackData.NumChannels == 6)
		{
			return FiveOneSpeakerIndexMap[MapIndex];
		}
		else if (CallbackData.NumChannels == 8)
		{
			return SevenOneSpeakerIndexMap[MapIndex];
		}
		return 0;
	}

	int32 FPan::StereoSpeakerIndexMap[] =
	{
		0, // LEFT CHANNEL
		1, // RIGHT CHANNEL
	};

	int32 FPan::QuadSpeakerIndexMap[] =
	{
		0, // LEFT CHANNEL
		1, // RIGHT CHANNEL
		3, // BACK RIGHT CHANNEL
		2, // BACK LEFT CHANNEL
	};

	int32 FPan::FiveOneSpeakerIndexMap[] =
	{
		0,	// LEFT CHANNEL
		2,	// CENTER CHANNEL
		1,	// RIGHT CHANNEL
		5,	// RIGHT BACK CHANNEL
		4	// LEFT BACK CHANNEL
	};

	int32 FPan::SevenOneSpeakerIndexMap[] =
	{
		0,  // LEFT CHANNEL
		2,  // CENTER CHANNEL
		1,	// RIGHT CHANNEL
		7,	// RIGHT SIDE
		5,	// RIGHT BACK
		4,	// LEFT BACK
		6,	// LEFT SIDE
	};

	/************************************************************************/
	/* FDelay																*/
	/************************************************************************/

	FDelay::FDelay()
		: MaxLengthSec(0.0f)
		, LengthFrames(0)
		, WriteIndex(0)
	{
	}

	FDelay::~FDelay()
	{
	}

	void FDelay::Init(int32 NumTaps, float InMaxLengthSec)
	{
		check(CallbackData.FrameRate > 0);

		if (LengthFrames != 0)
		{
			return;
		}

		MaxLengthSec = InMaxLengthSec;
		LengthFrames = (int32)(CallbackData.FrameRate * InMaxLengthSec) + 1;

		DelayBuffer.Init(0.0f, LengthFrames);
		Taps.Init(FTap(), NumTaps);
	}

	void FDelay::SetDelayLength(int32 Tap, float LengthSec)
	{
		LengthSec = FMath::Clamp(LengthSec, 0.0f, MaxLengthSec);

		check(Tap < Taps.Num());
		Taps[Tap].DelayFrames = LengthSec * CallbackData.FrameRate;
		Taps[Tap].ReadIndex = WriteIndex - (int32)Taps[Tap].DelayFrames;
		if (Taps[Tap].ReadIndex < 0)
		{
			Taps[Tap].ReadIndex += LengthFrames;
		}
	}

	void FDelay::SetWet(int32 Tap, float WetLevel)
	{
		check(Tap < Taps.Num());
		Taps[Tap].Wet = FMath::Clamp(WetLevel, 0.0f, 1.0f);
	}

	void FDelay::SetFeedback(int32 Tap, float FeedbackLevel)
	{
		check(Tap < Taps.Num());
		Taps[Tap].Feedback = FMath::Clamp(FeedbackLevel, 0.0f, 1.0f);
	}

	void FDelay::GetOutput(float InSample, TArray<float>& TapOutput)
	{
		float Xn = InSample;

		check(TapOutput.Num() == Taps.Num());

		// previous output read in from delay line
		for (int32 TapIndex = 0; TapIndex < Taps.Num(); ++TapIndex)
		{
			FTap& Tap = Taps[TapIndex];
			float Yn = DelayBuffer[Tap.ReadIndex];
			if (Tap.ReadIndex == WriteIndex && Tap.DelayFrames < 1.0f)
			{
				Yn = Xn;
			}

			// Get sample index less than current read index
			int32 ReadIndexPrev = Tap.ReadIndex - 1;

			// Wrap if needed
			if (ReadIndexPrev < 0)
			{
				ReadIndexPrev = LengthFrames - 1;
			}

			float YnPrev = DelayBuffer[ReadIndexPrev];
			float Alpha = Tap.DelayFrames - (int)Tap.DelayFrames;
			float Interp = Lerp(0.0f, 1.0f, Yn, YnPrev, Alpha);
			Yn = Interp;

			// Write input value into the delay buffer (with previous output scaled according to feedback)
			DelayBuffer[WriteIndex] = Xn + Tap.Feedback * Yn;

			Tap.ReadIndex = (Tap.ReadIndex + 1) % LengthFrames;
			TapOutput[TapIndex] = Tap.Wet * Yn + (1.0f - Tap.Wet) * Xn;
		}

		// Update read/write indices
		WriteIndex = (WriteIndex + 1) % LengthFrames;
	}

	/************************************************************************/
	/* IGenerator															*/
	/************************************************************************/

	IGenerator::IGenerator(double LifeTime)
		: LifeTimer(LifeTime)
		, bIsDone(false)
		, bIsInit(false)
	{
	}

	bool IGenerator::IsDone() const
	{
		return bIsDone;
	}

	bool IGenerator::UpdateTimers()
	{
		if (bIsDone)
		{
			return false;
		}

		if (!bIsInit)
		{
			bIsInit = true;
			LifeTimer.Reset();
		}

		if (LifeTimer.Update())
		{
			bIsDone = true;
			return false;
		}

		return true;
	}


	/************************************************************************/
	/* FSimpleOutput														*/
	/************************************************************************/

	FSimpleOutput::FSimpleOutput(double LifeTime)
		: IGenerator(LifeTime)
		, Amplitude(0.5f)
		, ChannelTimer(1.0f)
	{
	}

	FSimpleOutput::~FSimpleOutput()
	{
	}

	bool FSimpleOutput::GetNextBuffer(FCallbackInfo& CallbackInfo)
	{
		if (!UpdateTimers())
		{
			return true;
		}

		if (Sinusoids.Num() == 0)
		{
			ChannelTimer.Start(1.0f);
			CurrentChannelOut = 0;
			for (int32 i = 0; i < CallbackInfo.NumChannels; ++i)
			{
				FChannelSine ChannelSine;
				ChannelSine.SineOsc.SetFrequency(220.0f * (1 + i));
				if (i == CurrentChannelOut)
				{
					ChannelSine.Fader.Start(1.0f, 0.1f);
				}
				Sinusoids.Add(ChannelSine);
			}
		}

		uint32 FrameIndex = 0;
		for (uint32 Frame = 0; Frame < CallbackInfo.NumFrames; ++Frame, FrameIndex += CallbackInfo.NumChannels)
		{
			int32 SpeakerIndex = 0;
			for (int32 Channel = 0; Channel < CallbackInfo.NumChannels; ++Channel)
			{
				FSineOsc& SineOsc = Sinusoids[Channel].SineOsc;
				FLinEnv& Fader = Sinusoids[Channel].Fader;
				float Value = SineOsc.Update();
				Value *= Fader.Update();
				Value *= Amplitude;
				CallbackInfo.OutBuffer[FrameIndex + Channel] = Value;

				if (Fader.IsDone() && Channel == CurrentChannelOut)
				{
					Fader.Start(0.0f, 0.5f);
				}
			}

			if (ChannelTimer.Update())
			{
				CurrentChannelOut = (CurrentChannelOut + 1) % CallbackInfo.NumChannels;
				Sinusoids[CurrentChannelOut].Fader.Start(1.0f, 0.1f);
				ChannelTimer.Start(1.0f);
			}
		}
		return true;
	}

	/************************************************************************/
	/* FPhaseModulator														*/
	/************************************************************************/

	FPhaseModulator::FPhaseModulator(double LifeTime)
		: IGenerator(LifeTime)
		, Amplitude(0.4f)
		, TotalNumSynthesizers(4)
		, CurrNumSynthesisers(0)
	{
		SynthesisData.Init(FSynth(this), TotalNumSynthesizers);
		CurrNumSynthesisers = SynthesisData.Num();
	}

	FPhaseModulator::~FPhaseModulator()
	{
	}

	bool FPhaseModulator::GetNextBuffer(FCallbackInfo& CallbackInfo)
	{
		if (!UpdateTimers())
		{
			return true;
		}

		uint32 FrameIndex = 0;
		for (uint32 Frame = 0; Frame < CallbackInfo.NumFrames; ++Frame, FrameIndex += CallbackInfo.NumChannels)
		{
			for (int32 DataIndex = 0; DataIndex < CurrNumSynthesisers; ++DataIndex)
			{
				SynthesisData[DataIndex].GetNextFrame(&CallbackInfo.OutBuffer[FrameIndex]);
			}
		}
		return true;
	}

	FPhaseModulator::OscData::OscData(float InMaxAmp)
		: Phase(FMath::FRandRange(0.0f, 2.0 * PI))
		, Delta(0.01f)
		, TargetDelta(Delta)
		, DeltaEase(0.001f)
		, SweepPhase(FMath::FRandRange(0.0f, 2.0 * PI))
		, SweepDelta(0.0f)
		, Amp(0.0f)
		, MaxAmp(InMaxAmp)
		, bOscilateSweep(true)
	{
		Reset();
	}

	void FPhaseModulator::OscData::Reset()
	{
		static float Scale[] = { 1.0f, 1.25f, 1.5f };
		float Root = 0.001f;
		TargetDelta = Root * Scale[FMath::RandRange(0, 2)];
		DeltaEase = 0.0001f * FMath::FRand();

		if (FMath::RandBool())
		{
			Delta = FMath::FRandRange(0.01f, 0.2f);
		}
		else
		{
			Delta = FMath::FRandRange(0.01f, 0.2f) / 1000.0f;
		}

		Amp = MaxAmp * FMath::FRandRange(0.1f, 1.0f);
		bOscilateSweep = FMath::RandBool();
		SweepDelta = FMath::FRandRange(-0.000001f, 0.000001f);
	}

	void FPhaseModulator::OscData::Update(bool bIsCarrier /* = false */)
	{
		Phase += Delta;
		Phase = WrapTwoPi(Phase);

		if (bIsCarrier)
		{
			Delta += (TargetDelta - Delta) * DeltaEase;
		}
		else
		{
			if (bOscilateSweep)
			{
				float Sweep = SweepDelta * FMath::Sin(SweepPhase);
				SweepPhase += SweepDelta;
				SweepPhase = WrapTwoPi(SweepPhase);
				Delta += Sweep;
			}
			else
			{
				Delta += SweepDelta;
			}
		}
	}

	float FPhaseModulator::OscData::GetAmp() const
	{
		return Amp;
	}

	float FPhaseModulator::OscData::GetValue() const
	{
		return Amp * FMath::Sin(Phase);
	}

	float FPhaseModulator::OscData::GetPhase() const
	{
		return Phase;
	}

	FPhaseModulator::FSynth::FSynth(FPhaseModulator* InParent)
		: Parent(InParent)
		, Carrier(0.5f)
		, ModIndex(1.0f)
		, PrevSpeakerIndex(-1)
		, bInit(false)
	{
	}

	FPhaseModulator::FSynth::~FSynth()
	{
	}

	void FPhaseModulator::FSynth::Init()
	{
		if (!bInit)
		{
			bInit = true;
			Panner.Init(FMath::FRandRange(0.0f, 1.0f));
			PanRamp.SetFrequency(FMath::FRandRange(-3.0f, 3.0f));
			Mods.Init(OscData(FMath::FRandRange(0.25f, 1.0f)), FMath::RandRange(1, 4));
			Carrier.Reset();
			ModIndex.Reset();
			FilterLFO.SetFrequency(FMath::FRandRange(0.1f, 20.0f));
			FilterLFO.SetScaleAdd(1000.0f, 2500.0f);
			LowPass.SetParams(1500.0f, 1.0f);
			for (OscData& Mod : Mods)
			{
				Mod.Reset();
			}
		}
	}

	void FPhaseModulator::FSynth::GetNextFrame(float* Frame)
	{
		Init();

		// Compute the mod value
		float ModValue = 0.0f;
		for (int32 Mod = 0; Mod < Mods.Num(); ++Mod)
		{
			Mods[Mod].Update();
			ModValue += Mods[Mod].GetValue();
		}

		ModIndex.Update();
		float ModIndexValue = ModIndex.GetValue();

		// Plug the mod value into the carrier to generate the value of this synth
		float Value = Carrier.GetAmp() * ModIndexValue * FMath::Sin(Carrier.GetPhase() + 2.0 * PI * ModValue * ModIndexValue);

		// Scale by the total amplitude
		Value *= Parent->Amplitude;

		// apply the filter
		LowPass.SetFrequency(FilterLFO.Update());
		Value = LowPass.Update(Value);

		// Update the carrier frequency data
		Carrier.Update(true);

		Panner.SetPan(PanRamp.Update());
		Panner.Spatialize(Value, Frame);
	}

	/************************************************************************/
	/* FNoisePan															*/
	/************************************************************************/

	FNoisePan::FNoisePan(double LifeTime)
		: IGenerator(LifeTime)
		, Amp(0.5f)
		, bInit(false)
	{
	}

	FNoisePan::~FNoisePan()
	{
	}

	bool FNoisePan::GetNextBuffer(FCallbackInfo& CallbackInfo)
	{
		if (!UpdateTimers())
		{
			return true;
		}

		if (!bInit)
		{
			bInit = true;

			// Initialize our filter and panner
			PanRamp.SetFrequency(1.0f);
			LowPass.SetParams(1000.0f, 2.0f);
			Panner.Init(0.0f);
			FilterLFO.SetFrequency(0.1f);
			FilterLFO.SetScaleAdd(1000.0f, 2000.0f);
		}

		uint32 BufferIndex = 0;
		for (uint32 Frame = 0; Frame < CallbackInfo.NumFrames; ++Frame, BufferIndex += CallbackInfo.NumChannels)
		{

			float FilterLFOValue = FilterLFO.Update();
			LowPass.SetFrequency(FilterLFOValue);

			// Get new random value (noise!)
			float Value = Amp * FMath::FRand();

			// Filter it to make it a bit less intense
			Value = LowPass.Update(Value);

			// Now spatialize it
			Panner.SetPan(PanRamp.Update());
			Panner.Spatialize(Value, &CallbackInfo.OutBuffer[BufferIndex]);
		}
		return true;
	}

} // namespace Test
} // namespace UAudio

#endif // #if ENABLE_UNREAL_AUDIO & WITH_DEV_AUTOMATION_TESTS
