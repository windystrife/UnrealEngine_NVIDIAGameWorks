// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/VoiceInterface.h"
#include "Net/VoiceDataCommon.h"
#include "OnlineSubsystemTypes.h"
#include "OnlineSubsystemSteamPrivate.h"
#include "OnlineSubsystemSteamTypes.h"
#include "UObject/GCObject.h"
#include "OnlineSubsystemSteamPackage.h"

class FOnlineSubsystemSteam;
class UAudioComponent;

/** 
 * Remote voice data playing on a single client
 */
class FRemoteTalkerDataSteam
{
public:
	FRemoteTalkerDataSteam() :
		LastSeen(0.0),
		AudioComponent(NULL)
	{
	}

	// Receive side timestamp since last voice packet fragment
	double LastSeen;
	// Audio component playing this buffer (only valid on remote instances)
	class UAudioComponent* AudioComponent;
};

/**
 * The Steam implementation of the voice engine 
 */
class FVoiceEngineSteam : public IVoiceEngine
{
 	class FVoiceSerializeHelper : public FGCObject
 	{
 		/** Reference to audio components */
 		FVoiceEngineSteam* VoiceEngine;
 		FVoiceSerializeHelper() :
 			VoiceEngine(NULL)
 		{}
 
 	public:
 
 		FVoiceSerializeHelper(FVoiceEngineSteam* InVoiceEngine) :
 			VoiceEngine(InVoiceEngine)
 		{}
 		~FVoiceSerializeHelper() {}
 		
 		/** FGCObject interface */
 		virtual void AddReferencedObjects(FReferenceCollector& Collector) override
 		{
 			// Prevent garbage collection of audio components
 			for (FRemoteTalkerData::TIterator It(VoiceEngine->RemoteTalkerBuffers); It; ++It)
 			{
 				FRemoteTalkerDataSteam& RemoteData = It.Value();
 				if (RemoteData.AudioComponent)
 				{
 					Collector.AddReferencedObject(RemoteData.AudioComponent);
 				}
 			}
 		}
 	};

	friend class FVoiceSerializeHelper;

	/** Mapping of UniqueIds to the incoming voice data and their audio component */
	typedef TMap<FUniqueNetIdSteam, FRemoteTalkerDataSteam> FRemoteTalkerData;

	/** Reference to the main Steam subsystem */
	class FOnlineSubsystemSteam* SteamSubsystem;
    /** Steam User interface */
	class ISteamUser* SteamUserPtr;
    /** Steam Friends interface */
	class ISteamFriends* SteamFriendsPtr;
    /** User index currently holding onto the voice interface */
	int32 OwningUserIndex;
	/** Amount of compressed data available this frame */
	uint32 CompressedBytesAvailable;
	/** Result of call to GetAvailableVoice() this frame*/
	EVoiceResult AvailableVoiceResult;
    /** Have we stopped Steam voice but are waiting for its completion */
	mutable bool bPendingFinalCapture;
    /** State of voice recording */
	bool bIsCapturing;

	/** Data from Steamworks, waiting to send to network. */
	TArray<uint8> CompressedVoiceBuffer;
	/** Data from network playing on an audio component. */
	FRemoteTalkerData RemoteTalkerBuffers;
	/** Voice decompression buffer, shared by all talkers */
	TArray<uint8> DecompressedVoiceBuffer;
	/** Serialization helper */
	class FVoiceSerializeHelper* SerializeHelper;

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
	 * Handles Steam's continual recording for "last half second" after requested stop
	 */
	void VoiceCaptureUpdate() const;

	/** Tell Steam to start capturing voice data */
	void StartRecording() const;

	/** Tell Steam to stop capturing voice data */
	void StopRecording() const;

	/** Called when "last half second" is over */
	void StoppedRecording() const;

	/** @return is active recording occurring at the moment */
	bool IsRecording() const { return bIsCapturing || bPendingFinalCapture; }

PACKAGE_SCOPE:

	/** Constructor */
	FVoiceEngineSteam() :
		SteamSubsystem(NULL),
		SteamUserPtr(NULL),
		SteamFriendsPtr(NULL),
		OwningUserIndex(INVALID_INDEX),
		CompressedBytesAvailable(0),
		AvailableVoiceResult(k_EVoiceResultNotInitialized),
		bPendingFinalCapture(false),
		bIsCapturing(false),
		SerializeHelper(NULL)
	{};

	// IVoiceEngine
	virtual bool Init(int32 MaxLocalTalkers, int32 MaxRemoteTalkers) override;

public:

	FVoiceEngineSteam(FOnlineSubsystemSteam* InSteamSubsystem);
	virtual ~FVoiceEngineSteam();

	// IVoiceEngine
    virtual uint32 StartLocalVoiceProcessing(uint32 LocalUserNum) override;
    virtual uint32 StopLocalVoiceProcessing(uint32 LocalUserNum) override;
    virtual uint32 StartRemoteVoiceProcessing(const FUniqueNetId& UniqueId) override
	{
		// Not needed in Steam
		return S_OK;
	}
    virtual uint32 StopRemoteVoiceProcessing(const FUniqueNetId& UniqueId) override
	{
		// Not needed in Steam
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
		// Not needed in Steam
		return S_OK;
	}

    virtual uint32 UnregisterRemoteTalker(const FUniqueNetId& UniqueId) override
	{
		// Not needed in Steam
		return S_OK;
	}

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
		return RemoteTalkerBuffers.Find((const FUniqueNetIdSteam&)UniqueId) != NULL;
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

	/**
	 * Update the state of all remote talkers, possibly dropping data or the talker entirely
	 */
	void TickTalkers(float DeltaTime);

	/**
	 * Delegate triggered when an audio component Stop() function is called
	 */
	void OnAudioFinished(UAudioComponent* AC);
};

typedef TSharedPtr<FVoiceEngineSteam, ESPMode::ThreadSafe> FVoiceEngineSteamPtr;
