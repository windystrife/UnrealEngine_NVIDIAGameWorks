// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "DSP/Filter.h"
#include "DSP/Dsp.h"

namespace Audio
{
	FBiquadFilter::FBiquadFilter()
		: FilterType(EBiquadFilter::Lowpass)
		, Biquad(nullptr)
		, SampleRate(0.0f)
		, NumChannels(0)
		, Frequency(0.0f)
		, Bandwidth(0.0f)
		, GainDB(0.0f)
		, bEnabled(true)
	{
	}

	FBiquadFilter::~FBiquadFilter()
	{
		if (Biquad)
		{
			delete[] Biquad;
		}
	}

	void FBiquadFilter::Init(const float InSampleRate, const int32 InNumChannels, const EBiquadFilter::Type InFilterType, const float InCutoffFrequency, const float InBandwidth, const float InGainDB)
	{
		SampleRate = InSampleRate;
		NumChannels = InNumChannels;
		FilterType = InFilterType;
		Frequency = FMath::Max(InCutoffFrequency, 20.0f);
		Bandwidth = InBandwidth;
		GainDB = InGainDB;

		if (Biquad)
		{
			delete[] Biquad;
		}

		Biquad = new FBiquad[NumChannels];
		Reset();
		CalculateBiquadCoefficients();
	}

	void FBiquadFilter::ProcessAudioFrame(const float* InAudio, float* OutAudio)
	{
		if (bEnabled)
		{
			for (int32 Channel = 0; Channel < NumChannels; ++Channel)
			{
				OutAudio[Channel] = Biquad[Channel].ProcessAudio(InAudio[Channel]);
			}
		}
		else
		{
			// Pass through if disabled
			for (int32 Channel = 0; Channel < NumChannels; ++Channel)
			{
				OutAudio[Channel] = InAudio[Channel];
			}
		}
	}

	void FBiquadFilter::Reset()
	{
		for (int32 Channel = 0; Channel < NumChannels; ++Channel)
		{
			Biquad[Channel].Reset();
		}
	}

	void FBiquadFilter::SetParams(const EBiquadFilter::Type InFilterType, const float InCutoffFrequency, const float InBandwidth, const float InGainDB)
	{
		const float InCutoffFrequencyClamped = FMath::Max(InCutoffFrequency, 20.0f);

		if (InFilterType != FilterType ||
			InCutoffFrequencyClamped != InCutoffFrequency ||
			Bandwidth != InBandwidth ||
			GainDB != InGainDB)
		{
			FilterType = InFilterType;
			Frequency = InCutoffFrequency;
			Bandwidth = InBandwidth;
			GainDB = InGainDB;
			CalculateBiquadCoefficients();
		}
	}

	void FBiquadFilter::SetType(const EBiquadFilter::Type InFilterType)
	{
		if (FilterType != InFilterType)
		{
			FilterType = InFilterType;
			CalculateBiquadCoefficients();
		}
	}

	void FBiquadFilter::SetFrequency(const float InCutoffFrequency)
	{
		const float InCutoffFrequencyClamped = FMath::Max(InCutoffFrequency, 0.0f);
		if (Frequency != InCutoffFrequencyClamped)
		{
			Frequency = InCutoffFrequencyClamped;
			CalculateBiquadCoefficients();
		}
	}

	void FBiquadFilter::SetBandwidth(const float InBandwidth)
	{
		if (Bandwidth != InBandwidth)
		{
			Bandwidth = InBandwidth;
			CalculateBiquadCoefficients();
		}
	}

	void FBiquadFilter::SetGainDB(const float InGainDB)
	{
		if (GainDB != InGainDB)
		{
			GainDB = InGainDB;
			if (FilterType == EBiquadFilter::ParametricEQ)
			{
				CalculateBiquadCoefficients();
			}
		}
	}

	void FBiquadFilter::SetEnabled(const bool bInEnabled)
	{
		bEnabled = bInEnabled;
	}

	void FBiquadFilter::CalculateBiquadCoefficients()
	{
		static const float NaturalLog2 = 0.69314718055994530942f;

		const float Omega = 2.0f * PI * Frequency / SampleRate;
		const float Sn = FMath::Sin(Omega);
		const float Cs = FMath::Cos(Omega);

		const float Alpha = Sn * FMath::Sinh(0.5f * NaturalLog2 * Bandwidth * Omega / Sn);

		float a0;
		float a1;
		float a2;
		float b0;
		float b1;
		float b2;

		switch (FilterType)
		{
			default:
			case EBiquadFilter::Lowpass:
			{
				a0 = (1.0f - Cs) / 2.0f;
				a1 = (1.0f - Cs);
				a2 = (1.0f - Cs) / 2.0f;
				b0 = 1.0f + Alpha;
				b1 = -2.0f * Cs;
				b2 = 1.0f - Alpha;
			}
			break;

			case EBiquadFilter::Highpass:
			{
				a0 = (1.0f + Cs) / 2.0f;
				a1 = -(1.0f + Cs);
				a2 = (1.0f + Cs) / 2.0f;
				b0 = 1.0f + Alpha;
				b1 = -2.0f * Cs;
				b2 = 1.0f - Alpha;
			}
			break;

			case EBiquadFilter::Bandpass:
			{
				a0 = Alpha;
				a1 = 0.0f;
				a2 = -Alpha;
				b0 = 1.0f + Alpha;
				b1 = -2.0f * Cs;
				b2 = 1.0f - Alpha;
			}
			break;

			case EBiquadFilter::Notch:
			{
				a0 = 1.0f;
				a1 = -2.0f * Cs;
				a2 = 1.0f;
				b0 = 1.0f + Alpha;
				b1 = -2.0f * Cs;
				b2 = 1.0f - Alpha;
			}
			break;

			case EBiquadFilter::ParametricEQ:
			{
				const float Amp = FMath::Pow(10.0f, GainDB / 40.0f);
				const float Beta = FMath::Sqrt(2.0f * Amp);

				a0 = 1.0f + (Alpha * Amp);
				a1 = -2.0f * Cs;
				a2 = 1.0f - (Alpha * Amp);
				b0 = 1.0f + (Alpha / Amp);
				b1 = -2.0f * Cs;
				b2 = 1.0f - (Alpha / Amp);
			}
			break;

			case EBiquadFilter::AllPass:
			{
				a0 = 1.0f - Alpha;
				a1 = -2.0f * Cs;
				a2 = 1.0f + Alpha;
				b0 = 1.0f + Alpha;
				b1 = -2.0f * Cs;
				b2 = 1.0f - Alpha;
			}
			break;
		}

		a0 /= b0;
		a1 /= b0;
		a2 /= b0;
		b1 /= b0;
		b2 /= b0;

		for (int32 Channel = 0; Channel < NumChannels; ++Channel)
		{
			Biquad[Channel].A0 = a0;
			Biquad[Channel].A1 = a1;
			Biquad[Channel].A2 = a2;
			Biquad[Channel].B1 = b1;
			Biquad[Channel].B2 = b2;
		}
	}

	static const float MaxFilterFreq = 18000.0f;
	static const float MinFilterFreq = 80.0f;

	IFilter::IFilter()
		: VoiceId(0)
		, SampleRate(44100.0f)
		, NumChannels(1)
		, Frequency(MaxFilterFreq)
		, BaseFrequency(MaxFilterFreq)
		, ModFrequency(0.0f)
		, ExternalModFrequency(0.0f)
		, Q(1.5f)
		, ModQ(0.0f)
		, BaseQ(1.5f)
		, ExternalModQ(0.0f)
		, FilterType(EFilter::LowPass)
		, ModMatrix(nullptr)
		, bChanged(false)
	{
	}

	IFilter::~IFilter()
	{
	}

	void IFilter::Init(const float InSampleRate, const int32 InNumChannels, const int32 InVoiceId, FModulationMatrix* InModMatrix)
	{
		VoiceId = InVoiceId;
		SampleRate = InSampleRate;
		NumChannels = FMath::Min(MaxFilterChannels, InNumChannels);

		ModMatrix = InModMatrix;
		if (ModMatrix)
		{
			ModCutoffFrequencyDest = ModMatrix->CreatePatchDestination(VoiceId, 1, 100.0f);
			ModQDest = ModMatrix->CreatePatchDestination(VoiceId, 1, 10.0f);

#if MOD_MATRIX_DEBUG_NAMES
			ModCutoffFrequencyDest.Name = TEXT("ModCutoffFrequencyDest");
			ModQDest.Name = TEXT("ModQDest");
#endif
		}
	}

	void IFilter::SetFrequency(const float InCutoffFrequency)
	{
		if (BaseFrequency != InCutoffFrequency)
		{
			BaseFrequency = InCutoffFrequency;
			bChanged = true;
		}
	}

	void IFilter::SetFrequencyMod(const float InModFrequency)
	{
		if (ExternalModFrequency != InModFrequency)
		{	
			ExternalModFrequency = InModFrequency;		
			bChanged = true;
		}
	}

	void IFilter::SetQ(const float InQ)
	{
		if (BaseQ != InQ)
		{
			BaseQ = InQ;
			bChanged = true;
		}
	}

	void IFilter::SetQMod(const float InModQ)
	{
		if (ExternalModQ != InModQ)
		{
			ExternalModQ = InModQ;
			bChanged = true;
		}
	}

	void IFilter::SetFilterType(const EFilter::Type InFilterType)
	{
		FilterType = InFilterType;
	}

	void IFilter::Update()
	{
		if (ModMatrix)
		{
			bChanged |= ModMatrix->GetDestinationValue(VoiceId, ModCutoffFrequencyDest, ModFrequency);
			bChanged |= ModMatrix->GetDestinationValue(VoiceId, ModQDest, ModQ);
		}

		if (bChanged)
		{
			bChanged = false;

			Frequency = FMath::Clamp(BaseFrequency * GetFrequencyMultiplier(ModFrequency + ExternalModFrequency), 80.0f, 18000.0f);
			Q = BaseQ + ModQ + ExternalModQ;
		}
	}

	// One pole filter

	FOnePoleFilter::FOnePoleFilter()
		: A0(0.0f)
		, Z1(nullptr)
	{

	}

	FOnePoleFilter::~FOnePoleFilter()
	{
		if (Z1)
		{
			delete[] Z1;
		}
	}

	void FOnePoleFilter::Init(const float InSampleRate, const int32 InNumChannels, const int32 InVoiceId, FModulationMatrix* InModMatrix)
	{
		IFilter::Init(InSampleRate, InNumChannels, InVoiceId, InModMatrix);

		if (Z1)
		{
			delete[] Z1;
		}
		Z1 = new float[NumChannels];
		Reset();
	}

	void FOnePoleFilter::Reset()
	{
		FMemory::Memzero(Z1, sizeof(float)*NumChannels);
	}

	void FOnePoleFilter::Update()
	{
		// Call base class to get an updated frequency
		IFilter::Update();

		const float G = GetGCoefficient();
		A0 = G / (1.0f + G);
	}

	void FOnePoleFilter::ProcessAudio(const float* InSamples, float* OutSamples)
	{
		for (int32 Channel = 0; Channel < NumChannels; ++Channel)
		{
			const float Vn = (InSamples[Channel] - Z1[Channel]) * A0;
			const float LPF = Vn + Z1[Channel];
			Z1[Channel] = Vn + LPF;

			OutSamples[Channel] = (FilterType == EFilter::HighPass) ? InSamples[Channel] - LPF : LPF;
		}
	}

	FStateVariableFilter::FStateVariableFilter()
		: InputScale(1.0f)
		, A0(1.0f)
		, Feedback(1.0f)
		, BandStopParam(0.5f)
	{
	}

	FStateVariableFilter::~FStateVariableFilter()
	{
	}

	void FStateVariableFilter::Init(const float InSampleRate, const int32 InNumChannels, const int32 InVoiceId, FModulationMatrix* InModMatrix)
	{
		IFilter::Init(InSampleRate, InNumChannels, InVoiceId, InModMatrix);

		FilterState.Reset();
		FilterState.AddDefaulted(NumChannels);
		Reset();
	}

	void FStateVariableFilter::SetBandStopControl(const float InBandStop)
	{
		BandStopParam = FMath::Clamp(InBandStop, 0.0f, 1.0f);
	}

	void FStateVariableFilter::Reset()
	{
		FMemory::Memzero(FilterState.GetData(), sizeof(FFilterState) * NumChannels);
	}

	void FStateVariableFilter::Update()
	{
		IFilter::Update();

		float FinalQ = FMath::Clamp(Q, 1.0f, 10.0f);
		FinalQ = FMath::Lerp(0.5f, 25.0f, (FinalQ - 1.0f) / 9.0f);

		const float G = GetGCoefficient();
		const float Dampening = 0.5f / FinalQ;

		InputScale = 1.0f / (1.0f + 2.0f*Dampening*G + G*G);
		A0 = G;
		Feedback = 2.0f * Dampening + G;
	}

	void FStateVariableFilter::ProcessAudio(const float* InSamples, float* OutSamples)
	{
		for (int32 Channel = 0; Channel < NumChannels; ++Channel)
		{
			const float HPF = InputScale * (InSamples[Channel] - Feedback * FilterState[Channel].Z1_1 - FilterState[Channel].Z1_2);
			float BPF = Audio::FastTanh(A0 * HPF + FilterState[Channel].Z1_1);

			const float LPF = A0 * BPF + FilterState[Channel].Z1_2;
			const float Dampening = 0.5f / Q;
			const float BSF = BandStopParam * HPF + (1.0f - BandStopParam) * LPF;

			FilterState[Channel].Z1_1 = A0 * HPF + BPF;
			FilterState[Channel].Z1_2 = A0 * BPF + LPF;

			switch (FilterType)
			{
				default:
				case EFilter::LowPass:
					OutSamples[Channel] = LPF;
					break;

				case EFilter::HighPass:
					OutSamples[Channel] = HPF;
					break;

				case EFilter::BandPass:
					OutSamples[Channel] = BPF;
					break;

				case EFilter::BandStop:
					OutSamples[Channel] = BSF;
					break;
			}
		}
	}

	FLadderFilter::FLadderFilter()
		: K(0.0f)
		, Gamma(0.0f)
		, Alpha(0.0f)
		, PassBandGainCompensation(0.0f)
	{
		FMemory::Memzero(Factors, sizeof(float) * ARRAY_COUNT(Beta));
		FMemory::Memzero(Beta, sizeof(float) * ARRAY_COUNT(Beta));
	}

	FLadderFilter::~FLadderFilter()
	{

	}

	void FLadderFilter::Init(const float InSampleRate, const int32 InNumChannels, const int32 InVoiceId, FModulationMatrix* InModMatrix)
	{
		IFilter::Init(InSampleRate, InNumChannels, InVoiceId, InModMatrix);

		for (int32 i = 0; i < 4; ++i)
		{
			OnePoleFilters[i].Init(InSampleRate, InNumChannels);
			OnePoleFilters[i].SetFilterType(EFilter::LowPass);
		}
	}

	void FLadderFilter::Reset()
	{
		for (int32 i = 0; i < 4; ++i)
		{
			OnePoleFilters[i].Reset();
			OnePoleFilters[i].Reset();
		}
	}

	void FLadderFilter::Update()
	{
		IFilter::Update();

		// Compute feedforward coefficient to use on all LPFs
		const float G = GetGCoefficient();
		const float FeedforwardCoeff = G / (1.0f + G);

		Gamma = FeedforwardCoeff * FeedforwardCoeff * FeedforwardCoeff * FeedforwardCoeff;
		Alpha = 1.0f / (1.0f + K * Gamma);

		Beta[0] = FeedforwardCoeff * FeedforwardCoeff * FeedforwardCoeff;
		Beta[1] = FeedforwardCoeff * FeedforwardCoeff;
		Beta[2] = FeedforwardCoeff;
		Beta[3] = 1.0f;

		const float Div = 1.0f + FeedforwardCoeff;
		for (int32 i = 0; i < 4; ++i)
		{
			OnePoleFilters[i].SetCoefficient(FeedforwardCoeff);
			Beta[i] /= Div;
		}

		// Setup LPF factors based on filter type
		switch (FilterType)
		{
			default:
			case EFilter::LowPass:
				Factors[0] = 0.0f;
				Factors[1] = 0.0f;
				Factors[2] = 0.0f;
				Factors[3] = 0.0f;
				Factors[4] = 1.0f;
				break;

			case EFilter::BandPass:
				Factors[0] = 0.0f;
				Factors[1] = 0.0f;
				Factors[2] = 4.0f;
				Factors[3] = -8.0f;
				Factors[4] = 4.0f;
				break;

			case EFilter::HighPass:
				Factors[0] = 1.0f;
				Factors[1] = -4.0f;
				Factors[2] = 6.0f;
				Factors[3] = -4.0f;
				Factors[4] = 1.0f;
				break;
		}
	}

	void FLadderFilter::SetQ(const float InQ)
	{
		Q = FMath::Clamp(InQ, 1.0f, 10.0f);
		K = 3.88f * (Q - 1.0f) / 9.0f + 0.1f;
	}

	void FLadderFilter::SetPassBandGainCompensation(const float InPassBandGainCompensation)
	{
		PassBandGainCompensation = InPassBandGainCompensation;
	}

	void FLadderFilter::ProcessAudio(const float* InSamples, float* OutSamples)
	{
		// Compute input into first LPF
		float U[MaxFilterChannels];

		for (int32 Channel = 0; Channel < NumChannels; ++Channel)
		{
			float Sigma = 0.0f;
			for (int32 i = 0; i < 4; ++i)
			{
				Sigma += OnePoleFilters[i].GetState(Channel) * Beta[i];
			}

			float InSample = InSamples[Channel];
			InSample *= 1.0f + PassBandGainCompensation * K;
			U[Channel] = FMath::Min(Audio::FastTanh((InSample - K * Sigma)* Alpha), 1.0f);
		}

		// Feed U into first filter, then cascade down
		float OutputFilter0[MaxFilterChannels];
		float OutputFilter1[MaxFilterChannels];
		float OutputFilter2[MaxFilterChannels];
		float OutputFilter3[MaxFilterChannels];

		OnePoleFilters[0].ProcessAudio(U, OutputFilter0);
		OnePoleFilters[1].ProcessAudio(OutputFilter0, OutputFilter1);
		OnePoleFilters[2].ProcessAudio(OutputFilter1, OutputFilter2);
		OnePoleFilters[3].ProcessAudio(OutputFilter2, OutputFilter3);

		for (int32 Channel = 0; Channel < NumChannels; ++Channel)
		{
			OutSamples[Channel] =  Factors[0] * U[Channel];
			OutSamples[Channel] += Factors[1] * OutputFilter0[Channel];
			OutSamples[Channel] += Factors[2] * OutputFilter1[Channel];
			OutSamples[Channel] += Factors[3] * OutputFilter2[Channel];
			OutSamples[Channel] += Factors[4] * OutputFilter3[Channel];
		}
	}

}
