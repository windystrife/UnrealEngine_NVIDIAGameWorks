// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
 	IOSAudioDevice.cpp: Unreal IOSAudio audio interface object.
 =============================================================================*/

/*------------------------------------------------------------------------------------
	Includes
 ------------------------------------------------------------------------------------*/

#include "IOSAudioDevice.h"
#include "AudioEffect.h"
#include "ADPCMAudioInfo.h"

DEFINE_LOG_CATEGORY(LogIOSAudio);

class FIOSAudioDeviceModule : public IAudioDeviceModule
{
public:

	/** Creates a new instance of the audio device implemented by the module. */
	virtual FAudioDevice* CreateAudioDevice() override
	{
		return new FIOSAudioDevice;
	}
};

IMPLEMENT_MODULE(FIOSAudioDeviceModule, IOSAudio);

/*------------------------------------------------------------------------------------
	FIOSAudioDevice
 ------------------------------------------------------------------------------------*/

void FIOSAudioDevice::ResumeContext()
{
	int32& SuspendCounter = GetSuspendCounter();
	if (SuspendCounter > 0)
	{
		FPlatformAtomics::InterlockedDecrement(&SuspendCounter);

		AUGraphStart(AudioUnitGraph);
		AudioOutputUnitStart(OutputUnit);
		UE_LOG(LogIOSAudio, Display, TEXT("Resuming Audio"));
	}
}

void FIOSAudioDevice::SuspendContext()
{
	int32& SuspendCounter = GetSuspendCounter();
	if (SuspendCounter == 0)
	{
		FPlatformAtomics::InterlockedIncrement(&SuspendCounter);

		AudioOutputUnitStop(OutputUnit);
		AUGraphStop(AudioUnitGraph);
		UE_LOG(LogIOSAudio, Display, TEXT("Suspending Audio"));
	}
}

int32& FIOSAudioDevice::GetSuspendCounter()
{
	static int32 SuspendCounter = 0;
	return SuspendCounter;
}

FIOSAudioDevice::FIOSAudioDevice() :
	FAudioDevice(),
	AudioUnitGraph(NULL),
	OutputNode(0),
	MixerNode(0),
	NextBusNumber(0)
{
	bDisableAudioCaching = true;	// Do not allow DTYPE_Native buffers, only DTYPE_RealTime or DTYPE_Streaming since on the fly decompression is so cheap, it saves memory, and requires fewer code paths
}

bool FIOSAudioDevice::Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
{
	if (FParse::Command(&Cmd, TEXT("DumpAUGraph")) && AudioUnitGraph)
	{
		CAShow(AudioUnitGraph);
		return true;
	}
	
	return FAudioDevice::Exec(InWorld, Cmd, Ar);
}

bool FIOSAudioDevice::InitializeHardware()
{
	SIZE_T SampleSize = sizeof(AudioSampleType);
	double GraphSampleRate = 44100.0;

	if (!SetHardwareSampleRate(GraphSampleRate) || !SetAudioSessionActive(true))
	{
		HandleError(TEXT("Failed to establish the audio session!"));
		return false;
	}

	// Retrieve the actual hardware sample rate
	GetHardwareSampleRate(GraphSampleRate);

	// Linear PCM stream format
	MixerFormat.mFormatID         = kAudioFormatLinearPCM;
	MixerFormat.mFormatFlags	  = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagsNativeEndian | kAudioFormatFlagIsPacked;
	MixerFormat.mBytesPerPacket   = SampleSize;
	MixerFormat.mFramesPerPacket  = 1;
	MixerFormat.mBytesPerFrame    = SampleSize;
	MixerFormat.mChannelsPerFrame = 1;
	MixerFormat.mBitsPerChannel   = 8 * SampleSize;
	MixerFormat.mSampleRate       = GraphSampleRate;

	OSStatus Status = NewAUGraph(&AudioUnitGraph);
	if (Status != noErr)
	{
		HandleError(TEXT("Failed to create audio unit graph!"));
		return false;
	}

	AudioComponentDescription UnitDescription;

	// Setup audio output unit
	UnitDescription.componentType         = kAudioUnitType_Output;
	UnitDescription.componentSubType      = kAudioUnitSubType_RemoteIO;
	UnitDescription.componentManufacturer = kAudioUnitManufacturer_Apple;
	UnitDescription.componentFlags        = 0;
	UnitDescription.componentFlagsMask    = 0;
	Status = AUGraphAddNode(AudioUnitGraph, &UnitDescription, &OutputNode);
	if (Status != noErr)
	{
		HandleError(TEXT("Failed to initialize audio output node!"), true);
		return false;
	}

	// Setup audo mixer unit
	UnitDescription.componentType         = kAudioUnitType_Mixer;
#ifdef __IPHONE_9_0
    UnitDescription.componentSubType      = kAudioUnitSubType_SpatialMixer;
#else
	UnitDescription.componentSubType      = kAudioUnitSubType_AU3DMixerEmbedded;
#endif
	UnitDescription.componentManufacturer = kAudioUnitManufacturer_Apple;
	UnitDescription.componentFlags        = 0;
	UnitDescription.componentFlagsMask    = 0;
	Status = AUGraphAddNode(AudioUnitGraph, &UnitDescription, &MixerNode);
	if (Status != noErr)
	{
		HandleError(TEXT("Failed to initialize audio mixer node!"), true);
		return false;
	}
	
	Status = AUGraphOpen(AudioUnitGraph);
	if (Status != noErr)
	{
		HandleError(TEXT("Failed to open audio unit graph"), true);
		return false;
	}
	
	Status = AUGraphNodeInfo(AudioUnitGraph, OutputNode, NULL, &OutputUnit);
	if (Status != noErr)
	{
		HandleError(TEXT("Failed to retrieve output unit reference!"), true);
		return false;
	}
	
	Status = AUGraphNodeInfo(AudioUnitGraph, MixerNode, NULL, &MixerUnit);
	if (Status != noErr)
	{
		HandleError(TEXT("Failed to retrieve mixer unit reference!"), true);
		return false;
	}

	uint32 BusCount = MaxChannels * CHANNELS_PER_BUS;
	Status = AudioUnitSetProperty(MixerUnit,
	                              kAudioUnitProperty_ElementCount,
	                              kAudioUnitScope_Input,
	                              0,
	                              &BusCount,
	                              sizeof(BusCount));
	if (Status != noErr)
	{
		HandleError(TEXT("Failed to set kAudioUnitProperty_ElementCount for audio mixer unit!"), true);
		return false;
	}
	
	// Initialize sound source early on, allowing for render callback hookups
	InitSoundSources();

	// Setup the mixer unit sample rate
	Status = AudioUnitSetProperty(MixerUnit,
	                              kAudioUnitProperty_SampleRate,
	                              kAudioUnitScope_Output,
	                              0,
	                              &GraphSampleRate,
	                              sizeof(GraphSampleRate));
	if (Status != noErr)
	{
		HandleError(TEXT("Failed to set kAudioUnitProperty_SampleRate for audio mixer unit!"), true);
		return false;
	}
	
	// Connect mixer node output to output node input
	Status = AUGraphConnectNodeInput(AudioUnitGraph, MixerNode, 0, OutputNode, 0);
	if (Status != noErr)
	{
		HandleError(TEXT("Failed to connect mixer node to output node!"), true);
		return false;
	}

	// Initialize and start the audio unit graph
	Status = AUGraphInitialize(AudioUnitGraph);
	if (Status == noErr && GetSuspendCounter() == 0)
	{
		Status = AUGraphStart(AudioUnitGraph);
	}

	if (Status != noErr)
	{
		HandleError(TEXT("Failed to start audio graph!"), true);
		return false;
	}

	return true;
}

void FIOSAudioDevice::TeardownHardware()
{
	if (AudioUnitGraph)
	{
		AUGraphStop(AudioUnitGraph);
		DisposeAUGraph(AudioUnitGraph);

		AudioUnitGraph = NULL;
		OutputNode = -1;
		OutputUnit = NULL;
		MixerNode = -1;
		MixerUnit = NULL;
	}
}

void FIOSAudioDevice::UpdateHardware()
{
	const FListener& Listener = GetListeners()[0];
	PlayerLocation	= Listener.Transform.GetLocation();
	PlayerFacing	= Listener.GetFront();
	PlayerUp		= Listener.GetUp();
	PlayerRight		= Listener.GetRight();
}

void FIOSAudioDevice::HandleError(const TCHAR* InLogOutput, bool bTeardown)
{
	UE_LOG(LogIOSAudio, Log, TEXT("%s"), InLogOutput);
	if (bTeardown)
	{
		Teardown();
	}
}

class ICompressedAudioInfo* FIOSAudioDevice::CreateCompressedAudioInfo(USoundWave* SoundWave)
{
	return new FADPCMAudioInfo();
}


FAudioEffectsManager* FIOSAudioDevice::CreateEffectsManager()
{
	// Create the basic no-op effects manager
	return FAudioDevice::CreateEffectsManager();
}

FSoundSource* FIOSAudioDevice::CreateSoundSource()
{
	return new FIOSAudioSoundSource(this, NextBusNumber++);
}

