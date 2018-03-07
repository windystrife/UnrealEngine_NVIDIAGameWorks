// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AvfMediaPlayer.h"
#include "AvfMediaPrivate.h"

#include "HAL/PlatformProcess.h"
#include "IMediaEventSink.h"
#include "MediaSamples.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "Misc/CoreDelegates.h"

#if PLATFORM_MAC
	#include "Mac/CocoaThread.h"
#else
	#include "IOS/IOSAsyncTask.h"
#endif

#include "AvfMediaTracks.h"
#include "AvfMediaUtils.h"


/* FAVPlayerDelegate
 *****************************************************************************/

/**
 * Cocoa class that can help us with reading player item information.
 */
@interface FAVPlayerDelegate : NSObject
{
};

/** We should only initiate a helper with a media player */
-(FAVPlayerDelegate*) initWithMediaPlayer:(FAvfMediaPlayer*)InPlayer;

/** Destructor */
-(void)dealloc;

/** Notification called when player item reaches the end of playback. */
-(void)playerItemPlaybackEndReached:(NSNotification*)Notification;

/** Reference to the media player which will be responsible for this media session */
@property FAvfMediaPlayer* MediaPlayer;

/** Flag indicating whether the media player item has reached the end of playback */
@property bool bHasPlayerReachedEnd;

@end


@implementation FAVPlayerDelegate
@synthesize MediaPlayer;


-(FAVPlayerDelegate*) initWithMediaPlayer:(FAvfMediaPlayer*)InPlayer
{
	id Self = [super init];
	if (Self)
	{
		MediaPlayer = InPlayer;
	}	
	return Self;
}


/** Listener for changes in our media classes properties. */
- (void) observeValueForKeyPath:(NSString*)keyPath
		ofObject:	(id)object
		change:		(NSDictionary*)change
		context:	(void*)context
{
	if ([keyPath isEqualToString:@"status"])
	{
		if (object == (id)context)
		{
			AVPlayerItemStatus Status = ((AVPlayerItem*)object).status;
			MediaPlayer->OnStatusNotification(Status);
		}
	}
}


- (void)dealloc
{
	[super dealloc];
}


-(void)playerItemPlaybackEndReached:(NSNotification*)Notification
{
	MediaPlayer->OnEndReached();
}

@end


/* FAvfMediaPlayer structors
 *****************************************************************************/

FAvfMediaPlayer::FAvfMediaPlayer(IMediaEventSink& InEventSink)
	: EventSink(InEventSink)
{
	CurrentRate = 0.0f;
	CurrentState = EMediaState::Closed;
    CurrentTime = FTimespan::Zero();

	Duration = FTimespan::Zero();
	MediaUrl = FString();
	ShouldLoop = false;
    
	MediaHelper = nil;
    MediaPlayer = nil;
	PlayerItem = nil;
		
	bPrerolled = false;

	Samples = new FMediaSamples;
	Tracks = new FAvfMediaTracks(*Samples);
}


FAvfMediaPlayer::~FAvfMediaPlayer()
{
	Close();

	delete Samples;
	Samples = nullptr;
}


/* FAvfMediaPlayer interface
 *****************************************************************************/

void FAvfMediaPlayer::OnEndReached()
{
	if (ShouldLoop)
	{
		PlayerTasks.Enqueue([=]() {
			EventSink.ReceiveMediaEvent(EMediaEvent::PlaybackEndReached);
			Seek(FTimespan::FromSeconds(0.0f));
			SetRate(CurrentRate);
		});
	}
	else
	{
		CurrentState = EMediaState::Paused;
		CurrentRate = 0.0f;

		PlayerTasks.Enqueue([=]() {
			Seek(FTimespan::FromSeconds(0.0f));
			EventSink.ReceiveMediaEvent(EMediaEvent::PlaybackEndReached);
			EventSink.ReceiveMediaEvent(EMediaEvent::PlaybackSuspended);
		});
	}
}


void FAvfMediaPlayer::OnStatusNotification(AVPlayerItemStatus Status)
{
	switch(Status)
	{
		case AVPlayerItemStatusReadyToPlay:
		{
			if (Duration == 0.0f || CurrentState == EMediaState::Closed)
			{
				PlayerTasks.Enqueue([=]() {
					Tracks->Initialize(PlayerItem, Info);
					
					EventSink.ReceiveMediaEvent(EMediaEvent::TracksChanged);
				});
				
				PlayerTasks.Enqueue([=]() {
					Duration = FTimespan::FromSeconds(CMTimeGetSeconds(PlayerItem.asset.duration));
					CurrentState = (CurrentState == EMediaState::Closed) ? EMediaState::Preparing : CurrentState;

					if (!bPrerolled)
					{
						// Preroll for playback.
						[MediaPlayer prerollAtRate:1.0f completionHandler:^(BOOL bFinished)
						{
							bPrerolled = true;
							if (bFinished)
							{
								CurrentState = EMediaState::Stopped;
								PlayerTasks.Enqueue([=]() {
									EventSink.ReceiveMediaEvent(EMediaEvent::MediaOpened);
									if (CurrentTime != FTimespan::Zero())
									{
										Seek(CurrentTime);
									}
									if(CurrentRate != 0.0f)
									{
										SetRate(CurrentRate);
									}
								});
							}
							else
							{
								CurrentState = EMediaState::Error;
								PlayerTasks.Enqueue([=]() {
									EventSink.ReceiveMediaEvent(EMediaEvent::MediaOpenFailed);
								});
							}
						}];
					}
				});
			}
			break;
		}
		case AVPlayerItemStatusFailed:
		{
			if (Duration == 0.0f || CurrentState == EMediaState::Closed)
			{
				CurrentState = EMediaState::Error;
				PlayerTasks.Enqueue([=]() {
					EventSink.ReceiveMediaEvent(EMediaEvent::MediaOpenFailed);
				});
			}
			else
			{
				CurrentState = EMediaState::Error;
				PlayerTasks.Enqueue([=]() {
					EventSink.ReceiveMediaEvent(EMediaEvent::PlaybackSuspended);
				});
			}
			break;
		}
		case AVPlayerItemStatusUnknown:
		default:
			break;
	}
}


/* IMediaPlayer interface
 *****************************************************************************/

void FAvfMediaPlayer::Close()
{
	if (CurrentState == EMediaState::Closed)
	{
		return;
	}

    if (ResumeHandle.IsValid())
    {
        FCoreDelegates::ApplicationHasEnteredForegroundDelegate.Remove(ResumeHandle);
        ResumeHandle.Reset();
    }
    
    if (PauseHandle.IsValid())
    {
        FCoreDelegates::ApplicationWillEnterBackgroundDelegate.Remove(PauseHandle);
        PauseHandle.Reset();
    }
    
	CurrentTime = 0;
	MediaUrl = FString();
	
	if (PlayerItem != nil)
	{
		if (MediaHelper != nil)
		{
			[[NSNotificationCenter defaultCenter] removeObserver:MediaHelper name:AVPlayerItemDidPlayToEndTimeNotification object:PlayerItem];
		}

		[PlayerItem removeObserver:MediaHelper forKeyPath:@"status"];
		[PlayerItem release];
		PlayerItem = nil;
	}
	
	if (MediaHelper != nil)
	{
		[MediaHelper release];
		MediaHelper = nil;
	}

	if (MediaPlayer != nil)
	{
		// If we don't remove the current player item then the retain count is > 1 for the MediaPlayer then on it's release then the MetalPlayer stays around forever
		[MediaPlayer replaceCurrentItemWithPlayerItem:nil];
		[MediaPlayer release];
		MediaPlayer = nil;
	}
	
	Tracks->Reset();
	EventSink.ReceiveMediaEvent(EMediaEvent::TracksChanged);

	CurrentState = EMediaState::Closed;
	Duration = CurrentTime = FTimespan::Zero();
	Info.Empty();
	
	EventSink.ReceiveMediaEvent(EMediaEvent::MediaClosed);
		
	bPrerolled = false;
	
	CurrentRate = 0.f;
}


IMediaCache& FAvfMediaPlayer::GetCache()
{
	return *this;
}


IMediaControls& FAvfMediaPlayer::GetControls()
{
	return *this;
}


FString FAvfMediaPlayer::GetInfo() const
{
	return Info;
}


FName FAvfMediaPlayer::GetPlayerName() const
{
	static FName PlayerName(TEXT("AvfMedia"));
	return PlayerName;
}


IMediaSamples& FAvfMediaPlayer::GetSamples()
{
	return *Samples;
}


FString FAvfMediaPlayer::GetStats() const
{
	FString Result;

	Tracks->AppendStats(Result);

	return Result;
}


IMediaTracks& FAvfMediaPlayer::GetTracks()
{
	return *Tracks;
}


FString FAvfMediaPlayer::GetUrl() const
{
	return MediaUrl;
}


IMediaView& FAvfMediaPlayer::GetView()
{
	return *this;
}


bool FAvfMediaPlayer::Open(const FString& Url, const IMediaOptions* /*Options*/)
{
	Close();

	NSURL* nsMediaUrl = nil;
	FString Path;

	if (Url.StartsWith(TEXT("file://")))
	{
		// Media Framework doesn't percent encode the URL, so the path portion is just a native file path.
		// Extract it and then use it create a proper URL.
		Path = Url.Mid(7);
		nsMediaUrl = [NSURL fileURLWithPath:Path.GetNSString() isDirectory:NO];
	}
	else
	{
		// Assume that this has been percent encoded for now - when we support HTTP Live Streaming we will need to check for that.
		nsMediaUrl = [NSURL URLWithString: Path.GetNSString()];
	}

	// open media file
	if (nsMediaUrl == nil)
	{
		UE_LOG(LogAvfMedia, Error, TEXT("Failed to open Media file:"), *Url);

		return false;
	}

	// On non-Mac Apple OSes the path is:
	//	a) case-sensitive
	//	b) relative to the 'cookeddata' directory, not the notional GameContentDirectory which is 'virtual' and resolved by the FIOSPlatformFile calls
#if !PLATFORM_MAC
	if ([[nsMediaUrl scheme] isEqualToString:@"file"])
	{
		FString FullPath = AvfMedia::ConvertToIOSPath(Path, false);
		nsMediaUrl = [NSURL fileURLWithPath: FullPath.GetNSString() isDirectory:NO];
	}
#endif
	
	// create player instance
	MediaUrl = FPaths::GetCleanFilename(Url);
	MediaPlayer = [[AVPlayer alloc] init];

	if (!MediaPlayer)
	{
		UE_LOG(LogAvfMedia, Error, TEXT("Failed to create instance of an AVPlayer"));

		return false;
	}
	
	MediaPlayer.actionAtItemEnd = AVPlayerActionAtItemEndPause;

	// create player item
	MediaHelper = [[FAVPlayerDelegate alloc] initWithMediaPlayer:this];
	check(MediaHelper != nil);

	PlayerItem = [[AVPlayerItem playerItemWithURL:nsMediaUrl] retain];

	if (PlayerItem == nil)
	{
		UE_LOG(LogAvfMedia, Error, TEXT("Failed to open player item with Url:"), *Url);

		return false;
	}

	CurrentState = EMediaState::Preparing;

	// load tracks
	[[PlayerItem asset] loadValuesAsynchronouslyForKeys:@[@"tracks"] completionHandler:^
	{
		NSError* Error = nil;

		if ([[PlayerItem asset] statusOfValueForKey:@"tracks" error : &Error] == AVKeyValueStatusLoaded)
		{
			[[NSNotificationCenter defaultCenter] addObserver:MediaHelper selector:@selector(playerItemPlaybackEndReached:) name:AVPlayerItemDidPlayToEndTimeNotification object:PlayerItem];
			
			// File movies will be ready now
			if (PlayerItem.status == AVPlayerItemStatusReadyToPlay)
			{
				OnStatusNotification(PlayerItem.status);
			}
			
			// Streamed movies might not be and we want to know if it ever fails.
			[PlayerItem addObserver:MediaHelper forKeyPath:@"status" options:0 context:PlayerItem];
		}
		else if (Error != nullptr)
		{
			CurrentState = EMediaState::Error;
			
			NSDictionary *userInfo = [Error userInfo];
			NSString *errstr = [[userInfo objectForKey : NSUnderlyingErrorKey] localizedDescription];

			UE_LOG(LogAvfMedia, Warning, TEXT("Failed to load video tracks. [%s]"), *FString(errstr));
	 
			PlayerTasks.Enqueue([=]() {
				EventSink.ReceiveMediaEvent(EMediaEvent::MediaOpenFailed);
			});
		}
	}];

	[MediaPlayer replaceCurrentItemWithPlayerItem : PlayerItem];
	[[MediaPlayer currentItem] seekToTime:kCMTimeZero];

	MediaPlayer.rate = 0.0;
	CurrentTime = FTimespan::Zero();
		
    if (!ResumeHandle.IsValid())
    {
        FCoreDelegates::ApplicationHasEnteredForegroundDelegate.AddRaw(this, &FAvfMediaPlayer::HandleApplicationHasEnteredForeground);
        ResumeHandle.Reset();
    }
    
    if (!PauseHandle.IsValid())
    {
        FCoreDelegates::ApplicationWillEnterBackgroundDelegate.AddRaw(this, &FAvfMediaPlayer::HandleApplicationWillEnterBackground);
        PauseHandle.Reset();
    }
    
	return true;
}


bool FAvfMediaPlayer::Open(const TSharedRef<FArchive, ESPMode::ThreadSafe>& /*Archive*/, const FString& /*OriginalUrl*/, const IMediaOptions* /*Options*/)
{
	return false; // not supported
}


void FAvfMediaPlayer::TickAudio()
{
	if ((CurrentState > EMediaState::Error) && (Duration > 0.0f))
	{
		Tracks->ProcessAudio();
		if(MediaPlayer != nil)
		{
			CMTime Current = [MediaPlayer currentTime];
			FTimespan DiplayTime = FTimespan::FromSeconds(CMTimeGetSeconds(Current));
			CurrentTime = FMath::Min(DiplayTime, Duration);
		}
	}
}


void FAvfMediaPlayer::TickFetch(FTimespan DeltaTime, FTimespan /*Timecode*/)
{
	if ((CurrentState > EMediaState::Error) && (Duration > 0.0f))
	{
		Tracks->ProcessVideo();
	}
}


void FAvfMediaPlayer::TickInput(FTimespan DeltaTime, FTimespan /*Timecode*/)
{
	// process deferred tasks
	TFunction<void()> Task;

	while (PlayerTasks.Dequeue(Task))
	{
		Task();
	}
}


/* IMediaControls interface
 *****************************************************************************/

bool FAvfMediaPlayer::CanControl(EMediaControl Control) const
{
	if (!bPrerolled)
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


FTimespan FAvfMediaPlayer::GetDuration() const
{
	return Duration;
}


float FAvfMediaPlayer::GetRate() const
{
	return CurrentRate;
}


EMediaState FAvfMediaPlayer::GetState() const
{
	return CurrentState;
}


EMediaStatus FAvfMediaPlayer::GetStatus() const
{
	return EMediaStatus::None;
}


TRangeSet<float> FAvfMediaPlayer::GetSupportedRates(EMediaRateThinning Thinning) const
{
	TRangeSet<float> Result;

	Result.Add(TRange<float>(PlayerItem.canPlayFastReverse ? -8.0f : -1.0f, 0.0f));
	Result.Add(TRange<float>(0.0f, PlayerItem.canPlayFastForward ? 8.0f : 0.0f));

	return Result;
}


FTimespan FAvfMediaPlayer::GetTime() const
{
	return CurrentTime;
}


bool FAvfMediaPlayer::IsLooping() const
{
	return ShouldLoop;
}


bool FAvfMediaPlayer::Seek(const FTimespan& Time)
{
	CurrentTime = Time;
	
	if (bPrerolled)
	{
		Tracks->Seek(Time);
		
		double TotalSeconds = Time.GetTotalSeconds();
		CMTime CurrentTimeInSeconds = CMTimeMakeWithSeconds(TotalSeconds, 1000);
		
		static CMTime Tolerance = CMTimeMakeWithSeconds(0.01, 1000);
		[MediaPlayer seekToTime:CurrentTimeInSeconds toleranceBefore:Tolerance toleranceAfter:Tolerance];
	}

	EventSink.ReceiveMediaEvent(EMediaEvent::SeekCompleted);

	return true;
}


bool FAvfMediaPlayer::SetLooping(bool Looping)
{
	ShouldLoop = Looping;
	
	if (ShouldLoop)
	{
		MediaPlayer.actionAtItemEnd = AVPlayerActionAtItemEndNone;
	}
	else
	{
		MediaPlayer.actionAtItemEnd = AVPlayerActionAtItemEndPause;
	}

	return true;
}


bool FAvfMediaPlayer::SetRate(float Rate)
{
	CurrentRate = Rate;
	
	if (bPrerolled)
	{
		[MediaPlayer setRate : CurrentRate];

		Tracks->SetRate(Rate);
		
		if (FMath::IsNearlyZero(Rate))
		{
			CurrentState = EMediaState::Paused;
			EventSink.ReceiveMediaEvent(EMediaEvent::PlaybackSuspended);
		}
		else
		{
			CurrentState = EMediaState::Playing;
			EventSink.ReceiveMediaEvent(EMediaEvent::PlaybackResumed);
		}
	}

	return true;
}

void FAvfMediaPlayer::HandleApplicationHasEnteredForeground()
{
    // check the state to ensure we are still playing
    if ((CurrentState == EMediaState::Playing) && MediaPlayer != nil)
    {
        [MediaPlayer play];
    }
}

void FAvfMediaPlayer::HandleApplicationWillEnterBackground()
{
    // check the state to ensure we are still playing
    if ((CurrentState == EMediaState::Playing) && MediaPlayer != nil)
    {
        [MediaPlayer pause];
    }
}
