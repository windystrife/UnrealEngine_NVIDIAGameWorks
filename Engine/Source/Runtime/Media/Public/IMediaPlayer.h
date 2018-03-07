// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"
#include "Internationalization/Text.h"
#include "Misc/Paths.h"
#include "Misc/Timespan.h"

class FArchive;

class IMediaCache;
class IMediaControls;
class IMediaOptions;
class IMediaSamples;
class IMediaTracks;
class IMediaView;

struct FGuid;


/**
 * Interface for media players.
 *
 * @see IMediaPlayerFactory
 */
class IMediaPlayer
{
public:

	//~ The following methods must be implemented by media players

	/**
	 * Close a previously opened media source.
	 *
	 * Call this method to free up all resources associated with an opened
	 * media source. If no media is open, this function has no effect.
	 *
	 * The media may not necessarily be closed after this function succeeds,
	 * because closing may happen asynchronously. Subscribe to the MediaClosed
	 * event to detect when the media finished closing. This events is only
	 * triggered if Close returns true.
	 *
	 * @see IsReady, Open
	 */
	virtual void Close() = 0;

	/**
	 * Get the player's cache controls.
	 *
	 * The interface returned by this method must remain valid for the player's life time.
	 *
	 * @return Cache controls.
	 * @see GetControls, GetSamples, GetTracks, GetView
	 */
	virtual IMediaCache& GetCache() = 0;

	/**
	 * Get the player's playback controls.
	 *
	 * The interface returned by this method must remain valid for the player's life time.
	 *
	 * @return Playback controls.
	 * @see GetCache, GetSamples, GetTracks, GetView
	 */
	virtual IMediaControls& GetControls() = 0;

	/**
	 * Get debug information about the player and currently opened media.
	 *
	 * @return Information string.
	 * @see GetStats
	 */
	virtual FString GetInfo() const = 0;

	/**
	 * Get the name of this player.
	 *
	 * @return Media player name, i.e. 'AndroidMedia' or 'WmfMedia'.
	 * @see GetMediaName
	 */
	virtual FName GetPlayerName() const = 0;

	/**
	 * Get the player's sample queue.
	 *
	 * The interface returned by this method must remain valid for the player's life time.
	 *
	 * @return Cache interface.
	 * @see GetCache, GetControls, GetTracks, GetView
	 */
	virtual IMediaSamples& GetSamples() = 0;

	/**
	 * Get playback statistics information.
	 *
	 * @return Information string.
	 * @see GetInfo
	 */
	virtual FString GetStats() const = 0;

	/**
	 * Get the player's track collection.
	 *
	 * The interface returned by this method must remain valid for the player's life time.
	 *
	 * @return Tracks interface.
	 * @see GetCache, GetControls, GetSamples, GetView
	 */
	virtual IMediaTracks& GetTracks() = 0;

	/**
	 * Get the URL of the currently loaded media.
	 *
	 * @return Media URL.
	 */
	virtual FString GetUrl() const = 0;

	/**
	 * Get the player's view settings.
	 *
	 * The interface returned by this method must remain valid for the player's life time.
	 *
	 * @return View interface.
	 * @see GetCache, GetControls, GetSamples, GetTracks
	 */
	virtual IMediaView& GetView() = 0;

	/**
	 * Open a media source from a URL with optional parameters.
	 *
	 * The media may not necessarily be opened after this function succeeds,
	 * because opening may happen asynchronously. Subscribe to the MediaOpened
	 * and MediaOpenFailed events to detect when the media finished or failed
	 * to open. These events are only triggered if Open returns true.
	 *
	 * The optional parameters can be used to configure aspects of media playback
	 * and are specific to the type of media source and the underlying player.
	 * Check their documentation for available keys and values.
	 *
	 * @param Url The URL of the media to open (file name or web address).
	 * @param Options Optional media parameters.
	 * @return true if the media is being opened, false otherwise.
	 * @see Close, IsReady, OnOpen, OnOpenFailed
	 */
	virtual bool Open(const FString& Url, const IMediaOptions* Options) = 0;

	/**
	 * Open a media source from a file or memory archive with optional parameters.
	 *
	 * The media may not necessarily be opened after this function succeeds,
	 * because opening may happen asynchronously. Subscribe to the MediaOpened
	 * and MediaOpenFailed events to detect when the media finished or failed
	 * to open. These events are only triggered if Open returns true.
	 *
	 * The optional parameters can be used to configure aspects of media playback
	 * and are specific to the type of media source and the underlying player.
	 * Check their documentation for available keys and values.
	 *
	 * @param Archive The archive holding the media data.
	 * @param OriginalUrl The original URL of the media that was loaded into the buffer.
	 * @param Options Optional media parameters.
	 * @return true if the media is being opened, false otherwise.
	 * @see Close, IsReady, OnOpen, OnOpenFailed
	 */
	virtual bool Open(const TSharedRef<FArchive, ESPMode::ThreadSafe>& Archive, const FString& OriginalUrl, const IMediaOptions* Options) = 0;

public:

	//~ The following methods are optional

	/**
	 * Get the human readable name of the currently loaded media source.
	 *
	 * Depending on the type of media source, this might be the name of a file,
	 * the display name of a capture device, or some other identifying string.
	 * If the player does not provide a specialized implementation for this
	 * method, the media name will be derived from the current media URL.
	 *
	 * @return Media source name, or empty text if no media is opened
	 * @see GetPlayerName, GetUrl
	 */
	virtual FText GetMediaName() const
	{
		const FString Url = GetUrl();

		if (Url.IsEmpty())
		{
			return FText::GetEmpty();
		}

		return FText::FromString(FPaths::GetBaseFilename(Url));
	}

	/**
	 * Set the player's globally unique identifier.
	 *
	 * @param Guid The GUID to set.
	 */
	virtual void SetGuid(const FGuid& Guid)
	{
		// override in child classes if supported
	}

	/**
	 * Tick the player's audio related code.
	 *
	 * This is a high-frequency tick function. Media players override this method
	 * to fetch and process audio samples, or to perform other time critical tasks.
	 *
	 * @see TickInput, TickFetch
	 */
	virtual void TickAudio()
	{
		// override in child class if needed
	}

	/**
	 * Tick the player in the Fetch phase.
	 *
	 * Media players may override this method to fetch newly decoded input
	 * samples before they are rendered on textures or audio components.
	 *
	 * @param DeltaTime Time since last tick.
	 * @param Timecode The current media time code.
	 * @see TickAudio, TickInput
	 */
	virtual void TickFetch(FTimespan DeltaTime, FTimespan Timecode)
	{
		// override in child class if needed
	}

	/**
	 * Tick the player in the Input phase.
	 *
	 * Media players may override this method to update their state before the
	 * Engine is being ticked, or to initiate the processing of input samples.
	 *
	 * @param DeltaTime Time since last tick.
	 * @param Timecode The current media time code.
	 * @see TickAudio, TickFetch
	 */
	virtual void TickInput(FTimespan DeltaTime, FTimespan Timecode)
	{
		// override in child class if needed
	}

public:

	/** Virtual destructor. */
	virtual ~IMediaPlayer() { }
};
