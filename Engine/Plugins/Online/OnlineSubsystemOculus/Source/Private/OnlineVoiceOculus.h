// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "VoiceInterface.h"
#include "OnlineSubsystemOculusTypes.h"

/**
* Remote voice data playing on a single client
*/
class FRemoteTalkerDataOculus
{
public:

	FRemoteTalkerDataOculus();

	~FRemoteTalkerDataOculus() = default;

	/** Receive side timestamp since last voice packet fragment */
	double LastSeen;
	/** Audio component playing this buffer */
	class UAudioComponent* AudioComponent;
};

/**
* The Oculus implementation of the voice interface
*/
class FOnlineVoiceOculus : public IOnlineVoice
{

public:

	/**
	* Constructor
	*
	* @param InSubsystem online subsystem being used
	*/
	FOnlineVoiceOculus(class FOnlineSubsystemOculus& InSubsystem);
	virtual ~FOnlineVoiceOculus();

	// IOnlineVoice
	virtual void StartNetworkedVoice(uint8 LocalUserNum) override;
	virtual void StopNetworkedVoice(uint8 LocalUserNum) override;
	virtual bool RegisterLocalTalker(uint32 LocalUserNum) override
	{
		// no-op
		return (LocalUserNum == 0);
	}

	virtual void RegisterLocalTalkers() override
	{
		// no-op
	}
	virtual bool UnregisterLocalTalker(uint32 LocalUserNum) override
	{
		// no-op
		return true;
	}
	virtual void UnregisterLocalTalkers() override
	{
		// no-op
	}
	virtual bool RegisterRemoteTalker(const FUniqueNetId& UniqueId) override;
	virtual bool UnregisterRemoteTalker(const FUniqueNetId& UniqueId) override;
	virtual void RemoveAllRemoteTalkers() override;
	virtual bool IsHeadsetPresent(uint32 LocalUserNum) override
	{
		// no-op
		return (LocalUserNum == 0);
	}
	virtual bool IsLocalPlayerTalking(uint32 LocalUserNum) override
	{
		return (!bIsLocalPlayerMuted && LocalUserNum <= MAX_LOCAL_PLAYERS);
	}
	virtual bool IsRemotePlayerTalking(const FUniqueNetId& UniqueId) override;
	virtual bool IsMuted(uint32 LocalUserNum, const FUniqueNetId& UniqueId) const override;
	virtual bool MuteRemoteTalker(uint8 LocalUserNum, const FUniqueNetId& PlayerId, bool bIsSystemWide) override;
	virtual bool UnmuteRemoteTalker(uint8 LocalUserNum, const FUniqueNetId& PlayerId, bool bIsSystemWide) override;
	virtual TSharedPtr<class FVoicePacket> SerializeRemotePacket(FArchive& Ar) override
	{
		// not used
		return nullptr;
	}
	virtual TSharedPtr<class FVoicePacket> GetLocalPacket(uint32 LocalUserNum) override
	{
		// not used
		return nullptr;
	}
	virtual int32 GetNumLocalTalkers() override
	{
		return 1;
	}

	virtual void ClearVoicePackets() override
	{
		// no-op
	}
	virtual void Tick(float DeltaTime) override;
	virtual FString GetVoiceDebugState() const override;

	/**
	* Delegate triggered when an audio component Stop() function is called
	*/
	void OnAudioFinished(UAudioComponent* AC);

PACKAGE_SCOPE:

	// IOnlineVoice
	virtual bool Init() override;
	void ProcessMuteChangeNotification() override {}

	/**
	* Submits network packets to audio system for playback
	*/
	void ProcessRemoteVoicePackets();

private:

	bool bIsLocalPlayerMuted;

	/** Mapping of UniqueIds to the incoming voice data and their audio component */
	typedef TMap<class FUniqueNetIdOculus, FRemoteTalkerDataOculus> FRemoteTalkerData;
	/** Data from network playing on an audio component. */
	FRemoteTalkerData RemoteTalkerBuffers;
	/** Voice decompression buffer, shared by all talkers */
	TArray<int16_t> DecompressedVoiceBuffer;

	/** Set of talkers that the local player has muted */
	TSet<class FUniqueNetIdOculus> MutedRemoteTalkers;

	/**
	* Finds a remote talker in the cached list
	*
	* @param TalkerId the net id of the player to search for
	*
	* @return pointer to the remote talker or nullptr if not found
	*/
	struct FRemoteTalker* FindRemoteTalker(const FUniqueNetId& TalkerId);

	int32 IndexOfRemoteTalker(const FUniqueNetId& TalkerId);

	/** Reference to the main Oculus subsystem */
	class FOnlineSubsystemOculus& OculusSubsystem;

	FDelegateHandle VoipConnectionRequestDelegateHandle;
	void OnVoipConnectionRequest(ovrMessageHandle Message, bool bIsError) const;

	FDelegateHandle VoipStateChangeDelegateHandle;
	void OnVoipStateChange(ovrMessageHandle Message, bool bIsError);

	/** State of all possible remote talkers */
	TArray<FRemoteTalker> RemoteTalkers;

};

typedef TSharedPtr<FOnlineVoiceOculus, ESPMode::ThreadSafe> FOnlineVoiceOculusPtr;