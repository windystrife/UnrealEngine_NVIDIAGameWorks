// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "UnrealAudioVoice.h"

namespace UAudio
{
	/** High-level unreal audio module for managing, playing, and updating audio in the unreal engine. */
	class UNREALAUDIO_API IUnrealAudioModule :  public IModuleInterface
	{
	public:

		/**
		 * Destructor.
		 */
		virtual ~IUnrealAudioModule() {}

		/**
		 * Initializes this object.
		 *
		 * @return	true if it succeeds, false if it fails.
		 */
		virtual bool Initialize()
		{
			return false;
		}

		/**
		 * Initializes this object.
		 *
		 * @param	DeviceModuleName	Name of the device module.
		 *
		 * @return	true if it succeeds, false if it fails.
		 */
		virtual bool Initialize(const FString& DeviceModuleName)
		{
			return false;
		}

		/** Updates this object. */
		virtual void Update()
		{
		}

		/** Shuts down this object and frees any resources it is using. */
		virtual void Shutdown()
		{
		}

		virtual int32 GetNumBackgroundTasks() const
		{
			return 0;
		}

		virtual void ConvertSound(const FString& FilePath, const FString& OutputFilePath, const FSoundFileConvertFormat& ConvertFormat)
		{

		}

		virtual void ConvertSound(const FString& FilePath, const FString& OutputFilePath)
		{

		}

		virtual TSharedPtr<ISoundFile> LoadSoundFile(const FName& Name, TArray<uint8>& InBulkData)
		{
			return nullptr;
		}

		virtual TSharedPtr<ISoundFile> LoadSoundFile(const FName& Path, bool bLoadAsync = true)
		{
			return nullptr;
		}

		virtual TSharedPtr<ISoundFile>	StreamSoundFile(const FName& Path, bool bLoadAsync = true)
		{
			return nullptr;
		}

		/** Returns the number of sound files currently streamed */
		virtual int32 GetNumSoundFilesStreamed() const
		{
			return 0;
		}

		/** Returns the number of sound files currently loaded */
		virtual int32 GetNumSoundFilesLoaded() const
		{
			return 0;
		}

		/** Returns the number of total bytes of sound files loaded */
		virtual int32 GetSoundFileNumBytes() const
		{
			return 0;
		}

		/** Returns the percentage of loaded memory as fraction of target memory limit */
		virtual float GetSoundFilePercentageOfTargetMemoryLimit() const
		{
			return 0.0f;
		}

		/** Logs sound file information */
		virtual void LogSoundFileMemoryInfo() const
		{
		}

		/**
		 * Emitter create.
		 *
		 * @return	An IEmitter object.
		 */
		virtual TSharedPtr<IEmitter> EmitterCreate()
		{
			return nullptr;
		}

		/**
		 * Voice create.
		 *
		 * @param	Params	A variable-length parameters list containing parameters.
		 *
		 * @return	An IVoice object.
		 */
		virtual TSharedPtr<IVoice> VoiceCreate(const FVoiceInitializationParams& Params)
		{
			return nullptr;
		}


		virtual class IUnrealAudioDeviceModule* GetDeviceModule()
		{
			return nullptr;
		}

	};
}



