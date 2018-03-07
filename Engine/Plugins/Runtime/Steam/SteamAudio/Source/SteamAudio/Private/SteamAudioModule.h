//
// Copyright (C) Valve Corporation. All rights reserved.
//

#pragma once

#include "ISteamAudioModule.h"
#include "PhononSpatialization.h"
#include "PhononOcclusion.h"
#include "PhononReverb.h"
#include "PhononPluginManager.h"
#include "AudioPluginUtilities.h"
#include "AudioDevice.h"

namespace SteamAudio
{
	/************************************************************************/
	/* Plugin Factories registered with Steam Audio Module                  */
	/************************************************************************/
	class FSpatializationPluginFactory : public IAudioSpatializationFactory
	{
	public:
		virtual FString GetDisplayName() override
		{
			static FString DisplayName = FString(TEXT("Steam Audio"));
			return DisplayName;
		}

		virtual bool SupportsPlatform(EAudioPlatform Platform) override
		{
			if (Platform == EAudioPlatform::Windows)
			{
				return true;
			}
			else
			{
				return false;
			}
		}

		virtual TAudioSpatializationPtr CreateNewSpatializationPlugin(FAudioDevice* OwningDevice) override;

		virtual bool HasCustomSpatializationSetting() const override { return true; }
	};

	class FReverbPluginFactory : public IAudioReverbFactory
	{
	public:
		virtual FString GetDisplayName() override
		{
			static FString DisplayName = FString(TEXT("Steam Audio"));
			return DisplayName;
		}

		virtual bool SupportsPlatform(EAudioPlatform Platform) override
		{
			if (Platform == EAudioPlatform::Windows)
			{
				return true;
			}
			else
			{
				return false;
			}
		}

		virtual TAudioReverbPtr CreateNewReverbPlugin(FAudioDevice* OwningDevice) override;

		virtual bool HasCustomReverbSetting() const override { return true; }
	};

	class FOcclusionPluginFactory : public IAudioOcclusionFactory
	{
	public:
		virtual FString GetDisplayName() override
		{
			static FString DisplayName = FString(TEXT("Steam Audio"));
			return DisplayName;
		}

		virtual bool SupportsPlatform(EAudioPlatform Platform) override
		{
			if (Platform == EAudioPlatform::Windows)
			{
				return true;
			}
			else
			{
				return false;
			}
		}

		virtual TAudioOcclusionPtr CreateNewOcclusionPlugin(FAudioDevice* OwningDevice) override;

		virtual bool HasCustomOcclusionSetting() const override { return true; }
	};

	/************************************************************************/
	/* FSteamAudioModule                                                    */
	/* Module interface for Steam Audio. Registers the Plugin Factories     */
	/* and handles the third party DLL.                                     */
	/************************************************************************/
	class FSteamAudioModule : public ISteamAudioModule
	{
	public:
		FSteamAudioModule();
		~FSteamAudioModule();

		virtual void StartupModule() override;
		virtual void ShutdownModule() override;
		//~ End IAudioPlugin

		/** 
		*  Returns pointer to each type of plugin factory with Steam Audio.
		*  returns nullptr if given an invalid parameter for PluginType.
		*/
		IAudioPluginFactory* GetPluginFactory(EAudioPlugin PluginType);

		/*
		 * Registers a given audio device to the Steam Audio Device.
		 * Every audio device running Steam Audio's reverb or occlusion manager
		 * requires a PhononPluginManager registered to it.
		 */
		void RegisterAudioDevice(FAudioDevice* AudioDeviceHandle);

		/*
		* Unregisters an audio device from the module.
		*/
		void UnregisterAudioDevice(FAudioDevice* AudioDeviceHandle);

	private:
		static void* PhononDllHandle;

		TArray<FAudioDevice*> RegisteredAudioDevices;

		//Factories:
		FSpatializationPluginFactory SpatializationPluginFactory;
		FReverbPluginFactory ReverbPluginFactory;
		FOcclusionPluginFactory OcclusionPluginFactory;
	};
}