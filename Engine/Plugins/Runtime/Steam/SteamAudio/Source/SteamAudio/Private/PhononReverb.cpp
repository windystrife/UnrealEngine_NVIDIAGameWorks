//
// Copyright (C) Valve Corporation. All rights reserved.
//

#include "PhononReverb.h"
#include "SteamAudioModule.h"
#include "SteamAudioSettings.h"
#include "PhononReverbSourceSettings.h"
#include "Sound/SoundEffectSubmix.h"
#include "Sound/SoundSubmix.h"
#include "DSP/Dsp.h"
#include "ScopeLock.h"
#include "PhononPluginManager.h"

#include "AudioDevice.h"

namespace SteamAudio
{
	//==============================================================================================================================================
	// FPhononReverb
	//==============================================================================================================================================

	FPhononReverb::FPhononReverb()
		: EnvironmentalRenderer(nullptr)
		, BinauralRenderer(nullptr)
		, IndirectBinauralEffect(nullptr)
		, IndirectPanningEffect(nullptr)
		, ReverbConvolutionEffect(nullptr)
		, AmbisonicsChannels(0)
		, IndirectOutDeinterleaved(nullptr)
		, CachedSpatializationMethod(EIplSpatializationMethod::PANNING)
		, EnvironmentalCriticalSectionHandle(nullptr)
	{
		const int32 IndirectImpulseResponseOrder = GetDefault<USteamAudioSettings>()->IndirectImpulseResponseOrder;

		InputAudioFormat.channelLayout = IPL_CHANNELLAYOUT_MONO;
		InputAudioFormat.channelLayoutType = IPL_CHANNELLAYOUTTYPE_SPEAKERS;
		InputAudioFormat.channelOrder = IPL_CHANNELORDER_INTERLEAVED;
		InputAudioFormat.numSpeakers = 1;
		InputAudioFormat.speakerDirections = nullptr;
		InputAudioFormat.ambisonicsOrder = -1;
		InputAudioFormat.ambisonicsNormalization = IPL_AMBISONICSNORMALIZATION_N3D;
		InputAudioFormat.ambisonicsOrdering = IPL_AMBISONICSORDERING_ACN;

		ReverbInputAudioFormat.channelLayout = IPL_CHANNELLAYOUT_STEREO;
		ReverbInputAudioFormat.channelLayoutType = IPL_CHANNELLAYOUTTYPE_SPEAKERS;
		ReverbInputAudioFormat.channelOrder = IPL_CHANNELORDER_INTERLEAVED;
		ReverbInputAudioFormat.numSpeakers = 2;
		ReverbInputAudioFormat.speakerDirections = nullptr;
		ReverbInputAudioFormat.ambisonicsOrder = -1;
		ReverbInputAudioFormat.ambisonicsNormalization = IPL_AMBISONICSNORMALIZATION_N3D;
		ReverbInputAudioFormat.ambisonicsOrdering = IPL_AMBISONICSORDERING_ACN;

		IndirectOutputAudioFormat.channelLayout = IPL_CHANNELLAYOUT_MONO;
		IndirectOutputAudioFormat.channelLayoutType = IPL_CHANNELLAYOUTTYPE_AMBISONICS;
		IndirectOutputAudioFormat.channelOrder = IPL_CHANNELORDER_DEINTERLEAVED;
		IndirectOutputAudioFormat.numSpeakers = (IndirectImpulseResponseOrder + 1) * (IndirectImpulseResponseOrder + 1);
		IndirectOutputAudioFormat.speakerDirections = nullptr;
		IndirectOutputAudioFormat.ambisonicsOrder = IndirectImpulseResponseOrder;
		IndirectOutputAudioFormat.ambisonicsNormalization = IPL_AMBISONICSNORMALIZATION_N3D;
		IndirectOutputAudioFormat.ambisonicsOrdering = IPL_AMBISONICSORDERING_ACN;

		// Assume stereo output - if wrong, will be dynamically changed in the mixer processing
		BinauralOutputAudioFormat.channelLayout = IPL_CHANNELLAYOUT_STEREO;
		BinauralOutputAudioFormat.channelLayoutType = IPL_CHANNELLAYOUTTYPE_SPEAKERS;
		BinauralOutputAudioFormat.channelOrder = IPL_CHANNELORDER_INTERLEAVED;
		BinauralOutputAudioFormat.numSpeakers = 2;
		BinauralOutputAudioFormat.speakerDirections = nullptr;
		BinauralOutputAudioFormat.ambisonicsOrder = -1;
		BinauralOutputAudioFormat.ambisonicsNormalization = IPL_AMBISONICSNORMALIZATION_N3D;
		BinauralOutputAudioFormat.ambisonicsOrdering = IPL_AMBISONICSORDERING_ACN;
	}

	FPhononReverb::~FPhononReverb()
	{
		for (auto& ReverbSource : ReverbSources)
		{
			if (ReverbSource.ConvolutionEffect)
			{
				iplDestroyConvolutionEffect(&ReverbSource.ConvolutionEffect);
			}
		}

		if (ReverbConvolutionEffect)
		{
			iplDestroyConvolutionEffect(&ReverbConvolutionEffect);
		}

		if (IndirectBinauralEffect)
		{
			iplDestroyAmbisonicsBinauralEffect(&IndirectBinauralEffect);
		}

		if (IndirectPanningEffect)
		{
			iplDestroyAmbisonicsPanningEffect(&IndirectPanningEffect);
		}

		if (BinauralRenderer)
		{
			iplDestroyBinauralRenderer(&BinauralRenderer);
		}

		if (IndirectOutDeinterleaved)
		{
			for (int32 i = 0; i < AmbisonicsChannels; ++i)
			{
				delete[] IndirectOutDeinterleaved[i];
			}
			delete[] IndirectOutDeinterleaved;
			IndirectOutDeinterleaved = nullptr;
		}
	}

	void FPhononReverb::Initialize(const FAudioPluginInitializationParams InitializationParams)
	{
		RenderingSettings.convolutionType = IPL_CONVOLUTIONTYPE_PHONON;
		RenderingSettings.frameSize = InitializationParams.BufferLength;
		RenderingSettings.samplingRate = InitializationParams.SampleRate;

		IPLHrtfParams HrtfParams;
		HrtfParams.hrtfData = nullptr;
		HrtfParams.loadCallback = nullptr;
		HrtfParams.lookupCallback = nullptr;
		HrtfParams.unloadCallback = nullptr;
		HrtfParams.numHrirSamples = 0;
		HrtfParams.type = IPL_HRTFDATABASETYPE_DEFAULT;

		iplCreateBinauralRenderer(SteamAudio::GlobalContext, RenderingSettings, HrtfParams, &BinauralRenderer);
		iplCreateAmbisonicsBinauralEffect(BinauralRenderer, IndirectOutputAudioFormat, BinauralOutputAudioFormat, &IndirectBinauralEffect);
		iplCreateAmbisonicsPanningEffect(BinauralRenderer, IndirectOutputAudioFormat, BinauralOutputAudioFormat, &IndirectPanningEffect);

		int32 IndirectImpulseResponseOrder = GetDefault<USteamAudioSettings>()->IndirectImpulseResponseOrder;
		AmbisonicsChannels = (IndirectImpulseResponseOrder + 1) * (IndirectImpulseResponseOrder + 1);
		IndirectOutDeinterleaved = new float*[AmbisonicsChannels];

		for (int32 i = 0; i < AmbisonicsChannels; ++i)
		{
			IndirectOutDeinterleaved[i] = new float[InitializationParams.BufferLength];
		}
		IndirectIntermediateBuffer.format = IndirectOutputAudioFormat;
		IndirectIntermediateBuffer.numSamples = InitializationParams.BufferLength;
		IndirectIntermediateBuffer.interleavedBuffer = nullptr;
		IndirectIntermediateBuffer.deinterleavedBuffer = IndirectOutDeinterleaved;

		DryBuffer.format = ReverbInputAudioFormat;
		DryBuffer.numSamples = InitializationParams.BufferLength;
		DryBuffer.interleavedBuffer = nullptr;
		DryBuffer.deinterleavedBuffer = nullptr;

		IndirectOutArray.SetNumZeroed(InitializationParams.BufferLength * BinauralOutputAudioFormat.numSpeakers);
		IndirectOutBuffer.format = BinauralOutputAudioFormat;
		IndirectOutBuffer.numSamples = InitializationParams.BufferLength;
		IndirectOutBuffer.interleavedBuffer = IndirectOutArray.GetData();
		IndirectOutBuffer.deinterleavedBuffer = nullptr;

		ReverbSources.SetNum(InitializationParams.NumSources);
		for (FReverbSource& ReverbSource : ReverbSources)
		{
			ReverbSource.InBuffer.format = InputAudioFormat;
			ReverbSource.InBuffer.numSamples = InitializationParams.BufferLength;
		}

		ReverbIndirectContribution = 1.0f;

		CachedSpatializationMethod = GetDefault<USteamAudioSettings>()->IndirectSpatializationMethod;
	}

	void FPhononReverb::OnInitSource(const uint32 SourceId, const FName& AudioComponentUserId, const uint32 NumChannels, UReverbPluginSourceSettingsBase* InSettings)
	{
		if (!EnvironmentalRenderer)
		{
			UE_LOG(LogSteamAudio, Error, TEXT("Unable to find environmental renderer for reverb. Reverb will not be applied. Make sure to export the scene."));
			return;
		}

		UE_LOG(LogSteamAudio, Log, TEXT("Creating reverb effect for %s"), *AudioComponentUserId.ToString().ToLower());

		UPhononReverbSourceSettings* Settings = static_cast<UPhononReverbSourceSettings*>(InSettings);
		FReverbSource& ReverbSource = ReverbSources[SourceId];

		ReverbSource.IndirectContribution = Settings->IndirectContribution;

		InputAudioFormat.numSpeakers = NumChannels;
		switch (NumChannels)
		{
			case 1: InputAudioFormat.channelLayout = IPL_CHANNELLAYOUT_MONO; break;
			case 2: InputAudioFormat.channelLayout = IPL_CHANNELLAYOUT_STEREO; break;
			case 4: InputAudioFormat.channelLayout = IPL_CHANNELLAYOUT_QUADRAPHONIC; break;
			case 6: InputAudioFormat.channelLayout = IPL_CHANNELLAYOUT_FIVEPOINTONE; break;
			case 8: InputAudioFormat.channelLayout = IPL_CHANNELLAYOUT_SEVENPOINTONE; break;
		}

		ReverbSource.InBuffer.format = InputAudioFormat;

		switch (Settings->IndirectSimulationType)
		{
		case EIplSimulationType::BAKED:
			iplCreateConvolutionEffect(EnvironmentalRenderer, TCHAR_TO_ANSI(*AudioComponentUserId.ToString().ToLower()), IPL_SIMTYPE_BAKED,
				InputAudioFormat, IndirectOutputAudioFormat, &ReverbSource.ConvolutionEffect);
			break;
		case EIplSimulationType::REALTIME:
			iplCreateConvolutionEffect(EnvironmentalRenderer, TCHAR_TO_ANSI(*AudioComponentUserId.ToString().ToLower()), IPL_SIMTYPE_REALTIME,
				InputAudioFormat, IndirectOutputAudioFormat, &ReverbSource.ConvolutionEffect);
			break;
		case EIplSimulationType::DISABLED:
		default:
			break;
		}
	}

	void FPhononReverb::OnReleaseSource(const uint32 SourceId)
	{
		UE_LOG(LogSteamAudio, Log, TEXT("Destroying reverb effect."));

		ReverbSources[SourceId].IndirectContribution = 1.0f;
		iplDestroyConvolutionEffect(&ReverbSources[SourceId].ConvolutionEffect);
	}

	void FPhononReverb::ProcessSourceAudio(const FAudioPluginSourceInputData& InputData, FAudioPluginSourceOutputData& OutputData)
	{
		if (!EnvironmentalRenderer || !EnvironmentalCriticalSectionHandle)
		{
			return;
		}

		FScopeLock EnvironmentLock(EnvironmentalCriticalSectionHandle);

		FReverbSource& ReverbSource = ReverbSources[InputData.SourceId];
		IPLVector3 Position = SteamAudio::UnrealToPhononIPLVector3(InputData.SpatializationParams->EmitterWorldPosition);

		if (ReverbSource.ConvolutionEffect)
		{
			ReverbSource.IndirectInArray.SetNumUninitialized(InputData.AudioBuffer->Num());
			for (int32 i = 0; i < InputData.AudioBuffer->Num(); ++i)
			{
				ReverbSource.IndirectInArray[i] = (*InputData.AudioBuffer)[i] * ReverbSource.IndirectContribution;
			}
			ReverbSource.InBuffer.interleavedBuffer = ReverbSource.IndirectInArray.GetData();

			iplSetDryAudioForConvolutionEffect(ReverbSource.ConvolutionEffect, Position, ReverbSource.InBuffer);
		}
	}

	void FPhononReverb::ProcessMixedAudio(const FSoundEffectSubmixInputData& InData, FSoundEffectSubmixOutputData& OutData)
	{
		if (!EnvironmentalRenderer || !EnvironmentalCriticalSectionHandle)
		{
			return;
		}

		FScopeLock EnvironmentLock(EnvironmentalCriticalSectionHandle);

		if (IndirectOutBuffer.format.numSpeakers != OutData.NumChannels)
		{
			iplDestroyAmbisonicsBinauralEffect(&IndirectBinauralEffect);
			iplDestroyAmbisonicsPanningEffect(&IndirectPanningEffect);

			IndirectOutBuffer.format.numSpeakers = OutData.NumChannels;
			switch (OutData.NumChannels)
			{
				case 1: IndirectOutBuffer.format.channelLayout = IPL_CHANNELLAYOUT_MONO; break;
				case 2: IndirectOutBuffer.format.channelLayout = IPL_CHANNELLAYOUT_STEREO; break;
				case 4: IndirectOutBuffer.format.channelLayout = IPL_CHANNELLAYOUT_QUADRAPHONIC; break;
				case 6: IndirectOutBuffer.format.channelLayout = IPL_CHANNELLAYOUT_FIVEPOINTONE; break;
				case 8: IndirectOutBuffer.format.channelLayout = IPL_CHANNELLAYOUT_SEVENPOINTONE; break;
			}

			IndirectOutArray.SetNumZeroed(OutData.AudioBuffer->Num());
			IndirectOutBuffer.interleavedBuffer = IndirectOutArray.GetData();

			iplCreateAmbisonicsBinauralEffect(BinauralRenderer, IndirectOutputAudioFormat, IndirectOutBuffer.format, &IndirectBinauralEffect);
			iplCreateAmbisonicsPanningEffect(BinauralRenderer, IndirectOutputAudioFormat, IndirectOutBuffer.format, &IndirectPanningEffect);
		}

		if (ReverbConvolutionEffect)
		{
			ReverbIndirectInArray.SetNumUninitialized(InData.AudioBuffer->Num());
			for (int32 i = 0; i < InData.AudioBuffer->Num(); ++i)
			{
				ReverbIndirectInArray[i] = (*InData.AudioBuffer)[i] * ReverbIndirectContribution;
			}

			DryBuffer.interleavedBuffer = ReverbIndirectInArray.GetData();
			iplSetDryAudioForConvolutionEffect(ReverbConvolutionEffect, ListenerPosition, DryBuffer);
		}

		iplGetMixedEnvironmentalAudio(EnvironmentalRenderer, ListenerPosition, ListenerForward, ListenerUp, IndirectIntermediateBuffer);

		switch (CachedSpatializationMethod)
		{
		case EIplSpatializationMethod::HRTF:
			iplApplyAmbisonicsBinauralEffect(IndirectBinauralEffect, IndirectIntermediateBuffer, IndirectOutBuffer);
			break;
		case EIplSpatializationMethod::PANNING:
			iplApplyAmbisonicsPanningEffect(IndirectPanningEffect, IndirectIntermediateBuffer, IndirectOutBuffer);
			break;
		}

		FMemory::Memcpy(OutData.AudioBuffer->GetData(), IndirectOutArray.GetData(), sizeof(float) * IndirectOutArray.Num());
	}

	void FPhononReverb::CreateReverbEffect()
	{
		check(EnvironmentalRenderer && EnvironmentalCriticalSectionHandle);

		FScopeLock Lock(EnvironmentalCriticalSectionHandle);

		ReverbIndirectContribution = GetDefault<USteamAudioSettings>()->IndirectContribution;
		switch (GetDefault<USteamAudioSettings>()->ReverbSimulationType)
		{
		case EIplSimulationType::BAKED:
			iplCreateConvolutionEffect(EnvironmentalRenderer, (IPLstring)"__reverb__", IPL_SIMTYPE_BAKED, ReverbInputAudioFormat, IndirectOutputAudioFormat,
				&ReverbConvolutionEffect);
			break;
		case EIplSimulationType::REALTIME:
			iplCreateConvolutionEffect(EnvironmentalRenderer, (IPLstring)"__reverb__", IPL_SIMTYPE_REALTIME, ReverbInputAudioFormat, IndirectOutputAudioFormat,
				&ReverbConvolutionEffect);
			break;
		case EIplSimulationType::DISABLED:
		default:
			break;
		}
	}

	void FPhononReverb::UpdateListener(const FVector& Position, const FVector& Forward, const FVector& Up)
	{
		ListenerPosition = SteamAudio::UnrealToPhononIPLVector3(Position);
		ListenerForward = SteamAudio::UnrealToPhononIPLVector3(Forward, false);
		ListenerUp = SteamAudio::UnrealToPhononIPLVector3(Up, false);
	}

	FSoundEffectSubmix* FPhononReverb::GetEffectSubmix(USoundSubmix* Submix)
	{
		USubmixEffectReverbPluginPreset* ReverbPluginPreset = NewObject<USubmixEffectReverbPluginPreset>(Submix, TEXT("Master Reverb Plugin Effect Preset"));
		auto Effect = static_cast<FSubmixEffectReverbPlugin*>(ReverbPluginPreset->CreateNewEffect());
		Effect->SetPhononReverbPlugin(this);
		return static_cast<FSoundEffectSubmix*>(Effect);
	}

	void FPhononReverb::SetEnvironmentalRenderer(IPLhandle InEnvironmentalRenderer)
	{
		EnvironmentalRenderer = InEnvironmentalRenderer;
	}

	void FPhononReverb::SetEnvironmentCriticalSection(FCriticalSection* CriticalSection)
	{
		EnvironmentalCriticalSectionHandle = CriticalSection;
	}

	//==============================================================================================================================================
	// FReverbSource
	//==============================================================================================================================================

	FReverbSource::FReverbSource()
		: ConvolutionEffect(nullptr)
		, IndirectContribution(1.0f)
	{
	}
}

//==================================================================================================================================================
// FSubmixEffectReverbPlugin
//==================================================================================================================================================

FSubmixEffectReverbPlugin::FSubmixEffectReverbPlugin()
	: PhononReverbPlugin(nullptr)
{}

void FSubmixEffectReverbPlugin::Init(const FSoundEffectSubmixInitData& InSampleRate)
{
}

void FSubmixEffectReverbPlugin::OnPresetChanged()
{
}

uint32 FSubmixEffectReverbPlugin::GetDesiredInputChannelCountOverride() const
{
	return 2;
}

void FSubmixEffectReverbPlugin::OnProcessAudio(const FSoundEffectSubmixInputData& InData, FSoundEffectSubmixOutputData& OutData)
{
	PhononReverbPlugin->ProcessMixedAudio(InData, OutData);
}

void FSubmixEffectReverbPlugin::SetPhononReverbPlugin(SteamAudio::FPhononReverb* InPhononReverbPlugin)
{
	PhononReverbPlugin = InPhononReverbPlugin;
}
