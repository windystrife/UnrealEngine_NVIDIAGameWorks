// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "DefaultGameMoviePlayer.h"
#include "HAL/PlatformSplash.h"
#include "Misc/ScopeLock.h"
#include "Misc/CoreDelegates.h"
#include "EngineGlobals.h"
#include "Widgets/SViewport.h"
#include "Engine/GameEngine.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Layout/SBox.h"
#include "GlobalShader.h"
#include "MoviePlayerThreading.h"
#include "MoviePlayerSettings.h"
#include "ShaderCompiler.h"
#include "IHeadMountedDisplay.h"
#include "IXRTrackingSystem.h"
#include "IStereoLayers.h"
#include "ConfigCacheIni.h"
#include "FileManager.h"
#include "SVirtualWindow.h"
#include "SlateDrawBuffer.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Widgets/Layout/SDPIScaler.h"
#include "Engine/UserInterfaceSettings.h"

DEFINE_LOG_CATEGORY_STATIC(LogMoviePlayer, Log, All);

class SDefaultMovieBorder : public SBorder
{
public:

	SLATE_BEGIN_ARGS(SDefaultMovieBorder)		
		: _OnKeyDown()
	{}

		SLATE_EVENT(FPointerEventHandler, OnMouseButtonDown)
		SLATE_EVENT(FOnKeyDown, OnKeyDown)
		SLATE_DEFAULT_SLOT(FArguments, Content)

	SLATE_END_ARGS()

	/**
	* Construct this widget
	*
	* @param	InArgs	The declaration data for this widget
	*/
	void Construct(const FArguments& InArgs)
	{
		OnKeyDownHandler = InArgs._OnKeyDown;

		SBorder::Construct(SBorder::FArguments()
			.BorderImage(FCoreStyle::Get().GetBrush(TEXT("BlackBrush")))
			.OnMouseButtonDown(InArgs._OnMouseButtonDown)
			.Padding(0)[InArgs._Content.Widget]);

	}

	/**
	* Set the handler to be invoked when the user presses a key.
	*
	* @param InHandler   Method to execute when the user presses a key
	*/
	void SetOnOnKeyDown(const FOnKeyDown& InHandler)
	{
		OnKeyDownHandler = InHandler;
	}

	/**
	* Overrides SWidget::OnKeyDown()
	* executes OnKeyDownHandler if it is bound
	*/
	FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override
	{
		if (OnKeyDownHandler.IsBound())
		{
			// If a handler is assigned, call it.
			return OnKeyDownHandler.Execute(MyGeometry, InKeyEvent);
		}
		return SBorder::OnKeyDown(MyGeometry, InKeyEvent);
	}

	/**
	* Overrides SWidget::SupportsKeyboardFocus()
	* Must support keyboard focus to accept OnKeyDown events
	*/
	bool SupportsKeyboardFocus() const override
	{
		return true;
	}

protected:

	FOnKeyDown OnKeyDownHandler;
};

TSharedPtr<FDefaultGameMoviePlayer> FDefaultGameMoviePlayer::MoviePlayer;

FDefaultGameMoviePlayer* FDefaultGameMoviePlayer::Get()
{
	return MoviePlayer.Get();
}

FDefaultGameMoviePlayer::FDefaultGameMoviePlayer()
	: FTickableObjectRenderThread(false, true)
	, SyncMechanism(NULL)
	, MovieStreamingIsDone(1)
	, LoadingIsDone(1)
	, bUserCalledFinish(false)
	, LoadingScreenAttributes()
	, LastPlayTime(0.0)
	, bInitialized(false)
{
	FCoreDelegates::IsLoadingMovieCurrentlyPlaying.BindRaw(this, &FDefaultGameMoviePlayer::IsMovieCurrentlyPlaying);
}

FDefaultGameMoviePlayer::~FDefaultGameMoviePlayer()
{
	if ( bInitialized )
	{
		// This should not happen if initialize was called correctly.  This is a fallback to ensure that the movie player rendering tickable gets unregistered on the rendering thread correctly
		Shutdown();
	}
	else if (GIsRHIInitialized)
	{
		// Even when uninitialized we must safely unregister the movie player on the render thread
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(UnregisterMoviePlayerTickable, FDefaultGameMoviePlayer*, MoviePlayer, this,
		{
			MoviePlayer->Unregister();
		});
	}

	FCoreDelegates::IsLoadingMovieCurrentlyPlaying.Unbind();

	FlushRenderingCommands();
}

void FDefaultGameMoviePlayer::RegisterMovieStreamer(TSharedPtr<IMovieStreamer> InMovieStreamer)
{
	MovieStreamer = InMovieStreamer;
	MovieStreamer->OnCurrentMovieClipFinished().AddRaw(this, &FDefaultGameMoviePlayer::BroadcastMovieClipFinished);
}

void FDefaultGameMoviePlayer::Initialize(FSlateRenderer& InSlateRenderer)
{
	if(bInitialized)
	{
		return;
	}

	UE_LOG(LogMoviePlayer, Log, TEXT("Initializing movie player"));

	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(RegisterMoviePlayerTickable, FDefaultGameMoviePlayer*, MoviePlayer, this,
	{
		MoviePlayer->Register();
	});

	bInitialized = true;

	// Initialize shaders, because otherwise they might not be guaranteed to exist at this point
	if (!FPlatformProperties::RequiresCookedData())
	{
		TArray<int32> ShaderMapIds;
		ShaderMapIds.Add(GlobalShaderMapId);
		GShaderCompilingManager->FinishCompilation(TEXT("Global"), ShaderMapIds);
	}

	// Add a delegate to start playing movies when we start loading a map
	FCoreUObjectDelegates::PreLoadMap.AddRaw( this, &FDefaultGameMoviePlayer::OnPreLoadMap );
	
	// Shutdown the movie player if the app is exiting
	FCoreDelegates::OnPreExit.AddRaw( this, &FDefaultGameMoviePlayer::Shutdown );

	FPlatformSplash::Hide();

	TSharedRef<SWindow> GameWindow = UGameEngine::CreateGameWindow();

	TSharedPtr<SViewport> MovieViewport;

	VirtualRenderWindow =
		SNew(SVirtualWindow)
		.Size(GameWindow->GetClientSizeInScreen());

	WidgetRenderer = MakeShared<FMoviePlayerWidgetRenderer, ESPMode::ThreadSafe>(GameWindow, VirtualRenderWindow, &InSlateRenderer);

	LoadingScreenContents = SNew(SDefaultMovieBorder)	
		.OnKeyDown(this, &FDefaultGameMoviePlayer::OnLoadingScreenKeyDown)
		.OnMouseButtonDown(this, &FDefaultGameMoviePlayer::OnLoadingScreenMouseButtonDown)
		[
			SNew(SOverlay)
			+SOverlay::Slot()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(SBox)
				.WidthOverride(this, &FDefaultGameMoviePlayer::GetMovieWidth)
				.HeightOverride(this, &FDefaultGameMoviePlayer::GetMovieHeight)
				[
					SAssignNew(MovieViewport, SViewport)
					.EnableGammaCorrection(false)
					.Visibility(this, &FDefaultGameMoviePlayer::GetViewportVisibility)
				]
			]
			+SOverlay::Slot()
			[
				SAssignNew(UserWidgetDPIScaler, SDPIScaler)
				[
				SAssignNew(UserWidgetHolder, SBorder)
				.BorderImage(FCoreStyle::Get().GetBrush(TEXT("NoBorder")))
				.Padding(0)
			]
			]
		];

	if (MovieStreamer.IsValid())
	{
		MovieViewport->SetViewportInterface( MovieStreamer->GetViewportInterface().ToSharedRef() );
	}
	
	MovieViewport->SetActive(true);

	// Register the movie viewport so that it can receive user input.
	if (!FPlatformProperties::SupportsWindowedMode())
	{
		FSlateApplication::Get().RegisterGameViewport( MovieViewport.ToSharedRef() );
	}

	MainWindow = GameWindow;
}

void FDefaultGameMoviePlayer::Shutdown()
{
	UE_LOG(LogMoviePlayer, Log, TEXT("Shutting down movie player"));

	StopMovie();
	WaitForMovieToFinish();

	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(UnregisterMoviePlayerTickable, FDefaultGameMoviePlayer*, MoviePlayer, this,
	{
		MoviePlayer->Unregister();
	});

	bInitialized = false;

	FCoreDelegates::OnPreExit.RemoveAll(this);
	FCoreUObjectDelegates::PreLoadMap.RemoveAll( this );
	FCoreUObjectDelegates::PostLoadMapWithWorld.RemoveAll(this);

	LoadingScreenContents.Reset();
	UserWidgetHolder.Reset();
	MainWindow.Reset();
	VirtualRenderWindow.Reset();

	MovieStreamer.Reset();

	LoadingScreenAttributes = FLoadingScreenAttributes();

	if( SyncMechanism )
	{
		SyncMechanism->DestroySlateThread();
		FScopeLock SyncMechanismLock(&SyncMechanismCriticalSection);
		delete SyncMechanism;
		SyncMechanism = NULL;
	}
}
void FDefaultGameMoviePlayer::PassLoadingScreenWindowBackToGame() const
{
	UGameEngine* GameEngine = Cast<UGameEngine>(GEngine);
	if (MainWindow.IsValid() && GameEngine)
	{
		GameEngine->GameViewportWindow = MainWindow;
	}
	else
	{
		UE_LOG(LogMoviePlayer, Warning, TEXT("PassLoadingScreenWindowBackToGame failed.  No Window") );
	}
}

void FDefaultGameMoviePlayer::SetupLoadingScreen(const FLoadingScreenAttributes& InLoadingScreenAttributes)
{
	if (!CanPlayMovie())
	{
		LoadingScreenAttributes = FLoadingScreenAttributes();
		UE_LOG(LogMoviePlayer, Warning, TEXT("Initial loading screen disabled from BaseDeviceProfiles.ini: r.AndroidDisableThreadedRenderingFirstLoad=1"));
	}
	else
	{
	LoadingScreenAttributes = InLoadingScreenAttributes;
}
}

bool FDefaultGameMoviePlayer::HasEarlyStartupMovie() const
{
#if PLATFORM_SUPPORTS_EARLY_MOVIE_PLAYBACK
	return LoadingScreenAttributes.bAllowInEarlyStartup == true;
#else
	return false;
#endif
}

bool FDefaultGameMoviePlayer::PlayEarlyStartupMovies()
{
	if(HasEarlyStartupMovie())
	{
		return PlayMovie();
	}

	return false;
}

bool FDefaultGameMoviePlayer::PlayMovie()
{
	bool bBeganPlaying = false;

	// Allow systems to hook onto the movie player and provide loading screen data on demand 
	// if it has not been setup explicitly by the user.
	if ( !LoadingScreenIsPrepared() )
	{
		OnPrepareLoadingScreenDelegate.Broadcast();
	}

	if (LoadingScreenIsPrepared() && !IsMovieCurrentlyPlaying() && FPlatformMisc::NumberOfCores() > 1)
	{
		check(LoadingScreenAttributes.IsValid());
		bUserCalledFinish = false;
		
		LastPlayTime = FPlatformTime::Seconds();

		bool bIsInitialized = true;
		if (MovieStreamingIsPrepared())
		{
			bIsInitialized = MovieStreamer->Init(LoadingScreenAttributes.MoviePaths, LoadingScreenAttributes.PlaybackType);
		}

		if (bIsInitialized)
		{
			MovieStreamingIsDone.Set(MovieStreamingIsPrepared() ? 0 : 1);
			LoadingIsDone.Set(0);

			UserWidgetDPIScaler->SetDPIScale(GetViewportDPIScale());
			
			UserWidgetHolder->SetContent(LoadingScreenAttributes.WidgetLoadingScreen.IsValid() ? LoadingScreenAttributes.WidgetLoadingScreen.ToSharedRef() : SNullWidget::NullWidget);
			VirtualRenderWindow->Resize(MainWindow.Pin()->GetClientSizeInScreen());
			VirtualRenderWindow->SetContent(LoadingScreenContents.ToSharedRef());
		
			{
				FScopeLock SyncMechanismLock(&SyncMechanismCriticalSection);
				SyncMechanism = new FSlateLoadingSynchronizationMechanism(WidgetRenderer);
				SyncMechanism->Initialize();
			}

			bBeganPlaying = true;
		}
	}

	return bBeganPlaying;
}

/** Check if the device can render on a parallel thread on the initial loading*/
bool FDefaultGameMoviePlayer::CanPlayMovie() const
{
	const IConsoleVariable *const CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.AndroidDisableThreadedRenderingFirstLoad"));
	if (CVar && CVar->GetInt() != 0)
	{
		return (GEngine && GEngine->IsInitialized());
	}
	return true;
}

void FDefaultGameMoviePlayer::StopMovie()
{
	LastPlayTime = 0;
	bUserCalledFinish = true;
	if (UserWidgetHolder.IsValid())
	{
		UserWidgetHolder->SetContent( SNullWidget::NullWidget );
	}
}

void FDefaultGameMoviePlayer::WaitForMovieToFinish()
{
	const bool bEnforceMinimumTime = LoadingScreenAttributes.MinimumLoadingScreenDisplayTime >= 0.0f;

	if (LoadingScreenIsPrepared() && ( IsMovieCurrentlyPlaying() || !bEnforceMinimumTime ) )
	{
	
		if (SyncMechanism)
		{
			SyncMechanism->DestroySlateThread();

			FScopeLock SyncMechanismLock(&SyncMechanismCriticalSection);
			delete SyncMechanism;
			SyncMechanism = nullptr;
		}
		if( !bEnforceMinimumTime )
		{
			LoadingIsDone.Set(1);
		}
		
		// Transfer the content to the main window
		MainWindow.Pin()->SetContent(LoadingScreenContents.ToSharedRef());
		VirtualRenderWindow->SetContent(SNullWidget::NullWidget);

		const bool bAutoCompleteWhenLoadingCompletes = LoadingScreenAttributes.bAutoCompleteWhenLoadingCompletes;
		const bool bWaitForManualStop = LoadingScreenAttributes.bWaitForManualStop;

		FSlateApplication& SlateApp = FSlateApplication::Get();

		// Make sure the movie player widget has user focus to accept keypresses
		if (LoadingScreenContents.IsValid())
		{
			SlateApp.ForEachUser([&](FSlateUser* User) {
				SlateApp.SetUserFocus(User->GetUserIndex(), LoadingScreenContents);
			});
		}

		// Continue to wait until the user calls finish (if enabled) or when loading completes or the minimum enforced time (if any) has been reached.
		while ( 
				(bWaitForManualStop && !bUserCalledFinish)
			||	(!bUserCalledFinish && !bEnforceMinimumTime && !IsMovieStreamingFinished() && !bAutoCompleteWhenLoadingCompletes) 
			||	(bEnforceMinimumTime && (FPlatformTime::Seconds() - LastPlayTime) < LoadingScreenAttributes.MinimumLoadingScreenDisplayTime))
		{
			// If we are in a loading loop, and this is the last movie in the playlist.. assume you can break out.
			if (MovieStreamer.IsValid() && LoadingScreenAttributes.PlaybackType == MT_LoadingLoop && MovieStreamer->IsLastMovieInPlaylist())
			{
				break;
			}

			if (FSlateApplication::IsInitialized())
			{
				// Break out of the loop if the main window is closed during the movie.
				if ( !MainWindow.IsValid() )
				{
					break;
				}

				FPlatformApplicationMisc::PumpMessages(true);

				SlateApp.PollGameDeviceState();
				// Gives widgets a chance to process any accumulated input
				SlateApp.FinishedInputThisFrame();

				float DeltaTime = SlateApp.GetDeltaTime();				

				ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
					BeginLoadingMovieFrameAndTickMovieStreamer,
					FDefaultGameMoviePlayer*, MoviePlayer, this,
					float, DeltaTime, DeltaTime,
					{
						GFrameNumberRenderThread++;
						GRHICommandList.GetImmediateCommandList().BeginFrame();
				
						MoviePlayer->TickStreamer(DeltaTime);
					}
				);
				
				SlateApp.Tick();

				// Synchronize the game thread and the render thread so that the render thread doesn't get too far behind.
				SlateApp.GetRenderer()->Sync();

				ENQUEUE_RENDER_COMMAND(FinishLoadingMovieFrame)(
					[](FRHICommandListImmediate& RHICmdList)
					{
						GRHICommandList.GetImmediateCommandList().EndFrame();
						GRHICommandList.GetImmediateCommandList().ImmediateFlush(EImmediateFlushType::FlushRHIThreadFlushResources);
					}
				);
				FlushRenderingCommands();
			}
		}

		UserWidgetHolder->SetContent( SNullWidget::NullWidget );

		LoadingIsDone.Set(1);

		IStereoLayers* StereoLayers;
		if (GEngine && GEngine->StereoRenderingDevice.IsValid() && (StereoLayers = GEngine->StereoRenderingDevice->GetStereoLayers()) != nullptr && SyncMechanism == nullptr)
		{
			StereoLayers->SetSplashScreenMovie(FTextureRHIRef());
		}

		MovieStreamingIsDone.Set(1);

		FlushRenderingCommands();

		if( MovieStreamer.IsValid() )
		{
			MovieStreamer->ForceCompletion();
		}

		// Allow the movie streamer to clean up any resources it uses once there are no movies to play.
		if( MovieStreamer.IsValid() )
		{
			MovieStreamer->Cleanup();
		}
	
		// Finally, clear out the loading screen attributes, forcing users to always
		// explicitly set the loading screen they want (rather than have stale loading screens)
		LoadingScreenAttributes = FLoadingScreenAttributes();

		BroadcastMoviePlaybackFinished();
	}
	else
	{	
		UGameEngine* GameEngine = Cast<UGameEngine>(GEngine);
		if (GameEngine)
		{
			GameEngine->SwitchGameWindowToUseGameViewport();
		}
	}

}

bool FDefaultGameMoviePlayer::IsLoadingFinished() const
{
	return LoadingIsDone.GetValue() != 0;
}

bool FDefaultGameMoviePlayer::IsMovieCurrentlyPlaying() const
{
	return SyncMechanism != NULL;
}

bool FDefaultGameMoviePlayer::IsMovieStreamingFinished() const
{
	return MovieStreamingIsDone.GetValue() != 0;
}

void FDefaultGameMoviePlayer::Tick( float DeltaTime )
{
	check(IsInRenderingThread());
	if (MainWindow.IsValid() && VirtualRenderWindow.IsValid() && !IsLoadingFinished())
	{
		FScopeLock SyncMechanismLock(&SyncMechanismCriticalSection);
		if(SyncMechanism)
		{
			if(SyncMechanism->IsSlateDrawPassEnqueued())
			{
				GFrameNumberRenderThread++;
				GRHICommandList.GetImmediateCommandList().BeginFrame();
				TickStreamer(DeltaTime);
				SyncMechanism->ResetSlateDrawPassEnqueued();
				GRHICommandList.GetImmediateCommandList().EndFrame();
				GRHICommandList.GetImmediateCommandList().ImmediateFlush(EImmediateFlushType::FlushRHIThreadFlushResources);
			}
		}
	}
}

void FDefaultGameMoviePlayer::TickStreamer(float DeltaTime)
{	
	if (MovieStreamingIsPrepared() && !IsMovieStreamingFinished())
	{
		const bool bMovieIsDone = MovieStreamer->Tick(DeltaTime);		
		if (bMovieIsDone)
		{
			MovieStreamingIsDone.Set(1);
		}

		IStereoLayers* StereoLayers;
		if (GEngine && GEngine->StereoRenderingDevice.IsValid() && (StereoLayers = GEngine->StereoRenderingDevice->GetStereoLayers()) != nullptr)
		{
			FTexture2DRHIRef Movie2DTexture = MovieStreamer->GetTexture();
			FTextureRHIRef MovieTexture;
			if (Movie2DTexture.IsValid() && !bMovieIsDone)
			{
				MovieTexture = (FRHITexture*)Movie2DTexture.GetReference();
			}
			StereoLayers->SetSplashScreenMovie(MovieTexture);
		}
	}
}

TStatId FDefaultGameMoviePlayer::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(FDefaultGameMoviePlayer, STATGROUP_Tickables);
}

bool FDefaultGameMoviePlayer::IsTickable() const
{
	return true;
}

bool FDefaultGameMoviePlayer::LoadingScreenIsPrepared() const
{
	return LoadingScreenAttributes.WidgetLoadingScreen.IsValid() || MovieStreamingIsPrepared();
}

void FDefaultGameMoviePlayer::SetupLoadingScreenFromIni()
{
	// We may have already setup a movie from a startup module
	if( !LoadingScreenAttributes.IsValid() )
	{
		// fill out the attributes
		FLoadingScreenAttributes LoadingScreen;

		bool bWaitForMoviesToComplete = false;
		// Note: this code is executed too early so we cannot access UMoviePlayerSettings because the configs for that object have not been loaded and coalesced .  Have to read directly from the configs instead
		GConfig->GetBool(TEXT("/Script/MoviePlayer.MoviePlayerSettings"), TEXT("bWaitForMoviesToComplete"), bWaitForMoviesToComplete, GGameIni);
		GConfig->GetBool(TEXT("/Script/MoviePlayer.MoviePlayerSettings"), TEXT("bMoviesAreSkippable"), LoadingScreen.bMoviesAreSkippable, GGameIni);

		LoadingScreen.bAutoCompleteWhenLoadingCompletes = !bWaitForMoviesToComplete;

		TArray<FString> StartupMovies;
		GConfig->GetArray(TEXT("/Script/MoviePlayer.MoviePlayerSettings"), TEXT("StartupMovies"), StartupMovies, GGameIni);

		if (StartupMovies.Num() == 0)
		{
			StartupMovies.Add(TEXT("Default_Startup"));
		}

		// double check that the movies exist
		// We dont know the extension so compare against any file in the directory with the same name for now
		// @todo New Movie Player: movies should have the extension on them when set via the project settings
		TArray<FString> ExistingMovieFiles;
		IFileManager::Get().FindFiles(ExistingMovieFiles, *(FPaths::ProjectContentDir() + TEXT("Movies")));

		bool bHasValidMovie = false;
		for(const FString& Movie : StartupMovies)
		{
			bool bFound = ExistingMovieFiles.ContainsByPredicate(
				[&Movie](const FString& ExistingMovie)
				{
					return ExistingMovie.Contains(Movie);
				});

			if(bFound)
			{
				bHasValidMovie = true;
				LoadingScreen.MoviePaths.Add(Movie);
			}
		}

		if(bHasValidMovie)
		{
			// These movies are all considered safe to play in very early startup sequences
			LoadingScreen.bAllowInEarlyStartup = true;

			// now setup the actual loading screen
			SetupLoadingScreen(LoadingScreen);
		}
	}
}

bool FDefaultGameMoviePlayer::MovieStreamingIsPrepared() const
{
	return MovieStreamer.IsValid() && LoadingScreenAttributes.MoviePaths.Num() > 0;
}

FVector2D FDefaultGameMoviePlayer::GetMovieSize() const
{
	const FVector2D ScreenSize = MainWindow.Pin()->GetClientSizeInScreen();
	if (MovieStreamingIsPrepared())
	{
		const float MovieAspectRatio = MovieStreamer->GetAspectRatio();
		const float ScreenAspectRatio = ScreenSize.X / ScreenSize.Y;
		if (MovieAspectRatio < ScreenAspectRatio)
		{
			return FVector2D(ScreenSize.Y * MovieAspectRatio, ScreenSize.Y);
		}
		else
		{
			return FVector2D(ScreenSize.X, ScreenSize.X / MovieAspectRatio);
		}
	}

	// No movie, so simply return the size of the window
	return ScreenSize;
}

FOptionalSize FDefaultGameMoviePlayer::GetMovieWidth() const
{
	return GetMovieSize().X;
}

FOptionalSize FDefaultGameMoviePlayer::GetMovieHeight() const
{
	return GetMovieSize().Y;
}

EVisibility FDefaultGameMoviePlayer::GetSlateBackgroundVisibility() const
{
	return MovieStreamingIsPrepared() && !IsMovieStreamingFinished() ? EVisibility::Collapsed : EVisibility::Visible;
}

EVisibility FDefaultGameMoviePlayer::GetViewportVisibility() const
{
	return MovieStreamingIsPrepared() && !IsMovieStreamingFinished() ? EVisibility::Visible : EVisibility::Collapsed;
}

FReply FDefaultGameMoviePlayer::OnLoadingScreenMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& PointerEvent)
{
	return OnAnyDown();
}

FReply FDefaultGameMoviePlayer::OnLoadingScreenKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent)
{
	return OnAnyDown();
}

FReply FDefaultGameMoviePlayer::OnAnyDown()
{
	if (IsLoadingFinished())
	{
		if (LoadingScreenAttributes.bMoviesAreSkippable)
		{
			MovieStreamingIsDone.Set(1);
			if (MovieStreamer.IsValid())
			{
				MovieStreamer->ForceCompletion();
			}
		}

		if (IsMovieStreamingFinished())
		{
			bUserCalledFinish = true;
		}
	}

	return FReply::Handled();
}

void FDefaultGameMoviePlayer::OnPreLoadMap(const FString& LevelName)
{
	FCoreUObjectDelegates::PostLoadMapWithWorld.RemoveAll(this);

	if( PlayMovie() )
	{
		FCoreUObjectDelegates::PostLoadMapWithWorld.AddRaw(this, &FDefaultGameMoviePlayer::OnPostLoadMap );
	}
}

void FDefaultGameMoviePlayer::OnPostLoadMap(UWorld* LoadedWorld)
{
	WaitForMovieToFinish();
}

void FDefaultGameMoviePlayer::SetSlateOverlayWidget(TSharedPtr<SWidget> NewOverlayWidget)
{
	if (MovieStreamer.IsValid() && UserWidgetHolder.IsValid())
	{
		UserWidgetHolder->SetContent(NewOverlayWidget.ToSharedRef());
	}
}

bool FDefaultGameMoviePlayer::WillAutoCompleteWhenLoadFinishes()
{
	return LoadingScreenAttributes.bAutoCompleteWhenLoadingCompletes || (LoadingScreenAttributes.PlaybackType == MT_LoadingLoop && (MovieStreamer.IsValid() && MovieStreamer->IsLastMovieInPlaylist()));
}

FString FDefaultGameMoviePlayer::GetMovieName()
{
	return MovieStreamer.IsValid() ? MovieStreamer->GetMovieName() : TEXT("");
}

bool FDefaultGameMoviePlayer::IsLastMovieInPlaylist()
{
	return MovieStreamer.IsValid() ? MovieStreamer->IsLastMovieInPlaylist() : false;
}

FMoviePlayerWidgetRenderer::FMoviePlayerWidgetRenderer(TSharedPtr<SWindow> InMainWindow, TSharedPtr<SVirtualWindow> InVirtualRenderWindow, FSlateRenderer* InRenderer)
	: MainWindow(InMainWindow.Get())
	, VirtualRenderWindow(InVirtualRenderWindow.ToSharedRef())
	, SlateRenderer(InRenderer)
{
	HittestGrid = MakeShareable(new FHittestGrid);
}

void FMoviePlayerWidgetRenderer::DrawWindow(float DeltaTime)
{
	FVector2D DrawSize = VirtualRenderWindow->GetClientSizeInScreen();

	FSlateApplication::Get().Tick(ESlateTickType::TimeOnly);

	const float Scale = 1.0f;
	FGeometry WindowGeometry = FGeometry::MakeRoot(DrawSize, FSlateLayoutTransform(Scale));

	VirtualRenderWindow->SlatePrepass(WindowGeometry.Scale);

	FSlateRect ClipRect = WindowGeometry.GetLayoutBoundingRect();

	HittestGrid->ClearGridForNewFrame(ClipRect);

	// Get the free buffer & add our virtual window
	FSlateDrawBuffer& DrawBuffer = SlateRenderer->GetDrawBuffer();
	FSlateWindowElementList& WindowElementList = DrawBuffer.AddWindowElementList(VirtualRenderWindow);

	WindowElementList.SetRenderTargetWindow(MainWindow);

	int32 MaxLayerId = 0;
	{
		FPaintArgs PaintArgs(*VirtualRenderWindow, *HittestGrid, FVector2D::ZeroVector, FSlateApplication::Get().GetCurrentTime(), FSlateApplication::Get().GetDeltaTime());

		// Paint the window
		MaxLayerId = VirtualRenderWindow->Paint(
			PaintArgs,
			WindowGeometry, ClipRect,
			WindowElementList,
			0,
			FWidgetStyle(),
			VirtualRenderWindow->IsEnabled());
	}

	SlateRenderer->DrawWindows(DrawBuffer);

	DrawBuffer.ViewOffset = FVector2D::ZeroVector;
}

float FDefaultGameMoviePlayer::GetViewportDPIScale() const
{
	return 1.f;
}