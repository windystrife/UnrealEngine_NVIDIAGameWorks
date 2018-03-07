// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "Serialization/JsonSerializerMacros.h"
#include "NetworkReplayStreaming.h"
#include "Tickable.h"

class FNetworkReplayVersion;

/* Class to hold metadata about an entire replay */
class FNullReplayInfo : public FJsonSerializable
{
public:
	FNullReplayInfo() : LengthInMS(0), NetworkVersion(0), Changelist(0), DemoFileLastOffset(0), bIsValid(false) {}

	int32		LengthInMS;
	uint32		NetworkVersion;
	uint32		Changelist;
	FString		FriendlyName;
	int32		DemoFileLastOffset;
	bool		bIsValid;

	// FJsonSerializable
	BEGIN_JSON_SERIALIZER
		JSON_SERIALIZE( "LengthInMS",		LengthInMS );
		JSON_SERIALIZE( "NetworkVersion",	NetworkVersion );
		JSON_SERIALIZE( "Changelist",		Changelist );
		JSON_SERIALIZE( "FriendlyName",		FriendlyName );
		JSON_SERIALIZE( "DemoFileLastOffset",	DemoFileLastOffset );
	END_JSON_SERIALIZER
};

/** Default streamer that goes straight to the HD */
class FNullNetworkReplayStreamer : public INetworkReplayStreamer, public FTickableGameObject
{
public:
	FNullNetworkReplayStreamer() :
		StreamerState( EStreamerState::Idle ),
		CurrentCheckpointIndex( 0 ),
		LastKnownFileSize( 0 )
	{}
	
	/** INetworkReplayStreamer implementation */
	virtual void StartStreaming( const FString& CustomName, const FString& FriendlyName, const TArray< FString >& UserNames, bool bRecord, const FNetworkReplayVersion& ReplayVersion, const FOnStreamReadyDelegate& Delegate ) override;
	virtual void StopStreaming() override;
	virtual FArchive* GetHeaderArchive() override;
	virtual FArchive* GetStreamingArchive() override;
	virtual FArchive* GetCheckpointArchive() override;
	virtual void FlushCheckpoint( const uint32 TimeInMS ) override;
	virtual void GotoCheckpointIndex( const int32 CheckpointIndex, const FOnCheckpointReadyDelegate& Delegate ) override;
	virtual void GotoTimeInMS( const uint32 TimeInMS, const FOnCheckpointReadyDelegate& Delegate ) override;
	virtual void UpdateTotalDemoTime( uint32 TimeInMS ) override;
	virtual uint32 GetTotalDemoTime() const override { return ReplayInfo.LengthInMS; }
	virtual bool IsDataAvailable() const override;
	virtual void SetHighPriorityTimeRange( const uint32 StartTimeInMS, const uint32 EndTimeInMS ) override { }
	virtual bool IsDataAvailableForTimeRange( const uint32 StartTimeInMS, const uint32 EndTimeInMS ) override { return true; }
	virtual bool IsLoadingCheckpoint() const override { return false; }
	virtual bool IsLive() const override;
	virtual void DeleteFinishedStream( const FString& StreamName, const FOnDeleteFinishedStreamComplete& Delegate) const override;
	virtual void EnumerateStreams( const FNetworkReplayVersion& ReplayVersion, const FString& UserString, const FString& MetaString, const FOnEnumerateStreamsComplete& Delegate ) override;
	virtual void EnumerateStreams( const FNetworkReplayVersion& InReplayVersion, const FString& UserString, const FString& MetaString, const TArray< FString >& ExtraParms, const FOnEnumerateStreamsComplete& Delegate ) override;
	virtual void EnumerateRecentStreams( const FNetworkReplayVersion& ReplayVersion, const FString& RecentViewer, const FOnEnumerateStreamsComplete& Delegate ) override {}
	virtual ENetworkReplayError::Type GetLastError() const override { return ENetworkReplayError::None; }
	virtual void AddUserToReplay(const FString& UserString) override;
	virtual void AddEvent(const uint32 TimeInMS, const FString& Group, const FString& Meta, const TArray<uint8>& Data) override;
	virtual void AddOrUpdateEvent( const FString& Name, const uint32 TimeInMS, const FString& Group, const FString& Meta, const TArray<uint8>& Data ) override {}
	virtual void EnumerateEvents( const FString& Group, const FEnumerateEventsCompleteDelegate& EnumerationCompleteDelegate ) override;
	virtual void EnumerateEvents( const FString& ReplayName, const FString& Group, const FEnumerateEventsCompleteDelegate& EnumerationCompleteDelegate ) override {}
	virtual void RequestEventData(const FString& EventID, const FOnRequestEventDataComplete& RequestEventDataComplete) override;
	virtual void SearchEvents(const FString& EventGroup, const FOnEnumerateStreamsComplete& Delegate) override;
	virtual void KeepReplay( const FString& ReplayName, const bool bKeep ) override {}
	virtual FString	GetReplayID() const override { return TEXT( "" ); }
	virtual void SetTimeBufferHintSeconds(const float InTimeBufferHintSeconds) override {}
	virtual void RefreshHeader() override {};
	virtual void DownloadHeader(const FOnDownloadHeaderComplete& Delegate = FOnDownloadHeaderComplete()) override {}

	/** FTickableObjectBase implementation */
	virtual void Tick(float DeltaSeconds) override;
	virtual bool IsTickable() const override { return true; }
	virtual TStatId GetStatId() const override;

	/** FTickableGameObject implementation */
	virtual bool IsTickableWhenPaused() const override { return true; }

private:
	bool IsNamedStreamLive( const FString& StreamName ) const;

	/** Handles the details of loading a checkpoint */
	void GotoCheckpointIndexInternal(int32 CheckpointIndex, const FOnCheckpointReadyDelegate& Delegate, int32 TimeInMS);

	/** Reopen the file to refresh it's size, since file-based FArchives do not appear to update their size if they're being written to. */
	void ReopenStreamFileForReading();

	/** Overwrites the cached ReplayInfo only if the read succeeded */
	void UpdateReplayInfoIfValid();

	/** Handle to the archive that will read/write the demo header */
	TUniquePtr<FArchive> HeaderAr;

	/** Handle to the archive that will read/write network packets */
	TUniquePtr<FArchive> FileAr;

	/* Handle to the archive that will read/write checkpoint files */
	TUniquePtr<FArchive> CheckpointAr;

	/** EStreamerState - Overall state of the streamer */
	enum class EStreamerState
	{
		Idle,					// The streamer is idle. Either we haven't started streaming yet, or we are done
		Recording,				// We are in the process of recording a replay to disk
		Playback,				// We are in the process of playing a replay from disk
	};

	/** Overall state of the streamer */
	EStreamerState StreamerState;

	/** Remember the name of the current stream, if any. */
	FString CurrentStreamName;

	/** Current number of checkpoints written. */
	int32 CurrentCheckpointIndex;

	/** Currently playing or recording replay metadata */
	FNullReplayInfo ReplayInfo;

	/** Last known size of the replay stream file. */
	int64 LastKnownFileSize;
};

class FNullNetworkReplayStreamingFactory : public INetworkReplayStreamingFactory
{
public:
	virtual TSharedPtr< INetworkReplayStreamer > CreateReplayStreamer();
};
