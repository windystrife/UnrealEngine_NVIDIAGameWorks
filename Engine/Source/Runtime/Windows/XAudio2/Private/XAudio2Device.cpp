// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	XeAudioDevice.cpp: Unreal XAudio2 Audio interface object.

	Unreal is RHS with Y and Z swapped (or technically LHS with flipped axis)

=============================================================================*/

/*------------------------------------------------------------------------------------
	Audio includes.
------------------------------------------------------------------------------------*/

#include "XAudio2Device.h"
#include "AudioEffect.h"
#include "OpusAudioInfo.h"
#include "VorbisAudioInfo.h"
#include "XAudio2Effects.h"
#include "Interfaces/IAudioFormat.h"
#include "HAL/PlatformAffinity.h"
#include "WindowsHWrapper.h"
#include "AllowWindowsPlatformTypes.h"
#include "AllowWindowsPlatformAtomics.h"
THIRD_PARTY_INCLUDES_START
	#include <xapobase.h>
	#include <xapofx.h>
	#include <xaudio2fx.h>
THIRD_PARTY_INCLUDES_END
#include "HideWindowsPlatformAtomics.h"
#include "HideWindowsPlatformTypes.h"
#include "XAudio2Support.h"
#include "Runtime/HeadMountedDisplay/Public/IHeadMountedDisplayModule.h"

#if WITH_XMA2
#include "XMAAudioInfo.h"
#endif

DEFINE_LOG_CATEGORY(LogXAudio2);

class FXAudio2DeviceModule : public IAudioDeviceModule
{
public:

	/** Creates a new instance of the audio device implemented by the module. */
	virtual FAudioDevice* CreateAudioDevice() override
	{
		return new FXAudio2Device;
	}
};

IMPLEMENT_MODULE(FXAudio2DeviceModule, XAudio2);


/*------------------------------------------------------------------------------------
Static variables from the early init
------------------------------------------------------------------------------------*/

// The number of speakers producing sound (stereo or 5.1)
int32 FXAudioDeviceProperties::NumSpeakers = 0;
const float* FXAudioDeviceProperties::OutputMixMatrix = NULL;
#if XAUDIO_SUPPORTS_DEVICE_DETAILS
XAUDIO2_DEVICE_DETAILS FXAudioDeviceProperties::DeviceDetails;
#endif	//XAUDIO_SUPPORTS_DEVICE_DETAILS

/*------------------------------------------------------------------------------------
	FAudioDevice Interface.
------------------------------------------------------------------------------------*/

#define DEBUG_XAUDIO2 0

void FXAudio2Device::GetAudioDeviceList(TArray<FString>& OutAudioDeviceNames) const
{
	DeviceProperties->GetAudioDeviceList(OutAudioDeviceNames);
}

bool FXAudio2Device::InitializeHardware()
{
	bIsAudioDeviceHardwareInitialized = false;

	bHardwareChanged = false;

	if (IsRunningDedicatedServer())
	{
		return false;
	}

	// Create a new DeviceProperties object
	DeviceProperties = new FXAudioDeviceProperties;
	// null out the non-static data
	DeviceProperties->XAudio2 = nullptr;
	DeviceProperties->MasteringVoice = nullptr;

#if WITH_OGGVORBIS
	// Load ogg and vorbis dlls if they haven't been loaded yet
	LoadVorbisLibraries();
#endif

	SampleRate = UE4_XAUDIO2_SAMPLERATE;

#if PLATFORM_WINDOWS
	bComInitialized = FWindowsPlatformMisc::CoInitialize();
#if PLATFORM_64BITS
	// Work around the fact the x64 version of XAudio2_7.dll does not properly ref count
	// by forcing it to be always loaded

	// Load the xaudio2 library and keep a handle so we can free it on teardown
	// Note: windows internally ref-counts the library per call to load library so 
	// when we call FreeLibrary, it will only free it once the refcount is zero
	DeviceProperties->XAudio2Dll = LoadLibraryA("XAudio2_7.dll");

	// returning null means we failed to load XAudio2, which means everything will fail
	if (DeviceProperties->XAudio2Dll == nullptr)
	{
		UE_LOG(LogInit, Warning, TEXT("Failed to load XAudio2 dll"));
		return false;
	}
#endif	//PLATFORM_64BITS
#endif	//PLATFORM_WINDOWS

#if DEBUG_XAUDIO2
	uint32 Flags = XAUDIO2_DEBUG_ENGINE;
#else
	uint32 Flags = 0;
#endif

#if WITH_XMA2
	// We don't use all of the SHAPE processor, so this flag prevents wasted resources
	Flags |= XAUDIO2_DO_NOT_USE_SHAPE;
#endif

	// Create a new XAudio2 device object instance
	if (XAudio2Create(&DeviceProperties->XAudio2, Flags, (XAUDIO2_PROCESSOR)FPlatformAffinity::GetAudioThreadMask()) != S_OK)
	{
		UE_LOG(LogInit, Log, TEXT("Failed to create XAudio2 interface"));
		return(false);
	}

	check(DeviceProperties->XAudio2 != nullptr);

#if XAUDIO_SUPPORTS_DEVICE_DETAILS
	UINT32 DeviceCount = 0;
	ValidateAPICall(TEXT("GetDeviceCount"),
		DeviceProperties->XAudio2->GetDeviceCount(&DeviceCount));
	if( DeviceCount < 1 )
	{
		UE_LOG(LogInit, Log, TEXT( "No audio devices found!" ) );
		DeviceProperties->XAudio2->Release();
		DeviceProperties->XAudio2 = nullptr;
		return( false );		
	}

	// Initialize the audio device index to 0, which is the windows default device index
	UINT32 DeviceIndex = 0;

#if XAUDIO_SUPPORTS_DEVICE_DETAILS
	FString WindowsAudioDeviceName;
	GConfig->GetString(TEXT("/Script/WindowsTargetPlatform.WindowsTargetSettings"), TEXT("AudioDevice"), WindowsAudioDeviceName, GEngineIni);

	// Allow HMD to specify audio device, if one was not specified in settings
	if (WindowsAudioDeviceName.IsEmpty() && FAudioDevice::CanUseVRAudioDevice() && IHeadMountedDisplayModule::IsAvailable())
	{
		WindowsAudioDeviceName = IHeadMountedDisplayModule::Get().GetAudioOutputDevice();
	}
	
	// If an audio device was specified, try to find it
	if (!WindowsAudioDeviceName.IsEmpty())
	{
		uint32 NumDevices = 0;
		DeviceProperties->XAudio2->GetDeviceCount(&NumDevices);

		for (uint32 i = 0; i < NumDevices; ++i)
		{
			XAUDIO2_DEVICE_DETAILS Details;
			DeviceProperties->XAudio2->GetDeviceDetails(i, &Details);

			if (FString(Details.DeviceID) == WindowsAudioDeviceName || FString(Details.DisplayName) == WindowsAudioDeviceName)
			{
				DeviceIndex = i;
				break;
			}
		}
	}

	// Get the details of the desired device index (0 is default)
	if (!ValidateAPICall(TEXT("GetDeviceDetails"),
		DeviceProperties->XAudio2->GetDeviceDetails(DeviceIndex, &FXAudioDeviceProperties::DeviceDetails)))
	{
		UE_LOG(LogInit, Log, TEXT("Failed to get DeviceDetails for XAudio2"));
		DeviceProperties->XAudio2 = nullptr;
		return(false);
	}
#endif // #if XAUDIO_SUPPORTS_DEVICE_DETAILS

#if DEBUG_XAUDIO2
	XAUDIO2_DEBUG_CONFIGURATION DebugConfig = {0};
	DebugConfig.TraceMask = XAUDIO2_LOG_WARNINGS | XAUDIO2_LOG_DETAIL;
	DebugConfig.BreakMask = XAUDIO2_LOG_ERRORS;
	DeviceProperties->XAudio2->SetDebugConfiguration(&DebugConfig);
#endif // #if DEBUG_XAUDIO2

	FXAudioDeviceProperties::NumSpeakers = UE4_XAUDIO2_NUMCHANNELS;
	SampleRate = FXAudioDeviceProperties::DeviceDetails.OutputFormat.Format.nSamplesPerSec;

	// Clamp the output frequency to the limits of the reverb XAPO
	if( SampleRate > XAUDIO2FX_REVERB_MAX_FRAMERATE )
	{
		SampleRate = XAUDIO2FX_REVERB_MAX_FRAMERATE;
		FXAudioDeviceProperties::DeviceDetails.OutputFormat.Format.nSamplesPerSec = SampleRate;
	}

	UE_LOG(LogInit, Log, TEXT( "XAudio2 using '%s' : %d channels at %g kHz using %d bits per sample (channel mask 0x%x)" ), 
		FXAudioDeviceProperties::DeviceDetails.DisplayName,
		FXAudioDeviceProperties::NumSpeakers, 
		( float )SampleRate / 1000.0f, 
		FXAudioDeviceProperties::DeviceDetails.OutputFormat.Format.wBitsPerSample,
		(uint32)UE4_XAUDIO2_CHANNELMASK );

	if( !GetOutputMatrix( UE4_XAUDIO2_CHANNELMASK, FXAudioDeviceProperties::NumSpeakers ) )
	{
		UE_LOG(LogInit, Log, TEXT( "Unsupported speaker configuration for this number of channels" ) );
		DeviceProperties->XAudio2 = NULL;
		return( false );
	}

	// Create the final output voice with either 2 or 6 channels
	if (!ValidateAPICall(TEXT("CreateMasteringVoice"), 
		DeviceProperties->XAudio2->CreateMasteringVoice(&DeviceProperties->MasteringVoice, FXAudioDeviceProperties::NumSpeakers, SampleRate, 0, DeviceIndex, nullptr)))
	{
		UE_LOG(LogInit, Warning, TEXT( "Failed to create the mastering voice for XAudio2" ) );
		DeviceProperties->XAudio2 = nullptr;
		return( false );
	}
#else	// #if XAUDIO_SUPPORTS_DEVICE_DETAILS
	// Create the final output voice
	if (!ValidateAPICall(TEXT("CreateMasteringVoice"),
		DeviceProperties->XAudio2->CreateMasteringVoice(&DeviceProperties->MasteringVoice, UE4_XAUDIO2_NUMCHANNELS, UE4_XAUDIO2_SAMPLERATE, 0, 0, nullptr )))
	{
		UE_LOG(LogInit, Warning, TEXT( "Failed to create the mastering voice for XAudio2" ) );
		DeviceProperties->XAudio2 = nullptr;
		return false;
	}
#endif	// #else // #if XAUDIO_SUPPORTS_DEVICE_DETAILS

	DeviceProperties->SpatializationHelper.Init();

	// Set that we initialized our hardware audio device ok so we should use real voices.
	bIsAudioDeviceHardwareInitialized = true;

	// Initialize permanent memory stack for initial & always loaded sound allocations.
	if( CommonAudioPoolSize )
	{
		UE_LOG(LogAudio, Log, TEXT( "Allocating %g MByte for always resident audio data" ), CommonAudioPoolSize / ( 1024.0f * 1024.0f ) );
		CommonAudioPoolFreeBytes = CommonAudioPoolSize;
		CommonAudioPool = ( uint8* )FMemory::Malloc( CommonAudioPoolSize );
	}
	else
	{
		UE_LOG(LogAudio, Log, TEXT( "CommonAudioPoolSize is set to 0 - disabling persistent pool for audio data" ) );
		CommonAudioPoolFreeBytes = 0;
	}

	// Now initialize the audio clock voice after xaudio2 is initialized
	DeviceProperties->InitAudioClockVoice();

#if WITH_XMA2
	FXMAAudioInfo::Initialize();
#endif

	return true;
}

void FXAudio2Device::TeardownHardware()
{
	if (DeviceProperties)
	{
		delete DeviceProperties;
		DeviceProperties = nullptr;
	}

#if PLATFORM_WINDOWS
	if (bComInitialized)
	{
		FWindowsPlatformMisc::CoUninitialize();
	}
#endif
}

void FXAudio2Device::UpdateHardware()
{
	// If the audio device changed, we need to tear down and restart the audio engine state
	if (DeviceProperties->DidAudioDeviceChange())
	{
		//Cache the current audio clock.
		CachedAudioClockStartTime = GetAudioClock();

		// Flush stops all sources so sources can be safely deleted below.
		Flush(nullptr);

		// Remove the effects manager
		if (Effects)
		{
			delete Effects;
			Effects = nullptr;
		}
	
		// Teardown hardware
		TeardownHardware();
		
		// Restart the hardware
		InitializeHardware();

		// Recreate the effects manager
		Effects = CreateEffectsManager();

		// Now reset and restart the sound source objects
		FreeSources.Reset();
		Sources.Reset();

		InitSoundSources();
	}
}

void FXAudio2Device::UpdateAudioClock()
{
	// Update the audio clock time
	const double NewAudioClock = DeviceProperties->GetAudioClockTime();

	// If the device properties failed at getting an audio clock, then fallback to using device delta time
	if (NewAudioClock == 0.0)
	{
		AudioClock += GetDeviceDeltaTime();
	}
	else
	{
		AudioClock = NewAudioClock + CachedAudioClockStartTime;
	}
}

FAudioEffectsManager* FXAudio2Device::CreateEffectsManager()
{
	// Create the effects subsystem (reverb, EQ, etc.)
	return new FXAudio2EffectsManager(this);
}

FSoundSource* FXAudio2Device::CreateSoundSource()
{
	// create source source object
	return new FXAudio2SoundSource(this);
}

bool FXAudio2Device::HasCompressedAudioInfoClass(USoundWave* SoundWave)
{
#if WITH_OGGVORBIS || WITH_XMA2
	return true;
#else
	return false;
#endif
}

class ICompressedAudioInfo* FXAudio2Device::CreateCompressedAudioInfo(USoundWave* SoundWave)
{
	check(SoundWave);

	if (SoundWave->IsStreaming())
	{
		return new FOpusAudioInfo();
	}

#if WITH_OGGVORBIS
	static const FName NAME_OGG(TEXT("OGG"));
	if (FPlatformProperties::RequiresCookedData() ? SoundWave->HasCompressedData(NAME_OGG) : (SoundWave->GetCompressedData(NAME_OGG) != nullptr))
	{
		ICompressedAudioInfo* CompressedInfo = new FVorbisAudioInfo();
		if (!CompressedInfo)
		{
			UE_LOG(LogAudio, Error, TEXT("Failed to create new FVorbisAudioInfo for SoundWave %s: out of memory."), *SoundWave->GetName());
			return nullptr;
		}
		return CompressedInfo;
	}
#endif

#if WITH_XMA2
	static const FName NAME_XMA(TEXT("XMA"));
	if (FPlatformProperties::RequiresCookedData() ? SoundWave->HasCompressedData(NAME_XMA) : (SoundWave->GetCompressedData(NAME_XMA) != nullptr))
	{
		ICompressedAudioInfo* CompressedInfo = new FXMAAudioInfo();
		if (!CompressedInfo)
		{
			UE_LOG(LogAudio, Error, TEXT("Failed to create new FXMAAudioInfo for SoundWave %s: out of memory."), *SoundWave->GetName());
			return nullptr;
		}
		return CompressedInfo;
	}
#endif

	return nullptr;
}

/**  
 * Check for errors and output a human readable string 
 */
bool FXAudio2Device::ValidateAPICall( const TCHAR* Function, uint32 ErrorCode )
{
	return DeviceProperties->Validate(Function, ErrorCode);
}

/** 
 * Derives the output matrix to use based on the channel mask and the number of channels
 */
const float OutputMatrixMono[SPEAKER_COUNT] = 
{ 
	0.7f, 0.7f, 0.5f, 0.0f, 0.5f, 0.5f	
};

const float OutputMatrix2dot0[SPEAKER_COUNT * 2] = 
{ 
	1.0f, 0.0f, 0.7f, 0.0f, 1.25f, 0.0f, // FL 
	0.0f, 1.0f, 0.7f, 0.0f, 0.0f, 1.25f  // FR
};

//	const float OutputMatrixDownMix[SPEAKER_COUNT * 2] = 
//	{ 
//		1.0f, 0.0f, 0.7f, 0.0f, 0.87f, 0.49f,  
//		0.0f, 1.0f, 0.7f, 0.0f, 0.49f, 0.87f 
//	};

const float OutputMatrix2dot1[SPEAKER_COUNT * 3] = 
{ 
	// Same as stereo, but also passing in LFE signal
	1.0f, 0.0f, 0.7f, 0.0f, 1.25f, 0.0f, // FL
	0.0f, 1.0f, 0.7f, 0.0f, 0.0f, 1.25f, // FR
	0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,  // LFE
};

const float OutputMatrix4dot0s[SPEAKER_COUNT * 4] = 
{ 
	// Combine both rear channels to make a rear center channel
	1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, // FL
	0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, // FR
	0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, // FC
	0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f  // RC
};

const float OutputMatrix4dot0[SPEAKER_COUNT * 4] = 
{ 
	// Split the center channel to the front two speakers
	1.0f, 0.0f, 0.7f, 0.0f, 0.0f, 0.0f, // FL
	0.0f, 1.0f, 0.7f, 0.0f, 0.0f, 0.0f, // FR
	0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, // RL
	0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f  // RR
};

const float OutputMatrix4dot1[SPEAKER_COUNT * 5] = 
{ 
	// Split the center channel to the front two speakers
	1.0f, 0.0f, 0.7f, 0.0f, 0.0f, 0.0f, // FL
	0.0f, 1.0f, 0.7f, 0.0f, 0.0f, 0.0f, // FR
	0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, // LFE
	0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, // RL
	0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f  // RR
};

const float OutputMatrix5dot0[SPEAKER_COUNT * 5] = 
{ 
	// Split the center channel to the front two speakers
	1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, // FL
	0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, // FR
	0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, // FC
	0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, // SL
	0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f  // SR
};

const float OutputMatrix5dot1[SPEAKER_COUNT * 6] = 
{ 
	// Classic 5.1 setup
	1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, // FL
	0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, // FR
	0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, // FC
	0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, // LFE
	0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, // RL
	0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f  // RR
};

const float OutputMatrix5dot1s[SPEAKER_COUNT * 6] = 
{ 
	// Classic 5.1 setup
	1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, // FL
	0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, // FR
	0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, // FC
	0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, // LFE
	0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, // SL
	0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f  // SR
};

const float OutputMatrix6dot1[SPEAKER_COUNT * 7] = 
{ 
	1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, // FL
	0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, // FR
	0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, // FC
	0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, // LFE
	0.0f, 0.0f, 0.0f, 0.0f, 0.8f, 0.0f, // RL
	0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.8f, // RR
	0.0f, 0.0f, 0.0f, 0.0f, 0.5f, 0.5f, // RC
};

const float OutputMatrix7dot1[SPEAKER_COUNT * 8] = 
{ 
	0.7f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, // FL
	0.0f, 0.7f, 0.0f, 0.0f, 0.0f, 0.0f, // FR
	0.0f, 0.0f, 0.7f, 0.0f, 0.0f, 0.0f, // FC
	0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, // LFE
	0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, // RL
	0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, // RR
	0.7f, 0.0f, 0.5f, 0.0f, 0.0f, 0.0f, // FCL
	0.0f, 0.7f, 0.5f, 0.0f, 0.0f, 0.0f  // FCR
};

const float OutputMatrix7dot1s[SPEAKER_COUNT * 8] = 
{ 
	// Split the rear channels evenly between side and rear
	1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, // FL
	0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, // FR
	0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, // FC
	0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, // LFE
	0.0f, 0.0f, 0.0f, 0.0f, 0.7f, 0.0f, // RL
	0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.7f, // RR
	0.0f, 0.0f, 0.0f, 0.0f, 0.7f, 0.0f, // SL
	0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.7f  // SR
};

typedef struct SOuputMapping
{
	uint32 NumChannels;
	uint32 SpeakerMask;
	const float* OuputMatrix;
} TOuputMapping;

TOuputMapping OutputMappings[] =
{
	{ 1, SPEAKER_MONO, OutputMatrixMono },
	{ 2, SPEAKER_STEREO, OutputMatrix2dot0 },
	{ 3, SPEAKER_2POINT1, OutputMatrix2dot1 },
	{ 4, SPEAKER_SURROUND, OutputMatrix4dot0s },
	{ 4, SPEAKER_QUAD, OutputMatrix4dot0 },
	{ 5, SPEAKER_4POINT1, OutputMatrix4dot1 },
	{ 5, SPEAKER_5POINT0, OutputMatrix5dot0 },
	{ 6, SPEAKER_5POINT1, OutputMatrix5dot1 },
	{ 6, SPEAKER_5POINT1_SURROUND, OutputMatrix5dot1s },
	{ 7, SPEAKER_6POINT1, OutputMatrix6dot1 },
	{ 8, SPEAKER_7POINT1, OutputMatrix7dot1 },
	{ 8, SPEAKER_7POINT1_SURROUND, OutputMatrix7dot1s }
};

bool FXAudio2Device::GetOutputMatrix( uint32 ChannelMask, uint32 NumChannels )
{
	// Default is vanilla stereo
	FXAudioDeviceProperties::OutputMixMatrix = OutputMatrix2dot0;

	// Find the best match
	for( int32 MappingIndex = 0; MappingIndex < sizeof( OutputMappings ) / sizeof( TOuputMapping ); MappingIndex++ )
	{
		if( NumChannels != OutputMappings[MappingIndex].NumChannels )
		{
			continue;
		}

		if( ( ChannelMask & OutputMappings[MappingIndex].SpeakerMask ) != ChannelMask )
		{
			continue;
		}

		FXAudioDeviceProperties::OutputMixMatrix = OutputMappings[MappingIndex].OuputMatrix;
		break;
	}

	return( FXAudioDeviceProperties::OutputMixMatrix != NULL );
}


/** Test decompress a vorbis file */
void FXAudio2Device::TestDecompressOggVorbis( USoundWave* Wave )
{
#if WITH_OGGVORBIS
	FVorbisAudioInfo	OggInfo;
	FSoundQualityInfo	QualityInfo = { 0 };

	// Parse the ogg vorbis header for the relevant information
	if( OggInfo.ReadCompressedInfo( Wave->ResourceData, Wave->ResourceSize, &QualityInfo ) )
	{
		// Extract the data
		Wave->SampleRate = QualityInfo.SampleRate;
		Wave->NumChannels = QualityInfo.NumChannels;
		Wave->RawPCMDataSize = QualityInfo.SampleDataSize;
		Wave->Duration = QualityInfo.Duration;

		Wave->RawPCMData = ( uint8* )FMemory::Malloc( Wave->RawPCMDataSize );

		// Decompress all the sample data (and preallocate memory)
		OggInfo.ExpandFile( Wave->RawPCMData, &QualityInfo );

		FMemory::Free( Wave->RawPCMData );
	}
#endif
}

#if !UE_BUILD_SHIPPING
/** Decompress a wav a number of times for profiling purposes */
void FXAudio2Device::TimeTest( FOutputDevice& Ar, const TCHAR* WaveAssetName )
{
	USoundWave* Wave = LoadObject<USoundWave>( NULL, WaveAssetName, NULL, LOAD_NoWarn, NULL );
	if( Wave )
	{
		// Wait for initial decompress
		if (Wave->AudioDecompressor)
		{
			while (!Wave->AudioDecompressor->IsDone())
			{
			}

			delete Wave->AudioDecompressor;
			Wave->AudioDecompressor = NULL;
		}
		
		// If the wave loaded in fine, time the decompression
		Wave->InitAudioResource(GetRuntimeFormat(Wave));

		double Start = FPlatformTime::Seconds();

		for( int32 i = 0; i < 1000; i++ )
		{
			TestDecompressOggVorbis( Wave );
		} 

		double Duration = FPlatformTime::Seconds() - Start;
		Ar.Logf( TEXT( "%s: %g ms - %g ms per second per channel" ), WaveAssetName, Duration, Duration / ( Wave->Duration * Wave->NumChannels ) );

		Wave->RemoveAudioResource();
	}
	else
	{
		Ar.Logf( TEXT( "Failed to find test file '%s' to decompress" ), WaveAssetName );
	}
}
#endif // !UE_BUILD_SHIPPING

/**
 * Exec handler used to parse console commands.
 *
 * @param	InWorld		World context
 * @param	Cmd		Command to parse
 * @param	Ar		Output device to use in case the handler prints anything
 * @return	true if command was handled, false otherwise
 */
bool FXAudio2Device::Exec( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar )
{
	if( FAudioDevice::Exec( InWorld, Cmd, Ar ) )
	{
		return( true );
	}
#if !UE_BUILD_SHIPPING
	else if( FParse::Command( &Cmd, TEXT( "TestVorbisDecompressionSpeed" ) ) )
	{

		TimeTest( Ar, TEXT( "TestSounds.44Mono_TestWeaponSynthetic" ) );
		TimeTest( Ar, TEXT( "TestSounds.44Mono_TestDialogFemale" ) );
		TimeTest( Ar, TEXT( "TestSounds.44Mono_TestDialogMale" ) );

		TimeTest( Ar, TEXT( "TestSounds.22Mono_TestWeaponSynthetic" ) );
		TimeTest( Ar, TEXT( "TestSounds.22Mono_TestDialogFemale" ) );
		TimeTest( Ar, TEXT( "TestSounds.22Mono_TestDialogMale" ) );

		TimeTest( Ar, TEXT( "TestSounds.22Stereo_TestMusicAcoustic" ) );
		TimeTest( Ar, TEXT( "TestSounds.44Stereo_TestMusicAcoustic" ) );
	}
#endif // !UE_BUILD_SHIPPING

	return( false );
}

/**
 * Allocates memory from permanent pool. This memory will NEVER be freed.
 *
 * @param	Size	Size of allocation.
 *
 * @return pointer to a chunk of memory with size Size
 */
void* FXAudio2Device::AllocatePermanentMemory( int32 Size, bool& AllocatedInPool )
{
	void* Allocation = NULL;
	
	// Fall back to using regular allocator if there is not enough space in permanent memory pool.
	if( Size > CommonAudioPoolFreeBytes )
	{
		Allocation = FMemory::Malloc( Size );
		check( Allocation );

		AllocatedInPool = false;
	}
	// Allocate memory from pool.
	else
	{
		uint8* CommonAudioPoolAddress = ( uint8* )CommonAudioPool;
		Allocation = CommonAudioPoolAddress + ( CommonAudioPoolSize - CommonAudioPoolFreeBytes );

		AllocatedInPool = true;
	}

	// Decrement available size regardless of whether we allocated from pool or used regular allocator
	// to allow us to log suggested size at the end of initial loading.
	CommonAudioPoolFreeBytes -= Size;
	
	return( Allocation );
}
