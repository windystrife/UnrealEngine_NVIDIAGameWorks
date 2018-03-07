// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/UnrealString.h"
#include "Delegates/Delegate.h"
#include "Math/Quat.h"
#include "Math/Rotator.h"
#include "Templates/SharedPointer.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/ScriptMacros.h"
#include "Misc/Guid.h"

#include "MediaPlayer.generated.h"

class FMediaPlayerFacade;
class IMediaPlayer;
class UMediaPlaylist;
class UMediaSource;

enum class EMediaEvent;


/** Multicast delegate that is invoked when a media event occurred in the player. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnMediaPlayerMediaEvent);

/** Multicast delegate that is invoked when a media player's media has been opened. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMediaPlayerMediaOpened, FString, OpenedUrl);

/** Multicast delegate that is invoked when a media player's media has failed to open. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMediaPlayerMediaOpenFailed, FString, FailedUrl);


/**
 * Media track types.
 *
 * Note: Keep this in sync with EMediaTrackType
 */
UENUM(BlueprintType)
enum class EMediaPlayerTrack : uint8
{
	/** Audio track. */
	Audio,

	/** Caption track. */
	Caption,

	/** Metadata track. */
	Metadata,

	/** Script track. */
	Script,

	/** Subtitle track. */
	Subtitle,

	/** Text track. */
	Text,

	/** Video track. */
	Video
};


/**
 * Implements a media player asset that can play movies and other media sources.
 */
UCLASS(BlueprintType, hidecategories=(Object))
class MEDIAASSETS_API UMediaPlayer
	: public UObject
{
	GENERATED_UCLASS_BODY()

public:

	/**
	 * Check whether media playback can be paused right now.
	 *
	 * Playback can be paused if the media supports pausing and if it is currently playing.
	 *
	 * @return true if pausing playback can be paused, false otherwise.
	 * @see CanPlay, Pause
	 */
	UFUNCTION(BlueprintCallable, Category="Media|MediaPlayer")
	bool CanPause() const;

	/**
	 * Check whether the specified media source can be played by this player.
	 *
	 * If a desired player name is set for this player, it will only check
	 * whether that particular player type can play the specified source.
	 *
	 * @param MediaSource The media source to check.
	 * @return true if the media source can be opened, false otherwise.
	 * @see CanPlayUrl, SetDesiredPlayerName
	 */
	UFUNCTION(BlueprintCallable, Category = "Media|MediaPlayer")
	bool CanPlaySource(UMediaSource* MediaSource);

	/**
	 * Check whether the specified URL can be played by this player.
	 *
	 * If a desired player name is set for this player, it will only check
	 * whether that particular player type can play the specified URL.
	 *
	 * @param Url The URL to check.
	 * @see CanPlaySource, SetDesiredPlayerName
	 */
	UFUNCTION(BlueprintCallable, Category="Media|MediaPlayer")
	bool CanPlayUrl(const FString& Url);

	/**
	 * Close the currently open media, if any.
	 *
	 * @see OnMediaClosed, OpenPlaylist, OpenPlaylistIndex, OpenSource, OpenUrl, Pause, Play
	 */
	UFUNCTION(BlueprintCallable, Category="Media|MediaPlayer")
	void Close();

	/**
	 * Get the number of channels in the specified audio track.
	 *
	 * @param TrackIndex Index of the audio track, or INDEX_NONE for the selected one.
	 * @param FormatIndex Index of the track format, or INDEX_NONE for the selected one.
	 * @return Number of channels.
	 * @see GetAudioTrackSampleRate, GetAudioTrackType
	 */
	UFUNCTION(BlueprintCallable, Category = "Media|MediaPlayer")
	int32 GetAudioTrackChannels(int32 TrackIndex, int32 FormatIndex) const;

	/**
	 * Get the sample rate of the specified audio track.
	 *
	 * @param TrackIndex Index of the audio track, or INDEX_NONE for the selected one.
	 * @param FormatIndex Index of the track format, or INDEX_NONE for the selected one.
	 * @return Samples per second.
	 * @see GetAudioTrackChannels, GetAudioTrackType
	 */
	UFUNCTION(BlueprintCallable, Category = "Media|MediaPlayer")
	int32 GetAudioTrackSampleRate(int32 TrackIndex, int32 FormatIndex) const;

	/**
	 * Get the type of the specified audio track format.
	 *
	 * @param TrackIndex The index of the track, or INDEX_NONE for the selected one.
	 * @param FormatIndex Index of the track format, or INDEX_NONE for the selected one.
	 * @return Audio format type string.
	 * @see GetAudioTrackSampleRate, GetAudioTrackSampleRate
	 */
	UFUNCTION(BlueprintCallable, Category="Media|MediaPlayer")
	FString GetAudioTrackType(int32 TrackIndex, int32 FormatIndex) const;

	/**
	 * Get the name of the current desired native player.
	 *
	 * @return The name of the desired player, or NAME_None if not set.
	 * @see SetDesiredPlayerName
	 */
	UFUNCTION(BlueprintCallable, Category = "Media|MediaPlayer")
	FName GetDesiredPlayerName() const;

	/**
	 * Get the media's duration.
	 *
	 * @return A time span representing the duration.
	 * @see GetTime, Seek
	 */
	UFUNCTION(BlueprintCallable, Category="Media|MediaPlayer")
	FTimespan GetDuration() const;

	/**
	 * Get the current horizontal field of view (only for 360 videos).
	 *
	 * @return Horizontal field of view (in Euler degrees).
	 * @see GetVerticalFieldOfView, GetViewRotation, SetHorizontalFieldOfView
	 */
	UFUNCTION(BlueprintCallable, Category="Media|MediaPlayer")
	float GetHorizontalFieldOfView() const;

	/**
	 * Get the human readable name of the currently loaded media source.
	 *
	 * @return Media source name, or empty text if no media is opened
	 * @see GetPlayerName, GetUrl
	 */
	UFUNCTION(BlueprintCallable, Category="Media|MediaPlayer")
	virtual FText GetMediaName() const;

	/**
	 * Get the number of tracks of the given type.
	 *
	 * @param TrackType The type of media tracks.
	 * @return Number of tracks.
	 * @see GetNumTrackFormats, GetSelectedTrack, SelectTrack
	 */
	UFUNCTION(BlueprintCallable, Category="Media|MediaPlayer")
	int32 GetNumTracks(EMediaPlayerTrack TrackType) const;

	/**
	 * Get the number of formats of the specified track.
	 *
	 * @param TrackType The type of media tracks.
	 * @param TrackIndex The index of the track.
	 * @return Number of formats.
	 * @see GetNumTracks, GetSelectedTrack, SelectTrack
	 */
	UFUNCTION(BlueprintCallable, Category="Media|MediaPlayer")
	int32 GetNumTrackFormats(EMediaPlayerTrack TrackType, int32 TrackIndex) const;

	/**
	 * Get the name of the current native media player.
	 *
	 * @return Player name, or NAME_None if not available.
	 * @see GetMediaName
	 */
	UFUNCTION(BlueprintCallable, Category="Media|MediaPlayer")
	FName GetPlayerName() const;

	/**
	 * Get the current play list.
	 *
	 * Media players always have a valid play list. In C++ code you can use
	 * the GetPlaylistRef to get a reference instead of a pointer to it.
	 *
	 * @return The play list.
	 * @see GetPlaylistIndex, GetPlaylistRef
	 */
	UFUNCTION(BlueprintCallable, Category="Media|MediaPlayer")
	UMediaPlaylist* GetPlaylist() const
	{
		return Playlist;
	}

	/**
	 * Get the current play list index.
	 *
	 * @return Play list index.
	 * @see GetPlaylist
	 */
	UFUNCTION(BlueprintCallable, Category = "Media|MediaPlayer")
	int32 GetPlaylistIndex() const
	{
		return PlaylistIndex;
	}

	/**
	 * Get the media's current playback rate.
	 *
	 * @return The playback rate.
	 * @see SetRate, SupportsRate
	 */
	UFUNCTION(BlueprintCallable, Category="Media|MediaPlayer")
	float GetRate() const;

	/**
	 * Get the index of the currently selected track of the given type.
	 *
	 * @param TrackType The type of track to get.
	 * @return The index of the selected track, or INDEX_NONE if no track is active.
	 * @see GetNumTracks, GetTrackFormat, SelectTrack
	 */
	UFUNCTION(BlueprintCallable, Category="Media|MediaPlayer")
	int32 GetSelectedTrack(EMediaPlayerTrack TrackType) const;

	/**
	 * Get the supported playback rates.
	 *
	 * @param Unthinned Whether the rates are for unthinned playback.
	 * @param Will contain the the ranges of supported rates.
	 * @see SetRate, SupportsRate
	 */
	UFUNCTION(BlueprintCallable, Category="Media|MediaPlayer")
	void GetSupportedRates(TArray<FFloatRange>& OutRates, bool Unthinned) const;

	/**
	 * Get the media's current playback time.
	 *
	 * @return Playback time.
	 * @see GetDuration, Seek
	 */
	UFUNCTION(BlueprintCallable, Category="Media|MediaPlayer")
	FTimespan GetTime() const;

	/**
	 * Get the human readable name of the specified track.
	 *
	 * @param TrackType The type of track.
	 * @param TrackIndex The index of the track, or INDEX_NONE for the selected one.
	 * @return Display name.
	 * @see GetNumTracks, GetTrackLanguage
	 */
	UFUNCTION(BlueprintCallable, Category = "Media|MediaPlayer")
	FText GetTrackDisplayName(EMediaPlayerTrack TrackType, int32 TrackIndex) const;

	/**
	 * Get the index of the active format of the specified track type.
	 *
	 * @param TrackType The type of track.
	 * @param TrackIndex The index of the track, or INDEX_NONE for the selected one.
	 * @return The index of the selected format.
	 * @see GetNumTrackFormats, GetSelectedTrack, SetTrackFormat
	 */
	UFUNCTION(BlueprintCallable, Category="Media|MediaPlayer")
	int32 GetTrackFormat(EMediaPlayerTrack TrackType, int32 TrackIndex) const;

	/**
	 * Get the language tag of the specified track.
	 *
	 * @param TrackType The type of track.
	 * @param TrackIndex The index of the track, or INDEX_NONE for the selected one.
	 * @return Language tag, i.e. "en-US" for English, or "und" for undefined.
	 * @see GetNumTracks, GetTrackDisplayName
	 */
	UFUNCTION(BlueprintCallable, Category="Media|MediaPlayer")
	FString GetTrackLanguage(EMediaPlayerTrack TrackType, int32 TrackIndex) const;

	/**
	 * Get the URL of the currently loaded media, if any.
	 *
	 * @return Media URL, or empty string if no media was loaded.
	 * @see OpenUrl
	 */
	UFUNCTION(BlueprintCallable, Category="Media|MediaPlayer")
	const FString& GetUrl() const;

	/**
	 * Get the current vertical field of view (only for 360 videos).
	 *
	 * @return Vertical field of view (in Euler degrees), or 0.0 if not available.
	 * @see GetHorizontalFieldOfView, GetViewRotation, SetVerticalFieldOfView
	 */
	UFUNCTION(BlueprintCallable, Category="Media|MediaPlayer")
	float GetVerticalFieldOfView() const;

	/**
	 * Get the aspect ratio of the specified video track.
	 *
	 * @param TrackIndex Index of the video track, or INDEX_NONE for the selected one.
	 * @param FormatIndex Index of the track format, or INDEX_NONE for the selected one.
	 * @return Aspect ratio.
	 * @see GetVideoTrackDimensions, GetVideoTrackFrameRate, GetVideoTrackFrameRates, GetVideoTrackType
	 */
	UFUNCTION(BlueprintCallable, Category = "Media|MediaPlayer")
	float GetVideoTrackAspectRatio(int32 TrackIndex, int32 FormatIndex) const;

	/**
	 * Get the current dimensions of the specified video track.
	 *
	 * @param TrackIndex The index of the track, or INDEX_NONE for the selected one.
	 * @param FormatIndex Index of the track format, or INDEX_NONE for the selected one.
	 * @return Video dimensions (in pixels).
	 * @see GetVideoTrackAspectRatio, GetVideoTrackFrameRate, GetVideoTrackFrameRates, GetVideoTrackType
	 */
	UFUNCTION(BlueprintCallable, Category="Media|MediaPlayer")
	FIntPoint GetVideoTrackDimensions(int32 TrackIndex, int32 FormatIndex) const;

	/**
	 * Get the frame rate of the specified video track.
	 *
	 * @param TrackIndex The index of the track, or INDEX_NONE for the selected one.
	 * @param FormatIndex Index of the track format, or INDEX_NONE for the selected one.
	 * @return Frame rate (in frames per second).
	 * @see GetVideoTrackAspectRatio, GetVideoTrackDimensions, GetVideoTrackFrameRates, GetVideoTrackType, SetVideoTrackFrameRate
	 */
	UFUNCTION(BlueprintCallable, Category="Media|MediaPlayer")
	float GetVideoTrackFrameRate(int32 TrackIndex, int32 FormatIndex) const;

	/**
	 * Get the supported range of frame rates of the specified video track.
	 *
	 * @param TrackIndex The index of the track, or INDEX_NONE for the selected one.
	 * @param FormatIndex Index of the track format, or INDEX_NONE for the selected one.
	 * @return Frame rate range (in frames per second).
	 * @see GetVideoTrackAspectRatio, GetVideoTrackDimensions, GetVideoTrackFrameRate, GetVideoTrackType
	 */
	UFUNCTION(BlueprintCallable, Category="Media|MediaPlayer")
	FFloatRange GetVideoTrackFrameRates(int32 TrackIndex, int32 FormatIndex) const;

	/**
	 * Get the type of the specified video track format.
	 *
	 * @param TrackIndex The index of the track, or INDEX_NONE for the selected one.
	 * @param FormatIndex Index of the track format, or INDEX_NONE for the selected one.
	 * @return Video format type string.
	 * @see GetVideoTrackAspectRatio, GetVideoTrackDimensions, GetVideoTrackFrameRate, GetVideoTrackFrameRates
	 */
	UFUNCTION(BlueprintCallable, Category="Media|MediaPlayer")
	FString GetVideoTrackType(int32 TrackIndex, int32 FormatIndex) const;

	/**
	 * Get the current view rotation (only for 360 videos).
	 *
	 * @return View rotation, or zero rotator if not available.
	 * @see GetHorizontalFieldOfView, GetVerticalFieldOfView, SetViewRotation
	 */
	UFUNCTION(BlueprintCallable, Category="Media|MediaPlayer")
	FRotator GetViewRotation() const;

	/**
	 * Check whether the player is in an error state.
	 *
	 * When the player is in an error state, no further operations are possible.
	 * The current media must be closed, and a new media source must be opened
	 * before the player can be used again. Errors are usually caused by faulty
	 * media files or interrupted network connections.
	 *
	 * @see IsReady
	 */
	UFUNCTION(BlueprintCallable, Category="Media|MediaPlayer")
	bool HasError() const;

	/**
	 * Check whether playback is buffering data.
	 *
	 * @return true if looping, false otherwise.
	 * @see IsConnecting, IsLooping, IsPaused, IsPlaying, IsPreparing, IsReady
	 */
	UFUNCTION(BlueprintCallable, Category="Media|MediaPlayer")
	bool IsBuffering() const;

	/**
	 * Check whether the player is currently connecting to a media source.
	 *
	 * @return true if connecting, false otherwise.
	 * @see IsBuffering, IsLooping, IsPaused, IsPlaying, IsPreparing, IsReady
	 */
	UFUNCTION(BlueprintCallable, Category="Media|MediaPlayer")
	bool IsConnecting() const;

	/**
	 * Check whether playback is looping.
	 *
	 * @return true if looping, false otherwise.
	 * @see IsBuffering, IsConnecting, IsPaused, IsPlaying, IsPreparing, IsReady, SetLooping
	 */
	UFUNCTION(BlueprintCallable, Category="Media|MediaPlayer")
	bool IsLooping() const;

	/**
	 * Check whether playback is currently paused.
	 *
	 * @return true if playback is paused, false otherwise.
	 * @see CanPause, IsBuffering, IsConnecting, IsLooping, IsPaused, IsPlaying, IsPreparing, IsReady, Pause
	 */
	UFUNCTION(BlueprintCallable, Category="Media|MediaPlayer")
	bool IsPaused() const;

	/**
	 * Check whether playback has started.
	 *
	 * @return true if playback has started, false otherwise.
	 * @see CanPlay, IsBuffering, IsConnecting, IsLooping, IsPaused, IsPlaying, IsPreparing, IsReady, Play
	 */
	UFUNCTION(BlueprintCallable, Category="Media|MediaPlayer")
	bool IsPlaying() const;

	/**
	 * Check whether the media is currently opening or buffering.
	 *
	 * @return true if playback is being prepared, false otherwise.
	 * @see CanPlay, IsBuffering, IsConnecting, IsLooping, IsPaused, IsPlaying, IsReady, Play
	 */
	UFUNCTION(BlueprintCallable, Category="Media|MediaPlayer")
	bool IsPreparing() const;

	/**
	 * Check whether media is ready for playback.
	 *
	 * A player is ready for playback if it has a media source opened that
	 * finished preparing and is not in an error state.
	 *
	 * @return true if media is ready, false otherwise.
	 * @see HasError, IsBuffering, IsConnecting, IsLooping, IsPaused, IsPlaying, IsPreparing
	 */
	UFUNCTION(BlueprintCallable, Category="Media|MediaPlayer")
	bool IsReady() const;

	/**
	 * Open the next item in the current play list.
	 *
	 * The player will start playing the new media source if it was playing
	 * something previously, otherwise it will only open the media source.
	 *
	 * @return true on success, false otherwise.
	 * @see Close, OpenUrl, OpenSource, Play, Previous, SetPlaylist
	 */
	UFUNCTION(BlueprintCallable, Category="Media|MediaPlayer")
	bool Next();

	/**
	 * Opens the specified media file path.
	 *
	 * A return value of true indicates that the player will attempt to open
	 * the media, but it may fail to do so later for other reasons, i.e. if
	 * a connection to the media server timed out. Use the OnMediaOpened and
	 * OnMediaOpenFailed delegates to detect if and when the media is ready!
	 *
	 * @param FilePath The file path to open.
	 * @return true if the file path will be opened, false otherwise.
	 * @see GetUrl, Close, OpenPlaylist, OpenPlaylistIndex, OpenSource, OpenUrl, Reopen
	 */
	UFUNCTION(BlueprintCallable, Category="Media|MediaPlayer")
	bool OpenFile(const FString& FilePath);

	/**
	 * Open the first media source in the specified play list.
	 *
	 * @param InPlaylist The play list to open.
	 * @return true if the source will be opened, false otherwise.
	 * @see Close, OpenFile, OpenPlaylistIndex, OpenSource, OpenUrl, Reopen
	 */
	UFUNCTION(BlueprintCallable, Category="Media|MediaPlayer")
	bool OpenPlaylist(UMediaPlaylist* InPlaylist)
	{
		return OpenPlaylistIndex(InPlaylist, 0);
	}

	/**
	 * Open a particular media source in the specified play list.
	 *
	 * @param InPlaylist The play list to open.
	 * @param Index The index of the source to open.
	 * @return true if the source will be opened, false otherwise.
	 * @see Close, OpenFile, OpenPlaylist, OpenSource, OpenUrl, Reopen
	 */
	UFUNCTION(BlueprintCallable, Category="Media|MediaPlayer")
	bool OpenPlaylistIndex(UMediaPlaylist* InPlaylist, int32 Index);

	/**
	 * Open the specified media source.
	 *
	 * A return value of true indicates that the player will attempt to open
	 * the media, but it may fail to do so later for other reasons, i.e. if
	 * a connection to the media server timed out. Use the OnMediaOpened and
	 * OnMediaOpenFailed delegates to detect if and when the media is ready!
	 *
	 * @param MediaSource The media source to open.
	 * @return true if the source will be opened, false otherwise.
	 * @see Close, OpenFile, OpenPlaylist, OpenPlaylistIndex, OpenUrl, Reopen
	 */
	UFUNCTION(BlueprintCallable, Category="Media|MediaPlayer")
	bool OpenSource(UMediaSource* MediaSource);

	/**
	 * Opens the specified media URL.
	 *
	 * A return value of true indicates that the player will attempt to open
	 * the media, but it may fail to do so later for other reasons, i.e. if
	 * a connection to the media server timed out. Use the OnMediaOpened and
	 * OnMediaOpenFailed delegates to detect if and when the media is ready!
	 *
	 * @param Url The URL to open.
	 * @return true if the URL will be opened, false otherwise.
	 * @see GetUrl, Close, OpenFile, OpenPlaylist, OpenPlaylistIndex, OpenSource, Reopen
	 */
	UFUNCTION(BlueprintCallable, Category="Media|MediaPlayer")
	bool OpenUrl(const FString& Url);

	/**
	 * Pauses media playback.
	 *
	 * This is the same as setting the playback rate to 0.0.
	 *
	 * @return true if playback is being paused, false otherwise.
	 * @see CanPause, Close, Next, Play, Previous, Rewind, Seek
	 */
	UFUNCTION(BlueprintCallable, Category="Media|MediaPlayer")
	bool Pause();

	/**
	 * Starts media playback.
	 *
	 * This is the same as setting the playback rate to 1.0.
	 *
	 * @return true if playback is starting, false otherwise.
	 * @see CanPlay, GetRate, Next, Pause, Previous, SetRate
	 */
	UFUNCTION(BlueprintCallable, Category="Media|MediaPlayer")
	bool Play();

	/**
	 * Open the previous item in the current play list.
	 *
	 * The player will start playing the new media source if it was playing
	 * something previously, otherwise it will only open the media source.
	 *
	 * @return true on success, false otherwise.
	 * @see Close, Next, OpenUrl, OpenSource, Play, SetPlaylist
	 */
	UFUNCTION(BlueprintCallable, Category="Media|MediaPlayer")
	bool Previous();

	/**
	 * Reopens the currently opened media or play list.
	 *
	 * @return true if the media will be opened, false otherwise.
	 * @see Close, Open, OpenFile, OpenPlaylist, OpenPlaylistIndex, OpenSource, OpenUrl
	 */
	UFUNCTION(BlueprintCallable, Category="Media|MediaPlayer")
	bool Reopen();

	/**
	 * Rewinds the media to the beginning.
	 *
	 * This is the same as seeking to zero time.
	 *
	 * @return true if rewinding, false otherwise.
	 * @see GetTime, Seek
	 */
	UFUNCTION(BlueprintCallable, Category="Media|MediaPlayer")
	bool Rewind();

	/**
	 * Seeks to the specified playback time.
	 *
	 * @param Time The playback time to set.
	 * @return true on success, false otherwise.
	 * @see GetTime, Rewind
	 */
	UFUNCTION(BlueprintCallable, Category="Media|MediaPlayer")
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
	UFUNCTION(BlueprintCallable, Category="Media|MediaPlayer")
	bool SelectTrack(EMediaPlayerTrack TrackType, int32 TrackIndex);

	/**
	 * Set the name of the desired native player.
	 *
	 * @param PlayerName The name of the player to set.
	 * @see GetDesiredPlayerName
	 */
	UFUNCTION(BlueprintCallable, Category = "Media|MediaPlayer")
	void SetDesiredPlayerName(FName PlayerName);

	/**
	 * Enables or disables playback looping.
	 *
	 * @param Looping Whether playback should be looped.
	 * @return true on success, false otherwise.
	 * @see IsLooping
	 */
	UFUNCTION(BlueprintCallable, Category="Media|MediaPlayer")
	bool SetLooping(bool Looping);

	/**
	 * Changes the media's playback rate.
	 *
	 * @param Rate The playback rate to set.
	 * @return true on success, false otherwise.
	 * @see GetRate, SupportsRate
	 */
	UFUNCTION(BlueprintCallable, Category="Media|MediaPlayer")
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
	UFUNCTION(BlueprintCallable, Category="Media|MediaPlayer")
	bool SetTrackFormat(EMediaPlayerTrack TrackType, int32 TrackIndex, int32 FormatIndex);

	/**
	 * Set the frame rate of the specified video track.
	 *
	 * @param TrackIndex The index of the track, or INDEX_NONE for the selected one.
	 * @param FormatIndex Index of the track format, or INDEX_NONE for the selected one.
	 * @param FrameRate The frame rate to set (must be in range of format's supported frame rates).
	 * @return true on success, false otherwise.
	 * @see GetVideoTrackAspectRatio, GetVideoTrackDimensions, GetVideoTrackFrameRate, GetVideoTrackFrameRates, GetVideoTrackType
	 */
	UFUNCTION(BlueprintCallable, Category="Media|MediaPlayer")
	bool SetVideoTrackFrameRate(int32 TrackIndex, int32 FormatIndex, float FrameRate);

	/**
	 * Set the field of view (only for 360 videos).
	 *
	 * @param Horizontal Horizontal field of view (in Euler degrees).
	 * @param Vertical Vertical field of view (in Euler degrees).
	 * @param Whether the field of view change should be absolute (true) or relative (false).
	 * @return true on success, false otherwise.
	 * @see GetHorizontalFieldOfView, GetVerticalFieldOfView, SetViewRotation
	 */
	UFUNCTION(BlueprintCallable, Category = "Media|MediaPlayer")
	bool SetViewField(float Horizontal, float Vertical, bool Absolute);

	/**
	 * Set the view's rotation (only for 360 videos).
	 *
	 * @param Rotation The desired view rotation.
	 * @param Whether the rotation change should be absolute (true) or relative (false).
	 * @return true on success, false otherwise.
	 * @see GetViewRotation, SetViewField
	 */
	UFUNCTION(BlueprintCallable, Category = "Media|MediaPlayer")
	bool SetViewRotation(const FRotator& Rotation, bool Absolute);

	/**
	 * Check whether the specified playback rate is supported.
	 *
	 * @param Rate The playback rate to check.
	 * @param Unthinned Whether no frames should be dropped at the given rate.
	 * @see SupportsScrubbing, SupportsSeeking
	 */
	UFUNCTION(BlueprintCallable, Category="Media|MediaPlayer")
	bool SupportsRate(float Rate, bool Unthinned) const;

	/**
	 * Check whether the currently loaded media supports scrubbing.
	 *
	 * @return true if scrubbing is supported, false otherwise.
	 * @see SupportsRate, SupportsSeeking
	 */
	UFUNCTION(BlueprintCallable, Category="Media|MediaPlayer")
	bool SupportsScrubbing() const;

	/**
	 * Check whether the currently loaded media can jump to a certain position.
	 *
	 * @return true if seeking is supported, false otherwise.
	 * @see SupportsRate, SupportsScrubbing
	 */
	UFUNCTION(BlueprintCallable, Category="Media|MediaPlayer")
	bool SupportsSeeking() const;

public:

	/** A delegate that is invoked when playback has reached the end of the media. */
	UPROPERTY(BlueprintAssignable, Category="Media|MediaPlayer")
	FOnMediaPlayerMediaEvent OnEndReached;

	/** A delegate that is invoked when a media source has been closed. */
	UPROPERTY(BlueprintAssignable, Category="Media|MediaPlayer")
	FOnMediaPlayerMediaEvent OnMediaClosed;

	/**
	 * A delegate that is invoked when a media source has been opened.
	 *
	 * Depending on whether the underlying player implementation opens the media
	 * synchronously or asynchronously, this event may be executed before or
	 * after the call to OpenSource / OpenUrl returns.
	 *
	 * @see OnMediaOpenFailed, OnTracksChanged
	 */
	UPROPERTY(BlueprintAssignable, Category="Media|MediaPlayer")
	FOnMediaPlayerMediaOpened OnMediaOpened;

	/**
	 * A delegate that is invoked when a media source has failed to open.
	 *
	 * This delegate is only executed if OpenSource / OpenUrl returned true and
	 * the media failed to open asynchronously later. It is not executed if
	 * OpenSource / OpenUrl returned false, indicating an immediate failure.
	 *
	 * @see OnMediaOpened
	 */
	UPROPERTY(BlueprintAssignable, Category="Media|MediaPlayer")
	FOnMediaPlayerMediaOpenFailed OnMediaOpenFailed;

	/**
	 * A delegate that is invoked when media playback has been resumed.
	 *
	 * @see OnPlaybackSuspended
	 */
	UPROPERTY(BlueprintAssignable, Category="Media|MediaPlayer")
	FOnMediaPlayerMediaEvent OnPlaybackResumed;

	/**
	 * A delegate that is invoked when media playback has been suspended.
	 *
	 * @see OnPlaybackResumed
	 */
	UPROPERTY(BlueprintAssignable, Category="Media|MediaPlayer")
	FOnMediaPlayerMediaEvent OnPlaybackSuspended;

	/**
	 * A delegate that is invoked when a seek operation completed successfully.
	 *
	 * Depending on whether the underlying player implementation performs seeks
	 * synchronously or asynchronously, this event may be executed before or
	 * after the call to Seek returns.
	 */
	UPROPERTY(BlueprintAssignable, Category="Media|MediaPlayer")
	FOnMediaPlayerMediaEvent OnSeekCompleted;

	/**
	 * A delegate that is invoked when the media track collection changed.
	 *
	 * @see OnMediaOpened
	 */
	UPROPERTY(BlueprintAssignable, Category="Media|MediaPlayer")
	FOnMediaPlayerMediaEvent OnTracksChanged;

public:

	/**
	 * Get the Guid associated with this media player
	 *
	 * @return The Guid.
	 */
	const FGuid& GetGuid()
	{
		return PlayerGuid;
	}

	/**
	 * Get the media player facade that manages low-level media players
	 *
	 * @return The media player facade.
	 */
	TSharedRef<FMediaPlayerFacade, ESPMode::ThreadSafe> GetPlayerFacade() const;

	/**
	 * Get the current play list.
	 *
	 * @return The play list.
	 * @see GetPlaylistIndex, GetPlaylist
	 */
	UMediaPlaylist& GetPlaylistRef() const
	{
		check(Playlist != nullptr);
		return *Playlist;
	}

	/**
	 * Get an event delegate that is invoked when a media event occurred.
	 *
	 * @return The delegate.
	 */
	DECLARE_EVENT_OneParam(UMediaPlayer, FOnMediaEvent, EMediaEvent /*Event*/)
	FOnMediaEvent& OnMediaEvent()
	{
		return MediaEvent;
	}

#if WITH_EDITOR
	/**
	 * Called when PIE has been paused.
	 *
	 * @see ResumePIE
	 */
	void PausePIE();

	/**
	 * Called when PIE has been resumed.
	 *
	 * @see PausePIE
	 */
	void ResumePIE();
#endif

public:

	//~ UObject interface

	virtual void BeginDestroy() override;
	virtual bool CanBeInCluster() const override;
	virtual FString GetDesc() override;
	virtual void PostDuplicate(bool bDuplicateForPIE) override;
	virtual void PostInitProperties() override;
	virtual void PostLoad() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

public:

	/**
	 * Duration of samples to cache ahead of the play head.
	 *
	 * @see CacheBehind, CacheBehindGame
	 */
	UPROPERTY(BlueprintReadWrite, Category=Caching)
	FTimespan CacheAhead;

	/**
	 * Duration of samples to cache behind the play head (when not running as game).
	 *
	 * @see CacheAhead, CacheBehindGame
	 */
	UPROPERTY(BlueprintReadWrite, Category=Caching)
	FTimespan CacheBehind;

	/**
	 * Duration of samples to cache behind the play head (when running as game).
	 *
	 * @see CacheAhead, CacheBehind
	 */
	UPROPERTY(BlueprintReadWrite, Category=Caching)
	FTimespan CacheBehindGame;

public:

	/**
	 * Output any audio via the operating system's sound mixer instead of a Sound Wave asset.
	 *
	 * If enabled, the assigned Sound Wave asset will be ignored. The SetNativeVolume
	 * function can then be used to change the audio output volume at runtime. Note that
	 * not all media player plug-ins may support native audio output on all platforms.
	 *
	 * @see SetNativeVolume
	 */
	UPROPERTY(BlueprintReadWrite, Category=Output, AdvancedDisplay)
	bool NativeAudioOut;

public:

	/**
	 * Automatically start playback after media opened successfully.
	 *
	 * If disabled, listen to the OnMediaOpened Blueprint event to detect when
	 * the media finished opening, and then start playback using the Play function.
	 *
	 * @see OpenFile, OpenPlaylist, OpenPlaylistIndex, OpenSource, OpenUrl, Play
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Playback)
	bool PlayOnOpen;

	/**
	 * Whether playback should shuffle media sources in the play list.
	 *
	 * @see OpenPlaylist, OpenPlaylistIndex
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Playback)
	uint32 Shuffle : 1;

protected:

	/**
	 * Whether the player should loop when media playback reaches the end.
	 *
	 * Use the SetLooping function to change this value at runtime.
	 *
	 * @see IsLooping, SetLooping
	 */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category=Playback)
	uint32 Loop:1;

	/**
	 * The play list to use, if any.
	 *
	 * Use the OpenPlaylist or OpenPlaylistIndex function to change this value at runtime.
	 *
	 * @see OpenPlaylist, OpenPlaylistIndex
	 */
	UPROPERTY(BlueprintReadOnly, transient, Category=Playback)
	UMediaPlaylist* Playlist;

	/**
	 * The current index of the source in the play list being played.
	 *
	 * Use the Previous and Next methods to change this value at runtime.
	 *
	 * @see Next, Previous
	 */
	UPROPERTY(BlueprintReadOnly, Category=Playback)
	int32 PlaylistIndex;

protected:

	/**
	 * The initial horizontal field of view (in Euler degrees; default = 90).
	 *
	 * This setting is used only for 360 videos. It determines the portion of the
	 * video that is visible at a time. To modify the field of view at runtime in
	 * Blueprints, use the SetHorizontalFieldOfView function.
	 *
	 * @see GetHorizontalFieldOfView, SetHorizontalFieldOfView, VerticalFieldOfView, ViewRotation
	 */
	UPROPERTY(EditAnywhere, Category=ViewSettings)
	float HorizontalFieldOfView;

	/**
	 * The initial vertical field of view (in Euler degrees; default = 60).
	 *
	 * This setting is used only for 360 videos. It determines the portion of the
	 * video that is visible at a time. To modify the field of view at runtime in
	 * Blueprints, use the SetHorizontalFieldOfView function.
	 *
	 * Please note that some 360 video players may be able to change only the
	 * horizontal field of view, and this setting may be ignored.
	 *
	 * @see GetVerticalFieldOfView, SetVerticalFieldOfView, HorizontalFieldOfView, ViewRotation
	 */
	UPROPERTY(EditAnywhere, Category=ViewSettings)
	float VerticalFieldOfView;

	/**
	 * The initial view rotation.
	 *
	 * This setting is used only for 360 videos. It determines the rotation of
	 * the video's view. To modify the view orientation at runtime in Blueprints,
	 * use the GetViewRotation and SetViewRotation functions.
	 *
	 * Please note that not all players may support video view rotations.
	 *
	 * @see GetViewRotation, SetViewRotation, HorizontalFieldOfView, VerticalFieldOfView
	 */
	UPROPERTY(EditAnywhere, Category=ViewSettings)
	FRotator ViewRotation;

private:

	/** Callback for when a media event occurred in the player. */
	void HandlePlayerMediaEvent(EMediaEvent Event);

private:

	/** An event delegate that is invoked when a media event occurred. */
	FOnMediaEvent MediaEvent;

	/** The player facade. */
	TSharedPtr<FMediaPlayerFacade, ESPMode::ThreadSafe> PlayerFacade;

	/** The player's globally unique identifier. */
	UPROPERTY()
	FGuid PlayerGuid;

	/** Automatically start playback of next item in play list. */
	bool PlayOnNext;

#if WITH_EDITOR
	/** Whether the player was playing in PIE/SIE. */
	bool WasPlayingInPIE;
#endif
};
