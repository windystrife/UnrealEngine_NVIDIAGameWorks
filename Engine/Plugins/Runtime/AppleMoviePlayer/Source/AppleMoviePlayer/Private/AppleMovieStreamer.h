// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "MoviePlayer.h"

#include "Slate/SlateTextures.h"

#import <AVFoundation/AVFoundation.h>


// The actual streamer class
class FAVPlayerMovieStreamer : public IMovieStreamer
{
public:
	FAVPlayerMovieStreamer();
	virtual ~FAVPlayerMovieStreamer();

	virtual bool Init(const TArray<FString>& MoviePaths, TEnumAsByte<EMoviePlaybackType> inPlaybackType) override;
	virtual void ForceCompletion() override;
	virtual bool Tick(float DeltaTime) override;
	virtual TSharedPtr<class ISlateViewport> GetViewportInterface() override;
	virtual float GetAspectRatio() const override;
	virtual void Cleanup() override;

	virtual FString GetMovieName() override;
	virtual bool IsLastMovieInPlaylist() override;

	FOnCurrentMovieClipFinished OnCurrentMovieClipFinishedDelegate;
	virtual FOnCurrentMovieClipFinished& OnCurrentMovieClipFinished() override { return OnCurrentMovieClipFinishedDelegate; }

private:

    enum ESyncStatus
    {
        Default,    // Starting state
        Ahead,      // Frame is ahead of playback cursor. 
        Behind,     // Frame is behind playback cursor. 
        Ready,      // Frame is within tolerance of playback cursor.
    };

    // Holds references to textures until their RHI resources are freed
    TArray<TSharedPtr<FSlateTexture2DRHIRef, ESPMode::ThreadSafe>> TexturesPendingDeletion;

	/** Texture and viewport data for displaying to Slate */
	TSharedPtr<FMovieViewport> MovieViewport;

    TSharedPtr<FSlateTextureData, ESPMode::ThreadSafe> TextureData;
    TSharedPtr<FSlateTexture2DRHIRef, ESPMode::ThreadSafe> Texture;

    // The list of pending movies
    TArray<FString>		        MovieQueue;

    // Current Movie
    AVAudioPlayer*              AudioPlayer;
    AVURLAsset*                 AVMovie;
    AVAssetReader*              AVReader;
    AVAssetReaderTrackOutput*   AVVideoOutput;
    AVAssetTrack*               AVVideoTrack;
    CMSampleBufferRef           LatestSamples;

    // AV Synchronization
    float                       VideoRate;
    int                         SyncStatus;
    double                      StartTime;
    double                      Cursor;

    bool                        bVideoTracksLoaded;
    bool                        bWasActive;

	FCriticalSection			VideoTracksLoadingLock;

private:

    /**
	 * Sets up and starts playback on the next movie in the queue
	 */
	bool StartNextMovie();

    bool FinishLoadingTracks();

    void TeardownPlayback();

    bool CheckForNextFrameAndCopy();

};
