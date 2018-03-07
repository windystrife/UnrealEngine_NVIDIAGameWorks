// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/**
Implementation of IAudioDevice for CoreAudio
 
See https://developer.apple.com/library/mac/documentation/MusicAudio/Conceptual/CoreAudioOverview/Introduction/Introduction.html
 
*/

#include "UnrealAudioDeviceModule.h"
#include "ModuleManager.h"

/**
* CoreAudio System Headers
*/
#include <CoreAudio/AudioHardware.h>
#include <AudioToolbox/AudioToolbox.h>
#include <AudioUnit/AudioUnit.h>

#if ENABLE_UNREAL_AUDIO

// Used to toggle on white noise testing for CoreAudio output
#define UNREAL_AUDIO_TEST_WHITE_NOISE 0

static const TCHAR* GetCoreAudioError(OSStatus Result)
{
	switch (Result)
	{
		case kAudioHardwareNotRunningError:				return TEXT("kAudioHardwareNotRunningError");
		case kAudioHardwareUnspecifiedError:			return TEXT("kAudioHardwareUnspecifiedError");
		case kAudioHardwareUnknownPropertyError:		return TEXT("kAudioHardwareUnknownPropertyError");
		case kAudioHardwareBadPropertySizeError:		return TEXT("kAudioHardwareBadPropertySizeError");
		case kAudioHardwareIllegalOperationError:		return TEXT("kAudioHardwareIllegalOperationError");
		case kAudioHardwareBadObjectError:				return TEXT("kAudioHardwareBadObjectError");
		case kAudioHardwareBadDeviceError:				return TEXT("kAudioHardwareBadDeviceError");
		case kAudioHardwareBadStreamError:				return TEXT("kAudioHardwareBadStreamError");
		case kAudioHardwareUnsupportedOperationError:	return TEXT("kAudioHardwareUnsupportedOperationError");
		case kAudioDeviceUnsupportedFormatError:		return TEXT("kAudioDeviceUnsupportedFormatError");
		case kAudioDevicePermissionsError:				return TEXT("kAudioDevicePermissionsError");
		default:										return TEXT("Unknown CoreAudio Error");
	}
}

// Helper macro to report CoreAudio API errors
#define CORE_AUDIO_ERR(Status, Context)													\
	if (Status != noErr)														\
	{																			\
		const TCHAR* Err = GetCoreAudioError(Status);							\
		FString Message = FString::Printf(TEXT("%s: %s"), TEXT(Context), Err);	\
		UA_DEVICE_PLATFORM_ERROR(*Message);										\
		return false;															\
	}

// Helper macro to make a new global audio property address
#define NEW_GLOBAL_PROPERTY(SELECTOR)		\
	{										\
		SELECTOR,							\
		kAudioObjectPropertyScopeGlobal,	\
		kAudioObjectPropertyElementMaster	\
	}

// Helper macro to make a new output audio property address
#define NEW_OUTPUT_PROPERTY(SELECTOR)		\
	{										\
		SELECTOR,							\
		kAudioDevicePropertyScopeOutput,	\
		kAudioObjectPropertyElementMaster	\
	}

namespace UAudio
{
	class FUnrealAudioCoreAudio :   public IUnrealAudioDeviceModule,
									public FRunnable
	{
	public:
		FUnrealAudioCoreAudio();
		~FUnrealAudioCoreAudio();
		
		// IUnrealAudioDeviceModule
		virtual bool Initialize() override;
		virtual bool Shutdown() override;
		virtual bool GetDevicePlatformApi(EDeviceApi::Type & OutType) const override;
		virtual bool GetNumOutputDevices(uint32& OutNumDevices) const override;
		virtual bool GetOutputDeviceInfo(const uint32 DeviceIndex, FDeviceInfo& OutInfo) const override;
		virtual bool GetDefaultOutputDeviceIndex(uint32& OutDefaultIndex) const override;
		virtual bool StartStream() override;
		virtual bool StopStream() override;
		virtual bool ShutdownStream() override;
		virtual bool GetLatency(uint32& OutputDeviceLatency) const override;
		virtual bool GetFrameRate(uint32& OutFrameRate) const override;
		virtual bool OpenDevice(const FCreateStreamParams& Params) override;
		
		bool PerformCallback(AudioDeviceID DeviceId, const AudioBufferList* OutputBufferData);
		void SetOverloaded();

		// FRunnable, used to shutdown CoreAudio callback
		uint32 Run() override;

	private:

		// Helper Functions
		bool InitRunLoop();

		bool GetOutputDevices();
		bool GetDefaultOutputDevice();
		bool GetDeviceInfos();
		bool GetDeviceInfo(AudioDeviceID DeviceID, FDeviceInfo& DeviceInfo);
		bool GetDeviceName(AudioDeviceID DeviceID, FString& OutName);
		bool GetDeviceChannels(AudioDeviceID DeviceID, TArray<ESpeaker::Type>& OutChannels);
		bool GetDeviceChannelsForStereo(AudioDeviceID DeviceID, TArray<ESpeaker::Type>& OutChannels);
		bool GetDeviceChannelsForLayoutDescriptions(AudioChannelLayout* ChannelLayout, TArray<ESpeaker::Type>& OutChannels);
		bool GetDeviceChannelsForBitMap(UInt32 BitMap, TArray<ESpeaker::Type>& OutChannels);
		bool GetDeviceChannelsForLayoutTag(AudioChannelLayoutTag LayoutTag, TArray<ESpeaker::Type>& OutChannels);
		bool GetDeviceChannelsForChannelCount(UInt32 NumChannels, TArray<ESpeaker::Type>& OutChannels);
		bool GetDeviceFrameRates(AudioDeviceID DeviceID, TArray<uint32>& PossibleFramerates, uint32& DefaultFrameRate);
		bool GetDeviceLatency(AudioDeviceID DeviceID, UInt32& Latency);

		bool InitDeviceOutputId(UInt32 DeviceIndex);
		bool InitDeviceCallback(const FCreateStreamParams& Params);
		bool InitDeviceFrameRate(const UInt32 RequestedFrameRate, bool& bChanged);
		bool InitDeviceVirtualFormat(bool bSampleRateChanged);
		bool InitDevicePhysicalFormat();
		bool InitDeviceNumDeviceStreams();
		bool InitDeviceOverrunCallback();

		/** CoreAudio-specific state */
		struct FCoreAudioInfo
		{
			FCoreAudioInfo()
				: OutputDeviceId(INDEX_NONE)
				, OutputDeviceIndex(INDEX_NONE)
				, DefaultDeviceId(INDEX_NONE)
				, DefaultDeviceIndex(INDEX_NONE)
				, DeviceIOProcId(nullptr)
				, NumDeviceStreams(1)
				, StoppingCallbackCount(0)
				, bInternalStop(false)
			{
			}

			/** The open output device id */
			AudioDeviceID OutputDeviceId;

			/** The open output device index (into array of output devices) */
			UInt32 OutputDeviceIndex;

			/** The default device Id */
			AudioDeviceID DefaultDeviceId;

			/** The index into the device arrays which corresponds to the default output device. */
			int32 DefaultDeviceIndex;

			/** The device IO process Id */
			AudioDeviceIOProcID DeviceIOProcId;

			/** Array of output device info. This is built on initialize. */
			TArray<FDeviceInfo> OutputDevices;

			/** Array of CoreAudio DeviceID objects */
			TArray<AudioDeviceID> OutputDeviceIDs;

			/** 
			* Number of device streams the device has. Some devices
			* can have multiple output streams for multi-channel playback. 
			*/
			UInt32 NumDeviceStreams;

			/** Info about the callback passed to user */
			FCallbackInfo CallbackInfo;

			/** Condition used for signalling callback thread during shutdown */
			pthread_cond_t Condition;

			/** Mutex used to block stop thread during CoreAudio callback shutdown */
			pthread_mutex_t Mutex;

			/** Number used to track shutdown states for CoreAudio callback */
			int32 StoppingCallbackCount;

			/** Whether or not a stop was initiated from inside the CoreAudio callback */
			bool bInternalStop;
		};

		/** CoreAudio-specific state */
		FCoreAudioInfo CoreAudioInfo;

		/** Whether or not we've initialized the device */
		bool bInitialized;
	};

	/**
	* CoreAudio Callbacks
	*/

	/** A CoreAudio Property Listener to listen to the sample rate property when it gets changed. */
	static OSStatus SampleRatePropertyListener(	AudioObjectID DeviceId,
												UInt32 NumAddresses,
												const AudioObjectPropertyAddress* Addresses,
												void* SampleRatePtr)
	{
		Float64* SampleRate = (Float64*)SampleRatePtr;
		UInt32 DataSize = sizeof(Float64);
		AudioObjectPropertyAddress PropertyAddress = NEW_GLOBAL_PROPERTY(kAudioDevicePropertyNominalSampleRate);
		OSStatus Status = AudioObjectGetPropertyData(DeviceId, &PropertyAddress, 0, nullptr, &DataSize, SampleRate);
		return Status;
	}

	/** A CoreAudio Property Listener to listen to whether or not the audio callback is overloaded (resulting in hitching or buffer over/underun). */
	static OSStatus OverrunPropertyListener(AudioObjectID DeviceId,
											UInt32 NumAddresses,
											const AudioObjectPropertyAddress* Addresses,
											void* UserData)
	{
		for (UInt32 i = 0; i < NumAddresses; ++i)
		{
			if (Addresses[i].mSelector == kAudioDeviceProcessorOverload)
			{
				class UAudio::FUnrealAudioCoreAudio* UnrealAudioCoreAudio = (class UAudio::FUnrealAudioCoreAudio*)UserData;
				UnrealAudioCoreAudio->SetOverloaded();
			}
		}
		return kAudioHardwareNoError;
	}

	/** Core audio callback function called when the output device is ready for more data. */
	static OSStatus CoreAudioCallback(	AudioDeviceID DeviceId,
										const AudioTimeStamp* CurrentTimeStamp,
										const AudioBufferList* InputBufferData,
										const AudioTimeStamp* InputTime,
										AudioBufferList* OutputBufferData,
										const AudioTimeStamp* OutputTime,
										void* UserData)
	{
		// Get the user data and cast to our FUnrealAudioCoreAudio object
		FUnrealAudioCoreAudio* UnrealAudioCoreAudio = (FUnrealAudioCoreAudio*) UserData;

		// Call our callback function
		if (!UnrealAudioCoreAudio->PerformCallback(DeviceId, OutputBufferData))
		{
			// Something went wrong...
			return kAudioHardwareUnspecifiedError;
		}
		// Everything went cool...
		return kAudioHardwareNoError;
	}

	/**
	* FUnrealAudioCoreAudio Implementation
	*/

	FUnrealAudioCoreAudio::FUnrealAudioCoreAudio()
		: bInitialized(false)
	{
	}

	FUnrealAudioCoreAudio::~FUnrealAudioCoreAudio()
	{
		if (bInitialized)
		{
			Shutdown();
		}
	}

	bool FUnrealAudioCoreAudio::Initialize()
	{
		if (bInitialized)
		{
			return false;
		}
		bool bSuccess = InitRunLoop();
		bSuccess &= GetOutputDevices();
		bSuccess &= GetDefaultOutputDevice();
		bSuccess &= GetDeviceInfos();
		return (bInitialized = bSuccess);
	}

	bool FUnrealAudioCoreAudio::Shutdown()
	{
		if (StreamInfo.State != EStreamState::SHUTDOWN)
		{
			ShutdownStream();
		}
		return true;
	}
	
	bool FUnrealAudioCoreAudio::GetDevicePlatformApi(EDeviceApi::Type & OutType) const
	{
		OutType = EDeviceApi::CORE_AUDIO;
		return true;
	}
	
	bool FUnrealAudioCoreAudio::GetNumOutputDevices(uint32& OutNumDevices) const
	{
		OutNumDevices = CoreAudioInfo.OutputDevices.Num();
		return true;
	}
	
	bool FUnrealAudioCoreAudio::GetOutputDeviceInfo(const uint32 DeviceIndex, FDeviceInfo& OutInfo) const
	{
		if (DeviceIndex < CoreAudioInfo.OutputDevices.Num())
		{
			OutInfo = CoreAudioInfo.OutputDevices[DeviceIndex];
			return true;
		}
		return false;
	}
	
	bool FUnrealAudioCoreAudio::GetDefaultOutputDeviceIndex(uint32& OutDefaultIndex) const
	{
		OutDefaultIndex = CoreAudioInfo.DefaultDeviceIndex;
		return true;
	}

	bool FUnrealAudioCoreAudio::StartStream()
	{
		if (!bInitialized || StreamInfo.State == EStreamState::RUNNING)
		{
			return false;
		}

		// Start up the audio device stream
		OSStatus Status = AudioDeviceStart(CoreAudioInfo.OutputDeviceId, CoreAudioCallback);
		CORE_AUDIO_ERR(Status, "Failed to start audio device");

		StreamInfo.State = EStreamState::RUNNING;
		CoreAudioInfo.StoppingCallbackCount = 0;
		return true;
	}

	bool FUnrealAudioCoreAudio::StopStream()
	{
		if (StreamInfo.State == EStreamState::STOPPED)
		{
			return false;
		}

		// If this was a non-internal, stop, then set the count to 2
		// and then wait for the device callback to finish its current callback.
		if (!CoreAudioInfo.StoppingCallbackCount)
		{
			CoreAudioInfo.StoppingCallbackCount = 2;
			pthread_cond_wait(&CoreAudioInfo.Condition, &CoreAudioInfo.Mutex);
		}

		OSStatus Status = AudioDeviceStop(CoreAudioInfo.OutputDeviceId, CoreAudioCallback);
		CORE_AUDIO_ERR(Status, "Failed to stop audio device callback.");

		StreamInfo.State = EStreamState::STOPPED;

		if (StreamInfo.Thread)
		{
			delete StreamInfo.Thread;
			StreamInfo.Thread = nullptr;
		}
		return true;
	}
	
	bool FUnrealAudioCoreAudio::ShutdownStream()
	{
		if (StreamInfo.State == EStreamState::SHUTDOWN)
		{
			return false;
		}

		AudioObjectPropertyAddress Property = NEW_GLOBAL_PROPERTY(kAudioDeviceProcessorOverload);
		OSStatus Status = AudioObjectRemovePropertyListener(CoreAudioInfo.OutputDeviceId, &Property, OverrunPropertyListener, (void *)this);
		CORE_AUDIO_ERR(Status, "Failed to remove device overrun property listener");

		if (StreamInfo.State == EStreamState::RUNNING)
		{
			Status = AudioDeviceStop(CoreAudioInfo.OutputDeviceId, CoreAudioCallback);
			CORE_AUDIO_ERR(Status, "Failed to stop audio device");
		}

		Status = AudioDeviceDestroyIOProcID(CoreAudioInfo.OutputDeviceId, CoreAudioInfo.DeviceIOProcId);
		CORE_AUDIO_ERR(Status, "Failed to stop audio destroy IOProcID");

		pthread_cond_destroy(&CoreAudioInfo.Condition);
		StreamInfo.State = EStreamState::SHUTDOWN;
		return true;
	}
	
	bool FUnrealAudioCoreAudio::GetLatency(uint32& OutputDeviceLatency) const
	{
		OutputDeviceLatency = StreamInfo.DeviceInfo.Latency;
		return true;
	}
	
	bool FUnrealAudioCoreAudio::GetFrameRate(uint32& OutFrameRate) const
	{
		OutFrameRate = StreamInfo.FrameRate;
		return true;
	}
	
	bool FUnrealAudioCoreAudio::OpenDevice(const FCreateStreamParams& Params)
	{
		if (!bInitialized || StreamInfo.State != EStreamState::SHUTDOWN)
		{
			return false;
		}

		bool bSampleRateChanged = false;
		bool bSuccess = InitDeviceOutputId(Params.OutputDeviceIndex);
		bSuccess &= InitDeviceFrameRate(Params.FrameRate, bSampleRateChanged);
		bSuccess &= InitDeviceVirtualFormat(bSampleRateChanged);
		bSuccess &= InitDevicePhysicalFormat();
		bSuccess &= InitDeviceNumDeviceStreams();
		bSuccess &= InitDeviceCallback(Params);
		bSuccess &= InitDeviceOverrunCallback();
		StreamInfo.State = bSuccess ? EStreamState::STOPPED : EStreamState::SHUTDOWN;
		return bSuccess;
	}

	uint32 FUnrealAudioCoreAudio::Run()
	{
		// Just call stop stream
		StopStream();
		return 0;
	}

	bool FUnrealAudioCoreAudio::PerformCallback(AudioDeviceID DeviceId, const AudioBufferList* OutputBuffer)
	{
		if (StreamInfo.State == EStreamState::STOPPED || StreamInfo.State == EStreamState::STOPPING)
		{
			return true;
		}

		if (StreamInfo.State == EStreamState::SHUTDOWN)
		{
			UA_DEVICE_PLATFORM_ERROR(TEXT("Callback called while stream was closed."));
			return false;
		}

		if (CoreAudioInfo.StoppingCallbackCount > 3)
		{
			StreamInfo.State = EStreamState::STOPPING;
			// If this shutdown was initiated internally, we need to create a thread just to call StopStream
			// to avoid deadlocking with this callback
			if (CoreAudioInfo.bInternalStop)
			{
				StreamInfo.Thread = FRunnableThread::Create(this, TEXT("CoreAudio Shutdown Thread"), 0, TPri_AboveNormal);
			}
			else
			{
				pthread_cond_signal(&CoreAudioInfo.Condition);
			}
			return true;
		}

		AudioDeviceID DeviceID = CoreAudioInfo.OutputDeviceId;
		if (!CoreAudioInfo.StoppingCallbackCount)
		{
			FCallbackInfo& CallbackInfo = CoreAudioInfo.CallbackInfo;
			CallbackInfo.StreamTime = StreamInfo.StreamTime;

#if UNREAL_AUDIO_TEST_WHITE_NOISE
			for (uint32 Sample = 0; Sample < CallbackInfo.NumSamples; ++Sample)
			{
				CallbackInfo.OutBuffer[Sample] = 0.5f * FMath::FRandRange(-1.0f, 1.0f);
			}
#else // #if UNREAL_AUDIO_TEST_WHITE_NOISE
			memset((void*)CallbackInfo.OutBuffer, 0, CallbackInfo.NumSamples*sizeof(float));
			if (!StreamInfo.CallbackFunction(CallbackInfo))
			{
				UA_DEVICE_PLATFORM_ERROR(TEXT("Error occurred in user callback, stopping CoreAudio thread."));
				StreamInfo.State = EStreamState::STOPPING;
				CoreAudioInfo.StoppingCallbackCount = 1;
				CoreAudioInfo.bInternalStop = true;
			}
			// Clear status flags after the mix callback
			CallbackInfo.StatusFlags = 0;
#endif // #else // #if UNREAL_AUDIO_TEST_WHITE_NOISE
		}

		// If we are stopping, write zero to output buffers
		if (CoreAudioInfo.StoppingCallbackCount)
		{
			for (uint32 i = 0; i < CoreAudioInfo.NumDeviceStreams; ++i)
			{
				memset(OutputBuffer->mBuffers[i].mData, 0, OutputBuffer->mBuffers[i].mDataByteSize);
			}
			++CoreAudioInfo.StoppingCallbackCount;
		}
		else if (CoreAudioInfo.NumDeviceStreams == 1)
		{
			memcpy(OutputBuffer->mBuffers[0].mData, CoreAudioInfo.CallbackInfo.OutBuffer, OutputBuffer->mBuffers[0].mDataByteSize);
		}
		else
		{
			// TODO: Support streaming to multiple output streams
			// Report the an error (but only once)
			static bool bPrintedError = false;
			if (!bPrintedError)
			{
				bPrintedError = true;
				UA_DEVICE_PLATFORM_ERROR(TEXT("Streaming to multiple output streams not currently supported"));
			}
		}

		UpdateStreamTimeTick();
		return true;
	}

	void FUnrealAudioCoreAudio::SetOverloaded()
	{
		UA_DEVICE_PLATFORM_ERROR(TEXT("CoreAudio Overload (Buffer Underun or Overflow) occurred."));
		StreamInfo.CallbackInfo.StatusFlags = EStreamFlowStatus::OUTPUT_OVERFLOW;
	}

	bool FUnrealAudioCoreAudio::InitRunLoop()
	{
		CFRunLoopRef RunLoop = nullptr;
		AudioObjectPropertyAddress Property = NEW_GLOBAL_PROPERTY(kAudioHardwarePropertyRunLoop);
		OSStatus Status = AudioObjectSetPropertyData(kAudioObjectSystemObject, &Property, 0, nullptr, sizeof(CFRunLoopRef), &RunLoop);
		CORE_AUDIO_ERR(Status, "Failed to initialize run loop");
		return true;
	}

	bool FUnrealAudioCoreAudio::GetOutputDevices()
	{
		UInt32 DataSize;
		AudioObjectPropertyAddress Property = NEW_GLOBAL_PROPERTY(kAudioHardwarePropertyDevices);
		OSStatus Status = AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &Property, 0, nullptr, &DataSize);
		CORE_AUDIO_ERR(Status, "Failed to get size of devices property");

		UInt32 NumDevices = DataSize / sizeof(AudioDeviceID);
		AudioDeviceID Devices[NumDevices];

		Status = AudioObjectGetPropertyData(kAudioObjectSystemObject, &Property, 0, nullptr, &DataSize, (void*)&Devices);
		CORE_AUDIO_ERR(Status, "Failed to get device list");

		CoreAudioInfo.OutputDeviceIDs.Empty();
		for (UInt32 i = 0; i < NumDevices; ++i)
		{
			AudioDeviceID DeviceID = Devices[i];
			Property = NEW_OUTPUT_PROPERTY(kAudioDevicePropertyStreamConfiguration);
			Status = AudioObjectGetPropertyDataSize(DeviceID, &Property, 0, nullptr, &DataSize);
			CORE_AUDIO_ERR(Status, "Failed to get stream configuration size");

			AudioBufferList* BufferList = (AudioBufferList*)alloca(DataSize);

			Status = AudioObjectGetPropertyData(DeviceID, &Property, 0, nullptr, &DataSize, BufferList);
			CORE_AUDIO_ERR(Status, "Failed to get stream configuration");

			if (BufferList->mNumberBuffers != 0)
			{
				CoreAudioInfo.OutputDeviceIDs.Add(DeviceID);
			}
		}
		return true;
	}

	bool FUnrealAudioCoreAudio::GetDefaultOutputDevice()
	{
		UInt32 PropertySize = sizeof(AudioDeviceID);
		AudioObjectPropertyAddress Property = NEW_GLOBAL_PROPERTY(kAudioHardwarePropertyDefaultOutputDevice);
		OSStatus Result = AudioObjectGetPropertyData(kAudioObjectSystemObject, &Property, 0, nullptr, &PropertySize, &CoreAudioInfo.DefaultDeviceId);
		CORE_AUDIO_ERR(Result, "Failed to get default output device");
		return true;
	}

	bool FUnrealAudioCoreAudio::GetDeviceInfos()
	{
		for (int32 i = 0; i < CoreAudioInfo.OutputDeviceIDs.Num(); ++i)
		{
			AudioDeviceID DeviceID = CoreAudioInfo.OutputDeviceIDs[i];
			FDeviceInfo DeviceInfo;
			if (!GetDeviceInfo(DeviceID, DeviceInfo))
			{
				return false;
			}

			if (DeviceID == CoreAudioInfo.DefaultDeviceId)
			{
				DeviceInfo.bIsSystemDefault = true;
				CoreAudioInfo.DefaultDeviceIndex = i;
			}
			CoreAudioInfo.OutputDevices.Add(DeviceInfo);
		}
		return true;
	}

	bool FUnrealAudioCoreAudio::GetDeviceInfo(AudioDeviceID DeviceID, FDeviceInfo& DeviceInfo)
	{
		bool bSuccess = GetDeviceName(DeviceID, DeviceInfo.FriendlyName);
		bSuccess &=	GetDeviceChannels(DeviceID, DeviceInfo.Speakers);
		bSuccess &= GetDeviceFrameRates(DeviceID, DeviceInfo.PossibleFrameRates, DeviceInfo.FrameRate);
		bSuccess &= GetDeviceLatency(DeviceID, DeviceInfo.Latency);
		DeviceInfo.StreamFormat = EStreamFormat::FLT;
		DeviceInfo.NumChannels = DeviceInfo.Speakers.Num();
		return bSuccess;
	}

	bool FUnrealAudioCoreAudio::GetDeviceName(AudioDeviceID DeviceId, FString& OutName)
	{
		AudioObjectPropertyAddress Property = NEW_GLOBAL_PROPERTY(kAudioObjectPropertyManufacturer);

		CFStringRef ManufacturerName;
		UInt32 DataSize = sizeof(CFStringRef);
		OSStatus Status = AudioObjectGetPropertyData(DeviceId, &Property, 0, nullptr, &DataSize, &ManufacturerName);
		CORE_AUDIO_ERR(Status, "Failed to get device manufacturer name");

		// Convert the Apple CFStringRef to an Unreal FString
		TCHAR ManufacturerNameBuff[256];
		FPlatformString::CFStringToTCHAR(ManufacturerName, ManufacturerNameBuff);
		CFRelease(ManufacturerName);

		CFStringRef DeviceName;
		DataSize = sizeof(CFStringRef);
		Property.mSelector = kAudioObjectPropertyName;
		Status = AudioObjectGetPropertyData(DeviceId, &Property, 0, nullptr, &DataSize, &DeviceName);
		CORE_AUDIO_ERR(Status, "Failed to get device name");

		// Convert the Apple CFStringRef to an Unreal FString
		TCHAR DeviceNameBuff[256];
		FPlatformString::CFStringToTCHAR(DeviceName, DeviceNameBuff);
		CFRelease(DeviceName);

		OutName = FString::Printf(TEXT("%s - %s"), ManufacturerNameBuff, DeviceNameBuff);
		return true;
	}

	bool FUnrealAudioCoreAudio::GetDeviceChannels(AudioDeviceID DeviceID, TArray<ESpeaker::Type>& OutChannels)
	{
		// First get number of channels by getting the buffer list
		UInt32 PropertySize;
		AudioObjectPropertyAddress Property = NEW_OUTPUT_PROPERTY(kAudioDevicePropertyStreamConfiguration);
		OSStatus Status = AudioObjectGetPropertyDataSize(DeviceID, &Property, 0, nullptr, &PropertySize);
		CORE_AUDIO_ERR(Status, "Failed to get stream configuration property size");

		AudioBufferList* BufferList = (AudioBufferList*)alloca(PropertySize);
		Status = AudioObjectGetPropertyData(DeviceID, &Property, 0, nullptr, &PropertySize, BufferList);
		CORE_AUDIO_ERR(Status, "Failed to get stream configuration property");

		UInt32 NumChannels = 0;
		for (UInt32 i = 0; i < BufferList->mNumberBuffers; ++i)
		{
			NumChannels += BufferList->mBuffers[i].mNumberChannels;
		}

		if (NumChannels == 0)
		{
			UA_DEVICE_PLATFORM_ERROR(TEXT("Output device has 0 channels"));
			return false;
		}

		// CoreAudio has a different property for stereo speaker layouts
		if (NumChannels == 2)
		{
			return GetDeviceChannelsForStereo(DeviceID, OutChannels);
		}

		// Now CORE_AUDIO_ERR multi-channel layouts
		Property = NEW_OUTPUT_PROPERTY(kAudioDevicePropertyPreferredChannelLayout);
		if (!AudioObjectHasProperty(DeviceID, &Property))
		{
			return false;
		}

		// Bool used to try different methods of getting channel layout (which is surprisingly complex!)
		bool bSuccess = true;
		Status = AudioObjectGetPropertyDataSize(DeviceID, &Property, 0, nullptr, &PropertySize);
		CORE_AUDIO_ERR(Status, "Failed to get preferred channel layout size");

		AudioChannelLayout* ChannelLayout = (AudioChannelLayout*)alloca(PropertySize);
		Status = AudioObjectGetPropertyData(DeviceID, &Property, 0, nullptr, &PropertySize, ChannelLayout);
		CORE_AUDIO_ERR(Status, "Failed to get preferred channel layout");

		if (ChannelLayout->mChannelLayoutTag == kAudioChannelLayoutTag_UseChannelDescriptions)
		{
			bSuccess = GetDeviceChannelsForLayoutDescriptions(ChannelLayout, OutChannels);
		}
		else if (ChannelLayout->mChannelLayoutTag == kAudioChannelLayoutTag_UseChannelBitmap)
		{
			bSuccess = GetDeviceChannelsForBitMap(ChannelLayout->mChannelBitmap, OutChannels);
		}
		else
		{
			bSuccess = GetDeviceChannelsForLayoutTag(ChannelLayout->mChannelLayoutTag, OutChannels);
		}

		// If everything failed, then lets just get the number of channels and "guess" the layout
		if (!bSuccess)
		{
			bSuccess = GetDeviceChannelsForChannelCount(NumChannels, OutChannels);
		}

		return bSuccess;
	}

	bool FUnrealAudioCoreAudio::GetDeviceFrameRates(AudioDeviceID DeviceID, TArray<uint32>& InPossibleFrameRates, uint32& DefaultFrameRate)
	{
		AudioObjectPropertyAddress Property = NEW_OUTPUT_PROPERTY(kAudioDevicePropertyAvailableNominalSampleRates);

		UInt32 DataSize;
		OSStatus Status = AudioObjectGetPropertyDataSize(DeviceID, &Property, 0, nullptr, &DataSize);
		CORE_AUDIO_ERR(Status, "Failed to get nominal sample rates size");

		UInt32 NumSampleRates = DataSize / sizeof(AudioValueRange);
		AudioValueRange SampleRates[NumSampleRates];

		Status = AudioObjectGetPropertyData(DeviceID, &Property, 0, nullptr, &DataSize, &SampleRates);
		CORE_AUDIO_ERR(Status, "Failed to get nominal sample rates");

		// If we are given ranges of values for this device, we want to find the largest min value
		// and the smallest max value for sample rates, then pick good sample rates based on that
		double LargestMinSampleRateRange = 1.0f;
		double SmallestMaxSampleRateRange = DBL_MAX;
		bool bHasSampleRateRanges = false;
		for (uint32 i = 0; i < NumSampleRates; ++i)
		{
			const AudioValueRange& SampleRate = SampleRates[i];

			// If the min and max sample rate is the same, then just add this to the list of frame rates
			if (SampleRate.mMinimum == SampleRate.mMaximum)
			{
				uint32 FrameRate = (uint32)SampleRate.mMinimum;
				if (!InPossibleFrameRates.Contains(FrameRate))
				{
					InPossibleFrameRates.Add(FrameRate);
				}
			}
			else
			{
				bHasSampleRateRanges = true;
				if (SampleRate.mMinimum > LargestMinSampleRateRange)
				{
					LargestMinSampleRateRange = SampleRate.mMinimum;
				}

				if (SampleRate.mMaximum < SmallestMaxSampleRateRange)
				{
					SmallestMaxSampleRateRange = SampleRate.mMaximum;
				}
			}
		}

		if (bHasSampleRateRanges)
		{
			for (uint32 i = 0; i < MaxPossibleFrameRates; ++i)
			{
				uint32 PossibleFrameRate = InPossibleFrameRates[i];
				if (PossibleFrameRate >= LargestMinSampleRateRange && PossibleFrameRate <= SmallestMaxSampleRateRange)
				{
					InPossibleFrameRates.Add(PossibleFrameRate);
				}
			}
		}
		// Picking a good default framerate from the supported framerates
		if (InPossibleFrameRates.Contains(48000))
		{
			DefaultFrameRate = 48000;
		}
		else if (InPossibleFrameRates.Contains(44100))
		{
			DefaultFrameRate = 44100;
		}
		else
		{
			UA_DEVICE_PLATFORM_ERROR(TEXT("Audio device doesn't support 48k or 44.1k sample rates."));
			return false;
		}
		return true;
	}

	bool FUnrealAudioCoreAudio::InitDeviceOutputId(UInt32 DeviceIndex)
	{
		if (DeviceIndex >= CoreAudioInfo.OutputDevices.Num())
		{
			return false;
		}
		CoreAudioInfo.OutputDeviceId = CoreAudioInfo.OutputDeviceIDs[DeviceIndex];
		CoreAudioInfo.OutputDeviceIndex = DeviceIndex;
		return true;
	}

	bool FUnrealAudioCoreAudio::InitDeviceFrameRate(const UInt32 RequestedFrameRate, bool& bChanged)
	{
		bool bSupportedFrameRate = false;
		StreamInfo.FrameRate = RequestedFrameRate;
		FDeviceInfo& DeviceInfo = CoreAudioInfo.OutputDevices[CoreAudioInfo.OutputDeviceIndex];
		for (int32 i = 0; i < DeviceInfo.PossibleFrameRates.Num(); ++i)
		{
			if (RequestedFrameRate == DeviceInfo.PossibleFrameRates[i])
			{
				bSupportedFrameRate = true;
				break;
			}
		}

		if (!bSupportedFrameRate)
		{
			UA_DEVICE_PLATFORM_ERROR(TEXT("Requested frame rate is not supported by device, trying to use default."));
			StreamInfo.FrameRate = DeviceInfo.FrameRate;
		}

		// CORE_AUDIO_ERR what the sample rate of the device is currently set to
		Float64 NominalSampleRate;
		UInt32 DataSize = sizeof(Float64);
		AudioObjectPropertyAddress Property = NEW_OUTPUT_PROPERTY(kAudioDevicePropertyNominalSampleRate);
		OSStatus Status = AudioObjectGetPropertyData(CoreAudioInfo.OutputDeviceId, &Property, 0, nullptr, &DataSize, &NominalSampleRate);
		CORE_AUDIO_ERR(Status, "Failed to get nominal sample rate");

		// If the desired sample rate is different than the nominal sample rate, then we change it
		if (FMath::Abs(NominalSampleRate - (Float64)StreamInfo.FrameRate) > 1.0)
		{
			// Set a property listener to make sure we set the new sample rate correctly
			Float64 ReportedSampleRate = 0.0;
			AudioObjectPropertyAddress Property2 = NEW_GLOBAL_PROPERTY(kAudioDevicePropertyNominalSampleRate);
			Status = AudioObjectAddPropertyListener(CoreAudioInfo.OutputDeviceId, &Property2, SampleRatePropertyListener, (void*)&ReportedSampleRate);
			CORE_AUDIO_ERR(Status, "Failed to add a sample rate property listener");

			NominalSampleRate = (Float64)StreamInfo.FrameRate;
			Status = AudioObjectSetPropertyData(CoreAudioInfo.OutputDeviceId, &Property, 0, nullptr, DataSize, &NominalSampleRate);
			CORE_AUDIO_ERR(Status, "Failed to set a sample rate on device");

			// Wait until the sample rate has changed to the requested rate with a timeout
			UInt32 Counter = 0;
			const UInt32 Inc = 5000;
			const UInt32 Timeout = 5000000;
			bool bTimedout = false;
			while (ReportedSampleRate != NominalSampleRate)
			{
				Counter += Inc;
				if (Counter > Timeout)
				{
					bTimedout = true;
					break;
				}
				usleep(Inc);
			}

			Status = AudioObjectRemovePropertyListener(CoreAudioInfo.OutputDeviceId, &Property2, SampleRatePropertyListener, (void*)&ReportedSampleRate);
			CORE_AUDIO_ERR(Status, "Failed to remove the sample rate property listener");

			if (bTimedout)
			{
				UA_DEVICE_PLATFORM_ERROR(TEXT("Timed out while setting sample rate of audio device."));
				return false;
			}

			bChanged = true;
		}
		return true;
	}

	bool FUnrealAudioCoreAudio::InitDeviceVirtualFormat(bool bSampleRateChanged)
	{
		// Change the output stream virtual format if we need to
		AudioStreamBasicDescription Format;
		UInt32 DataSize = sizeof(AudioStreamBasicDescription);
		AudioObjectPropertyAddress Property = NEW_OUTPUT_PROPERTY(kAudioStreamPropertyVirtualFormat);
		OSStatus Status = AudioObjectGetPropertyData(CoreAudioInfo.OutputDeviceId, &Property, 0, nullptr, &DataSize, &Format);
		CORE_AUDIO_ERR(Status, "Failed to get audio stream virtual format");

		// we'll only change the virtual format if we need to
		bool bChangeFormat = false;
		if (bSampleRateChanged)
		{
			bChangeFormat = true;
			Format.mSampleRate = (Float64)StreamInfo.FrameRate;
		}

		if (Format.mFormatID != kAudioFormatLinearPCM)
		{
			bChangeFormat = true;
			Format.mFormatID = kAudioFormatLinearPCM;
		}

		if (bChangeFormat)
		{
			Status = AudioObjectSetPropertyData(CoreAudioInfo.OutputDeviceId, &Property, 0, nullptr, DataSize, &Format);
			CORE_AUDIO_ERR(Status, "Failed to set the virtual format on device");
		}
		return true;
	}

	bool FUnrealAudioCoreAudio::InitDevicePhysicalFormat()
	{
		AudioObjectPropertyAddress Property = NEW_OUTPUT_PROPERTY(kAudioStreamPropertyPhysicalFormat);
		AudioStreamBasicDescription Format;
		UInt32 DataSize = sizeof(AudioStreamBasicDescription);
		OSStatus Status = AudioObjectGetPropertyData(CoreAudioInfo.OutputDeviceId, &Property, 0, nullptr, &DataSize, &Format);
		CORE_AUDIO_ERR(Status, "Failed to get audio stream physical format");

		if (Format.mFormatID != kAudioFormatLinearPCM || Format.mBitsPerChannel < 16)
		{
			bool bSuccess = false;
			Format.mFormatID = kAudioFormatLinearPCM;

			// Make a copy of the old format
			AudioStreamBasicDescription NewFormat = Format;

			// Try setting the physical format to Float32
			NewFormat.mFormatFlags = Format.mFormatFlags;
			NewFormat.mFormatFlags |= kLinearPCMFormatFlagIsFloat;
			NewFormat.mFormatFlags &= ~kLinearPCMFormatFlagIsSignedInteger;

			NewFormat.mBitsPerChannel = 32;
			NewFormat.mBytesPerFrame = 4 * Format.mChannelsPerFrame;
			NewFormat.mBytesPerPacket = NewFormat.mBytesPerFrame * NewFormat.mFramesPerPacket;

			Status = AudioObjectSetPropertyData(CoreAudioInfo.OutputDeviceId, &Property, 0, nullptr, DataSize, &NewFormat);
			if (Status == noErr)
			{
				bSuccess = true;
			}

			// Lets try signed 32 bit integer format
			if (!bSuccess)
			{
				NewFormat.mFormatFlags = Format.mFormatFlags;
				NewFormat.mFormatFlags |= kLinearPCMFormatFlagIsSignedInteger;
				NewFormat.mFormatFlags |= kAudioFormatFlagIsPacked;
				NewFormat.mFormatFlags &= ~kLinearPCMFormatFlagIsFloat;

				Status = AudioObjectSetPropertyData(CoreAudioInfo.OutputDeviceId, &Property, 0, nullptr, DataSize, &NewFormat);
				if (Status == noErr)
				{
					bSuccess = true;
				}
			}

			// Now try 24 bit signed integer
			if (!bSuccess)
			{
				NewFormat.mBitsPerChannel = 24;
				NewFormat.mBytesPerFrame = 3 * Format.mChannelsPerFrame;
				NewFormat.mBytesPerPacket = NewFormat.mBytesPerFrame * NewFormat.mFramesPerPacket;

				Status = AudioObjectSetPropertyData(CoreAudioInfo.OutputDeviceId, &Property, 0, nullptr, DataSize, &NewFormat);
				if (Status == noErr)
				{
					bSuccess = true;
				}
			}

			if (!bSuccess)
			{
				// TODO: look at supporting other physical formats if we fail here
				UA_DEVICE_PLATFORM_ERROR(TEXT("Failed to set physical format of the audio device to something reasonable."));
				return false;
			}
		}
		return true;
	}

	bool FUnrealAudioCoreAudio::GetDeviceLatency(AudioDeviceID DeviceID, UInt32& Latency)
	{
		UInt32 DataSize = sizeof(UInt32);
		AudioObjectPropertyAddress Property = NEW_OUTPUT_PROPERTY(kAudioDevicePropertyLatency);
		if (AudioObjectHasProperty(DeviceID, &Property))
		{
			OSStatus Status = AudioObjectGetPropertyData(DeviceID, &Property, 0, nullptr, &DataSize, &Latency);
			CORE_AUDIO_ERR(Status, "Failed to get device latency");
		}
		return true;
	}

	bool FUnrealAudioCoreAudio::InitDeviceNumDeviceStreams()
	{
		UInt32 DataSize;
		AudioObjectPropertyAddress Property = NEW_OUTPUT_PROPERTY(kAudioDevicePropertyStreamConfiguration);
		OSStatus Status = AudioObjectGetPropertyDataSize(CoreAudioInfo.OutputDeviceId, &Property, 0, nullptr, &DataSize);
		CORE_AUDIO_ERR(Status, "Failed to get stream config size");

		AudioBufferList* BufferList = (AudioBufferList*)alloca(DataSize);
		Status = AudioObjectGetPropertyData(CoreAudioInfo.OutputDeviceId, &Property, 0, nullptr, &DataSize, BufferList);
		CORE_AUDIO_ERR(Status, "Failed to get output stream configuration");

		CoreAudioInfo.NumDeviceStreams = BufferList->mNumberBuffers;
		return true;
	}

	bool FUnrealAudioCoreAudio::InitDeviceOverrunCallback()
	{
		AudioObjectPropertyAddress Property = NEW_GLOBAL_PROPERTY(kAudioDeviceProcessorOverload);
		OSStatus Status = AudioObjectAddPropertyListener(CoreAudioInfo.OutputDeviceId, &Property, OverrunPropertyListener, (void *)this);
		CORE_AUDIO_ERR(Status, "Failed to set device overrun property listener");
		return true;
	}

	bool FUnrealAudioCoreAudio::InitDeviceCallback(const FCreateStreamParams& Params)
	{
		// Determine device callback buffer size range and set the best callback buffer size
		AudioValueRange BufferSizeRange;
		UInt32 DataSize = sizeof(AudioValueRange);
		AudioObjectPropertyAddress Property = NEW_OUTPUT_PROPERTY(kAudioDevicePropertyBufferFrameSizeRange);
		OSStatus Status = AudioObjectGetPropertyData(CoreAudioInfo.OutputDeviceId, &Property, 0, nullptr, &DataSize, &BufferSizeRange);
		CORE_AUDIO_ERR(Status, "Failed to get callback buffer size range");

		StreamInfo.BlockSize = (UInt32)FMath::Clamp(Params.CallbackBlockSize, (uint32)BufferSizeRange.mMinimum, (uint32)BufferSizeRange.mMaximum);

		DataSize = sizeof(UInt32);
		Property.mSelector = kAudioDevicePropertyBufferFrameSize;
		Status = AudioObjectSetPropertyData(CoreAudioInfo.OutputDeviceId, &Property, 0, nullptr, DataSize, &StreamInfo.BlockSize);
		CORE_AUDIO_ERR(Status, "Failed to set the callback buffer size");

		Status = AudioDeviceCreateIOProcID(CoreAudioInfo.OutputDeviceId, CoreAudioCallback, (void*)this, &CoreAudioInfo.DeviceIOProcId);
		CORE_AUDIO_ERR(Status, "Failed to set device callback function");

		// Initialize the device byte buffer to the correct number of bytes
		FDeviceInfo& DeviceInfo = CoreAudioInfo.OutputDevices[CoreAudioInfo.OutputDeviceIndex];
		FStreamDeviceInfo& StreamDeviceInfo = StreamInfo.DeviceInfo;
		StreamDeviceInfo.NumChannels = DeviceInfo.NumChannels;
		uint64 UserBufferBytes = DeviceInfo.NumChannels * StreamInfo.BlockSize * sizeof(float);
		StreamDeviceInfo.UserBuffer.Init(0, UserBufferBytes);

		// Setup the callback info struct before starting the device stream
		FCallbackInfo& CallbackInfo = CoreAudioInfo.CallbackInfo;

		CallbackInfo.OutBuffer		= (float*)StreamDeviceInfo.UserBuffer.GetData();
		CallbackInfo.NumFrames		= StreamInfo.BlockSize;
		CallbackInfo.NumChannels	= DeviceInfo.NumChannels;
		CallbackInfo.NumSamples		= CallbackInfo.NumFrames * CallbackInfo.NumChannels;
		CallbackInfo.UserData		= Params.UserData;
		CallbackInfo.StatusFlags	= 0;
		CallbackInfo.OutputSpeakers	= DeviceInfo.Speakers;
		CallbackInfo.FrameRate		= StreamInfo.FrameRate;
		CallbackInfo.StreamTime		= 0.0;

		if (pthread_cond_init(&CoreAudioInfo.Condition, NULL))
		{
			UA_DEVICE_PLATFORM_ERROR(TEXT("Failed to initialize thread condition"));
			return false;
		}
		return true;
	}

	bool FUnrealAudioCoreAudio::GetDeviceChannelsForStereo(AudioDeviceID DeviceID, TArray<ESpeaker::Type>& OutChannels)
	{
		UInt32 ChannelIndices[2];
		UInt32 PropertySize = sizeof(ChannelIndices);
		AudioObjectPropertyAddress Property = NEW_OUTPUT_PROPERTY(kAudioDevicePropertyPreferredChannelsForStereo);
		OSStatus Status = AudioObjectGetPropertyData(DeviceID, &Property, 0, nullptr, &PropertySize, ChannelIndices);
		CORE_AUDIO_ERR(Status, "Failed to get preferred channels for stereo property");

		if (ChannelIndices[0] == kAudioChannelLabel_Left)
		{
			OutChannels.Add(ESpeaker::FRONT_LEFT);
			OutChannels.Add(ESpeaker::FRONT_RIGHT);
		}
		else
		{
			OutChannels.Add(ESpeaker::FRONT_RIGHT);
			OutChannels.Add(ESpeaker::FRONT_LEFT);
		}
		return true;
	}

	bool FUnrealAudioCoreAudio::GetDeviceChannelsForLayoutDescriptions(AudioChannelLayout* ChannelLayout, TArray<ESpeaker::Type>& OutChannels)
	{
		bool bSuccess = true;
		if (ChannelLayout->mNumberChannelDescriptions == 0)
		{
			bSuccess = false;
		}
		else
		{
			for (UInt32 Channel = 0; Channel < ChannelLayout->mNumberChannelDescriptions; ++Channel)
			{
				AudioChannelDescription Description = ChannelLayout->mChannelDescriptions[Channel];
				ESpeaker::Type Speaker;
				switch(Description.mChannelLabel)
				{
					case kAudioChannelLabel_Left:					Speaker = ESpeaker::FRONT_LEFT;				break;
					case kAudioChannelLabel_Right:					Speaker = ESpeaker::FRONT_RIGHT;			break;
					case kAudioChannelLabel_Center:					Speaker = ESpeaker::FRONT_CENTER;			break;
					case kAudioChannelLabel_LFEScreen:				Speaker = ESpeaker::LOW_FREQUENCY;			break;
					case kAudioChannelLabel_LeftSurround:			Speaker = ESpeaker::SIDE_LEFT;				break;
					case kAudioChannelLabel_RightSurround:			Speaker = ESpeaker::SIDE_RIGHT;				break;
					case kAudioChannelLabel_LeftCenter:				Speaker = ESpeaker::FRONT_LEFT_OF_CENTER;	break;
					case kAudioChannelLabel_RightCenter:			Speaker = ESpeaker::FRONT_RIGHT_OF_CENTER;	break;
					case kAudioChannelLabel_CenterSurround:			Speaker = ESpeaker::BACK_CENTER;			break;
					case kAudioChannelLabel_LeftSurroundDirect:		Speaker = ESpeaker::SIDE_LEFT;				break;
					case kAudioChannelLabel_RightSurroundDirect:	Speaker = ESpeaker::SIDE_RIGHT;				break;
					case kAudioChannelLabel_TopCenterSurround:		Speaker = ESpeaker::TOP_CENTER;				break;
					case kAudioChannelLabel_VerticalHeightLeft:		Speaker = ESpeaker::TOP_FRONT_LEFT;			break;
					case kAudioChannelLabel_VerticalHeightCenter:	Speaker = ESpeaker::TOP_FRONT_CENTER;		break;
					case kAudioChannelLabel_VerticalHeightRight:	Speaker = ESpeaker::TOP_FRONT_RIGHT;		break;
					case kAudioChannelLabel_TopBackLeft:			Speaker = ESpeaker::TOP_BACK_LEFT;			break;
					case kAudioChannelLabel_TopBackCenter:			Speaker = ESpeaker::TOP_BACK_CENTER;		break;
					case kAudioChannelLabel_TopBackRight:			Speaker = ESpeaker::TOP_BACK_RIGHT;			break;
					case kAudioChannelLabel_RearSurroundLeft:		Speaker = ESpeaker::BACK_LEFT;				break;
					case kAudioChannelLabel_RearSurroundRight:		Speaker = ESpeaker::BACK_RIGHT;				break;
					case kAudioChannelLabel_Unused:					Speaker = ESpeaker::UNUSED;					break;
					case kAudioChannelLabel_Unknown:				Speaker = ESpeaker::UNUSED;					break;

					default:
						UA_DEVICE_PLATFORM_ERROR(TEXT("Unknown or unsupported channel label"));
						OutChannels.Empty();
						return false;
				}
				OutChannels.Add(Speaker);
			}
		}
		return bSuccess;
	}

	bool FUnrealAudioCoreAudio::GetDeviceChannelsForBitMap(UInt32 BitMap, TArray<ESpeaker::Type>& OutChannels)
	{
		bool bSuccess = true;
		// Bit maps for standard speaker layouts
		static const UInt32 BitMapMono				= kAudioChannelBit_Center;
		static const UInt32 BitMapStereo			= kAudioChannelBit_Left | kAudioChannelBit_Right;
		static const UInt32 BitMapStereoPoint1		= kAudioChannelBit_Left | kAudioChannelBit_Right | kAudioChannelBit_LFEScreen;
		static const UInt32 BitMapSurround			= kAudioChannelBit_Left | kAudioChannelBit_Right | kAudioChannelBit_Center | kAudioChannelBit_CenterSurround;
		static const UInt32 BitMapQuad				= kAudioChannelBit_Left | kAudioChannelBit_Right | kAudioChannelBit_LeftSurround | kAudioChannelBit_RightSurround;
		static const UInt32 BitMap4Point1			= kAudioChannelBit_Left | kAudioChannelBit_Right | kAudioChannelBit_LeftSurround | kAudioChannelBit_RightSurround | kAudioChannelBit_LFEScreen;
		static const UInt32 BitMap5Point1			= kAudioChannelBit_Left | kAudioChannelBit_Right | kAudioChannelBit_Center | kAudioChannelBit_LeftSurround | kAudioChannelBit_RightSurround | kAudioChannelBit_LFEScreen;
		static const UInt32 BitMap7Point1			= kAudioChannelBit_Left | kAudioChannelBit_Right | kAudioChannelBit_Center | kAudioChannelBit_LeftSurround | kAudioChannelBit_RightSurround | kAudioChannelBit_LFEScreen | kAudioChannelBit_LeftCenter | kAudioChannelBit_RightCenter;
		static const UInt32 BitMap5Point1Surround	= kAudioChannelBit_Left | kAudioChannelBit_Right | kAudioChannelBit_Center | kAudioChannelBit_LeftSurroundDirect | kAudioChannelBit_RightSurroundDirect | kAudioChannelBit_LFEScreen;
		static const UInt32 BitMap7Point1Surround	= kAudioChannelBit_Left | kAudioChannelBit_Right | kAudioChannelBit_Center | kAudioChannelBit_LeftSurroundDirect | kAudioChannelBit_RightSurroundDirect | kAudioChannelBit_LFEScreen | kAudioChannelBit_LeftSurround | kAudioChannelBit_RightSurround;

		switch(BitMap)
		{
			case BitMapMono:
				OutChannels.Add(ESpeaker::FRONT_CENTER);
				break;

			case BitMapStereo:
				OutChannels.Add(ESpeaker::FRONT_LEFT);
				OutChannels.Add(ESpeaker::FRONT_RIGHT);
				break;

			case BitMapStereoPoint1:
				OutChannels.Add(ESpeaker::FRONT_LEFT);
				OutChannels.Add(ESpeaker::FRONT_RIGHT);
				OutChannels.Add(ESpeaker::LOW_FREQUENCY);
				break;

			case BitMapSurround:
				OutChannels.Add(ESpeaker::FRONT_LEFT);
				OutChannels.Add(ESpeaker::FRONT_RIGHT);
				OutChannels.Add(ESpeaker::FRONT_CENTER);
				OutChannels.Add(ESpeaker::BACK_CENTER);
				break;

			case BitMapQuad:
				OutChannels.Add(ESpeaker::FRONT_LEFT);
				OutChannels.Add(ESpeaker::FRONT_RIGHT);
				OutChannels.Add(ESpeaker::BACK_LEFT);
				OutChannels.Add(ESpeaker::BACK_RIGHT);
				break;

			case BitMap4Point1:
				OutChannels.Add(ESpeaker::FRONT_LEFT);
				OutChannels.Add(ESpeaker::FRONT_RIGHT);
				OutChannels.Add(ESpeaker::BACK_LEFT);
				OutChannels.Add(ESpeaker::BACK_RIGHT);
				OutChannels.Add(ESpeaker::LOW_FREQUENCY);
				break;

			case BitMap5Point1:
				OutChannels.Add(ESpeaker::FRONT_LEFT);
				OutChannels.Add(ESpeaker::FRONT_RIGHT);
				OutChannels.Add(ESpeaker::FRONT_CENTER);
				OutChannels.Add(ESpeaker::LOW_FREQUENCY);
				OutChannels.Add(ESpeaker::BACK_LEFT);
				OutChannels.Add(ESpeaker::BACK_RIGHT);
				break;

			case BitMap7Point1:
				OutChannels.Add(ESpeaker::FRONT_LEFT);
				OutChannels.Add(ESpeaker::FRONT_RIGHT);
				OutChannels.Add(ESpeaker::FRONT_CENTER);
				OutChannels.Add(ESpeaker::LOW_FREQUENCY);
				OutChannels.Add(ESpeaker::BACK_LEFT);
				OutChannels.Add(ESpeaker::BACK_RIGHT);
				OutChannels.Add(ESpeaker::FRONT_LEFT_OF_CENTER);
				OutChannels.Add(ESpeaker::FRONT_RIGHT_OF_CENTER);
				break;

			case BitMap5Point1Surround:
				OutChannels.Add(ESpeaker::FRONT_LEFT);
				OutChannels.Add(ESpeaker::FRONT_RIGHT);
				OutChannels.Add(ESpeaker::FRONT_CENTER);
				OutChannels.Add(ESpeaker::LOW_FREQUENCY);
				OutChannels.Add(ESpeaker::SIDE_LEFT);
				OutChannels.Add(ESpeaker::SIDE_RIGHT);
				break;

			case BitMap7Point1Surround:
				OutChannels.Add(ESpeaker::FRONT_LEFT);
				OutChannels.Add(ESpeaker::FRONT_RIGHT);
				OutChannels.Add(ESpeaker::FRONT_CENTER);
				OutChannels.Add(ESpeaker::LOW_FREQUENCY);
				OutChannels.Add(ESpeaker::BACK_LEFT);
				OutChannels.Add(ESpeaker::BACK_RIGHT);
				OutChannels.Add(ESpeaker::SIDE_LEFT);
				OutChannels.Add(ESpeaker::SIDE_RIGHT);
				break;

			default:
				UA_DEVICE_PLATFORM_ERROR(TEXT("Unknown or unsupported channel bitmap"));
				bSuccess = false;
				break;
		}
		return bSuccess;
	}

	bool FUnrealAudioCoreAudio::GetDeviceChannelsForLayoutTag(AudioChannelLayoutTag LayoutTag, TArray<ESpeaker::Type>& OutChannels)
	{
		bool bSuccess = true;
		switch (LayoutTag)
		{
			case kAudioChannelLayoutTag_Mono:
				OutChannels.Add(ESpeaker::FRONT_CENTER);
				break;

			case kAudioChannelLayoutTag_Stereo:
			case kAudioChannelLayoutTag_StereoHeadphones:
			case kAudioChannelLayoutTag_MatrixStereo:
			case kAudioChannelLayoutTag_MidSide:
			case kAudioChannelLayoutTag_XY:
			case kAudioChannelLayoutTag_Binaural:
				OutChannels.Add(ESpeaker::FRONT_LEFT);
				OutChannels.Add(ESpeaker::FRONT_RIGHT);
				break;

			case kAudioChannelLayoutTag_Quadraphonic:
				OutChannels.Add(ESpeaker::FRONT_LEFT);
				OutChannels.Add(ESpeaker::FRONT_RIGHT);
				OutChannels.Add(ESpeaker::BACK_LEFT);
				OutChannels.Add(ESpeaker::BACK_RIGHT);
				break;

			case kAudioChannelLayoutTag_Pentagonal:
				OutChannels.Add(ESpeaker::FRONT_LEFT);
				OutChannels.Add(ESpeaker::FRONT_RIGHT);
				OutChannels.Add(ESpeaker::BACK_LEFT);
				OutChannels.Add(ESpeaker::BACK_RIGHT);
				OutChannels.Add(ESpeaker::FRONT_CENTER);
				break;

			case kAudioChannelLayoutTag_Hexagonal:
				OutChannels.Add(ESpeaker::FRONT_LEFT);
				OutChannels.Add(ESpeaker::FRONT_RIGHT);
				OutChannels.Add(ESpeaker::BACK_LEFT);
				OutChannels.Add(ESpeaker::BACK_RIGHT);
				OutChannels.Add(ESpeaker::FRONT_CENTER);
				OutChannels.Add(ESpeaker::LOW_FREQUENCY);
				break;

			case kAudioChannelLayoutTag_Octagonal:
				OutChannels.Add(ESpeaker::FRONT_LEFT);
				OutChannels.Add(ESpeaker::FRONT_RIGHT);
				OutChannels.Add(ESpeaker::BACK_LEFT);
				OutChannels.Add(ESpeaker::BACK_RIGHT);
				OutChannels.Add(ESpeaker::FRONT_CENTER);
				OutChannels.Add(ESpeaker::LOW_FREQUENCY);
				OutChannels.Add(ESpeaker::SIDE_LEFT);
				OutChannels.Add(ESpeaker::SIDE_RIGHT);
				break;

			default:
				bSuccess = false;
				break;
		}
		return bSuccess;
	}

	bool FUnrealAudioCoreAudio::GetDeviceChannelsForChannelCount(UInt32 NumChannels, TArray<ESpeaker::Type>& OutChannels)
	{
		bool bSuccess = true;
		switch (NumChannels)
		{
			case 1:
				OutChannels.Add(ESpeaker::FRONT_CENTER);
				break;

			case 2:
				OutChannels.Add(ESpeaker::FRONT_LEFT);
				OutChannels.Add(ESpeaker::FRONT_RIGHT);
				break;

			case 3:
				OutChannels.Add(ESpeaker::FRONT_LEFT);
				OutChannels.Add(ESpeaker::FRONT_RIGHT);
				OutChannels.Add(ESpeaker::LOW_FREQUENCY);
				break;

			case 4:
				OutChannels.Add(ESpeaker::FRONT_LEFT);
				OutChannels.Add(ESpeaker::FRONT_RIGHT);
				OutChannels.Add(ESpeaker::BACK_LEFT);
				OutChannels.Add(ESpeaker::BACK_RIGHT);
				break;

			case 5:
				OutChannels.Add(ESpeaker::FRONT_LEFT);
				OutChannels.Add(ESpeaker::FRONT_RIGHT);
				OutChannels.Add(ESpeaker::BACK_LEFT);
				OutChannels.Add(ESpeaker::BACK_RIGHT);
				OutChannels.Add(ESpeaker::FRONT_CENTER);
				break;

			case 6:
				OutChannels.Add(ESpeaker::FRONT_LEFT);
				OutChannels.Add(ESpeaker::FRONT_RIGHT);
				OutChannels.Add(ESpeaker::BACK_LEFT);
				OutChannels.Add(ESpeaker::BACK_RIGHT);
				OutChannels.Add(ESpeaker::FRONT_CENTER);
				OutChannels.Add(ESpeaker::LOW_FREQUENCY);
				break;

			case 8:
				OutChannels.Add(ESpeaker::FRONT_LEFT);
				OutChannels.Add(ESpeaker::FRONT_RIGHT);
				OutChannels.Add(ESpeaker::BACK_LEFT);
				OutChannels.Add(ESpeaker::BACK_RIGHT);
				OutChannels.Add(ESpeaker::FRONT_CENTER);
				OutChannels.Add(ESpeaker::LOW_FREQUENCY);
				OutChannels.Add(ESpeaker::SIDE_LEFT);
				OutChannels.Add(ESpeaker::SIDE_RIGHT);
				break;

			default:
				bSuccess = false;
				UA_DEVICE_PLATFORM_ERROR(TEXT("Failed to get a speaker array from number of channels"));
				break;
		}
		return bSuccess;
	}


}

IMPLEMENT_MODULE(UAudio::FUnrealAudioCoreAudio, UnrealAudioCoreAudio);

#endif