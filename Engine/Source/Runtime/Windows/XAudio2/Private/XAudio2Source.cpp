// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	XeAudioDevice.cpp: Unreal XAudio2 Audio interface object.

	Unreal is RHS with Y and Z swapped (or technically LHS with flipped axis)

=============================================================================*/

/*------------------------------------------------------------------------------------
	Audio includes.
------------------------------------------------------------------------------------*/

#include "XAudio2Device.h"
#include "XAudio2Effects.h"
#include "XAudio2Support.h"
#include "IAudioExtensionPlugin.h"
#include "ActiveSound.h"
#include "Sound/AudioSettings.h"
#include "ContentStreaming.h"
#include "HAL/LowLevelMemTracker.h"

/*------------------------------------------------------------------------------------
	For muting user soundtracks during cinematics
------------------------------------------------------------------------------------*/
FXMPHelper XMPHelper;
FXMPHelper* FXMPHelper::GetXMPHelper( void ) 
{ 
	return( &XMPHelper ); 
}

/*------------------------------------------------------------------------------------
	FXAudio2SoundSource.
------------------------------------------------------------------------------------*/

/**
 * Simple constructor
 */
FXAudio2SoundSource::FXAudio2SoundSource(FAudioDevice* InAudioDevice)
	: FSoundSource(InAudioDevice)
	, XAudio2Buffer(nullptr)
	, Source(nullptr)
	, MaxEffectChainChannels(0)
	, RealtimeAsyncTask(nullptr)
	, CurrentBuffer(0)
	, bLoopCallback(false)
	, bIsFinished(false)
	, bPlayedCachedBuffer(false)
	, bFirstRTBuffersSubmitted(false)
	, bBuffersToFlush(false)
	, bResourcesNeedFreeing(false)
	, bUsingHRTFSpatialization(false)
	, bEditorWarnedChangedSpatialization(false)
{

	AudioDevice = ( FXAudio2Device* )InAudioDevice;
	check( AudioDevice );
	Effects = (FXAudio2EffectsManager*)AudioDevice->GetEffects();
	check( Effects );

	Destinations[DEST_DRY].Flags = 0;
	Destinations[DEST_DRY].pOutputVoice = NULL;
	Destinations[DEST_REVERB].Flags = 0;
	Destinations[DEST_REVERB].pOutputVoice = NULL;
	Destinations[DEST_RADIO].Flags = 0;
	Destinations[DEST_RADIO].pOutputVoice = NULL;

	FMemory::Memzero( XAudio2Buffers, sizeof( XAudio2Buffers ) );
	FMemory::Memzero( XAudio2BufferXWMA, sizeof( XAudio2BufferXWMA ) );

	if (!InAudioDevice->bIsAudioDeviceHardwareInitialized)
	{
		bIsVirtual = true;
	}
}

/**
 * Destructor, cleaning up voice
 */
FXAudio2SoundSource::~FXAudio2SoundSource( void )
{
	FreeResources();
}

void FXAudio2SoundSource::InitializeSourceEffects(uint32 InVoiceId)
{
	VoiceId = InVoiceId;
	if (AudioDevice->SpatializationPluginInterface.IsValid())
	{
		AudioDevice->SpatializationPluginInterface->CreateSpatializationEffect(VoiceId);
	}
}

/**
 * Free up any allocated resources
 */
void FXAudio2SoundSource::FreeResources( void )
{
	// Release voice. Note that this will stop calling OnBufferEnd
	if (Source)
	{
		AudioDevice->DeviceProperties->ReleaseSourceVoice(Source, XAudio2Buffer->PCM, MaxEffectChainChannels); //-V595
		Source = nullptr;
	}

	if (XAudio2Buffer && XAudio2Buffer->RealtimeAsyncHeaderParseTask)
	{
		check(bResourcesNeedFreeing);

		XAudio2Buffer->RealtimeAsyncHeaderParseTask->EnsureCompletion();
		delete XAudio2Buffer->RealtimeAsyncHeaderParseTask;
		XAudio2Buffer->RealtimeAsyncHeaderParseTask = nullptr;
	}

	if (RealtimeAsyncTask)
	{
		RealtimeAsyncTask->EnsureCompletion();
		delete RealtimeAsyncTask;
		RealtimeAsyncTask = nullptr;
		check(bResourcesNeedFreeing);
	}

	if (bResourcesNeedFreeing && Buffer)
	{
		// If we failed to initialize, then we will have a non-zero resource ID, but still need to delete the buffer
		check(!bInitialized || Buffer->ResourceID == 0);
		delete Buffer;
	}

	// Make sure to nullify the buffer ptrs so that on re-use the source will have a clean buffer
	// Note that most cases will not require a delete since they are cached and owned by the audio device manager.
	Buffer = XAudio2Buffer = nullptr;
	CurrentBuffer = 0;
}

/** 
 * Submit the relevant audio buffers to the system
 */
void FXAudio2SoundSource::SubmitPCMBuffers( void )
{
	SCOPE_CYCLE_COUNTER( STAT_AudioSubmitBuffersTime );

	FMemory::Memzero( XAudio2Buffers, sizeof( XAUDIO2_BUFFER ) );

	CurrentBuffer = 0;

	XAudio2Buffers[0].pAudioData = XAudio2Buffer->PCM.PCMData;
	XAudio2Buffers[0].AudioBytes = XAudio2Buffer->PCM.PCMDataSize;
	XAudio2Buffers[0].pContext = this;

	if (!AudioDevice)
	{
		UE_LOG(LogXAudio2, Error, TEXT("SubmitPCMBuffers: Audio Device is nullptr"));
		return;
	}

	if (!Source)
	{
		UE_LOG(LogXAudio2, Error, TEXT("SubmitPCMBuffers: Source (IXAudio2SourceVoice is nullptr"));
		return;
	}

	if( WaveInstance->LoopingMode == LOOP_Never )
	{
		XAudio2Buffers[0].Flags = XAUDIO2_END_OF_STREAM;

		AudioDevice->ValidateAPICall( TEXT( "SubmitSourceBuffer - PCM - LOOP_Never" ), 
			Source->SubmitSourceBuffer( XAudio2Buffers ) );
	}
	else
	{
		XAudio2Buffers[0].LoopCount = XAUDIO2_LOOP_INFINITE;

		AudioDevice->ValidateAPICall( TEXT( "SubmitSourceBuffer - PCM - LOOP_*" ), 
			Source->SubmitSourceBuffer( XAudio2Buffers ) );
	}
}

bool FXAudio2SoundSource::ReadMorePCMData( const int32 BufferIndex, EDataReadMode DataReadMode )
{
	USoundWave* WaveData = WaveInstance->WaveData;
	if( WaveData && WaveData->bProcedural )
	{
		const int32 MaxSamples = ( MONO_PCM_BUFFER_SIZE * Buffer->NumChannels ) / sizeof( int16 );

		if (DataReadMode == EDataReadMode::Synchronous || WaveData->bCanProcessAsync == false)
		{
			const int32 BytesWritten = WaveData->GeneratePCMData( (uint8*)XAudio2Buffers[BufferIndex].pAudioData, MaxSamples );
			XAudio2Buffers[BufferIndex].AudioBytes = BytesWritten;
		}
		else
		{
			check(!RealtimeAsyncTask);
			RealtimeAsyncTask = new FAsyncRealtimeAudioTask(WaveData, (uint8*)XAudio2Buffers[BufferIndex].pAudioData, MaxSamples);
			RealtimeAsyncTask->StartBackgroundTask();
		}

		// we're never actually "looping" here.
		return false;
	}
	else
	{
		if (DataReadMode == EDataReadMode::Synchronous)
		{
			return XAudio2Buffer->ReadCompressedData( ( uint8* )XAudio2Buffers[BufferIndex].pAudioData, WaveInstance->LoopingMode != LOOP_Never );
		}
		else
		{
			check(!RealtimeAsyncTask);
			RealtimeAsyncTask = new FAsyncRealtimeAudioTask(XAudio2Buffer, (uint8*)XAudio2Buffers[BufferIndex].pAudioData, WaveInstance->LoopingMode != LOOP_Never, DataReadMode == EDataReadMode::AsynchronousSkipFirstFrame);
			RealtimeAsyncTask->StartBackgroundTask();
			return false;
		}
	}
}

uint8* FXAudio2SoundSource::GetRealtimeBufferData(const int32 InBufferIndex, const int32 InBufferSize)
{
	// Only supporting 3 realtime buffers
	check(InBufferIndex < 3);

	// Resize the array in case the new buffer size is bigger than previously allocated
	RealtimeBufferData[InBufferIndex].Reset();
	RealtimeBufferData[InBufferIndex].AddZeroed(InBufferSize);

	return RealtimeBufferData[InBufferIndex].GetData();
}

void FXAudio2SoundSource::SubmitPCMRTBuffers( void )
{
	SCOPE_CYCLE_COUNTER( STAT_AudioSubmitBuffersTime );

	FMemory::Memzero( XAudio2Buffers, sizeof( XAUDIO2_BUFFER ) * 3 );

	// Set the buffer to be in real time mode
	CurrentBuffer = 0;

	const uint32 BufferSize = MONO_PCM_BUFFER_SIZE * Buffer->NumChannels;

	// Set up buffer areas to decompress to
	for (int32 i = 0; i < 3; ++i)
	{
		XAudio2Buffers[i].pAudioData = GetRealtimeBufferData(i, BufferSize);
		XAudio2Buffers[i].AudioBytes = BufferSize;
		XAudio2Buffers[i].pContext = this;
	}

	// Only use the cached data if we're starting from the beginning, otherwise we'll have to take a synchronous hit
	bPlayedCachedBuffer = false;
	bool bIsSeeking = (WaveInstance->StartTime > 0.f);
	if (WaveInstance->WaveData && WaveInstance->WaveData->CachedRealtimeFirstBuffer && !bIsSeeking)
	{
		bPlayedCachedBuffer = true;
		FMemory::Memcpy((uint8*)XAudio2Buffers[0].pAudioData, WaveInstance->WaveData->CachedRealtimeFirstBuffer, BufferSize);
		FMemory::Memcpy((uint8*)XAudio2Buffers[1].pAudioData, WaveInstance->WaveData->CachedRealtimeFirstBuffer + BufferSize, BufferSize);
	}
	else
	{
		// Read the first two buffers and submit them
		ReadMorePCMData(0, EDataReadMode::Synchronous);
		ReadMorePCMData(1, EDataReadMode::Synchronous);
	}

	// Immediately submit the first two buffers that were either cached or synchronously read
	// The first buffer will start the voice processing buffers and trigger an OnBufferEnd callback, which will then 
	// trigger async tasks to generate more PCMRT buffers.
	AudioDevice->ValidateAPICall(TEXT("SubmitSourceBuffer - PCMRT"),
		Source->SubmitSourceBuffer(&XAudio2Buffers[0]));

	AudioDevice->ValidateAPICall(TEXT("SubmitSourceBuffer - PCMRT"),
		Source->SubmitSourceBuffer(&XAudio2Buffers[1]));

	// Prepare the third buffer for the OnBufferEnd callback to write to in the OnBufferEnd callback
	CurrentBuffer = 2;

	bResourcesNeedFreeing = true;
}

/** 
 * Submit the relevant audio buffers to the system, accounting for looping modes
 */
void FXAudio2SoundSource::SubmitXMA2Buffers( void )
{
#if XAUDIO_SUPPORTS_XMA2WAVEFORMATEX
	SCOPE_CYCLE_COUNTER( STAT_AudioSubmitBuffersTime );

	FMemory::Memzero( XAudio2Buffers, sizeof( XAUDIO2_BUFFER ) );

	CurrentBuffer = 0;

	XAudio2Buffers[0].pAudioData = XAudio2Buffer->XMA2.XMA2Data;
	XAudio2Buffers[0].AudioBytes = XAudio2Buffer->XMA2.XMA2DataSize;
	XAudio2Buffers[0].pContext = this;

	// Deal with looping behavior.
	if( WaveInstance->LoopingMode == LOOP_Never )
	{
		// Regular sound source, don't loop.
		XAudio2Buffers[0].Flags = XAUDIO2_END_OF_STREAM;

		AudioDevice->ValidateAPICall( TEXT( "SubmitSourceBuffer - XMA2 - LOOP_Never" ), 
			Source->SubmitSourceBuffer( XAudio2Buffers ) );
	}
	else
	{
		// Set to reserved "infinite" value.
		XAudio2Buffers[0].LoopCount = 255;
		XAudio2Buffers[0].LoopBegin = XAudio2Buffer->XMA2.XMA2Format.LoopBegin;
		XAudio2Buffers[0].LoopLength = XAudio2Buffer->XMA2.XMA2Format.LoopLength;

		AudioDevice->ValidateAPICall( TEXT( "SubmitSourceBuffer - XMA2 - LOOP_*" ), 
			Source->SubmitSourceBuffer( XAudio2Buffers ) );
	}
#else	//XAUDIO_SUPPORTS_XMA2WAVEFORMATEX
	checkf(0, TEXT("SubmitXMA2Buffers on a platform that does not support XMA2!"));
#endif	//XAUDIO_SUPPORTS_XMA2WAVEFORMATEX
}

/** 
 * Submit the relevant audio buffers to the system
 */
void FXAudio2SoundSource::SubmitXWMABuffers( void )
{
	SCOPE_CYCLE_COUNTER( STAT_AudioSubmitBuffersTime );

	FMemory::Memzero( XAudio2Buffers, sizeof( XAUDIO2_BUFFER ) );
	FMemory::Memzero( XAudio2BufferXWMA, sizeof( XAUDIO2_BUFFER_WMA ) );

	CurrentBuffer = 0;

	// Regular sound source, don't loop.
	XAudio2Buffers[0].pAudioData = XAudio2Buffer->XWMA.XWMAData;
	XAudio2Buffers[0].AudioBytes = XAudio2Buffer->XWMA.XWMADataSize;
	XAudio2Buffers[0].pContext = this;

	XAudio2BufferXWMA[0].pDecodedPacketCumulativeBytes = XAudio2Buffer->XWMA.XWMASeekData;
	XAudio2BufferXWMA[0].PacketCount = XAudio2Buffer->XWMA.XWMASeekDataSize / sizeof( UINT32 );

	if( WaveInstance->LoopingMode == LOOP_Never )
	{
		XAudio2Buffers[0].Flags = XAUDIO2_END_OF_STREAM;

		AudioDevice->ValidateAPICall( TEXT( "SubmitSourceBuffer - XWMA - LOOP_Never" ), 
			Source->SubmitSourceBuffer( XAudio2Buffers, XAudio2BufferXWMA ) );
	}
	else
	{
		XAudio2Buffers[0].LoopCount = 255;
		XAudio2Buffers[0].Flags = XAUDIO2_END_OF_STREAM;

		AudioDevice->ValidateAPICall( TEXT( "SubmitSourceBuffer - XWMA - LOOP_*" ), 
			Source->SubmitSourceBuffer( XAudio2Buffers, XAudio2BufferXWMA ) );
	}
}

/**
 * Create a new source voice
 */
bool FXAudio2SoundSource::CreateSource( void )
{
	SCOPE_CYCLE_COUNTER( STAT_AudioSourceCreateTime );

	// No need to create a hardware voice if we're virtual
	if (bIsVirtual)
	{
		return true;
	}

	uint32 NumSends = 0;

	// Create a source that goes to the spatialisation code and reverb effect
	Destinations[NumSends].pOutputVoice = Effects->DryPremasterVoice;

	// EQFilter Causes some sound devices to lag and starve important game threads. Hack disable until a long term solution is put into place.

	const bool bIsEQDisabled = GetDefault<UAudioSettings>()->bDisableMasterEQ;
	if (!bIsEQDisabled && IsEQFilterApplied())
	{
		Destinations[NumSends].pOutputVoice = Effects->EQPremasterVoice;
	}

	NumSends++;

	const bool bIsReverbDisabled = GetDefault<UAudioSettings>()->bDisableMasterReverb;
	if( bReverbApplied && !bIsReverbDisabled)
	{
		Destinations[NumSends].pOutputVoice = Effects->ReverbEffectVoice;
		NumSends++;
	}

	if( WaveInstance->bApplyRadioFilter )
	{
		Destinations[NumSends].pOutputVoice = Effects->RadioEffectVoice;
		NumSends++;
	}

	const XAUDIO2_VOICE_SENDS SourceSendList =
	{
		NumSends, Destinations
	};

	// Mark the source as music if it is a member of the music group and allow low, band and high pass filters

	// Reset the bUsingSpatializationEffect flag
	bUsingHRTFSpatialization = false;
	bool bCreatedWithSpatializationEffect = false;
	MaxEffectChainChannels = 0;
	
	// Set to nullptr in case the voice is not successfully created, the source won't be garbage
	Source = nullptr;

	if (CreateWithSpatializationEffect())
	{
		check(AudioDevice->SpatializationPluginInterface.IsValid());
		IUnknown* Effect = (IUnknown*)AudioDevice->SpatializationPluginInterface->GetSpatializationEffect(VoiceId);
		if (Effect)
		{
			// Indicate that this source is currently using the 3d spatialization effect. We can't stop using it
			// for the lifetime of this sound, so if this if the spatialization effect is toggled off, we're still 
			// going to hear the sound for the duration of this sound.
			bUsingHRTFSpatialization = true;

			MaxEffectChainChannels = 2;

			XAUDIO2_EFFECT_DESCRIPTOR EffectDescriptor[1];
			EffectDescriptor[0].pEffect = Effect;
			EffectDescriptor[0].OutputChannels = MaxEffectChainChannels;
			EffectDescriptor[0].InitialState = true;

			const XAUDIO2_EFFECT_CHAIN EffectChain = { 1, EffectDescriptor };
			AudioDevice->DeviceProperties->GetFreeSourceVoice(&Source, XAudio2Buffer->PCM, &EffectChain, &SourceSendList, MaxEffectChainChannels);

			if (!Source)
			{
				return false;
			}

			// We succeeded, then return
			bCreatedWithSpatializationEffect = true;
		}
	}

	if (!bCreatedWithSpatializationEffect)
	{
		check(AudioDevice->DeviceProperties != nullptr);
		check(AudioDevice->DeviceProperties->XAudio2 != nullptr);

		AudioDevice->DeviceProperties->GetFreeSourceVoice(&Source, XAudio2Buffer->PCM, nullptr, &SourceSendList);

		if (!Source)
		{
			return false;
		}
	}

	return true;
}

bool FXAudio2SoundSource::PrepareForInitialization(FWaveInstance* InWaveInstance)
{
	// If the headphones have been unplugged, set this voice to be virtual
	if (!AudioDevice->DeviceProperties->bAllowNewVoices)
	{
		bIsVirtual = true;
	}

	// We need to set the wave instance regardless of what happens below so that the wave instance can be stopped if this sound source fails to init
	WaveInstance = InWaveInstance;

	// If virtual only need wave instance data and no need to load source data
	if (bIsVirtual)
	{
		bIsFinished = false;
		return true;
	}

	// Reset so next instance will warn if algorithm changes inflight
	bEditorWarnedChangedSpatialization = false;

	// We are not supporting playing audio on a controller
	if (InWaveInstance->OutputTarget == EAudioOutputTarget::Controller)
	{
		return false;
	}

	// Flag that we are not initialized yet
	bInitialized = false;

	// Reset so next instance will warn if algorithm changes inflight
	bEditorWarnedChangedSpatialization = false;

	// Find matching buffer.
	check(InWaveInstance->ActiveSound->AudioDevice);

	check(XAudio2Buffer == nullptr);
	XAudio2Buffer = FXAudio2SoundBuffer::Init(InWaveInstance->ActiveSound->AudioDevice, InWaveInstance->WaveData, InWaveInstance->StartTime > 0.f);
	if (XAudio2Buffer)
	{
		// If our realtime source is not ready, then we will need to free our resources because this 
		// buffer is an async decoded buffer and could be stopped before the header is finished being parsed
		if (!XAudio2Buffer->IsRealTimeSourceReady())
		{
			bResourcesNeedFreeing = true;
		}

		Buffer = XAudio2Buffer;

		// Reset the LPFFrequency values
		LPFFrequency = MAX_FILTER_FREQUENCY;
		LastLPFFrequency = FLT_MAX;

		bIsFinished = false;

		// We succeeded in preparing our xaudio2 buffer for initialization. We are technically not initialized yet.
		// If the buffer is asynchronously preparing the ogg-vorbis file handle, we may not yet initialize the source
		return true;
	}

	// Something went wrong with creating the XAudio2SoundBuffer
	return false;
}

bool FXAudio2SoundSource::IsPreparedToInit()
{
	return bIsVirtual || (XAudio2Buffer && XAudio2Buffer->IsRealTimeSourceReady());
}

bool FXAudio2SoundSource::Init(FWaveInstance* InWaveInstance)
{
	FSoundSource::InitCommon();

	if (bIsVirtual)
	{
		bInitialized = true;
		return true;
	}

	check(XAudio2Buffer);
	check(XAudio2Buffer->IsRealTimeSourceReady());
	check(Buffer);

	// Buffer failed to be created, or there was an error with the compressed data
	if (Buffer->NumChannels > 0)
	{
		SCOPE_CYCLE_COUNTER(STAT_AudioSourceInitTime);

		// Set whether to apply reverb
		SetReverbApplied(Effects->ReverbEffectVoice != nullptr);

		// Create a new source if we haven't already
		if (CreateSource())
		{
			check(WaveInstance);

			if (WaveInstance->StartTime > 0.0f)
			{
				if (WaveInstance->WaveData->bStreaming)
				{
					UE_LOG(LogXAudio2, Verbose, TEXT("Seeking (aka start time) is not supported for streaming sound waves ('%s')."), *InWaveInstance->GetName());
				}
				else
				{
					XAudio2Buffer->Seek(WaveInstance->StartTime);
				}
			}

			// Submit audio buffers
			switch (XAudio2Buffer->SoundFormat)
			{
			case SoundFormat_PCM:
			case SoundFormat_PCMPreview:
				SubmitPCMBuffers();
				break;

			case SoundFormat_PCMRT:
			case SoundFormat_Streaming:
				SubmitPCMRTBuffers();
				break;

			case SoundFormat_XMA2:
				SubmitXMA2Buffers();
				break;

			case SoundFormat_XWMA:
				SubmitXWMABuffers();
				break;
			}

			// First updates of the source which e.g. sets the pitch and volume.
			bInitialized = true;
			bFirstRTBuffersSubmitted = false;

			Update();

			// Initialize the total  number of frames of audio for this sound source
			int32 NumBytes = InWaveInstance->WaveData->RawPCMDataSize;
			NumTotalFrames = NumBytes / (Buffer->NumChannels * sizeof(int16));
			StartFrame = 0;

			if (InWaveInstance->StartTime > 0.0f)
			{
				const float StartFraction = InWaveInstance->StartTime / InWaveInstance->WaveData->GetDuration();
				StartFrame = StartFraction * NumTotalFrames;
			}

			// Now set the source state to initialized so it can be played
			// Initialization succeeded.
			return true;
		}
		else if (AudioDevice->DeviceProperties->bAllowNewVoices)
		{
			UE_LOG(LogXAudio2, Warning, TEXT("Failed to init sound source for wave instance '%s' due to being unable to create an IXAudio2SourceVoice."), *InWaveInstance->GetName());
		}
		else
		{
			// The audio device was unplugged after init was declared, we're actually virtual now
			bIsVirtual = true;

			// Set that we've actually successfully initialized
			bInitialized = true;
			return true;
		}
	}
	else
	{
		UE_LOG(LogXAudio2, Warning, TEXT("Failed to init sound source for wave instance '%s' due to invalid buffer or error in compression."), *InWaveInstance->GetName());
	}

	// Initialization failed.
	FreeResources();
	return false;
}

/**
 * Calculates the volume for each channel
 */
void FXAudio2SoundSource::GetChannelVolumes(float ChannelVolumes[CHANNEL_MATRIX_COUNT], float AttenuatedVolume)
{
	if (bIsVirtual)
	{
		return;
	}

	if (AudioDevice->IsAudioDeviceMuted())
	{
		for( int32 i = 0; i < CHANNELOUT_COUNT; i++ )
		{
			ChannelVolumes[i] = 0;
		}
		return;
	}

	switch (Buffer->NumChannels)
	{
		case 1:
			GetMonoChannelVolumes(ChannelVolumes, AttenuatedVolume);
			break;

		case 2:
			GetStereoChannelVolumes(ChannelVolumes, AttenuatedVolume);
			break;

		case 4:
			GetQuadChannelVolumes(ChannelVolumes, AttenuatedVolume);
			break;

		case 6:
			GetHexChannelVolumes(ChannelVolumes, AttenuatedVolume);
			break;

		default:
			break;
	};

	// Apply any debug settings
	switch (AudioDevice->GetMixDebugState())
	{
	case DEBUGSTATE_IsolateReverb:
		for( int32 i = 0; i < SPEAKER_COUNT; i++ )
		{
			ChannelVolumes[i] = 0.0f;
		}
		break;

	case DEBUGSTATE_IsolateDryAudio:
		ChannelVolumes[CHANNELOUT_REVERB] = 0.0f;
		ChannelVolumes[CHANNELOUT_RADIO] = 0.0f;
		break;
	};

	for (int32 i = 0; i < CHANNEL_MATRIX_COUNT; i++)
	{
		// Detect and warn about NaN and INF volumes. XAudio does not do this internally and behavior is undefined.
		// This is known to happen in X3DAudioCalculate in channel 0 and the cause is unknown.
		if (!FMath::IsFinite(ChannelVolumes[i]))
		{
			const FString NaNorINF = FMath::IsNaN(ChannelVolumes[i]) ? TEXT("NaN") : TEXT("INF");
			UE_LOG(LogXAudio2, Warning, TEXT("FXAudio2SoundSource contains %s in channel %d: %s"), *NaNorINF, i, *Describe_Internal(true, false));
			ChannelVolumes[i] = 0;
		}
		// Detect and warn about unreasonable volumes. These are clamped anyway, but are good to know about.
		else if (ChannelVolumes[i] > FLT_MAX / 2.f || ChannelVolumes[i] < -FLT_MAX / 2.f)
		{
			UE_LOG(LogXAudio2, Warning, TEXT("FXAudio2SoundSource contains unreasonble value %f in channel %d: %s"), ChannelVolumes[i], i, *Describe_Internal(true, false));
		}

		ChannelVolumes[i] = FMath::Clamp<float>(ChannelVolumes[i] * AudioDevice->GetPlatformAudioHeadroom(), 0.0f, MAX_VOLUME);
	}
}

FVector FXAudio2SoundSource::ConvertToXAudio2Orientation(const FVector& InputVector)
{
	return FVector(InputVector.Y, InputVector.X, -InputVector.Z);

}

void FXAudio2SoundSource::GetMonoChannelVolumes(float ChannelVolumes[CHANNEL_MATRIX_COUNT], float AttenuatedVolume)
{
	FSpatializationParams SpatializationParams = GetSpatializationParams();

	if (IsUsingHrtfSpatializer())
	{
		// If we are using a HRTF spatializer, we are going to be using an XAPO effect that takes a mono stream and splits it into stereo
		// So in th at case we will just set the emitter position as a parameter to the XAPO plugin and then treat the
		// sound as if it was a non-spatialized stereo asset
		if (WaveInstance->SpatializationMethod == ESoundSpatializationAlgorithm::SPATIALIZATION_HRTF && !bEditorWarnedChangedSpatialization)
		{
			bEditorWarnedChangedSpatialization = true;
			UE_LOG(LogXAudio2, Warning, TEXT("Changing the spatialization algorithm on a playing sound is not supported (WaveInstance: %s)"), *WaveInstance->WaveData->GetFullName());
		}
		check(AudioDevice->SpatializationPluginInterface.IsValid());

		AudioDevice->SpatializationPluginInterface->SetSpatializationParameters(VoiceId, SpatializationParams);
		GetStereoChannelVolumes(ChannelVolumes, AttenuatedVolume);
	}
	else // Spatialize the mono stream using the normal 3d audio algorithm
	{
		// Convert to xaudio2 coordinates
		SpatializationParams.EmitterPosition = ConvertToXAudio2Orientation(SpatializationParams.EmitterPosition);

		// Calculate 5.1 channel dolby surround rate/multipliers.
		ChannelVolumes[CHANNELOUT_FRONTLEFT] = AttenuatedVolume;
		ChannelVolumes[CHANNELOUT_FRONTRIGHT] = AttenuatedVolume;
		ChannelVolumes[CHANNELOUT_FRONTCENTER] = AttenuatedVolume;
		ChannelVolumes[CHANNELOUT_LEFTSURROUND] = AttenuatedVolume;
		ChannelVolumes[CHANNELOUT_RIGHTSURROUND] = AttenuatedVolume;

		if (bReverbApplied)
		{
			ChannelVolumes[CHANNELOUT_REVERB] = AttenuatedVolume;
		}

		ChannelVolumes[CHANNELOUT_RADIO] = 0.0f;

		AudioDevice->DeviceProperties->SpatializationHelper.CalculateDolbySurroundRate(SpatializationParams.ListenerOrientation, SpatializationParams.ListenerPosition, SpatializationParams.EmitterPosition, SpatializationParams.NormalizedOmniRadius, ChannelVolumes);


		// Handle any special post volume processing
		if (WaveInstance->bApplyRadioFilter)
		{
			// If radio filter applied, output on radio channel only (no reverb)
			FMemory::Memzero(ChannelVolumes, CHANNELOUT_COUNT * sizeof(float));

			ChannelVolumes[CHANNELOUT_RADIO] = WaveInstance->RadioFilterVolume;
		}
		else if (WaveInstance->bCenterChannelOnly)
		{
			// If center channel only applied, output on center channel only (no reverb)
			FMemory::Memzero(ChannelVolumes, CHANNELOUT_COUNT * sizeof(float));

			ChannelVolumes[CHANNELOUT_FRONTCENTER] = WaveInstance->VoiceCenterChannelVolume * AttenuatedVolume;
		}
		else
		{
			if (FXAudioDeviceProperties::NumSpeakers == 6)
			{
				ChannelVolumes[CHANNELOUT_LOWFREQUENCY] = AttenuatedVolume * LFEBleed;

				// Smooth out the left and right channels with the center channel
				if (ChannelVolumes[CHANNELOUT_FRONTCENTER] > ChannelVolumes[CHANNELOUT_FRONTLEFT])
				{
					ChannelVolumes[CHANNELOUT_FRONTLEFT] = (ChannelVolumes[CHANNELOUT_FRONTCENTER] + ChannelVolumes[CHANNELOUT_FRONTLEFT]) / 2;
				}

				if (ChannelVolumes[CHANNELOUT_FRONTCENTER] > ChannelVolumes[CHANNELOUT_FRONTRIGHT])
				{
					ChannelVolumes[CHANNELOUT_FRONTRIGHT] = (ChannelVolumes[CHANNELOUT_FRONTCENTER] + ChannelVolumes[CHANNELOUT_FRONTRIGHT]) / 2;
				}
			}

			// Weight some of the sound to the center channel
			ChannelVolumes[CHANNELOUT_FRONTCENTER] = FMath::Max(ChannelVolumes[CHANNELOUT_FRONTCENTER], WaveInstance->VoiceCenterChannelVolume * AttenuatedVolume);
		}
	}
}

void FXAudio2SoundSource::GetStereoChannelVolumes(float ChannelVolumes[CHANNEL_MATRIX_COUNT], float AttenuatedVolume)
{
	// If we're doing 3d spatializaton of stereo sounds
	if (!IsUsingHrtfSpatializer() && WaveInstance->bUseSpatialization)
	{
		check(MAX_INPUT_CHANNELS_SPATIALIZED >= 2);

		// Loop through the left and right input channels and set the attenuation volumes
		for (int32 i = 0; i < 2; ++i)
		{
			// Offset is the offset into the channel matrix
			int32 Offset = CHANNELOUT_COUNT*i;
			ChannelVolumes[CHANNELOUT_FRONTLEFT + Offset] = AttenuatedVolume;
			ChannelVolumes[CHANNELOUT_FRONTRIGHT + Offset] = AttenuatedVolume;
			ChannelVolumes[CHANNELOUT_FRONTCENTER + Offset] = AttenuatedVolume;
			ChannelVolumes[CHANNELOUT_LEFTSURROUND + Offset] = AttenuatedVolume;
			ChannelVolumes[CHANNELOUT_RIGHTSURROUND + Offset] = AttenuatedVolume;

			if (bReverbApplied)
			{
				ChannelVolumes[CHANNELOUT_REVERB + Offset] = AttenuatedVolume;
			}

			ChannelVolumes[CHANNELOUT_RADIO + Offset] = 0.0f;

			// Add some LFE bleed 
			if (FXAudioDeviceProperties::NumSpeakers == 6)
			{
				ChannelVolumes[CHANNELOUT_LOWFREQUENCY + Offset] = AttenuatedVolume * LFEBleed;
			}
		}

		// Make sure we have up-to-date left and right channel positions for stereo spatialization
		UpdateStereoEmitterPositions();

		// Now get the spatialization params transformed into listener-space
		FSpatializationParams SpatializationParams = GetSpatializationParams();
		
		// Convert to Xaudio2 coordinates
		SpatializationParams.LeftChannelPosition = ConvertToXAudio2Orientation(SpatializationParams.LeftChannelPosition);
		SpatializationParams.RightChannelPosition = ConvertToXAudio2Orientation(SpatializationParams.RightChannelPosition);

		// Compute the speaker mappings for the left channel
		float* ChannelMap = ChannelVolumes;
		AudioDevice->DeviceProperties->SpatializationHelper.CalculateDolbySurroundRate(SpatializationParams.ListenerOrientation, SpatializationParams.ListenerPosition, SpatializationParams.LeftChannelPosition, SpatializationParams.NormalizedOmniRadius, ChannelMap);
		
		// Now compute the speaker mappings for the right channel
		ChannelMap = &ChannelVolumes[CHANNELOUT_COUNT];
		AudioDevice->DeviceProperties->SpatializationHelper.CalculateDolbySurroundRate(SpatializationParams.ListenerOrientation, SpatializationParams.ListenerPosition, SpatializationParams.RightChannelPosition, SpatializationParams.NormalizedOmniRadius, ChannelMap);
	}
	else
	{
		// Stereo is always treated as unspatialized (except when the HRTF spatialization effect is being used)
		ChannelVolumes[CHANNELOUT_FRONTLEFT] = AttenuatedVolume;
		ChannelVolumes[CHANNELOUT_FRONTRIGHT] = AttenuatedVolume;

		// Potentially bleed to the rear speakers from 2.0 channel to simulated 4.0 channel
		// but only if this is not an HRTF-spatialized mono sound
		if (!IsUsingHrtfSpatializer() && FXAudioDeviceProperties::NumSpeakers == 6)
		{
			ChannelVolumes[CHANNELOUT_LEFTSURROUND] = AttenuatedVolume * StereoBleed;
			ChannelVolumes[CHANNELOUT_RIGHTSURROUND] = AttenuatedVolume * StereoBleed;

			ChannelVolumes[CHANNELOUT_LOWFREQUENCY] = AttenuatedVolume * LFEBleed * 0.5f;
		}

		if (bReverbApplied)
		{
			ChannelVolumes[CHANNELOUT_REVERB] = AttenuatedVolume;
		}

		// Handle radio distortion if the sound can handle it. 
		ChannelVolumes[CHANNELOUT_RADIO] = 0.0f;
		if (WaveInstance->bApplyRadioFilter)
		{
			ChannelVolumes[CHANNELOUT_RADIO] = AttenuatedVolume * WaveInstance->RadioFilterVolume;
		}
	}
}

void FXAudio2SoundSource::GetQuadChannelVolumes(float ChannelVolumes[CHANNEL_MATRIX_COUNT], float AttenuatedVolume)
{
	ChannelVolumes[CHANNELOUT_FRONTLEFT] = AttenuatedVolume;
	ChannelVolumes[CHANNELOUT_FRONTRIGHT] = AttenuatedVolume;
	ChannelVolumes[CHANNELOUT_LEFTSURROUND] = AttenuatedVolume;
	ChannelVolumes[CHANNELOUT_RIGHTSURROUND] = AttenuatedVolume;

	if (FXAudioDeviceProperties::NumSpeakers == 6)
	{
		ChannelVolumes[CHANNELOUT_LOWFREQUENCY] = AttenuatedVolume * LFEBleed * 0.25f;
	}
}

void FXAudio2SoundSource::GetHexChannelVolumes(float ChannelVolumes[CHANNEL_MATRIX_COUNT], float AttenuatedVolume)
{
	ChannelVolumes[CHANNELOUT_FRONTLEFT] = AttenuatedVolume;
	ChannelVolumes[CHANNELOUT_FRONTRIGHT] = AttenuatedVolume;
	ChannelVolumes[CHANNELOUT_FRONTCENTER] = AttenuatedVolume;
	ChannelVolumes[CHANNELOUT_LOWFREQUENCY] = AttenuatedVolume;
	ChannelVolumes[CHANNELOUT_LEFTSURROUND] = AttenuatedVolume;
	ChannelVolumes[CHANNELOUT_RIGHTSURROUND] = AttenuatedVolume;
}

/** 
 * Maps a sound with a given number of channels to to expected speakers
 */
void FXAudio2SoundSource::RouteDryToSpeakers(float ChannelVolumes[CHANNEL_MATRIX_COUNT], const float InVolume)
{
	// Only need to account to the special cases that are not a simple match of channel to speaker
	switch( Buffer->NumChannels )
	{
		case 1:
		RouteMonoToDry(ChannelVolumes);
		break;

		case 2:
		RouteStereoToDry(ChannelVolumes);
		break;

		case 4:
		RouteQuadToDry(ChannelVolumes);
		break;

		case 6:
		RouteHexToDry(ChannelVolumes);
		break;

		default:
			// For all other channel counts, just apply the volume and let XAudio2 handle doing downmixing of the source
			Source->SetVolume(InVolume);
			break;
	};
}

void FXAudio2SoundSource::RouteMonoToDry(float ChannelVolumes[CHANNEL_MATRIX_COUNT])
{
	if (IsUsingHrtfSpatializer())
	{
		// If we're spatializing using HRTF algorithms, then our output is actually stereo
		RouteStereoToDry(ChannelVolumes);
	}
	else
	{
		// Spatialised audio maps 1 channel to 6 speakers
		float SpatialisationMatrix[SPEAKER_COUNT * 1] =
		{
			ChannelVolumes[CHANNELOUT_FRONTLEFT],
			ChannelVolumes[CHANNELOUT_FRONTRIGHT],
			ChannelVolumes[CHANNELOUT_FRONTCENTER],
			ChannelVolumes[CHANNELOUT_LOWFREQUENCY],
			ChannelVolumes[CHANNELOUT_LEFTSURROUND],
			ChannelVolumes[CHANNELOUT_RIGHTSURROUND]
		};

		// Update the dry output to the mastering voice
		AudioDevice->ValidateAPICall(TEXT("SetOutputMatrix (mono)"), Source->SetOutputMatrix(Destinations[DEST_DRY].pOutputVoice, 1, SPEAKER_COUNT, SpatialisationMatrix));
	}
}

void FXAudio2SoundSource::RouteStereoToDry(float Chans[CHANNEL_MATRIX_COUNT])
{
	if (IsUsingHrtfSpatializer())
	{
		// A 2d sound
		float SpatialisationMatrix[SPEAKER_COUNT * 2] =
		{
			// Left Input					Right Input
			Chans[CHANNELOUT_FRONTLEFT],	0.0f,							// Left
			0.0f,							Chans[CHANNELOUT_FRONTRIGHT],	// Right
			0.0f,							0.0f,							// Center
			0.0f,							0.0f,							// LFE
			0.0f,							0.0f,							// Left Surround
			0.0f,							0.0f							// Right Surround
		};
		// Stereo sounds map 2 channels to 6 speakers
		AudioDevice->ValidateAPICall(TEXT("SetOutputMatrix (stereo)"), Source->SetOutputMatrix(Destinations[DEST_DRY].pOutputVoice, 2, SPEAKER_COUNT, SpatialisationMatrix));		// Build a non-3d "multi-channel" blend from the stereo channels
	}
	else if (WaveInstance->bUseSpatialization)
	{
		// Build a non-3d "multi-channel" blend from the stereo channels
		float SpatialisationMatrix[SPEAKER_COUNT * 2] =
		{
			// Left Input						Right Input
			Chans[CHANNELOUT_FRONTLEFT],		Chans[CHANNELOUT_COUNT + CHANNELOUT_FRONTLEFT],		// Left
			Chans[CHANNELOUT_FRONTRIGHT],		Chans[CHANNELOUT_COUNT + CHANNELOUT_FRONTRIGHT],	// Right
			Chans[CHANNELOUT_FRONTCENTER],		Chans[CHANNELOUT_COUNT + CHANNELOUT_FRONTCENTER],	// Right
			Chans[CHANNELOUT_LOWFREQUENCY],		Chans[CHANNELOUT_COUNT + CHANNELOUT_LOWFREQUENCY],	// LFE
			Chans[CHANNELOUT_LEFTSURROUND],		Chans[CHANNELOUT_COUNT + CHANNELOUT_LEFTSURROUND],	// Left Surround
			Chans[CHANNELOUT_RIGHTSURROUND],	Chans[CHANNELOUT_COUNT + CHANNELOUT_RIGHTSURROUND],	// Right Surround
		};

		AudioDevice->ValidateAPICall(TEXT("SetOutputMatrix (stereo)"), Source->SetOutputMatrix(Destinations[DEST_DRY].pOutputVoice, 2, SPEAKER_COUNT, SpatialisationMatrix));
	}
	else
	{
		float SpatialisationMatrix[SPEAKER_COUNT * 2] =
		{
			// Left Input					Right Input
			Chans[CHANNELOUT_FRONTLEFT],	0.0f,								// Left
			0.0f,							Chans[CHANNELOUT_FRONTRIGHT],		// Right
			0.0f,							0.0f,								// Center
			Chans[CHANNELOUT_LOWFREQUENCY], Chans[CHANNELOUT_LOWFREQUENCY],		// LFE
			Chans[CHANNELOUT_LEFTSURROUND], 0.0f,								// Left Surround
			0.0f,							Chans[CHANNELOUT_RIGHTSURROUND]		// Right Surround
		};

		// Stereo sounds map 2 channels to 6 speakers
		AudioDevice->ValidateAPICall(TEXT("SetOutputMatrix (stereo)"), Source->SetOutputMatrix(Destinations[DEST_DRY].pOutputVoice, 2, SPEAKER_COUNT, SpatialisationMatrix));
	}
}

void FXAudio2SoundSource::RouteQuadToDry(float Chans[CHANNEL_MATRIX_COUNT])
{
	float SpatialisationMatrix[SPEAKER_COUNT * 4] =
	{
		// Left Input						Right Input						Left Surround Input				Right Surround Input
		Chans[CHANNELOUT_FRONTLEFT],		0.0f,							0.0f,							0.0f,								// Left
		0.0f,								Chans[CHANNELOUT_FRONTRIGHT],	0.0f,							0.0f,								// Right
		0.0f,								0.0f,							0.0f,							0.0f,								// Center
		Chans[CHANNELOUT_LOWFREQUENCY],		Chans[CHANNELOUT_LOWFREQUENCY],	Chans[CHANNELOUT_LOWFREQUENCY],	Chans[CHANNELOUT_LOWFREQUENCY],		// LFE
		0.0f,								0.0f,							Chans[CHANNELOUT_LEFTSURROUND],	0.0f,								// Left Surround
		0.0f,								0.0f,							0.0f,							Chans[CHANNELOUT_RIGHTSURROUND]		// Right Surround
	};

	// Quad sounds map 4 channels to 6 speakers
	AudioDevice->ValidateAPICall(TEXT("SetOutputMatrix (4 channel)"),
		Source->SetOutputMatrix(Destinations[DEST_DRY].pOutputVoice, 4, SPEAKER_COUNT, SpatialisationMatrix));
}

void FXAudio2SoundSource::RouteHexToDry(float Chans[CHANNEL_MATRIX_COUNT])
{
	if ((XAudio2Buffer->DecompressionState && XAudio2Buffer->DecompressionState->UsesVorbisChannelOrdering())
		|| WaveInstance->WaveData->bDecompressedFromOgg)
	{
		// Ordering of channels is different for 6 channel OGG
		float SpatialisationMatrix[SPEAKER_COUNT * 6] =
		{
			// Left In						Center In						Right In						Left Surround In				Right Surround In					LFE In
			Chans[CHANNELOUT_FRONTLEFT],	0.0f,							0.0f,							0.0f,							0.0f,								0.0f,							// Left Out
			0.0f,							0.0f,							Chans[CHANNELOUT_FRONTRIGHT],	0.0f,							0.0f,								0.0f,							// Right Out
			0.0f,							Chans[CHANNELOUT_FRONTCENTER],	0.0f,							0.0f,							0.0f,								0.0f,							// Center Out
			0.0f,							0.0f,							0.0f,							0.0f,							0.0f,								Chans[CHANNELOUT_LOWFREQUENCY],	// LFE Out
			0.0f,							0.0f,							0.0f,							Chans[CHANNELOUT_LEFTSURROUND], 0.0f,								0.0f,							// Left Surround Out
			0.0f,							0.0f,							0.0f,							0.0f,							Chans[CHANNELOUT_RIGHTSURROUND],	0.0f							// Right Surround Out
		};

		// 5.1 sounds map 6 channels to 6 speakers
		AudioDevice->ValidateAPICall(TEXT("SetOutputMatrix (6 channel OGG)"),
			Source->SetOutputMatrix(Destinations[DEST_DRY].pOutputVoice, 6, SPEAKER_COUNT, SpatialisationMatrix));
	}
	else
	{
		float SpatialisationMatrix[SPEAKER_COUNT * 6] =
		{
			// Left In						Center In						Right In						Left Surround In				Right Surround In					LFE In
			Chans[CHANNELOUT_FRONTLEFT],	0.0f,							0.0f,							0.0f,							0.0f,								0.0f,								// Left Out
			0.0f,							Chans[CHANNELOUT_FRONTRIGHT],	0.0f,							0.0f,							0.0f,								0.0f,								// Right Out
			0.0f,							0.0f,							Chans[CHANNELOUT_FRONTCENTER],	0.0f,							0.0f,								0.0f,								// Center Out
			0.0f,							0.0f,							0.0f,							Chans[CHANNELOUT_LOWFREQUENCY], 0.0f,								0.0f,								// LFE Out
			0.0f,							0.0f,							0.0f,							0.0f,							Chans[CHANNELOUT_LEFTSURROUND],		0.0f,								// Left Surround Out
			0.0f,							0.0f,							0.0f,							0.0f,							0.0f,								Chans[CHANNELOUT_RIGHTSURROUND]		// Right Surround Out
		};

		// 5.1 sounds map 6 channels to 6 speakers
		AudioDevice->ValidateAPICall(TEXT("SetOutputMatrix (6 channel)"),
			Source->SetOutputMatrix(Destinations[DEST_DRY].pOutputVoice, 6, SPEAKER_COUNT, SpatialisationMatrix));
	}
}

/** 
 * Maps the sound to the relevant reverb effect
 */
void FXAudio2SoundSource::RouteToReverb(float ChannelVolumes[CHANNEL_MATRIX_COUNT])
{
	// Reverb must be applied to process this function because the index 
	// of the destination output voice may not be at DEST_REVERB.
	check( bReverbApplied );

	switch( Buffer->NumChannels )
	{
	case 1:
		RouteMonoToReverb(ChannelVolumes);
		break;

	case 2:
		RouteStereoToReverb(ChannelVolumes);
		break;
	}
}

void FXAudio2SoundSource::RouteMonoToReverb(float ChannelVolumes[CHANNEL_MATRIX_COUNT])
{
	if (IsUsingHrtfSpatializer())
	{
		RouteStereoToReverb(ChannelVolumes);
	}
	else
	{
		float SpatialisationMatrix[2] =
		{
			ChannelVolumes[CHANNELOUT_REVERB],
			ChannelVolumes[CHANNELOUT_REVERB],
		};

		// Update the dry output to the mastering voice
		AudioDevice->ValidateAPICall(TEXT("SetOutputMatrix (Mono reverb)"),
			Source->SetOutputMatrix(Destinations[DEST_REVERB].pOutputVoice, 1, 2, SpatialisationMatrix));
	}
}

void FXAudio2SoundSource::RouteStereoToReverb(float ChannelVolumes[CHANNEL_MATRIX_COUNT])
{
	float SpatialisationMatrix[4] =
	{
		ChannelVolumes[CHANNELOUT_REVERB], 0.0f,
		0.0f, ChannelVolumes[CHANNELOUT_REVERB]
	};

	// Stereo sounds map 2 channels to 6 speakers
	AudioDevice->ValidateAPICall(TEXT("SetOutputMatrix (Stereo reverb)"),
		Source->SetOutputMatrix(Destinations[DEST_REVERB].pOutputVoice, 2, 2, SpatialisationMatrix));
}


/** 
 * Maps the sound to the relevant radio effect.
 *
 * @param	ChannelVolumes	The volumes associated to each channel. 
 *							Note: Not all channels are mapped directly to a speaker.
 */
void FXAudio2SoundSource::RouteToRadio(float ChannelVolumes[CHANNEL_MATRIX_COUNT])
{
	// Radio distortion must be applied to process this function because 
	// the index of the destination output voice would be incorrect. 
	check( WaveInstance->bApplyRadioFilter );

	// Get the index for the radio voice because it doesn't 
	// necessarily match up to the enum value for radio.
	const int32 Index = GetDestinationVoiceIndexForEffect( DEST_RADIO );

	// If the index is -1, something changed with the Destinations array or 
	// SourceDestinations enum without an update to GetDestinationIndexForEffect().
	check( Index != -1 );

	// NOTE: The radio-distorted audio will only get routed to the center speaker.
	switch( Buffer->NumChannels )
	{
	case 1:
		{
			// Audio maps 1 channel to 6 speakers
			float OutputMatrix[SPEAKER_COUNT * 1] = 
			{		
				0.0f,
				0.0f,
				ChannelVolumes[CHANNELOUT_RADIO],
				0.0f,
				0.0f,
				0.0f,
			};

			// Mono sounds map 1 channel to 6 speakers.
			AudioDevice->ValidateAPICall( TEXT( "SetOutputMatrix (Mono radio)" ), 
				Source->SetOutputMatrix( Destinations[Index].pOutputVoice, 1, SPEAKER_COUNT, OutputMatrix) );
		}
		break;

	case 2:
		{
			// Audio maps 2 channels to 6 speakers
			float OutputMatrix[SPEAKER_COUNT * 2] = 
			{			
				0.0f, 0.0f,
				0.0f, 0.0f,
				ChannelVolumes[CHANNELOUT_RADIO], ChannelVolumes[CHANNELOUT_RADIO],
				0.0f, 0.0f,
				0.0f, 0.0f,
				0.0f, 0.0f,
			};

			// Stereo sounds map 2 channels to 6 speakers.
			AudioDevice->ValidateAPICall( TEXT( "SetOutputMatrix (Stereo radio)" ), 
				Source->SetOutputMatrix( Destinations[Index].pOutputVoice, 2, SPEAKER_COUNT, OutputMatrix) );
		}
		break;
	}
}


/**
 * Utility function for determining the proper index of an effect. Certain effects (such as: reverb and radio distortion) 
 * are optional. Thus, they may be NULL, yet XAudio2 cannot have a NULL output voice in the send list for this source voice.
 *
 * @return	The index of the destination XAudio2 submix voice for the given effect; -1 if effect not in destination array. 
 *
 * @param	Effect	The effect type's (Reverb, Radio Distoriton, etc) index to find. 
 */
int32 FXAudio2SoundSource::GetDestinationVoiceIndexForEffect( SourceDestinations Effect )
{
	int32 Index = -1;

	switch( Effect )
	{
	case DEST_DRY:
		// The dry mix is ALWAYS the first voice because always set it. 
		Index = 0;
		break;

	case DEST_REVERB:
		// If reverb is applied, then it comes right after the dry mix.
		Index = bReverbApplied ? DEST_REVERB : -1;
		break;

	case DEST_RADIO:
		// If radio distortion is applied, it depends on if there is 
		// reverb in the chain. Radio will always come after reverb.
		Index = ( WaveInstance->bApplyRadioFilter ) ? ( bReverbApplied ? DEST_RADIO : DEST_REVERB ) : -1;
		break;

	default:
		Index = -1;
	}

	return Index;
}

void FXAudio2SoundSourceCallback::OnBufferEnd(void* BufferContext)
{
	LLM_SCOPE(ELLMTag::Audio);

	if (BufferContext)
	{
		FXAudio2SoundSource* SoundSource = (FXAudio2SoundSource*)BufferContext;

		// Only submit more buffers if the source is playing (not stopped or paused)
		if (SoundSource->Playing && SoundSource->Source)
		{
			// Retrieve state source is in.
			XAUDIO2_VOICE_STATE SourceState;

			SoundSource->Source->GetState(&SourceState);

			const bool bIsRealTimeSource = SoundSource->XAudio2Buffer->SoundFormat == SoundFormat_PCMRT || SoundSource->XAudio2Buffer->SoundFormat == SoundFormat_Streaming;

			// If we have no queued buffers, we're either at the end of a sound, or starved
			// and we are expecting the sound to be finishing
			if (SourceState.BuffersQueued == 0 && (SoundSource->bBuffersToFlush || !bIsRealTimeSource))
			{
				// Set the flag to notify wave instances that we're finished
				SoundSource->bIsFinished = true;
				return;
			}

			// Service any real time sounds
			if (bIsRealTimeSource && !SoundSource->bBuffersToFlush && SourceState.BuffersQueued <= 2)
			{
				// Continue feeding new sound data (unless we are waiting for the sound to finish)
				SoundSource->HandleRealTimeSource(SourceState.BuffersQueued < 2);
				return;
			}
		}
	}
}

void FXAudio2SoundSourceCallback::OnLoopEnd( void* BufferContext )
{
	if( BufferContext )
	{
		FXAudio2SoundSource* Source = ( FXAudio2SoundSource* )BufferContext;
		Source->bLoopCallback = true;
	}
}

FString FXAudio2SoundSource::Describe(bool bUseLongName)
{
	return Describe_Internal(bUseLongName, true);
}

FString FXAudio2SoundSource::Describe_Internal(bool bUseLongName, bool bIncludeChannelVolumes)
{
	FString SpatializedVolumeInfo;
	if (bIncludeChannelVolumes && WaveInstance->bUseSpatialization)
	{
		float ChannelVolumes[CHANNEL_MATRIX_COUNT] = { 0.0f };
		GetChannelVolumes( ChannelVolumes, WaveInstance->GetActualVolume() );

		if (Buffer->NumChannels == 1)
		{
			SpatializedVolumeInfo = FString::Printf(TEXT(" (FL: %.2f FR: %.2f FC: %.2f LF: %.2f, LS: %.2f, RS: %.2f)"),
													ChannelVolumes[CHANNELOUT_FRONTLEFT],
													ChannelVolumes[CHANNELOUT_FRONTRIGHT],
													ChannelVolumes[CHANNELOUT_FRONTCENTER],
													ChannelVolumes[CHANNELOUT_LOWFREQUENCY],
													ChannelVolumes[CHANNELOUT_LEFTSURROUND],
													ChannelVolumes[CHANNELOUT_RIGHTSURROUND]);
		}
		else if (Buffer->NumChannels == 2)
		{
			SpatializedVolumeInfo = FString::Printf(TEXT(" Left: (FL: %.2f FR: %.2f FC: %.2f LF: %.2f, LS: %.2f, RS: %.2f), Right: (FL: %.2f FR: %.2f FC: %.2f LF: %.2f, LS: %.2f, RS: %.2f)"),
													ChannelVolumes[CHANNELOUT_FRONTLEFT],		
													ChannelVolumes[CHANNELOUT_FRONTRIGHT],		
													ChannelVolumes[CHANNELOUT_FRONTCENTER],		
													ChannelVolumes[CHANNELOUT_LOWFREQUENCY],	
													ChannelVolumes[CHANNELOUT_LEFTSURROUND],	
													ChannelVolumes[CHANNELOUT_RIGHTSURROUND],	
													ChannelVolumes[CHANNELOUT_FRONTLEFT + CHANNELOUT_COUNT],
													ChannelVolumes[CHANNELOUT_FRONTRIGHT + CHANNELOUT_COUNT],
													ChannelVolumes[CHANNELOUT_FRONTCENTER + CHANNELOUT_COUNT],
													ChannelVolumes[CHANNELOUT_LOWFREQUENCY + CHANNELOUT_COUNT],
													ChannelVolumes[CHANNELOUT_LEFTSURROUND + CHANNELOUT_COUNT],
													ChannelVolumes[CHANNELOUT_RIGHTSURROUND + CHANNELOUT_COUNT]);
		}
	}

	const FString SoundOwnerName = (WaveInstance->ActiveSound ? WaveInstance->ActiveSound->GetOwnerName() : TEXT("None"));

	return FString::Printf(TEXT("Wave: %s, Volume: %6.2f%s, Owner: %s"), 
		bUseLongName ? *WaveInstance->WaveData->GetPathName() : *WaveInstance->WaveData->GetName(),
		WaveInstance->GetActualVolume(),
		*SpatializedVolumeInfo,
		*SoundOwnerName);

}

void FXAudio2SoundSource::Update()
{
	SCOPE_CYCLE_COUNTER(STAT_AudioUpdateSources);

	if (!WaveInstance || (!bIsVirtual && !Source) || Paused || !bInitialized)
	{	
		return;
	}

	FSoundSource::UpdateCommon();

	// If the headphones have been unplugged after playing, set this voice to be virtual
	if (!AudioDevice->DeviceProperties->bAllowNewVoices)
	{
		bIsVirtual = true;
	}

	// If this is a virtual source, then do any notification on completion
	if (bIsVirtual)
	{
		if (PlaybackTime >= WaveInstance->WaveData->GetDuration())
		{
			if (WaveInstance->LoopingMode == LOOP_Never)
			{
				bIsFinished = true;
			}
			else
			{
				// This will trigger a loop callback notification
				bLoopCallback = true;
			}
		}
	}
	else
	{
			// Set the pitch on the xaudio2 source
		AudioDevice->ValidateAPICall( TEXT( "SetFrequencyRatio" ), 
			Source->SetFrequencyRatio( Pitch) );

		// Set whether to bleed to the rear speakers
		SetStereoBleed();

		// Set the amount to bleed to the LFE speaker
		SetLFEBleed();

		// Set the low pass filter frequency value
		SetFilterFrequency();

		if (LastLPFFrequency != LPFFrequency)
		{
			// Apply the low pass filter
			XAUDIO2_FILTER_PARAMETERS LPFParameters = { LowPassFilter, 1.0f, AudioDevice->GetLowPassFilterResonance() };

			check(AudioDevice->SampleRate > 0.0f);

			// Convert the frequency value to normalized radian frequency values where 0.0f to 2.0f sweeps 0.0hz to sample rate
			// and 1.0f is the nyquist frequency. A normalized frequency of 1.0f is an effective bypass.
			LPFParameters.Frequency = FMath::Clamp(2.0f * LPFFrequency / AudioDevice->SampleRate, 0.0f, 1.0f);

			AudioDevice->ValidateAPICall(TEXT("SetFilterParameters"),
										 Source->SetFilterParameters(&LPFParameters));

			LastLPFFrequency = LPFFrequency;
		}

		// Get the current xaudio2 source voice state to determine the number of frames played
		XAUDIO2_VOICE_STATE VoiceState;
		Source->GetState(&VoiceState);

		// XAudio2's "samples" appear to actually be frames (1 interleaved time-slice)
		NumFramesPlayed = VoiceState.SamplesPlayed;

		// Initialize channel volumes
		float ChannelVolumes[CHANNEL_MATRIX_COUNT] = { 0.0f };

		const float Volume = FSoundSource::GetDebugVolume(WaveInstance->GetActualVolume());

		GetChannelVolumes( ChannelVolumes, Volume );

		// Send to the 5.1 channels
		RouteDryToSpeakers(ChannelVolumes, Volume);

		// Send to the reverb channel
		if( bReverbApplied )
		{
			RouteToReverb( ChannelVolumes );
		}

		// If this audio can have radio distortion applied, 
		// send the volumes to the radio distortion voice. 
		if( WaveInstance->bApplyRadioFilter )
		{
			RouteToRadio( ChannelVolumes );
		}
	}

	FSoundSource::DrawDebugInfo();

}

float FXAudio2SoundSource::GetPlaybackPercent() const
{
	// If we didn't compute NumTotalFrames then there's no playback percent
	if (NumTotalFrames == 0)
	{
		return 0.0f;
	}

	const int32 CurrentFrame = StartFrame + NumFramesPlayed;

	// Compute the percent based on frames played and total frames
	const float Percent = (float)CurrentFrame / NumTotalFrames;

	if (WaveInstance->LoopingMode == LOOP_Never)
	{
		return FMath::Clamp(Percent, 0.0f, 1.0f);
	}
	else
	{
		// Wrap the playback percent for looping sounds
		return FMath::Fmod(Percent, 1.0f);
	}
}

void FXAudio2SoundSource::Play()
{
	if (WaveInstance)
	{
		// It's possible if Pause and Play are called while a sound is async initializing. In this case
		// we'll just not actually play the source here. Instead we'll call play when the sound finishes loading.
		if (Source && bInitialized)
		{
			AudioDevice->ValidateAPICall(TEXT("Start"),
										 Source->Start(0));
		}

		Paused = false;
		Playing = true;
		bBuffersToFlush = false;
		bLoopCallback = false;
	}
}

void FXAudio2SoundSource::Stop()
{
	bInitialized = false;
	IStreamingManager::Get().GetAudioStreamingManager().RemoveStreamingSoundSource(this);

	if( WaveInstance )
	{	
		Paused = false;
		Playing = false;

		// Free resources
		FreeResources();

		bBuffersToFlush = false;
		bLoopCallback = false;
		bResourcesNeedFreeing = false;
	}

	FSoundSource::Stop();
}

void FXAudio2SoundSource::Pause()
{
	if (WaveInstance)
	{
		if (Source)
		{
			// If a source is paused while it's async loading for realtime decoding,
			// we'll set the paused flag but our IXAudio2Source pointer won't be valid yet.
			// We check if the sound is paused after initialization finishes.
			check(bInitialized);
			AudioDevice->ValidateAPICall(TEXT("Stop"),
										 Source->Stop(0));
		}

		Paused = true;
	}
}

void FXAudio2SoundSource::HandleRealTimeSourceData(bool bLooped)
{
	// Have we reached the end of the compressed sound?
	if( bLooped )
	{
		switch( WaveInstance->LoopingMode )
		{
			case LOOP_Never:
				// Play out any queued buffers - once there are no buffers left, the state check at the beginning of IsFinished will fire
				bBuffersToFlush = true;
				XAudio2Buffers[CurrentBuffer].Flags |= XAUDIO2_END_OF_STREAM;
				break;

			case LOOP_WithNotification:
				// If we have just looped, and we are programmatically looping, send notification
				// This will trigger a WaveInstance->NotifyFinished() in the FXAudio2SoundSournce::IsFinished() function on main thread.
				bLoopCallback = true;
				break;

			case LOOP_Forever:
				// Let the sound loop indefinitely
				break;
		}
	}

	if (XAudio2Buffers[CurrentBuffer].AudioBytes > 0)
	{
		AudioDevice->ValidateAPICall(TEXT("SubmitSourceBuffer - PCMRT"),
									 Source->SubmitSourceBuffer(&XAudio2Buffers[CurrentBuffer]));
	}
	else
	{
		if (--CurrentBuffer < 0)
		{
			CurrentBuffer = 2;
		}
	}
}

void FXAudio2SoundSource::HandleRealTimeSource(bool bBlockForData)
{
	const bool bGetMoreData = bBlockForData || (RealtimeAsyncTask == nullptr);
	if (RealtimeAsyncTask)
	{
		const bool bTaskDone = RealtimeAsyncTask->IsDone();
		if (bTaskDone || bBlockForData)
		{
			bool bLooped = false;

			if (!bTaskDone)
			{
				RealtimeAsyncTask->EnsureCompletion();
			}

			switch(RealtimeAsyncTask->GetTask().GetTaskType())
			{
				case ERealtimeAudioTaskType::Decompress:
					bLooped = RealtimeAsyncTask->GetTask().GetBufferLooped();
					break;

				case ERealtimeAudioTaskType::Procedural:
					XAudio2Buffers[CurrentBuffer].AudioBytes = RealtimeAsyncTask->GetTask().GetBytesWritten();
					break;
			}

			delete RealtimeAsyncTask;
			RealtimeAsyncTask = nullptr;

			HandleRealTimeSourceData(bLooped);
		}
	}
	
	if (bGetMoreData)
	{
		// Update the buffer index
		if (++CurrentBuffer > 2)
		{
			CurrentBuffer = 0;
		}

		EDataReadMode DataReadMode;
		if (bPlayedCachedBuffer)
		{
			bPlayedCachedBuffer = false;
			DataReadMode = EDataReadMode::AsynchronousSkipFirstFrame;
		}
		else
		{
			DataReadMode = EDataReadMode::Asynchronous;
		}
		const bool bLooped = ReadMorePCMData(CurrentBuffer, DataReadMode);

		// If this was a synchronous read, then immediately write it
		if (RealtimeAsyncTask == nullptr)
		{
			HandleRealTimeSourceData(bLooped);
		}
	}
}

bool FXAudio2SoundSource::IsFinished()
{
	// A paused source is not finished.
	if (Paused || !bInitialized)
	{
		return false;
	}


	if (!WaveInstance || (!bIsVirtual && !Source))
	{
		return true;
	}

	if (bIsFinished)
	{
		WaveInstance->NotifyFinished();
		return true;
	}

	if (bLoopCallback && WaveInstance->LoopingMode == LOOP_WithNotification)
	{
		WaveInstance->NotifyFinished();
		bLoopCallback = false;
	}

	return false;
}

bool FXAudio2SoundSource::IsUsingHrtfSpatializer()
{
	return bUsingHRTFSpatialization;
}

bool FXAudio2SoundSource::CreateWithSpatializationEffect()
{
	return (Buffer->NumChannels == 1 &&
			AudioDevice->IsSpatializationPluginEnabled() &&
			WaveInstance->SpatializationMethod == ESoundSpatializationAlgorithm::SPATIALIZATION_HRTF);
}

/*------------------------------------------------------------------------------------
	FSpatializationHelper.
------------------------------------------------------------------------------------*/

/**
 * Constructor, initializing all member variables.
 */
FSpatializationHelper::FSpatializationHelper( void )
{
}

void FSpatializationHelper::Init()
{
	// Initialize X3DAudio
	//
	//  Speaker geometry configuration on the final mix, specifies assignment of channels
	//  to speaker positions, defined as per WAVEFORMATEXTENSIBLE.dwChannelMask
	X3DAudioInitialize( UE4_XAUDIO2_CHANNELMASK, X3DAUDIO_SPEED_OF_SOUND, X3DInstance );

	// Initialize 3D audio parameters
#if X3DAUDIO_VECTOR_IS_A_D3DVECTOR
	X3DAUDIO_VECTOR ZeroVector = { 0.0f, 0.0f, 0.0f };
#else	//X3DAUDIO_VECTOR_IS_A_D3DVECTOR
	X3DAUDIO_VECTOR ZeroVector(0.0f, 0.0f, 0.0f);
#endif	//X3DAUDIO_VECTOR_IS_A_D3DVECTOR

	// Set up listener parameters
	Listener.OrientFront.x = 0.0f;
	Listener.OrientFront.y = 0.0f;
	Listener.OrientFront.z = 1.0f;
	Listener.OrientTop.x = 0.0f;
	Listener.OrientTop.y = 1.0f;
	Listener.OrientTop.z = 0.0f;
	Listener.Position.x	= 0.0f;
	Listener.Position.y = 0.0f;
	Listener.Position.z = 0.0f;
	Listener.Velocity = ZeroVector;
	Listener.pCone = nullptr;

	// Set up emitter parameters
	Emitter.OrientFront.x = 0.0f;
	Emitter.OrientFront.y = 0.0f;
	Emitter.OrientFront.z = 1.0f;
	Emitter.OrientTop.x = 0.0f;
	Emitter.OrientTop.y = 1.0f;
	Emitter.OrientTop.z = 0.0f;
	Emitter.Position = ZeroVector;
	Emitter.Velocity = ZeroVector;
	Emitter.pCone = &Cone;
	Emitter.pCone->InnerAngle = 0.0f; 
	Emitter.pCone->OuterAngle = 0.0f;
	Emitter.pCone->InnerVolume = 0.0f;
	Emitter.pCone->OuterVolume = 1.0f;
	Emitter.pCone->InnerLPF = 0.0f;
	Emitter.pCone->OuterLPF = 1.0f;
	Emitter.pCone->InnerReverb = 0.0f;
	Emitter.pCone->OuterReverb = 1.0f;

	Emitter.ChannelCount = UE4_XAUDIO3D_INPUTCHANNELS;
	Emitter.ChannelRadius = 0.0f;
	// we aren't using the helper to spatialize multichannel files so we can set this nullptr
	Emitter.pChannelAzimuths = nullptr;

	// real volume -> 5.1-ch rate
	VolumeCurvePoint[0].Distance = 0.0f;
	VolumeCurvePoint[0].DSPSetting = 1.0f;
	VolumeCurvePoint[1].Distance = 1.0f;
	VolumeCurvePoint[1].DSPSetting = 1.0f;
	VolumeCurve.PointCount = ARRAY_COUNT( VolumeCurvePoint );
	VolumeCurve.pPoints = VolumeCurvePoint;

	ReverbVolumeCurvePoint[0].Distance = 0.0f;
	ReverbVolumeCurvePoint[0].DSPSetting = 0.5f;
	ReverbVolumeCurvePoint[1].Distance = 1.0f;
	ReverbVolumeCurvePoint[1].DSPSetting = 0.5f;
	ReverbVolumeCurve.PointCount = ARRAY_COUNT( ReverbVolumeCurvePoint );
	ReverbVolumeCurve.pPoints = ReverbVolumeCurvePoint;

	Emitter.pVolumeCurve = &VolumeCurve;
	Emitter.pLFECurve = NULL;
	Emitter.pLPFDirectCurve = NULL;
	Emitter.pLPFReverbCurve = NULL;
	Emitter.pReverbCurve = &ReverbVolumeCurve;
	Emitter.CurveDistanceScaler = 1.0f;
	Emitter.DopplerScaler = 1.0f;

	// Zero the matrix coefficients
	FMemory::Memzero(MatrixCoefficients, sizeof(float)*ARRAY_COUNT(MatrixCoefficients));

	DSPSettings.SrcChannelCount = UE4_XAUDIO3D_INPUTCHANNELS;
	DSPSettings.DstChannelCount = SPEAKER_COUNT;
	DSPSettings.pMatrixCoefficients = MatrixCoefficients;
	DSPSettings.pDelayTimes = nullptr;
}

void FSpatializationHelper::DumpSpatializationState() const
{
	struct FLocal
	{
		static void DumpChannelArray(const FString& Indent, const FString& ArrayName, int32 NumChannels, const float* pChannelArray)
		{
			if (pChannelArray)
			{
				FString CommaSeparatedValueString;
				for (int32 ChannelIdx = 0; ChannelIdx < NumChannels; ++ChannelIdx)
				{
					if ( ChannelIdx > 0 )
					{
						CommaSeparatedValueString += TEXT(",");
					}

					CommaSeparatedValueString += FString::Printf(TEXT("%f"), pChannelArray[ChannelIdx]);
				}

				UE_LOG(LogXAudio2, Log, TEXT("%s%s: {%s}"), *Indent, *ArrayName, *CommaSeparatedValueString);
			}
			else
			{
				UE_LOG(LogXAudio2, Log, TEXT("%s%s: NULL"), *Indent, *ArrayName);
			}
		}

		static void DumpCone(const FString& Indent, const FString& ConeName, const X3DAUDIO_CONE* pCone)
		{
			if (pCone)
			{
				UE_LOG(LogXAudio2, Log, TEXT("%s%s"), *Indent, *ConeName);
				UE_LOG(LogXAudio2, Log, TEXT("%s  InnerAngle: %f"), *Indent, pCone->InnerAngle);
				UE_LOG(LogXAudio2, Log, TEXT("%s  OuterAngle: %f"), *Indent, pCone->OuterAngle);
				UE_LOG(LogXAudio2, Log, TEXT("%s  InnerVolume: %f"), *Indent, pCone->InnerVolume);
				UE_LOG(LogXAudio2, Log, TEXT("%s  OuterVolume: %f"), *Indent, pCone->OuterVolume);
				UE_LOG(LogXAudio2, Log, TEXT("%s  InnerLPF: %f"), *Indent, pCone->InnerLPF);
				UE_LOG(LogXAudio2, Log, TEXT("%s  OuterLPF: %f"), *Indent, pCone->OuterLPF);
				UE_LOG(LogXAudio2, Log, TEXT("%s  InnerReverb: %f"), *Indent, pCone->InnerReverb);
				UE_LOG(LogXAudio2, Log, TEXT("%s  OuterReverb: %f"), *Indent, pCone->OuterReverb);
			}
			else
			{
				UE_LOG(LogXAudio2, Log, TEXT("%s%s: NULL"), *Indent, *ConeName);
			}
		}

		static void DumpDistanceCurvePoint(const FString& Indent, const FString& PointName, uint32 Index, const X3DAUDIO_DISTANCE_CURVE_POINT& Point)
		{
			UE_LOG(LogXAudio2, Log, TEXT("%s%s[%u]: {%f,%f}"), *Indent, *PointName, Index, Point.Distance, Point.DSPSetting);
		}

		static void DumpDistanceCurve(const FString& Indent, const FString& CurveName, const X3DAUDIO_DISTANCE_CURVE* pCurve)
		{
			const uint32 MaxPointsToDump = 20;
			if (pCurve)
			{
				UE_LOG(LogXAudio2, Log, TEXT("%s%s: %u points"), *Indent, *CurveName, pCurve->PointCount);
				for (uint32 PointIdx = 0; PointIdx < pCurve->PointCount && PointIdx < MaxPointsToDump; ++PointIdx)
				{
					const X3DAUDIO_DISTANCE_CURVE_POINT& CurPoint = pCurve->pPoints[PointIdx];
					DumpDistanceCurvePoint(Indent + TEXT("  "), TEXT("pPoints"), PointIdx, CurPoint);
				}
			}
			else
			{
				UE_LOG(LogXAudio2, Log, TEXT("%s%s: NULL"), *Indent, *CurveName);
			}
		}
	};

	UE_LOG(LogXAudio2, Log, TEXT("Dumping all XAudio2 Spatialization"));
	UE_LOG(LogXAudio2, Log, TEXT("==================================="));
	
	// X3DInstance
	UE_LOG(LogXAudio2, Log, TEXT("  X3DInstance: %#010x"), X3DInstance);

	// DSPSettings
	UE_LOG(LogXAudio2, Log, TEXT("  DSPSettings"));
	FLocal::DumpChannelArray(TEXT("    "), TEXT("pMatrixCoefficients"), ARRAY_COUNT(MatrixCoefficients), DSPSettings.pMatrixCoefficients);
	UE_LOG(LogXAudio2, Log, TEXT("    SrcChannelCount: %u"), DSPSettings.SrcChannelCount);
	UE_LOG(LogXAudio2, Log, TEXT("    DstChannelCount: %u"), DSPSettings.DstChannelCount);
	UE_LOG(LogXAudio2, Log, TEXT("    LPFDirectCoefficient: %f"), DSPSettings.LPFDirectCoefficient);
	UE_LOG(LogXAudio2, Log, TEXT("    LPFReverbCoefficient: %f"), DSPSettings.LPFReverbCoefficient);
	UE_LOG(LogXAudio2, Log, TEXT("    ReverbLevel: %f"), DSPSettings.ReverbLevel);
	UE_LOG(LogXAudio2, Log, TEXT("    DopplerFactor: %f"), DSPSettings.DopplerFactor);
	UE_LOG(LogXAudio2, Log, TEXT("    EmitterToListenerAngle: %f"), DSPSettings.EmitterToListenerAngle);
	UE_LOG(LogXAudio2, Log, TEXT("    EmitterToListenerDistance: %f"), DSPSettings.EmitterToListenerDistance);
	UE_LOG(LogXAudio2, Log, TEXT("    EmitterVelocityComponent: %f"), DSPSettings.EmitterVelocityComponent);
	UE_LOG(LogXAudio2, Log, TEXT("    ListenerVelocityComponent: %f"), DSPSettings.ListenerVelocityComponent);

	// Listener
	UE_LOG(LogXAudio2, Log, TEXT("  Listener"));
	UE_LOG(LogXAudio2, Log, TEXT("    OrientFront: {%f,%f,%f}"), Listener.OrientFront.x, Listener.OrientFront.y, Listener.OrientFront.z);
	UE_LOG(LogXAudio2, Log, TEXT("    OrientTop: {%f,%f,%f}"), Listener.OrientTop.x, Listener.OrientTop.y, Listener.OrientTop.z);
	UE_LOG(LogXAudio2, Log, TEXT("    Position: {%f,%f,%f}"), Listener.Position.x, Listener.Position.y, Listener.Position.z);
	UE_LOG(LogXAudio2, Log, TEXT("    Velocity: {%f,%f,%f}"), Listener.Velocity.x, Listener.Velocity.y, Listener.Velocity.z);
	FLocal::DumpCone(TEXT("    "), TEXT("pCone"), Listener.pCone);

	// Emitter
	UE_LOG(LogXAudio2, Log, TEXT("  Emitter"));
	FLocal::DumpCone(TEXT("    "), TEXT("pCone"), Emitter.pCone);
	UE_LOG(LogXAudio2, Log, TEXT("    OrientFront: {%f,%f,%f}"), Emitter.OrientFront.x, Emitter.OrientFront.y, Emitter.OrientFront.z);
	UE_LOG(LogXAudio2, Log, TEXT("    OrientTop: {%f,%f,%f}"), Emitter.OrientTop.x, Emitter.OrientTop.y, Emitter.OrientTop.z);
	UE_LOG(LogXAudio2, Log, TEXT("    Position: {%f,%f,%f}"), Emitter.Position.x, Emitter.Position.y, Emitter.Position.z);
	UE_LOG(LogXAudio2, Log, TEXT("    Velocity: {%f,%f,%f}"), Emitter.Velocity.x, Emitter.Velocity.y, Emitter.Velocity.z);
	UE_LOG(LogXAudio2, Log, TEXT("    InnerRadius: %f"), Emitter.InnerRadius);
	UE_LOG(LogXAudio2, Log, TEXT("    InnerRadiusAngle: %f"), Emitter.InnerRadiusAngle);
	UE_LOG(LogXAudio2, Log, TEXT("    ChannelCount: %u"), Emitter.ChannelCount);
	UE_LOG(LogXAudio2, Log, TEXT("    ChannelRadius: %f"), Emitter.ChannelRadius);

	if ( Emitter.pChannelAzimuths )
	{
		UE_LOG(LogXAudio2, Log, TEXT("    pChannelAzimuths: %f"), *Emitter.pChannelAzimuths);
	}
	else
	{
		UE_LOG(LogXAudio2, Log, TEXT("    pChannelAzimuths: NULL"));
	}

	FLocal::DumpDistanceCurve(TEXT("    "), TEXT("pVolumeCurve"), Emitter.pVolumeCurve);
	FLocal::DumpDistanceCurve(TEXT("    "), TEXT("pLFECurve"), Emitter.pLFECurve);
	FLocal::DumpDistanceCurve(TEXT("    "), TEXT("pLPFDirectCurve"), Emitter.pLPFDirectCurve);
	FLocal::DumpDistanceCurve(TEXT("    "), TEXT("pLPFReverbCurve"), Emitter.pLPFReverbCurve);
	FLocal::DumpDistanceCurve(TEXT("    "), TEXT("pReverbCurve"), Emitter.pReverbCurve);

	UE_LOG(LogXAudio2, Log, TEXT("    CurveDistanceScaler: %f"), Emitter.CurveDistanceScaler);
	UE_LOG(LogXAudio2, Log, TEXT("    DopplerScaler: %f"), Emitter.DopplerScaler);

	// Cone
	FLocal::DumpCone(TEXT("  "), TEXT("Cone"), &Cone);

	// VolumeCurvePoint
	FLocal::DumpDistanceCurvePoint(TEXT("  "), TEXT("VolumeCurvePoint"), 0, VolumeCurvePoint[0]);
	FLocal::DumpDistanceCurvePoint(TEXT("  "), TEXT("VolumeCurvePoint"), 1, VolumeCurvePoint[1]);
	
	// VolumeCurve
	FLocal::DumpDistanceCurve(TEXT("  "), TEXT("VolumeCurve"), &VolumeCurve);

	// ReverbVolumeCurvePoint
	FLocal::DumpDistanceCurvePoint(TEXT("  "), TEXT("ReverbVolumeCurvePoint"), 0, ReverbVolumeCurvePoint[0]);
	FLocal::DumpDistanceCurvePoint(TEXT("  "), TEXT("ReverbVolumeCurvePoint"), 1, ReverbVolumeCurvePoint[1]);

	// ReverbVolumeCurve
	FLocal::DumpDistanceCurve(TEXT("  "), TEXT("ReverbVolumeCurve"), &ReverbVolumeCurve);
	
	// EmitterAzimuths
	FLocal::DumpChannelArray(TEXT("  "), TEXT("EmitterAzimuths"), UE4_XAUDIO3D_INPUTCHANNELS, EmitterAzimuths);
	
	// MatrixCoefficients
	FLocal::DumpChannelArray(TEXT("  "), TEXT("MatrixCoefficients"), ARRAY_COUNT(MatrixCoefficients), MatrixCoefficients);
}

void FSpatializationHelper::CalculateDolbySurroundRate( const FVector& OrientFront, const FVector& ListenerPosition, const FVector& EmitterPosition, float OmniRadius, float* OutVolumes  )
{
#if ENABLE_NAN_DIAGNOSTIC
	OrientFront.DiagnosticCheckNaN(TEXT("FSpatializationHelper: OrientFront"));
	ListenerPosition.DiagnosticCheckNaN(TEXT("FSpatializationHelper: ListenerPosition"));
	EmitterPosition.DiagnosticCheckNaN(TEXT("FSpatializationHelper: EmitterPosition"));
	static bool bLoggedOmniRadius = false;
	if (!FMath::IsFinite(OmniRadius) && !bLoggedOmniRadius)
	{
		bLoggedOmniRadius = true;
		const FString NaNorINF = FMath::IsNaN(OmniRadius) ? TEXT("NaN") : TEXT("INF");
		UE_LOG(LogXAudio2, Warning, TEXT("OmniRadius generated a %s: %f"), *NaNorINF, OmniRadius);
	}
#endif

	uint32 CalculateFlags = X3DAUDIO_CALCULATE_MATRIX | X3DAUDIO_CALCULATE_REVERB;

	Listener.OrientFront.x = OrientFront.X;
	Listener.OrientFront.y = OrientFront.Y;
	Listener.OrientFront.z = OrientFront.Z;
	Listener.Position.x = ListenerPosition.X;
	Listener.Position.y = ListenerPosition.Y;
	Listener.Position.z = ListenerPosition.Z;
	Emitter.Position.x = EmitterPosition.X;
	Emitter.Position.y = EmitterPosition.Y;
	Emitter.Position.z = EmitterPosition.Z;
	Emitter.InnerRadius = OmniRadius*OmniRadius;
	Emitter.InnerRadiusAngle = 0;

	X3DAudioCalculate( X3DInstance, &Listener, &Emitter, CalculateFlags, &DSPSettings );

	for( int32 SpeakerIndex = 0; SpeakerIndex < SPEAKER_COUNT; SpeakerIndex++ )
	{
		OutVolumes[SpeakerIndex] *= DSPSettings.pMatrixCoefficients[SpeakerIndex];

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
		static bool bLoggedDSPSettings = false;
		// Detect and warn about NaN and INF volumes. XAudio does not do this internally and behavior is undefined.
		if (!FMath::IsFinite(OutVolumes[SpeakerIndex]) && !bLoggedDSPSettings)
		{
			bLoggedDSPSettings = true;
			const FString NaNorINF = FMath::IsNaN(OutVolumes[SpeakerIndex]) ? TEXT("NaN") : TEXT("INF");
			UE_LOG(LogXAudio2, Warning, TEXT("CalculateDolbySurroundRate generated a %s in channel %d. OmniRadius:%f MatrixCoefficient:%f"),
				*NaNorINF, SpeakerIndex, OmniRadius, DSPSettings.pMatrixCoefficients[SpeakerIndex]);
			//DumpSpatializationState();

#if ENABLE_NAN_DIAGNOSTIC
			DumpSpatializationState();
			//logOrEnsureNanError(TEXT("CalculateDolbySurroundRate generated a %s in channel %d. OmniRadius:%f MatrixCoefficient:%f"),
			//	*NaNorINF, SpeakerIndex, OmniRadius, DSPSettings.pMatrixCoefficients[SpeakerIndex]);
#endif
			// Zero the coefficients so we don't continue getting bad values
			FMemory::Memzero(MatrixCoefficients, sizeof(float)*ARRAY_COUNT(MatrixCoefficients));
		}
#endif
	}

	OutVolumes[CHANNELOUT_REVERB] *= DSPSettings.ReverbLevel;
}

/*------------------------------------------------------------------------------------
	FXMPHelper.
------------------------------------------------------------------------------------*/

/**
 * FXMPHelper constructor - Protected.
 */
FXMPHelper::FXMPHelper( void )
	: CinematicAudioCount( 0 )
, MoviePlaying( false )
, XMPEnabled( true )
, XMPBlocked( false )
{
}

/**
 * FXMPHelper destructor.
 */
FXMPHelper::~FXMPHelper( void )
{
}

/**
 * Records that a cinematic audio track has started playing.
 */
void FXMPHelper::CinematicAudioStarted( void )
{
	check( CinematicAudioCount >= 0 );
	CinematicAudioCount++;
	CountsUpdated();
}

/**
 * Records that a cinematic audio track has stopped playing.
 */
void FXMPHelper::CinematicAudioStopped( void )
{
	check( CinematicAudioCount > 0 );
	CinematicAudioCount--;
	CountsUpdated();
}

/**
 * Records that a cinematic audio track has started playing.
 */
void FXMPHelper::MovieStarted( void )
{
	MoviePlaying = true;
	CountsUpdated();
}

/**
 * Records that a cinematic audio track has stopped playing.
 */
void FXMPHelper::MovieStopped( void )
{
	MoviePlaying = false;
	CountsUpdated();
}

/**
 * Called every frame to update XMP status if necessary
 */
void FXMPHelper::CountsUpdated( void )
{
	if( XMPEnabled )
	{
		if( ( MoviePlaying == true ) || ( CinematicAudioCount > 0 ) )
		{
			XMPEnabled = false;
		}
	}
	else
	{
		if( ( MoviePlaying == false ) && ( CinematicAudioCount == 0 ) )
		{
			XMPEnabled = true;
		}
	}
}

// end
