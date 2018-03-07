// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "VoiceEngineSteam.h"
#include "Misc/ConfigCacheIni.h"
#include "Components/AudioComponent.h"
#include "OnlineSubsystemSteam.h"
#include "VoiceModule.h"
#include "SteamUtilities.h"

#include "Sound/SoundWaveProcedural.h"
#include "OnlineSubsystemUtils.h"

/** Largest size Steam says it will need to compress data */
#define MAX_COMPRESSED_VOICE_BUFFER_SIZE 8 * 1024
/** Largest size Steam says it will uncompress to */
#define MAX_UNCOMPRESSED_VOICE_BUFFER_SIZE 22 * 1024

FVoiceEngineSteam::FVoiceEngineSteam(FOnlineSubsystemSteam* InSteamSubsystem) :
	SteamSubsystem(InSteamSubsystem),
	OwningUserIndex(INVALID_INDEX),
	bPendingFinalCapture(false),
	bIsCapturing(false),
	SerializeHelper(NULL)
{
	SteamUserPtr = SteamUser();
	SteamFriendsPtr = SteamFriends();
}

FVoiceEngineSteam::~FVoiceEngineSteam()
{
	if (bIsCapturing)
	{
		SteamUserPtr->StopVoiceRecording();
		SteamFriendsPtr->SetInGameVoiceSpeaking(SteamUserPtr->GetSteamID(), false);
	}
	delete SerializeHelper;
}

void FVoiceEngineSteam::VoiceCaptureUpdate() const
{
	if (bPendingFinalCapture)
	{
		uint32 CompressedSize;
		const EVoiceResult RecordingState = SteamUserPtr->GetAvailableVoice(&CompressedSize, NULL, 0);

		// If no data is available, we have finished capture the last (post-StopRecording) half-second of voice data
		if (RecordingState == k_EVoiceResultNotRecording)
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

void FVoiceEngineSteam::StartRecording() const
{
	UE_LOG(LogVoiceEngine, VeryVerbose, TEXT("VOIP StartRecording"));
	if (SteamUserPtr)
	{
		SteamUserPtr->StartVoiceRecording();
		SteamFriendsPtr->SetInGameVoiceSpeaking(SteamUserPtr->GetSteamID(), true);
	}
}

void FVoiceEngineSteam::StopRecording() const
{
	UE_LOG(LogVoiceEngine, VeryVerbose, TEXT("VOIP StopRecording"));
	if (SteamUserPtr)
	{
		SteamUserPtr->StopVoiceRecording();
	}
}

void FVoiceEngineSteam::StoppedRecording() const
{
	UE_LOG(LogVoiceEngine, VeryVerbose, TEXT("VOIP StoppedRecording"));
	if (SteamFriendsPtr)
	{
		SteamFriendsPtr->SetInGameVoiceSpeaking(SteamUserPtr->GetSteamID(), false);
	}
}

bool FVoiceEngineSteam::Init(int32 MaxLocalTalkers, int32 MaxRemoteTalkers) 
{
	bool bSuccess = false;

	if (SteamSubsystem->IsSteamClientAvailable())
	{
		bool bHasVoiceEnabled = false;
		if (GConfig->GetBool(TEXT("OnlineSubsystem"),TEXT("bHasVoiceEnabled"), bHasVoiceEnabled, GEngineIni) && bHasVoiceEnabled)
		{
			if (SteamUserPtr && SteamFriendsPtr)
			{
				// Just verify voice capture is available
				uint32 CompressedBytes;
				SteamUserPtr->StartVoiceRecording();
				EVoiceResult VoiceResult = SteamUserPtr->GetAvailableVoice(&CompressedBytes, NULL, 0);
				SteamUserPtr->StopVoiceRecording();

				bSuccess = (VoiceResult != k_EVoiceResultNotInitialized);
				if (bSuccess)
				{
					CompressedVoiceBuffer.Empty(MAX_COMPRESSED_VOICE_BUFFER_SIZE);
					DecompressedVoiceBuffer.Empty(MAX_UNCOMPRESSED_VOICE_BUFFER_SIZE);
				}
				else
				{
					UE_LOG(LogVoice, Warning, TEXT("Steamworks voice initialization failed!"));
				}
			}
		}
		else
		{
			UE_LOG(LogVoice, Log, TEXT("Voice module disabled by config [OnlineSubsystem].bHasVoiceEnabled"));
		}
	}

	return bSuccess;
}

uint32 FVoiceEngineSteam::StartLocalVoiceProcessing(uint32 LocalUserNum) 
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

uint32 FVoiceEngineSteam::StopLocalVoiceProcessing(uint32 LocalUserNum) 
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

uint32 FVoiceEngineSteam::GetVoiceDataReadyFlags() const 
{
	// First check and update the internal state of VOIP recording
	VoiceCaptureUpdate();
	if (OwningUserIndex != INVALID_INDEX && IsRecording())
	{
		// Check if there is new data available via the Steam API
		if (AvailableVoiceResult == k_EVoiceResultOK && CompressedBytesAvailable > 0)
		{
			return 1 << OwningUserIndex;
		}
	}

	return 0;
}

uint32 FVoiceEngineSteam::ReadLocalVoiceData(uint32 LocalUserNum, uint8* Data, uint32* Size) 
{
	check(*Size > 0);

	// Before doing anything, check/update the current recording state
	VoiceCaptureUpdate();

	// Return data even if not capturing, if the final half-second of data from Steam is still pending
	if (IsOwningUser(LocalUserNum) && IsRecording())
	{
		CompressedVoiceBuffer.Empty(MAX_COMPRESSED_VOICE_BUFFER_SIZE);

		uint32 CompressedBytes = 0;
		EVoiceResult VoiceResult = SteamUserPtr->GetAvailableVoice(&CompressedBytes, NULL, 0);
		if (VoiceResult != k_EVoiceResultOK && VoiceResult != k_EVoiceResultNoData)
		{
			UE_LOG(LogVoiceEngine, Warning, TEXT("ReadLocalVoiceData: GetAvailableVoice failure: VoiceResult: %s"), *SteamVoiceResult(VoiceResult));
			return E_FAIL;
		}

		// This shouldn't happen, but just in case.
		if (CompressedBytes == 0)
		{
			UE_LOG(LogVoiceEngine, VeryVerbose, TEXT("ReadLocalVoiceData: No Data: VoiceResult: %s"), *SteamVoiceResult(VoiceResult));
			*Size = 0;
			return S_OK;
		}

		// Update the amount of data available for consumption (CompressedBytes can be more than AvailableWritten below)
		CompressedVoiceBuffer.AddUninitialized(CompressedBytes);

		uint32 AvailableWritten = 0;
		VoiceResult = SteamUserPtr->GetVoice(true, CompressedVoiceBuffer.GetData(), CompressedVoiceBuffer.Num(), &AvailableWritten, false, NULL, 0, NULL, 0);

		static double LastGetVoiceCallTime = 0.0;
		double CurTime = FPlatformTime::Seconds();
		double TimeSinceLastCall = (LastGetVoiceCallTime > 0) ? (CurTime - LastGetVoiceCallTime) : 0.0;
		LastGetVoiceCallTime = CurTime;

		UE_LOG(LogVoiceEngine, Log, TEXT("ReadLocalVoiceData: GetVoice: Result: %s, Available: %i, LastCall: %0.3f"), *SteamVoiceResult(VoiceResult), AvailableWritten, TimeSinceLastCall * 1000.0);
		if (VoiceResult != k_EVoiceResultOK)
		{
			UE_LOG(LogVoiceEngine, Warning, TEXT("ReadLocalVoiceData: GetVoice failure: VoiceResult: %s"), *SteamVoiceResult(VoiceResult));
			*Size = 0;
			CompressedVoiceBuffer.Empty(MAX_COMPRESSED_VOICE_BUFFER_SIZE);
			return E_FAIL;
		}

		if (AvailableWritten > 0)
		{
			*Size = FMath::Min<int32>(*Size, AvailableWritten);
			FMemory::Memcpy(Data, CompressedVoiceBuffer.GetData(), *Size);

			UE_LOG(LogVoiceEngine, VeryVerbose, TEXT("ReadLocalVoiceData: Size: %d"), *Size);
			return S_OK;
		}
		else
		{
			*Size = 0;
		}
	}

	return E_FAIL;
}

uint32 FVoiceEngineSteam::SubmitRemoteVoiceData(const FUniqueNetId& RemoteTalkerId, uint8* Data, uint32* Size) 
{
	UE_LOG(LogVoiceEngine, VeryVerbose, TEXT("SubmitRemoteVoiceData(%s) Size: %d received!"), *RemoteTalkerId.ToDebugString(), *Size);

	const FUniqueNetIdSteam& SteamId = (const FUniqueNetIdSteam&)RemoteTalkerId;
	FRemoteTalkerDataSteam* QueuedData = RemoteTalkerBuffers.Find(SteamId);

	if (QueuedData == NULL)
	{
		RemoteTalkerBuffers.Add(SteamId, FRemoteTalkerDataSteam());
		QueuedData = RemoteTalkerBuffers.Find(SteamId);
	}

	check(QueuedData);

	// new voice packet.
	QueuedData->LastSeen = FPlatformTime::Seconds();

	uint32 BytesWritten = 0;

	DecompressedVoiceBuffer.Empty(MAX_UNCOMPRESSED_VOICE_BUFFER_SIZE);
	DecompressedVoiceBuffer.AddUninitialized(MAX_UNCOMPRESSED_VOICE_BUFFER_SIZE);
	const EVoiceResult VoiceResult = SteamUserPtr->DecompressVoice(Data, *Size, DecompressedVoiceBuffer.GetData(),
		DecompressedVoiceBuffer.Num(), &BytesWritten, SteamUserPtr->GetVoiceOptimalSampleRate());

	if (VoiceResult != k_EVoiceResultOK)
	{
		UE_LOG(LogVoiceEngine, Warning, TEXT("SubmitRemoteVoiceData: DecompressVoice failure: VoiceResult: %s"), *SteamVoiceResult(VoiceResult));
		*Size = 0;
		return E_FAIL;
	}

	// If there is no data, return
	if (BytesWritten <= 0)
	{
		*Size = 0;
		return S_OK;
	}

	// Generate a streaming wave audio component for voice playback
	if (QueuedData->AudioComponent == NULL || QueuedData->AudioComponent->IsPendingKill())
	{
		if (SerializeHelper == NULL)
		{
			SerializeHelper = new FVoiceSerializeHelper(this);
		}

		QueuedData->AudioComponent = CreateVoiceAudioComponent(SteamUserPtr->GetVoiceOptimalSampleRate(), DEFAULT_NUM_VOICE_CHANNELS);
		if (QueuedData->AudioComponent)
		{
			QueuedData->AudioComponent->OnAudioFinishedNative.AddRaw(this, &FVoiceEngineSteam::OnAudioFinished);
			QueuedData->AudioComponent->Play();
		}
	}

	if (QueuedData->AudioComponent != NULL)
	{
		USoundWaveProcedural* SoundStreaming = CastChecked<USoundWaveProcedural>(QueuedData->AudioComponent->Sound);
		if (SoundStreaming->GetAvailableAudioByteCount() == 0)
		{
			UE_LOG(LogVoiceEngine, Log, TEXT("VOIP audio component was starved!"));
		}
		SoundStreaming->QueueAudio(DecompressedVoiceBuffer.GetData(), BytesWritten);
	}

	return S_OK;
}

void FVoiceEngineSteam::TickTalkers(float DeltaTime)
{
	// Remove users that are done talking.
	const double CurTime = FPlatformTime::Seconds();
	for (FRemoteTalkerData::TIterator It(RemoteTalkerBuffers); It; ++It)
	{
		FRemoteTalkerDataSteam& RemoteData = It.Value();
		double TimeSince = CurTime - RemoteData.LastSeen;
		if (TimeSince >= 5.0)
		{
			// Dump the whole talker
			if (RemoteData.AudioComponent)
			{
				RemoteData.AudioComponent->Stop();
				RemoteData.AudioComponent = NULL;
			}

			It.RemoveCurrent();
		}
	}
}

void FVoiceEngineSteam::Tick(float DeltaTime) 
{
	// Check available voice one a frame, this value changes after calling GetVoice()
	AvailableVoiceResult = SteamUserPtr->GetAvailableVoice(&CompressedBytesAvailable, NULL, 0);
	//UE_LOG(LogVoiceEngine, Log, TEXT("TickResult: %s"), *SteamVoiceResult(AvailableVoiceResult));
	TickTalkers(DeltaTime);
}

void FVoiceEngineSteam::OnAudioFinished(UAudioComponent* AC)
{
	for (FRemoteTalkerData::TIterator It(RemoteTalkerBuffers); It; ++It)
	{
		FRemoteTalkerDataSteam& RemoteData = It.Value();
		if (RemoteData.AudioComponent->IsPendingKill() || AC == RemoteData.AudioComponent)
		{
			UE_LOG(LogVoiceEngine, Log, TEXT("Removing VOIP AudioComponent for SteamId: %s"), *It.Key().ToDebugString());
			RemoteData.AudioComponent = NULL;
			break;
		}
	}
	UE_LOG(LogVoiceEngine, Verbose, TEXT("Audio Finished"));
}

FString FVoiceEngineSteam::GetVoiceDebugState() const 
{
	FString Output;
	Output = FString::Printf(TEXT("IsRecording: %d\n DataReady: 0x%08x State:%s\n BufferRemaining: %d\n"),
		IsRecording(), 
		GetVoiceDataReadyFlags(),
		*SteamVoiceResult(AvailableVoiceResult),
		CompressedVoiceBuffer.Num()
		);

	return Output;
}
