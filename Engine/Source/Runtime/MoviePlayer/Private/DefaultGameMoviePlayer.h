// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/ThreadSafeCounter.h"
#include "Stats/Stats.h"
#include "Types/SlateStructs.h"
#include "Layout/Visibility.h"
#include "Input/Reply.h"
#include "Rendering/SlateRenderer.h"
#include "Widgets/SWidget.h"
#include "Widgets/SWindow.h"
#include "Widgets/Layout/SBorder.h"
#include "MoviePlayer.h"
#include "TickableObjectRenderThread.h"


class FWidgetRenderer;
class SVirtualWindow;

class FMoviePlayerWidgetRenderer
{
public:
	FMoviePlayerWidgetRenderer(TSharedPtr<SWindow> InMainWindow, TSharedPtr<SVirtualWindow> InVirtualRenderWindowWindow, FSlateRenderer* InRenderer);

	void DrawWindow(float DeltaTime);

private:
	/** The actual window content will be drawn to */
	/** Note: This is raw as we SWindows registered with SlateApplication are not thread safe */
	SWindow* MainWindow;

	/** Virtual window that we render to instead of the main slate window (for thread safety).  Shares only the same backbuffer as the main window */
	TSharedRef<class SVirtualWindow> VirtualRenderWindow;

	TSharedPtr<FHittestGrid> HittestGrid;

	FSlateRenderer* SlateRenderer;

	FViewportRHIRef ViewportRHI;
};

/** An implementation of the movie player/loading screen we will use */
class FDefaultGameMoviePlayer : public FTickableObjectRenderThread, public IGameMoviePlayer,
	public TSharedFromThis<FDefaultGameMoviePlayer>
{
public:
	static void Create()
	{
		check(IsInGameThread() && !IsInSlateThread());
		check(!MoviePlayer.IsValid());

		MoviePlayer = MakeShareable(new FDefaultGameMoviePlayer);
	}

	static void Destroy()
	{
		check(IsInGameThread() && !IsInSlateThread());
		MoviePlayer.Reset();
	}

	static FDefaultGameMoviePlayer* Get();
	~FDefaultGameMoviePlayer();

	/** IGameMoviePlayer Interface */
	virtual void RegisterMovieStreamer(TSharedPtr<IMovieStreamer> InMovieStreamer) override;
	virtual void Initialize(FSlateRenderer& InSlateRenderer) override;
	virtual void Shutdown() override;
	virtual void PassLoadingScreenWindowBackToGame() const override;
	virtual void SetupLoadingScreen(const FLoadingScreenAttributes& LoadingScreenAttributes) override;
	virtual bool HasEarlyStartupMovie() const override;
	virtual bool PlayEarlyStartupMovies() override;
	virtual bool PlayMovie() override;
	virtual void StopMovie() override;
	virtual void WaitForMovieToFinish() override;
	virtual bool IsLoadingFinished() const override;
	virtual bool IsMovieCurrentlyPlaying() const override;
	virtual bool LoadingScreenIsPrepared() const override;
	virtual void SetupLoadingScreenFromIni() override;

	virtual FOnPrepareLoadingScreen& OnPrepareLoadingScreen() override { return OnPrepareLoadingScreenDelegate; }
	virtual FOnMoviePlaybackFinished& OnMoviePlaybackFinished() override { return OnMoviePlaybackFinishedDelegate; }
	virtual FOnMovieClipFinished& OnMovieClipFinished() override { return OnMovieClipFinishedDelegate; }

	/** FTickableObjectRenderThread interface */
	virtual void Tick( float DeltaTime ) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override;
	
	/** Callback for clicking on the viewport */
	FReply OnLoadingScreenMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& PointerEvent);
	FReply OnLoadingScreenKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent);

	virtual void SetSlateOverlayWidget(TSharedPtr<SWidget> NewOverlayWidget) override;

	virtual bool WillAutoCompleteWhenLoadFinishes() override;

	virtual FString GetMovieName() override;
	virtual bool IsLastMovieInPlaylist() override;
	float GetViewportDPIScale() const;

private:

	/** Ticks the underlying MovieStreamer.  Must be done exactly once before each DrawWindows call. */
	void TickStreamer(float DeltaTime);

	/** True if we have both a registered movie streamer and movies to stream */
	bool MovieStreamingIsPrepared() const;

	/** True if movie streamer has finished streaming all the movies it wanted to */
	bool IsMovieStreamingFinished() const;

	/** Callbacks for movie sizing for the movie viewport */
	FVector2D GetMovieSize() const;
	FOptionalSize GetMovieWidth() const;
	FOptionalSize GetMovieHeight() const;
	EVisibility GetSlateBackgroundVisibility() const;
	EVisibility GetViewportVisibility() const;	
	
	/** Called via a delegate in the engine when maps start to load */
	void OnPreLoadMap(const FString& LevelName);
	
	/** Called via a delegate in the engine when maps finish loading */
	void OnPostLoadMap(UWorld* LoadedWorld);
	
	/** Check if the device can render on a parallel thread on the initial loading*/
	bool CanPlayMovie() const;
private:
	FDefaultGameMoviePlayer();

	FReply OnAnyDown();
	
	/** The movie streaming system that will be used by us */
	TSharedPtr<IMovieStreamer> MovieStreamer;
	
	/** The window that the loading screen resides in */
	TWeakPtr<class SWindow> MainWindow;
	/** The widget which includes all contents of the loading screen, widgets and movie player and all */
	TSharedPtr<class SWidget> LoadingScreenContents;
	/** The widget which holds the loading screen widget passed in via the FLoadingScreenAttributes */
	TSharedPtr<class SBorder> UserWidgetHolder;
	/** Virtual window that we render to instead of the main slate window (for thread safety).  Shares only the same backbuffer as the main window */
	TSharedPtr<class SVirtualWindow> VirtualRenderWindow;

	/** The threading mechanism with which we handle running slate on another thread */
	class FSlateLoadingSynchronizationMechanism* SyncMechanism;

	/** True if all movies have successfully streamed and completed */
	FThreadSafeCounter MovieStreamingIsDone;

	/** True if the game thread has finished loading */
	FThreadSafeCounter LoadingIsDone;

	/** User has called finish (needed if LoadingScreenAttributes.bAutoCompleteWhenLoadingCompletes is off) */
	bool bUserCalledFinish;
	
	/** Attributes of the loading screen we are currently displaying */
	FLoadingScreenAttributes LoadingScreenAttributes;

	/** Called before a movie is queued up to play to configure the movie player accordingly. */
	FOnPrepareLoadingScreen OnPrepareLoadingScreenDelegate;
	
	FOnMoviePlaybackFinished OnMoviePlaybackFinishedDelegate;

	FOnMovieClipFinished OnMovieClipFinishedDelegate;

	/** The last time a movie was started */
	double LastPlayTime;

	/** True if the movie player has been initialized */
	bool bInitialized;

	/** Critical section to allow the slate loading thread and the render thread to safely utilize the synchronization mechanism for ticking Slate. */
	FCriticalSection SyncMechanismCriticalSection;

	/** Widget renderer used to tick and paint windows in a thread safe way */
	TSharedPtr<FMoviePlayerWidgetRenderer, ESPMode::ThreadSafe> WidgetRenderer;

	/** DPIScaler parented to the UserWidgetHolder to ensure correct scaling */
	TSharedPtr<class SDPIScaler> UserWidgetDPIScaler;

private:
	/** Singleton handle */
	static TSharedPtr<FDefaultGameMoviePlayer> MoviePlayer;


};
