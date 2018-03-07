// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/Queue.h"
#include "Containers/UnrealString.h"
#include "Delegates/Delegate.h"
#include "IMediaClockSink.h"
#include "IMediaEventSink.h"
#include "IMediaTickable.h"
#include "Math/Quat.h"
#include "Math/Range.h"
#include "Math/RangeSet.h"
#include "Math/Rotator.h"
#include "MediaSampleSinks.h"
#include "Misc/Guid.h"
#include "Misc/Timespan.h"
#include "Templates/SharedPointer.h"
#include "UObject/NameTypes.h"

class FMediaSampleCache;
class IMediaOptions;
class IMediaPlayer;
class IMediaSamples;

enum class EMediaEvent;
enum class EMediaCacheState;
enum class EMediaThreads;
enum class EMediaTrackType;

struct FMediaAudioTrackFormat;
struct FMediaVideoTrackFormat;


/**
 * Facade for low-level media player objects.
 *
 * The purpose of this class is to provide a simpler interface to low-level media player
 * implementations. It implements common functionality, such as translating between time
 * codes and play times, and manages the selection and creation of player implementations
 * for a given media source.
 *
 * Note that, unlike the low-level methods in IMediaTracks, most track and track format
 * related methods in this class allow for INDEX_NONE to be used as track and format
 * indices in order to indicate the 'current selection'.
 */
class MEDIAUTILS_API FMediaPlayerFacade
	: public IMediaClockSink
	, public IMediaTickable
	, protected IMediaEventSink
{
public:

	/** Name of the desired native player, if any. */
	FName DesiredPlayerName;

public:

	/** Default constructor. */
	FMediaPlayerFacade();

	/** Virtual destructor. */
	virtual ~FMediaPlayerFacade();

public:

	void AddAudioSampleSink(const TSharedRef<FMediaAudioSampleSink, ESPMode::ThreadSafe>& SampleSink);

	void AddCaptionSampleSink(const TSharedRef<FMediaOverlaySampleSink, ESPMode::ThreadSafe>& SampleSink);

	void AddMetadataSampleSink(const TSharedRef<FMediaBinarySampleSink, ESPMode::ThreadSafe>& SampleSink);

	void AddSubtitleSampleSink(const TSharedRef<FMediaOverlaySampleSink, ESPMode::ThreadSafe>& SampleSink);

	void AddVideoSampleSink(const TSharedRef<FMediaTextureSampleSink, ESPMode::ThreadSafe>& SampleSink);

	/**
	 * Whether playback can be paused.
	 *
	 * Playback can be paused if the media supports pausing
	 * and if it is currently playing.
	 *
	 * @return true if pausing is allowed, false otherwise.
	 * @see CanResume, CanScrub, CanSeek, Pause
	 */
	bool CanPause() const;

	/**
	 * Whether the specified URL can be played by this player.
	 *
	 * If a desired player name is set for this player, it will only check
	 * whether that particular player type can play the specified URL.
	 *
	 * @param Url The URL to check.
	 * @param Options Optional media parameters.
	 * @see CanPlaySource, SetDesiredPlayerName
	 */
	bool CanPlayUrl(const FString& Url, const IMediaOptions* Options);

	/**
	 * Whether playback can be resumed.
	 *
	 * Playback can be resumed if media is loaded and
	 * if it is not already playing.
	 *
	 * @return true if resuming is allowed, false otherwise.
	 * @see CanPause, CanScrub, CanSeek, SetRate
	 */
	bool CanResume() const;

	/**
	 * Whether playback can be scrubbed.
	 *
	 * @return true if scrubbing is allowed, false otherwise.
	 * @see CanPause, CanResume, CanSeek, Seek
	 */
	bool CanScrub() const;

	/**
	 * Whether playback can jump to a position.
	 *
	 * @return true if seeking is allowed, false otherwise.
	 * @see CanPause, CanResume, CanScrub, Seek
	 */
	bool CanSeek() const;

	/**
	 * Close the currently open media, if any.
	 */
	void Close();

	/**
	 * Get the number of channels in the specified audio track.
	 *
	 * @param TrackIndex Index of the audio track.
	 * @param FormatIndex Index of the track format.
	 * @return Number of channels.
	 * @see GetAudioTrackSampleRate, GetAudioTrackType
	 */
	uint32 GetAudioTrackChannels(int32 TrackIndex, int32 FormatIndex) const;

	/**
	 * Get the sample rate of the specified audio track.
	 *
	 * @param TrackIndex Index of the audio track.
	 * @param FormatIndex Index of the track format.
	 * @return Samples per second.
	 * @see GetAudioTrackChannels, GetAudioTrackType
	 */
	uint32 GetAudioTrackSampleRate(int32 TrackIndex, int32 FormatIndex) const;

	/**
	 * Get the type of the specified audio track format.
	 *
	 * @param TrackIndex The index of the track.
	 * @param FormatIndex Index of the track format.
	 * @return Audio format type string.
	 * @see GetAudioTrackSampleRate, GetAudioTrackSampleRate
	 */
	FString GetAudioTrackType(int32 TrackIndex, int32 FormatIndex) const;

	/**
	 * Get the media's duration.
	 *
	 * @return A time span representing the duration.
	 * @see GetTime, Seek
	 */
	FTimespan GetDuration() const;

	/**
	 * Get the player's globally unique identifier.
	 *
	 * @return The Guid.
	 * @see SetGuid
	 */
	const FGuid& GetGuid();

	/**
	 * Get debug information about the player and currently opened media.
	 *
	 * @return Information string.
	 * @see GetStats
	 */
	FString GetInfo() const;

	/**
	 * Get the human readable name of the currently loaded media source.
	 *
	 * @return Media source name, or empty text if no media is opened
	 * @see GetPlayerName, GetUrl
	 */
	FText GetMediaName() const;

	/**
	 * Get the number of tracks of the given type.
	 *
	 * @param TrackType The type of media tracks.
	 * @return Number of tracks.
	 * @see GetSelectedTrack, SelectTrack
	 */
	int32 GetNumTracks(EMediaTrackType TrackType) const;

	/**
	 * Get the number of formats of the specified track.
	 *
	 * @param TrackType The type of media tracks.
	 * @param TrackIndex The index of the track.
	 * @return Number of formats.
	 * @see GetNumTracks, GetSelectedTrack, SelectTrack
	 */
	int32 GetNumTrackFormats(EMediaTrackType TrackType, int32 TrackIndex) const;

	/**
	 * Get the low-level player associated with this object.
	 *
	 * @return The player, or nullptr if no player was created.
	 */
	TSharedPtr<IMediaPlayer, ESPMode::ThreadSafe> GetPlayer() const
	{
		return Player;
	}

	/**
	 * Get the name of the current native media player.
	 *
	 * @return Player name, or NAME_None if not available.
	 * @see GetMediaName
	 */
	FName GetPlayerName() const;

	/**
	 * Get the media's current playback rate.
	 *
	 * @return The playback rate.
	 * @see SetRate, SupportsRate
	 */
	float GetRate() const;

	/**
	 * Get the index of the currently selected track of the given type.
	 *
	 * @param TrackType The type of track to get.
	 * @return The index of the selected track, or INDEX_NONE if no track is active.
	 * @see GetNumTracks, SelectTrack
	 */
	int32 GetSelectedTrack(EMediaTrackType TrackType) const;

	/**
	 * Get playback statistics information.
	 *
	 * @return Information string.
	 * @see GetInfo
	 */
	FString GetStats() const;

	/**
	 * Get the supported playback rates.
	 *
	 * @param Unthinned Whether the rates are for unthinned playback (default = true).
	 * @return The ranges of supported rates.
	 * @see SetRate, SupportsRate
	 */
	TRangeSet<float> GetSupportedRates(bool Unthinned = true) const;

	/**
	 * Get the media's current playback time.
	 *
	 * @return Playback time.
	 * @see GetDuration, Seek
	 */
	FTimespan GetTime() const;

	/**
	 * Get the human readable name of the specified track.
	 *
	 * @param TrackType The type of track.
	 * @param TrackIndex The index of the track.
	 * @return Display name.
	 * @see GetNumTracks, GetTrackLanguage
	 */
	FText GetTrackDisplayName(EMediaTrackType TrackType, int32 TrackIndex) const;

	/**
	 * Get the index of the active format of the specified track.
	 *
	 * @param TrackType The type of track.
	 * @param TrackIndex The index of the track.
	 * @return The index of the selected format.
	 * @see GetNumTrackFormats, GetSelectedTrack, SetTrackFormat
	 */
	int32 GetTrackFormat(EMediaTrackType TrackType, int32 TrackIndex) const;

	/**
	 * Get the language tag of the specified track.
	 *
	 * @param TrackType The type of track.
	 * @param TrackIndex The index of the track.
	 * @return Language tag, i.e. "en-US" for English, or "und" for undefined.
	 * @see GetNumTracks, GetTrackDisplayName
	 */
	FString GetTrackLanguage(EMediaTrackType TrackType, int32 TrackIndex) const;

	/**
	 * Get the URL of the currently loaded media, if any.
	 *
	 * @return Media URL, or empty string if no media was loaded.
	 */
	const FString& GetUrl() const
	{
		return CurrentUrl;
	}

	/**
	 * Get the aspect ratio of the specified video track.
	 *
	 * @param TrackIndex The index of the track.
	 * @param FormatIndex Index of the track format.
	 * @return Aspect ratio.
	 * @see GetVideoTrackDimensions, GetVideoTrackFrameRate, GetVideoTrackFrameRates, GetVideoTrackType
	 */
	float GetVideoTrackAspectRatio(int32 TrackIndex, int32 FormatIndex) const;

	/**
	 * Get the width and height of the specified video track.
	 *
	 * @param TrackIndex The index of the track.
	 * @param FormatIndex Index of the track format.
	 * @return Video dimensions.
	 * @see GetVideoTrackAspectRatio, GetVideoTrackFrameRate, GetVideoTrackFrameRates, GetVideoTrackType
	 */
	FIntPoint GetVideoTrackDimensions(int32 TrackIndex, int32 FormatIndex) const;

	/**
	 * Get frame rate of the specified video track.
	 *
	 * @param TrackIndex The index of the track.
	 * @param FormatIndex Index of the track format.
	 * @return Video frame rate.
	 * @see GetVideoTrackAspectRatio, GetVideoTrackDimensions, GetVideoTrackFrameRates, GetVideoTrackType
	 */
	float GetVideoTrackFrameRate(int32 TrackIndex, int32 FormatIndex) const;

	/**
	 * Get the supported range of frame rates of the specified video track.
	 *
	 * @param TrackIndex The index of the track.
	 * @param FormatIndex Index of the track format.
	 * @return Frame rate range (in frames per second).
	 * @see GetVideoTrackAspectRatio, GetVideoTrackDimensions, GetVideoTrackFrameRate, GetVideoTrackType
	 */
	TRange<float> GetVideoTrackFrameRates(int32 TrackIndex, int32 FormatIndex) const;

	/**
	 * Get the type of the specified video track format.
	 *
	 * @param TrackIndex The index of the track.
	 * @param FormatIndex Index of the track format.
	 * @return Video format type string.
	 * @see GetVideoTrackAspectRatio, GetVideoTrackDimensions, GetVideoTrackFrameRate, GetVideoTrackFrameRates
	 */
	FString GetVideoTrackType(int32 TrackIndex, int32 FormatIndex) const;

	/**
	 * Get the field of view.
	 *
	 * @param OutHorizontal Will contain the horizontal field of view.
	 * @param OutVertical Will contain the vertical field of view.
	 * @return true on success, false if feature is not available or if field of view has never been set.
	 * @see GetViewOrientation, SetViewField
	 */
	bool GetViewField(float& OutHorizontal, float& OutVertical) const;

	/**
	 * Get the view's orientation.
	 *
	 * @param OutOrientation Will contain the view orientation.
	 * @return true on success, false if feature is not available or if orientation has never been set.
	 * @see GetViewField, SetViewOrientation
	 */
	bool GetViewOrientation(FQuat& OutOrientation) const;

	/**
	 * Check whether the player is in an error state.
	 *
	 * @see IsReady
	 */
	bool HasError() const;

	/**
	 * Whether the player is currently buffering data.
	 *
	 * @return true if buffering, false otherwise.
	 * @see GetState, IsConnecting, IsLooping
	 */
	bool IsBuffering() const;

	/**
	 * Whether the player is currently connecting to a media source.
	 *
	 * @return true if connecting, false otherwise.
	 * @see GetState, IsBuffering, IsLooping
	 */
	bool IsConnecting() const;

	/**
	 * Whether playback is looping.
	 *
	 * @return true if looping, false otherwise.
	 * @see GetState, IsBuffering, IsConnecting, SetLooping
	 */
	bool IsLooping() const;

	/**
	 * Whether playback is currently paused.
	 *
	 * @return true if playback is paused, false otherwise.
	 * @see CanPause, IsPlaying, IsReady, Pause
	 */
	bool IsPaused() const;

	/**
	 * Whether playback is in progress.
	 *
	 * @return true if playback has started, false otherwise.
	 * @see CanPlay, IsPaused, IsReady, Play
	 */
	bool IsPlaying() const;

	/**
	 * Whether the media is currently opening or buffering.
	 *
	 * @return true if playback is being prepared, false otherwise.
	 * @see CanPlay, IsPaused, IsReady, Play
	 */
	bool IsPreparing() const;

	/**
	 * Whether media is ready for playback.
	 *
	 * A player is ready for playback if it has a media source opened that
	 * finished preparing and is not in an error state.
	 *
	 * @return true if media is ready, false otherwise.
	 * @see HasError, IsPaused, IsPlaying, Stop
	 */
	bool IsReady() const;

	/**
	 * Open a media source from a URL with optional parameters.
	 *
	 * @param Url The URL of the media to open (file name or web address).
	 * @param Options Optional media parameters.
	 * @return true if the media is being opened, false otherwise.
	 */
	bool Open(const FString& Url, const IMediaOptions* Options);

	/**
	 * Query the time ranges of cached media samples for the specified caching state.
	 *
	 * @param State The sample state we're interested in.
	 * @param OutTimeRanges Will contain the set of matching sample time ranges.
	 */
	void QueryCacheState(EMediaTrackType TrackType, EMediaCacheState State, TRangeSet<FTimespan>& OutTimeRanges) const;

	/**
	 * Seeks to the specified playback time.
	 *
	 * @param Time The playback time to set.
	 * @return true on success, false otherwise.
	 * @see GetTime, Rewind
	 */
	bool Seek(const FTimespan& Time);

	/**
	 * Select the active track of the given type.
	 *
	 * The selected track will use its currently active format. Active formats will
	 * be remembered on a per track basis. The first available format is active by
	 * default. To switch the track format, use SetTrackFormat instead.
	 *
	 * @param TrackType The type of track to select.
	 * @param TrackIndex The index of the track to select, or INDEX_NONE to deselect.
	 * @return true if the track was selected, false otherwise.
	 * @see GetNumTracks, GetSelectedTrack, SetTrackFormat
	 */
	bool SelectTrack(EMediaTrackType TrackType, int32 TrackIndex);

	/**
	 * Set sample caching options.
	 *
	 * @param Ahead Duration of samples to cache ahead of the play head.
	 * @param Behind Duration of samples to cache behind the play head.
	 */
	void SetCacheWindow(FTimespan Ahead, FTimespan Behind);

	/**
	 * Set the player's globally unique identifier.
	 *
	 * @param Guid The GUID to set.
	 * @see GetGuid
	 */
	void SetGuid(FGuid& Guid);

	/**
	 * Enables or disables playback looping.
	 *
	 * @param Looping Whether playback should be looped.
	 * @return true on success, false otherwise.
	 * @see IsLooping
	 */
	bool SetLooping(bool Looping);

	/**
	 * Changes the media's playback rate.
	 *
	 * @param Rate The playback rate to set.
	 * @return true on success, false otherwise.
	 * @see GetRate, SupportsRate
	 */
	bool SetRate(float Rate);

	/**
	 * Set the format on the specified track.
	 *
	 * Selecting the format will not switch to the specified track. To switch
	 * tracks, use SelectTrack instead. If the track is already selected, the
	 * format change will be applied immediately.
	 *
	 * @param TrackType The type of track to update.
	 * @param TrackIndex The index of the track to update.
	 * @param FormatIndex The index of the format to select (must be valid).
	 * @return true if the track was selected, false otherwise.
	 * @see GetNumTrackFormats, GetNumTracks, GetTrackFormat, SelectTrack
	 */
	bool SetTrackFormat(EMediaTrackType TrackType, int32 TrackIndex, int32 FormatIndex);

	/**
	 * Set the frame rate of the specified video track.
	 *
	 * @param TrackIndex The index of the track, or INDEX_NONE for the selected one.
	 * @param FormatIndex Index of the track format, or INDEX_NONE for the selected one.
	 * @param FrameRate The frame rate to set (must be in range of format's supported frame rates).
	 * @return true on success, false otherwise.
	 * @see GetVideoTrackAspectRatio, GetVideoTrackDimensions, GetVideoTrackFrameRate, GetVideoTrackFrameRates, GetVideoTrackType
	 */
	bool SetVideoTrackFrameRate(int32 TrackIndex, int32 FormatIndex, float FrameRate);

	/**
	 * Set the field of view.
	 *
	 * @param Horizontal Horizontal field of view (in Euler degrees).
	 * @param Vertical Vertical field of view (in Euler degrees).
	 * @param Whether the field of view change should be absolute (true) or relative (false).
	 * @return true on success, false otherwise.
	 * @see GetViewField, SetViewOrientation
	 */
	bool SetViewField(float Horizontal, float Vertical, bool Absolute);

	/**
	 * Set the view's orientation.
	 *
	 * @param Orientation Quaternion representing the orientation.
	 * @param Whether the orientation change should be absolute (true) or relative (false).
	 * @return true on success, false otherwise.
	 * @see GetViewOrientation, SetViewField
	 */
	bool SetViewOrientation(const FQuat& Orientation, bool Absolute);

	/**
	 * Whether the specified playback rate is supported.
	 *
	 * @param Rate The playback rate to check.
	 * @param Unthinned Whether no frames should be dropped at the given rate.
	 * @see CanScrub, CanSeek
	 */
	bool SupportsRate(float Rate, bool Unthinned) const;

public:

	/** Get an event delegate that is invoked when a media event occurred. */
	DECLARE_EVENT_OneParam(FMediaPlayerFacade, FOnMediaEvent, EMediaEvent /*Event*/)
	FOnMediaEvent& OnMediaEvent()
	{
		return MediaEvent;
	}

public:

	//~ IMediaClockSink interface

	virtual void TickFetch(FTimespan DeltaTime, FTimespan Timecode) override;
	virtual void TickInput(FTimespan DeltaTime, FTimespan Timecode) override;
	virtual void TickOutput(FTimespan DeltaTime, FTimespan Timecode) override;

public:

	//~ IMediaTickable interface

	virtual void TickTickable() override;

protected:

	/** Flush all media sample sinks. */
	void FlushSinks();

	/**
	 * Get details about the specified audio track format.
	 *
	 * @param TrackIndex The index of the audio track.
	 * @param FormatIndex The index of the track's format.
	 * @param OutFormat Will contain the format details.
	 * @return true on success, false otherwise.
	 * @see GetVideoTrackFormat
	 */
	bool GetAudioTrackFormat(int32 TrackIndex, int32 FormatIndex, FMediaAudioTrackFormat& OutFormat) const;

	/**
	 * Get a player that can play the specified media URL.
	 *
	 * @param Url The URL to play.
	 * @param Options The media options for the URL.
	 * @return The player if found, or nullptr otherwise.
	 */
	TSharedPtr<IMediaPlayer, ESPMode::ThreadSafe> GetPlayerForUrl(const FString& Url, const IMediaOptions* Options);

	/**
	 * Get details about the specified audio track format.
	 *
	 * @param TrackIndex The index of the audio track.
	 * @param FormatIndex The index of the track's format.
	 * @param OutFormat Will contain the format details.
	 * @return true on success, false otherwise.
	 * @see GetVideoTrackFormat
	 */
	bool GetVideoTrackFormat(int32 TrackIndex, int32 FormatIndex, FMediaVideoTrackFormat& OutFormat) const;

	/**
	 * Process the given media event.
	 *
	 * @param Event The event to process.
	 **/
	void ProcessEvent(EMediaEvent Event);

	/** Select the default media tracks. */
	void SelectDefaultTracks();

protected:

	//~ IMediaEventSink interface

	void ReceiveMediaEvent(EMediaEvent Event) override;

protected:

	/** Fetch audio samples from the player and forward them to the registered sinks. */
	void ProcessAudioSamples(IMediaSamples& Samples, TRange<FTimespan> TimeRange);

	/** Fetch audio samples from the player and forward them to the registered sinks. */
	void ProcessCaptionSamples(IMediaSamples& Samples, TRange<FTimespan> TimeRange);

	/** Fetch metadata samples from the player and forward them to the registered sinks. */
	void ProcessMetadataSamples(IMediaSamples& Samples, TRange<FTimespan> TimeRange);

	/** Fetch subtitle samples from the player and forward them to the registered sinks. */
	void ProcessSubtitleSamples(IMediaSamples& Samples, TRange<FTimespan> TimeRange);

	/** Fetch video samples from the player and forward them to the registered sinks. */
	void ProcessVideoSamples(IMediaSamples& Samples, TRange<FTimespan> TimeRange);

private:

	/** Audio sample sinks. */
	TMediaSampleSinks<IMediaAudioSample> AudioSampleSinks;

	/** Caption sample sinks. */
	TMediaSampleSinks<IMediaOverlaySample> CaptionSampleSinks;

	/** Metadata sample sinks. */
	TMediaSampleSinks<IMediaBinarySample> MetadataSampleSinks;

	/** Subtitle sample sinks. */
	TMediaSampleSinks<IMediaOverlaySample> SubtitleSampleSinks;

	/** Video sample sinks. */
	TMediaSampleSinks<IMediaTextureSample> VideoSampleSinks;

private:

	/** Media sample cache. */
	FMediaSampleCache* Cache;

	/** Synchronizes access to Player. */
	FCriticalSection CriticalSection;

	/** Holds the URL of the currently loaded media source. */
	FString CurrentUrl;

	/** The last used non-zero play rate (zero if playback never started). */
	float LastRate;

	/** An event delegate that is invoked when a media event occurred. */
	FOnMediaEvent MediaEvent;

	/** The low-level player used to play the media source. */
	TSharedPtr<IMediaPlayer, ESPMode::ThreadSafe> Player;

	/** Media player Guid */
	FGuid PlayerGuid;

	/** Media player event queue. */
	TQueue<EMediaEvent, EQueueMode::Mpsc> QueuedEvents;
};
