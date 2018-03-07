// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

// Dependencies.

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "Serialization/JsonSerializerMacros.h"

class FNetworkReplayVersion;

class FReplayEventListItem : public FJsonSerializable
{
public:
	FReplayEventListItem() {}
	virtual ~FReplayEventListItem() {}

	FString		ID;
	FString		Group;
	FString		Metadata;
	uint32		Time1;
	uint32		Time2;

	// FJsonSerializable
	BEGIN_JSON_SERIALIZER
		JSON_SERIALIZE("id", ID);
		JSON_SERIALIZE("group", Group);
		JSON_SERIALIZE("meta", Metadata);
		JSON_SERIALIZE("time1", Time1);
		JSON_SERIALIZE("time2", Time2);
	END_JSON_SERIALIZER
};

class FReplayEventList : public FJsonSerializable
{
public:
	FReplayEventList()
	{}
	virtual ~FReplayEventList() {}

	TArray< FReplayEventListItem > ReplayEvents;

	// FJsonSerializable
	BEGIN_JSON_SERIALIZER
		JSON_SERIALIZE_ARRAY_SERIALIZABLE("events", ReplayEvents, FReplayEventListItem);
	END_JSON_SERIALIZER
};

/** Struct to store information about a stream, returned from search results. */
struct FNetworkReplayStreamInfo
{
	FNetworkReplayStreamInfo() : SizeInBytes( 0 ), LengthInMS( 0 ), NumViewers( 0 ), bIsLive( false ), Changelist( 0 ), bShouldKeep( false ) {}

	/** The name of the stream (generally this is auto generated, refer to friendly name for UI) */
	FString Name;

	/** The UI friendly name of the stream */
	FString FriendlyName;

	/** The date and time the stream was recorded */
	FDateTime Timestamp;

	/** The size of the stream */
	int64 SizeInBytes;
	
	/** The duration of the stream in MS */
	int32 LengthInMS;

	/** Number of viewers viewing this stream */
	int32 NumViewers;

	/** True if the stream is live and the game hasn't completed yet */
	bool bIsLive;

	/** The changelist of the replay */
	int32 Changelist;

	/** Debug feature that allows us to mark replays to not be deleted. True if this replay has been marked as such */
	bool bShouldKeep;
};

namespace ENetworkReplayError
{
	enum Type
	{
		/** There are currently no issues */
		None,

		/** The backend service supplying the stream is unavailable, or connection interrupted */
		ServiceUnavailable,
	};

	inline const TCHAR* ToString( const ENetworkReplayError::Type FailureType )
	{
		switch ( FailureType )
		{
			case None:
				return TEXT( "None" );
			case ServiceUnavailable:
				return TEXT( "ServiceUnavailable" );
		}
		return TEXT( "Unknown ENetworkReplayError error" );
	}
}

/**
 * Delegate called when StartStreaming() completes.
 *
 * @param bWasSuccessful Whether streaming was started.
 * @param bRecord Whether streaming is recording or not (vs playing)
 */
DECLARE_DELEGATE_TwoParams( FOnStreamReadyDelegate, const bool, const bool );

/**
 * Delegate called when GotoCheckpointIndex() completes.
 *
 * @param bWasSuccessful Whether streaming was started.
 */
DECLARE_DELEGATE_TwoParams( FOnCheckpointReadyDelegate, const bool, const int64 );

/**
 * Delegate called when DeleteFinishedStream() completes.
 *
 * @param bWasSuccessful Whether the stream was deleted.
 */
DECLARE_DELEGATE_OneParam( FOnDeleteFinishedStreamComplete, const bool );

/**
 * Delegate called when EnumerateStreams() completes.
 *
 * @param Streams An array containing information about the streams that were found.
 */
DECLARE_DELEGATE_OneParam( FOnEnumerateStreamsComplete, const TArray<FNetworkReplayStreamInfo>& );

/**
* Delegate called when EnumerateEvents() completes.
*
* @param ReplayEventList A list of events that were found
# @param bWasSuccessful Whether enumerating events was successful
*/
DECLARE_DELEGATE_TwoParams(FEnumerateEventsCompleteDelegate, const FReplayEventList&, bool);

/**
* Delegate called when RequestEventData() completes.
*
* @param ReplayEventListItem A replay event with its data parameter filled in
# @param bWasSuccessful Whether enumerating events was successful
*/
DECLARE_DELEGATE_TwoParams(FOnRequestEventDataComplete, const TArray<uint8>&, bool)

/**
* Delegate called when DownloadHeader() completes.
*
* @param bWasSuccessful Whether the header was successfully downloaded.
*/
DECLARE_DELEGATE_OneParam(FOnDownloadHeaderComplete, const bool);

/** Generic interface for network replay streaming */
class INetworkReplayStreamer 
{
public:
	virtual ~INetworkReplayStreamer() {}

	virtual void StartStreaming( const FString& CustomName, const FString& FriendlyName, const TArray< FString >& UserNames, bool bRecord, const FNetworkReplayVersion& ReplayVersion, const FOnStreamReadyDelegate& Delegate ) = 0;
	virtual void StopStreaming() = 0;
	virtual FArchive* GetHeaderArchive() = 0;
	virtual FArchive* GetStreamingArchive() = 0;
	virtual FArchive* GetCheckpointArchive() = 0;
	virtual void FlushCheckpoint( const uint32 TimeInMS ) = 0;
	virtual void GotoCheckpointIndex( const int32 CheckpointIndex, const FOnCheckpointReadyDelegate& Delegate ) = 0;
	virtual void GotoTimeInMS( const uint32 TimeInMS, const FOnCheckpointReadyDelegate& Delegate ) = 0;
	virtual void UpdateTotalDemoTime( uint32 TimeInMS ) = 0;
	virtual uint32 GetTotalDemoTime() const = 0;
	virtual bool IsDataAvailable() const = 0;
	virtual void SetHighPriorityTimeRange( const uint32 StartTimeInMS, const uint32 EndTimeInMS ) = 0;
	virtual bool IsDataAvailableForTimeRange( const uint32 StartTimeInMS, const uint32 EndTimeInMS ) = 0;
	virtual bool IsLoadingCheckpoint() const = 0;
	virtual void AddEvent( const uint32 TimeInMS, const FString& Group, const FString& Meta, const TArray<uint8>& Data ) = 0;
	virtual void AddOrUpdateEvent( const FString& Name, const uint32 TimeInMS, const FString& Group, const FString& Meta, const TArray<uint8>& Data ) = 0;
	virtual void EnumerateEvents( const FString& Group, const FEnumerateEventsCompleteDelegate& EnumerationCompleteDelegate ) = 0;
	virtual void EnumerateEvents( const FString& ReplayName, const FString& Group, const FEnumerateEventsCompleteDelegate& EnumerationCompleteDelegate ) = 0;
	virtual void RequestEventData( const FString& EventID, const FOnRequestEventDataComplete& RequestEventDataComplete ) = 0;
	virtual void SearchEvents(const FString& EventGroup, const FOnEnumerateStreamsComplete& Delegate) = 0;
	virtual void KeepReplay( const FString& ReplayName, const bool bKeep ) = 0;
	virtual void RefreshHeader() = 0;
	virtual void DownloadHeader(const FOnDownloadHeaderComplete& Delegate = FOnDownloadHeaderComplete()) = 0;

	/** Returns true if the playing stream is currently in progress */
	virtual bool IsLive() const = 0;
	virtual FString	GetReplayID() const = 0;

	/**
	 * Attempts to delete the stream with the specified name. May execute asynchronously.
	 *
	 * @param StreamName The name of the stream to delete
	 * @param Delegate A delegate that will be executed if bound when the delete operation completes
	 */
	virtual void DeleteFinishedStream( const FString& StreamName, const FOnDeleteFinishedStreamComplete& Delegate) const = 0;

	/**
	 * Retrieves the streams that are available for viewing. May execute asynchronously.
	 *
	 * @param Delegate A delegate that will be executed if bound when the list of streams is available
	 */
	virtual void EnumerateStreams( const FNetworkReplayVersion& ReplayVersion, const FString& UserString, const FString& MetaString, const FOnEnumerateStreamsComplete& Delegate ) = 0;

	/**
	* Retrieves the streams that are available for viewing. May execute asynchronously.
	* Allows the caller to pass in a custom list of query parameters
	*/
	virtual void EnumerateStreams( const FNetworkReplayVersion& InReplayVersion, const FString& UserString, const FString& MetaString, const TArray< FString >& ExtraParms, const FOnEnumerateStreamsComplete& Delegate ) = 0;

	/**
	 * Retrieves the streams that have been recently viewed. May execute asynchronously.
	 *
	 * @param Delegate A delegate that will be executed if bound when the list of streams is available
	 */
	virtual void EnumerateRecentStreams( const FNetworkReplayVersion& ReplayVersion, const FString& RecentViewer, const FOnEnumerateStreamsComplete& Delegate ) = 0;

	/** Returns the last error that occurred while streaming replays */
	virtual ENetworkReplayError::Type GetLastError() const = 0;

	/**
	 * Adds a join-in-progress user to the set of users associated with the currently recording replay (if any)
	 *
	 * @param UserString a string that uniquely identifies the user, usually his or her FUniqueNetId
	 */
	virtual void AddUserToReplay( const FString& UserString ) = 0;

	/**
	 * Sets a hint for how much data needs to be kept in memory. If set to a value greater than zero,
	 * a streamer implementation may free any in-memory data that would be required to go to a time
	 * before the beginning of the buffer.
	 */
	virtual void SetTimeBufferHintSeconds(const float InTimeBufferHintSeconds) = 0;
};

/** Replay streamer factory */
class INetworkReplayStreamingFactory : public IModuleInterface
{
public:
	virtual TSharedPtr< INetworkReplayStreamer > CreateReplayStreamer() = 0;
};

/** Replay streaming factory manager */
class FNetworkReplayStreaming : public IModuleInterface
{
public:
	static inline FNetworkReplayStreaming& Get()
	{
		return FModuleManager::LoadModuleChecked< FNetworkReplayStreaming >( "NetworkReplayStreaming" );
	}

	virtual INetworkReplayStreamingFactory& GetFactory(const TCHAR* FactoryNameOverride = nullptr);
};
