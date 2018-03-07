// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ALAudioDevice.cpp: Unreal OpenAL Audio interface object.

	Unreal is RHS with Y and Z swapped (or technically LHS with flipped axis)
=============================================================================*/

/*------------------------------------------------------------------------------------
	Audio includes.
------------------------------------------------------------------------------------*/

#include "ALAudioDevice.h"
#include "AudioEffect.h"
#include "VorbisAudioInfo.h"

DEFINE_LOG_CATEGORY(LogALAudio);


class FALAudioDeviceModule : public IAudioDeviceModule
{
public:

	/** Creates a new instance of the audio device implemented by the module. */
	virtual FAudioDevice* CreateAudioDevice() override
	{
		return new FALAudioDevice;
	}
};

IMPLEMENT_MODULE(FALAudioDeviceModule, ALAudio );
/*------------------------------------------------------------------------------------
	UALAudioDevice constructor and UObject interface.
------------------------------------------------------------------------------------*/
FALAudioDevice::FALAudioDevice()
	: HardwareDevice(nullptr)
	, SoundContext(nullptr)
{}

void FALAudioDevice::TeardownHardware( void )
{
	// Push any pending data to the hardware
	if( &alcProcessContext )
	{
		alcProcessContext( SoundContext );
	}

	// Destroy all sound sources
	for( int32 i = 0; i < Sources.Num(); i++ )
	{
		delete Sources[ i ];
	}
	Sources.Empty();
	FreeSources.Empty();

	// Destroy OpenAL buffers associated with this audio device
	FAudioDeviceManager* AudioDeviceManager = GEngine->GetAudioDeviceManager();
	check(AudioDeviceManager != nullptr);
	for( int32 i = 0; i < AudioDeviceManager->Buffers.Num(); i++ )
	{
		FALSoundBuffer* Buffer = static_cast<FALSoundBuffer*>(AudioDeviceManager->Buffers[i]);
		if( Buffer->AudioDevice == this )
		{
			alDeleteBuffers(1, &Buffer->BufferId);
			Buffer->AudioDevice = nullptr;
			Buffer->BufferId = 0;
		}
	}

	// Disable the context
	if( &alcMakeContextCurrent )
	{
		alcMakeContextCurrent(nullptr);
	}

	// Destroy the context
	if( &alcDestroyContext )
	{
		alcDestroyContext( SoundContext );
		SoundContext = NULL;
	}

	// Close the hardware device
	if( &alcCloseDevice )
	{
		if (HardwareDevice)
		{
			const ALCchar* DeviceName = alcGetString(HardwareDevice, ALC_DEVICE_SPECIFIER);
			UE_LOG(LogALAudio, Log, TEXT("Closing ALAudio device : %s"), StringCast<TCHAR>(static_cast<const ANSICHAR*>(DeviceName)).Get());

			alcCloseDevice(HardwareDevice);
			HardwareDevice = nullptr;
		}
	}

}

/*------------------------------------------------------------------------------------
	UAudioDevice Interface.
------------------------------------------------------------------------------------*/
/**
 * Initializes the audio device and creates sources.
 *
 * @warning:
 *
 * @return TRUE if initialization was successful, FALSE otherwise
 */
bool FALAudioDevice::InitializeHardware( void )
{
	// Make sure no interface classes contain any garbage
	Effects = NULL;
	DLLHandle = NULL;

	// Default to sensible channel count.
	if( MaxChannels < 1 )
	{
		MaxChannels = 32;
	}
	// Load ogg and vorbis dlls if they haven't been loaded yet
	//LoadVorbisLibraries();

	// Open device
	HardwareDevice = alcOpenDevice(nullptr);
	if( !HardwareDevice )
	{
		UE_LOG(LogALAudio, Log, TEXT( "ALAudio: no OpenAL devices found." ) );
		return  false  ;
	}

	// Display the audio device that was actually opened
	const ALCchar* OpenedDeviceName = alcGetString( HardwareDevice, ALC_DEVICE_SPECIFIER );
	UE_LOG(LogALAudio, Log, TEXT("ALAudio device opened : %s"), StringCast<TCHAR>(static_cast<const ANSICHAR*>(OpenedDeviceName)).Get());

	// Create a context
	int Caps[] =
	{
		ALC_FREQUENCY, 44100,
		ALC_STEREO_SOURCES, 4,
		0, 0
	};
#if PLATFORM_LINUX
	SoundContext = alcCreateContext( HardwareDevice, Caps );
#elif PLATFORM_HTML5
	SoundContext = alcCreateContext( HardwareDevice, 0 );
#endif

	if( !SoundContext )
	{
		return false ;
	}

	alcMakeContextCurrent(SoundContext);

	// Make sure everything happened correctly
	if( alError( TEXT( "Init" ) ) )
	{
		UE_LOG(LogALAudio, Warning, TEXT("ALAudio: alcMakeContextCurrent failed."));
		return false ;
	}

	UE_LOG(LogALAudio, Log, TEXT("AL_VENDOR      : %s"), StringCast<TCHAR>(static_cast<const ANSICHAR*>(alGetString(AL_VENDOR))).Get());
	UE_LOG(LogALAudio, Log, TEXT("AL_RENDERER    : %s"), StringCast<TCHAR>(static_cast<const ANSICHAR*>(alGetString(AL_RENDERER))).Get());
	UE_LOG(LogALAudio, Log, TEXT("AL_VERSION     : %s"), StringCast<TCHAR>(static_cast<const ANSICHAR*>(alGetString(AL_VERSION))).Get());
	UE_LOG(LogALAudio, Log, TEXT("AL_EXTENSIONS  : %s"), StringCast<TCHAR>(static_cast<const ANSICHAR*>(alGetString(AL_EXTENSIONS))).Get());

	// Get the enums for multichannel support
#if !PLATFORM_HTML5
	Surround40Format = alGetEnumValue( "AL_FORMAT_QUAD16" );
	Surround51Format = alGetEnumValue( "AL_FORMAT_51CHN16" );
	Surround61Format = alGetEnumValue( "AL_FORMAT_61CHN16" );
	Surround71Format = alGetEnumValue( "AL_FORMAT_71CHN16" );
#endif
	// Initialize channels.
	alError( TEXT( "Emptying error stack" ), 0 );
	for( int i = 0; i < (( MaxChannels >  MAX_AUDIOCHANNELS ) ? MAX_AUDIOCHANNELS : MaxChannels); i++ )
	{
		ALuint SourceId;
		alGenSources( 1, &SourceId );
		if( !alError( TEXT( "Init (creating sources)" ), 0 ) )
		{
			FALSoundSource* Source = new FALSoundSource( this );
			Source->SourceId = SourceId;
			Sources.Add( Source );
			FreeSources.Add( Source );
		}
		else
		{
			break;
		}
	}

	if( Sources.Num() < 1 )
	{
		UE_LOG(LogALAudio, Warning, TEXT("ALAudio: couldn't allocate any sources"));
		return false ;
	}

	// Update MaxChannels in case we couldn't create enough sources.
	MaxChannels = Sources.Num();
	UE_LOG(LogALAudio, Verbose, TEXT("ALAudioDevice: Allocated %i sources"), MaxChannels);

	// Use our own distance model.
	alDistanceModel( AL_NONE );

	// Set up a default (nop) effects manager
	Effects = new FAudioEffectsManager( this );

	return true ;
}

/**
 * Update the audio device and calculates the cached inverse transform later
 * on used for spatialization.
 */
void FALAudioDevice::UpdateHardware()
{
	if (GetListeners().Num() > 0)
	{
		// Caches the matrix used to transform a sounds position into local space so we can just look
		// at the Y component after normalization to determine spatialization.
		const FListener& Listener = GetListeners()[0];
		const FVector Up = Listener.GetUp();
		const FVector Right = Listener.GetFront();
		InverseTransform = FMatrix(Up, Right, Up ^ Right, Listener.Transform.GetTranslation()).InverseFast();
	}
}

ALuint FALAudioDevice::GetInternalFormat( int NumChannels )
{
	ALuint InternalFormat = 0;

	switch( NumChannels )
	{
	case 0:
	case 3:
	case 5:
		break;
	case 1:
		InternalFormat = AL_FORMAT_MONO16;
		break;
	case 2:
		InternalFormat = AL_FORMAT_STEREO16;
		break;
#if !PLATFORM_HTML5
	case 4:
		InternalFormat = Surround40Format;
		break;
	case 6:
		InternalFormat = Surround51Format;
		break;
	case 7:
		InternalFormat = Surround61Format;
		break;
	case 8:
		InternalFormat = Surround71Format;
		break;
#endif
	}

	return( InternalFormat );
}

void FALAudioDevice::MakeCurrent(const TCHAR * CallSiteIdentifier)
{
#if PLATFORM_LINUX
	checkf(SoundContext, TEXT("Unitialiized sound context in FALAudioDevice::MakeCurrent()!"));
	if (!alcMakeContextCurrent(SoundContext))
	{
		alError(CallSiteIdentifier ? CallSiteIdentifier : TEXT("FALAudioDevice::MakeCurrent()"));
	}
#endif
}

/*------------------------------------------------------------------------------------
OpenAL utility functions
------------------------------------------------------------------------------------*/

//
//	alError
//
bool FALAudioDevice::alError( const TCHAR* Text, bool Log )
{
	ALenum Error = alGetError();
	if( Error != AL_NO_ERROR )
	{
		do
		{
			if( Log )
			{
				switch ( Error )
				{
					case AL_INVALID_NAME:
						UE_LOG(LogALAudio, Warning, TEXT("ALAudio: AL_INVALID_NAME in %s"), Text);
						break;
					case AL_INVALID_VALUE:
						UE_LOG(LogALAudio, Warning, TEXT("ALAudio: AL_INVALID_VALUE in %s"), Text);
						break;
					case AL_OUT_OF_MEMORY:
						UE_LOG(LogALAudio, Warning, TEXT("ALAudio: AL_OUT_OF_MEMORY in %s"), Text);
						break;
					case AL_INVALID_ENUM:
						UE_LOG(LogALAudio, Warning, TEXT("ALAudio: AL_INVALID_ENUM in %s"), Text);
						break;
					case AL_INVALID_OPERATION:
						UE_LOG(LogALAudio, Warning, TEXT("ALAudio: AL_INVALID_OPERATION in %s"), Text);
						break;
					default:
						UE_LOG(LogALAudio, Warning, TEXT("ALAudio: Unknown Error NUM in %s"), Text);
						break;
				}
			}
		}
		while( ( Error = alGetError() ) != AL_NO_ERROR );

		return true;
	}

	return false;
}

FSoundSource* FALAudioDevice::CreateSoundSource()
{
	return new FALSoundSource(this);
}

bool FALAudioDevice::HasCompressedAudioInfoClass(USoundWave* SoundWave)
{
#if WITH_OGGVORBIS
	return true;
#else
	return false;
#endif
}

class ICompressedAudioInfo* FALAudioDevice::CreateCompressedAudioInfo(USoundWave* SoundWave)
{
#if WITH_OGGVORBIS
	return new FVorbisAudioInfo();
#else
	return NULL;
#endif
}
