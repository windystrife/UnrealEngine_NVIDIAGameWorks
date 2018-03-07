// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GameEngine.cpp: Unreal game engine.
=============================================================================*/

#include "Engine/GameEngine.h"
#include "GenericPlatform/GenericPlatformSurvey.h"
#include "Misc/CommandLine.h"
#include "Misc/TimeGuard.h"
#include "Misc/App.h"
#include "GameMapsSettings.h"
#include "EngineStats.h"
#include "EngineGlobals.h"
#include "RenderingThread.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LevelStreaming.h"
#include "Engine/PlatformInterfaceBase.h"
#include "ContentStreaming.h"
#include "UnrealEngine.h"
#include "HAL/PlatformSplash.h"
#include "UObject/Package.h"
#include "GameFramework/GameModeBase.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "AudioDeviceManager.h"
#include "Net/NetworkProfiler.h"
#include "RendererInterface.h"
#include "EngineModule.h"
#include "GeneralProjectSettings.h"
#include "Misc/PackageName.h"
#include "HAL/PlatformApplicationMisc.h"

#include "Slate/SceneViewport.h"

#include "IMovieSceneCapture.h"
#include "MovieSceneCaptureModule.h"

#include "SynthBenchmark.h"

#include "SceneViewExtension.h"
#include "Misc/HotReloadInterface.h"
#include "Engine/LocalPlayer.h"
#include "Slate/SGameLayerManager.h"
#include "Components/SkyLightComponent.h"
#include "Components/ReflectionCaptureComponent.h"
#include "GameFramework/GameUserSettings.h"
#include "GameDelegates.h"
#include "Engine/CoreSettings.h"
#include "EngineAnalytics.h"
#include "Engine/DemoNetDriver.h"

#include "Tickable.h"
#include "AssetRegistryModule.h"


ENGINE_API bool GDisallowNetworkTravel = false;

// How slow must a frame be (in seconds) to be logged out (<= 0 to disable)
ENGINE_API float GSlowFrameLoggingThreshold = 0.0f;

static FAutoConsoleVariableRef CvarSlowFrameLoggingThreshold(
	TEXT("t.SlowFrameLoggingThreshold"),
	GSlowFrameLoggingThreshold,
	TEXT("How slow must a frame be (in seconds) to be logged out (<= 0 to disable)."),
	ECVF_Default
	);

static int32 GDoAsyncEndOfFrameTasks = 0;
static FAutoConsoleVariableRef CVarDoAsyncEndOfFrameTasks(
	TEXT("tick.DoAsyncEndOfFrameTasks"),
	GDoAsyncEndOfFrameTasks,
	TEXT("Experimental option to run various things concurrently with the HUD render.")
	);

/** Benchmark results to the log */
static void RunSynthBenchmark(const TArray<FString>& Args)
{
	float WorkScale = 10.0f;

	if ( Args.Num() > 0 )
	{
		WorkScale = FCString::Atof(*Args[0]);
		WorkScale = FMath::Clamp(WorkScale, 1.0f, 1000.0f);
	}

	FSynthBenchmarkResults Result;
	ISynthBenchmark::Get().Run(Result, true, WorkScale);
}

/** Helper function to generate a set of windowed resolutions which are convenient for the current primary display size */
void GenerateConvenientWindowedResolutions(const struct FDisplayMetrics& InDisplayMetrics, TArray<FIntPoint>& OutResolutions)
{
	bool bInPortraitMode = InDisplayMetrics.PrimaryDisplayWidth < InDisplayMetrics.PrimaryDisplayHeight;

	// Generate windowed resolutions as scaled versions of primary monitor size
	static const float Scales[] = { 3.0f / 6.0f, 4.0f / 6.0f, 4.5f / 6.0f, 5.0f / 6.0f };
	static const float Ratios[] = { 9.0f, 10.0f, 12.0f };
	static const float MinWidth = 1280.0f;
	static const float MinHeight = 720.0f; // UI layout doesn't work well below this, as the accept/cancel buttons go off the bottom of the screen

	static const uint32 NumScales = sizeof(Scales) / sizeof(float);
	static const uint32 NumRatios = sizeof(Ratios) / sizeof(float);

	for (uint32 ScaleIndex = 0; ScaleIndex < NumScales; ++ScaleIndex)
	{
		for (uint32 AspectIndex = 0; AspectIndex < NumRatios; ++AspectIndex)
		{
			float TargetWidth, TargetHeight;
			float Aspect = Ratios[AspectIndex] / 16.0f;

			if (bInPortraitMode)
			{
				TargetHeight = FMath::RoundToFloat(InDisplayMetrics.PrimaryDisplayHeight * Scales[ScaleIndex]);
				TargetWidth = TargetHeight * Aspect;
			}
			else
			{
				TargetWidth = FMath::RoundToFloat(InDisplayMetrics.PrimaryDisplayWidth * Scales[ScaleIndex]);
				TargetHeight = TargetWidth * Aspect;
			}

			if (TargetWidth < InDisplayMetrics.PrimaryDisplayWidth && TargetHeight < InDisplayMetrics.PrimaryDisplayHeight && TargetWidth >= MinWidth && TargetHeight >= MinHeight)
			{
				OutResolutions.Add(FIntPoint(TargetWidth, TargetHeight));
			}
		}
	}
	
	// if no convenient resolutions have been found, add a minimum one
	if (OutResolutions.Num() == 0)
	{
		if (InDisplayMetrics.PrimaryDisplayHeight > MinHeight && InDisplayMetrics.PrimaryDisplayWidth > MinWidth)
		{
			//Add the minimum size if it fit
			OutResolutions.Add(FIntPoint(MinWidth, MinHeight));
		}
		else
		{
			//Force a resolution even if its smaller then the minimum height and width to avoid a bigger window then the desktop
			float TargetWidth = FMath::RoundToFloat(InDisplayMetrics.PrimaryDisplayWidth) * Scales[NumScales - 1];
			float TargetHeight = FMath::RoundToFloat(InDisplayMetrics.PrimaryDisplayHeight) * Scales[NumScales - 1];
			OutResolutions.Add(FIntPoint(TargetWidth, TargetHeight));
		}
	}
}

static FAutoConsoleCommand GSynthBenchmarkCmd(
	TEXT("SynthBenchmark"),
	TEXT("Run simple benchmark to get some metrics to find reasonable game settings automatically\n")
	TEXT("Optional (float) parameter allows to scale with work amount to trade time or precision (default: 10)."),
	FConsoleCommandWithArgsDelegate::CreateStatic(&RunSynthBenchmark)
	);

EWindowMode::Type GetWindowModeType(EWindowMode::Type WindowMode)
{
	return FPlatformProperties::SupportsWindowedMode() ? WindowMode : EWindowMode::Fullscreen;
}

UGameEngine::UGameEngine(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
}

/*-----------------------------------------------------------------------------
	cleanup!!
-----------------------------------------------------------------------------*/

void UGameEngine::CreateGameViewportWidget( UGameViewportClient* GameViewportClient )
{
	bool bRenderDirectlyToWindow = !StartupMovieCaptureHandle.IsValid() && GIsDumpingMovie == 0; 
	const bool bStereoAllowed = bRenderDirectlyToWindow;
	TSharedRef<SOverlay> ViewportOverlayWidgetRef = SNew( SOverlay );

	TSharedRef<SGameLayerManager> GameLayerManagerRef = SNew(SGameLayerManager)
		.SceneViewport_UObject(this, &UGameEngine::GetGameSceneViewport, GameViewportClient)
		[
			ViewportOverlayWidgetRef
		];

	TSharedRef<SViewport> GameViewportWidgetRef = 
		SNew( SViewport )
			// Render directly to the window backbuffer unless capturing a movie or getting screenshots
			// @todo TEMP
			.RenderDirectlyToWindow(bRenderDirectlyToWindow)
			//gamma handled by the scene renderer
			.EnableGammaCorrection(false)
			.EnableStereoRendering(bStereoAllowed)
			[
				GameLayerManagerRef
			];

	GameViewportWidget = GameViewportWidgetRef;

	GameViewportClient->SetViewportOverlayWidget( GameViewportWindow.Pin(), ViewportOverlayWidgetRef );
	GameViewportClient->SetGameLayerManager(GameLayerManagerRef);
}

void UGameEngine::CreateGameViewport( UGameViewportClient* GameViewportClient )
{
	check(GameViewportWindow.IsValid());

	if( !GameViewportWidget.IsValid() )
	{
		CreateGameViewportWidget( GameViewportClient );
	}
	TSharedRef<SViewport> GameViewportWidgetRef = GameViewportWidget.ToSharedRef();

	auto Window = GameViewportWindow.Pin();

	Window->SetOnWindowClosed( FOnWindowClosed::CreateUObject( this, &UGameEngine::OnGameWindowClosed ) );

	// SAVEWINPOS tells us to load/save window positions to user settings (this is disabled by default)
	int32 SaveWinPos;
	if (FParse::Value(FCommandLine::Get(), TEXT("SAVEWINPOS="), SaveWinPos) && SaveWinPos > 0 )
	{
		// Get WinX/WinY from GameSettings, apply them if valid.
		FIntPoint PiePosition = GetGameUserSettings()->GetWindowPosition();
		if (PiePosition.X >= 0 && PiePosition.Y >= 0)
		{
			int32 WinX = GetGameUserSettings()->GetWindowPosition().X;
			int32 WinY = GetGameUserSettings()->GetWindowPosition().Y;
			Window->MoveWindowTo(FVector2D(WinX, WinY));
		}
		Window->SetOnWindowMoved( FOnWindowMoved::CreateUObject( this, &UGameEngine::OnGameWindowMoved ) );
	}

	SceneViewport = MakeShareable( new FSceneViewport( GameViewportClient, GameViewportWidgetRef ) );
	GameViewportClient->Viewport = SceneViewport.Get();
	//GameViewportClient->CreateHighresScreenshotCaptureRegionWidget(); //  Disabled until mouse based input system can be made to work correctly.

	// The viewport widget needs an interface so it knows what should render
	GameViewportWidgetRef->SetViewportInterface( SceneViewport.ToSharedRef() );

	FViewportFrame* ViewportFrame = SceneViewport.Get();

	GameViewport->SetViewportFrame(ViewportFrame);
}

const FSceneViewport* UGameEngine::GetGameSceneViewport(UGameViewportClient* ViewportClient) const
{
	return ViewportClient->GetGameViewport();
}

void UGameEngine::ConditionallyOverrideSettings(int32& ResolutionX, int32& ResolutionY, EWindowMode::Type& WindowMode)
{
	if (FParse::Param(FCommandLine::Get(), TEXT("Windowed")) || FParse::Param(FCommandLine::Get(), TEXT("SimMobile")))
	{
		// -Windowed or -SimMobile
		WindowMode = EWindowMode::Windowed;
	}
	else if (FParse::Param(FCommandLine::Get(), TEXT("FullScreen")))
	{
		// -FullScreen
		static auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.FullScreenMode"));
		check(CVar);
		WindowMode = CVar->GetValueOnGameThread() == 0 ? EWindowMode::Fullscreen : EWindowMode::WindowedFullscreen;

		if (PLATFORM_WINDOWS && WindowMode == EWindowMode::Fullscreen)
		{
			// Handle fullscreen mode differently for D3D11/D3D12
			static const bool bD3D12 = FParse::Param(FCommandLine::Get(), TEXT("d3d12")) || FParse::Param(FCommandLine::Get(), TEXT("dx12"));
			if (bD3D12)
			{
				// Force D3D12 RHI to use windowed fullscreen mode
				WindowMode = EWindowMode::WindowedFullscreen;
			}
		}
	}

	DetermineGameWindowResolution(ResolutionX, ResolutionY, WindowMode);
}

void UGameEngine::DetermineGameWindowResolution( int32& ResolutionX, int32& ResolutionY, EWindowMode::Type& WindowMode )
{
	//fullscreen is always supported, but don't allow windowed mode on platforms that dont' support it.
	WindowMode = (!FPlatformProperties::SupportsWindowedMode() && (WindowMode == EWindowMode::Windowed || WindowMode == EWindowMode::WindowedFullscreen)) ? EWindowMode::Fullscreen : WindowMode;

	FParse::Value(FCommandLine::Get(), TEXT("ResX="), ResolutionX);
	FParse::Value(FCommandLine::Get(), TEXT("ResY="), ResolutionY);

	// consume available desktop area
	FDisplayMetrics DisplayMetrics;
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().GetInitialDisplayMetrics(DisplayMetrics);
	}
	else
	{
		FDisplayMetrics::GetDisplayMetrics(DisplayMetrics);
	}

	// Find the maximum allowed resolution
	// Use PrimaryDisplayWidth/Height in windowed mode
	int32 MaxResolutionX = DisplayMetrics.PrimaryDisplayWidth;
	int32 MaxResolutionY = DisplayMetrics.PrimaryDisplayHeight;
	if (WindowMode == EWindowMode::Fullscreen && DisplayMetrics.MonitorInfo.Num() > 0)
	{
		// In fullscreen, PrimaryDisplayWidth/Height is equal to your current resolution, so we will use your max native resolution instead
		// Since we have info for at least one monitor, default to that if the primary can not be found
		MaxResolutionX = DisplayMetrics.MonitorInfo[0].NativeWidth;
		MaxResolutionY = DisplayMetrics.MonitorInfo[0].NativeHeight;

		// Now try to find the primary monitor
		for (const FMonitorInfo& MonitorInfo : DisplayMetrics.MonitorInfo)
		{
			if (MonitorInfo.bIsPrimary)
			{
				// This is the primary monitor. Use this monitor's native width/height
				MaxResolutionX = MonitorInfo.NativeWidth;
				MaxResolutionY = MonitorInfo.NativeHeight;
				break;
			}
		}
	}

	// Optionally force the resolution by passing -ForceRes
	const bool bForceRes = FParse::Param(FCommandLine::Get(), TEXT("ForceRes"));

	//Don't allow a resolution bigger then the desktop found a convenient one
	if (!bForceRes && !IsRunningDedicatedServer() && ((ResolutionX <= 0 || ResolutionX > MaxResolutionX) || (ResolutionY <= 0 || ResolutionY > MaxResolutionY)))
	{
		ResolutionX = MaxResolutionX;
		ResolutionY = MaxResolutionY;

		// If we're in windowed mode, attempt to choose a suitable starting resolution that is smaller than the desktop, with a matching aspect ratio
		if (WindowMode == EWindowMode::Windowed)
		{
			TArray<FIntPoint> WindowedResolutions;
			GenerateConvenientWindowedResolutions(DisplayMetrics, WindowedResolutions);

			if (WindowedResolutions.Num() > 0)
			{
				// We'll default to the largest one we have
				ResolutionX = WindowedResolutions[WindowedResolutions.Num() - 1].X;
				ResolutionY = WindowedResolutions[WindowedResolutions.Num() - 1].Y;

				// Attempt to find the largest one with the same aspect ratio
				float DisplayAspect = (float)DisplayMetrics.PrimaryDisplayWidth / (float)DisplayMetrics.PrimaryDisplayHeight;
				for (int32 i = WindowedResolutions.Num() - 1; i >= 0; --i)
				{
					float Aspect = (float)WindowedResolutions[i].X / (float)WindowedResolutions[i].Y;
					if (FMath::Abs(Aspect - DisplayAspect) < KINDA_SMALL_NUMBER)
					{
						ResolutionX = WindowedResolutions[i].X;
						ResolutionY = WindowedResolutions[i].Y;
						break;
					}
				}
			}
		}
	}

	// Check the platform to see if we should override the user settings.
	if (FPlatformProperties::HasFixedResolution())
	{
		// We need to pass the resolution back out to GameUserSettings, or it will just override it again
		ResolutionX = DisplayMetrics.PrimaryDisplayWorkAreaRect.Right - DisplayMetrics.PrimaryDisplayWorkAreaRect.Left;
		ResolutionY = DisplayMetrics.PrimaryDisplayWorkAreaRect.Bottom - DisplayMetrics.PrimaryDisplayWorkAreaRect.Top;
		FSystemResolution::RequestResolutionChange(ResolutionX, ResolutionY, EWindowMode::Fullscreen);
	}


	if (FParse::Param(FCommandLine::Get(), TEXT("Portrait")))
	{
		Swap(ResolutionX, ResolutionY);
	}
}

TSharedRef<SWindow> UGameEngine::CreateGameWindow()
{
	int32 ResX = GSystemResolution.ResX;
	int32 ResY = GSystemResolution.ResY;
	EWindowMode::Type WindowMode = GSystemResolution.WindowMode;

	ConditionallyOverrideSettings(ResX, ResY, WindowMode);

	// If the current settings have been overridden, apply them back into the system
	if (ResX != GSystemResolution.ResX || ResY != GSystemResolution.ResY || WindowMode != GSystemResolution.WindowMode)
	{
		FSystemResolution::RequestResolutionChange(ResX, ResY, WindowMode);
		IConsoleManager::Get().CallAllConsoleVariableSinks();
	}

	const FText WindowTitleOverride = GetDefault<UGeneralProjectSettings>()->ProjectDisplayedTitle;
	const FText WindowTitleComponent = WindowTitleOverride.IsEmpty() ? NSLOCTEXT("UnrealEd", "GameWindowTitle", "{GameName}") : WindowTitleOverride;

	FText WindowDebugInfoComponent;
#if !UE_BUILD_SHIPPING
	const FText WindowDebugInfoOverride = GetDefault<UGeneralProjectSettings>()->ProjectDebugTitleInfo;
	WindowDebugInfoComponent = WindowDebugInfoOverride.IsEmpty() ? NSLOCTEXT("UnrealEd", "GameWindowTitleDebugInfo", "({PlatformArchitecture}-bit, {RHIName})") : WindowDebugInfoOverride;
#endif

#if PLATFORM_64BITS
	//These are invariant strings so they don't need to be localized
	const FText PlatformBits = FText::FromString( TEXT( "64" ) );
#else	//PLATFORM_64BITS
	const FText PlatformBits = FText::FromString( TEXT( "32" ) );
#endif	//PLATFORM_64BITS

	// Note: If these parameters are updated or renamed, please update the tooltip on the ProjectDisplayedTitle and ProjectDebugTitleInfo properties
	FFormatNamedArguments Args;
	Args.Add( TEXT("GameName"), FText::FromString( FApp::GetProjectName() ) );
	Args.Add( TEXT("PlatformArchitecture"), PlatformBits );
	Args.Add( TEXT("RHIName"), FText::FromName( LegacyShaderPlatformToShaderFormat( GMaxRHIShaderPlatform ) ) );

	const FText WindowTitleVar = FText::Format(FText::FromString(TEXT("{0} {1}")), WindowTitleComponent, WindowDebugInfoComponent);
	const FText WindowTitle = FText::Format(WindowTitleVar, Args);
	const bool bShouldPreserveAspectRatio = GetDefault<UGeneralProjectSettings>()->bShouldWindowPreserveAspectRatio;
	const bool bUseBorderlessWindow = GetDefault<UGeneralProjectSettings>()->bUseBorderlessWindow;
	const bool bAllowWindowResize = GetDefault<UGeneralProjectSettings>()->bAllowWindowResize;
	const bool bAllowClose = GetDefault<UGeneralProjectSettings>()->bAllowClose;
	const bool bAllowMaximize = GetDefault<UGeneralProjectSettings>()->bAllowMaximize;
	const bool bAllowMinimize = GetDefault<UGeneralProjectSettings>()->bAllowMinimize;

	// Allow optional winX/winY parameters to set initial window position
	EAutoCenter AutoCenterType = EAutoCenter::PrimaryWorkArea;
	int32 WinX=0;
	int32 WinY=0;
	if (FParse::Value(FCommandLine::Get(), TEXT("WinX="), WinX) && FParse::Value(FCommandLine::Get(), TEXT("WinY="), WinY))
	{
		AutoCenterType = EAutoCenter::None;
	}

	// Give the window the max width/height of either the requested resolution, or your available desktop resolution
	// We need to do this as we request some 4K windows when rendering sequences, and the OS may try and clamp that
	// window to your available desktop resolution
	TOptional<float> MaxWindowWidth;
	TOptional<float> MaxWindowHeight;
	if (WindowMode == EWindowMode::Windowed)
	{
		// Get available desktop area
		FDisplayMetrics DisplayMetrics;
		if (FSlateApplication::IsInitialized())
		{
			FSlateApplication::Get().GetInitialDisplayMetrics(DisplayMetrics);
		}
		else
		{
			FDisplayMetrics::GetDisplayMetrics(DisplayMetrics);
		}

		MaxWindowWidth = FMath::Max(DisplayMetrics.VirtualDisplayRect.Right - DisplayMetrics.VirtualDisplayRect.Left, ResX);
		MaxWindowHeight = FMath::Max(DisplayMetrics.VirtualDisplayRect.Bottom - DisplayMetrics.VirtualDisplayRect.Top, ResY);
	}

	static FWindowStyle BorderlessStyle = FWindowStyle::GetDefault();
	BorderlessStyle
		.SetActiveTitleBrush(FSlateNoResource())
		.SetInactiveTitleBrush(FSlateNoResource())
		.SetFlashTitleBrush(FSlateNoResource())
		.SetOutlineBrush(FSlateNoResource())
		.SetBorderBrush(FSlateNoResource())
		.SetBackgroundBrush(FSlateNoResource())
		.SetChildBackgroundBrush(FSlateNoResource());

	TSharedRef<SWindow> Window = SNew(SWindow)
	.Type(EWindowType::GameWindow)
	.Style(bUseBorderlessWindow ? &BorderlessStyle : &FCoreStyle::Get().GetWidgetStyle<FWindowStyle>("Window"))
	.ClientSize(FVector2D(ResX, ResY))
	.Title(WindowTitle)
	.AutoCenter(AutoCenterType)
	.ScreenPosition(FVector2D(WinX, WinY))
	.MaxWidth(MaxWindowWidth)
	.MaxHeight(MaxWindowHeight)
	.FocusWhenFirstShown(true)
	.SaneWindowPlacement(AutoCenterType == EAutoCenter::None)
	.UseOSWindowBorder(!bUseBorderlessWindow)
	.CreateTitleBar(!bUseBorderlessWindow)
	.ShouldPreserveAspectRatio(bShouldPreserveAspectRatio)
	.LayoutBorder(bUseBorderlessWindow ? FMargin(0) : FMargin(5, 5, 5, 5))
	.SizingRule(bAllowWindowResize ? ESizingRule::UserSized : ESizingRule::FixedSize)
	.HasCloseButton(bAllowClose)
	.SupportsMinimize(bAllowMinimize)
	.SupportsMaximize(bAllowMaximize);

	const bool bShowImmediately = false;

	FSlateApplication::Get().AddWindow( Window, bShowImmediately );
	
	// Do not set fullscreen mode here, since it doesn't take 
	// HMDDevice into account. The window mode will be set properly later
	// from SwitchGameWindowToUseGameViewport() method (see ResizeWindow call).
	if (WindowMode == EWindowMode::Fullscreen)
	{
		Window->SetWindowMode(EWindowMode::WindowedFullscreen);
	}
	else
	{
		Window->SetWindowMode(WindowMode);
	}

	Window->ShowWindow();

	// Tick now to force a redraw of the window and ensure correct fullscreen application
	FSlateApplication::Get().Tick();

	return Window;
}

void UGameEngine::SwitchGameWindowToUseGameViewport()
{
	if (GameViewportWindow.IsValid() && GameViewportWindow.Pin()->GetContent() != GameViewportWidget)
	{
		if( !GameViewportWidget.IsValid() )
		{
			CreateGameViewport( GameViewport );
		}
		
		TSharedRef<SViewport> GameViewportWidgetRef = GameViewportWidget.ToSharedRef();
		TSharedPtr<SWindow> GameViewportWindowPtr = GameViewportWindow.Pin();
		
		GameViewportWindowPtr->SetContent(GameViewportWidgetRef);
		GameViewportWindowPtr->SlatePrepass();
		
		if ( SceneViewport.IsValid() )
		{
			SceneViewport->ResizeFrame((uint32)GSystemResolution.ResX, (uint32)GSystemResolution.ResY, GSystemResolution.WindowMode);
		}

		// Registration of the game viewport to that messages are correctly received.
		// Could be a re-register, however it's necessary after the window is set.
		FSlateApplication::Get().RegisterGameViewport(GameViewportWidgetRef);

		if (FSlateApplication::IsInitialized())
		{
			FSlateApplication::Get().SetAllUserFocusToGameViewport(EFocusCause::SetDirectly);
		}
	}
}

void UGameEngine::OnGameWindowClosed( const TSharedRef<SWindow>& WindowBeingClosed )
{
	FSlateApplication::Get().UnregisterGameViewport();
	// This will shutdown the game
	GameViewport->CloseRequested( SceneViewport->GetViewport() );
	SceneViewport.Reset();
}

void UGameEngine::OnGameWindowMoved( const TSharedRef<SWindow>& WindowBeingMoved )
{
	const FSlateRect WindowRect = WindowBeingMoved->GetRectInScreen();
	GetGameUserSettings()->SetWindowPosition(WindowRect.Left, WindowRect.Top);
	GetGameUserSettings()->SaveConfig();
}

void UGameEngine::RedrawViewports( bool bShouldPresent /*= true*/ )
{
	SCOPE_CYCLE_COUNTER(STAT_RedrawViewports);

	if ( GameViewport != NULL )
	{
		GameViewport->LayoutPlayers();
		if ( GameViewport->Viewport != NULL )
		{
			GameViewport->Viewport->Draw(bShouldPresent);
		}
	}
}

/*-----------------------------------------------------------------------------
	Game init and exit.
-----------------------------------------------------------------------------*/
UEngine::UEngine(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, ViewExtensions( new FSceneViewExtensions() )
{
	C_WorldBox = FColor(0, 0, 40, 255);
	C_BrushWire = FColor(192, 0, 0, 255);
	C_AddWire = FColor(127, 127, 255, 255);
	C_SubtractWire = FColor(255, 127, 127, 255);
	C_SemiSolidWire = FColor(127, 255, 0, 255);
	C_NonSolidWire = FColor(63, 192, 32, 255);
	C_WireBackground = FColor(0, 0, 0, 255);
	C_ScaleBoxHi = FColor(223, 149, 157, 255);
	C_VolumeCollision = FColor(149, 223, 157, 255);
	C_BSPCollision = FColor(149, 157, 223, 255);
	C_OrthoBackground = FColor(30, 30, 30, 255);
	C_Volume = FColor(255, 196, 255, 255);
	C_BrushShape = FColor(128, 255, 128, 255);

	SelectionHighlightIntensity = 0.0f;
#if WITH_EDITOR
	SelectionMeshSectionHighlightIntensity = 0.2f;
#endif
	BSPSelectionHighlightIntensity = 0.0f;
	HoverHighlightIntensity = 10.f;

	SelectionHighlightIntensityBillboards = 0.25f;

	bUseSound = true;

	bHardwareSurveyEnabled_DEPRECATED = true;
	bIsInitialized = false;

	BeginStreamingPauseDelegate = NULL;
	EndStreamingPauseDelegate = NULL;

	bCanBlueprintsTickByDefault = true;
	bOptimizeAnimBlueprintMemberVariableAccess = true;
	bAllowMultiThreadedAnimationUpdate = true;

	bUseFixedFrameRate = false;
	FixedFrameRate = 30.f;

	bIsVanillaProduct = false;

	GameScreenshotSaveDirectory.Path = FPaths::ScreenShotDir();

	LastGCFrame = TNumericLimits<uint64>::Max();
}

void UGameEngine::Init(IEngineLoop* InEngineLoop)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("UGameEngine Init"), STAT_GameEngineStartup, STATGROUP_LoadTime);

	// Call base.
	UEngine::Init(InEngineLoop);

#if USE_NETWORK_PROFILER
	FString NetworkProfilerTag;
	if( FParse::Value(FCommandLine::Get(), TEXT("NETWORKPROFILER="), NetworkProfilerTag ) )
	{
		GNetworkProfiler.EnableTracking(true);
	}
#endif

	// Load and apply user game settings
	GetGameUserSettings()->LoadSettings();
	GetGameUserSettings()->ApplyNonResolutionSettings();

	// Create game instance.  For GameEngine, this should be the only GameInstance that ever gets created.
	{
		FSoftClassPath GameInstanceClassName = GetDefault<UGameMapsSettings>()->GameInstanceClass;
		UClass* GameInstanceClass = (GameInstanceClassName.IsValid() ? LoadObject<UClass>(NULL, *GameInstanceClassName.ToString()) : UGameInstance::StaticClass());
		
		if (GameInstanceClass == nullptr)
		{
			UE_LOG(LogEngine, Error, TEXT("Unable to load GameInstance Class '%s'. Falling back to generic UGameInstance."), *GameInstanceClassName.ToString());
			GameInstanceClass = UGameInstance::StaticClass();
		}

		GameInstance = NewObject<UGameInstance>(this, GameInstanceClass);

		GameInstance->InitializeStandalone();
	}
 
//  	// Creates the initial world context. For GameEngine, this should be the only WorldContext that ever gets created.
//  	FWorldContext& InitialWorldContext = CreateNewWorldContext(EWorldType::Game);

	IMovieSceneCaptureInterface* MovieSceneCaptureImpl = nullptr;
#if WITH_EDITOR
	if (!IsRunningDedicatedServer() && !IsRunningCommandlet())
	{
		MovieSceneCaptureImpl = IMovieSceneCaptureModule::Get().InitializeFromCommandLine();
		if (MovieSceneCaptureImpl)
		{
			StartupMovieCaptureHandle = MovieSceneCaptureImpl->GetHandle();
		}
	}
#endif

	// Initialize the viewport client.
	UGameViewportClient* ViewportClient = NULL;
	if(GIsClient)
	{
		ViewportClient = NewObject<UGameViewportClient>(this, GameViewportClientClass);
		ViewportClient->Init(*GameInstance->GetWorldContext(), GameInstance);
		GameViewport = ViewportClient;
		GameInstance->GetWorldContext()->GameViewport = ViewportClient;
	}

	LastTimeLogsFlushed = FPlatformTime::Seconds();

	// Attach the viewport client to a new viewport.
	if(ViewportClient)
	{
		// This must be created before any gameplay code adds widgets
		bool bWindowAlreadyExists = GameViewportWindow.IsValid();
		if (!bWindowAlreadyExists)
		{
			UE_LOG(LogEngine, Log, TEXT("GameWindow did not exist.  Was created"));
			GameViewportWindow = CreateGameWindow();
		}

		CreateGameViewport( ViewportClient );

		if( !bWindowAlreadyExists )
		{
			SwitchGameWindowToUseGameViewport();
		}

		FString Error;
		if(ViewportClient->SetupInitialLocalPlayer(Error) == NULL)
		{
			UE_LOG(LogEngine, Fatal,TEXT("%s"),*Error);
		}

		UGameViewportClient::OnViewportCreated().Broadcast();
	}

	UE_LOG(LogInit, Display, TEXT("Game Engine Initialized.") );

	// for IsInitialized()
	bIsInitialized = true;
}

void UGameEngine::Start()
{
	UE_LOG(LogInit, Display, TEXT("Starting Game."));

	GameInstance->StartGameInstance();
}

void UGameEngine::PreExit()
{
	// Stop tracking, automatically flushes.
	NETWORK_PROFILER(GNetworkProfiler.EnableTracking(false));

	CancelAllPending();

	// Clean up all worlds
	for (int32 WorldIndex = 0; WorldIndex < WorldList.Num(); ++WorldIndex)
	{
		UWorld* const World = WorldList[WorldIndex].World();
		if ( World != NULL )
		{
			World->bIsTearingDown = true;

			// Cancel any pending connection to a server
			CancelPending(World);

			// Shut down any existing game connections
			ShutdownWorldNetDriver(World);

			for (FActorIterator ActorIt(World); ActorIt; ++ActorIt)
			{
				ActorIt->RouteEndPlay(EEndPlayReason::Quit);
			}

			if (World->GetGameInstance() != nullptr)
			{
				World->GetGameInstance()->Shutdown();
			}

			World->FlushLevelStreaming(EFlushLevelStreamingType::Visibility);
			World->CleanupWorld();
		}
	}

	Super::PreExit();
}

void UGameEngine::FinishDestroy()
{
	if ( !HasAnyFlags(RF_ClassDefaultObject) )
	{
		// Game exit.
		UE_LOG(LogExit, Log, TEXT("Game engine shut down") );
	}

	Super::FinishDestroy();
}

bool UGameEngine::NetworkRemapPath(UNetDriver* Driver, FString& Str, bool bReading /*= true*/)
{
	if (Driver == nullptr)
	{
		return false;
	}

	UWorld* const World = Driver->GetWorld();

	// If the driver is using a duplicate level ID, find the level collection using the driver
	// and see if any of its levels match the prefixed name. If so, remap Str to that level's
	// prefixed name.
	if (Driver->GetDuplicateLevelID() != INDEX_NONE && bReading)
	{
		const FName PrefixedName = *UWorld::ConvertToPIEPackageName(Str, Driver->GetDuplicateLevelID());

		for (const FLevelCollection& Collection : World->GetLevelCollections())
		{
			if (Collection.GetNetDriver() == Driver || Collection.GetDemoNetDriver() == Driver)
			{
				for (const ULevel* Level : Collection.GetLevels())
				{
					const UPackage* const CachedOutermost = Level ? Level->GetOutermost() : nullptr;
					if (CachedOutermost && CachedOutermost->GetFName() == PrefixedName)
					{
						Str = PrefixedName.ToString();
						return true;
					}
				}
			}
		}
	}

	if (!bReading)
	{
		return false;
	}

	// If the game has created multiple worlds, some of them may have prefixed package names,
	// so we need to remap the world package and streaming levels for replay playback to work correctly.
	FWorldContext& Context = GetWorldContextFromWorldChecked(World);
	if (Context.PIEInstance == INDEX_NONE)
	{
		// If this is not a PIE instance but sender is PIE, we need to strip the PIE prefix
		const FString Stripped = UWorld::RemovePIEPrefix(Str);
		if (!Stripped.Equals(Str, ESearchCase::CaseSensitive))
		{
			Str = Stripped;
			return true;
		}
		return false;
	}

	// If the prefixed path matches the world package name or the name of a streaming level,
	// return the prefixed name.
	FString PackageNameOnly = Str;
	FPackageName::TryConvertFilenameToLongPackageName(PackageNameOnly, PackageNameOnly);

	const FString PrefixedFullName = UWorld::ConvertToPIEPackageName(Str, Context.PIEInstance);
	const FString PrefixedPackageName = UWorld::ConvertToPIEPackageName(PackageNameOnly, Context.PIEInstance);
	const FString WorldPackageName = World->GetOutermost()->GetName();

	if (WorldPackageName == PrefixedPackageName)
	{
		Str = PrefixedFullName;
		return true;
	}

	for (ULevelStreaming* StreamingLevel : World->StreamingLevels)
	{
		if (StreamingLevel != nullptr)
		{
			const FString StreamingLevelName = StreamingLevel->GetWorldAsset().GetLongPackageName();
			if (StreamingLevelName == PrefixedPackageName)
			{
				Str = PrefixedFullName;
				return true;
			}
		}
	}

	return false;
}

bool UGameEngine::ShouldDoAsyncEndOfFrameTasks() const
{
	return FApp::ShouldUseThreadingForPerformance() && ENamedThreads::RenderThread != ENamedThreads::GameThread && !!GDoAsyncEndOfFrameTasks;
}

/*-----------------------------------------------------------------------------
	Command line executor.
-----------------------------------------------------------------------------*/

bool UGameEngine::Exec( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar )
{
	if( FParse::Command( &Cmd,TEXT("REATTACHCOMPONENTS")) || FParse::Command( &Cmd,TEXT("REREGISTERCOMPONENTS")))
	{
		UE_LOG(LogConsoleResponse, Warning, TEXT("Deprectated command! Please use 'Reattach.Components' instead."));
		return true;
	}
	else if( FParse::Command( &Cmd,TEXT("EXIT")) || FParse::Command(&Cmd,TEXT("QUIT")))
	{
		FString CmdName = FParse::Token(Cmd, 0);
		bool Background = false;
		if (!CmdName.IsEmpty() && !FCString::Stricmp(*CmdName, TEXT("background")))
		{
			Background = true;
		}

		if ( Background && FPlatformProperties::SupportsMinimize() )
		{
			return HandleMinimizeCommand( Cmd, Ar );
		}
		else if ( FPlatformProperties::SupportsQuit() )
		{
			return HandleExitCommand( Cmd, Ar );
		}
		else
		{
			// ignore command on xbox one and ps4 as it will cause a crash
			// ttp:321126
			return true;
		}
	}
	else if( FParse::Command( &Cmd, TEXT("GETMAXTICKRATE") ) )
	{
		return HandleGetMaxTickRateCommand( Cmd, Ar );
	}
	else if( FParse::Command( &Cmd, TEXT("CANCEL") ) )
	{
		return HandleCancelCommand( Cmd, Ar, InWorld );	
	}
	else if ( FParse::Command( &Cmd, TEXT("TOGGLECVAR") ) )
	{
		FString CVarName;
		FParse::Token(Cmd, CVarName, false);

		bool bEnoughParamsSupplied = false;
		IConsoleVariable * CVar = nullptr;

		if (CVarName.Len() > 0)
		{
			CVar = IConsoleManager::Get().FindConsoleVariable(*CVarName);
		}

		if (CVar)
		{
			// values to toggle between
			FString StringVal1, StringVal2;
			
			if (FParse::Token(Cmd, StringVal1, false))
			{
				if (FParse::Token(Cmd, StringVal2, false))
				{
					bEnoughParamsSupplied = true;
					FString CurrentValue = CVar->GetString();

					FString Command(FString::Printf(TEXT("%s %s"), *CVarName, (CurrentValue == StringVal1) ? *StringVal2 : *StringVal1));
					GEngine->Exec(InWorld, *Command);
				}
			}
		}
		else
		{
			Ar.Log(*FString::Printf(TEXT("TOGGLECVAR: cvar '%s' was not found"), *CVarName));
			bEnoughParamsSupplied = true;	// cannot say anything about the rest of parameters
		}
		
		if (!bEnoughParamsSupplied)
		{
			Ar.Log(TEXT("Usage: TOGGLECVAR CVarName Value1 Value2"));
		}

		return true;
	}
#if !UE_BUILD_SHIPPING
	else if( FParse::Command( &Cmd, TEXT("ApplyUserSettings") ) )
	{
		return HandleApplyUserSettingsCommand( Cmd, Ar );
	}
#endif // !UE_BUILD_SHIPPING
#if WITH_EDITOR
	else if( FParse::Command(&Cmd,TEXT("STARTMOVIECAPTURE")) && GIsEditor )
	{
		IMovieSceneCaptureInterface* CaptureInterface = IMovieSceneCaptureModule::Get().GetFirstActiveMovieSceneCapture();
		if (CaptureInterface)
		{
			CaptureInterface->StartCapturing();
			return true;
		}
		else if (SceneViewport.IsValid())
		{
			if (IMovieSceneCaptureModule::Get().CreateMovieSceneCapture(SceneViewport))
			{
				return true;
			}
		}
		return false;
	}
#endif
	else if( InWorld && InWorld->Exec( InWorld, Cmd, Ar ) )
	{
		return true;
	}
	else if( InWorld && InWorld->GetAuthGameMode() && InWorld->GetAuthGameMode()->ProcessConsoleExec(Cmd,Ar,NULL) )
	{
		return true;
	}
	else
	{
#if UE_BUILD_SHIPPING
		// disallow set of actor properties if network game
		if ((FParse::Command( &Cmd, TEXT("SET")) || FParse::Command( &Cmd, TEXT("SETNOPEC"))))
		{
			FWorldContext &Context = GetWorldContextFromWorldChecked(InWorld);
			if( Context.PendingNetGame != NULL || InWorld->GetNetMode() != NM_Standalone)
			{
				return true;
			}
			// the effects of this cannot be easily reversed, so prevent the user from playing network games without restarting to avoid potential exploits
			GDisallowNetworkTravel = true;
		}
#endif // UE_BUILD_SHIPPING
		if (UEngine::Exec(InWorld, Cmd, Ar))
		{
			return true;
		}
		else if (UPlatformInterfaceBase::StaticExec(Cmd, Ar))
		{
			return true;
		}
	
		return false;
	}
}

bool UGameEngine::HandleExitCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	Ar.Log( TEXT("Closing by request") );

	FGameDelegates::Get().GetExitCommandDelegate().Broadcast();

	FPlatformMisc::RequestExit( 0 );
	return true;
}

bool UGameEngine::HandleMinimizeCommand( const TCHAR *Cmd, FOutputDevice &Ar )
{
	Ar.Log( TEXT("Minimize by request") );
	FPlatformApplicationMisc::RequestMinimize();

	return true;
}

bool UGameEngine::HandleGetMaxTickRateCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	Ar.Logf( TEXT("%f"), GetMaxTickRate(0,false) );
	return true;
}

bool UGameEngine::HandleCancelCommand( const TCHAR* Cmd, FOutputDevice& Ar, UWorld* InWorld )
{
	CancelPending(GetWorldContextFromWorldChecked(InWorld));
	return true;
}

#if !UE_BUILD_SHIPPING
bool UGameEngine::HandleApplyUserSettingsCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	GetGameUserSettings()->ApplySettings(false);
	return true;
}
#endif // !UE_BUILD_SHIPPING

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

float UGameEngine::GetMaxTickRate(float DeltaTime, bool bAllowFrameRateSmoothing) const
{
	float MaxTickRate = 0.f;

	if (FPlatformProperties::SupportsWindowedMode() == false && !IsRunningDedicatedServer())
	{
		static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.VSync"));
		// Limit framerate on console if VSYNC is enabled to avoid jumps from 30 to 60 and back.
		if( CVar->GetValueOnGameThread() != 0 )
		{
			if (SmoothedFrameRateRange.HasUpperBound())
			{
				MaxTickRate = SmoothedFrameRateRange.GetUpperBoundValue();
			}
		}
	}
	else 
	{
		UWorld* World = NULL;

		for (int32 WorldIndex = 0; WorldIndex < WorldList.Num(); ++WorldIndex)
		{
			if (WorldList[WorldIndex].WorldType == EWorldType::Game)
			{
				World = WorldList[WorldIndex].World();
				break;
			}
		}

		if( World )
		{
			UNetDriver* NetDriver = World->GetNetDriver();
			// In network games, limit framerate to not saturate bandwidth.
			if( NetDriver && (NetDriver->GetNetMode() == NM_DedicatedServer || (NetDriver->GetNetMode() == NM_ListenServer && NetDriver->bClampListenServerTickRate)))
			{
				// We're a dedicated server, use the LAN or Net tick rate.
				MaxTickRate = FMath::Clamp( NetDriver->NetServerMaxTickRate, 1, 1000 );
			}
			/*else if( NetDriver && NetDriver->ServerConnection )
			{
				if( NetDriver->ServerConnection->CurrentNetSpeed <= 10000 )
				{
					MaxTickRate = FMath::Clamp( MaxTickRate, 10.f, 90.f );
				}
			}*/
		}
	}

	// See if the code in the base class wants to replace this
	float SuperTickRate = Super::GetMaxTickRate(DeltaTime, bAllowFrameRateSmoothing);
	if(SuperTickRate != 0.0)
	{
		MaxTickRate = SuperTickRate;
	}

	return MaxTickRate;
}


void UGameEngine::Tick( float DeltaSeconds, bool bIdleMode )
{
	SCOPE_TIME_GUARD(TEXT("UGameEngine::Tick"));

	SCOPE_CYCLE_COUNTER(STAT_GameEngineTick);
	NETWORK_PROFILER(GNetworkProfiler.TrackFrameBegin());

	int32 LocalTickCycles=0;
	CLOCK_CYCLES(LocalTickCycles);
	
	// -----------------------------------------------------
	// Non-World related stuff
	// -----------------------------------------------------

	if( DeltaSeconds < 0.0f )
	{
#if (UE_BUILD_SHIPPING && WITH_EDITOR)
		// End users don't have access to the secure parts of UDN.  Regardless, they won't
		// need the warning because the game ships with AMD drivers that address the issue.
		UE_LOG(LogEngine, Fatal,TEXT("Negative delta time!"));
#else
		// Send developers to the support list thread.
		UE_LOG(LogEngine, Fatal,TEXT("Negative delta time! Please see https://udn.epicgames.com/lists/showpost.php?list=ue3bugs&id=4364"));
#endif
	}

	if ((GSlowFrameLoggingThreshold > 0.0f) && (DeltaSeconds > GSlowFrameLoggingThreshold))
	{
		UE_LOG(LogEngine, Log, TEXT("Slow GT frame detected (GT frame %u, delta time %f s)"), GFrameCounter - 1, DeltaSeconds);
	}

	// Tick the module manager
	IHotReloadInterface* HotReload = IHotReloadInterface::GetPtr();
	if(HotReload != nullptr)
	{
		HotReload->Tick();
	}

	if (IsRunningDedicatedServer())
	{
		double CurrentTime = FPlatformTime::Seconds();
		if (CurrentTime - LastTimeLogsFlushed > static_cast<double>(ServerFlushLogInterval))
		{
			GLog->Flush();

			LastTimeLogsFlushed = FPlatformTime::Seconds();
		}
	}
	else if (!IsRunningCommandlet() && FApp::CanEverRender())	// skip in case of commandlets, dedicated servers and headless games
	{
		// Clean up the game viewports that have been closed.
		CleanupGameViewport();
	}

	// If all viewports closed, time to exit - unless we're running headless
	if (GIsClient && (GameViewport == nullptr) && FApp::CanEverRender())
	{
		UE_LOG(LogEngine, Log,  TEXT("All Windows Closed") );
		FPlatformMisc::RequestExit( 0 );
		return;
	}

	if ( GameViewport != NULL )
	{
		// Decide whether to drop high detail because of frame rate.
		QUICK_SCOPE_CYCLE_COUNTER(STAT_UGameEngine_Tick_SetDropDetail);
		GameViewport->SetDropDetail(DeltaSeconds);
	}

	// Update subsystems.
	{
		// This assumes that UObject::StaticTick only calls ProcessAsyncLoading.
		SCOPE_TIME_GUARD(TEXT("UGameEngine::Tick - StaticTick"));
		StaticTick(DeltaSeconds, !!GAsyncLoadingUseFullTimeLimit, GAsyncLoadingTimeLimit / 1000.f);
	}

	{
		SCOPE_TIME_GUARD(TEXT("UGameEngine::Tick - Analytics"));
		FEngineAnalytics::Tick(DeltaSeconds);
	}

	// -----------------------------------------------------
	// Begin ticking worlds
	// -----------------------------------------------------

	bool bIsAnyNonPreviewWorldUnpaused = false;

	FName OriginalGWorldContext = NAME_None;
	for (int32 i=0; i < WorldList.Num(); ++i)
	{
		if (WorldList[i].World() == GWorld)
		{
			OriginalGWorldContext = WorldList[i].ContextHandle;
			break;
		}
	}

	for (int32 WorldIdx = 0; WorldIdx < WorldList.Num(); ++WorldIdx)
	{
		FWorldContext &Context = WorldList[WorldIdx];
		if (Context.World() == NULL || !Context.World()->ShouldTick())
		{
			continue;
		}

		GWorld = Context.World();

		// Tick all travel and Pending NetGames (Seamless, server, client)
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_UGameEngine_Tick_TickWorldTravel);
			TickWorldTravel(Context, DeltaSeconds);
		}

		if (!bIdleMode)
		{
			SCOPE_TIME_GUARD(TEXT("UGameEngine::Tick - WorldTick"));

			// Tick the world.
			GameCycles=0;
			CLOCK_CYCLES(GameCycles);
			Context.World()->Tick( LEVELTICK_All, DeltaSeconds );
			UNCLOCK_CYCLES(GameCycles);
		}

		if (!IsRunningDedicatedServer() && !IsRunningCommandlet())
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_UGameEngine_Tick_CheckCaptures);
			// Only update reflection captures in game once all 'always loaded' levels have been loaded
			// This won't work with actual level streaming though
			if (Context.World()->AreAlwaysLoadedLevelsLoaded())
			{
				// Update sky light first because it's considered direct lighting, sky diffuse will be visible in reflection capture indirect specular
				USkyLightComponent::UpdateSkyCaptureContents(Context.World());
				UReflectionCaptureComponent::UpdateReflectionCaptureContents(Context.World());
			}
		}



		// Issue cause event after first tick to provide a chance for the game to spawn the player and such.
		if( Context.World()->bWorldWasLoadedThisTick )
		{
			Context.World()->bWorldWasLoadedThisTick = false;
			
			const TCHAR* InitialExec = Context.LastURL.GetOption(TEXT("causeevent="),NULL);
			ULocalPlayer* GamePlayer = Context.OwningGameInstance ? Context.OwningGameInstance->GetFirstGamePlayer() : NULL;
			if( InitialExec && GamePlayer )
			{
				UE_LOG(LogEngine, Log, TEXT("Issuing initial cause event passed from URL: %s"), InitialExec);
				GamePlayer->Exec( GamePlayer->GetWorld(), *(FString("CAUSEEVENT ") + InitialExec), *GLog );
			}

			Context.World()->bTriggerPostLoadMap = true;
		}
	
		UpdateTransitionType(Context.World());

		// Block on async loading if requested.
		if (Context.World()->bRequestedBlockOnAsyncLoading)
		{
			BlockTillLevelStreamingCompleted(Context.World());
			Context.World()->bRequestedBlockOnAsyncLoading = false;
		}

		// streamingServer
		if( GIsServer == true )
		{
			SCOPE_CYCLE_COUNTER(STAT_UpdateLevelStreaming);
			Context.World()->UpdateLevelStreaming();
		}

		UNCLOCK_CYCLES(LocalTickCycles);
		TickCycles=LocalTickCycles;

		// See whether any map changes are pending and we requested them to be committed.
		QUICK_SCOPE_CYCLE_COUNTER(STAT_UGameEngine_Tick_ConditionalCommitMapChange);
		ConditionalCommitMapChange(Context);

		if (Context.WorldType != EWorldType::EditorPreview && !Context.World()->IsPaused())
		{
			bIsAnyNonPreviewWorldUnpaused = true;
		}
	}

	// ----------------------------
	//	End per-world ticking
	// ----------------------------
	{
		SCOPE_TIME_GUARD(TEXT("UGameEngine::Tick - TickObjects"));
		FTickableGameObject::TickObjects(nullptr, LEVELTICK_All, false, DeltaSeconds);
	}

	// Restore original GWorld*. This will go away one day.
	if (OriginalGWorldContext != NAME_None)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_UGameEngine_Tick_GetWorldContextFromHandleChecked);
		GWorld = GetWorldContextFromHandleChecked(OriginalGWorldContext).World();
	}

	// Tick the viewport
	if ( GameViewport != NULL && !bIdleMode )
	{
		SCOPE_TIME_GUARD(TEXT("UGameEngine::Tick - TickViewport"));
		SCOPE_CYCLE_COUNTER(STAT_GameViewportTick);
		GameViewport->Tick(DeltaSeconds);
	}

	if (FPlatformProperties::SupportsWindowedMode())
	{
		// Hide the splashscreen and show the game window
		static bool bFirstTime = true;
		if ( bFirstTime )
		{
			bFirstTime = false;
			FPlatformSplash::Hide();
			if ( GameViewportWindow.IsValid() )
			{
				GameViewportWindow.Pin()->ShowWindow();
				FSlateApplication::Get().RegisterGameViewport( GameViewportWidget.ToSharedRef() );
			}
		}
	}

	if (!bIdleMode && !IsRunningDedicatedServer() && !IsRunningCommandlet())
	{
		// Render everything.
		RedrawViewports();

		// Some tasks can only be done once we finish all scenes/viewports
		GetRendererModule().PostRenderAllViewports();
	}

	if( GIsClient )
	{
		// Update resource streaming after viewports have had a chance to update view information. Normal update.
		QUICK_SCOPE_CYCLE_COUNTER(STAT_UGameEngine_Tick_IStreamingManager);
		IStreamingManager::Get().Tick( DeltaSeconds );
	}

	// Update Audio. This needs to occur after rendering as the rendering code updates the listener position.
	FAudioDeviceManager* GameAudioDeviceManager = GEngine->GetAudioDeviceManager();
	if (GameAudioDeviceManager)
	{
		SCOPE_TIME_GUARD(TEXT("UGameEngine::Tick - Update Audio"));
		GameAudioDeviceManager->UpdateActiveAudioDevices(bIsAnyNonPreviewWorldUnpaused);
	}

	// rendering thread commands
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
			TickRenderingTimer,
			bool, bPauseRenderingRealtimeClock, GPauseRenderingRealtimeClock,
			float, DeltaTime, DeltaSeconds,
		{
			if(!bPauseRenderingRealtimeClock)
			{
				// Tick the GRenderingRealtimeClock, unless it's paused
				GRenderingRealtimeClock.Tick(DeltaTime);
			}

			GetRendererModule().TickRenderTargetPool();
		});
	}

#if WITH_EDITOR
	BroadcastPostEditorTick(DeltaSeconds);

	// Tick the asset registry
	FAssetRegistryModule::TickAssetRegistry(DeltaSeconds);
#endif
}


void UGameEngine::ProcessToggleFreezeCommand( UWorld* InWorld )
{
	if (GameViewport)
	{
		GameViewport->Viewport->ProcessToggleFreezeCommand();
	}
}


void UGameEngine::ProcessToggleFreezeStreamingCommand(UWorld* InWorld)
{
	// if not already frozen, then flush async loading before we freeze so that we don't mess up any in-process streaming
	if (!InWorld->bIsLevelStreamingFrozen)
	{
		FlushAsyncLoading();
	}

	// toggle the frozen state
	InWorld->bIsLevelStreamingFrozen = !InWorld->bIsLevelStreamingFrozen;
}


UWorld* UGameEngine::GetGameWorld()
{
	for (auto It = WorldList.CreateConstIterator(); It; ++It)
	{
		const FWorldContext& Context = *It;
		// Explicitly not checking for PIE worlds here, this should only 
		// be called outside of editor (and thus is in UGameEngine
		if (Context.WorldType == EWorldType::Game && Context.World())
		{
			return Context.World();
		}
	}

	return NULL;
}

void UGameEngine::HandleNetworkFailure_NotifyGameInstance(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType)
{
	if (GameInstance != nullptr)
	{
		bool bIsServer = true;
		if (NetDriver != nullptr)
		{
			bIsServer = NetDriver->GetNetMode() != NM_Client;
		}
		GameInstance->HandleNetworkError(FailureType, bIsServer);
	}
}

void UGameEngine::HandleTravelFailure_NotifyGameInstance(UWorld* World, ETravelFailure::Type FailureType)
{
	if (GameInstance != nullptr)
	{
		GameInstance->HandleTravelError(FailureType);
	}
}

void UGameEngine::HandleBrowseToDefaultMapFailure(FWorldContext& Context, const FString& TextURL, const FString& Error)
{
	Super::HandleBrowseToDefaultMapFailure(Context, TextURL, Error);
	FPlatformMisc::RequestExit(false);
}
