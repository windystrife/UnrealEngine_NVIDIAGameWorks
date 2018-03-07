//
// Copyright (C) Valve Corporation. All rights reserved.
//

#include "PhononOcclusion.h"
#include "PhononOcclusionSourceSettings.h"
#include "PhononCommon.h"
#include "SteamAudioModule.h"
#include "ScopeLock.h"

//==================================================================================================================================================
// FPhononOcclusion
//==================================================================================================================================================

namespace SteamAudio
{
	FPhononOcclusion::FPhononOcclusion()
		: EnvironmentalRenderer(nullptr)
		, EnvironmentCriticalSectionHandle(nullptr)
	{
		InputAudioFormat.channelLayout = IPL_CHANNELLAYOUT_MONO;
		InputAudioFormat.channelLayoutType = IPL_CHANNELLAYOUTTYPE_SPEAKERS;
		InputAudioFormat.channelOrder = IPL_CHANNELORDER_INTERLEAVED;
		InputAudioFormat.numSpeakers = 1;
		InputAudioFormat.speakerDirections = nullptr;
		InputAudioFormat.ambisonicsOrder = -1;
		InputAudioFormat.ambisonicsNormalization = IPL_AMBISONICSNORMALIZATION_N3D;
		InputAudioFormat.ambisonicsOrdering = IPL_AMBISONICSORDERING_ACN;

		OutputAudioFormat.channelLayout = IPL_CHANNELLAYOUT_MONO;
		OutputAudioFormat.channelLayoutType = IPL_CHANNELLAYOUTTYPE_SPEAKERS;
		OutputAudioFormat.channelOrder = IPL_CHANNELORDER_INTERLEAVED;
		OutputAudioFormat.numSpeakers = 1;
		OutputAudioFormat.speakerDirections = nullptr;
		OutputAudioFormat.ambisonicsOrder = -1;
		OutputAudioFormat.ambisonicsNormalization = IPL_AMBISONICSNORMALIZATION_N3D;
		OutputAudioFormat.ambisonicsOrdering = IPL_AMBISONICSORDERING_ACN;
	}

	FPhononOcclusion::~FPhononOcclusion()
	{
	}

	void FPhononOcclusion::Initialize(const FAudioPluginInitializationParams InitializationParams)
	{
		DirectSoundSources.AddDefaulted(InitializationParams.NumSources);

		for (FDirectSoundSource& DirectSoundSource : DirectSoundSources)
		{
			DirectSoundSource.InBuffer.format = InputAudioFormat;
			DirectSoundSource.InBuffer.numSamples = InitializationParams.BufferLength;
			DirectSoundSource.InBuffer.interleavedBuffer = nullptr;
			DirectSoundSource.InBuffer.deinterleavedBuffer = nullptr;

			DirectSoundSource.OutBuffer.format = OutputAudioFormat;
			DirectSoundSource.OutBuffer.numSamples = InitializationParams.BufferLength;
			DirectSoundSource.OutBuffer.interleavedBuffer = nullptr;
			DirectSoundSource.OutBuffer.deinterleavedBuffer = nullptr;
		}
	}

	void FPhononOcclusion::OnInitSource(const uint32 SourceId, const FName& AudioComponentUserId, const uint32 NumChannels, UOcclusionPluginSourceSettingsBase* InSettings)
	{
		if (!EnvironmentalRenderer)
		{
			UE_LOG(LogSteamAudio, Error, TEXT("Unable to find environmental renderer for occlusion. Audio will not be occluded. Make sure to export the scene."));
			return;
		}

		UE_LOG(LogSteamAudio, Log, TEXT("Creating occlusion effect."));

		UPhononOcclusionSourceSettings* OcclusionSettings = CastChecked<UPhononOcclusionSourceSettings>(InSettings);
		DirectSoundSources[SourceId].bDirectAttenuation = OcclusionSettings->DirectAttenuation;
		DirectSoundSources[SourceId].bAirAbsorption = OcclusionSettings->AirAbsorption;
		DirectSoundSources[SourceId].DirectOcclusionMethod = OcclusionSettings->DirectOcclusionMethod;
		DirectSoundSources[SourceId].DirectOcclusionMode = OcclusionSettings->DirectOcclusionMode;
		DirectSoundSources[SourceId].Radius = OcclusionSettings->DirectOcclusionSourceRadius;

		InputAudioFormat.numSpeakers = OutputAudioFormat.numSpeakers = NumChannels;
		switch (NumChannels)
		{
			case 1: InputAudioFormat.channelLayout = OutputAudioFormat.channelLayout = IPL_CHANNELLAYOUT_MONO; break;
			case 2: InputAudioFormat.channelLayout = OutputAudioFormat.channelLayout = IPL_CHANNELLAYOUT_STEREO; break;
			case 4: InputAudioFormat.channelLayout = OutputAudioFormat.channelLayout = IPL_CHANNELLAYOUT_QUADRAPHONIC; break;
			case 6: InputAudioFormat.channelLayout = OutputAudioFormat.channelLayout = IPL_CHANNELLAYOUT_FIVEPOINTONE; break;
			case 8: InputAudioFormat.channelLayout = OutputAudioFormat.channelLayout = IPL_CHANNELLAYOUT_SEVENPOINTONE; break;
		}

		DirectSoundSources[SourceId].InBuffer.format = InputAudioFormat;
		DirectSoundSources[SourceId].OutBuffer.format = OutputAudioFormat;

		iplCreateDirectSoundEffect(EnvironmentalRenderer, InputAudioFormat, OutputAudioFormat, &(DirectSoundSources[SourceId].DirectSoundEffect));
	}

	void FPhononOcclusion::OnReleaseSource(const uint32 SourceId)
	{
		UE_LOG(LogSteamAudio, Log, TEXT("Destroying occlusion effect."));

		iplDestroyDirectSoundEffect(&(DirectSoundSources[SourceId].DirectSoundEffect));
	}

	void FPhononOcclusion::ProcessAudio(const FAudioPluginSourceInputData& InputData, FAudioPluginSourceOutputData& OutputData)
	{
		FDirectSoundSource& DirectSoundSource = DirectSoundSources[InputData.SourceId];

		if (!EnvironmentalRenderer)
		{
			FMemory::Memcpy(OutputData.AudioBuffer.GetData(), InputData.AudioBuffer->GetData(), InputData.AudioBuffer->Num() * sizeof(float));
			return;
		}

		DirectSoundSource.InBuffer.interleavedBuffer = InputData.AudioBuffer->GetData();
		DirectSoundSource.OutBuffer.interleavedBuffer = OutputData.AudioBuffer.GetData();

		{
			FScopeLock Lock(&DirectSoundSources[InputData.SourceId].CriticalSection);
			DirectSoundSource.Position = SteamAudio::UnrealToPhononIPLVector3(InputData.SpatializationParams->EmitterWorldPosition);
			DirectSoundSource.bNeedsUpdate = true;
		}

		IPLDirectSoundEffectOptions DirectSoundEffectOptions;
		DirectSoundEffectOptions.applyAirAbsorption = static_cast<IPLbool>(DirectSoundSources[InputData.SourceId].bAirAbsorption);
		DirectSoundEffectOptions.applyDistanceAttenuation = static_cast<IPLbool>(DirectSoundSources[InputData.SourceId].bDirectAttenuation);
		DirectSoundEffectOptions.directOcclusionMode = static_cast<IPLDirectOcclusionMode>(DirectSoundSources[InputData.SourceId].DirectOcclusionMode);

		iplApplyDirectSoundEffect(DirectSoundSource.DirectSoundEffect, DirectSoundSource.InBuffer, DirectSoundSource.DirectSoundPath, DirectSoundEffectOptions, DirectSoundSource.OutBuffer);
	}

	void FPhononOcclusion::UpdateDirectSoundSources(const FVector& ListenerPosition, const FVector& ListenerForward, const FVector& ListenerUp)
	{
		if (!EnvironmentalRenderer || !EnvironmentCriticalSectionHandle)
		{
			return;
		}

		FScopeLock EnvironmentLock(EnvironmentCriticalSectionHandle);

		for (FDirectSoundSource& DirectSoundSource : DirectSoundSources)
		{
			FScopeLock DirectSourceLock(&DirectSoundSource.CriticalSection);

			if (DirectSoundSource.bNeedsUpdate)
			{
				IPLDirectSoundPath DirectSoundPath = iplGetDirectSoundPath(EnvironmentalRenderer, SteamAudio::UnrealToPhononIPLVector3(ListenerPosition),
					SteamAudio::UnrealToPhononIPLVector3(ListenerForward, false), SteamAudio::UnrealToPhononIPLVector3(ListenerUp, false),
					DirectSoundSource.Position, DirectSoundSource.Radius * SteamAudio::SCALEFACTOR,
					static_cast<IPLDirectOcclusionMode>(DirectSoundSource.DirectOcclusionMode),
					static_cast<IPLDirectOcclusionMethod>(DirectSoundSource.DirectOcclusionMethod));

				DirectSoundSource.DirectSoundPath = DirectSoundPath;
				DirectSoundSource.bNeedsUpdate = false;
			}
		}
	}

	void FPhononOcclusion::SetEnvironmentalRenderer(IPLhandle InEnvironmentalRenderer)
	{
		EnvironmentalRenderer = InEnvironmentalRenderer;
	}

	void FPhononOcclusion::SetCriticalSectionHandle(FCriticalSection* CriticalSectionHandle)
	{
		EnvironmentCriticalSectionHandle = CriticalSectionHandle;
	}

}

//==================================================================================================================================================
// FDirectSoundSource
//==================================================================================================================================================

namespace SteamAudio
{
	FDirectSoundSource::FDirectSoundSource()
		: bNeedsUpdate(false)
		, bAirAbsorption(false)
		, bDirectAttenuation(false)
		, DirectOcclusionMethod(EIplDirectOcclusionMethod::RAYCAST)
		, DirectOcclusionMode(EIplDirectOcclusionMode::NONE)
		, DirectSoundEffect(nullptr)
	{
		memset(&DirectSoundPath, 0, sizeof(DirectSoundPath));
		Position.x = Position.y = Position.z = 0.0f;
	}
}
