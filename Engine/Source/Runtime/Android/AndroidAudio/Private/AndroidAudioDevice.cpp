// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


/*------------------------------------------------------------------------------------
	Audio includes.
------------------------------------------------------------------------------------*/

#include "AndroidAudioDevice.h"
#include "VorbisAudioInfo.h"
#include "ADPCMAudioInfo.h"

DEFINE_LOG_CATEGORY(LogAndroidAudio);


class FSLESAudioDeviceModule : public IAudioDeviceModule
{
public:

	/** Creates a new instance of the audio device implemented by the module. */
	virtual FAudioDevice* CreateAudioDevice() override
	{
		return new FSLESAudioDevice;
	}
};

IMPLEMENT_MODULE(FSLESAudioDeviceModule, AndroidAudio );
/*------------------------------------------------------------------------------------
	UALAudioDevice constructor and UObject interface.
------------------------------------------------------------------------------------*/

void FSLESAudioDevice::Teardown( void )
{
	// Flush stops all sources and deletes all buffers so sources can be safely deleted below.
	Flush( NULL );	
	
	// Destroy all sound sources
	for( int32 i = 0; i < Sources.Num(); i++ )
	{
		delete Sources[ i ];
	}

	UE_LOG( LogAndroidAudio, Warning, TEXT("OpenSLES Tearing Down HW"));
	
	// Teardown OpenSLES..
	// Destroy the SLES objects in reverse order of creation:
	if (SL_OutputMixObject)
	{
		(*SL_OutputMixObject)->Destroy(SL_OutputMixObject);
		SL_OutputMixObject = NULL;
	}
	if (SL_EngineObject)
	{
		(*SL_EngineObject)->Destroy(SL_EngineObject);
		SL_EngineObject = NULL;
		SL_EngineEngine = NULL;
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
bool FSLESAudioDevice::InitializeHardware( void )
{
	UE_LOG( LogAndroidAudio, Warning, TEXT("SL Entered Init HW"));

	SLresult result;

	UE_LOG( LogAndroidAudio, Warning, TEXT("OpenSLES Initializing HW"));
	
	SLEngineOption EngineOption[] = { {(SLuint32) SL_ENGINEOPTION_THREADSAFE, (SLuint32) SL_BOOLEAN_TRUE} };
	
    // create engine
    result = slCreateEngine( &SL_EngineObject, 1, EngineOption, 0, NULL, NULL);
    //check(SL_RESULT_SUCCESS == result);
	if (SL_RESULT_SUCCESS != result)
	{
		UE_LOG( LogAndroidAudio, Error, TEXT("Engine create failed %d"), int32(result));
	}
	
    // realize the engine
    result = (*SL_EngineObject)->Realize(SL_EngineObject, SL_BOOLEAN_FALSE);
    check(SL_RESULT_SUCCESS == result);
	
    // get the engine interface, which is needed in order to create other objects
    result = (*SL_EngineObject)->GetInterface(SL_EngineObject, SL_IID_ENGINE, &SL_EngineEngine);
    check(SL_RESULT_SUCCESS == result);
	
	// create output mix, with environmental reverb specified as a non-required interface    
    result = (*SL_EngineEngine)->CreateOutputMix( SL_EngineEngine, &SL_OutputMixObject, 0, NULL, NULL );
    check(SL_RESULT_SUCCESS == result);
	
    // realize the output mix
    result = (*SL_OutputMixObject)->Realize(SL_OutputMixObject, SL_BOOLEAN_FALSE);
    check(SL_RESULT_SUCCESS == result);
	
	UE_LOG( LogAndroidAudio, Warning, TEXT("OpenSLES Initialized"));
    // ignore unsuccessful result codes for env

	
	// Default to sensible channel count.
	if( MaxChannels < 1 )
	{  
		MaxChannels = 12;
	}
	
	
	// Initialize channels.
	for( int32 i = 0; i < FMath::Min( MaxChannels, 12 ); i++ )
	{
		FSLESSoundSource* Source = new FSLESSoundSource( this );		
		Sources.Add( Source );
		FreeSources.Add( Source );
	}
	
	if( Sources.Num() < 1 )
	{
		UE_LOG( LogAndroidAudio,  Warning, TEXT( "OpenSLAudio: couldn't allocate any sources" ) );
		return false;
	}
	
	// Update MaxChannels in case we couldn't create enough sources.
	MaxChannels = Sources.Num();
	UE_LOG( LogAndroidAudio, Warning, TEXT( "OpenSLAudioDevice: Allocated %i sources" ), MaxChannels );
	
	// Set up a default (nop) effects manager 
	Effects = new FAudioEffectsManager( this );
	
	return true;
}

FSoundSource* FSLESAudioDevice::CreateSoundSource()
{
	return new FSLESSoundSource(this);
}

bool FSLESAudioDevice::HasCompressedAudioInfoClass(USoundWave* SoundWave)
{
#if WITH_OGGVORBIS
	if(SoundWave->bStreaming)
	{
		return true;
	}

	static FName NAME_OGG(TEXT("OGG"));
	if (SoundWave->HasCompressedData(NAME_OGG))
	{
		return true;
	}
#endif

	if(SoundWave->bStreaming)
	{
		return true;
	}

	static FName NAME_ADPCM(TEXT("ADPCM"));
	if (SoundWave->HasCompressedData(NAME_ADPCM))
	{
		return true;
	}

	return false;

}

class ICompressedAudioInfo* FSLESAudioDevice::CreateCompressedAudioInfo(USoundWave* SoundWave)
{
#if WITH_OGGVORBIS
	static FName NAME_OGG(TEXT("OGG"));
	if (SoundWave->bStreaming || SoundWave->HasCompressedData(NAME_OGG))
	{
		return new FVorbisAudioInfo();
	}
#endif
	static FName NAME_ADPCM(TEXT("ADPCM"));
	if (SoundWave->bStreaming || SoundWave->HasCompressedData(NAME_ADPCM))
	{
		return new FADPCMAudioInfo();
	}

	return nullptr;
}

/** Check if any background music or sound is playing through the audio device */
bool FSLESAudioDevice::IsExernalBackgroundSoundActive()
{
	extern bool AndroidThunkCpp_IsMusicActive();
	return AndroidThunkCpp_IsMusicActive();

}
