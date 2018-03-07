// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AppleMovieStreamer.h"

#include "RenderingCommon.h"
#include "Slate/SlateTextures.h"
#include "MoviePlayer.h"
#include "Misc/CommandLine.h"
#include "Misc/ScopeLock.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogMoviePlayer, Log, All);

#define MOVIE_FILE_EXTENSION @"mp4"
#define TIMESCALE 1000

static FString ConvertToNativePath(const FString& Filename, bool bForWrite)
{
	FString Result = Filename;
#if !PLATFORM_MAC
	if (Result.Contains(TEXT("/OnDemandResources/")))
	{
		return Result;
	}
	
	Result.ReplaceInline(TEXT("../"), TEXT(""));
	Result.ReplaceInline(TEXT(".."), TEXT(""));
	Result.ReplaceInline(FPlatformProcess::BaseDir(), TEXT(""));
	
	if(bForWrite)
	{
		static FString WritePathBase = FString([NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES) objectAtIndex:0]) + TEXT("/");
		return WritePathBase + Result;
	}
	else
	{
		// if filehostip exists in the command line, cook on the fly read path should be used
		FString Value;
		// Cache this value as the command line doesn't change...
		static bool bHasHostIP = FParse::Value(FCommandLine::Get(), TEXT("filehostip"), Value) || FParse::Value(FCommandLine::Get(), TEXT("streaminghostip"), Value);
		static bool bIsIterative = FParse::Value(FCommandLine::Get(), TEXT("iterative"), Value);
		if (bHasHostIP)
		{
			static FString ReadPathBase = FString([NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES) objectAtIndex:0]) + TEXT("/");
			return ReadPathBase + Result;
		}
		else if (bIsIterative)
		{
			static FString ReadPathBase = FString([NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES) objectAtIndex:0]) + TEXT("/");
			return ReadPathBase + Result.ToLower();
		}
		else
		{
			static FString ReadPathBase = FString([[NSBundle mainBundle] bundlePath]) + TEXT("/cookeddata/");
			return ReadPathBase + Result.ToLower();
		}
	}
#endif
	
	return Result;
}

//
// Methods
//

FAVPlayerMovieStreamer::FAVPlayerMovieStreamer() :
AudioPlayer         ( NULL ),
AVMovie             ( NULL ),
AVReader            ( NULL ),
AVVideoOutput       ( NULL ),
AVVideoTrack        ( NULL ),
LatestSamples       ( NULL ),
VideoRate           ( 0.0f ),
SyncStatus          ( Default ),
StartTime           ( 0.0 ),
Cursor              ( 0.0 ),
bVideoTracksLoaded  ( false ),
bWasActive          ( false )
{
	UE_LOG(LogMoviePlayer, Log, TEXT("FAVMoviePlayer ctor..."));

    TextureData = MakeShareable(new FSlateTextureData());
    MovieViewport = MakeShareable(new FMovieViewport());
}

FAVPlayerMovieStreamer::~FAVPlayerMovieStreamer()
{
#if !PLATFORM_MAC // Crash on quit - the plugin won't be destroyed until after everything has been de-inited.
    UE_LOG(LogMoviePlayer, Log, TEXT("FAVMoviePlayer dtor..."));
#endif

    // Clean up any remaining resources
    Cleanup();

    // Clear out the pending list
    // NOTE: No guarantees here that the resources are actually freed, and we can't force them to be freed.
    TexturesPendingDeletion.Empty();
}

void FAVPlayerMovieStreamer::ForceCompletion()
{
    // Stop the player and make sure it doesn't attempt to start the next movie
    MovieQueue.Empty();

    if( bVideoTracksLoaded && AVReader != nil )
    {
        [AVReader cancelReading];
    }
    if( AudioPlayer != nil )
    {
        [AudioPlayer stop];
    }

    // Teardown playback
    TeardownPlayback();
}

bool FAVPlayerMovieStreamer::Init(const TArray<FString>& MoviePaths, TEnumAsByte<EMoviePlaybackType> inPlaybackType)
{
	// 
	// Initializes the streamer for audio and video playback of the given path(s).
	// NOTE: If multiple paths are provided, it is expect that they be played back seamlessly.
	UE_LOG(LogMoviePlayer, Warning, TEXT("FAVMoviePlayer init. Path count = %d..."), MoviePaths.Num());

	// Add the given paths to the movie queue
	MovieQueue.Append(MoviePaths);

	// Play the next movie in the queue
	return StartNextMovie();
}

bool FAVPlayerMovieStreamer::Tick(float DeltaTime)
{
    // Check the list of textures pending deletion and remove any that are no longer valid
    for (int32 TextureIndex = 0; TextureIndex < TexturesPendingDeletion.Num(); )
    {
        TSharedPtr<FSlateTexture2DRHIRef, ESPMode::ThreadSafe> Tex = TexturesPendingDeletion[TextureIndex];
        if (!Tex->IsInitialized())
        {
            // Texture can now be removed from the list
            TexturesPendingDeletion.RemoveAt(TextureIndex);

            // Don't increment i, since element 'i' wasn't just filled by the next element
        }
        else
        {
            // Advance to the next one
            ++TextureIndex;
        }
    }

	FScopeLock LockVideoTracksLoading(&VideoTracksLoadingLock);

    if( bVideoTracksLoaded )
    {
        // Remember that we were active. Used to edge detect active/not-active transitions
        bWasActive = true;

        if( CheckForNextFrameAndCopy() )
        {
            // Copy new frame data.
            uint32 Stride;
            uint8* DestTextureData = (uint8*)RHILockTexture2D( Texture->GetTypedResource(), 0, RLM_WriteOnly, Stride, false );
            FMemory::Memcpy( DestTextureData, TextureData->GetRawBytesPtr(), TextureData->GetRawBytes().Num() );
            RHIUnlockTexture2D( Texture->GetTypedResource(), 0, false );
        }

        check(AVReader != nil);
        AVAssetReaderStatus Status = [AVReader status];
        switch( Status )
        {
        case AVAssetReaderStatusReading:
            // Good...
            break;
        case AVAssetReaderStatusCompleted:
            // Mark video so that we can move on.
            bVideoTracksLoaded = false;
            break;
        case AVAssetReaderStatusFailed:
            UE_LOG(LogMoviePlayer, Error, TEXT("Movie reader entered Failure status.") );
			bVideoTracksLoaded = false;
            break;
        case AVAssetReaderStatusCancelled:
            UE_LOG(LogMoviePlayer, Error, TEXT("Movie reader entered Cancelled status.") );
			bVideoTracksLoaded = false;
            break;
        default:
            UE_LOG(LogMoviePlayer, Error, TEXT("Movie reader encountered unknown error.") );
			bVideoTracksLoaded = false;
            break;
        }
    }
    else
    {
        // We aren't active any longer, so stop the player
        if (bWasActive)
        {
			// Not active - has to try to reload tracks to become active again - keeps 2nd/3rd..etc.. videos same behaviour as 1st
			bWasActive = false;
			
            // The previous playback is complete, so shutdown.
            // NOTE: The texture resources are not freed here.
            TeardownPlayback();

			UE_LOG(LogMoviePlayer, Verbose, TEXT("%d movie(s) left to play."), MovieQueue.Num());
            // If there are still movies to play, start the next movie
            if (MovieQueue.Num() > 0)
            {
                StartNextMovie();

                // Still playing a movie, so return that we aren't done yet
                return false;
            }
            else
            {
                // Done.
                return true;
            }
        }
		else
		{
			// No movie object and nothing left in queue - it's done - probably an error case - movie does not exist or failed to load
			// Otherwise waiting for load operation
			return AVMovie == nil && MovieQueue.Num() == 0;
		}
    }

    // Not completed.
    return false;	
}

TSharedPtr<class ISlateViewport> FAVPlayerMovieStreamer::GetViewportInterface()
{
	return MovieViewport;
}

float FAVPlayerMovieStreamer::GetAspectRatio() const
{
	return (float)MovieViewport->GetSize().X / (float)MovieViewport->GetSize().Y;
}

void FAVPlayerMovieStreamer::Cleanup()
{
	// Reset variables
	bWasActive = false;
	SyncStatus = Default;

	if( LatestSamples != NULL )
	{
		CFRelease( LatestSamples );
		LatestSamples = NULL;
	}
	
    MovieViewport->SetTexture(NULL);

	// Schedule textures for release.
    if (Texture.IsValid())
    {
        TexturesPendingDeletion.Add(Texture);
        BeginReleaseResource(Texture.Get());
        Texture.Reset();
    }
}

bool FAVPlayerMovieStreamer::StartNextMovie()
{
	bool bDidStartMovie = false;
	UE_LOG(LogMoviePlayer, Verbose, TEXT("Starting next movie....") );
    if (MovieQueue.Num() > 0)
    {
        if( AVMovie != NULL )
        {
            // Can't setup playback when already set up.
            UE_LOG(LogMoviePlayer, Error, TEXT("can't setup FAVPlayerMovieStreamer because it is already set up"));
            return false;
        }

        // Reset flag to indicate the movie may have started, but isn't playing yet.
        bVideoTracksLoaded = false;

        NSURL* nsURL = nil;
		FString MoviePath = FPaths::ProjectContentDir() + TEXT("Movies/") + MovieQueue[0] + TEXT(".") + FString(MOVIE_FILE_EXTENSION);
		if (FPaths::FileExists(MoviePath))
		{
			nsURL = [NSURL fileURLWithPath:ConvertToNativePath(MoviePath, false).GetNSString()];
		}
		
        if (nsURL == nil)
        {
            UE_LOG(LogMoviePlayer, Warning, TEXT("Couldn't find movie: %s"), *MovieQueue[0]);
            MovieQueue.RemoveAt(0);
            return false;
        }
        
        NSError *error = nil;
        AudioPlayer = [[AVAudioPlayer alloc] initWithContentsOfURL:nsURL error:&error];
        if ( AudioPlayer == nil )
        {
            UE_LOG(LogMoviePlayer, Warning, TEXT("couldn't initialize Movie player audio, bad file, or possibly just no Audio"));
        }
		else
		{
			AudioPlayer.numberOfLoops = 0;
			AudioPlayer.volume = 1;
			[AudioPlayer prepareToPlay];
		}

        // Load the Movie with the appropriate URL.
        AVMovie = [[AVURLAsset alloc] initWithURL:nsURL options:nil];
        MovieQueue.RemoveAt(0);

        // Obtain the tracks asynchronously.
        NSArray* nsTrackKeys = @[@"tracks"];
        [AVMovie loadValuesAsynchronouslyForKeys:nsTrackKeys completionHandler:^()
        {
            // !!! This block will execute asynchronously !!!

            // Once loaded, initialize our reader object to start pulling frames.
			FScopeLock LockVideoTracksLoading(&VideoTracksLoadingLock);
            bVideoTracksLoaded = FinishLoadingTracks();
            if( bVideoTracksLoaded && AudioPlayer != nil )
            {
                // Good time to start the audio playing.
                [AudioPlayer play];
            }

            // !!!
        }];

        // Movie has started.
        bDidStartMovie = true;
		
		UE_LOG(LogMoviePlayer, Verbose, TEXT("Started next movie.") );
    }
    return bDidStartMovie;
}

bool FAVPlayerMovieStreamer::FinishLoadingTracks()
{
    NSError *nsError = nil;
    AVKeyValueStatus TrackStatus = [AVMovie statusOfValueForKey:@"tracks" error:&nsError];

    bool bLoadedAndReading = false;
    switch( TrackStatus )
    {
    case AVKeyValueStatusLoaded:
        {
            // Tracks loaded correctly!

            // Create a reader to actually process the tracks.
            AVReader = [[AVAssetReader alloc] initWithAsset: AVMovie error: &nsError];
            check(AVReader != nil);

            // The media may have multiple tracks (like audio), but we'll just grab the first Video track.
            NSArray* nsVideoTracks = [AVMovie tracksWithMediaType:AVMediaTypeVideo];
            if( [nsVideoTracks count] == 0 )
            {
                UE_LOG(LogMoviePlayer, Error, TEXT("Movie contains no Video tracks.") );
                return false;
            }
            else
            {
                // Save the track for later.
                AVVideoTrack = [nsVideoTracks objectAtIndex:0];
				
				CGSize naturalSize = [AVVideoTrack naturalSize];
				if((int(naturalSize.width) % 16) != 0)
				{
					UE_LOG(LogMoviePlayer, Error, TEXT("Movie width must be a multiple of 16 pixels.") );
					return false;
				}
				
                // Initialize our video output to match the format of the texture we'll create later.
                NSMutableDictionary* OutputSettings = [NSMutableDictionary dictionary];
                [OutputSettings setObject: [NSNumber numberWithInt:kCVPixelFormatType_32BGRA] forKey:(NSString*)kCVPixelBufferPixelFormatTypeKey];
                AVVideoOutput = [[AVAssetReaderTrackOutput alloc] initWithTrack:AVVideoTrack outputSettings:OutputSettings];
                AVVideoOutput.alwaysCopiesSampleData = NO;

                // Assign the track to the reader.
                [AVReader addOutput:AVVideoOutput];

                // Begin reading!
                if( ![AVReader startReading] )
                {
                    UE_LOG(LogMoviePlayer, Error, TEXT("AVReader 'startReading' returned failure."));
                    return false;
                }

                // Save the rate of playback.
                check( AVVideoTrack.nominalFrameRate );
                VideoRate = 1.0f / AVVideoTrack.nominalFrameRate;

                // Save the starting time.
                StartTime = CACurrentMediaTime();

                // Good to go.
                bLoadedAndReading = true;
            }

        } break;
    case AVKeyValueStatusFailed:
        {
            UE_LOG(LogMoviePlayer, Error, TEXT("Failed to load Tracks for Movie.") );
        } break;
    case AVKeyValueStatusCancelled:
        {
            UE_LOG(LogMoviePlayer, Error, TEXT("Cancelled loading Tracks for Movie.") );
        } break;
    default:
        {
            UE_LOG(LogMoviePlayer, Error, TEXT("Unknown error loading Tracks for Movie.") );
        } break;
    }

    return bLoadedAndReading;
}

bool FAVPlayerMovieStreamer::CheckForNextFrameAndCopy()
{
    check( bVideoTracksLoaded );

    bool bHasNewFrame = false;

    // We need to synchronize the video playback with the audio.
    // If the video frame is within tolerance (Ready), update the Texture Data.
    // If the video is Behind, throw it away and get the next one until we catch up with the ref time.
    // If the video is Ahead, update the TextureData but don't retrieve more frames until time catches up.

    
    while( SyncStatus != Ready )
    {
        double Delta;
        if( SyncStatus != Ahead )
        {
            LatestSamples = [AVVideoOutput copyNextSampleBuffer];
        }
        if( LatestSamples == NULL )
        {
            // Failed to get next sample buffer.
            break;
        }

        // Get the time stamp of the video frame
        CMTime FrameTimeStamp = CMSampleBufferGetPresentationTimeStamp(LatestSamples);

        // Get the time since playback began
        Cursor = CACurrentMediaTime() - StartTime;
        CMTime caCurrentTime = CMTimeMake(Cursor * TIMESCALE, TIMESCALE);

        // Compute delta of video frame and current playback times
        Delta = CMTimeGetSeconds(caCurrentTime) - CMTimeGetSeconds(FrameTimeStamp);

        if( Delta < 0 ) 
        {
            Delta *= -1;  
            SyncStatus = Ahead;
        }
        else 
        {
            SyncStatus = Behind;
        }

        if( Delta < VideoRate )
        {
             // Video in sync with audio
            SyncStatus = Ready;
        }
        else if(SyncStatus == Ahead)
        {
            // Video ahead of audio: stay in Ahead state, exit loop
            break;
        }
        else 
        {
            // Video behind audio (Behind): stay in loop
            CFRelease(LatestSamples);
            LatestSamples = NULL;
        }
    }
    

    // Do we have a new frame to process?
    if( SyncStatus == Ready )
    {
        check(LatestSamples != NULL);

        // Grab the pixel buffer and lock it for reading.
        CVImageBufferRef pPixelBuffer = CMSampleBufferGetImageBuffer(LatestSamples);
        CGSize Size                   = CVImageBufferGetEncodedSize( pPixelBuffer );
        CVPixelBufferLockBaseAddress( pPixelBuffer, kCVPixelBufferLock_ReadOnly );

        uint8* pVideoData = (uint8*)CVPixelBufferGetBaseAddress(pPixelBuffer);

        uint32 SrcWidth  = (uint32)Size.width;
        uint32 SrcHeight = (uint32)Size.height;

        // Now that we have video information, ensure that we have texture data in the right dimensions
        if (TextureData->GetWidth() != SrcWidth || TextureData->GetHeight() != SrcHeight)
        {
            check( SrcWidth > 0 && SrcHeight > 0 );

			TArray<uint8> TempData;
			TempData.AddZeroed(SrcWidth * SrcHeight * 4);
			TextureData->SetRawData(SrcWidth, SrcHeight, SrcWidth * 4, TempData);
			check( TextureData->GetRawBytesPtr() != NULL );
        }

        // Now that we have video information, check on texture allocation. If we don't have a texture yet, create one.
        if(!Texture.IsValid() || (Texture->GetWidth() != SrcWidth || Texture->GetHeight() != SrcHeight))
        {
            MovieViewport->SetTexture(NULL);

            // Release any resources associated with the previous texture
            if (Texture.IsValid())
            {
                // *** Aren't we already in the rendering thread? Why can't we just release the resource right here, right now?
                TexturesPendingDeletion.Add(Texture);
                BeginReleaseResource(Texture.Get());
                Texture.Reset();
            }

            // Create and initialize a new texture
            Texture = MakeShareable(new FSlateTexture2DRHIRef(SrcWidth, SrcHeight, PF_B8G8R8A8, nullptr, TexCreate_Dynamic | TexCreate_NoTiling, true));
            Texture->InitResource();

            // Make sure the texture is at least updated once.
            Texture->UpdateRHI();
            MovieViewport->SetTexture(Texture);
        }

        check( TextureData->GetBytesPerPixel() > 0 );
        
        // Copy the video data
        uint32 Len = TextureData->GetBytesPerPixel() * SrcHeight;
        check( Len > 0 );
        FMemory::Memcpy(TextureData->GetRawBytesPtr(), pVideoData, Len);

        // Re-lock and release the video data.
        CVPixelBufferUnlockBaseAddress( pPixelBuffer, kCVPixelBufferLock_ReadOnly );
        
        // Processed this frame, so dump the samples.
        CFRelease(LatestSamples);
        LatestSamples = NULL;

        bHasNewFrame = true;
    }
    if( SyncStatus != Ahead )
    {
        // Reset status;
        SyncStatus = Default;
    }

    return bHasNewFrame;
}

void FAVPlayerMovieStreamer::TeardownPlayback()
{
    if( LatestSamples != NULL )
    {
        CFRelease( LatestSamples );
        LatestSamples = NULL;
    }
    
    // NS Object release handled by external AutoReleasePool
    if( AVVideoOutput != nil )
    {
        AVVideoOutput = nil;
    }
    if( AVVideoTrack != nil )
    {
        AVVideoTrack = nil;
    }
    if( AVReader != nil )
    {
        AVReader = nil;
    }
    if( AVMovie != nil )
    {
        AVMovie = nil;
    }
    if ( AudioPlayer != nil )
    {
        AudioPlayer = nil;
    }

    // NOTE: The any textures allocated are still allocated at this point. They will get released in Cleanup()
}

FString FAVPlayerMovieStreamer::GetMovieName()
{
	return MovieQueue.Num() > 0 ? MovieQueue[0] : TEXT("");
}

bool FAVPlayerMovieStreamer::IsLastMovieInPlaylist()
{
	return MovieQueue.Num() <= 1;
}

