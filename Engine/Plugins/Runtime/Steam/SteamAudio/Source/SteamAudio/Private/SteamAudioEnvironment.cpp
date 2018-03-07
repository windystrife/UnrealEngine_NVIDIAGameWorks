//
// Copyright (C) Valve Corporation. All rights reserved.
//

#include "SteamAudioEnvironment.h"
#include "PhononScene.h"
#include "PhononProbeVolume.h"
#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "Classes/Kismet/GameplayStatics.h"
#include "IPluginManager.h"
#include "EngineUtils.h"
#include "Regex.h"
#include "ScopeLock.h"
#include "HAL/PlatformProcess.h"
#include "Engine/StreamableManager.h"
#include "AudioDevice.h"
#include "PhononScene.h"
#include "SteamAudioSettings.h"
#include "PhononProbeVolume.h"

namespace SteamAudio
{

	FEnvironment::FEnvironment()
		: EnvironmentCriticalSection()
		, ComputeDevice(nullptr)
		, PhononScene(nullptr)
		, PhononEnvironment(nullptr)
		, EnvironmentalRenderer(nullptr)
		, ProbeManager(nullptr)
		, SimulationSettings()
		, RenderingSettings()
		, EnvironmentalOutputAudioFormat()
	{
	}

	FEnvironment::~FEnvironment()
	{
		Shutdown();
	}

	IPLhandle FEnvironment::Initialize(UWorld* World, FAudioDevice* InAudioDevice)
	{
		if (World == nullptr)
		{
			UE_LOG(LogSteamAudio, Error, TEXT("Unable to create Phonon environment: null World."));
			return nullptr;
		}

		if (InAudioDevice == nullptr)
		{
			UE_LOG(LogSteamAudio, Error, TEXT("Unable to create Phonon environment: null Audio Device."));
			return nullptr;
		}

		TArray<AActor*> PhononSceneActors;
		UGameplayStatics::GetAllActorsOfClass(World, APhononScene::StaticClass(), PhononSceneActors);

		if (PhononSceneActors.Num() == 0)
		{
			UE_LOG(LogSteamAudio, Error, TEXT("Unable to create Phonon environment: PhononScene not found. Be sure to add a PhononScene actor to your level and export the scene."));
			return nullptr;
		}
		else if (PhononSceneActors.Num() > 1)
		{
			UE_LOG(LogSteamAudio, Warning, TEXT("More than one PhononScene actor found in level. Arbitrarily choosing one. Ensure only one exists to avoid unexpected behavior."));
		}

		APhononScene* PhononSceneActor = Cast<APhononScene>(PhononSceneActors[0]);
		check(PhononSceneActor);

		if (PhononSceneActor->SceneData.Num() == 0)
		{
			UE_LOG(LogSteamAudio, Error, TEXT("Unable to create Phonon environment: PhononScene actor does not have scene data. Be sure to export the scene."));
			return nullptr;
		}

		int32 IndirectImpulseResponseOrder = GetDefault<USteamAudioSettings>()->IndirectImpulseResponseOrder;
		float IndirectImpulseResponseDuration = GetDefault<USteamAudioSettings>()->IndirectImpulseResponseDuration;

		SimulationSettings.maxConvolutionSources = GetDefault<USteamAudioSettings>()->MaxSources;
		SimulationSettings.numBounces = GetDefault<USteamAudioSettings>()->RealtimeBounces;
		SimulationSettings.numDiffuseSamples = GetDefault<USteamAudioSettings>()->RealtimeSecondaryRays;
		SimulationSettings.numRays = GetDefault<USteamAudioSettings>()->RealtimeRays;
		SimulationSettings.ambisonicsOrder = IndirectImpulseResponseOrder;
		SimulationSettings.irDuration = IndirectImpulseResponseDuration;
		SimulationSettings.sceneType = IPL_SCENETYPE_PHONON;

		RenderingSettings.convolutionType = IPL_CONVOLUTIONTYPE_PHONON;
		RenderingSettings.frameSize = InAudioDevice->GetBufferLength();
		RenderingSettings.samplingRate = InAudioDevice->GetSampleRate();

		EnvironmentalOutputAudioFormat.channelLayout = IPL_CHANNELLAYOUT_STEREO;
		EnvironmentalOutputAudioFormat.channelLayoutType = IPL_CHANNELLAYOUTTYPE_AMBISONICS;
		EnvironmentalOutputAudioFormat.channelOrder = IPL_CHANNELORDER_DEINTERLEAVED;
		EnvironmentalOutputAudioFormat.numSpeakers = (IndirectImpulseResponseOrder + 1) * (IndirectImpulseResponseOrder + 1);
		EnvironmentalOutputAudioFormat.speakerDirections = nullptr;
		EnvironmentalOutputAudioFormat.ambisonicsOrder = IndirectImpulseResponseOrder;
		EnvironmentalOutputAudioFormat.ambisonicsNormalization = IPL_AMBISONICSNORMALIZATION_N3D;
		EnvironmentalOutputAudioFormat.ambisonicsOrdering = IPL_AMBISONICSORDERING_ACN;

		IPLerror Error = IPLerror::IPL_STATUS_SUCCESS;

		iplLoadFinalizedScene(GlobalContext, SimulationSettings, PhononSceneActor->SceneData.GetData(), PhononSceneActor->SceneData.Num(),
			ComputeDevice, nullptr, &PhononScene);
		LogSteamAudioStatus(Error);

		Error = iplCreateProbeManager(&ProbeManager);
		LogSteamAudioStatus(Error);

		TArray<AActor*> PhononProbeVolumes;
		UGameplayStatics::GetAllActorsOfClass(World, APhononProbeVolume::StaticClass(), PhononProbeVolumes);

		for (AActor* PhononProbeVolumeActor : PhononProbeVolumes)
		{
			APhononProbeVolume* PhononProbeVolume = Cast<APhononProbeVolume>(PhononProbeVolumeActor);
			IPLhandle ProbeBatch = nullptr;
			Error = iplLoadProbeBatch(PhononProbeVolume->GetProbeBatchData(), PhononProbeVolume->GetProbeBatchDataSize(), &ProbeBatch);
			LogSteamAudioStatus(Error);

			iplAddProbeBatch(ProbeManager, ProbeBatch);

			ProbeBatches.Add(ProbeBatch);
		}

		Error = iplCreateEnvironment(GlobalContext, ComputeDevice, SimulationSettings, PhononScene, ProbeManager, &PhononEnvironment);
		LogSteamAudioStatus(Error);

		Error = iplCreateEnvironmentalRenderer(GlobalContext, PhononEnvironment, RenderingSettings, EnvironmentalOutputAudioFormat,
			nullptr, nullptr, &EnvironmentalRenderer);
		LogSteamAudioStatus(Error);

		return EnvironmentalRenderer;
	}

	void FEnvironment::Shutdown()
	{
		FScopeLock EnvironmentLock(&EnvironmentCriticalSection);
		if (ProbeManager)
		{
			for (IPLhandle ProbeBatch : ProbeBatches)
			{
				iplRemoveProbeBatch(ProbeManager, ProbeBatch);
				iplDestroyProbeBatch(&ProbeBatch);
			}

			ProbeBatches.Empty();

			iplDestroyProbeManager(&ProbeManager);
		}

		if (EnvironmentalRenderer)
		{
			iplDestroyEnvironmentalRenderer(&EnvironmentalRenderer);
		}

		if (PhononEnvironment)
		{
			iplDestroyEnvironment(&PhononEnvironment);
		}

		if (PhononScene)
		{
			iplDestroyScene(&PhononScene);
		}

		if (ComputeDevice)
		{
			iplDestroyComputeDevice(&ComputeDevice);
		}
	}

}