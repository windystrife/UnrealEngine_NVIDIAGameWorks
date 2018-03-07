// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
 	CoreAudioSource.cpp: Unreal CoreAudio source interface object.
 =============================================================================*/

/*------------------------------------------------------------------------------------
 Audio includes.
 ------------------------------------------------------------------------------------*/

#include "CoreAudioDevice.h"
#include "CoreAudioEffects.h"
#include "ContentStreaming.h"

#define AUDIO_DISTANCE_FACTOR ( 0.0127f )

// CoreAudio functions may return error -10863 (output device not available) if called while OS X is switching to a different output device.
// This happens, for example, when headphones are plugged in or unplugged.
#define SAFE_CA_CALL(Expression)\
{\
	OSStatus Status = noErr;\
	do {\
		Status = Expression;\
	} while (Status == -10863);\
	check(Status == noErr);\
}


/*------------------------------------------------------------------------------------
 FCoreAudioSoundSource.
 ------------------------------------------------------------------------------------*/

/**
 * Simple constructor
 */
FCoreAudioSoundSource::FCoreAudioSoundSource( FAudioDevice* InAudioDevice )
:	FSoundSource( InAudioDevice ),
	CoreAudioBuffer(nullptr),
	CoreAudioConverter(nullptr),
	RealtimeAsyncTask(nullptr),
	bStreamedSound( false ),
	bBuffersToFlush( false ),
	SourceNode( 0 ),
	SourceUnit( NULL ),
	EQNode( 0 ),
	EQUnit( NULL ),
	LowPassNode( 0 ),
	LowPassUnit( NULL ),
	RadioNode( 0 ),
	RadioUnit( NULL ),
	bRadioMuted( false ),
	ReverbNode( 0 ),
	ReverbUnit( NULL ),
	bReverbMuted( false ),
	bDryMuted( false ),
	AudioChannel( 0 ),
	BufferInUse( 0 ),
	NumActiveBuffers( 0 ),
	MixerInputNumber( -1 )
{
	AudioDevice = ( FCoreAudioDevice *)InAudioDevice;
	check( AudioDevice );
	Effects = ( FCoreAudioEffectsManager* )AudioDevice->GetEffects();
	check( Effects );

	FMemory::Memzero( CoreAudioBuffers, sizeof( CoreAudioBuffers ) );
}

/**
 * Destructor, cleaning up voice
 */
FCoreAudioSoundSource::~FCoreAudioSoundSource()
{
	FreeResources();
}

/**
 * Free up any allocated resources
 */
void FCoreAudioSoundSource::FreeResources( void )
{
	if (RealtimeAsyncTask)
	{
		RealtimeAsyncTask->EnsureCompletion();
		delete RealtimeAsyncTask;
		RealtimeAsyncTask = nullptr;
	}

	// If we're a streaming buffer...
	if( bStreamedSound )
	{
		// ... free the buffers
		for (int32 Index = 0; Index < 3; Index++)
		{
			if (CoreAudioBuffers[Index].AudioData)
			{
				FMemory::Free((void*)CoreAudioBuffers[Index].AudioData);
				CoreAudioBuffers[Index].AudioData = nullptr;
			}
		}

		// Buffers without a valid resource ID are transient and need to be deleted.
		if( CoreAudioBuffer )
		{
			check( CoreAudioBuffer->ResourceID == 0 );
			delete CoreAudioBuffer;
			CoreAudioBuffer = nullptr;

			// Null out the base-class ptr
			Buffer = nullptr;
		}
		
		bStreamedSound = false;
	}
}

/** 
 * Submit the relevant audio buffers to the system
 */
void FCoreAudioSoundSource::SubmitPCMBuffers( void )
{
	SCOPE_CYCLE_COUNTER( STAT_AudioSubmitBuffersTime );

	FMemory::Memzero( CoreAudioBuffers, sizeof( CoreAudioBuffers ) );
	
	bStreamedSound = false;

	NumActiveBuffers=1;
	BufferInUse=0;

	CoreAudioBuffers[0].AudioData = CoreAudioBuffer->PCMData;
	CoreAudioBuffers[0].AudioDataSize = CoreAudioBuffer->PCMDataSize;
}

bool FCoreAudioSoundSource::ReadMorePCMData( const int32 BufferIndex, EDataReadMode DataReadMode )
{
	CoreAudioBuffers[BufferIndex].ReadCursor = 0;

	USoundWave* WaveData = WaveInstance->WaveData;
	if( WaveData && WaveData->bProcedural )
	{
		const int32 MaxSamples = ( MONO_PCM_BUFFER_SIZE * CoreAudioBuffer->NumChannels ) / sizeof( int16 );

		if (DataReadMode == EDataReadMode::Synchronous || WaveData->bCanProcessAsync == false)
		{
			const int32 BytesRead = WaveData->GeneratePCMData(CoreAudioBuffers[BufferIndex].AudioData, MaxSamples);
			CoreAudioBuffers[BufferIndex].AudioDataSize = BytesRead;

			if (BytesRead > 0)
			{
				++NumActiveBuffers;
			}
		}
		else
		{
			RealtimeAsyncTask = new FAsyncRealtimeAudioTask(WaveData, (uint8*)CoreAudioBuffers[BufferIndex].AudioData, MaxSamples);
			RealtimeAsyncTask->StartBackgroundTask();
		}
	
		// we're never actually "looping" here.
		return false;
	}
	else
	{
		if (DataReadMode == EDataReadMode::Synchronous)
		{
			++NumActiveBuffers;
			return CoreAudioBuffer->ReadCompressedData( CoreAudioBuffers[BufferIndex].AudioData, WaveInstance->LoopingMode != LOOP_Never );
		}
		else
		{
			RealtimeAsyncTask = new FAsyncRealtimeAudioTask(CoreAudioBuffer, (uint8*)CoreAudioBuffers[BufferIndex].AudioData, WaveInstance->LoopingMode != LOOP_Never, DataReadMode == EDataReadMode::AsynchronousSkipFirstFrame);
			RealtimeAsyncTask->StartBackgroundTask();
			return false;
		}
	}
}

/** 
 * Submit the relevant audio buffers to the system
 */
void FCoreAudioSoundSource::SubmitPCMRTBuffers( void )
{
	SCOPE_CYCLE_COUNTER( STAT_AudioSubmitBuffersTime );

	FMemory::Memzero( CoreAudioBuffers, sizeof( CoreAudioBuffers ) );

	bStreamedSound = true;

	const uint32 BufferSize = MONO_PCM_BUFFER_SIZE * CoreAudioBuffer->NumChannels;

	// Set up double buffer area to decompress to
	CoreAudioBuffers[0].AudioData = (uint8*)FMemory::Malloc(BufferSize);
	CoreAudioBuffers[0].AudioDataSize = BufferSize;

	CoreAudioBuffers[1].AudioData = (uint8*)FMemory::Malloc(BufferSize);
	CoreAudioBuffers[1].AudioDataSize = BufferSize;

	CoreAudioBuffers[2].AudioData = (uint8*)FMemory::Malloc(BufferSize);
	CoreAudioBuffers[2].AudioDataSize = BufferSize;

	NumActiveBuffers = 0;
	BufferInUse = 0;
	
	// Only use the cached data if we're starting from the beginning, otherwise we'll have to take a synchronous hit
	bool bSkipFirstBuffer = false;;
	if (WaveInstance->WaveData && WaveInstance->WaveData->CachedRealtimeFirstBuffer && WaveInstance->StartTime == 0.f)
	{
		FMemory::Memcpy((uint8*)CoreAudioBuffers[0].AudioData, WaveInstance->WaveData->CachedRealtimeFirstBuffer, BufferSize);
		FMemory::Memcpy((uint8*)CoreAudioBuffers[1].AudioData, WaveInstance->WaveData->CachedRealtimeFirstBuffer + BufferSize, BufferSize);
		bSkipFirstBuffer = true;
		NumActiveBuffers = 2;
	}
	else
	{
		ReadMorePCMData(0, EDataReadMode::Synchronous);
		ReadMorePCMData(1, EDataReadMode::Synchronous);
	}

	// Start the async population of the next buffer
	EDataReadMode DataReadMode = EDataReadMode::Asynchronous;
	if (CoreAudioBuffer->SoundFormat == ESoundFormat::SoundFormat_Streaming)
	{
		DataReadMode =  EDataReadMode::Synchronous;
	}
	else if (bSkipFirstBuffer)
	{
		DataReadMode =  EDataReadMode::AsynchronousSkipFirstFrame;
	}
	
	ReadMorePCMData(2, DataReadMode);
}

/**
 * Initializes a source with a given wave instance and prepares it for playback.
 *
 * @param	WaveInstance	wave instance being primed for playback
 * @return	true			if initialization was successful, false otherwise
 */
bool FCoreAudioSoundSource::Init( FWaveInstance* InWaveInstance )
{
	FSoundSource::InitCommon();

	if (InWaveInstance->OutputTarget != EAudioOutputTarget::Controller)
	{
		// Find matching buffer.
		CoreAudioBuffer = FCoreAudioSoundBuffer::Init( AudioDevice, InWaveInstance->WaveData, InWaveInstance->StartTime > 0.f );
		Buffer = nullptr;

		if( CoreAudioBuffer && CoreAudioBuffer->NumChannels > 0 )
		{
			SCOPE_CYCLE_COUNTER( STAT_AudioSourceInitTime );

			if (CoreAudioBuffer->NumChannels < 3)
			{
				MixerInputNumber = AudioDevice->GetFreeMixer3DInput();
			}
			else
			{
				MixerInputNumber = AudioDevice->GetFreeMatrixMixerInput();
			}

			if (MixerInputNumber == -1)
			{
				return false;
			}

			AudioChannel = AudioDevice->FindFreeAudioChannel();
			if (AudioChannel == 0)
			{
				return false;
			}


			Buffer = CoreAudioBuffer;
			WaveInstance = InWaveInstance;

			// Set whether to apply reverb
			SetReverbApplied( true );

			if (WaveInstance->StartTime > 0.f)
			{
				CoreAudioBuffer->Seek(WaveInstance->StartTime);
			}

			// Submit audio buffers
			switch( CoreAudioBuffer->SoundFormat )
			{
				case SoundFormat_PCM:
				case SoundFormat_PCMPreview:
					SubmitPCMBuffers();
					break;
				
				case SoundFormat_PCMRT:
				case SoundFormat_Streaming:
					SubmitPCMRTBuffers();
					break;
			}

			// Initialization succeeded.
			return( true );
		}
	}
	
	// Initialization failed.
	return false;
}

/**
 * Updates the source specific parameter like e.g. volume and pitch based on the associated
 * wave instance.	
 */
void FCoreAudioSoundSource::Update( void )
{
	SCOPE_CYCLE_COUNTER( STAT_AudioUpdateSources );

	if( !WaveInstance || Paused || !AudioChannel )
	{	
		return;
	}

	FSoundSource::UpdateCommon();

	check(AudioChannel != 0);
	check(MixerInputNumber != -1);

	float Volume = 0.0f;
	
	if (!AudioDevice->IsAudioDeviceMuted())
	{
		Volume = WaveInstance->GetActualVolume();
	}

	Volume *= AudioDevice->GetPlatformAudioHeadroom();

	if( CoreAudioBuffer->NumChannels < 3 )
	{
		float Azimuth = 0.0f;
		float Elevation = 0.0f;

		if( SetStereoBleed() )
		{
			// Emulate the bleed to rear speakers followed by stereo fold down
			Volume *= 1.25f;
		}
		
		// apply global multiplier (ie to disable sound when not the foreground app)
		Volume = FMath::Clamp<float>( Volume, 0.0f, MAX_VOLUME );
		
		// Convert to dB
		Volume = 20.0 * log10(Volume);
		Volume = FMath::Clamp<float>( Volume, -120.0f, 20.0f );

		Volume = FSoundSource::GetDebugVolume(Volume);
	
		// Set the HighFrequencyGain value
		SetFilterFrequency();
		
		if( WaveInstance->bApplyRadioFilter )
		{
			Volume = WaveInstance->RadioFilterVolume;
		}
		else if( WaveInstance->bUseSpatialization )
		{
			FVector Direction = AudioDevice->InverseTransform.TransformPosition(WaveInstance->Location).GetSafeNormal();

			FVector EmitterPosition;
			EmitterPosition.X = -Direction.Z;
			EmitterPosition.Y = Direction.Y;
			EmitterPosition.Z = Direction.X;

			FRotator Rotation = EmitterPosition.Rotation();
			Azimuth = Rotation.Yaw;
			Elevation = Rotation.Pitch;
		}

		SAFE_CA_CALL( AudioUnitSetParameter( AudioDevice->GetMixer3DUnit(), k3DMixerParam_Gain, kAudioUnitScope_Input, MixerInputNumber, Volume, 0 ) );
		SAFE_CA_CALL( AudioUnitSetParameter( AudioDevice->GetMixer3DUnit(), k3DMixerParam_PlaybackRate, kAudioUnitScope_Input, MixerInputNumber, Pitch, 0 ) );
		SAFE_CA_CALL( AudioUnitSetParameter( AudioDevice->GetMixer3DUnit(), k3DMixerParam_Azimuth, kAudioUnitScope_Input, MixerInputNumber, Azimuth, 0 ) );
		SAFE_CA_CALL( AudioUnitSetParameter( AudioDevice->GetMixer3DUnit(), k3DMixerParam_Elevation, kAudioUnitScope_Input, MixerInputNumber, Elevation, 0 ) );
	}
	else
	{
		// apply global multiplier (ie to disable sound when not the foreground app)
		Volume = FMath::Clamp<float>( Volume, 0.0f, MAX_VOLUME );
		
		if( AudioDevice->GetMixDebugState() == DEBUGSTATE_IsolateReverb )
		{
			Volume = 0.0f;
		}

		AudioDevice->SetMatrixMixerInputVolume( MixerInputNumber, Volume );
	}
}

/**
 * Plays the current wave instance.	
 */
void FCoreAudioSoundSource::Play( void )
{
	if( WaveInstance )
	{
		if (!Paused)
		{
			if (AttachToAUGraph())
			{
				Paused = false;
				Playing = true;

				// Updates the source which e.g. sets the pitch and volume.
				Update();
			}
		}
		else
		{
			// No need to re-attach the sound to the graph if its just unpausing
			Paused = false;
			Playing = true;
		}
	}
}

/**
 * Stops the current wave instance and detaches it from the source.	
 */
void FCoreAudioSoundSource::Stop( void )
{
	FScopeLock Lock(&CriticalSection);

	IStreamingManager::Get().GetAudioStreamingManager().RemoveStreamingSoundSource(this);

	if( WaveInstance )
	{
		if( Playing && AudioChannel )
		{
			DetachFromAUGraph();

			// Free resources
			FreeResources();
		}

		Paused = false;
		Playing = false;
		CoreAudioBuffer = nullptr;
		bBuffersToFlush = false;
	}

	FSoundSource::Stop();
}

/**
 * Pauses playback of current wave instance.
 */
void FCoreAudioSoundSource::Pause( void )
{
	if( WaveInstance )
	{
		// Note: no need to detach from graph when pausing (this is not stopping a sound)
		Paused = true;
	}
}

void FCoreAudioSoundSource::HandleRealTimeSourceData(bool bLooped)
{
	// Have we reached the end of the compressed sound?
	if( bLooped )
	{
		switch( WaveInstance->LoopingMode )
		{
			case LOOP_Never:
				// Play out any queued buffers - once there are no buffers left, the state check at the beginning of IsFinished will fire
				bBuffersToFlush = true;
				break;
				
			case LOOP_WithNotification:
				// If we have just looped, and we are programmatically looping, send notification
				WaveInstance->NotifyFinished();
				break;
				
			case LOOP_Forever:
				// Let the sound loop indefinitely
				break;
		}
	}
}

/**
 * Handles feeding new data to a real time decompressed sound
 */
void FCoreAudioSoundSource::HandleRealTimeSource(bool bBlockForData)
{
	FScopeLock Lock(&CriticalSection);

	const bool bGetMoreData = bBlockForData || (RealtimeAsyncTask == nullptr);
	int32 BufferIndex = (BufferInUse + NumActiveBuffers) % 3;
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
				++NumActiveBuffers;
				break;

			case ERealtimeAudioTaskType::Procedural:
				const int32 BytesWritten = RealtimeAsyncTask->GetTask().GetBytesWritten();
				CoreAudioBuffers[BufferIndex].AudioDataSize = BytesWritten;
				if (BytesWritten > 0)
				{
					++NumActiveBuffers;
				}
				break;
			}

			delete RealtimeAsyncTask;
			RealtimeAsyncTask = nullptr;

			HandleRealTimeSourceData(bLooped);

			if (++BufferIndex > 2)
			{
				BufferIndex = 0;
			}
		}
	}

	if (bGetMoreData)
	{
		// Get the next bit of streaming data
		const bool bLooped = ReadMorePCMData(BufferIndex, (CoreAudioBuffer->SoundFormat == ESoundFormat::SoundFormat_Streaming ? EDataReadMode::Synchronous : EDataReadMode::Asynchronous));

		if (RealtimeAsyncTask == nullptr)
		{
			HandleRealTimeSourceData(bLooped);
		}
	}
}

/**
 * Queries the status of the currently associated wave instance.
 *
 * @return	true if the wave instance/ source has finished playback and false if it is 
 *			currently playing or paused.
 */
bool FCoreAudioSoundSource::IsFinished( void )
{
	// A paused source is not finished.
	if( Paused )
	{
		return( false );
	}
	
	if( WaveInstance )
	{
		FScopeLock Lock(&CriticalSection);

		// If not rendering, we're either at the end of a sound, or starved
		// and we are expecting the sound to be finishing
		if (NumActiveBuffers == 0 && (bBuffersToFlush || !bStreamedSound))
		{
			// ... notify the wave instance that it has finished playing.
			WaveInstance->NotifyFinished();
			return true;
		}

		// Service any real time sounds
		if( bStreamedSound && !bBuffersToFlush && NumActiveBuffers < 3)
		{
			// Continue feeding new sound data (unless we are waiting for the sound to finish)
			HandleRealTimeSource(NumActiveBuffers < 2);
		}

		return( false );
	}

	return( true );
}

OSStatus FCoreAudioSoundSource::CreateAudioUnit( OSType Type, OSType SubType, OSType Manufacturer, AudioStreamBasicDescription* InputFormat, AudioStreamBasicDescription* OutputFormat, AUNode* OutNode, AudioUnit* OutUnit )
{
	AudioComponentDescription Desc;
	Desc.componentFlags = 0;
	Desc.componentFlagsMask = 0;
	Desc.componentType = Type;
	Desc.componentSubType = SubType;
	Desc.componentManufacturer = Manufacturer;

	OSStatus Status = AUGraphAddNode( AudioDevice->GetAudioUnitGraph(), &Desc, OutNode );
	if( Status == noErr )
	{
		Status = AUGraphNodeInfo( AudioDevice->GetAudioUnitGraph(), *OutNode, NULL, OutUnit );
	}

	if( Status == noErr )
	{
		if( InputFormat )
		{
			Status = AudioUnitSetProperty( *OutUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, InputFormat, sizeof( AudioStreamBasicDescription ) );
		}
		if( Status == noErr )
		{
			if( OutputFormat )
			{
				Status = AudioUnitSetProperty( *OutUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, 0, OutputFormat, sizeof( AudioStreamBasicDescription ) );
			}
		}
	}

	return Status;
}

OSStatus FCoreAudioSoundSource::ConnectAudioUnit( AUNode DestNode, uint32 DestInputNumber, AUNode OutNode, AudioUnit OutUnit )
{
	OSStatus Status = AudioUnitInitialize( OutUnit );
	if( Status == noErr )
	{
		Status = AUGraphConnectNodeInput( AudioDevice->GetAudioUnitGraph(), OutNode, 0, DestNode, DestInputNumber );
	}

	return Status;
}

OSStatus FCoreAudioSoundSource::CreateAndConnectAudioUnit( OSType Type, OSType SubType, OSType Manufacturer, AUNode DestNode, uint32 DestInputNumber, AudioStreamBasicDescription* InputFormat, AudioStreamBasicDescription* OutputFormat, AUNode* OutNode, AudioUnit* OutUnit )
{
	OSStatus Status = CreateAudioUnit( Type, SubType, Manufacturer, InputFormat, OutputFormat, OutNode, OutUnit );

	if( Status == noErr )
	{
		Status = ConnectAudioUnit( DestNode, DestInputNumber, *OutNode, *OutUnit );
	}
	
	return Status;
}

void FCoreAudioSoundSource::InitSourceUnit(AudioStreamBasicDescription* StreamFormat, AUNode& HeadNode)
{
	AudioComponentDescription Desc = {kAudioUnitType_FormatConverter, kAudioUnitSubType_AUConverter, kAudioUnitManufacturer_Apple, 0, 0};
	OSStatus ErrorStatus = AUGraphAddNode(AudioDevice->GetAudioUnitGraph(), &Desc, &SourceNode);
	check(ErrorStatus == noErr);

	ErrorStatus = AUGraphNodeInfo(AudioDevice->GetAudioUnitGraph(), SourceNode, nullptr, &SourceUnit);
	check(ErrorStatus == noErr);

	ErrorStatus = AudioUnitSetProperty(SourceUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, StreamFormat, sizeof(AudioStreamBasicDescription));
	check(ErrorStatus == noErr);

	ErrorStatus = AudioUnitSetProperty(SourceUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, 0, StreamFormat, sizeof(AudioStreamBasicDescription));
	check(ErrorStatus == noErr);

	// Setup the callback which feeds audio to the source audio unit
	AURenderCallbackStruct Input;
	Input.inputProc = &CoreAudioRenderCallback;
	Input.inputProcRefCon = this;
	SAFE_CA_CALL( AudioUnitSetProperty(SourceUnit, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, 0, &Input, sizeof(Input)));

	HeadNode = SourceNode;
}

void FCoreAudioSoundSource::InitLowPassEffect(AudioStreamBasicDescription* StreamFormat, AUNode& HeadNode)
{
	AudioComponentDescription Desc = {kAudioUnitType_Effect, kAudioUnitSubType_LowPassFilter, kAudioUnitManufacturer_Apple, 0, 0};
	OSStatus ErrorStatus = AUGraphAddNode(AudioDevice->GetAudioUnitGraph(), &Desc, &LowPassNode);
	check(ErrorStatus == noErr);

	ErrorStatus = AUGraphNodeInfo(AudioDevice->GetAudioUnitGraph(), LowPassNode, nullptr, &LowPassUnit);
	check(ErrorStatus == noErr);

	ErrorStatus = AudioUnitSetProperty(LowPassUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, StreamFormat, sizeof(AudioStreamBasicDescription));
	check(ErrorStatus == noErr);

	ErrorStatus = AudioUnitSetProperty(LowPassUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, 0, StreamFormat, sizeof(AudioStreamBasicDescription));
	check(ErrorStatus == noErr);

	// Set the cutoff frequency to be the nyquist at first
	float CutoffFreq = AudioDevice->SampleRate * 0.5f;
	ErrorStatus = AudioUnitSetParameter(LowPassUnit, kLowPassParam_CutoffFrequency,	kAudioUnitScope_Global, 0, CutoffFreq, 0);
	check(ErrorStatus == noErr);

	ErrorStatus = AudioUnitInitialize(LowPassUnit);
	check(ErrorStatus == noErr);

	// Connect the current head node to the radio node (i.e. source -> radio effect)
	ErrorStatus = AUGraphConnectNodeInput(AudioDevice->GetAudioUnitGraph(), HeadNode, 0, LowPassNode, 0);
	check(ErrorStatus == noErr);

	// The radio node becomes the head node
	HeadNode = LowPassNode;
}

void FCoreAudioSoundSource::InitRadioSourceEffect(AudioStreamBasicDescription* StreamFormat, AUNode& HeadNode)
{
	AudioComponentDescription Desc = {kAudioUnitType_Effect, 'Rdio', 'Epic', 0, 0};
	OSStatus ErrorStatus = AUGraphAddNode(AudioDevice->GetAudioUnitGraph(), &Desc, &RadioNode);
	check(ErrorStatus == noErr);

	ErrorStatus = AUGraphNodeInfo(AudioDevice->GetAudioUnitGraph(), RadioNode, nullptr, &RadioUnit);
	check(ErrorStatus == noErr);

	ErrorStatus = AudioUnitSetProperty(RadioUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, StreamFormat, sizeof(AudioStreamBasicDescription));
	check(ErrorStatus == noErr);

	ErrorStatus = AudioUnitSetProperty(RadioUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, 0, StreamFormat, sizeof(AudioStreamBasicDescription));
	check(ErrorStatus == noErr);

	ErrorStatus = AudioUnitInitialize(RadioUnit);
	check(ErrorStatus == noErr);

	// Connect the current head node to the radio node (i.e. source -> radio effect)
	ErrorStatus = AUGraphConnectNodeInput(AudioDevice->GetAudioUnitGraph(), HeadNode, 0, RadioNode, 0);
	check(ErrorStatus == noErr);

	// The radio node becomes the head node
	HeadNode = RadioNode;
}

void FCoreAudioSoundSource::InitEqSourceEffect(AudioStreamBasicDescription* StreamFormat, AUNode& HeadNode)
{
	AudioComponentDescription Desc = { kAudioUnitType_Effect, kAudioUnitSubType_NBandEQ, kAudioUnitManufacturer_Apple, 0, 0 };

	OSStatus ErrorStatus = AUGraphAddNode(AudioDevice->GetAudioUnitGraph(), &Desc, &EQNode);
	check(ErrorStatus == noErr);

	ErrorStatus = AUGraphNodeInfo(AudioDevice->GetAudioUnitGraph(), EQNode, nullptr, &EQUnit);
	check(ErrorStatus == noErr);

	UInt32 NumBands = 4;
	ErrorStatus = AudioUnitSetProperty(EQUnit, kAUNBandEQProperty_NumberOfBands, kAudioUnitScope_Global, 0, &NumBands, sizeof(NumBands));
	check(ErrorStatus == noErr);

	ErrorStatus = AudioUnitSetProperty(EQUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, StreamFormat, sizeof(AudioStreamBasicDescription));
	check(ErrorStatus == noErr);

	ErrorStatus = AudioUnitSetProperty(EQUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, 0, StreamFormat, sizeof(AudioStreamBasicDescription));
	check(ErrorStatus == noErr);

	for (int32 Band = 0; Band < 4; ++Band)
	{
		// Now set the filter types for each band
		ErrorStatus = AudioUnitSetParameter(EQUnit, kAUNBandEQParam_FilterType + Band, kAudioUnitScope_Global, 0, kAUNBandEQFilterType_Parametric, 0);
		check(ErrorStatus == noErr);

		// Now make sure the bands are not bypassed
		ErrorStatus = AudioUnitSetParameter(EQUnit, kAUNBandEQParam_BypassBand + Band, kAudioUnitScope_Global, 0, 0, 0);
		check(ErrorStatus == noErr);
	}

	ErrorStatus = AudioUnitInitialize(EQUnit);
	check(ErrorStatus == noErr);

	// Connect the current head node to the radio node (i.e. head -> eq effect)
	ErrorStatus = AUGraphConnectNodeInput(AudioDevice->GetAudioUnitGraph(), HeadNode, 0, EQNode, 0);
	check(ErrorStatus == noErr);

	// The radio node becomes the head node
	HeadNode = EQNode;
}

void FCoreAudioSoundSource::InitReverbSourceEffect(AudioStreamBasicDescription* StreamFormat, AUNode& HeadNode)
{
	AudioComponentDescription Desc = {kAudioUnitType_Effect, kAudioUnitSubType_MatrixReverb, kAudioUnitManufacturer_Apple, 0, 0};
	OSStatus ErrorStatus = AUGraphAddNode(AudioDevice->GetAudioUnitGraph(), &Desc, &ReverbNode);
	check(ErrorStatus == noErr);

	ErrorStatus = AUGraphNodeInfo(AudioDevice->GetAudioUnitGraph(), ReverbNode, nullptr, &ReverbUnit);
	check(ErrorStatus == noErr);

	ErrorStatus = AudioUnitSetProperty(ReverbUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, StreamFormat, sizeof(AudioStreamBasicDescription));
	check(ErrorStatus == noErr);

	ErrorStatus = AudioUnitSetProperty(ReverbUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, 0, StreamFormat, sizeof(AudioStreamBasicDescription));
	check(ErrorStatus == noErr);

	ErrorStatus = AudioUnitInitialize(ReverbUnit);
	check(ErrorStatus == noErr);

	// Connect the current head node to the input of the reverb node (i.e. head -> reverb effect)
	ErrorStatus = AUGraphConnectNodeInput(AudioDevice->GetAudioUnitGraph(), HeadNode, 0, ReverbNode, 0);
	check(ErrorStatus == noErr);

	// The radio node becomes the head node
	HeadNode = ReverbNode;
}


bool FCoreAudioSoundSource::AttachToAUGraph()
{
	// We should usually have a non-zero AudioChannel here, but this can happen when unpausing a sound
	if (AudioChannel == 0)
	{
		AudioChannel = AudioDevice->FindFreeAudioChannel();
		if (AudioChannel == 0)
		{
			return false;
		}
	}

	check(MixerInputNumber != -1);

	OSStatus ErrorStatus = noErr;
	AUNode HeadNode = -1;
	AUNode FinalNode = -1;
	AudioStreamBasicDescription* StreamFormat = NULL;
	
	if (CoreAudioBuffer->NumChannels < 3)
	{
		ErrorStatus = AudioConverterNew( &CoreAudioBuffer->PCMFormat, &AudioDevice->Mixer3DFormat, &CoreAudioConverter );
		FinalNode = AudioDevice->GetMixer3DNode();

		uint32 SpatialSetting = ( CoreAudioBuffer->NumChannels == 1 ) ? kSpatializationAlgorithm_SoundField : kSpatializationAlgorithm_StereoPassThrough;
		ErrorStatus = AudioUnitSetProperty( AudioDevice->GetMixer3DUnit(), kAudioUnitProperty_SpatializationAlgorithm, kAudioUnitScope_Input, MixerInputNumber, &SpatialSetting, sizeof( SpatialSetting ) );
		check(ErrorStatus == noErr);

		ErrorStatus = AudioUnitSetParameter( AudioDevice->GetMixer3DUnit(), k3DMixerParam_Distance, kAudioUnitScope_Input, MixerInputNumber, 1.0f, 0 );
		check(ErrorStatus == noErr);

		StreamFormat = &AudioDevice->Mixer3DFormat;
	}
	else
	{	
		FinalNode = AudioDevice->GetMatrixMixerNode();
		StreamFormat = &AudioDevice->MatrixMixerInputFormat;
		
		ErrorStatus = AudioConverterNew( &CoreAudioBuffer->PCMFormat, &AudioDevice->MatrixMixerInputFormat, &CoreAudioConverter );
		check(ErrorStatus == noErr);

		bool bIs6ChannelOGG = CoreAudioBuffer->NumChannels == 6
							&& ((CoreAudioBuffer->DecompressionState && CoreAudioBuffer->DecompressionState->UsesVorbisChannelOrdering())
							|| WaveInstance->WaveData->bDecompressedFromOgg);

		AudioDevice->SetupMatrixMixerInput( MixerInputNumber, bIs6ChannelOGG );
	}

	// Initiliaze the "source" node, the node that is generating audio
	// This node becomes the "head" node
	InitSourceUnit(StreamFormat, HeadNode);

	// Figure out what filters are needed

	bool bNeedEQFilter = false;
	bool bNeedRadioFilter = false;
	bool bNeedReverbFilter = false;

#if CORE_AUDIO_EQ_ENABLED
	bNeedEQFilter = IsEQFilterApplied();
#endif

#if CORE_AUDIO_RADIO_ENABLED
	bNeedRadioFilter = Effects->bRadioAvailable && WaveInstance->bApplyRadioFilter;
#endif

#if CORE_AUDIO_REVERB_ENABLED
	bNeedReverbFilter = IsReverbApplied();
#endif

#if CORE_AUDIO_LOWPASS_ENABLED
	InitLowPassEffect(StreamFormat, HeadNode);
#endif

	// Radio filter will always go first
	if (bNeedRadioFilter)
	{
		InitRadioSourceEffect(StreamFormat, HeadNode);
	}

	if (bNeedEQFilter)
	{
		InitEqSourceEffect(StreamFormat, HeadNode);
	}

	// Reverb filter always goes last
	if (bNeedReverbFilter)
	{
		InitReverbSourceEffect(StreamFormat, HeadNode);
	}

	// Now connect the head node to the final output node
	// Connect the current head node to the radio node (i.e. source -> radio effect)
	ErrorStatus = AUGraphConnectNodeInput(AudioDevice->GetAudioUnitGraph(), HeadNode, 0, FinalNode, MixerInputNumber);
	check(ErrorStatus == noErr);

	if( ErrorStatus == noErr )
	{
		if (AudioDevice->SourcesDetached.Contains(this))
		{
			AudioDevice->UpdateAUGraph();
		}
		AudioDevice->SourcesAttached.Add(this);
		AudioDevice->bNeedsUpdate = true;
		AudioDevice->AudioChannels[AudioChannel] = this;
	}
	return ErrorStatus == noErr;
}

bool FCoreAudioSoundSource::DetachFromAUGraph()
{
	check(AudioChannel != 0);
	check(MixerInputNumber != -1);

	if (SourceUnit)
	{
		AURenderCallbackStruct Input;
		Input.inputProc = NULL;
		Input.inputProcRefCon = NULL;
		SAFE_CA_CALL( AudioUnitSetProperty( SourceUnit, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, 0, &Input, sizeof( Input ) ) );
	}

// Make sure we still have null nodes
#if !CORE_AUDIO_RADIO_ENABLED
	check(RadioNode == 0);
#endif

#if !CORE_AUDIO_REVERB_ENABLED
	check(ReverbNode == 0);
#endif

#if !CORE_AUDIO_EQ_ENABLED
	check(EQNode == 0);
#endif

#if !CORE_AUDIO_LOWPASS_ENABLED
	check(LowPassNode == 0);
#endif

	if (ReverbNode)
	{
		SAFE_CA_CALL( AUGraphDisconnectNodeInput( AudioDevice->GetAudioUnitGraph(), ReverbNode, 0 ) );
	}
	if (RadioNode)
	{
		SAFE_CA_CALL( AUGraphDisconnectNodeInput( AudioDevice->GetAudioUnitGraph(), RadioNode, 0 ) );
	}
	if (EQNode)
	{
		SAFE_CA_CALL( AUGraphDisconnectNodeInput( AudioDevice->GetAudioUnitGraph(), EQNode, 0 ) );
	}
	if (LowPassNode)
	{
		SAFE_CA_CALL( AUGraphDisconnectNodeInput( AudioDevice->GetAudioUnitGraph(), LowPassNode, 0 ) );
	}

	if( AudioChannel )
	{
		if( CoreAudioBuffer->NumChannels < 3 )
		{
			SAFE_CA_CALL( AUGraphDisconnectNodeInput( AudioDevice->GetAudioUnitGraph(), AudioDevice->GetMixer3DNode(), MixerInputNumber ) );
			AudioDevice->SetFreeMixer3DInput( MixerInputNumber );
		}
		else
		{
			SAFE_CA_CALL( AUGraphDisconnectNodeInput( AudioDevice->GetAudioUnitGraph(), AudioDevice->GetMatrixMixerNode(), MixerInputNumber ) );
			AudioDevice->SetFreeMatrixMixerInput( MixerInputNumber );
		}
	}

	if (LowPassNode)
	{
		SAFE_CA_CALL( AUGraphRemoveNode( AudioDevice->GetAudioUnitGraph(), LowPassNode ) );
	}
	if( EQNode )
	{
		SAFE_CA_CALL( AUGraphRemoveNode( AudioDevice->GetAudioUnitGraph(), EQNode ) );
	}
	if( RadioNode )
	{
		SAFE_CA_CALL( AUGraphRemoveNode( AudioDevice->GetAudioUnitGraph(), RadioNode ) );
	}
	if( ReverbNode )
	{
		SAFE_CA_CALL( AUGraphRemoveNode( AudioDevice->GetAudioUnitGraph(), ReverbNode ) );
	}
	if( SourceNode )
	{
		SAFE_CA_CALL( AUGraphRemoveNode( AudioDevice->GetAudioUnitGraph(), SourceNode ) );
	}

	if (AudioDevice->SourcesAttached.Contains(this))
	{
		AudioDevice->UpdateAUGraph();
	}
	AudioDevice->SourcesDetached.Add(this);
	AudioDevice->ConvertersToDispose.Add(CoreAudioConverter);
	AudioDevice->bNeedsUpdate = true;
	
	CoreAudioConverter = NULL;

	LowPassNode = 0;
	LowPassUnit = nullptr;
	EQNode = 0;
	EQUnit = nullptr;
	RadioNode = 0;
	RadioUnit = nullptr;
	ReverbNode = 0;
	ReverbUnit = nullptr;
	SourceNode = 0;
	SourceUnit = nullptr;
	MixerInputNumber = -1;

	AudioDevice->AudioChannels[AudioChannel] = nullptr;
	AudioChannel = 0;
	
	return true;
}


OSStatus FCoreAudioSoundSource::CoreAudioRenderCallback( void *InRefCon, AudioUnitRenderActionFlags *IOActionFlags,
														const AudioTimeStamp *InTimeStamp, UInt32 InBusNumber,
														UInt32 InNumberFrames, AudioBufferList *IOData )
{
	OSStatus Status = noErr;
	FCoreAudioSoundSource *Source = ( FCoreAudioSoundSource *)InRefCon;
	FScopeLock Lock(&Source->CriticalSection);

	uint32 DataByteSize = InNumberFrames * sizeof( Float32 );
	uint32 PacketsRequested = InNumberFrames;
	uint32 PacketsObtained = 0;

	// AudioBufferList itself holds only one buffer, while AudioConverterFillComplexBuffer expects a couple of them
	struct
	{
		AudioBufferList BufferList;		
		AudioBuffer		AdditionalBuffers[5];
	} LocalBuffers;

	AudioBufferList *LocalBufferList = &LocalBuffers.BufferList;
	LocalBufferList->mNumberBuffers = IOData->mNumberBuffers;

	if( Source->CoreAudioBuffer && Source->Playing )
	{
		while( PacketsObtained < PacketsRequested )
		{
			int32 BufferFilledBytes = PacketsObtained * sizeof( Float32 );
			for( uint32 Index = 0; Index < LocalBufferList->mNumberBuffers; Index++ )
			{
				LocalBufferList->mBuffers[Index].mDataByteSize = DataByteSize - BufferFilledBytes;
				LocalBufferList->mBuffers[Index].mData = ( uint8 *)IOData->mBuffers[Index].mData + BufferFilledBytes;
			}

			uint32 PacketCount = PacketsRequested - PacketsObtained;
			Status = AudioConverterFillComplexBuffer( Source->CoreAudioConverter, &CoreAudioConvertCallback, InRefCon, &PacketCount, LocalBufferList, NULL );
			PacketsObtained += PacketCount;

			if( PacketCount == 0 || Status != noErr )
			{
				AudioConverterReset( Source->CoreAudioConverter );
				break;
			}
		}

		if( PacketsObtained == 0 )
		{
			*IOActionFlags |= kAudioUnitRenderAction_OutputIsSilence;
		}
	}
	else
	{
		*IOActionFlags |= kAudioUnitRenderAction_OutputIsSilence;
	}

	if( PacketsObtained < PacketsRequested )
	{
		// Fill the rest of buffers provided with zeroes
		int32 BufferFilledBytes = PacketsObtained * sizeof( Float32 );
		for( uint32 Index = 0; Index < IOData->mNumberBuffers; ++Index )
		{
			FMemory::Memzero( ( uint8 *)IOData->mBuffers[Index].mData + BufferFilledBytes, DataByteSize - BufferFilledBytes );
		}
	}
	
	return Status;
}

OSStatus FCoreAudioSoundSource::CoreAudioConvertCallback( AudioConverterRef Converter, UInt32 *IONumberDataPackets, AudioBufferList *IOData,
														 AudioStreamPacketDescription **OutPacketDescription, void *InUserData )
{
	FCoreAudioSoundSource *Source = ( FCoreAudioSoundSource *)InUserData;
	FScopeLock Lock(&Source->CriticalSection);

	uint8 *Buffer = Source->CoreAudioBuffers[Source->BufferInUse].AudioData;
	int32 BufferSize = Source->CoreAudioBuffers[Source->BufferInUse].AudioDataSize;
	int32 ReadCursor = Source->CoreAudioBuffers[Source->BufferInUse].ReadCursor;

	int32 PacketsAvailable = Source->CoreAudioBuffer ? ( BufferSize - ReadCursor ) / Source->CoreAudioBuffer->PCMFormat.mBytesPerPacket : 0;
	if( PacketsAvailable < *IONumberDataPackets )
	{
		*IONumberDataPackets = PacketsAvailable;
	}

	IOData->mBuffers[0].mData = *IONumberDataPackets ? Buffer + ReadCursor : NULL;
	IOData->mBuffers[0].mDataByteSize = Source->CoreAudioBuffer ? *IONumberDataPackets * Source->CoreAudioBuffer->PCMFormat.mBytesPerPacket : 0;
	ReadCursor += IOData->mBuffers[0].mDataByteSize;
	Source->CoreAudioBuffers[Source->BufferInUse].ReadCursor = ReadCursor;

	if( ReadCursor == BufferSize && Source->NumActiveBuffers )
	{
		if( Source->bStreamedSound )
		{
			Source->NumActiveBuffers--;
			if (++Source->BufferInUse > 2)
			{
				Source->BufferInUse = 0;
			}
		}
		else if( Source->WaveInstance )
		{
			switch( Source->WaveInstance->LoopingMode )
			{
			case LOOP_Never:
				Source->NumActiveBuffers--;
				break;

			case LOOP_WithNotification:
				Source->WaveInstance->NotifyFinished();
				// pass-through

			case LOOP_Forever:
				Source->CoreAudioBuffers[Source->BufferInUse].ReadCursor = 0;	// loop to start
				break;
			}
		}
	}

	return noErr;
}
