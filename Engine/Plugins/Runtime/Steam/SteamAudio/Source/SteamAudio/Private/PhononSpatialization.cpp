//
// Copyright (C) Valve Corporation. All rights reserved.
//

#include "PhononSpatialization.h"
#include "PhononSpatializationSourceSettings.h"
#include "PhononCommon.h"

namespace SteamAudio
{
	//==============================================================================================================================================
	// FPhononSpatialization
	//==============================================================================================================================================

	FPhononSpatialization::FPhononSpatialization()
		: BinauralRenderer(nullptr)
	{
		InputAudioFormat.channelLayout = IPL_CHANNELLAYOUT_MONO;
		InputAudioFormat.channelLayoutType = IPL_CHANNELLAYOUTTYPE_SPEAKERS;
		InputAudioFormat.channelOrder = IPL_CHANNELORDER_INTERLEAVED;
		InputAudioFormat.numSpeakers = 1;
		InputAudioFormat.speakerDirections = nullptr;
		InputAudioFormat.ambisonicsOrder = -1;
		InputAudioFormat.ambisonicsNormalization = IPL_AMBISONICSNORMALIZATION_N3D;
		InputAudioFormat.ambisonicsOrdering = IPL_AMBISONICSORDERING_ACN;

		BinauralOutputAudioFormat.channelLayout = IPL_CHANNELLAYOUT_STEREO;
		BinauralOutputAudioFormat.channelLayoutType = IPL_CHANNELLAYOUTTYPE_SPEAKERS;
		BinauralOutputAudioFormat.channelOrder = IPL_CHANNELORDER_INTERLEAVED;
		BinauralOutputAudioFormat.numSpeakers = 2;
		BinauralOutputAudioFormat.speakerDirections = nullptr;
		BinauralOutputAudioFormat.ambisonicsOrder = -1;
		BinauralOutputAudioFormat.ambisonicsNormalization = IPL_AMBISONICSNORMALIZATION_N3D;
		BinauralOutputAudioFormat.ambisonicsOrdering = IPL_AMBISONICSORDERING_ACN;
	}

	FPhononSpatialization::~FPhononSpatialization()
	{
		if (BinauralRenderer)
		{
			iplDestroyBinauralRenderer(&BinauralRenderer);
		}
	}

	void FPhononSpatialization::Initialize(const FAudioPluginInitializationParams InitializationParams)
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

		BinauralSources.AddDefaulted(InitializationParams.NumSources);
		for (FBinauralSource& BinauralSource : BinauralSources)
		{
			BinauralSource.InBuffer.format = InputAudioFormat;
			BinauralSource.InBuffer.numSamples = InitializationParams.BufferLength;
			BinauralSource.InBuffer.interleavedBuffer = nullptr;
			BinauralSource.InBuffer.deinterleavedBuffer = nullptr;

			BinauralSource.OutArray.SetNumZeroed(InitializationParams.BufferLength * 2);
			BinauralSource.OutBuffer.format = BinauralOutputAudioFormat;
			BinauralSource.OutBuffer.numSamples = InitializationParams.BufferLength;
			BinauralSource.OutBuffer.interleavedBuffer = nullptr;
			BinauralSource.OutBuffer.deinterleavedBuffer = nullptr;
		}
	}

	void FPhononSpatialization::OnInitSource(const uint32 SourceId, const FName& AudioComponentUserId,
		USpatializationPluginSourceSettingsBase* InSettings)
	{
		UPhononSpatializationSourceSettings* SpatializationSettings = static_cast<UPhononSpatializationSourceSettings*>(InSettings);
		FBinauralSource& BinauralSource = BinauralSources[SourceId];

		UE_LOG(LogSteamAudio, Log, TEXT("Creating spatialization effect."));

		if (SpatializationSettings)
		{
			switch (SpatializationSettings->SpatializationMethod)
			{
			case EIplSpatializationMethod::HRTF:
				iplCreateBinauralEffect(BinauralRenderer, InputAudioFormat, BinauralOutputAudioFormat, &BinauralSource.BinauralEffect);
				break;
			case EIplSpatializationMethod::PANNING:
				iplCreatePanningEffect(BinauralRenderer, InputAudioFormat, BinauralOutputAudioFormat, &BinauralSource.PanningEffect);
				break;
			}

			BinauralSource.SpatializationMethod = SpatializationSettings->SpatializationMethod;
			BinauralSource.HrtfInterpolationMethod = SpatializationSettings->HrtfInterpolationMethod;
		}
		else
		{
			iplCreateBinauralEffect(BinauralRenderer, InputAudioFormat, BinauralOutputAudioFormat, &BinauralSource.BinauralEffect);
			BinauralSource.SpatializationMethod = EIplSpatializationMethod::HRTF;
			BinauralSource.HrtfInterpolationMethod = EIplHrtfInterpolationMethod::NEAREST;
		}
	}

	void FPhononSpatialization::OnReleaseSource(const uint32 SourceId)
	{
		FBinauralSource& BinauralSource = BinauralSources[SourceId];

		UE_LOG(LogSteamAudio, Log, TEXT("Destroying spatialization effect."));

		switch (BinauralSource.SpatializationMethod)
		{
		case EIplSpatializationMethod::HRTF:
			iplDestroyBinauralEffect(&BinauralSource.BinauralEffect);
			break;
		case EIplSpatializationMethod::PANNING:
			iplDestroyPanningEffect(&BinauralSource.PanningEffect);
			break;
		}
	}

	void FPhononSpatialization::ProcessAudio(const FAudioPluginSourceInputData& InputData, FAudioPluginSourceOutputData& OutputData)
	{
		FBinauralSource& BinauralSource = BinauralSources[InputData.SourceId];
		BinauralSource.InBuffer.interleavedBuffer = InputData.AudioBuffer->GetData();
		BinauralSource.OutBuffer.interleavedBuffer = OutputData.AudioBuffer.GetData();

		// Workaround. The directions passed to spatializer is not consistent with the
		// coordinate system of UE4, therefore special tranformation is performed here.
		// Review this change if further changes are made to the direction passed to the
		// spatializer.
		IPLVector3 RelativeDirection;
		RelativeDirection.x = InputData.SpatializationParams->EmitterPosition.Y;
		RelativeDirection.y = InputData.SpatializationParams->EmitterPosition.X;
		RelativeDirection.z = InputData.SpatializationParams->EmitterPosition.Z;

		switch (BinauralSource.SpatializationMethod)
		{
		case EIplSpatializationMethod::HRTF:
			iplApplyBinauralEffect(BinauralSource.BinauralEffect, BinauralSource.InBuffer, RelativeDirection, static_cast<IPLHrtfInterpolation>(BinauralSource.HrtfInterpolationMethod), BinauralSource.OutBuffer);
			break;
		case EIplSpatializationMethod::PANNING:
			iplApplyPanningEffect(BinauralSource.PanningEffect, BinauralSource.InBuffer, RelativeDirection, BinauralSource.OutBuffer);
			break;
		}
	}

	bool FPhononSpatialization::IsSpatializationEffectInitialized() const
	{
		return true;
	}

	//==============================================================================================================================================
	// FBinauralSource
	//==============================================================================================================================================

	FBinauralSource::FBinauralSource()
		: BinauralEffect(nullptr)
		, PanningEffect(nullptr)
		, SpatializationMethod(EIplSpatializationMethod::HRTF)
		, HrtfInterpolationMethod(EIplHrtfInterpolationMethod::NEAREST)
	{
	}

	FBinauralSource::~FBinauralSource()
	{
		if (BinauralEffect)
		{
			iplDestroyBinauralEffect(&BinauralEffect);
		}

		if (PanningEffect)
		{
			iplDestroyPanningEffect(&PanningEffect);
		}
	}
}