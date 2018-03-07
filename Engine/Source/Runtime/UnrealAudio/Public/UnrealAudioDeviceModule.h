// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"
#include "UnrealAudioTypes.h"
#include "Modules/ModuleInterface.h"
#include "Runtime/UnrealAudio/Private/UnrealAudioDeviceFormat.h"

class Error;

#if ENABLE_UNREAL_AUDIO

UNREALAUDIO_API DECLARE_LOG_CATEGORY_EXTERN(LogUnrealAudioDevice, Log, All);

#define UA_DEVICE_PLATFORM_ERROR(INFO)	(UAudio::OnDeviceError(EDeviceError::PLATFORM, INFO, FString(__FILE__), __LINE__))
#define AU_DEVICE_PARAM_ERROR(INFO)		(UAudio::OnDeviceError(EDeviceError::INVALID_PARAMETER, INFO, FString(__FILE__), __LINE__))
#define AU_DEVICE_WARNING(INFO)			(UAudio::OnDeviceError(EDeviceError::WARNING, INFO, FString(__FILE__), __LINE__))


namespace UAudio
{

	/**
	* EStreamStatus
	* An enumeration to specify the state of the audio stream.
	*/
	namespace EStreamState
	{
		enum Type
		{
			SHUTDOWN,			/** The stream is shutdown */
			STOPPED,			/** The stream was running and is now stopped */
			RUNNING,			/** The stream is running (currently streaming callbacks) */
			STOPPING,			/** The stream is currently stopping*/
		};
	}

	/**
	* EStreamStatus
	* An enumeration to specify the flow status of the audio stream.
	*/
	namespace EStreamFlowStatus
	{
		enum Flag
		{
			OUTPUT_OVERFLOW	= (1 << 1)		/** The output stream has too little or too much data */
		};
	}
	typedef uint8 StreamStatus;

	/**
	Static array specifying which frame rates to check for support on audio devices.
	http://en.wikipedia.org/wiki/Sampling_%28signal_processing%29
	*/
	const uint32 PossibleFrameRates[] =
	{
		8000,		/** Used for telephony, walkie talkies, ok for human speech. */
		11025,		/** Quarter sample rate of CD's, low-quality PCM */
		16000,		/** Rate used for VOIP (which is why VOIP sounds slightly better than normal phones), wide-band extension over normal telephony */
		22050,		/** Half CD sample rate, low quality PCM */
		32000,		/** MiniDV, digital FM radio, decent wireless microphones */
		44100,		/** CD's, MPEG-1 (MP3), covers 20kHz bandwidth of human hearing with room for LP ripple. */
		48000,		/** Standard rate used by "professional" film and audio guys: mixing console, digital recorders, etc */
		88200,		/** Used for recording equipment intended for CDs */
		96000,		/** DVD audio, high-def audio, 2x the 48khz "professional" sample rate. */
		176400,		/** Rate used by HDCD recorders. */
		192000,		/** HD-DVD and blue ray audio, 4x the 48khz "professional" sample rate. */
	};
	const uint32 MaxPossibleFrameRates = 11;

	/**
	* FCallbackInfo
	* A struct for callback info. Using this rather than params in callback function
	* because updating new members here will be less painful.
	*/
	struct FCallbackInfo
	{
		/** A pointer to a buffer of float data which will be written to output device.*/
		float* OutBuffer;

		/** The number of buffer output frames */
		uint32 NumFrames;

		/** The number of channels in output. */
		int32 NumChannels;

		/** The number of total samples (NumFrames*NumChannels). */
		int32 NumSamples;

		/** Array of outputs speakers */
		TArray<ESpeaker::Type> OutputSpeakers;

		/** The current status flags of the input and output buffers */
		StreamStatus StatusFlags;

		/** The current frame-accurate lifetime of the audio stream. */
		double StreamTime;

		/** The output device framerate */
		int32 FrameRate;

		/** A pointer to user data */
		void* UserData;
	};

	/**
	* StreamCallback
	*
	* Callback which calls from platform audio device code into platform-independent mixing code.
	* @param CallbackInfo A struct of callback info needed to process audio.
	*/
	typedef bool(*StreamCallback)(FCallbackInfo& CallbackInfo);

	/**
	* FDeviceInfo
	* Struct used to hold information about audio devices, queried by user.
	*/
	struct FDeviceInfo
	{
		/** Friendly name of device. */
		FString FriendlyName;

		/** Number of channels (e.g. 2 for stereo) natively supported by the device. */
		uint32 NumChannels;

		/** The frame rate of the device. */
		uint32 FrameRate;

		/** The possible frame rates of the device. */
		TArray<uint32> PossibleFrameRates;

		/** The data format of the device (e.g. float). */
		EStreamFormat::Type StreamFormat;

		/** What speakers this device supports (if output device) */
		TArray<ESpeaker::Type> Speakers;

		/** Device latency (if available) */
		uint32 Latency;

		/** Whether or not it is the OS default device for the type. */
		bool bIsSystemDefault;

		FDeviceInfo()
			: FriendlyName(TEXT("Uknown"))
			, NumChannels(0)
			, StreamFormat(EStreamFormat::UNKNOWN)
			, Latency(0)
			, bIsSystemDefault(false)
		{
		}
	};

	/**
	* FCreateStreamParams
	* Struct used to define stream creation.
	*/
	struct FCreateStreamParams
	{
		/** The index of the device to use for audio output. Must be defined. */
		uint32 OutputDeviceIndex;

		/** The size of the callback block (in frames) that the user would like. (e.g. 512) */
		uint32 CallbackBlockSize;

		/** The function pointer of a user callback function to generate audio samples to output device. */
		StreamCallback CallbackFunction;

		/** The preferred frame rate of the audio stream (this may be platform dependent) */
		uint32 FrameRate;

		/** Pointer to store user data. Passed in callback function. */
		void* UserData;

		/** Constructor */
		FCreateStreamParams()
			: OutputDeviceIndex(INDEX_NONE)
			, CallbackBlockSize(512)
			, CallbackFunction(nullptr)
			, FrameRate(48000)
			, UserData(nullptr)
		{}
	};

	/**
	* IUnrealAudioDeviceModule
	* Main audio device module, needs to be implemented for every specific platform API.
	*/
	class UNREALAUDIO_API IUnrealAudioDeviceModule : public IModuleInterface
	{
	public:
		/** Constructor */
		IUnrealAudioDeviceModule()
		{
		}

		/** Virtual destructor */
		virtual ~IUnrealAudioDeviceModule()
		{
		}

		/** 
		* Initializes the audio device module. 
		* @return true if succeeded.
		*/
		virtual bool Initialize() = 0;

		/** 
		* Shuts down the audio device module. 
		* @return true if succeeded.
		*/
		virtual bool Shutdown() = 0;

		/** 
		* Returns the API enumeration for the internal implementation of the device module. 
		* @param OutType Enumeration of the output tyep.
		* @return true if succeeded.
		*/
		virtual bool GetDevicePlatformApi(EDeviceApi::Type & OutType) const = 0;

		/** 
		* Returns the number of connected output devices to the system. 
		* @param OutNumDevices The number of output devices
		* @return true if succeeded.
		*/
		virtual bool GetNumOutputDevices(uint32& OutNumDevices) const = 0;

		/** 
		* Returns information about the output device at that index. 
		* @param DeviceIndex The index of the output device. 
		* @param OutInfo Information struct about the device.
		* @return true if succeeded.
		* @note DeviceIndex should be less than total number of devices given by GetNumOutputDevices.
		*/
		virtual bool GetOutputDeviceInfo(const uint32 DeviceIndex, FDeviceInfo& OutInfo) const = 0;

		/**
		* Returns the default output device index.
		* @param OutDefaultIndex The default device index.
		* @return true if succeeded.
		* @note The default device is specified by the OS default device output.
		*/
		virtual bool GetDefaultOutputDeviceIndex(uint32& OutDefaultIndex) const = 0;

		/**
		* Starts the device audio stream.
		* @return true if succeeded.
		* @note The device stream needs to have been created before starting.
		*/
		virtual bool StartStream() = 0;

		/**
		* Stops the device audio stream.
		* @return true if succeeded.
		* @note The device stream needs to have been created and started before stopping.
		*/
		virtual bool StopStream() = 0;

		/**
		* Frees resources of the device audio stream.
		* @return true if succeeded.
		* @note The device stream needs to have been created before shutting down.
		*/
		virtual bool ShutdownStream() = 0;

		/**
		* Returns the latency of the input and output devices.
		* @param OutputDeviceLatency The latency of the output device as reported by platform API
		* @param InputDeviceLatency The latency of the input device as reported by platform API
		* @return true if succeeded.
		* @note The device stream needs to have been started for this return anything. A return of 0 means
		* the device hasn't started or that device hasn't been initialized.
		*/
		virtual bool GetLatency(uint32& OutputDeviceLatency) const = 0;

		/**
		* Returns the frame rate of the audio devices.
		* @param OutFrameRate The frame rate of the output audio device.
		* @return true if succeeded.
		* @note FrameRate is also known as SampleRate. A "Frame" is the minimal time delta of 
		* audio and is composed of interleaved samples. e.g. 1 stereo frame is 2 samples: left and right.
		*/
		virtual bool GetFrameRate(uint32& OutFrameRate) const = 0;

		/**
		* Creates an audio stream given the input parameter struct.
		* @param Params The parameters needed to create an audio stream.
		* @return true if succeeded.
		*/
		bool CreateStream(const FCreateStreamParams& Params)
		{
			checkf(StreamInfo.State == EStreamState::SHUTDOWN, TEXT("Stream state can't be open if creating a new one."));
			checkf(Params.OutputDeviceIndex != INDEX_NONE, TEXT("Input or output stream params need to be set."));

			bool bSuccess = false;

			Reset();

			if (OpenDevice(Params))
			{
				StreamInfo.State = EStreamState::STOPPED;
				StreamInfo.CallbackFunction = Params.CallbackFunction;
				StreamInfo.UserData = Params.UserData;
				StreamInfo.BlockSize = Params.CallbackBlockSize;
				StreamInfo.StreamDelta = (double)StreamInfo.BlockSize / StreamInfo.FrameRate;
				bSuccess = true;
			}

			return bSuccess;
		}

	protected:

		/**
		* FBufferFormatConvertInfo
		* Struct used to convert data formats (e.g. float, int32, etc) to and from the device to user callback stream.
		* The user audio callback will always be in floats, but audio devices may have native formats that are different, so 
		* we need to support converting data formats.
		*/
		struct FBufferFormatConvertInfo
		{
			/** The base number of channels for the format conversion. */
			uint32 NumChannels;

			/** The number of channels we're converting from. */
			uint32 FromChannels;

			/** The number of channels we're to. */
			uint32 ToChannels;

			/** The data format we're converting from. */
			EStreamFormat::Type FromFormat;

			/** The data format we're converting to. */
			EStreamFormat::Type	ToFormat;
		};

		/**
		* FStreamDeviceInfo
		* Struct used to represent information about a particular device (input or output device)
		*/
		struct FStreamDeviceInfo
		{
			/** The index this device is in (from list of devices of this type). */
			uint32 DeviceIndex;

			/** The speaker types this device uses. */
			TArray<ESpeaker::Type> Speakers;

			/** The number of channels this device actually supports. */
			uint32 NumChannels;

			/** The reported latency of this device. */
			uint32 Latency;

			/** The native framerate of this device. */
			uint32 FrameRate;

			/** The native data format of this device. */
			EStreamFormat::Type DeviceDataFormat;

			/** Conversion information to convert audio streams to/from this device. */
			FBufferFormatConvertInfo BufferFormatConvertInfo;

			/** A buffer used to store data to/from this device. */
			TArray<uint8> UserBuffer;

			/** True if we need to perform a format conversion. */
			bool bPerformFormatConversion;

			/** True if we need to perform a byte swap for this device. */
			bool bPerformByteSwap;

			/** Constructor */
			FStreamDeviceInfo()
			{
				Initialize();
			}

			/** Function which initializes the struct variables to default values. Maye be called multiple times. */
			void Initialize()
			{
				DeviceIndex = 0;
				NumChannels = 0;
				Latency = 0;
				FrameRate = 0;
				DeviceDataFormat = EStreamFormat::UNKNOWN;
				bPerformFormatConversion = false;
				bPerformByteSwap = false;

				BufferFormatConvertInfo.NumChannels = 0;
				BufferFormatConvertInfo.FromChannels = 0;
				BufferFormatConvertInfo.ToChannels = 0;
				BufferFormatConvertInfo.FromFormat = EStreamFormat::UNKNOWN;
				BufferFormatConvertInfo.ToFormat = EStreamFormat::UNKNOWN;
			}
		};

		/**
		* FStreamInfo
		* Struct used to represent general information about the audio stream.
		*/
		class FStreamInfo
		{
		public:
			/** The overall framerate of the stream. This may be different from device frame rate. */
			uint32 FrameRate;

			/** The current state of the stream. */
			EStreamState::Type State;

			/** Information on the user callback */
			FCallbackInfo CallbackInfo;

			/** The sample-accurate running time of the stream in seconds (i.e. not necessarily real-world time but stream time). */
			double StreamTime;

			/** The amount of time that passes per update block. */
			double StreamDelta;

			/** Running audio thread */
			FRunnableThread* Thread;

			/** User callback function */
			StreamCallback CallbackFunction;

			/** User data */
			void* UserData;

			/** The size of the callback frame count*/
			uint32 BlockSize;

			/** A byte array used to store data to and from audio devices. */
			TArray<uint8> DeviceBuffer;

			/** device-specific information for output device. */
			FStreamDeviceInfo DeviceInfo;

			/** Constructor */
			FStreamInfo()
			{
				// Just call initialize since it will get called when starting up a new stream.
				Initialize();
			}

			/** Gets called every time a stream is opened, is reset in cae the device data exists from a previous open. */
			void Initialize()
			{
				FrameRate = 0;
				State = EStreamState::SHUTDOWN;
				StreamTime = 0.0;
				StreamDelta = 0.0;

				CallbackFunction = nullptr;
				Thread = nullptr;
				UserData = nullptr;
				BlockSize = 0;
				DeviceInfo.Initialize();
			}
		};

	protected: // Protected Methods

		/** Opens audio devices given the input params. Implemented per platform */
		virtual bool OpenDevice(const FCreateStreamParams& Params) = 0;

		/** Called before opening up new streams. */
		void Reset()
		{
			StreamInfo.Initialize();
		}

		/** Sets up any convert information for given stream type (figures out to/from convert format and channel formats) */
		void SetupBufferFormatConvertInfo();

		/** Performs actual buffer format and channel conversion */
		bool ConvertBufferFormat(TArray<uint8>& OutputBuffer, TArray<uint8>& InputBuffer);

		/** Updates the sample-accurate stream time value. */
		FORCEINLINE void UpdateStreamTimeTick()
		{
			StreamInfo.StreamTime += StreamInfo.StreamDelta;
		}

		/** Templated function to convert to float from any other format type. */
		template <typename FromType, typename ToType>
		void ConvertToFloatType(TArray<uint8>& ToBuffer, TArray<uint8>& FromBuffer, uint32 NumFrames, FBufferFormatConvertInfo& ConvertInfo);

		/** Templated function to any format type to float. */
		template <typename FloatType>
		void ConvertAllToFloatType(TArray<uint8>& ToBuffer, TArray<uint8>& FromBuffer, uint32 NumFrames, FBufferFormatConvertInfo& ConvertInfo);

		/** Templated function to integer type to any other integer type. */
		template <typename FromType, typename ToType>
		void ConvertIntegerToIntegerType(TArray<uint8>& ToBuffer, TArray<uint8>& FromBuffer, uint32 NumFrames, FBufferFormatConvertInfo& ConvertInfo);

		/** Templated function to convert a float type to integer type. */
		template <typename FromType, typename ToType>
		void ConvertFloatToIntegerType(TArray<uint8>& ToBuffer, TArray<uint8>& FromBuffer, uint32 NumFrames, FBufferFormatConvertInfo& ConvertInfo);

		/** Templated function to any type to integer type. */
		template <typename FloatType>
		void ConvertAllToIntegerType(TArray<uint8>& ToBuffer, TArray<uint8>& FromBuffer, uint32 NumFrames, FBufferFormatConvertInfo& ConvertInfo);

	protected: // Protected Data

		/** Device stream info struct */
		FStreamInfo StreamInfo;
	};

	// Exported Functions

	/** Creates a dummy audio device */
	UNREALAUDIO_API IUnrealAudioDeviceModule* CreateDummyDeviceModule();

	/**
	* Function called when an error occurs in the device code.
	* @param Error The error type that occurred.
	* @param ErrorDetails A string of specific details about the error.
	* @param FileName The file that the error occurred in.
	* @param LineNumber The line number that the error occurred in.
	*/
	static inline void OnDeviceError(const EDeviceError::Type Error, const FString& ErrorDetails, const FString& FileName, int32 LineNumber)
	{
		UE_LOG(LogUnrealAudioDevice, Error, TEXT("Audio Device Error: (%s) : %s (%s::%d)"), EDeviceError::ToString(Error), *ErrorDetails, *FileName, LineNumber);
	}

	/** Helper function to get number of bytes for a given stream format. */
	static inline uint32 GetNumBytesForFormat(EStreamFormat::Type Format)
	{
		switch (Format)
		{
			case EStreamFormat::FLT:		return 4;
			case EStreamFormat::DBL:		return 8;
			case EStreamFormat::INT_16:		return 2;
			case EStreamFormat::INT_24:		return 3;
			case EStreamFormat::INT_32:		return 4;
			default:						return 0;
		}
	}

} // namespace UAudio

#endif // #if ENABLE_UNREAL_AUDIO


