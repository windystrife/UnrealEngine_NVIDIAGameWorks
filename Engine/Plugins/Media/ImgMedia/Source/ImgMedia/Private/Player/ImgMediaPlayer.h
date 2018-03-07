// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IMediaCache.h"
#include "IMediaControls.h"
#include "IMediaPlayer.h"
#include "IMediaSamples.h"
#include "IMediaTracks.h"
#include "IMediaView.h"

class FImgMediaLoader;
class IImgMediaReader;
class IMediaEventSink;
class IMediaTextureSample;


/**
 * Implements a media player for image sequences.
 */
class FImgMediaPlayer
	: public IMediaPlayer
	, protected IMediaCache
	, protected IMediaControls
	, protected IMediaSamples
	, protected IMediaTracks
	, protected IMediaView
{
public:

	/**
	 * Create and initialize a new instance.
	 *
	 * @param InEventSink The object that receives media events from this player.
	 */
	FImgMediaPlayer(IMediaEventSink& InEventSink);

	/** Virtual destructor. */
	virtual ~FImgMediaPlayer();

public:

	//~ IMediaPlayer interface

	virtual void Close() override;
	virtual IMediaCache& GetCache() override;
	virtual IMediaControls& GetControls() override;
	virtual FString GetInfo() const override;
	virtual FName GetPlayerName() const override;
	virtual IMediaSamples& GetSamples() override;
	virtual FString GetStats() const override;
	virtual IMediaTracks& GetTracks() override;
	virtual FString GetUrl() const override;
	virtual IMediaView& GetView() override;
	virtual bool Open(const FString& Url, const IMediaOptions* Options) override;
	virtual bool Open(const TSharedRef<FArchive, ESPMode::ThreadSafe>& Archive, const FString& OriginalUrl, const IMediaOptions* Options) override;
	virtual void TickInput(FTimespan DeltaTime, FTimespan Timecode) override;

protected:

	/**
	 * Check whether this player is initialized.
	 *
	 * @return true if initialized, false otherwise.
	 */
	bool IsInitialized() const;

protected:

	//~ IMediaCache interface

	virtual bool QueryCacheState(EMediaCacheState State, TRangeSet<FTimespan>& OutTimeRanges) const override;

protected:

	//~ IMediaControls interface

	virtual bool CanControl(EMediaControl Control) const override;
	virtual FTimespan GetDuration() const override;
	virtual float GetRate() const override;
	virtual EMediaState GetState() const override;
	virtual EMediaStatus GetStatus() const override;
	virtual TRangeSet<float> GetSupportedRates(EMediaRateThinning Thinning) const override;
	virtual FTimespan GetTime() const override;
	virtual bool IsLooping() const override;
	virtual bool Seek(const FTimespan& Time) override;
	virtual bool SetLooping(bool Looping) override;
	virtual bool SetRate(float Rate) override;

protected:

	//~ IMediaSamples interface

	virtual bool FetchVideo(TRange<FTimespan> TimeRange, TSharedPtr<IMediaTextureSample, ESPMode::ThreadSafe>& OutSample) override;
	virtual void FlushSamples() override;

protected:

	//~ IMediaTracks interface

	virtual bool GetAudioTrackFormat(int32 TrackIndex, int32 FormatIndex, FMediaAudioTrackFormat& OutFormat) const override;
	virtual int32 GetNumTracks(EMediaTrackType TrackType) const override;
	virtual int32 GetNumTrackFormats(EMediaTrackType TrackType, int32 TrackIndex) const override;
	virtual int32 GetSelectedTrack(EMediaTrackType TrackType) const override;
	virtual FText GetTrackDisplayName(EMediaTrackType TrackType, int32 TrackIndex) const override;
	virtual int32 GetTrackFormat(EMediaTrackType TrackType, int32 TrackIndex) const override;
	virtual FString GetTrackLanguage(EMediaTrackType TrackType, int32 TrackIndex) const override;
	virtual FString GetTrackName(EMediaTrackType TrackType, int32 TrackIndex) const override;
	virtual bool GetVideoTrackFormat(int32 TrackIndex, int32 FormatIndex, FMediaVideoTrackFormat& OutFormat) const override;
	virtual bool SelectTrack(EMediaTrackType TrackType, int32 TrackIndex) override;
	virtual bool SetTrackFormat(EMediaTrackType TrackType, int32 TrackIndex, int32 FormatIndex) override;

private:

	/** The duration of the currently loaded media. */
	FTimespan CurrentDuration;

	/** The current playback rate. */
	float CurrentRate;

	/** The player's current state. */
	EMediaState CurrentState;

	/** The current time of the playback. */
	FTimespan CurrentTime;

	/** The URL of the currently opened media. */
	FString CurrentUrl;

	/** The media event handler. */
	IMediaEventSink& EventSink;

	/** Sample time of the last fetched video sample. */
	FTimespan LastFetchTime;

	/** The image sequence loader. */
	TSharedPtr<FImgMediaLoader, ESPMode::ThreadSafe> Loader;

	/** If playback just restarted from the Stopped state. */
	bool PlaybackRestarted;

	/** Index of the selected video track. */
	int32 SelectedVideoTrack;

	/** Should the video loop to the beginning at completion */
    bool ShouldLoop;
};
