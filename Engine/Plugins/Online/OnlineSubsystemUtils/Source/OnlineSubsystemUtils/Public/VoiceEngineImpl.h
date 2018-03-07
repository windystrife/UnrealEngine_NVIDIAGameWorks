// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CoreMisc.h"
#include "UObject/GCObject.h"
#include "OnlineSubsystemTypes.h"
#include "Interfaces/VoiceInterface.h"
#include "Net/VoiceDataCommon.h"
#include "Interfaces/VoiceCapture.h"
#include "Interfaces/VoiceCodec.h"
#include "OnlineSubsystemBPCallHelper.h"
#include "OnlineSubsystemUtilsPackage.h"

class IOnlineSubsystem;
class UAudioComponent;
class USoundWaveProcedural;
class FUniqueNetIdString;
class IVoiceDecoder;
class IVoiceEncoder;
class IVoiceCapture;

/**
 * Container for unprocessed voice data
 */
struct FLocalVoiceData
{
	FLocalVoiceData() :
		VoiceRemainderSize(0)
	{
	}

	/** Amount of voice data not encoded last time */
	uint32 VoiceRemainderSize;
	/** Voice sample data not encoded last time */
	TArray<uint8> VoiceRemainder;
};

/** 
 * Remote voice data playing on a single client
 */
class FRemoteTalkerDataImpl
{
public:

	FRemoteTalkerDataImpl();
	/** Required for TMap FindOrAdd() */
	FRemoteTalkerDataImpl(FRemoteTalkerDataImpl&& Other);
	~FRemoteTalkerDataImpl();

	/** Reset the talker after long periods of silence */
	void Reset();
	/** Cleanup the talker before unregistration */
	void Cleanup();

	/** Maximum size of a single decoded packet */
	int32 MaxUncompressedDataSize;
	/** Maximum size of the outgoing playback queue */
	int32 MaxUncompressedDataQueueSize;
	/** Amount of data currently in the outgoing playback queue */
	int32 CurrentUncompressedDataQueueSize;

	/** Receive side timestamp since last voice packet fragment */
	double LastSeen;
	/** Number of frames starved of audio */
	int32 NumFramesStarved;
	/** Audio component playing this buffer (only valid on remote instances) */
	UAudioComponent* AudioComponent;
	/** Buffer for outgoing audio intended for procedural streaming */
	mutable FCriticalSection QueueLock;
	TArray<uint8> UncompressedDataQueue;
	/** Per remote talker voice decoding state */
	TSharedPtr<IVoiceDecoder> VoiceDecoder;
};

/**
 * Generic implementation of voice engine, using Voice module for capture/codec
 */
class FVoiceEngineImpl : public IVoiceEngine, public FSelfRegisteringExec
{
	class FVoiceSerializeHelper : public FGCObject
	{
		/** Reference to audio components */
		FVoiceEngineImpl* VoiceEngine;
		FVoiceSerializeHelper() :
			VoiceEngine(nullptr)
		{}

	public:

		FVoiceSerializeHelper(FVoiceEngineImpl* InVoiceEngine) :
			VoiceEngine(InVoiceEngine)
		{}
		~FVoiceSerializeHelper() {}
		
		/** FGCObject interface */
		virtual void AddReferencedObjects(FReferenceCollector& Collector) override
		{
			// Prevent garbage collection of audio components
			for (FRemoteTalkerData::TIterator It(VoiceEngine->RemoteTalkerBuffers); It; ++It)
			{
				FRemoteTalkerDataImpl& RemoteData = It.Value();
				if (RemoteData.AudioComponent)
				{
					Collector.AddReferencedObject(RemoteData.AudioComponent);
				}
			}
		}
	};

	friend class FVoiceSerializeHelper;

	/** Mapping of UniqueIds to the incoming voice data and their audio component */
	typedef TMap<FUniqueNetIdString, FRemoteTalkerDataImpl> FRemoteTalkerData;

	/** Reference to the main online subsystem */
	IOnlineSubsystem* OnlineSubsystem;

	FLocalVoiceData PlayerVoiceData[MAX_SPLITSCREEN_TALKERS];
	/** Reference to voice capture device */
	TSharedPtr<IVoiceCapture> VoiceCapture;
	/** Reference to voice encoding object */
	TSharedPtr<IVoiceEncoder> VoiceEncoder;

	/** User index currently holding onto the voice interface */
	int32 OwningUserIndex;
	/** Amount of uncompressed data available this frame */
	uint32 UncompressedBytesAvailable;
	/** Amount of compressed data available this frame */
	uint32 CompressedBytesAvailable;
	/** Current frame state of voice capture */
	EVoiceCaptureState::Type AvailableVoiceResult;
	/** Have we stopped capturing voice but are waiting for its completion */
	mutable bool bPendingFinalCapture;
	/** State of voice recording */
	bool bIsCapturing;

	/** Data from voice codec, waiting to send to network. */
	TArray<uint8> CompressedVoiceBuffer;
	/** Data from network playing on an audio component. */
	FRemoteTalkerData RemoteTalkerBuffers;
	/** Voice decompression buffer, shared by all talkers, valid during SubmitRemoteVoiceData */
	TArray<uint8> DecompressedVoiceBuffer;
	/** Serialization helper */
	FVoiceSerializeHelper* SerializeHelper;

	/**
	 * Determines if the specified index is the owner or not
	 *
	 * @param InIndex the index being tested
	 *
	 * @return true if this is the owner, false otherwise
	 */
	FORCEINLINE bool IsOwningUser(uint32 UserIndex)
	{
		return UserIndex >= 0 && UserIndex < MAX_SPLITSCREEN_TALKERS && OwningUserIndex == UserIndex;
	}

	/** 
	 * Update the internal state of the voice capturing state 
	 * Handles possible continuation waiting for capture stop event
	 */
	void VoiceCaptureUpdate() const;

	/** Start capturing voice data */
	void StartRecording() const;

	/** Stop capturing voice data */
	void StopRecording() const;

	/** Called when "last half second" is over */
	void StoppedRecording() const;

	/** @return is active recording occurring at the moment */
	bool IsRecording() const { return bIsCapturing || bPendingFinalCapture; }

	/**
	 * Callback from streaming audio when data is requested for playback
	 *
	 * @param InProceduralWave SoundWave requesting more data
	 * @param SamplesRequired number of samples needed for immediate playback
	 * @param TalkerId id of the remote talker to allocate voice data for
	 */
	void GenerateVoiceData(USoundWaveProcedural* InProceduralWave, int32 SamplesRequired, FUniqueNetIdString TalkerId);

PACKAGE_SCOPE:

	/** Constructor */
	FVoiceEngineImpl() :
		OnlineSubsystem(nullptr),
		VoiceCapture(nullptr),
		VoiceEncoder(nullptr),
		OwningUserIndex(INVALID_INDEX),
		UncompressedBytesAvailable(0),
		CompressedBytesAvailable(0),
		AvailableVoiceResult(EVoiceCaptureState::UnInitialized),
		bPendingFinalCapture(false),
		bIsCapturing(false),
		SerializeHelper(nullptr)
	{};

	// IVoiceEngine
	virtual bool Init(int32 MaxLocalTalkers, int32 MaxRemoteTalkers) override;

public:

	FVoiceEngineImpl(IOnlineSubsystem* InSubsystem);
	virtual ~FVoiceEngineImpl();

	// IVoiceEngine
	virtual uint32 StartLocalVoiceProcessing(uint32 LocalUserNum) override;
	virtual uint32 StopLocalVoiceProcessing(uint32 LocalUserNum) override;

	virtual uint32 StartRemoteVoiceProcessing(const FUniqueNetId& UniqueId) override
	{
		// Not needed
		return S_OK;
	}

	virtual uint32 StopRemoteVoiceProcessing(const FUniqueNetId& UniqueId) override
	{
		// Not needed
		return S_OK;
	}

	virtual uint32 RegisterLocalTalker(uint32 LocalUserNum) override
	{
		if (OwningUserIndex == INVALID_INDEX)
		{
			OwningUserIndex = LocalUserNum;
			return S_OK;
		}

		return E_FAIL;
	}

	virtual uint32 UnregisterLocalTalker(uint32 LocalUserNum) override
	{
		if (IsOwningUser(LocalUserNum))
		{
			OwningUserIndex = INVALID_INDEX;
			return S_OK;
		}

		return E_FAIL;
	}

	virtual uint32 RegisterRemoteTalker(const FUniqueNetId& UniqueId) override
	{
		// Not needed
		return S_OK;
	}

	virtual uint32 UnregisterRemoteTalker(const FUniqueNetId& UniqueId) override;

	virtual bool IsHeadsetPresent(uint32 LocalUserNum) override
	{
		return IsOwningUser(LocalUserNum) ? true : false;
	}

	virtual bool IsLocalPlayerTalking(uint32 LocalUserNum) override
	{
		return (GetVoiceDataReadyFlags() & (LocalUserNum << 1)) != 0;
	}

	virtual bool IsRemotePlayerTalking(const FUniqueNetId& UniqueId) override
	{
		return RemoteTalkerBuffers.Find((const FUniqueNetIdString&)UniqueId) != nullptr;
	}

	virtual uint32 GetVoiceDataReadyFlags() const override;
	virtual uint32 SetPlaybackPriority(uint32 LocalUserNum, const FUniqueNetId& RemoteTalkerId, uint32 Priority) override
	{
		// Not supported
		return S_OK;
	}

	virtual uint32 ReadLocalVoiceData(uint32 LocalUserNum, uint8* Data, uint32* Size) override;
	virtual uint32 SubmitRemoteVoiceData(const FUniqueNetId& RemoteTalkerId, uint8* Data, uint32* Size) override;
	virtual void Tick(float DeltaTime) override;
	FString GetVoiceDebugState() const override;

	// FSelfRegisteringExec
	virtual bool Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override;

private:

	/**
	 * Update the state of all remote talkers, possibly dropping data or the talker entirely
	 */
	void TickTalkers(float DeltaTime);

	/**
	 * Delegate triggered when an audio component Stop() function is called
	 */
	void OnAudioFinished(UAudioComponent* AC);
};

typedef TSharedPtr<FVoiceEngineImpl, ESPMode::ThreadSafe> FVoiceEngineImplPtr;
