// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#include "OculusAudioDllManager.h"
#include "Paths.h"

void* FOculusAudioLibraryManager::OculusAudioDllHandle = nullptr;
uint32 FOculusAudioLibraryManager::NumInstances = 0;
bool FOculusAudioLibraryManager::bInitialized = false;

void FOculusAudioLibraryManager::Initialize()
{
	if (FOculusAudioLibraryManager::NumInstances == 0)
	{
		if (!LoadDll())
		{
			UE_LOG(LogAudio, Error, TEXT("Failed to load OVR Audio dll"));
			check(false);
			return;
		}
	}

	FOculusAudioLibraryManager::NumInstances++;
	if (!FOculusAudioLibraryManager::bInitialized)
	{
		// Check the version number
		int32 MajorVersionNumber;
		int32 MinorVersionNumber;
		int32 PatchNumber;

		// Initialize the OVR Audio SDK before making any calls to ovrAudio
		ovrResult Result = ovrAudio_Initialize();
		OVR_AUDIO_CHECK(Result, "Failed to initialize OVR Audio system");

		const char* OvrVersionString = ovrAudio_GetVersion(&MajorVersionNumber, &MinorVersionNumber, &PatchNumber);
		if (MajorVersionNumber != OVR_AUDIO_MAJOR_VERSION || MinorVersionNumber != OVR_AUDIO_MINOR_VERSION)
		{
			UE_LOG(LogAudio, Warning, TEXT("Using mismatched OVR Audio SDK Version! %d.%d vs. %d.%d"), OVR_AUDIO_MAJOR_VERSION, OVR_AUDIO_MINOR_VERSION, MajorVersionNumber, MinorVersionNumber);
			return;
		}
		FOculusAudioLibraryManager::bInitialized = true;
	}
}

void FOculusAudioLibraryManager::Shutdown()
{
	if (FOculusAudioLibraryManager::NumInstances == 0)
	{
		// This means we failed to load the OVR Audio module during initialization and there's nothing to shutdown.
		return;
	}

	FOculusAudioLibraryManager::NumInstances--;

	if (FOculusAudioLibraryManager::NumInstances == 0)
	{
		// Shutdown OVR audio
		ovrAudio_Shutdown();
		ReleaseDll();
		FOculusAudioLibraryManager::bInitialized = false;
	}
}

bool FOculusAudioLibraryManager::LoadDll()
{
	if (!FOculusAudioLibraryManager::OculusAudioDllHandle)
	{
		FString Path = FPaths::EngineDir() / FString::Printf(TEXT("Binaries/ThirdParty/Oculus/Audio/Win64/"));
		FPlatformProcess::PushDllDirectory(*Path);
		FOculusAudioLibraryManager::OculusAudioDllHandle = FPlatformProcess::GetDllHandle(*(Path + "ovraudio64.dll"));
		FPlatformProcess::PopDllDirectory(*Path);
		return FOculusAudioLibraryManager::OculusAudioDllHandle != nullptr;
	}
	return true;
}

void FOculusAudioLibraryManager::ReleaseDll()
{
	if (FOculusAudioLibraryManager::NumInstances == 0 && FOculusAudioLibraryManager::OculusAudioDllHandle)
	{
		FPlatformProcess::FreeDllHandle(OculusAudioDllHandle);
		FOculusAudioLibraryManager::OculusAudioDllHandle = nullptr;
	}
}