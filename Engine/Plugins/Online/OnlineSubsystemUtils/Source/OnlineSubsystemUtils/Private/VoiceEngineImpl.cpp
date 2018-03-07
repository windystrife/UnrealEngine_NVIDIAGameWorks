// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "VoiceEngineImpl.h"
#include "Components/AudioComponent.h"
#include "VoiceModule.h"
#include "Voice.h"

#include "Sound/SoundWaveProcedural.h"
#include "OnlineSubsystemUtils.h"

/** Largest size preallocated for compressed data */
#define MAX_COMPRESSED_VOICE_BUFFER_SIZE 8 * 1024
/** Largest size preallocated for uncompressed data */
#define MAX_UNCOMPRESSED_VOICE_BUFFER_SIZE 22 * 1024
/** Largest size allowed to carry over into next buffer */
#define MAX_VOICE_REMAINDER_SIZE 1 * 1024

FRemoteTalkerDataImpl::FRemoteTalkerDataImpl() :
	MaxUncompressedDataSize(0),
	MaxUncompressedDataQueueSize(0),
	CurrentUncompressedDataQueueSize(0),
	LastSeen(0.0),
	NumFramesStarved(0),
	AudioComponent(nullptr),
	VoiceDecoder(nullptr)
{
	int32 SampleRate = DEFAULT_VOICE_SAMPLE_RATE;
	int32 NumChannels = DEFAULT_NUM_VOICE_CHANNELS;
	VoiceDecoder = FVoiceModule::Get().CreateVoiceDecoder(SampleRate, NumChannels);
	check(VoiceDecoder.IsValid());

	// Approx 1 sec worth of data
	MaxUncompressedDataSize = NumChannels * SampleRate * sizeof(uint16);
	MaxUncompressedDataQueueSize = MaxUncompressedDataSize * 5;
	{
		FScopeLock ScopeLock(&QueueLock);
		UncompressedDataQueue.Empty(MaxUncompressedDataQueueSize);
	}
}

FRemoteTalkerDataImpl::FRemoteTalkerDataImpl(FRemoteTalkerDataImpl&& Other)
{
	LastSeen = Other.LastSeen;
	Other.LastSeen = 0.0;

	NumFramesStarved = Other.NumFramesStarved;
	Other.NumFramesStarved = 0;

	AudioComponent = Other.AudioComponent;
	Other.AudioComponent = nullptr;

	VoiceDecoder = MoveTemp(Other.VoiceDecoder);
	Other.VoiceDecoder = nullptr;

	MaxUncompressedDataSize = Other.MaxUncompressedDataSize;
	Other.MaxUncompressedDataSize = 0;

	MaxUncompressedDataQueueSize = Other.MaxUncompressedDataQueueSize;
	Other.MaxUncompressedDataQueueSize = 0;

	CurrentUncompressedDataQueueSize = Other.CurrentUncompressedDataQueueSize;
	Other.CurrentUncompressedDataQueueSize = 0;

	{
		FScopeLock ScopeLock(&Other.QueueLock);
		UncompressedDataQueue = MoveTemp(Other.UncompressedDataQueue);
	}
}

FRemoteTalkerDataImpl::~FRemoteTalkerDataImpl()
{
	VoiceDecoder = nullptr;

	CurrentUncompressedDataQueueSize = 0;

	{
		FScopeLock ScopeLock(&QueueLock);
		UncompressedDataQueue.Empty();
	}
}

void FRemoteTalkerDataImpl::Reset()
{
	// Set to large number so TickTalkers doesn't come in here
	LastSeen = MAX_FLT;
	NumFramesStarved = 0;

	if (AudioComponent && !AudioComponent->IsPendingKill())
	{
		AudioComponent->Stop();
	}

	CurrentUncompressedDataQueueSize = 0;

	{
		FScopeLock ScopeLock(&QueueLock);
		UncompressedDataQueue.Empty();
	}
}

void FRemoteTalkerDataImpl::Cleanup()
{
	if (AudioComponent && !AudioComponent->IsPendingKill())
	{
		AudioComponent->Stop();
		USoundWaveProcedural* SoundStreaming = CastChecked<USoundWaveProcedural>(AudioComponent->Sound);
		if (SoundStreaming)
		{
			SoundStreaming->OnSoundWaveProceduralUnderflow.Unbind();
			AudioComponent->Sound = nullptr;
		}
	}

	AudioComponent = nullptr;
}

FVoiceEngineImpl::FVoiceEngineImpl(IOnlineSubsystem* InSubsystem) :
	OnlineSubsystem(InSubsystem),
	VoiceCapture(nullptr),
	VoiceEncoder(nullptr),
	OwningUserIndex(INVALID_INDEX),
	UncompressedBytesAvailable(0),
	CompressedBytesAvailable(0),
	AvailableVoiceResult(EVoiceCaptureState::UnInitialized),
	bPendingFinalCapture(false),
	bIsCapturing(false),
	SerializeHelper(nullptr)
{
}

FVoiceEngineImpl::~FVoiceEngineImpl()
{
	if (bIsCapturing)
	{
		VoiceCapture->Stop();
	}

	VoiceCapture = nullptr;
	VoiceEncoder = nullptr;

	delete SerializeHelper;
}

void FVoiceEngineImpl::VoiceCaptureUpdate() const
{
	if (bPendingFinalCapture)
	{
		uint32 CompressedSize;
		const EVoiceCaptureState::Type RecordingState = VoiceCapture->GetCaptureState(CompressedSize);

		// If no data is available, we have finished capture the last (post-StopRecording) half-second of voice data
		if (RecordingState == EVoiceCaptureState::NotCapturing)
		{
			UE_LOG(LogVoiceEngine, Log, TEXT("Internal voice capture complete."));

			bPendingFinalCapture = false;

			// If a new recording session has begun since the call to 'StopRecording', kick that off
			if (bIsCapturing)
			{
				StartRecording();
			}
			else
			{
				// Marks that recording has successfully stopped
				StoppedRecording();
			}
		}
	}
}

void FVoiceEngineImpl::StartRecording() const
{
	UE_LOG(LogVoiceEngine, VeryVerbose, TEXT("VOIP StartRecording"));
	if (VoiceCapture.IsValid())
	{
		if (!VoiceCapture->Start())
		{
			UE_LOG(LogVoiceEngine, Warning, TEXT("Failed to start voice recording"));
		}
	}
}

void FVoiceEngineImpl::StopRecording() const
{
	UE_LOG(LogVoiceEngine, VeryVerbose, TEXT("VOIP StopRecording"));
	if (VoiceCapture.IsValid())
	{
		VoiceCapture->Stop();
	}
}

void FVoiceEngineImpl::StoppedRecording() const
{
	UE_LOG(LogVoiceEngine, VeryVerbose, TEXT("VOIP StoppedRecording"));
}

bool FVoiceEngineImpl::Init(int32 MaxLocalTalkers, int32 MaxRemoteTalkers)
{
	bool bSuccess = false;

	if (!OnlineSubsystem->IsDedicated())
	{
		FVoiceModule& VoiceModule = FVoiceModule::Get();
		if (VoiceModule.IsVoiceEnabled())
		{
			VoiceCapture = VoiceModule.CreateVoiceCapture();
			VoiceEncoder = VoiceModule.CreateVoiceEncoder();

			bSuccess = VoiceCapture.IsValid() && VoiceEncoder.IsValid();
			if (bSuccess)
			{
				CompressedVoiceBuffer.Empty(MAX_COMPRESSED_VOICE_BUFFER_SIZE);
				DecompressedVoiceBuffer.Empty(MAX_UNCOMPRESSED_VOICE_BUFFER_SIZE);

				for (int32 TalkerIdx = 0; TalkerIdx < MaxLocalTalkers; TalkerIdx++)
				{
					PlayerVoiceData[TalkerIdx].VoiceRemainderSize = 0;
					PlayerVoiceData[TalkerIdx].VoiceRemainder.Empty(MAX_VOICE_REMAINDER_SIZE);
				}
			}
			else
			{
				UE_LOG(LogVoice, Warning, TEXT("Voice capture initialization failed!"));
			}
		}
		else
		{
			UE_LOG(LogVoice, Log, TEXT("Voice module disabled by config [Voice].bEnabled"));
		}
	}

	return bSuccess;
}

uint32 FVoiceEngineImpl::StartLocalVoiceProcessing(uint32 LocalUserNum) 
{
	uint32 Return = E_FAIL;
	if (IsOwningUser(LocalUserNum))
	{
		if (!bIsCapturing)
		{
			// Update the current recording state, if VOIP data was still being read
			VoiceCaptureUpdate();

			if (!IsRecording())
			{
				StartRecording();
			}

			bIsCapturing = true;
		}

		Return = S_OK;
	}
	else
	{
		UE_LOG(LogVoiceEngine, Error, TEXT("StartLocalVoiceProcessing(): Device is currently owned by another user"));
	}

	return Return;
}

uint32 FVoiceEngineImpl::StopLocalVoiceProcessing(uint32 LocalUserNum) 
{
	uint32 Return = E_FAIL;
	if (IsOwningUser(LocalUserNum))
	{
		if (bIsCapturing)
		{
			bIsCapturing = false;
			bPendingFinalCapture = true;

			// Make a call to begin stopping the current VOIP recording session
			StopRecording();

			// Now check/update the status of the recording session
			VoiceCaptureUpdate();
		}

		Return = S_OK;
	}
	else
	{
		UE_LOG(LogVoiceEngine, Error, TEXT("StopLocalVoiceProcessing: Ignoring stop request for non-owning user"));
	}

	return Return;
}

uint32 FVoiceEngineImpl::UnregisterRemoteTalker(const FUniqueNetId& UniqueId)
{
	const FUniqueNetIdString& RemoteTalkerId = (const FUniqueNetIdString&)UniqueId;
	FRemoteTalkerDataImpl* RemoteData = RemoteTalkerBuffers.Find(RemoteTalkerId);
	if (RemoteData != nullptr)
	{
		// Dump the whole talker
		RemoteData->Cleanup();
		RemoteTalkerBuffers.Remove(RemoteTalkerId);
	}

	return S_OK;
}

uint32 FVoiceEngineImpl::GetVoiceDataReadyFlags() const
{
	// First check and update the internal state of VOIP recording
	VoiceCaptureUpdate();
	if (OwningUserIndex != INVALID_INDEX && IsRecording())
	{
		// Check if there is new data available via the Voice API
		if (AvailableVoiceResult == EVoiceCaptureState::Ok && UncompressedBytesAvailable > 0)
		{
			return 1 << OwningUserIndex;
		}
	}

	return 0;
}

uint32 FVoiceEngineImpl::ReadLocalVoiceData(uint32 LocalUserNum, uint8* Data, uint32* Size)
{
	check(*Size > 0);

	// Before doing anything, check/update the current recording state
	VoiceCaptureUpdate();

	// Return data even if not capturing, possibly have data during stopping
	if (IsOwningUser(LocalUserNum) && IsRecording())
	{
		DecompressedVoiceBuffer.Empty(MAX_UNCOMPRESSED_VOICE_BUFFER_SIZE);
		CompressedVoiceBuffer.Empty(MAX_COMPRESSED_VOICE_BUFFER_SIZE);

		uint32 NewVoiceDataBytes = 0;
		EVoiceCaptureState::Type VoiceResult = VoiceCapture->GetCaptureState(NewVoiceDataBytes);
		if (VoiceResult != EVoiceCaptureState::Ok && VoiceResult != EVoiceCaptureState::NoData)
		{
			UE_LOG(LogVoiceEngine, Warning, TEXT("ReadLocalVoiceData: GetAvailableVoice failure: VoiceResult: %s"), EVoiceCaptureState::ToString(VoiceResult));
			return E_FAIL;
		}

		if (NewVoiceDataBytes == 0)
		{
			UE_LOG(LogVoiceEngine, VeryVerbose, TEXT("ReadLocalVoiceData: No Data: VoiceResult: %s"), EVoiceCaptureState::ToString(VoiceResult));
			*Size = 0;
			return S_OK;
		}

		// Make space for new and any previously remaining data
		uint32 TotalVoiceBytes = NewVoiceDataBytes + PlayerVoiceData[LocalUserNum].VoiceRemainderSize;
		if (TotalVoiceBytes > MAX_UNCOMPRESSED_VOICE_BUFFER_SIZE)
		{
			UE_LOG(LogVoiceEngine, Warning, TEXT("Exceeded uncompressed voice buffer size, clamping"))
			TotalVoiceBytes = MAX_UNCOMPRESSED_VOICE_BUFFER_SIZE;
		}

		DecompressedVoiceBuffer.AddUninitialized(TotalVoiceBytes);

		if (PlayerVoiceData[LocalUserNum].VoiceRemainderSize > 0)
		{
			FMemory::Memcpy(DecompressedVoiceBuffer.GetData(), PlayerVoiceData[LocalUserNum].VoiceRemainder.GetData(), PlayerVoiceData[LocalUserNum].VoiceRemainderSize);
		}

		// Get new uncompressed data
		VoiceResult = VoiceCapture->GetVoiceData(DecompressedVoiceBuffer.GetData() + PlayerVoiceData[LocalUserNum].VoiceRemainderSize,
			NewVoiceDataBytes, NewVoiceDataBytes);

		TotalVoiceBytes = NewVoiceDataBytes + PlayerVoiceData[LocalUserNum].VoiceRemainderSize;

		if (VoiceResult == EVoiceCaptureState::Ok && TotalVoiceBytes > 0)
		{
			CompressedBytesAvailable = MAX_COMPRESSED_VOICE_BUFFER_SIZE;
			CompressedVoiceBuffer.AddUninitialized(MAX_COMPRESSED_VOICE_BUFFER_SIZE);

			PlayerVoiceData[LocalUserNum].VoiceRemainderSize =
				VoiceEncoder->Encode(DecompressedVoiceBuffer.GetData(), TotalVoiceBytes, CompressedVoiceBuffer.GetData(), CompressedBytesAvailable);

			// Save off any unencoded remainder
			if (PlayerVoiceData[LocalUserNum].VoiceRemainderSize > 0)
			{
				if (PlayerVoiceData[LocalUserNum].VoiceRemainderSize > MAX_VOICE_REMAINDER_SIZE)
				{
					UE_LOG(LogVoiceEngine, Warning, TEXT("Exceeded voice remainder buffer size, clamping"))
						PlayerVoiceData[LocalUserNum].VoiceRemainderSize = MAX_VOICE_REMAINDER_SIZE;
				}

				PlayerVoiceData[LocalUserNum].VoiceRemainder.AddUninitialized(MAX_VOICE_REMAINDER_SIZE);
				FMemory::Memcpy(PlayerVoiceData[LocalUserNum].VoiceRemainder.GetData(), DecompressedVoiceBuffer.GetData() + (TotalVoiceBytes - PlayerVoiceData[LocalUserNum].VoiceRemainderSize), PlayerVoiceData[LocalUserNum].VoiceRemainderSize);
			}

			static double LastGetVoiceCallTime = 0.0;
			double CurTime = FPlatformTime::Seconds();
			double TimeSinceLastCall = (LastGetVoiceCallTime > 0) ? (CurTime - LastGetVoiceCallTime) : 0.0;
			LastGetVoiceCallTime = CurTime;

			UE_LOG(LogVoiceEngine, Log, TEXT("ReadLocalVoiceData: GetVoice: Result: %s, Available: %i, LastCall: %0.3f ms"), EVoiceCaptureState::ToString(VoiceResult), CompressedBytesAvailable, TimeSinceLastCall * 1000.0);
			if (CompressedBytesAvailable > 0)
			{
				*Size = FMath::Min<int32>(*Size, CompressedBytesAvailable);
				FMemory::Memcpy(Data, CompressedVoiceBuffer.GetData(), *Size);

				UE_LOG(LogVoiceEngine, VeryVerbose, TEXT("ReadLocalVoiceData: Size: %d"), *Size);
				return S_OK;
			}
			else
			{
				*Size = 0;
				CompressedVoiceBuffer.Empty(MAX_COMPRESSED_VOICE_BUFFER_SIZE);

				UE_LOG(LogVoiceEngine, Warning, TEXT("ReadLocalVoiceData: GetVoice failure: VoiceResult: %s"), EVoiceCaptureState::ToString(VoiceResult));
				return E_FAIL;
			}
		}
	}

	return E_FAIL;
}

uint32 FVoiceEngineImpl::SubmitRemoteVoiceData(const FUniqueNetId& RemoteTalkerId, uint8* Data, uint32* Size)
{
	UE_LOG(LogVoiceEngine, VeryVerbose, TEXT("SubmitRemoteVoiceData(%s) Size: %d received!"), *RemoteTalkerId.ToDebugString(), *Size);

	const FUniqueNetIdString& TalkerId = (const FUniqueNetIdString&)RemoteTalkerId;
	FRemoteTalkerDataImpl& QueuedData = RemoteTalkerBuffers.FindOrAdd(TalkerId);

	// new voice packet.
	QueuedData.LastSeen = FPlatformTime::Seconds();

	uint32 BytesWritten = MAX_UNCOMPRESSED_VOICE_BUFFER_SIZE;

	DecompressedVoiceBuffer.Empty(MAX_UNCOMPRESSED_VOICE_BUFFER_SIZE);
	DecompressedVoiceBuffer.AddUninitialized(MAX_UNCOMPRESSED_VOICE_BUFFER_SIZE);
	QueuedData.VoiceDecoder->Decode(Data, *Size, DecompressedVoiceBuffer.GetData(), BytesWritten);

	// If there is no data, return
	if (BytesWritten <= 0)
	{
		*Size = 0;
		return S_OK;
	}

	bool bAudioComponentCreated = false;
	// Generate a streaming wave audio component for voice playback
	if (QueuedData.AudioComponent == nullptr || QueuedData.AudioComponent->IsPendingKill())
	{
		if (SerializeHelper == nullptr)
		{
			SerializeHelper = new FVoiceSerializeHelper(this);
		}

		QueuedData.AudioComponent = CreateVoiceAudioComponent(DEFAULT_VOICE_SAMPLE_RATE, DEFAULT_NUM_VOICE_CHANNELS);
		if (QueuedData.AudioComponent)
		{
			QueuedData.AudioComponent->OnAudioFinishedNative.AddRaw(this, &FVoiceEngineImpl::OnAudioFinished);
			USoundWaveProcedural* SoundStreaming = CastChecked<USoundWaveProcedural>(QueuedData.AudioComponent->Sound);
			if (SoundStreaming)
			{
				// Bind the GenerateVoiceData callback with FOnSoundWaveProceduralUnderflow object
				SoundStreaming->OnSoundWaveProceduralUnderflow = FOnSoundWaveProceduralUnderflow::CreateRaw(this, &FVoiceEngineImpl::GenerateVoiceData, TalkerId);
			}
		}
	}
	
	if (QueuedData.AudioComponent != nullptr)
	{
		{
			FScopeLock ScopeLock(&QueuedData.QueueLock);

			const int32 OldSize = QueuedData.UncompressedDataQueue.Num();
			const int32 AmountToBuffer = (OldSize + (int32)BytesWritten);
			if (AmountToBuffer <= QueuedData.MaxUncompressedDataQueueSize)
			{
				QueuedData.UncompressedDataQueue.AddUninitialized(BytesWritten);
				FMemory::Memcpy(QueuedData.UncompressedDataQueue.GetData() + OldSize, DecompressedVoiceBuffer.GetData(), BytesWritten);
				QueuedData.CurrentUncompressedDataQueueSize += BytesWritten;
			}
			else
			{
				UE_LOG(LogVoiceEngine, Warning, TEXT("UncompressedDataQueue Overflow!"));
				QueuedData.UncompressedDataQueue.Empty(QueuedData.MaxUncompressedDataQueueSize);
				FMemory::Memcpy(QueuedData.UncompressedDataQueue.GetData(), DecompressedVoiceBuffer.GetData(), BytesWritten);
				QueuedData.CurrentUncompressedDataQueueSize = BytesWritten;
			}
		}

		// Wait for approx .5 sec worth of data before playing
		if (!QueuedData.AudioComponent->IsActive() && (QueuedData.CurrentUncompressedDataQueueSize > (QueuedData.MaxUncompressedDataSize / 2)))
		{
			UE_LOG(LogVoiceEngine, Log, TEXT("Playback started"));
			QueuedData.AudioComponent->Play();
		}
	}

	return S_OK;
}

void FVoiceEngineImpl::TickTalkers(float DeltaTime)
{
	// Remove users that are done talking.
	const double CurTime = FPlatformTime::Seconds();
	for (FRemoteTalkerData::TIterator It(RemoteTalkerBuffers); It; ++It)
	{
		FRemoteTalkerDataImpl& RemoteData = It.Value();
		double TimeSince = CurTime - RemoteData.LastSeen;
		if (TimeSince >= 1.0)
		{
			// Dump the whole talker
			RemoteData.Reset();
		}

#if !UE_BUILD_SHIPPING
		if (RemoteData.AudioComponent && RemoteData.AudioComponent->IsPlaying())
		{
			USoundWaveProcedural* SoundStreaming = CastChecked<USoundWaveProcedural>(RemoteData.AudioComponent->Sound);
			if (SoundStreaming)
			{
				RemoteData.NumFramesStarved = (SoundStreaming->GetAvailableAudioByteCount() == 0) ? (RemoteData.NumFramesStarved + 1) : 0;
				if (RemoteData.NumFramesStarved > 1)
				{
					int32 Data1, Data2;
					{
						FScopeLock ScopeLock(&RemoteData.QueueLock);
						Data1 = RemoteData.UncompressedDataQueue.Num();
						Data2 = RemoteData.CurrentUncompressedDataQueueSize;
					}
					UE_LOG(LogVoiceEngine, Log, TEXT("VOIP audio component starved %d frames! %d / %d"), RemoteData.NumFramesStarved, Data1, Data2);
				}
			}
		}
		else
		{
			RemoteData.NumFramesStarved = 0;
		}
#endif
	}
}

void FVoiceEngineImpl::Tick(float DeltaTime)
{
	// Check available voice once a frame, this value changes after calling GetVoiceData()
	AvailableVoiceResult = VoiceCapture->GetCaptureState(UncompressedBytesAvailable);

	TickTalkers(DeltaTime);
}

void FVoiceEngineImpl::GenerateVoiceData(USoundWaveProcedural* InProceduralWave, int32 SamplesRequired, FUniqueNetIdString TalkerId)
{
	FRemoteTalkerDataImpl* QueuedData = RemoteTalkerBuffers.Find(TalkerId);
	if (QueuedData)
	{
		const int32 SampleSize = sizeof(uint16) * DEFAULT_NUM_VOICE_CHANNELS;

		{
			FScopeLock ScopeLock(&QueuedData->QueueLock);
			QueuedData->CurrentUncompressedDataQueueSize = QueuedData->UncompressedDataQueue.Num();
			const int32 AvailableSamples = QueuedData->CurrentUncompressedDataQueueSize / SampleSize;
			if (AvailableSamples >= SamplesRequired)
			{
				UE_LOG(LogVoiceEngine, Verbose, TEXT("GenerateVoiceData %d / %d"), AvailableSamples, SamplesRequired);
				const int32 SamplesBytesTaken = AvailableSamples * SampleSize;
				InProceduralWave->QueueAudio(QueuedData->UncompressedDataQueue.GetData(), SamplesBytesTaken);
				QueuedData->UncompressedDataQueue.RemoveAt(0, SamplesBytesTaken, false);
				QueuedData->CurrentUncompressedDataQueueSize -= (SamplesBytesTaken);
			}
			else
			{
				UE_LOG(LogVoiceEngine, Verbose, TEXT("Voice underflow"));
			}
		}
	}
}

void FVoiceEngineImpl::OnAudioFinished(UAudioComponent* AC)
{
	for (FRemoteTalkerData::TIterator It(RemoteTalkerBuffers); It; ++It)
	{
		FRemoteTalkerDataImpl& RemoteData = It.Value();
		if (RemoteData.AudioComponent->IsPendingKill() || AC == RemoteData.AudioComponent)
		{
			UE_LOG(LogVoiceEngine, Log, TEXT("Removing VOIP AudioComponent for Id: %s"), *It.Key().ToDebugString());
			RemoteData.AudioComponent = nullptr;
			break;
		}
	}
	UE_LOG(LogVoiceEngine, Verbose, TEXT("Audio Finished"));
}

FString FVoiceEngineImpl::GetVoiceDebugState() const
{
	FString Output;
	Output = FString::Printf(TEXT("IsRecording: %d\n DataReady: 0x%08x State:%s\n UncompressedBytes: %d\n CompressedBytes: %d\n"),
		IsRecording(), 
		GetVoiceDataReadyFlags(),
		EVoiceCaptureState::ToString(AvailableVoiceResult),
		UncompressedBytesAvailable,
		CompressedBytesAvailable
		);

	// Add remainder size
	for (int32 Idx=0; Idx < MAX_SPLITSCREEN_TALKERS; Idx++)
	{
		Output += FString::Printf(TEXT("Remainder[%d] %d\n"), Idx, PlayerVoiceData[Idx].VoiceRemainderSize);
	}

	return Output;
}

bool FVoiceEngineImpl::Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
{
	bool bWasHandled = false;

	if (FParse::Command(&Cmd, TEXT("vcvbr")))
	{
		// vcvbr <true/false>
		FString VBRStr = FParse::Token(Cmd, false);
		int32 ShouldVBR = FPlatformString::Atoi(*VBRStr);
		bool bVBR = ShouldVBR != 0;
		if (VoiceEncoder.IsValid())
		{
			if (!VoiceEncoder->SetVBR(bVBR))
			{
				UE_LOG(LogVoice, Warning, TEXT("Failed to set VBR %d"), bVBR);
			}
		}

		bWasHandled = true;
	}
	else if (FParse::Command(&Cmd, TEXT("vcbitrate")))
	{
		// vcbitrate <bitrate>
		FString BitrateStr = FParse::Token(Cmd, false);
		int32 NewBitrate = !BitrateStr.IsEmpty() ? FPlatformString::Atoi(*BitrateStr) : 0;
		if (VoiceEncoder.IsValid() && NewBitrate > 0)
		{
			if (!VoiceEncoder->SetBitrate(NewBitrate))
			{
				UE_LOG(LogVoice, Warning, TEXT("Failed to set bitrate %d"), NewBitrate);
			}
		}

		bWasHandled = true;
	}
	else if (FParse::Command(&Cmd, TEXT("vccomplexity")))
	{
		// vccomplexity <complexity>
		FString ComplexityStr = FParse::Token(Cmd, false);
		int32 NewComplexity = !ComplexityStr.IsEmpty() ? FPlatformString::Atoi(*ComplexityStr) : -1;
		if (VoiceEncoder.IsValid() && NewComplexity >= 0)
		{
			if (!VoiceEncoder->SetComplexity(NewComplexity))
			{
				UE_LOG(LogVoice, Warning, TEXT("Failed to set complexity %d"), NewComplexity);
			}
		}

		bWasHandled = true;
	}
	else if (FParse::Command(&Cmd, TEXT("vcdump")))
	{
		if (VoiceCapture.IsValid())
		{
			VoiceCapture->DumpState();
		}

		if (VoiceEncoder.IsValid())
		{
			VoiceEncoder->DumpState();
		}

		for (FRemoteTalkerData::TIterator It(RemoteTalkerBuffers); It; ++It)
		{
			FRemoteTalkerDataImpl& RemoteData = It.Value();
			if (RemoteData.VoiceDecoder.IsValid())
			{
				RemoteData.VoiceDecoder->DumpState();
			}
		}

		bWasHandled = true;
	}

	return bWasHandled;
}


