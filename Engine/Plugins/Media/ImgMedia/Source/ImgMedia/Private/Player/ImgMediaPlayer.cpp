// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "ImgMediaPlayer.h"
#include "ImgMediaPrivate.h"

#include "Async/Async.h"
#include "IMediaEventSink.h"
#include "IMediaOptions.h"
#include "Misc/Paths.h"
#include "UObject/Class.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/WeakObjectPtr.h"

#include "ImgMediaLoader.h"
#include "ImgMediaSettings.h"
#include "ImgMediaTextureSample.h"

#define LOCTEXT_NAMESPACE "FImgMediaPlayer"


/* FExrVideoPlayer structors
 *****************************************************************************/

FImgMediaPlayer::FImgMediaPlayer(IMediaEventSink& InEventSink)
	: CurrentDuration(FTimespan::Zero())
	, CurrentRate(0.0f)
	, CurrentState(EMediaState::Closed)
	, CurrentTime(FTimespan::Zero())
	, EventSink(InEventSink)
	, LastFetchTime(FTimespan::MinValue())
	, PlaybackRestarted(false)
	, SelectedVideoTrack(INDEX_NONE)
{ }


FImgMediaPlayer::~FImgMediaPlayer()
{
	Close();
}


/* IMediaPlayer interface
 *****************************************************************************/

void FImgMediaPlayer::Close()
{
	if (!Loader.IsValid())
	{
		return;
	}

	Loader.Reset();

	CurrentDuration = FTimespan::Zero();
	CurrentUrl.Empty();
	CurrentRate = 0.0f;
	CurrentState = EMediaState::Closed;
	CurrentTime = FTimespan::Zero();
	LastFetchTime = FTimespan::MinValue();
	PlaybackRestarted = false;
	SelectedVideoTrack = INDEX_NONE;

	EventSink.ReceiveMediaEvent(EMediaEvent::TracksChanged);
	EventSink.ReceiveMediaEvent(EMediaEvent::MediaClosed);
}


IMediaCache& FImgMediaPlayer::GetCache()
{
	return *this;
}


IMediaControls& FImgMediaPlayer::GetControls()
{
	return *this;
}


FString FImgMediaPlayer::GetInfo() const
{
	return Loader.IsValid() ? Loader->GetInfo() : FString();
}


FName FImgMediaPlayer::GetPlayerName() const
{
	static FName PlayerName(TEXT("ImgMedia"));
	return PlayerName;
}


IMediaSamples& FImgMediaPlayer::GetSamples()
{
	return *this;
}


FString FImgMediaPlayer::GetStats() const
{
	FString StatsString;
	{
		StatsString += TEXT("not implemented yet");
		StatsString += TEXT("\n");
	}

	return StatsString;
}


IMediaTracks& FImgMediaPlayer::GetTracks()
{
	return *this;
}


FString FImgMediaPlayer::GetUrl() const
{
	return CurrentUrl;
}


IMediaView& FImgMediaPlayer::GetView()
{
	return *this;
}


bool FImgMediaPlayer::Open(const FString& Url, const IMediaOptions* Options)
{
	Close();

	if (Url.IsEmpty() || !Url.StartsWith(TEXT("img://")))
	{
		return false;
	}

	CurrentState = EMediaState::Preparing;
	CurrentUrl = Url;

	// determine image sequence proxy, if any
	FString Proxy;
	
	if (Options != nullptr)
	{
		Proxy = Options->GetMediaOption(ImgMedia::ProxyOverrideOption, FString());
	}

	if (Proxy.IsEmpty())
	{
		Proxy = GetDefault<UImgMediaSettings>()->GetDefaultProxy();
	}

	// initialize image loader on a separate thread
	Loader = MakeShared<FImgMediaLoader, ESPMode::ThreadSafe>();

	const float FpsOverride = (Options != nullptr) ? Options->GetMediaOption(ImgMedia::FramesPerSecondOverrideOption, 0.0f) : 0.0f;
	const FString SequencePath = Proxy.IsEmpty() ? Url.RightChop(6) : FPaths::Combine(&Url[6], Proxy);

	Async<void>(EAsyncExecution::ThreadPool, [FpsOverride, LoaderPtr = TWeakPtr<FImgMediaLoader, ESPMode::ThreadSafe>(Loader), SequencePath]()
	{
		TSharedPtr<FImgMediaLoader, ESPMode::ThreadSafe> PinnedLoader = LoaderPtr.Pin();

		if (PinnedLoader.IsValid())
		{
			PinnedLoader->Initialize(SequencePath, FpsOverride);
		}
	});

	return true;
}


bool FImgMediaPlayer::Open(const TSharedRef<FArchive, ESPMode::ThreadSafe>& /*Archive*/, const FString& /*OriginalUrl*/, const IMediaOptions* /*Options*/)
{
	return false; // not supported
}


void FImgMediaPlayer::TickInput(FTimespan DeltaTime, FTimespan /*Timecode*/)
{
	if (!Loader.IsValid() || (CurrentState == EMediaState::Error))
	{
		return;
	}

	// finalize loader initialization
	if ((CurrentState == EMediaState::Preparing) && Loader->IsInitialized())
	{
		if (Loader->GetSequenceDim().GetMin() == 0)
		{
			CurrentState = EMediaState::Error;

			EventSink.ReceiveMediaEvent(EMediaEvent::MediaOpenFailed);
		}
		else
		{
			CurrentDuration = Loader->GetSequenceDuration();
			CurrentState = EMediaState::Stopped;

			EventSink.ReceiveMediaEvent(EMediaEvent::TracksChanged);
			EventSink.ReceiveMediaEvent(EMediaEvent::MediaOpened);
		}
	}

	if ((CurrentState != EMediaState::Playing) || (CurrentDuration == FTimespan::Zero()))
	{
		return; // nothing to play
	}

	// update clock
	if (PlaybackRestarted)
	{
		PlaybackRestarted = false;
	}
	else
	{
		CurrentTime += DeltaTime * CurrentRate;
	}

	// handle looping
	if ((CurrentTime >= CurrentDuration) || (CurrentTime < FTimespan::Zero()))
	{
		EventSink.ReceiveMediaEvent(EMediaEvent::PlaybackEndReached);

		if (ShouldLoop)
		{
			CurrentTime %= CurrentDuration;

			if (CurrentTime < FTimespan::Zero())
			{
				CurrentTime += CurrentDuration;
			}
		}
		else
		{
			CurrentState = EMediaState::Stopped;
			CurrentTime = FTimespan::Zero();
			CurrentRate = 0.0f;

			EventSink.ReceiveMediaEvent(EMediaEvent::PlaybackSuspended);
		}
	}

	// update image loader
	if (SelectedVideoTrack != INDEX_NONE)
	{
		Loader->RequestFrame(CurrentTime, CurrentRate);
	}
}


/* FImgMediaPlayer implementation
 *****************************************************************************/

bool FImgMediaPlayer::IsInitialized() const
{
	return
		(CurrentState != EMediaState::Closed) &&
		(CurrentState != EMediaState::Error) &&
		(CurrentState != EMediaState::Preparing);
}


/* IMediaCache interface
 *****************************************************************************/

bool FImgMediaPlayer::QueryCacheState(EMediaCacheState State, TRangeSet<FTimespan>& OutTimeRanges) const
{
	if (!Loader.IsValid())
	{
		return false;
	}

	if (State == EMediaCacheState::Loading)
	{
		Loader->GetBusyTimeRanges(OutTimeRanges);
	}
	else if (State == EMediaCacheState::Loaded)
	{
		Loader->GetCompletedTimeRanges(OutTimeRanges);
	}
	else if (State == EMediaCacheState::Pending)
	{
		Loader->GetPendingTimeRanges(OutTimeRanges);
	}
	else
	{
		return false;
	}

	return true;
}


/* IMediaControls interface
 *****************************************************************************/

bool FImgMediaPlayer::CanControl(EMediaControl Control) const
{
	if (!IsInitialized())
	{
		return false;
	}

	if (Control == EMediaControl::Pause)
	{
		return (CurrentState == EMediaState::Playing);
	}

	if (Control == EMediaControl::Resume)
	{
		return (CurrentState != EMediaState::Playing);
	}

	if ((Control == EMediaControl::Scrub) || (Control == EMediaControl::Seek))
	{
		return true;
	}

	return false;
}


FTimespan FImgMediaPlayer::GetDuration() const
{
	return CurrentDuration;
}


float FImgMediaPlayer::GetRate() const
{
	return CurrentRate;
}


EMediaState FImgMediaPlayer::GetState() const
{
	return CurrentState;
}


EMediaStatus FImgMediaPlayer::GetStatus() const
{
	return EMediaStatus::None;
}


TRangeSet<float> FImgMediaPlayer::GetSupportedRates(EMediaRateThinning /*Thinning*/) const
{
	TRangeSet<float> Result;

	if (IsInitialized())
	{
		Result.Add(TRange<float>::Inclusive(-100000.0f, 100000.0f));
	}

	return Result;
}


FTimespan FImgMediaPlayer::GetTime() const
{
	return CurrentTime;
}


bool FImgMediaPlayer::IsLooping() const
{
	return ShouldLoop;
}


bool FImgMediaPlayer::Seek(const FTimespan& Time)
{
	// validate seek
	if (!IsInitialized())
	{
		UE_LOG(LogImgMedia, Warning, TEXT("Cannot seek while player is not ready"));
		return false;
	}

	if ((Time < FTimespan::Zero()) || (Time > CurrentDuration))
	{
		UE_LOG(LogImgMedia, Warning, TEXT("Invalid seek time %s (media duration is %s)"), *Time.ToString(), *CurrentDuration.ToString());
		return false;
	}

	// scrub to desired time if needed
	if (CurrentState == EMediaState::Stopped)
	{
		CurrentState = EMediaState::Paused;
	}

	CurrentTime = Time;
	LastFetchTime = FTimespan::MinValue();

	EventSink.ReceiveMediaEvent(EMediaEvent::SeekCompleted);

	return true;
}


bool FImgMediaPlayer::SetLooping(bool Looping)
{
	ShouldLoop = Looping;

	return true;
}


bool FImgMediaPlayer::SetRate(float Rate)
{
	if (!IsInitialized())
	{
		UE_LOG(LogImgMedia, Warning, TEXT("Cannot set play rate while player is not ready"));
		return false;
	}

	if (Rate == CurrentRate)
	{
		return true; // rate already set
	}

	if (CurrentDuration == FTimespan::Zero())
	{
		return false; // nothing to play
	}

	// handle restarting
	if ((CurrentRate == 0.0f) && (Rate != 0.0f))
	{
		if (CurrentState == EMediaState::Stopped)
		{
			if (Rate < 0.0f)
			{
				CurrentTime = CurrentDuration - FTimespan(1);
			}

			PlaybackRestarted = true;
		}

		CurrentRate = Rate;
		CurrentState = EMediaState::Playing;

		EventSink.ReceiveMediaEvent(EMediaEvent::PlaybackResumed);

		return true;
	}
	
	// handle pausing
	if ((CurrentRate != 0.0f) && (Rate == 0.0f))
	{
		CurrentRate = Rate;
		CurrentState = EMediaState::Paused;

		EventSink.ReceiveMediaEvent(EMediaEvent::PlaybackSuspended);

		return true;
	}

	CurrentRate = Rate;

	return true;
}


/* IMediaSamples interface
 *****************************************************************************/

bool FImgMediaPlayer::FetchVideo(TRange<FTimespan> TimeRange, TSharedPtr<IMediaTextureSample, ESPMode::ThreadSafe>& OutSample)
{
	if ((CurrentState != EMediaState::Paused) && (CurrentState != EMediaState::Playing))
	{
		return false; // nothing to play
	}

	if (SelectedVideoTrack != 0)
	{
		return false; // no video track selected
	}

	TSharedPtr<IMediaTextureSample, ESPMode::ThreadSafe> Sample = Loader->GetFrameSample(CurrentTime);

	if (!Sample.IsValid())
	{
		return false; // sample not loaded yet
	}

	const FTimespan SampleTime = Sample->GetTime();

	if (SampleTime == LastFetchTime)
	{
		return false; // sample already fetched
	}

	LastFetchTime = SampleTime;
	OutSample = Sample;

	return true;
}


void FImgMediaPlayer::FlushSamples()
{
	LastFetchTime = FTimespan::MinValue();
}


/* IMediaTracks interface
 *****************************************************************************/

bool FImgMediaPlayer::GetAudioTrackFormat(int32 TrackIndex, int32 FormatIndex, FMediaAudioTrackFormat& OutFormat) const
{
	return false; // not supported
}


int32 FImgMediaPlayer::GetNumTracks(EMediaTrackType TrackType) const
{
	return (Loader.IsValid() && (TrackType == EMediaTrackType::Video)) ? 1 : 0;
}


int32 FImgMediaPlayer::GetNumTrackFormats(EMediaTrackType TrackType, int32 TrackIndex) const
{
	return ((TrackIndex == 0) && (GetNumTracks(TrackType) > 0)) ? 1 : 0;
}


int32 FImgMediaPlayer::GetSelectedTrack(EMediaTrackType TrackType) const
{
	if (!IsInitialized() || (TrackType != EMediaTrackType::Video))
	{
		return INDEX_NONE;
	}

	return SelectedVideoTrack;
}


FText FImgMediaPlayer::GetTrackDisplayName(EMediaTrackType TrackType, int32 TrackIndex) const
{
	if (!IsInitialized() || (TrackType != EMediaTrackType::Video) || (TrackIndex != 0))
	{
		return FText::GetEmpty();
	}

	return LOCTEXT("DefaultVideoTrackName", "Video Track");
}


int32 FImgMediaPlayer::GetTrackFormat(EMediaTrackType TrackType, int32 TrackIndex) const
{
	return (GetSelectedTrack(TrackType) != INDEX_NONE) ? 0 : INDEX_NONE;
}


FString FImgMediaPlayer::GetTrackLanguage(EMediaTrackType TrackType, int32 TrackIndex) const
{
	if (!IsInitialized() || (TrackType != EMediaTrackType::Video) || (TrackIndex != 0))
	{
		return FString();
	}

	return TEXT("und");
}


FString FImgMediaPlayer::GetTrackName(EMediaTrackType TrackType, int32 TrackIndex) const
{
	if (!IsInitialized() || (TrackType != EMediaTrackType::Video) || (TrackIndex != 0))
	{
		return FString();
	}

	return TEXT("VideoTrack");
}


bool FImgMediaPlayer::GetVideoTrackFormat(int32 TrackIndex, int32 FormatIndex, FMediaVideoTrackFormat& OutFormat) const
{
	if (!IsInitialized() || (TrackIndex != 0) || (FormatIndex != 0))
	{
		return false;
	}

	OutFormat.Dim = Loader->GetSequenceDim();
	OutFormat.FrameRate = Loader->GetSequenceFps();
	OutFormat.FrameRates = TRange<float>(OutFormat.FrameRate);
	OutFormat.TypeName = TEXT("Image"); // @todo gmp: fix me (should be image type)

	return true;
}


bool FImgMediaPlayer::SelectTrack(EMediaTrackType TrackType, int32 TrackIndex)
{
	if (!IsInitialized() || (TrackType != EMediaTrackType::Video))
	{
		return false;
	}

	if ((TrackIndex != 0) && (TrackIndex != INDEX_NONE))
	{
		return false;
	}

	SelectedVideoTrack = TrackIndex;

	return true;
}


bool FImgMediaPlayer::SetTrackFormat(EMediaTrackType TrackType, int32 TrackIndex, int32 FormatIndex)
{
	return (IsInitialized() && (TrackIndex == 0) && (FormatIndex == 0));
}
