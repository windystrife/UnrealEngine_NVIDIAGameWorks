// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"
#include "HAL/ThreadSafeCounter.h"
#include "UnrealAudioTypes.h"
#include "GenericPlatform/GenericPlatformStackWalk.h"
#include "HAL/Runnable.h"
#include "HAL/ThreadSafeBool.h"
#include "UnrealAudioDeviceModule.h"
#include "UnrealAudioEmitterManager.h"
#include "UnrealAudioVoiceManager.h"
#include "UnrealAudioTests.h"

class Error;

#if ENABLE_UNREAL_AUDIO

DECLARE_LOG_CATEGORY_EXTERN(LogUnrealAudio, Log, All);

#define UA_SYSTEM_ERROR(ENUM, INFO)	(UAudio::OnSystemError(ENUM, INFO, FString(__FILE__), __LINE__))


namespace UAudio
{

	/**
	* EAudioThreadCommand
	* Commands sent from main thread to audio system thread.
	*/
	namespace EAudioThreadCommand
	{
		enum Type
		{
			NONE = 0,

			// Voice Commands
			VOICE_PLAY,
			VOICE_PAUSE,
			VOICE_STOP,
			VOICE_SET_VOLUME_SCALE,
			VOICE_SET_PITCH_SCALE,

			// Emitter Commands
			EMITTER_CREATE,
			EMITTER_RELEASE,
			EMITTER_SET_POSITION,

		};
	}

	/**
	* EMainThreadCommand
	* Commands sent from audio system thread to main thread.
	*/
	namespace EMainThreadCommand
	{
		enum Type
		{
			NONE = 0,

			// Voice Commands
			VOICE_DONE,
			VOICE_REAL,
			VOICE_VIRTUAL,
			VOICE_SUSPEND,
		};
	}

	/** 
	* FUnrealAudioModule 
	* Concrete implementation of IUnrealAudioModule
	*/
	class FUnrealAudioModule :	public IUnrealAudioModule,
								public FRunnable
	{
	public:

		FUnrealAudioModule();
		~FUnrealAudioModule() override;

		/************************************************************************/
		/* IUnrealAudioModule													*/
		/************************************************************************/

		bool Initialize() override;
		bool Initialize(const FString& DeviceModuleName) override;
		void Update() override;
		void Shutdown() override;
		int32 GetNumBackgroundTasks() const override;

		// Sound file API
		void ConvertSound(const FString& FilePath, const FString& OutputFilePath, const FSoundFileConvertFormat& ConvertFormat) override;
		void ConvertSound(const FString& FilePath, const FString& OutputFilePath) override;

		TSharedPtr<FSoundFileReader> CreateSoundFileReader();
		TSharedPtr<FSoundFileWriter> CreateSoundFileWriter();

		TSharedPtr<ISoundFile> LoadSoundFile(const FName& Name, TArray<uint8>& InBulkData) override;
		TSharedPtr<ISoundFile> LoadSoundFile(const FName& Path, bool bLoadAsync = true) override;
		TSharedPtr<ISoundFile> StreamSoundFile(const FName& Path, bool bLoadAsync = true) override;
		
		int32 GetNumSoundFilesStreamed() const override;
		int32 GetNumSoundFilesLoaded() const override;
		int32 GetSoundFileNumBytes() const override;
		float GetSoundFilePercentageOfTargetMemoryLimit() const override;
		void LogSoundFileMemoryInfo() const override;

		// Creation functions
		TSharedPtr<IEmitter> EmitterCreate() override;
		TSharedPtr<IVoice> VoiceCreate(const FVoiceInitializationParams& Params) override;

		class IUnrealAudioDeviceModule* GetDeviceModule() override;

		/************************************************************************/
		/* FRunnable implementation												*/
		/************************************************************************/

		bool Init();
		uint32 Run();
		void Stop();

		/************************************************************************/
		/* Internal Public API													*/
		/************************************************************************/

		FName GetDefaultDeviceModuleName() const;

		bool AudioDeviceCallback(struct FCallbackInfo& CallbackInfo);

		void GetPitchRange(float& OutMinPitch, float& OutMaxPitch) const;
		void IncrementBackgroundTaskCount();
		void DecrementBackgroundTaskCount();
		FEmitterManager& GetEmitterManager();
		FVoiceManager& GetVoiceManager();
		FSoundFileManager& GetSoundFileManager();

		void SendAudioThreadCommand(const FCommand& Command);
		void SendMainThreadCommand(const FCommand& Command);

		double GetCurrentTimeSec() const;

	private:

		bool InitializeInternal();
		
		bool InitializeAudioDevice();
		bool InitializeAudioSystem();

		void ShutdownAudioDevice();

		void ExecuteAudioThreadCommands();
		void ExecuteMainThreadCommands();

		bool LoadSoundFileLib();
		bool ShutdownSoundFileLib();

		/** Initializes unreal audio tests. */
		bool DeviceTestCallback(struct FCallbackInfo& CallbackInfo);

		static void InitializeSystemTests(IUnrealAudioModule* Module);
		static void InitializeDeviceTests(IUnrealAudioModule* Module);
		static void UpdateSystemTests();

		class IUnrealAudioDeviceModule* UnrealAudioDevice;

		FName ModuleName;

		/** The default settings for converting sound files. */
		FSoundFileConvertFormat DefaultConvertFormat;

		/** Number of background tasks in-flight. */
		FThreadSafeCounter NumBackgroundTasks;

		/** Emitter manager object. */
		FEmitterManager EmitterManager;

		/** Voice manager object. */
		FVoiceManager VoiceManager;

		/** Sound file manager object. */
		FSoundFileManager SoundFileManager;

		/** Handle to the sound file DLL. */
		void* SoundFileDllHandle;

		/** Whether or not to stop the system thread. */
		FThreadSafeBool bIsStoppingSystemThread;

		/** The current time in seconds of the audio system thread. */
		double AudioSystemTimeSec;

		/** The audio system thread handle. */
		FRunnableThread* SystemThread;

		/** Command queue to send thread commands from main thread to audio thread. */
		TCommandQueue<FCommand> AudioThreadCommandQueue;

		/** Command queue to send thread commands from audio thread to main thread. */
		TCommandQueue<FCommand> MainThreadCommandQueue;

		double SystemThreadUpdateTime;

		FThreadChecker MainThreadChecker;
		FThreadChecker AudioThreadChecker;
	};

	/**
	* Function called when an error occurs in the audio system code.
	* @param Error The error type that occurred.
	* @param ErrorDetails A string of specific details about the error.
	* @param FileName The file that the error occurred in.
	* @param LineNumber The line number that the error occurred in.
	*/
	static inline void OnSystemError(const ESystemError::Type Error, const FString& ErrorDetails, const FString& FileName, int32 LineNumber)
	{
		UE_LOG(LogUnrealAudio, Error, TEXT("Audio System Error: (%s) : %s (%s::%d)"), ESystemError::ToString(Error), *ErrorDetails, *FileName, LineNumber);
	}


}

#endif // #if ENABLE_UNREAL_AUDIO


