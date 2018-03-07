// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AudioMixerPlatformAudioUnit.h"
#include "ModuleManager.h"
#include "AudioMixer.h"
#include "AudioMixerDevice.h"
#include "CoreGlobals.h"
#include "CoreMinimal.h"
#include "Misc/ConfigCacheIni.h"
#if PLATFORM_IOS || PLATFORM_TVOS
#include "ADPCMAudioInfo.h"

#else

#include "VorbisAudioInfo.h"
#include "OpusAudioInfo.h"
#endif // #if PLATFORM_IOS || PLATFORM_TVOS
/*
	This implementation only depends on the audio units API which allows it to run on MacOS, iOS and tvOS. 
	
	For now just assume an iOS configuration (only 2 left and right channels on a single device)
*/

/**
* CoreAudio System Headers
*/
#include <AudioToolbox/AudioToolbox.h>
#include <AudioUnit/AudioUnit.h>
#if PLATFORM_IOS || PLATFORM_TVOS
#include <AVFoundation/AVAudioSession.h>
#elif  PLATFORM_MAC
#include <CoreAudio/AudioHardware.h>
#else
#error Invalid Platform!
#endif // #if PLATFORM_IOS || PLATFORM_TVOS

DECLARE_LOG_CATEGORY_EXTERN(LogAudioMixerAudioUnit, Log, All);
DEFINE_LOG_CATEGORY(LogAudioMixerAudioUnit);

namespace Audio
{
#if PLATFORM_IOS || PLATFORM_TVOS
    static const int32 DefaultBufferSize = 4096;
#else
    static const int32 DefaultBufferSize = 1024;
    static const int32 AUBufferSize = 256;
#endif //#if PLATFORM_IOS || PLATFORM_TVOS
    static const double DefaultSampleRate = 48000.0;
    
    static int32 SuspendCounter = 0;
    
	FMixerPlatformAudioUnit::FMixerPlatformAudioUnit()
		: bInitialized(false)
        , bInCallback(false)
        , SubmittedBufferPtr(nullptr)
        , RemainingBytesInCurrentSubmittedBuffer(0)
        , BytesPerSubmittedBuffer(0)
        , GraphSampleRate(DefaultSampleRate)
	{
	}

	FMixerPlatformAudioUnit::~FMixerPlatformAudioUnit()
	{
		if (bInitialized)
		{
			TeardownHardware();
		}
	}
    
    int32 FMixerPlatformAudioUnit::GetNumFrames(const int32 InNumReqestedFrames)
    {
#if PLATFORM_IOS || PLATFORM_TVOS
        AVAudioSession* AudioSession = [AVAudioSession sharedInstance];
        double BufferSizeInSec = [AudioSession preferredIOBufferDuration];
        double SampleRate = [AudioSession preferredSampleRate];
        
        if (BufferSizeInSec == 0.0)
        {
            return DefaultBufferSize;
        }
        
        int32 NumFrames = (int32)(SampleRate * BufferSizeInSec);
        
        return NumFrames;
#else
       //On MacOS, we hardcode buffer sizes.
        return DefaultBufferSize;
#endif
    }

	bool FMixerPlatformAudioUnit::InitializeHardware()
	{
        if (bInitialized)
		{
			return false;
		}
        
		OSStatus Status;
        GraphSampleRate = (double) AudioStreamInfo.DeviceInfo.SampleRate;
        UInt32 BufferSize = (UInt32) GetNumFrames(OpenStreamParams.NumFrames);
        const int32 NumChannels = 2;

        if (GraphSampleRate == 0)
        {
            GraphSampleRate = DefaultSampleRate;
        }
        
        if (BufferSize == 0)
        {
            BufferSize = DefaultBufferSize;
        }
        
        BytesPerSubmittedBuffer = BufferSize * NumChannels * sizeof(float);
        check(BytesPerSubmittedBuffer != 0);
        UE_LOG(LogAudioMixerAudioUnit, Display, TEXT("Bytes per submitted buffer: %d"), BytesPerSubmittedBuffer);
        
#if PLATFORM_IOS || PLATFORM_TVOS
        NSError* error;
        
        AVAudioSession* AudioSession = [AVAudioSession sharedInstance];

        GraphSampleRate = [AudioSession preferredSampleRate];
        bool Success = [AudioSession setPreferredSampleRate:GraphSampleRate error:&error];
        
        if (!Success)
        {
            UE_LOG(LogAudioMixerAudioUnit, Display, TEXT("Error setting sample rate."));
        }
        
        // By calling setPreferredIOBufferDuration, we indicate that we would prefer that the buffer size not change if possible.
        Success = [AudioSession setPreferredIOBufferDuration:[AudioSession preferredIOBufferDuration] error: &error];
        
        UE_LOG(LogAudioMixerAudioUnit, Display, TEXT("Device Sample Rate: %f"), GraphSampleRate);
        check(GraphSampleRate != 0);
        
        Success = [AudioSession setActive:true error:&error];
        
        if (!Success)
        {
            UE_LOG(LogAudioMixerAudioUnit, Display, TEXT("Error starting audio session."));
        }
#else
        AudioObjectID DeviceAudioObjectID;
        AudioObjectPropertyAddress DevicePropertyAddress;
        UInt32 AudioDeviceQuerySize;
        
        //Get Audio Device ID- this will be used throughout initialization to query the audio hardware.
        DevicePropertyAddress.mSelector = kAudioHardwarePropertyDefaultOutputDevice;
        DevicePropertyAddress.mScope = kAudioObjectPropertyScopeGlobal;
        DevicePropertyAddress.mElement = 0;
        AudioDeviceQuerySize = sizeof(AudioDeviceID);
        Status = AudioObjectGetPropertyData(kAudioObjectSystemObject, &DevicePropertyAddress, 0, nullptr, &AudioDeviceQuerySize, &DeviceAudioObjectID);
        
        if(Status != 0)
        {
            UE_LOG(LogAudioMixerAudioUnit, Display, TEXT("ERROR setting sample rate to %f"), GraphSampleRate);
        }
        
        Status = AudioObjectGetPropertyData(DeviceAudioObjectID, &DevicePropertyAddress, 0, nullptr, &AudioDeviceQuerySize, &GraphSampleRate);
        
        if(Status == 0)
        {
            UE_LOG(LogAudioMixerAudioUnit, Display, TEXT("Sample Rate: %f"), GraphSampleRate);
        }

#endif // #if PLATFORM_IOS || PLATFORM_TVOS
        

		// Linear PCM stream format
		OutputFormat.mFormatID         = kAudioFormatLinearPCM;
		OutputFormat.mFormatFlags	   = kAudioFormatFlagIsFloat | kAudioFormatFlagsNativeEndian | kAudioFormatFlagIsPacked;
		OutputFormat.mChannelsPerFrame = 2;
		OutputFormat.mBytesPerFrame    = sizeof(float) * OutputFormat.mChannelsPerFrame;
		OutputFormat.mFramesPerPacket  = 1;
		OutputFormat.mBytesPerPacket   = OutputFormat.mBytesPerFrame * OutputFormat.mFramesPerPacket;
		OutputFormat.mBitsPerChannel   = 8 * sizeof(float);
		OutputFormat.mSampleRate       = GraphSampleRate;

		Status = NewAUGraph(&AudioUnitGraph);
		if (Status != noErr)
		{
			HandleError(TEXT("Failed to create audio unit graph!"));
			return false;
		}

		AudioComponentDescription UnitDescription;

		// Setup audio output unit
		UnitDescription.componentType         = kAudioUnitType_Output;
#if PLATFORM_IOS || PLATFORM_TVOS
        //On iOS, we'll use the RemoteIO AudioUnit.
		UnitDescription.componentSubType      = kAudioUnitSubType_RemoteIO;
#else //PLATFORM_MAC
        //On MacOS, we'll use the DefaultOutput AudioUnit.
        UnitDescription.componentSubType      = kAudioUnitSubType_DefaultOutput;
#endif // #if PLATFORM_IOS || PLATFORM_TVOS
		UnitDescription.componentManufacturer = kAudioUnitManufacturer_Apple;
		UnitDescription.componentFlags        = 0;
		UnitDescription.componentFlagsMask    = 0;
		Status = AUGraphAddNode(AudioUnitGraph, &UnitDescription, &OutputNode);
		if (Status != noErr)
		{
			HandleError(TEXT("Failed to initialize audio output node!"), true);
			return false;
		}
		
		Status = AUGraphOpen(AudioUnitGraph);
		if (Status != noErr)
		{
			HandleError(TEXT("Failed to open audio unit graph"), true);
			return false;
		}
		
		Status = AUGraphNodeInfo(AudioUnitGraph, OutputNode, nullptr, &OutputUnit);
		if (Status != noErr)
		{
			HandleError(TEXT("Failed to retrieve output unit reference!"), true);
			return false;
		}

		Status = AudioUnitSetProperty(OutputUnit,
		                              kAudioUnitProperty_StreamFormat,
		                              kAudioUnitScope_Input,
		                              0,
		                              &OutputFormat,
		                              sizeof(AudioStreamBasicDescription));
		if (Status != noErr)
		{
			HandleError(TEXT("Failed to set output format!"), true);
			return false;
		}

#if PLATFORM_MAC
        DevicePropertyAddress.mSelector = kAudioDevicePropertyBufferFrameSize;
        DevicePropertyAddress.mScope = kAudioObjectPropertyScopeGlobal;
        DevicePropertyAddress.mElement = 0;
        AudioDeviceQuerySize = sizeof(AUBufferSize);
        Status = AudioObjectSetPropertyData(DeviceAudioObjectID, &DevicePropertyAddress, 0, nullptr, AudioDeviceQuerySize, &AUBufferSize);
        if(Status != 0)
        {
            HandleError(TEXT("Failed to set output format!"), true);
            return false;
        }
        
#endif //#if PLATFORM_MAC
        
        AudioStreamInfo.NumOutputFrames = BufferSize;

        AudioStreamInfo.DeviceInfo = GetPlatformDeviceInfo();
        
		AURenderCallbackStruct InputCallback;
		InputCallback.inputProc = &AudioRenderCallback;
		InputCallback.inputProcRefCon = this;
		Status = AUGraphSetNodeInputCallback(AudioUnitGraph,
		                                     OutputNode,
		                                     0,
		                                     &InputCallback);
		UE_CLOG(Status != noErr, LogAudioMixerAudioUnit, Error, TEXT("Failed to set input callback for audio output node"));

        OpenStreamParams.NumFrames = BufferSize;
		AudioStreamInfo.StreamState = EAudioOutputStreamState::Closed;
        
		bInitialized = true;

		return true;
	}

	bool FMixerPlatformAudioUnit::CheckAudioDeviceChange()
	{
		//TODO
		return false;
	}

	bool FMixerPlatformAudioUnit::TeardownHardware()
	{
		if(!bInitialized)
		{
			return true;
		}
		
		StopAudioStream();
		CloseAudioStream();

		DisposeAUGraph(AudioUnitGraph);

		AudioUnitGraph = nullptr;
		OutputNode = -1;
		OutputUnit = nullptr;

		bInitialized = false;
		
		return true;
	}

	bool FMixerPlatformAudioUnit::IsInitialized() const
	{
		return bInitialized;
	}

	bool FMixerPlatformAudioUnit::GetNumOutputDevices(uint32& OutNumOutputDevices)
	{
		OutNumOutputDevices = 1;
		
		return true;
	}

	bool FMixerPlatformAudioUnit::GetOutputDeviceInfo(const uint32 InDeviceIndex, FAudioPlatformDeviceInfo& OutInfo)
	{
		OutInfo = AudioStreamInfo.DeviceInfo;
		return true;
	}

	bool FMixerPlatformAudioUnit::GetDefaultOutputDeviceIndex(uint32& OutDefaultDeviceIndex) const
	{
		OutDefaultDeviceIndex = 0;
		
		return true;
	}

	bool FMixerPlatformAudioUnit::OpenAudioStream(const FAudioMixerOpenStreamParams& Params)
	{
		if (!bInitialized || AudioStreamInfo.StreamState != EAudioOutputStreamState::Closed)
		{
			return false;
		}
		
		AudioStreamInfo.OutputDeviceIndex = Params.OutputDeviceIndex;
		AudioStreamInfo.AudioMixer = Params.AudioMixer;
		
		// Initialize the audio unit graph
		OSStatus Status = AUGraphInitialize(AudioUnitGraph);
		if (Status != noErr)
		{
			HandleError(TEXT("Failed to initialize audio graph!"), true);
			return false;
		}

		AudioStreamInfo.StreamState = EAudioOutputStreamState::Open;

		return true;
	}

	bool FMixerPlatformAudioUnit::CloseAudioStream()
	{
		if (!bInitialized || (AudioStreamInfo.StreamState != EAudioOutputStreamState::Open && AudioStreamInfo.StreamState != EAudioOutputStreamState::Stopped))
		{
			return false;
		}
		
		AudioStreamInfo.StreamState = EAudioOutputStreamState::Closed;
		
		return true;
	}

	bool FMixerPlatformAudioUnit::StartAudioStream()
	{
		if (!bInitialized || (AudioStreamInfo.StreamState != EAudioOutputStreamState::Open && AudioStreamInfo.StreamState != EAudioOutputStreamState::Stopped))
		{
			return false;
		}
        
		BeginGeneratingAudio();
		
		// This will start the render audio callback
		OSStatus Status = AUGraphStart(AudioUnitGraph);
		if (Status != noErr)
		{
			HandleError(TEXT("Failed to start audio graph!"), true);
			return false;
		}

		return true;
	}

	bool FMixerPlatformAudioUnit::StopAudioStream()
	{
		if(!bInitialized || AudioStreamInfo.StreamState != EAudioOutputStreamState::Running)
		{
			return false;
		}
		
		AudioStreamInfo.StreamState = EAudioOutputStreamState::Stopping;
		
		AUGraphStop(AudioUnitGraph);

		AudioStreamInfo.StreamState = EAudioOutputStreamState::Stopped;
		
		return true;
	}

	bool FMixerPlatformAudioUnit::MoveAudioStreamToNewAudioDevice(const FString& InNewDeviceId)
	{
        //TODO
        
		return false;
	}

	FAudioPlatformDeviceInfo FMixerPlatformAudioUnit::GetPlatformDeviceInfo() const
	{
        FAudioPlatformDeviceInfo DeviceInfo;
        
    #if PLATFORM_IOS || PLATFORM_TVOS
        AVAudioSession* AudioSession = [AVAudioSession sharedInstance];
        double SampleRate = [AudioSession preferredSampleRate];
        DeviceInfo.SampleRate = (int32)SampleRate;
#else
        DeviceInfo.SampleRate = GraphSampleRate;
#endif
        DeviceInfo.NumChannels = 2;
        DeviceInfo.Format = EAudioMixerStreamDataFormat::Float;
        DeviceInfo.OutputChannelArray.SetNum(2);
        DeviceInfo.OutputChannelArray[0] = EAudioMixerChannel::FrontLeft;
        DeviceInfo.OutputChannelArray[1] = EAudioMixerChannel::FrontRight;
        DeviceInfo.bIsSystemDefault = true;
        
        return DeviceInfo;
	}

	void FMixerPlatformAudioUnit::SubmitBuffer(const uint8* Buffer)
	{
        SubmittedBufferPtr = (uint8*) Buffer;
        SubmittedBytes = 0;
        RemainingBytesInCurrentSubmittedBuffer = BytesPerSubmittedBuffer;
	}

	FName FMixerPlatformAudioUnit::GetRuntimeFormat(USoundWave* InSoundWave)
	{
#if PLATFORM_IOS || PLATFORM_TVOS
		static FName NAME_ADPCM(TEXT("ADPCM"));
		return NAME_ADPCM;
#else
        static FName NAME_OPUS(TEXT("OPUS"));
        
        if (InSoundWave->IsStreaming())
        {
            return NAME_OPUS;
        }
        static FName NAME_OGG(TEXT("OGG"));
        return NAME_OGG;
#endif
	}

	bool FMixerPlatformAudioUnit::HasCompressedAudioInfoClass(USoundWave* InSoundWave)
	{
		return true;
	}

	ICompressedAudioInfo* FMixerPlatformAudioUnit::CreateCompressedAudioInfo(USoundWave* InSoundWave)
	{
#if PLATFORM_IOS || PLATFORM_TVOS
		return new FADPCMAudioInfo();
#else
        check(InSoundWave);
        
        if (InSoundWave->IsStreaming())
        {
            return new FOpusAudioInfo();
        }
        
#if WITH_OGGVORBIS
        static const FName NAME_OGG(TEXT("OGG"));
        if (FPlatformProperties::RequiresCookedData() ? InSoundWave->HasCompressedData(NAME_OGG) : (InSoundWave->GetCompressedData(NAME_OGG) != nullptr))
        {
            ICompressedAudioInfo* CompressedInfo = new FVorbisAudioInfo();
            if (!CompressedInfo)
            {
                UE_LOG(LogAudio, Error, TEXT("Failed to create new FVorbisAudioInfo for SoundWave %s: out of memory."), *InSoundWave->GetName());
                return nullptr;
            }
            return CompressedInfo;
        }
        else
        {
            return nullptr;
        }
#else
        return nullptr;
#endif // WITH_OGGVORBIS
#endif // PLATFORM_IOS || PLATFORM_TVOS
	}

	FString FMixerPlatformAudioUnit::GetDefaultDeviceName()
	{
		return FString();
	}

	FAudioPlatformSettings FMixerPlatformAudioUnit::GetPlatformSettings() const
	{
        FAudioPlatformSettings Settings;
        Settings.NumBuffers = 2;
#if PLATFORM_IOS || PLATFORM_TVOS
        AVAudioSession* AudioSession = [AVAudioSession sharedInstance];
        double BufferSizeInSec = [AudioSession preferredIOBufferDuration];
        double SampleRate = [AudioSession preferredSampleRate];
        
        int32 NumFrames;
        
        if (BufferSizeInSec == 0.0)
        {
            NumFrames = DefaultBufferSize;
        }
        else
        {
            NumFrames = (int32)(SampleRate * BufferSizeInSec);
        }
        Settings.CallbackBufferFrameSize = NumFrames;
        Settings.SampleRate = SampleRate;
        Settings.MaxChannels = 32;
        
#else
        Settings.SampleRate = GraphSampleRate;
        Settings.CallbackBufferFrameSize = DefaultBufferSize;
        
#endif //#if PLATFORM_IOS || PLATFORM_TVOS
        
        return Settings;
	}

	void FMixerPlatformAudioUnit::ResumeContext()
	{
        if (SuspendCounter > 0)
        {
            FPlatformAtomics::InterlockedDecrement(&SuspendCounter);
            AUGraphStart(AudioUnitGraph);
            UE_LOG(LogAudioMixerAudioUnit, Display, TEXT("Resuming Audio"));
            bSuspended = false;
        }
	}
	
	void FMixerPlatformAudioUnit::SuspendContext()
	{
		if (SuspendCounter == 0)
		{
            FPlatformAtomics::InterlockedIncrement(&SuspendCounter);
			AUGraphStop(AudioUnitGraph);
			UE_LOG(LogAudioMixerAudioUnit, Display, TEXT("Suspending Audio"));
			bSuspended = true;
		}
	}

	void FMixerPlatformAudioUnit::HandleError(const TCHAR* InLogOutput, bool bTeardown)
	{
		UE_LOG(LogAudioMixerAudioUnit, Log, TEXT("%s"), InLogOutput);
		if (bTeardown)
		{
			TeardownHardware();
		}
	}

	bool FMixerPlatformAudioUnit::PerformCallback(AudioBufferList* OutputBufferData)
	{
		bInCallback = true;
		
		if (AudioStreamInfo.StreamState == EAudioOutputStreamState::Running)
		{
            if (!SubmittedBufferPtr)
            {
                ReadNextBuffer();
            }

            // How many bytes we have left over from previous callback
            int32 SubmittedBufferBytesLeft = BytesPerSubmittedBuffer - SubmittedBytes;
            int32 OutputBufferBytesLeft = OutputBufferData->mBuffers[0].mDataByteSize;
            uint8* OutputBufferPtr = (uint8*) OutputBufferData->mBuffers[0].mData;
            while (OutputBufferBytesLeft > 0)
            {
                const int32 BytesToCopy = FMath::Min(SubmittedBufferBytesLeft, OutputBufferBytesLeft);
                
                FMemory::Memcpy((void*) OutputBufferPtr, SubmittedBufferPtr + SubmittedBytes, BytesToCopy);
                
                OutputBufferBytesLeft -= BytesToCopy;
                SubmittedBufferBytesLeft -= BytesToCopy;
                
                if (SubmittedBufferBytesLeft <= 0)
                {
                    ReadNextBuffer();
                    SubmittedBytes = 0;
                    SubmittedBufferBytesLeft = BytesPerSubmittedBuffer;
                }
                else
                {
                    SubmittedBytes += BytesToCopy;
                }
                
                if (OutputBufferBytesLeft <= 0)
                {
                    break;
                }
                else
                {
                    OutputBufferPtr += BytesToCopy;
                }
            }
		}
		else
		{
			for (uint32 bufferItr = 0; bufferItr < OutputBufferData->mNumberBuffers; ++bufferItr)
			{
				memset(OutputBufferData->mBuffers[bufferItr].mData, 0, OutputBufferData->mBuffers[bufferItr].mDataByteSize);
			}
		}
		
		bInCallback = false;
		
		return true;
	}

	OSStatus FMixerPlatformAudioUnit::AudioRenderCallback(void* RefCon, AudioUnitRenderActionFlags* ActionFlags,
														  const AudioTimeStamp* TimeStamp, UInt32 BusNumber,
														  UInt32 NumFrames, AudioBufferList* IOData)
	{
		// Get the user data and cast to our FMixerPlatformCoreAudio object
		FMixerPlatformAudioUnit* me = (FMixerPlatformAudioUnit*) RefCon;
		
		me->PerformCallback(IOData);
		
		return noErr;
	}
}
