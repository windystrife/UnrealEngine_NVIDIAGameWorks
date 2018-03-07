// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Engine/GameViewportClient.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CoreDelegates.h"
#include "Misc/App.h"
#include "GameMapsSettings.h"
#include "EngineStats.h"
#include "RenderingThread.h"
#include "SceneView.h"
#include "AI/Navigation/NavigationSystem.h"
#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "GameFramework/Volume.h"
#include "Components/SkeletalMeshComponent.h"
#include "UObject/UObjectIterator.h"
#include "UObject/Package.h"
#include "SceneManagement.h"
#include "Particles/ParticleSystemComponent.h"
#include "Engine/NetDriver.h"
#include "Engine/LocalPlayer.h"
#include "ContentStreaming.h"
#include "UnrealEngine.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SViewport.h"
#include "Engine/Console.h"
#include "GameFramework/HUD.h"
#include "FXSystem.h"
#include "SubtitleManager.h"
#include "ImageUtils.h"
#include "SceneViewExtension.h"
#include "IHeadMountedDisplay.h"
#include "IXRTrackingSystem.h"
#include "EngineModule.h"
#include "AudioDeviceManager.h"
#include "AudioDevice.h"
#include "Sound/SoundWave.h"
#include "HighResScreenshot.h"
#include "BufferVisualizationData.h"
#include "GameFramework/InputSettings.h"
#include "Components/LineBatchComponent.h"
#include "Debug/DebugDrawService.h"
#include "Components/BrushComponent.h"
#include "Engine/GameEngine.h"
#include "Logging/MessageLog.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/GameUserSettings.h"
#include "Engine/UserInterfaceSettings.h"
#include "Slate/SceneViewport.h"
#include "Slate/SGameLayerManager.h"
#include "ActorEditorUtils.h"
#include "ComponentRecreateRenderStateContext.h"
#include "Framework/Application/HardwareCursor.h"

#define LOCTEXT_NAMESPACE "GameViewport"

/** This variable allows forcing full screen of the first player controller viewport, even if there are multiple controllers plugged in and no cinematic playing. */
bool GForceFullscreen = false;

/** Whether to visualize the lightmap selected by the Debug Camera. */
extern ENGINE_API bool GShowDebugSelectedLightmap;
/** The currently selected component in the actor. */
extern ENGINE_API UPrimitiveComponent* GDebugSelectedComponent;
/** The lightmap used by the currently selected component, if it's a static mesh component. */
extern ENGINE_API class FLightMap2D* GDebugSelectedLightmap;

/** Delegate called at the end of the frame when a screenshot is captured */
FOnScreenshotCaptured UGameViewportClient::ScreenshotCapturedDelegate;

/** Delegate called when the game viewport is created. */
FSimpleMulticastDelegate UGameViewportClient::CreatedDelegate;

/** A list of all the stat names which are enabled for this viewport (static so they persist between runs) */
TArray<FString> UGameViewportClient::EnabledStats;

/** Those sound stat flags which are enabled on this viewport */
FViewportClient::ESoundShowFlags::Type UGameViewportClient::SoundShowFlags = FViewportClient::ESoundShowFlags::Disabled;

/**
 * UI Stats
 */
DECLARE_CYCLE_STAT(TEXT("UI Drawing Time"),STAT_UIDrawingTime,STATGROUP_UI);

static TAutoConsoleVariable<int32> CVarSetBlackBordersEnabled(
	TEXT("r.BlackBorders"),
	0,
	TEXT("To draw black borders around the rendered image\n")
	TEXT("(prevents artifacts from post processing passes that read outside of the image e.g. PostProcessAA)\n")
	TEXT("in pixels, 0:off"),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarScreenshotDelegate(
	TEXT("r.ScreenshotDelegate"),
	1,
	TEXT("ScreenshotDelegates prevent processing of incoming screenshot request and break some features. This allows to disable them.\n")
	TEXT("Ideally we rework the delegate code to not make that needed.\n")
	TEXT(" 0: off\n")
	TEXT(" 1: delegates are on (default)"),
	ECVF_Default);


/**
 * Draw debug info on a game scene view.
 */
class FGameViewDrawer : public FViewElementDrawer
{
public:
	/**
	 * Draws debug info using the given draw interface.
	 */
	virtual void Draw(const FSceneView* View,FPrimitiveDrawInterface* PDI)
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		// Draw a wireframe sphere around the selected lightmap, if requested.
		if ( GShowDebugSelectedLightmap && GDebugSelectedComponent && GDebugSelectedLightmap )
		{
			float Radius = GDebugSelectedComponent->Bounds.SphereRadius;
			int32 Sides = FMath::Clamp<int32>( FMath::TruncToInt(Radius*Radius*4.0f*PI/(80.0f*80.0f)), 8, 200 );
			DrawWireSphere( PDI, GDebugSelectedComponent->Bounds.Origin, FColor(255,130,0), GDebugSelectedComponent->Bounds.SphereRadius, Sides, SDPG_Foreground );
		}
#endif
	}
};

UGameViewportClient::UGameViewportClient(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, EngineShowFlags(ESFIM_Game)
	, CurrentBufferVisualizationMode(NAME_None)
	, HighResScreenshotDialog(NULL)
	, bUseSoftwareCursorWidgets(true)
	, bIgnoreInput(false)
	, MouseCaptureMode(EMouseCaptureMode::CapturePermanently)
	, bHideCursorDuringCapture(false)
	, MouseLockMode(EMouseLockMode::LockOnCapture)
	, AudioDeviceHandle(INDEX_NONE)
	, bHasAudioFocus(false)
	, bIsMouseOverClient(false)
{

	TitleSafeZone.MaxPercentX = 0.9f;
	TitleSafeZone.MaxPercentY = 0.9f;
	TitleSafeZone.RecommendedPercentX = 0.8f;
	TitleSafeZone.RecommendedPercentY = 0.8f;

	bIsPlayInEditorViewport = false;
	ViewModeIndex = VMI_Lit;

	SplitscreenInfo.Init(FSplitscreenData(), ESplitScreenType::SplitTypeCount);

	SplitscreenInfo[ESplitScreenType::None].PlayerData.Add(FPerPlayerSplitscreenData(1.0f, 1.0f, 0.0f, 0.0f));

	SplitscreenInfo[ESplitScreenType::TwoPlayer_Horizontal].PlayerData.Add(FPerPlayerSplitscreenData(1.0f, 0.5f, 0.0f, 0.0f));
	SplitscreenInfo[ESplitScreenType::TwoPlayer_Horizontal].PlayerData.Add(FPerPlayerSplitscreenData(1.0f, 0.5f, 0.0f, 0.5f));

	SplitscreenInfo[ESplitScreenType::TwoPlayer_Vertical].PlayerData.Add(FPerPlayerSplitscreenData(0.5f, 1.0f, 0.0f, 0.0f));
	SplitscreenInfo[ESplitScreenType::TwoPlayer_Vertical].PlayerData.Add(FPerPlayerSplitscreenData(0.5f, 1.0f, 0.5f, 0.0f));

	SplitscreenInfo[ESplitScreenType::ThreePlayer_FavorTop].PlayerData.Add(FPerPlayerSplitscreenData(1.0f, 0.5f, 0.0f, 0.0f));
	SplitscreenInfo[ESplitScreenType::ThreePlayer_FavorTop].PlayerData.Add(FPerPlayerSplitscreenData(0.5f, 0.5f, 0.0f, 0.5f));
	SplitscreenInfo[ESplitScreenType::ThreePlayer_FavorTop].PlayerData.Add(FPerPlayerSplitscreenData(0.5f, 0.5f, 0.5f, 0.5f));

	SplitscreenInfo[ESplitScreenType::ThreePlayer_FavorBottom].PlayerData.Add(FPerPlayerSplitscreenData(0.5f, 0.5f, 0.0f, 0.0f));
	SplitscreenInfo[ESplitScreenType::ThreePlayer_FavorBottom].PlayerData.Add(FPerPlayerSplitscreenData(0.5f, 0.5f, 0.5f, 0.0f));
	SplitscreenInfo[ESplitScreenType::ThreePlayer_FavorBottom].PlayerData.Add(FPerPlayerSplitscreenData(1.0f, 0.5f, 0.0f, 0.5f));

	SplitscreenInfo[ESplitScreenType::FourPlayer].PlayerData.Add(FPerPlayerSplitscreenData(0.5f, 0.5f, 0.0f, 0.0f));
	SplitscreenInfo[ESplitScreenType::FourPlayer].PlayerData.Add(FPerPlayerSplitscreenData(0.5f, 0.5f, 0.5f, 0.0f));
	SplitscreenInfo[ESplitScreenType::FourPlayer].PlayerData.Add(FPerPlayerSplitscreenData(0.5f, 0.5f, 0.0f, 0.5f));
	SplitscreenInfo[ESplitScreenType::FourPlayer].PlayerData.Add(FPerPlayerSplitscreenData(0.5f, 0.5f, 0.5f, 0.5f));

	MaxSplitscreenPlayers = 4;
	bSuppressTransitionMessage = true;

	if (HasAnyFlags(RF_ClassDefaultObject) == false)
	{
		StatUnitData = new FStatUnitData();
		StatHitchesData = new FStatHitchesData();
		FCoreDelegates::StatCheckEnabled.AddUObject(this, &UGameViewportClient::HandleViewportStatCheckEnabled);
		FCoreDelegates::StatEnabled.AddUObject(this, &UGameViewportClient::HandleViewportStatEnabled);
		FCoreDelegates::StatDisabled.AddUObject(this, &UGameViewportClient::HandleViewportStatDisabled);
		FCoreDelegates::StatDisableAll.AddUObject(this, &UGameViewportClient::HandleViewportStatDisableAll);

#if WITH_EDITOR
		if (GIsEditor)
		{
			FSlateApplication::Get().OnWindowDPIScaleChanged().AddUObject(this, &UGameViewportClient::HandleWindowDPIScaleChanged);
		}
#endif

	}
}

UGameViewportClient::UGameViewportClient(FVTableHelper& Helper)
	: Super(Helper)
	, EngineShowFlags(ESFIM_Game)
	, CurrentBufferVisualizationMode(NAME_None)
	, HighResScreenshotDialog(NULL)
	, bIgnoreInput(false)
	, MouseCaptureMode(EMouseCaptureMode::CapturePermanently)
	, bHideCursorDuringCapture(false)
	, MouseLockMode(EMouseLockMode::LockOnCapture)
	, AudioDeviceHandle(INDEX_NONE)
	, bHasAudioFocus(false)
{

}

UGameViewportClient::~UGameViewportClient()
{
	if (EngineShowFlags.Collision)
	{
		EngineShowFlags.SetCollision(false);
		ToggleShowCollision();
	}

	FCoreDelegates::StatCheckEnabled.RemoveAll(this);
	FCoreDelegates::StatEnabled.RemoveAll(this);
	FCoreDelegates::StatDisabled.RemoveAll(this);
	FCoreDelegates::StatDisableAll.RemoveAll(this);

#if WITH_EDITOR
	if (GIsEditor && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().OnWindowDPIScaleChanged().RemoveAll(this);
	}
#endif

	if (StatHitchesData)
	{
		delete StatHitchesData;
		StatHitchesData = NULL;
	}

	if (StatUnitData)
	{
		delete StatUnitData;
		StatUnitData = NULL;
	}
}

void UGameViewportClient::PostInitProperties()
{
	Super::PostInitProperties();
	EngineShowFlags = FEngineShowFlags(ESFIM_Game);
}

void UGameViewportClient::BeginDestroy()
{
	if (GEngine)
	{
		class FAudioDeviceManager* AudioDeviceManager = GEngine->GetAudioDeviceManager();
		if (AudioDeviceManager)
		{
			AudioDeviceManager->ShutdownAudioDevice(AudioDeviceHandle);
		}
	}

	RemoveAllViewportWidgets();
	Super::BeginDestroy();
}

void UGameViewportClient::DetachViewportClient()
{
	ViewportConsole = NULL;
	ResetHardwareCursorStates();
	RemoveAllViewportWidgets();
	RemoveFromRoot();
}

FSceneViewport* UGameViewportClient::GetGameViewport()
{
	return static_cast<FSceneViewport*>(Viewport);
}

const FSceneViewport* UGameViewportClient::GetGameViewport() const
{
	return static_cast<FSceneViewport*>(Viewport);
}


TSharedPtr<class SViewport> UGameViewportClient::GetGameViewportWidget()
{
	FSceneViewport* SceneViewport = GetGameViewport();
	if (SceneViewport != nullptr)
	{
		TWeakPtr<SViewport> WeakViewportWidget = SceneViewport->GetViewportWidget();
		TSharedPtr<SViewport> ViewportWidget = WeakViewportWidget.Pin();
		return ViewportWidget;
	}
	return nullptr;
}

void UGameViewportClient::Tick( float DeltaTime )
{
	TickDelegate.Broadcast(DeltaTime);
}

FString UGameViewportClient::ConsoleCommand( const FString& Command)
{
	FString TruncatedCommand = Command.Left(1000);
	FConsoleOutputDevice ConsoleOut(ViewportConsole);
	Exec( GetWorld(), *TruncatedCommand,ConsoleOut);
	return *ConsoleOut;
}

void UGameViewportClient::SetEnabledStats(const TArray<FString>& InEnabledStats)
{
	if (FPlatformProcess::SupportsMultithreading())
	{
		EnabledStats = InEnabledStats;
	}
	else
	{
		UE_LOG(LogPlayerManagement, Warning, TEXT("WARNING: Stats disabled for non multi-threading platforms"));
	}

#if !UE_BUILD_SHIPPING
	if (UWorld* MyWorld = GetWorld())
	{
		if (FAudioDevice* AudioDevice = MyWorld->GetAudioDevice())
		{
			AudioDevice->ResolveDesiredStats(this);
		}
	}
#endif
}


void UGameViewportClient::Init(struct FWorldContext& WorldContext, UGameInstance* OwningGameInstance, bool bCreateNewAudioDevice)
{
	// set reference to world context
	WorldContext.AddRef(World);

	// remember our game instance
	GameInstance = OwningGameInstance;

	// Set the projects default viewport mouse capture mode
	MouseCaptureMode = GetDefault<UInputSettings>()->DefaultViewportMouseCaptureMode;
	MouseLockMode = GetDefault<UInputSettings>()->DefaultViewportMouseLockMode;

	// Create the cursor Widgets
	UUserInterfaceSettings* UISettings = GetMutableDefault<UUserInterfaceSettings>(UUserInterfaceSettings::StaticClass());

	if (GEngine)
	{
		FAudioDeviceManager* AudioDeviceManager = GEngine->GetAudioDeviceManager();
		if (AudioDeviceManager)
		{
			FAudioDeviceManager::FCreateAudioDeviceResults NewDeviceResults;
			if (AudioDeviceManager->CreateAudioDevice(bCreateNewAudioDevice, NewDeviceResults))
			{
				AudioDeviceHandle = NewDeviceResults.Handle;

#if !UE_BUILD_SHIPPING
				if (NewDeviceResults.bNewDevice)
				{
					NewDeviceResults.AudioDevice->ResolveDesiredStats(this);
				}
#endif // UE_BUILD_SHIPPING

				// Set the base mix of the new device based on the world settings of the world
				if (World)
				{
					NewDeviceResults.AudioDevice->SetDefaultBaseSoundMix(World->GetWorldSettings()->DefaultBaseSoundMix);

					// Set the world's audio device handle to use so that sounds which play in that world will use the correct audio device
					World->SetAudioDeviceHandle(AudioDeviceHandle);
				}

				// Set this audio device handle on the world context so future world's set onto the world context
				// will pass the audio device handle to them and audio will play on the correct audio device
				WorldContext.AudioDeviceHandle = AudioDeviceHandle;
			}
		}
	}

	// Set all the software cursors.
	for ( auto& Entry : UISettings->SoftwareCursors )
	{
		AddSoftwareCursor(Entry.Key, Entry.Value);
	}

	// Set all the hardware cursors.
	for ( auto& Entry : UISettings->HardwareCursors )
	{
		SetHardwareCursor(Entry.Key, Entry.Value.CursorPath, Entry.Value.HotSpot);
	}
}

void UGameViewportClient::RebuildCursors()
{
	UUserInterfaceSettings* UISettings = GetMutableDefault<UUserInterfaceSettings>(UUserInterfaceSettings::StaticClass());
	// Set all the software cursors.
	for (auto& Entry : UISettings->SoftwareCursors)
	{
		AddSoftwareCursor(Entry.Key, Entry.Value);
	}

	// Set all the hardware cursors.
	for (auto& Entry : UISettings->HardwareCursors)
	{
		SetHardwareCursor(Entry.Key, Entry.Value.CursorPath, Entry.Value.HotSpot);
	}
}

UWorld* UGameViewportClient::GetWorld() const
{
	return World;
}

UGameInstance* UGameViewportClient::GetGameInstance() const
{
	return GameInstance;
}

bool UGameViewportClient::InputKey(FViewport* InViewport, int32 ControllerId, FKey Key, EInputEvent EventType, float AmountDepressed, bool bGamepad)
{
	if (IgnoreInput())
	{
		return ViewportConsole ? ViewportConsole->InputKey(ControllerId, Key, EventType, AmountDepressed, bGamepad) : false;
	}

	if ((Key == EKeys::Enter && EventType == EInputEvent::IE_Pressed && FSlateApplication::Get().GetModifierKeys().IsAltDown() && GetDefault<UInputSettings>()->bAltEnterTogglesFullscreen)
		|| (IsRunningGame() && Key == EKeys::F11 && EventType == EInputEvent::IE_Pressed && GetDefault<UInputSettings>()->bF11TogglesFullscreen))
	{
		HandleToggleFullscreenCommand();
		return true;
	}

	const int32 NumLocalPlayers = World->GetGameInstance()->GetNumLocalPlayers();

	if (NumLocalPlayers > 1 && Key.IsGamepadKey() && GetDefault<UGameMapsSettings>()->bOffsetPlayerGamepadIds)
	{
		++ControllerId;
	}
	else if (InViewport->IsPlayInEditorViewport() && Key.IsGamepadKey())
	{
		GEngine->RemapGamepadControllerIdForPIE(this, ControllerId);
	}

#if WITH_EDITOR
	// Give debugger commands a chance to process key binding
	if (GameViewportInputKeyDelegate.IsBound())
	{
		if ( GameViewportInputKeyDelegate.Execute(Key, FSlateApplication::Get().GetModifierKeys(), EventType) )
		{
			return true;
		}
	}
#endif

	// route to subsystems that care
	bool bResult = ( ViewportConsole ? ViewportConsole->InputKey(ControllerId, Key, EventType, AmountDepressed, bGamepad) : false );

	if (!bResult)
	{
		ULocalPlayer* const TargetPlayer = GEngine->GetLocalPlayerFromControllerId(this, ControllerId);
		if (TargetPlayer && TargetPlayer->PlayerController)
		{
			bResult = TargetPlayer->PlayerController->InputKey(Key, EventType, AmountDepressed, bGamepad);
		}

		// A gameviewport is always considered to have responded to a mouse buttons to avoid throttling
		if (!bResult && Key.IsMouseButton())
		{
			bResult = true;
		}
	}

	// For PIE, let the next PIE window handle the input if none of our players did
	// (this allows people to use multiple controllers to control each window)
	if (!bResult && ControllerId > NumLocalPlayers - 1 && InViewport->IsPlayInEditorViewport())
	{
		UGameViewportClient *NextViewport = GEngine->GetNextPIEViewport(this);
		if (NextViewport)
		{
			bResult = NextViewport->InputKey(InViewport, ControllerId - NumLocalPlayers, Key, EventType, AmountDepressed, bGamepad);
		}
	}

	return bResult;
}


bool UGameViewportClient::InputAxis(FViewport* InViewport, int32 ControllerId, FKey Key, float Delta, float DeltaTime, int32 NumSamples, bool bGamepad)
{
	if (IgnoreInput())
	{
		return false;
	}

	const int32 NumLocalPlayers = World->GetGameInstance()->GetNumLocalPlayers();

	if (NumLocalPlayers > 1 && Key.IsGamepadKey() && GetDefault<UGameMapsSettings>()->bOffsetPlayerGamepadIds)
	{
		++ControllerId;
	}
	else if (InViewport->IsPlayInEditorViewport() && Key.IsGamepadKey())
	{
		GEngine->RemapGamepadControllerIdForPIE(this, ControllerId);
	}

	bool bResult = false;

	// Don't allow mouse/joystick input axes while in PIE and the console has forced the cursor to be visible.  It's
	// just distracting when moving the mouse causes mouse look while you are trying to move the cursor over a button
	// in the editor!
	if( !( InViewport->IsSlateViewport() && InViewport->IsPlayInEditorViewport() ) || ViewportConsole == NULL || !ViewportConsole->ConsoleActive() )
	{
		// route to subsystems that care
		if (ViewportConsole != NULL)
		{
			bResult = ViewportConsole->InputAxis(ControllerId, Key, Delta, DeltaTime, NumSamples, bGamepad);
		}
		if (!bResult)
		{
			ULocalPlayer* const TargetPlayer = GEngine->GetLocalPlayerFromControllerId(this, ControllerId);
			if (TargetPlayer && TargetPlayer->PlayerController)
			{
				bResult = TargetPlayer->PlayerController->InputAxis(Key, Delta, DeltaTime, NumSamples, bGamepad);
			}
		}

		// For PIE, let the next PIE window handle the input if none of our players did
		// (this allows people to use multiple controllers to control each window)
		if (!bResult && ControllerId > NumLocalPlayers - 1 && InViewport->IsPlayInEditorViewport())
		{
			UGameViewportClient *NextViewport = GEngine->GetNextPIEViewport(this);
			if (NextViewport)
			{
				bResult = NextViewport->InputAxis(InViewport, ControllerId - NumLocalPlayers, Key, Delta, DeltaTime, NumSamples, bGamepad);
			}
		}

		if( InViewport->IsSlateViewport() && InViewport->IsPlayInEditorViewport() )
		{
			// Absorb all keys so game input events are not routed to the Slate editor frame
			bResult = true;
		}
	}

	return bResult;
}


bool UGameViewportClient::InputChar(FViewport* InViewport, int32 ControllerId, TCHAR Character)
{
	// should probably just add a ctor to FString that takes a TCHAR
	FString CharacterString;
	CharacterString += Character;

	//Always route to the console
	bool bResult = (ViewportConsole ? ViewportConsole->InputChar(ControllerId, CharacterString) : false);

	if (IgnoreInput())
	{
		return bResult;
	}

	// route to subsystems that care
	if (!bResult && InViewport->IsSlateViewport() && InViewport->IsPlayInEditorViewport())
	{
		// Absorb all keys so game input events are not routed to the Slate editor frame
		bResult = true;
	}

	return bResult;
}

bool UGameViewportClient::InputTouch(FViewport* InViewport, int32 ControllerId, uint32 Handle, ETouchType::Type Type, const FVector2D& TouchLocation, FDateTime DeviceTimestamp, uint32 TouchpadIndex)
{
	if (IgnoreInput())
	{
		return false;
	}

	// route to subsystems that care
	bool bResult = (ViewportConsole ? ViewportConsole->InputTouch(ControllerId, Handle, Type, TouchLocation, DeviceTimestamp, TouchpadIndex) : false);
	if (!bResult)
	{
		ULocalPlayer* const TargetPlayer = GEngine->GetLocalPlayerFromControllerId(this, ControllerId);
		if (TargetPlayer && TargetPlayer->PlayerController)
		{
			bResult = TargetPlayer->PlayerController->InputTouch(Handle, Type, TouchLocation, DeviceTimestamp, TouchpadIndex);
		}
	}

	return bResult;
}

bool UGameViewportClient::InputMotion(FViewport* InViewport, int32 ControllerId, const FVector& Tilt, const FVector& RotationRate, const FVector& Gravity, const FVector& Acceleration)
{
	if (IgnoreInput())
	{
		return false;
	}

	// route to subsystems that care
	bool bResult = false;

	ULocalPlayer* const TargetPlayer = GEngine->GetLocalPlayerFromControllerId(this, ControllerId);
	if (TargetPlayer && TargetPlayer->PlayerController)
	{
		bResult = TargetPlayer->PlayerController->InputMotion(Tilt, RotationRate, Gravity, Acceleration);
	}

	return bResult;
}

void UGameViewportClient::SetIsSimulateInEditorViewport(bool bInIsSimulateInEditorViewport)
{
#if PLATFORM_DESKTOP || PLATFORM_HTML5
	if (GetDefault<UInputSettings>()->bUseMouseForTouch)
	{
		FSlateApplication::Get().SetGameIsFakingTouchEvents(!bInIsSimulateInEditorViewport);
	}
#endif

	for (ULocalPlayer* LocalPlayer : GetOuterUEngine()->GetGamePlayers(this))
	{
		if (LocalPlayer->PlayerController)
		{
			if (bInIsSimulateInEditorViewport)
			{
				LocalPlayer->PlayerController->CleanupGameViewport();
			}
			else
			{
				LocalPlayer->PlayerController->CreateTouchInterface();
			}
		}
	}
}

float UGameViewportClient::GetViewportClientWindowDPIScale() const
{
	TSharedPtr<SWindow> PinnedWindow = Window.Pin();

	float DPIScale = 1.0f;

	if(PinnedWindow.IsValid() && PinnedWindow->GetNativeWindow().IsValid())
	{
		DPIScale = PinnedWindow->GetNativeWindow()->GetDPIScaleFactor();
	}

	return DPIScale;
}

void UGameViewportClient::MouseEnter(FViewport* InViewport, int32 x, int32 y)
{
	Super::MouseEnter(InViewport, x, y);

#if PLATFORM_DESKTOP || PLATFORM_HTML5
	if (GetDefault<UInputSettings>()->bUseMouseForTouch && !GetGameViewport()->GetPlayInEditorIsSimulate())
	{
		FSlateApplication::Get().SetGameIsFakingTouchEvents(true);
	}
#endif

	// Replace all the cursors.
	TSharedPtr<ICursor> PlatformCursor = FSlateApplication::Get().GetPlatformCursor();
	if ( ICursor* Cursor = PlatformCursor.Get() )
	{
		for ( auto& Entry : HardwareCursors )
		{
			Cursor->SetTypeShape(Entry.Key, Entry.Value->GetHandle());
		}
	}

	bIsMouseOverClient = true;
}

void UGameViewportClient::MouseLeave(FViewport* InViewport)
{
	Super::MouseLeave(InViewport);

	if (InViewport && GetDefault<UInputSettings>()->bUseMouseForTouch)
	{
		// Only send the touch end event if we're not drag/dropping, as that will end the drag/drop operation.
		if ( !FSlateApplication::Get().IsDragDropping() )
		{
			FIntPoint LastViewportCursorPos;
			InViewport->GetMousePos(LastViewportCursorPos, false);

#if PLATFORM_DESKTOP || PLATFORM_HTML5
			FVector2D CursorPos(LastViewportCursorPos.X, LastViewportCursorPos.Y);
			FSlateApplication::Get().SetGameIsFakingTouchEvents(false, &CursorPos);
#endif
		}
	}

#if WITH_EDITOR

	// NOTE: Only do this in editor builds where the editor is running.
	// We don't care about bothering to clear them otherwise, and it may negatively impact
	// things like drag/drop, since those would 'leave' the viewport.
	if ( !FSlateApplication::Get().IsDragDropping() )
	{
		bIsMouseOverClient = false;
		ResetHardwareCursorStates();
	}

#endif
}

void UGameViewportClient::ResetHardwareCursorStates()
{
	// clear all the overridden hardware cursors
	TSharedPtr<ICursor> PlatformCursor = FSlateApplication::Get().GetPlatformCursor();
	if (ICursor* Cursor = PlatformCursor.Get())
	{
		for (auto& Entry : HardwareCursors)
		{
			Cursor->SetTypeShape(Entry.Key, nullptr);
		}
	}
}

bool UGameViewportClient::GetMousePosition(FVector2D& MousePosition) const
{
	bool bGotMousePosition = false;

	if (Viewport && FSlateApplication::Get().IsMouseAttached())
	{
		FIntPoint MousePos;
		Viewport->GetMousePos(MousePos);
		if (MousePos.X >= 0 && MousePos.Y >= 0)
		{
			MousePosition = FVector2D(MousePos);
			bGotMousePosition = true;
		}
	}

	return bGotMousePosition;
}

bool UGameViewportClient::RequiresUncapturedAxisInput() const
{
	bool bRequired = false;
	if (Viewport != NULL && Viewport->HasFocus())
	{
		if (ViewportConsole && ViewportConsole->ConsoleActive())
		{
			bRequired = true;
		}
		else if (GameInstance && GameInstance->GetFirstLocalPlayerController())
		{
			bRequired = GameInstance->GetFirstLocalPlayerController()->ShouldShowMouseCursor();
		}
	}

	return bRequired;
}


EMouseCursor::Type UGameViewportClient::GetCursor(FViewport* InViewport, int32 X, int32 Y)
{
	// If the viewport isn't active or the console is active we don't want to override the cursor
	if (!FSlateApplication::Get().IsActive() || (!InViewport->HasMouseCapture() && !InViewport->HasFocus()) || (ViewportConsole && ViewportConsole->ConsoleActive()))
	{
		return EMouseCursor::Default;
	}
	else if (GameInstance && GameInstance->GetFirstLocalPlayerController())
	{
		return GameInstance->GetFirstLocalPlayerController()->GetMouseCursor();
	}

	return FViewportClient::GetCursor(InViewport, X, Y);
}

void UGameViewportClient::SetVirtualCursorWidget(EMouseCursor::Type Cursor, UUserWidget* UserWidget)
{
	if (ensure(UserWidget))
	{
		if (CursorWidgets.Contains(Cursor))
		{
			TSharedRef<SWidget>* Widget = CursorWidgets.Find(Cursor);
			(*Widget) = UserWidget->TakeWidget();
		}
		else
		{
			CursorWidgets.Add(Cursor, UserWidget->TakeWidget());
		}
	}
}

void UGameViewportClient::AddSoftwareCursor(EMouseCursor::Type Cursor, const FSoftClassPath& CursorClass)
{
	if (ensureMsgf(CursorClass.IsValid(), TEXT("UGameViewportClient::AddCusor: Cursor class is not valid!")))
	{
		UClass* Class = CursorClass.TryLoadClass<UUserWidget>();
		if (Class)
		{
			UUserWidget* UserWidget = CreateWidget<UUserWidget>(GetGameInstance(), Class);
			AddCursorWidget(Cursor, UserWidget);
		}
		else
		{
			UE_LOG(LogPlayerManagement, Warning, TEXT("UGameViewportClient::AddCursor: Could not load cursor class %s."), *CursorClass.GetAssetName());
		}
	}
}

void UGameViewportClient::AddCursorWidget(EMouseCursor::Type Cursor, class UUserWidget* CursorWidget)
{
	if (ensure(CursorWidget))
	{
		CursorWidgets.Add(Cursor, CursorWidget->TakeWidget());
	}
}

TOptional<TSharedRef<SWidget>> UGameViewportClient::MapCursor(FViewport* InViewport, const FCursorReply& CursorReply)
{
	if (bUseSoftwareCursorWidgets)
	{
		if (CursorReply.GetCursorType() != EMouseCursor::None)
		{
			const TSharedRef<SWidget>* CursorWidgetPtr = CursorWidgets.Find(CursorReply.GetCursorType());

			if (CursorWidgetPtr != nullptr)
			{
				return *CursorWidgetPtr;
			}
			else
			{
				UE_LOG(LogPlayerManagement, Warning, TEXT("UGameViewportClient::MapCursor: Could not find cursor to map to %d."),int32(CursorReply.GetCursorType()));
			}
		}
	}
	return TOptional<TSharedRef<SWidget>>();
}

void UGameViewportClient::SetDropDetail(float DeltaSeconds)
{
	if (GEngine && GetWorld())
	{
		float FrameTime = 0.0f;
		if (FPlatformProperties::SupportsWindowedMode() == false)
		{
			FrameTime	= FPlatformTime::ToSeconds(FMath::Max3<uint32>( GRenderThreadTime, GGameThreadTime, GGPUFrameTime ));
			// If DeltaSeconds is bigger than 34 ms we can take it into account as we're not VSYNCing in that case.
			if( DeltaSeconds > 0.034 )
			{
				FrameTime = FMath::Max( FrameTime, DeltaSeconds );
			}
		}
		else
		{
			FrameTime = DeltaSeconds;
		}
		const float FrameRate	= FrameTime > 0 ? 1 / FrameTime : 0;

		// When using FixedFrameRate, FrameRate here becomes FixedFrameRate (even if actual framerate is smaller).
		const bool bTimeIsManipulated = FApp::IsBenchmarking() || FApp::UseFixedTimeStep() || GEngine->bUseFixedFrameRate;
		// Drop detail if framerate is below threshold.
		GetWorld()->bDropDetail		= FrameRate < FMath::Clamp(GEngine->MinDesiredFrameRate, 1.f, 100.f) && !bTimeIsManipulated;
		GetWorld()->bAggressiveLOD	= FrameRate < FMath::Clamp(GEngine->MinDesiredFrameRate - 5.f, 1.f, 100.f) && !bTimeIsManipulated;

		// this is slick way to be able to do something based on the frametime and whether we are bound by one thing or another
#if 0 
		// so where we check to see if we are above some threshold and below 150 ms (any thing above that is usually blocking loading of some sort)
		// also we don't want to do the auto trace when we are blocking on async loading
		if ((0.070 < FrameTime) && (FrameTime < 0.150) && IsAsyncLoading() == false && GetWorld()->bRequestedBlockOnAsyncLoading == false && (GetWorld()->GetTimeSeconds() > 30.0f))
		{
			// now check to see if we have done a trace in the last 30 seconds otherwise we will just trace ourselves to death
			static float LastTraceTime = -9999.0f;
			if( (LastTraceTime+30.0f < GetWorld()->GetTimeSeconds()))
			{
				LastTraceTime = GetWorld()->GetTimeSeconds();
				UE_LOG(LogPlayerManagement, Warning, TEXT("Auto Trace initiated!! FrameTime: %f"), FrameTime );

				// do what ever action you want here (e.g. trace <type>, GShouldLogOutAFrameOfMoveActor = true, c.f. LevelTick.cpp for more)
				//GShouldLogOutAFrameOfMoveActor = true;

#if !WITH_EDITORONLY_DATA
				UE_LOG(LogPlayerManagement, Warning, TEXT("    GGameThreadTime: %d GRenderThreadTime: %d "), GGameThreadTime, GRenderThreadTime );
#endif // WITH_EDITORONLY_DATA
			}
		}
#endif // 0 
	}
}


void UGameViewportClient::SetViewportFrame( FViewportFrame* InViewportFrame )
{
	ViewportFrame = InViewportFrame;
	SetViewport( ViewportFrame ? ViewportFrame->GetViewport() : NULL );
}


void UGameViewportClient::SetViewport( FViewport* InViewport )
{
	FViewport* PreviousViewport = Viewport;
	Viewport = InViewport;

	if ( PreviousViewport == NULL && Viewport != NULL )
	{
		// ensure that the player's Origin and Size members are initialized the moment we get a viewport
		LayoutPlayers();
	}
}

void UGameViewportClient::GetViewportSize( FVector2D& out_ViewportSize ) const
{
	if ( Viewport != NULL )
	{
		out_ViewportSize.X = Viewport->GetSizeXY().X;
		out_ViewportSize.Y = Viewport->GetSizeXY().Y;
	}
}

bool UGameViewportClient::IsFullScreenViewport() const
{
	return Viewport->IsFullscreen();
}

bool UGameViewportClient::ShouldForceFullscreenViewport() const
{
	bool bResult = false;
	if ( GForceFullscreen )
	{
		bResult = true;
	}
	else if ( GetOuterUEngine()->GetNumGamePlayers(this) == 0 )
	{
		bResult = true;
	}
	else if ( UWorld* MyWorld = GetWorld() )
	{
		if ( MyWorld->bIsDefaultLevel )
		{
			bResult = true;
		}
		else if ( GameInstance )
		{
			APlayerController* PlayerController = GameInstance->GetFirstLocalPlayerController();
			if( ( PlayerController ) && ( PlayerController->bCinematicMode ) )
			{
				bResult = true;
			}
		}
	}
	return bResult;
}

/** Util to find named canvas in transient package, and create if not found */
static UCanvas* GetCanvasByName(FName CanvasName)
{
	// Cache to avoid FString/FName conversions/compares
	static TMap<FName, UCanvas*> CanvasMap;
	UCanvas** FoundCanvas = CanvasMap.Find(CanvasName);
	if (!FoundCanvas)
	{
		UCanvas* CanvasObject = FindObject<UCanvas>(GetTransientPackage(),*CanvasName.ToString());
		if( !CanvasObject )
		{
			CanvasObject = NewObject<UCanvas>(GetTransientPackage(), CanvasName);
			CanvasObject->AddToRoot();
		}

		CanvasMap.Add(CanvasName, CanvasObject);
		return CanvasObject;
	}

	return *FoundCanvas;
}

void UGameViewportClient::Draw(FViewport* InViewport, FCanvas* SceneCanvas)
{
	//Valid SceneCanvas is required.  Make this explicit.
	check(SceneCanvas);

	BeginDrawDelegate.Broadcast();

	const bool bStereoRendering = GEngine->IsStereoscopic3D(InViewport);
	FCanvas* DebugCanvas = InViewport->GetDebugCanvas();

	// Create a temporary canvas if there isn't already one.
	static FName CanvasObjectName(TEXT("CanvasObject"));
	UCanvas* CanvasObject = GetCanvasByName(CanvasObjectName);
	CanvasObject->Canvas = SceneCanvas;		

	// Create temp debug canvas object
	FIntPoint DebugCanvasSize = InViewport->GetSizeXY();
	static FName DebugCanvasObjectName(TEXT("DebugCanvasObject"));
	UCanvas* DebugCanvasObject = GetCanvasByName(DebugCanvasObjectName);
	DebugCanvasObject->Init(DebugCanvasSize.X, DebugCanvasSize.Y, NULL, DebugCanvas);

	if (DebugCanvas)
	{
		DebugCanvas->SetScaledToRenderTarget(bStereoRendering);
		DebugCanvas->SetStereoRendering(bStereoRendering);
	}
	if (SceneCanvas)
	{
		SceneCanvas->SetScaledToRenderTarget(bStereoRendering);
		SceneCanvas->SetStereoRendering(bStereoRendering);
	}

	bool bUIDisableWorldRendering = false;
	FGameViewDrawer GameViewDrawer;

	UWorld* MyWorld = GetWorld();

	// create the view family for rendering the world scene to the viewport's render target
	FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues( 	
		InViewport,
		MyWorld->Scene,
		EngineShowFlags)
		.SetRealtimeUpdate(true));

#if WITH_EDITOR
	if (GIsEditor)
	{
		// Force enable view family show flag for HighDPI derived's screen percentage.
		ViewFamily.EngineShowFlags.ScreenPercentage = true;
	}
#endif

	ViewFamily.ViewExtensions = GEngine->ViewExtensions->GatherActiveExtensions(InViewport);

	for (auto ViewExt : ViewFamily.ViewExtensions)
	{
		ViewExt->SetupViewFamily(ViewFamily);
	}

	if (bStereoRendering && GEngine->XRSystem.IsValid() && GEngine->XRSystem->GetHMDDevice())
	{
		// Allow HMD to modify screen settings
		GEngine->XRSystem->GetHMDDevice()->UpdateScreenSettings(Viewport);
	}

	ESplitScreenType::Type SplitScreenConfig = GetCurrentSplitscreenConfiguration();
	ViewFamily.ViewMode = EViewModeIndex(ViewModeIndex);
	EngineShowFlagOverride(ESFIM_Game, ViewFamily.ViewMode, ViewFamily.EngineShowFlags, NAME_None, SplitScreenConfig != ESplitScreenType::None);

	if (ViewFamily.EngineShowFlags.VisualizeBuffer && AllowDebugViewmodes())
	{
		// Process the buffer visualization console command
		FName NewBufferVisualizationMode = NAME_None;
		static IConsoleVariable* ICVar = IConsoleManager::Get().FindConsoleVariable(FBufferVisualizationData::GetVisualizationTargetConsoleCommandName());
		if (ICVar)
		{
			static const FName OverviewName = TEXT("Overview");
			FString ModeNameString = ICVar->GetString();
			FName ModeName = *ModeNameString;
			if (ModeNameString.IsEmpty() || ModeName == OverviewName || ModeName == NAME_None)
			{
				NewBufferVisualizationMode = NAME_None;
			}
			else
			{
				if (GetBufferVisualizationData().GetMaterial(ModeName) == NULL)
				{
					// Mode is out of range, so display a message to the user, and reset the mode back to the previous valid one
					UE_LOG(LogConsoleResponse, Warning, TEXT("Buffer visualization mode '%s' does not exist"), *ModeNameString);
					NewBufferVisualizationMode = CurrentBufferVisualizationMode;
					// todo: cvars are user settings, here the cvar state is used to avoid log spam and to auto correct for the user (likely not what the user wants)
					ICVar->Set(*NewBufferVisualizationMode.GetPlainNameString(), ECVF_SetByCode);
				}
				else
				{
					NewBufferVisualizationMode = ModeName;
				}
			}
		}

		if (NewBufferVisualizationMode != CurrentBufferVisualizationMode)
		{
			CurrentBufferVisualizationMode = NewBufferVisualizationMode;
		}
	}

	TMap<ULocalPlayer*,FSceneView*> PlayerViewMap;

	FAudioDevice* AudioDevice = MyWorld->GetAudioDevice();

	for (FLocalPlayerIterator Iterator(GEngine, MyWorld); Iterator; ++Iterator)
	{
		ULocalPlayer* LocalPlayer = *Iterator;
		if (LocalPlayer)
		{
			APlayerController* PlayerController = LocalPlayer->PlayerController;

			const bool bEnableStereo = GEngine->IsStereoscopic3D(InViewport);
			const int32 NumViews = bStereoRendering ? ((ViewFamily.IsMonoscopicFarFieldEnabled()) ? 3 : 2) : 1;

			for (int32 i = 0; i < NumViews; ++i)
			{
				// Calculate the player's view information.
				FVector		ViewLocation;
				FRotator	ViewRotation;

				EStereoscopicPass PassType;
				if (!bStereoRendering)
				{
					PassType = eSSP_FULL;
				}
				else if (i == 0)
				{
					PassType = eSSP_LEFT_EYE;
				}
				else if (i == 1)
				{
					PassType = eSSP_RIGHT_EYE;
				}
				else
				{
					PassType = eSSP_MONOSCOPIC_EYE;
				}

				FSceneView* View = LocalPlayer->CalcSceneView(&ViewFamily, ViewLocation, ViewRotation, InViewport, &GameViewDrawer, PassType);

				if (View)
				{
					if (View->Family->EngineShowFlags.Wireframe)
					{
						// Wireframe color is emissive-only, and mesh-modifying materials do not use material substitution, hence...
						View->DiffuseOverrideParameter = FVector4(0.f, 0.f, 0.f, 0.f);
						View->SpecularOverrideParameter = FVector4(0.f, 0.f, 0.f, 0.f);
					}
					else if (View->Family->EngineShowFlags.OverrideDiffuseAndSpecular)
					{
						View->DiffuseOverrideParameter = FVector4(GEngine->LightingOnlyBrightness.R, GEngine->LightingOnlyBrightness.G, GEngine->LightingOnlyBrightness.B, 0.0f);
						View->SpecularOverrideParameter = FVector4(.1f, .1f, .1f, 0.0f);
					}
					else if (View->Family->EngineShowFlags.ReflectionOverride)
					{
						View->DiffuseOverrideParameter = FVector4(0.f, 0.f, 0.f, 0.f);
						View->SpecularOverrideParameter = FVector4(1, 1, 1, 0.0f);
						View->NormalOverrideParameter = FVector4(0, 0, 1, 0.0f);
						View->RoughnessOverrideParameter = FVector2D(0.0f, 0.0f);
					}

					if (!View->Family->EngineShowFlags.Diffuse)
					{
						View->DiffuseOverrideParameter = FVector4(0.f, 0.f, 0.f, 0.f);
					}

					if (!View->Family->EngineShowFlags.Specular)
					{
						View->SpecularOverrideParameter = FVector4(0.f, 0.f, 0.f, 0.f);
					}

					// NVCHANGE_BEGIN: Add VXGI
#if WITH_GFSDK_VXGI
					{
						static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataFloat(TEXT("r.VXGI.RoughnessOverride"));
						const float Roughness = CVar->GetValueOnGameThread();
						if (Roughness != 0.f)
						{
							View->RoughnessOverrideParameter = FVector2D(Roughness, 0.0f);
						}
					}
#endif
					// NVCHANGE_END: Add VXGI

					View->CurrentBufferVisualizationMode = CurrentBufferVisualizationMode;

					View->CameraConstrainedViewRect = View->UnscaledViewRect;

					// If this is the primary drawing pass, update things that depend on the view location
					if (i == 0)
					{
						// Save the location of the view.
						LocalPlayer->LastViewLocation = ViewLocation;

						PlayerViewMap.Add(LocalPlayer, View);

						// Update the listener.
						if (AudioDevice != NULL && PlayerController != NULL)
						{
							bool bUpdateListenerPosition = true;

							// If the main audio device is used for multiple PIE viewport clients, we only
							// want to update the main audio device listener position if it is in focus
							if (GEngine)
							{
								FAudioDeviceManager* AudioDeviceManager = GEngine->GetAudioDeviceManager();

								// If there is more than one world referencing the main audio device
								if (AudioDeviceManager->GetNumMainAudioDeviceWorlds() > 1)
								{
									uint32 MainAudioDeviceHandle = GEngine->GetAudioDeviceHandle();
									if (AudioDevice->DeviceHandle == MainAudioDeviceHandle && !bHasAudioFocus)
									{
										bUpdateListenerPosition = false;
									}
								}
							}

							if (bUpdateListenerPosition)
							{
								FVector Location;
								FVector ProjFront;
								FVector ProjRight;
								PlayerController->GetAudioListenerPosition(/*out*/ Location, /*out*/ ProjFront, /*out*/ ProjRight);

								FTransform ListenerTransform(FRotationMatrix::MakeFromXY(ProjFront, ProjRight));

								// Allow the HMD to adjust based on the head position of the player, as opposed to the view location
								if (GEngine->XRSystem.IsValid() && GEngine->StereoRenderingDevice.IsValid() && GEngine->StereoRenderingDevice->IsStereoEnabled())
								{
									const FVector Offset = GEngine->XRSystem->GetAudioListenerOffset();
									Location += ListenerTransform.TransformPositionNoScale(Offset);
								}

								ListenerTransform.SetTranslation(Location);
								ListenerTransform.NormalizeRotation();

								uint32 ViewportIndex = PlayerViewMap.Num() - 1;
								AudioDevice->SetListener(MyWorld, ViewportIndex, ListenerTransform, (View->bCameraCut ? 0.f : MyWorld->GetDeltaSeconds()));
							}
						}
						if (PassType == eSSP_LEFT_EYE)
						{
							// Save the size of the left eye view, so we can use it to reinitialize the DebugCanvasObject when rendering the console at the end of this method
							DebugCanvasSize = View->UnscaledViewRect.Size();
						}

					}

					// Add view information for resource streaming.
					IStreamingManager::Get().AddViewInformation(View->ViewMatrices.GetViewOrigin(), View->ViewRect.Width(), View->ViewRect.Width() * View->ViewMatrices.GetProjectionMatrix().M[0][0]);
					MyWorld->ViewLocationsRenderedLastFrame.Add(View->ViewMatrices.GetViewOrigin());
				}
			}
		}
	}

	FinalizeViews(&ViewFamily, PlayerViewMap);

	// Update level streaming.
	MyWorld->UpdateLevelStreaming();

	// Find largest rectangle bounded by all rendered views.
	uint32 MinX=InViewport->GetSizeXY().X, MinY=InViewport->GetSizeXY().Y, MaxX=0, MaxY=0;
	uint32 TotalArea = 0;
	{
		for( int32 ViewIndex = 0; ViewIndex < ViewFamily.Views.Num(); ++ViewIndex )
		{
			const FSceneView* View = ViewFamily.Views[ViewIndex];

			FIntRect UpscaledViewRect = View->UnscaledViewRect;

			MinX = FMath::Min<uint32>(UpscaledViewRect.Min.X, MinX);
			MinY = FMath::Min<uint32>(UpscaledViewRect.Min.Y, MinY);
			MaxX = FMath::Max<uint32>(UpscaledViewRect.Max.X, MaxX);
			MaxY = FMath::Max<uint32>(UpscaledViewRect.Max.Y, MaxY);
			TotalArea += FMath::TruncToInt(UpscaledViewRect.Width()) * FMath::TruncToInt(UpscaledViewRect.Height());
		}

		// To draw black borders around the rendered image (prevents artifacts from post processing passes that read outside of the image e.g. PostProcessAA)
		{
			int32 BlackBorders = FMath::Clamp(CVarSetBlackBordersEnabled.GetValueOnGameThread(), 0, 10);

			if(ViewFamily.Views.Num() == 1 && BlackBorders)
			{
				MinX += BlackBorders;
				MinY += BlackBorders;
				MaxX -= BlackBorders;
				MaxY -= BlackBorders;
				TotalArea = (MaxX - MinX) * (MaxY - MinY);
			}
		}
	}
	
	// If the views don't cover the entire bounding rectangle, clear the entire buffer.
	bool bBufferCleared = false;
	if (ViewFamily.Views.Num() == 0 || TotalArea != (MaxX-MinX)*(MaxY-MinY) || bDisableWorldRendering)
	{
		bool bStereoscopicPass = (ViewFamily.Views.Num() != 0 && ViewFamily.Views[0]->StereoPass != eSSP_FULL);
		if (bDisableWorldRendering || !bStereoscopicPass) // TotalArea computation does not work correctly for stereoscopic views
		{
			SceneCanvas->Clear(FLinearColor::Transparent);
		}
		
		bBufferCleared = true;
	}

	// Draw the player views.
	if (!bDisableWorldRendering && !bUIDisableWorldRendering && PlayerViewMap.Num() > 0) //-V560
	{
		GetRendererModule().BeginRenderingViewFamily(SceneCanvas,&ViewFamily);
	}
	else
	{
		// Make sure RHI resources get flushed if we're not using a renderer
		ENQUEUE_UNIQUE_RENDER_COMMAND( UGameViewportClient_FlushRHIResources,
		{ 
			FRHICommandListExecutor::GetImmediateCommandList().ImmediateFlush(EImmediateFlushType::FlushRHIThreadFlushResources);
		});
	}

	// Clear areas of the rendertarget (backbuffer) that aren't drawn over by the views.
	if (!bBufferCleared)
	{
		// clear left
		if( MinX > 0 )
		{
			SceneCanvas->DrawTile(0,0,MinX,InViewport->GetSizeXY().Y,0.0f,0.0f,1.0f,1.f,FLinearColor::Black,NULL,false);
		}
		// clear right
		if( MaxX < (uint32)InViewport->GetSizeXY().X )
		{
			SceneCanvas->DrawTile(MaxX,0,InViewport->GetSizeXY().X,InViewport->GetSizeXY().Y,0.0f,0.0f,1.0f,1.f,FLinearColor::Black,NULL,false);
		}
		// clear top
		if( MinY > 0 )
		{
			SceneCanvas->DrawTile(MinX,0,MaxX,MinY,0.0f,0.0f,1.0f,1.f,FLinearColor::Black,NULL,false);
		}
		// clear bottom
		if( MaxY < (uint32)InViewport->GetSizeXY().Y )
		{
			SceneCanvas->DrawTile(MinX,MaxY,MaxX,InViewport->GetSizeXY().Y,0.0f,0.0f,1.0f,1.f,FLinearColor::Black,NULL,false);
		}
	}
	
	// Remove temporary debug lines.
	if (MyWorld->LineBatcher != nullptr)
	{
		MyWorld->LineBatcher->Flush();
	}

	if (MyWorld->ForegroundLineBatcher != nullptr)
	{
		MyWorld->ForegroundLineBatcher->Flush();
	}

	// Draw FX debug information.
	if (MyWorld->FXSystem)
	{
		MyWorld->FXSystem->DrawDebug(SceneCanvas);
	}

	// Render the UI.
	{
		SCOPE_CYCLE_COUNTER(STAT_UIDrawingTime);

		// render HUD
		bool bDisplayedSubtitles = false;
		for( FConstPlayerControllerIterator Iterator = MyWorld->GetPlayerControllerIterator(); Iterator; ++Iterator )
		{
			APlayerController* PlayerController = Iterator->Get();
			if (PlayerController)
			{
				ULocalPlayer* LocalPlayer = Cast<ULocalPlayer>(PlayerController->Player);
				if( LocalPlayer )
				{
					FSceneView* View = PlayerViewMap.FindRef(LocalPlayer);
					if (View != NULL)
					{
						// rendering to directly to viewport target
						FVector CanvasOrigin(FMath::TruncToFloat(View->UnscaledViewRect.Min.X), FMath::TruncToInt(View->UnscaledViewRect.Min.Y), 0.f);
												
						CanvasObject->Init(View->UnscaledViewRect.Width(), View->UnscaledViewRect.Height(), View, SceneCanvas);

						// Set the canvas transform for the player's view rectangle.
						check(SceneCanvas);
						SceneCanvas->PushAbsoluteTransform(FTranslationMatrix(CanvasOrigin));
						CanvasObject->ApplySafeZoneTransform();						

						// Render the player's HUD.
						if( PlayerController->MyHUD )
						{
							SCOPE_CYCLE_COUNTER(STAT_HudTime);

							DebugCanvasObject->SceneView = View;
							PlayerController->MyHUD->SetCanvas(CanvasObject, DebugCanvasObject);

							PlayerController->MyHUD->PostRender();
							
							// Put these pointers back as if a blueprint breakpoint hits during HUD PostRender they can
							// have been changed
							CanvasObject->Canvas = SceneCanvas;
							DebugCanvasObject->Canvas = DebugCanvas;

							// A side effect of PostRender is that the playercontroller could be destroyed
							if (!PlayerController->IsPendingKill())
							{
								PlayerController->MyHUD->SetCanvas(NULL, NULL);
							}
						}

						if (DebugCanvas != NULL )
						{
							DebugCanvas->PushAbsoluteTransform(FTranslationMatrix(CanvasOrigin));
							UDebugDrawService::Draw(ViewFamily.EngineShowFlags, InViewport, View, DebugCanvas);
							DebugCanvas->PopTransform();
						}

						CanvasObject->PopSafeZoneTransform();
						SceneCanvas->PopTransform();						

						// draw subtitles
						if (!bDisplayedSubtitles)
						{
							FVector2D MinPos(0.f, 0.f);
							FVector2D MaxPos(1.f, 1.f);
							GetSubtitleRegion(MinPos, MaxPos);

							const uint32 SizeX = SceneCanvas->GetRenderTarget()->GetSizeXY().X;
							const uint32 SizeY = SceneCanvas->GetRenderTarget()->GetSizeXY().Y;
							FIntRect SubtitleRegion(FMath::TruncToInt(SizeX * MinPos.X), FMath::TruncToInt(SizeY * MinPos.Y), FMath::TruncToInt(SizeX * MaxPos.X), FMath::TruncToInt(SizeY * MaxPos.Y));
							FSubtitleManager::GetSubtitleManager()->DisplaySubtitles( SceneCanvas, SubtitleRegion, MyWorld->GetAudioTimeSeconds() );
							bDisplayedSubtitles = true;
						}
					}
				}
			}
		}

		//ensure canvas has been flushed before rendering UI
		SceneCanvas->Flush_GameThread();

		DrawnDelegate.Broadcast();

		// Allow the viewport to render additional stuff
		PostRender(DebugCanvasObject);
		
		// Render the console.
		if (ViewportConsole && DebugCanvas)
		{
			// Reset the debug canvas to be full-screen before drawing the console
			// (the debug draw service above has messed with the viewport size to fit it to a single player's subregion)
			DebugCanvasObject->Init(DebugCanvasSize.X, DebugCanvasSize.Y, NULL, DebugCanvas);

			ViewportConsole->PostRender_Console(DebugCanvasObject);
		}
	}


	// Grab the player camera location and orientation so we can pass that along to the stats drawing code.
	FVector PlayerCameraLocation = FVector::ZeroVector;
	FRotator PlayerCameraRotation = FRotator::ZeroRotator;
	{
		for( FConstPlayerControllerIterator Iterator = MyWorld->GetPlayerControllerIterator(); Iterator; ++Iterator )
		{
			(*Iterator)->GetPlayerViewPoint( PlayerCameraLocation, PlayerCameraRotation );
		}
	}

	DrawStatsHUD( MyWorld, InViewport, DebugCanvas, DebugCanvasObject, DebugProperties, PlayerCameraLocation, PlayerCameraRotation );

	if (GEngine->IsStereoscopic3D(InViewport))
	{
#if 0 //!UE_BUILD_SHIPPING
		// TODO: replace implementation in OculusHMD with a debug renderer
		if (GEngine->XRSystem.IsValid())
		{
			GEngine->XRSystem->DrawDebug(DebugCanvasObject);
		}
#endif
	}

	EndDrawDelegate.Broadcast();
}

void UGameViewportClient::ProcessScreenShots(FViewport* InViewport)
{
	if (GIsDumpingMovie || FScreenshotRequest::IsScreenshotRequested() || GIsHighResScreenshot)
	{	
		TArray<FColor> Bitmap;

		bool bShowUI = false;
		TSharedPtr<SWindow> WindowPtr = GetWindow();
		if (!GIsDumpingMovie && (FScreenshotRequest::ShouldShowUI() && WindowPtr.IsValid()))
		{
			bShowUI = true;
		}

		bool bScreenshotSuccessful = false;
		FIntVector Size(InViewport->GetSizeXY().X, InViewport->GetSizeXY().Y, 0);
		if( bShowUI && FSlateApplication::IsInitialized() )
		{
			TSharedRef<SWidget> WindowRef = WindowPtr.ToSharedRef();
			bScreenshotSuccessful = FSlateApplication::Get().TakeScreenshot( WindowRef, Bitmap, Size);
			GScreenshotResolutionX = Size.X;
			GScreenshotResolutionY = Size.Y;
		}
		else
		{
			bScreenshotSuccessful = GetViewportScreenShot(InViewport, Bitmap);
		}

		if (bScreenshotSuccessful)
		{
			if (ScreenshotCapturedDelegate.IsBound() && CVarScreenshotDelegate.GetValueOnGameThread())
			{
				// Ensure that all pixels' alpha is set to 255
				for (auto& Color : Bitmap)
				{
					Color.A = 255;
				}

				// If delegate subscribed, fire it instead of writing out a file to disk
				ScreenshotCapturedDelegate.Broadcast(Size.X, Size.Y, Bitmap);
			}
			else
			{
				FString ScreenShotName = FScreenshotRequest::GetFilename();
				if (GIsDumpingMovie && ScreenShotName.IsEmpty())
				{
					// Request a new screenshot with a formatted name
					bShowUI = false;
					const bool bAddFilenameSuffix = true;
					FScreenshotRequest::RequestScreenshot(FString(), bShowUI, bAddFilenameSuffix);
					ScreenShotName = FScreenshotRequest::GetFilename();
				}

				GetHighResScreenshotConfig().MergeMaskIntoAlpha(Bitmap);
				
				FIntRect SourceRect(0, 0, GScreenshotResolutionX, GScreenshotResolutionY);
				if (GIsHighResScreenshot)
				{
					SourceRect = GetHighResScreenshotConfig().CaptureRegion;
				}

				if (!FPaths::GetExtension(ScreenShotName).IsEmpty())
				{
					ScreenShotName = FPaths::GetBaseFilename(ScreenShotName, false);
					ScreenShotName += TEXT(".png");
				}

				// Save the contents of the array to a png file.
				TArray<uint8> CompressedBitmap;
				FImageUtils::CompressImageArray(Size.X, Size.Y, Bitmap, CompressedBitmap);
				FFileHelper::SaveArrayToFile(CompressedBitmap, *ScreenShotName);
			}
		}

		FScreenshotRequest::Reset();
		// Reeanble screen messages - if we are NOT capturing a movie
		GAreScreenMessagesEnabled = GScreenMessagesRestoreState;
	}
}

void UGameViewportClient::Precache()
{
	if(!GIsEditor)
	{
		// Precache sounds...
		if (FAudioDevice* AudioDevice = GetWorld()->GetAudioDevice())
		{
			UE_LOG(LogPlayerManagement, Log, TEXT("Precaching sounds..."));
			for(TObjectIterator<USoundWave> It;It;++It)
			{
				USoundWave* SoundWave = *It;
				AudioDevice->Precache( SoundWave );
			}
			UE_LOG(LogPlayerManagement, Log, TEXT("Precaching sounds completed..."));
		}
	}

	// Log time till first precache is finished.
	static bool bIsFirstCallOfFunction = true;
	if( bIsFirstCallOfFunction )
	{
		UE_LOG(LogPlayerManagement, Log, TEXT("%5.2f seconds passed since startup."),FPlatformTime::Seconds()-GStartTime);
		bIsFirstCallOfFunction = false;
	}
}

TOptional<bool> UGameViewportClient::QueryShowFocus(const EFocusCause InFocusCause) const
{
	UUserInterfaceSettings* UISettings = GetMutableDefault<UUserInterfaceSettings>(UUserInterfaceSettings::StaticClass());
	
	if ( UISettings->RenderFocusRule == ERenderFocusRule::Never ||
		(UISettings->RenderFocusRule == ERenderFocusRule::NonPointer && InFocusCause == EFocusCause::Mouse) ||
		(UISettings->RenderFocusRule == ERenderFocusRule::NavigationOnly && InFocusCause != EFocusCause::Navigation))
	{
		return false;
	}

	return true;
}

void UGameViewportClient::LostFocus(FViewport* InViewport)
{
	// We need to reset some key inputs, since keyup events will sometimes not be processed (such as going into immersive/maximized mode).  
	// Resetting them will prevent them from "sticking"
	UWorld* const ViewportWorld = GetWorld();
	if (ViewportWorld && !ViewportWorld->bIsTearingDown)
	{
		for (FConstPlayerControllerIterator Iterator = ViewportWorld->GetPlayerControllerIterator(); Iterator; ++Iterator)
		{
			APlayerController* const PlayerController = Iterator->Get();
			if (PlayerController)
			{
				PlayerController->FlushPressedKeys();
			}
		}
	}

	if (GEngine && GEngine->GetAudioDeviceManager())
	{
		bHasAudioFocus = false;
	}
}

void UGameViewportClient::ReceivedFocus(FViewport* InViewport)
{
#if PLATFORM_DESKTOP || PLATFORM_HTML5
	if (GetDefault<UInputSettings>()->bUseMouseForTouch && GetGameViewport() && !GetGameViewport()->GetPlayInEditorIsSimulate())
	{
		FSlateApplication::Get().SetGameIsFakingTouchEvents(true);
	}
#endif

	if (GEngine && GEngine->GetAudioDeviceManager())
	{ 
		GEngine->GetAudioDeviceManager()->SetActiveDevice(AudioDeviceHandle);
		bHasAudioFocus = true;
	}
}

bool UGameViewportClient::IsFocused(FViewport* InViewport)
{
	return InViewport->HasFocus() || InViewport->HasMouseCapture();
}

void UGameViewportClient::Activated(FViewport* InViewport, const FWindowActivateEvent& InActivateEvent)
{
	ReceivedFocus(InViewport);
}

void UGameViewportClient::Deactivated(FViewport* InViewport, const FWindowActivateEvent& InActivateEvent)
{
	LostFocus(InViewport);
}

bool UGameViewportClient::WindowCloseRequested()
{
	return !WindowCloseRequestedDelegate.IsBound() || WindowCloseRequestedDelegate.Execute();
}

void UGameViewportClient::CloseRequested(FViewport* InViewport)
{
	check(InViewport == Viewport);

#if PLATFORM_DESKTOP || PLATFORM_HTML5
	FSlateApplication::Get().SetGameIsFakingTouchEvents(false);
#endif
	
	// broadcast close request to anyone that registered an interest
	CloseRequestedDelegate.Broadcast(InViewport);

	SetViewportFrame(NULL);

	// If this viewport has a high res screenshot window attached to it, close it
	if (HighResScreenshotDialog.IsValid())
	{
		HighResScreenshotDialog.Pin()->RequestDestroyWindow();
		HighResScreenshotDialog = NULL;
	}
}

bool UGameViewportClient::IsOrtho() const
{
	return false;
}

void UGameViewportClient::PostRender(UCanvas* Canvas)
{
	if( bShowTitleSafeZone )
	{
		DrawTitleSafeArea(Canvas);
	}

	// Draw the transition screen.
	DrawTransition(Canvas);
}

void UGameViewportClient::PeekTravelFailureMessages(UWorld* InWorld, ETravelFailure::Type FailureType, const FString& ErrorString)
{
	UE_LOG(LogNet, Warning, TEXT("Travel Failure: [%s]: %s"), ETravelFailure::ToString(FailureType), *ErrorString);
}

void UGameViewportClient::PeekNetworkFailureMessages(UWorld *InWorld, UNetDriver *NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString)
{
	UE_LOG(LogNet, Warning, TEXT("Network Failure: %s[%s]: %s"), NetDriver ? *NetDriver->NetDriverName.ToString() : TEXT("NULL"), ENetworkFailure::ToString(FailureType), *ErrorString);
}

void UGameViewportClient::SSSwapControllers()
{
#if !UE_BUILD_SHIPPING
	UEngine* const Engine = GetOuterUEngine();

	int32 const NumPlayers = Engine ? Engine->GetNumGamePlayers(this) : 0;
	if (NumPlayers > 1)
	{
		ULocalPlayer* const LP = Engine ? Engine->GetFirstGamePlayer(this) : nullptr;
		const int32 TmpControllerID = LP ? LP->GetControllerId() : 0;

		for (int32 Idx = 0; Idx<NumPlayers-1; ++Idx)
		{
			Engine->GetGamePlayer(this, Idx)->SetControllerId(Engine->GetGamePlayer(this, Idx + 1)->GetControllerId());
		}
		Engine->GetGamePlayer(this, NumPlayers-1)->SetControllerId(TmpControllerID);
	}
#endif
}

void UGameViewportClient::ShowTitleSafeArea()
{
#if !UE_BUILD_SHIPPING
	bShowTitleSafeZone = !bShowTitleSafeZone;
#endif
}

void UGameViewportClient::SetConsoleTarget(int32 PlayerIndex)
{
#if !UE_BUILD_SHIPPING
	if (ViewportConsole)
	{
		if(PlayerIndex >= 0 && PlayerIndex < GetOuterUEngine()->GetNumGamePlayers(this))
		{
			ViewportConsole->ConsoleTargetPlayer = GetOuterUEngine()->GetGamePlayer(this, PlayerIndex);
		}
		else
		{
			ViewportConsole->ConsoleTargetPlayer = NULL;
		}
	}
#endif
}


ULocalPlayer* UGameViewportClient::SetupInitialLocalPlayer(FString& OutError)
{
	check(GetOuterUEngine()->ConsoleClass != NULL);

	ActiveSplitscreenType = ESplitScreenType::None;

#if ALLOW_CONSOLE
	// Create the viewport's console.
	ViewportConsole = NewObject<UConsole>(this, GetOuterUEngine()->ConsoleClass);
	// register console to get all log messages
	GLog->AddOutputDevice(ViewportConsole);
#endif // !UE_BUILD_SHIPPING

	// Keep an eye on any network or server travel failures
	GEngine->OnTravelFailure().AddUObject(this, &UGameViewportClient::PeekTravelFailureMessages);
	GEngine->OnNetworkFailure().AddUObject(this, &UGameViewportClient::PeekNetworkFailureMessages);

	UGameInstance * ViewportGameInstance = GEngine->GetWorldContextFromGameViewportChecked(this).OwningGameInstance;

	if ( !ensure( ViewportGameInstance != NULL ) )
	{
		return NULL;
	}

	// Create the initial player - this is necessary or we can't render anything in-game.
	return ViewportGameInstance->CreateInitialPlayer(OutError);
}

void UGameViewportClient::UpdateActiveSplitscreenType()
{
	ESplitScreenType::Type SplitType = ESplitScreenType::None;
	const int32 NumPlayers = GEngine->GetNumGamePlayers(GetWorld());
	const UGameMapsSettings* Settings = GetDefault<UGameMapsSettings>();

	if (Settings->bUseSplitscreen && !bDisableSplitScreenOverride)
	{
		switch (NumPlayers)
		{
		case 0:
		case 1:
			SplitType = ESplitScreenType::None;
			break;

		case 2:
			switch (Settings->TwoPlayerSplitscreenLayout)
			{
			case ETwoPlayerSplitScreenType::Horizontal:
				SplitType = ESplitScreenType::TwoPlayer_Horizontal;
				break;

			case ETwoPlayerSplitScreenType::Vertical:
				SplitType = ESplitScreenType::TwoPlayer_Vertical;
				break;

			default:
				check(0);
			}
			break;

		case 3:
			switch (Settings->ThreePlayerSplitscreenLayout)
			{
			case EThreePlayerSplitScreenType::FavorTop:
				SplitType = ESplitScreenType::ThreePlayer_FavorTop;
				break;

			case EThreePlayerSplitScreenType::FavorBottom:
				SplitType = ESplitScreenType::ThreePlayer_FavorBottom;
				break;

			default:
				check(0);
			}
			break;

		default:
			ensure(NumPlayers == 4);
			SplitType = ESplitScreenType::FourPlayer;
			break;
		}
	}
	else
	{
		SplitType = ESplitScreenType::None;
	}

	ActiveSplitscreenType = SplitType;
}

void UGameViewportClient::LayoutPlayers()
{
	UpdateActiveSplitscreenType();
	const ESplitScreenType::Type SplitType = GetCurrentSplitscreenConfiguration();
	
	// Initialize the players
	const TArray<ULocalPlayer*>& PlayerList = GetOuterUEngine()->GetGamePlayers(this);

	for ( int32 PlayerIdx = 0; PlayerIdx < PlayerList.Num(); PlayerIdx++ )
	{
		if ( SplitType < SplitscreenInfo.Num() && PlayerIdx < SplitscreenInfo[SplitType].PlayerData.Num() )
		{
			PlayerList[PlayerIdx]->Size.X =	SplitscreenInfo[SplitType].PlayerData[PlayerIdx].SizeX;
			PlayerList[PlayerIdx]->Size.Y =	SplitscreenInfo[SplitType].PlayerData[PlayerIdx].SizeY;
			PlayerList[PlayerIdx]->Origin.X =	SplitscreenInfo[SplitType].PlayerData[PlayerIdx].OriginX;
			PlayerList[PlayerIdx]->Origin.Y =	SplitscreenInfo[SplitType].PlayerData[PlayerIdx].OriginY;
		}
		else
		{
			PlayerList[PlayerIdx]->Size.X =	0.f;
			PlayerList[PlayerIdx]->Size.Y =	0.f;
			PlayerList[PlayerIdx]->Origin.X =	0.f;
			PlayerList[PlayerIdx]->Origin.Y =	0.f;
		}
	}
}

void UGameViewportClient::SetDisableSplitscreenOverride( const bool bDisabled )
{
	bDisableSplitScreenOverride = bDisabled;
	LayoutPlayers();
}

void UGameViewportClient::GetSubtitleRegion(FVector2D& MinPos, FVector2D& MaxPos)
{
	MaxPos.X = 1.0f;
	MaxPos.Y = (GetOuterUEngine()->GetNumGamePlayers(this) == 1) ? 0.9f : 0.5f;
}


int32 UGameViewportClient::ConvertLocalPlayerToGamePlayerIndex( ULocalPlayer* LPlayer )
{
	return GetOuterUEngine()->GetGamePlayers(this).Find( LPlayer );
}

bool UGameViewportClient::HasTopSafeZone( int32 LocalPlayerIndex )
{
	switch ( GetCurrentSplitscreenConfiguration() )
	{
	case ESplitScreenType::None:
	case ESplitScreenType::TwoPlayer_Vertical:
		return true;

	case ESplitScreenType::TwoPlayer_Horizontal:
	case ESplitScreenType::ThreePlayer_FavorTop:
		return (LocalPlayerIndex == 0) ? true : false;

	case ESplitScreenType::ThreePlayer_FavorBottom:
	case ESplitScreenType::FourPlayer:
		return (LocalPlayerIndex < 2) ? true : false;
	}

	return false;
}

bool UGameViewportClient::HasBottomSafeZone( int32 LocalPlayerIndex )
{
	switch ( GetCurrentSplitscreenConfiguration() )
	{
	case ESplitScreenType::None:
	case ESplitScreenType::TwoPlayer_Vertical:
		return true;

	case ESplitScreenType::TwoPlayer_Horizontal:
	case ESplitScreenType::ThreePlayer_FavorTop:
		return (LocalPlayerIndex == 0) ? false : true;

	case ESplitScreenType::ThreePlayer_FavorBottom:
	case ESplitScreenType::FourPlayer:
		return (LocalPlayerIndex > 1) ? true : false;
	}

	return false;
}

bool UGameViewportClient::HasLeftSafeZone( int32 LocalPlayerIndex )
{
	switch ( GetCurrentSplitscreenConfiguration() )
	{
	case ESplitScreenType::None:
	case ESplitScreenType::TwoPlayer_Horizontal:
		return true;

	case ESplitScreenType::TwoPlayer_Vertical:
		return (LocalPlayerIndex == 0) ? true : false;

	case ESplitScreenType::ThreePlayer_FavorTop:
		return (LocalPlayerIndex < 2) ? true : false;

	case ESplitScreenType::ThreePlayer_FavorBottom:
	case ESplitScreenType::FourPlayer:
		return (LocalPlayerIndex == 0 || LocalPlayerIndex == 2) ? true : false;
	}

	return false;
}

bool UGameViewportClient::HasRightSafeZone( int32 LocalPlayerIndex )
{
	switch ( GetCurrentSplitscreenConfiguration() )
	{
	case ESplitScreenType::None:
	case ESplitScreenType::TwoPlayer_Horizontal:
		return true;

	case ESplitScreenType::TwoPlayer_Vertical:
	case ESplitScreenType::ThreePlayer_FavorBottom:
		return (LocalPlayerIndex > 0) ? true : false;

	case ESplitScreenType::ThreePlayer_FavorTop:
		return (LocalPlayerIndex == 1) ? false : true;

	case ESplitScreenType::FourPlayer:
		return (LocalPlayerIndex == 0 || LocalPlayerIndex == 2) ? false : true;
	}

	return false;
}


void UGameViewportClient::GetPixelSizeOfScreen( float& Width, float& Height, UCanvas* Canvas, int32 LocalPlayerIndex )
{
	switch ( GetCurrentSplitscreenConfiguration() )
	{
	case ESplitScreenType::None:
		Width = Canvas->ClipX;
		Height = Canvas->ClipY;
		return;
	case ESplitScreenType::TwoPlayer_Horizontal:
		Width = Canvas->ClipX;
		Height = Canvas->ClipY * 2;
		return;
	case ESplitScreenType::TwoPlayer_Vertical:
		Width = Canvas->ClipX * 2;
		Height = Canvas->ClipY;
		return;
	case ESplitScreenType::ThreePlayer_FavorTop:
		if (LocalPlayerIndex == 0)
		{
			Width = Canvas->ClipX;
		}
		else
		{
			Width = Canvas->ClipX * 2;
		}
		Height = Canvas->ClipY * 2;
		return;
	case ESplitScreenType::ThreePlayer_FavorBottom:
		if (LocalPlayerIndex == 2)
		{
			Width = Canvas->ClipX;
		}
		else
		{
			Width = Canvas->ClipX * 2;
		}
		Height = Canvas->ClipY * 2;
		return;
	case ESplitScreenType::FourPlayer:
		Width = Canvas->ClipX * 2;
		Height = Canvas->ClipY * 2;
		return;
	}
}

void UGameViewportClient::CalculateSafeZoneValues( float& Horizontal, float& Vertical, UCanvas* Canvas, int32 LocalPlayerIndex, bool bUseMaxPercent )
{

	float XSafeZoneToUse = bUseMaxPercent ? TitleSafeZone.MaxPercentX : TitleSafeZone.RecommendedPercentX;
	float YSafeZoneToUse = bUseMaxPercent ? TitleSafeZone.MaxPercentY : TitleSafeZone.RecommendedPercentY;

	float ScreenWidth, ScreenHeight;
	GetPixelSizeOfScreen( ScreenWidth, ScreenHeight, Canvas, LocalPlayerIndex );
	Horizontal = (ScreenWidth * (1 - XSafeZoneToUse) / 2.0f);
	Vertical = (ScreenHeight * (1 - YSafeZoneToUse) / 2.0f);
}


bool UGameViewportClient::CalculateDeadZoneForAllSides( ULocalPlayer* LPlayer, UCanvas* Canvas, float& fTopSafeZone, float& fBottomSafeZone, float& fLeftSafeZone, float& fRightSafeZone, bool bUseMaxPercent )
{
	// save separate - if the split screen is in bottom right, then

	if ( LPlayer != NULL )
	{
		int32 LocalPlayerIndex = ConvertLocalPlayerToGamePlayerIndex( LPlayer );

		if ( LocalPlayerIndex != -1 )
		{
			// see if this player should have a safe zone for any particular zonetype
			bool bHasTopSafeZone = HasTopSafeZone( LocalPlayerIndex );
			bool bHasBottomSafeZone = HasBottomSafeZone( LocalPlayerIndex );
			bool bHasLeftSafeZone = HasLeftSafeZone( LocalPlayerIndex );
			bool bHasRightSafeZone = HasRightSafeZone( LocalPlayerIndex );

			// if they need a safezone, then calculate it and save it
			if ( bHasTopSafeZone || bHasBottomSafeZone || bHasLeftSafeZone || bHasRightSafeZone)
			{
				// calculate the safezones
				float HorizSafeZoneValue, VertSafeZoneValue;
				CalculateSafeZoneValues( HorizSafeZoneValue, VertSafeZoneValue, Canvas, LocalPlayerIndex, bUseMaxPercent );

				if (bHasTopSafeZone)
				{
					fTopSafeZone = VertSafeZoneValue;
				}
				else
				{
					fTopSafeZone = 0.f;
				}

				if (bHasBottomSafeZone)
				{
					fBottomSafeZone = VertSafeZoneValue;
				}
				else
				{
					fBottomSafeZone = 0.f;
				}

				if (bHasLeftSafeZone)
				{
					fLeftSafeZone = HorizSafeZoneValue;
				}
				else
				{
					fLeftSafeZone = 0.f;
				}

				if (bHasRightSafeZone)
				{
					fRightSafeZone = HorizSafeZoneValue;
				}
				else
				{
					fRightSafeZone = 0.f;
				}

				return true;
			}
		}
	}
	return false;
}

void UGameViewportClient::DrawTitleSafeArea( UCanvas* Canvas )
{	
	// red colored max safe area box
	Canvas->SetDrawColor(255,0,0,255);
	float X = Canvas->ClipX * (1 - TitleSafeZone.MaxPercentX) / 2.0f;
	float Y = Canvas->ClipY * (1 - TitleSafeZone.MaxPercentY) / 2.0f;
	FCanvasBoxItem BoxItem( FVector2D( X, Y ), FVector2D( Canvas->ClipX * TitleSafeZone.MaxPercentX, Canvas->ClipY * TitleSafeZone.MaxPercentY ) );
	BoxItem.SetColor( FLinearColor::Red );
	Canvas->DrawItem( BoxItem );
		
	// yellow colored recommended safe area box
	X = Canvas->ClipX * (1 - TitleSafeZone.RecommendedPercentX) / 2.0f;
	Y = Canvas->ClipY * (1 - TitleSafeZone.RecommendedPercentY) / 2.0f;
	BoxItem.SetColor( FLinearColor::Yellow );
	BoxItem.Size = FVector2D( Canvas->ClipX * TitleSafeZone.RecommendedPercentX, Canvas->ClipY * TitleSafeZone.RecommendedPercentY );
	Canvas->DrawItem( BoxItem, X, Y );
}

void UGameViewportClient::DrawTransition(UCanvas* Canvas)
{
	if (bSuppressTransitionMessage == false)
	{
		switch (GetOuterUEngine()->TransitionType)
		{
		case TT_Loading:
			DrawTransitionMessage(Canvas, NSLOCTEXT("GameViewportClient", "LoadingMessage", "LOADING").ToString());
			break;
		case TT_Saving:
			DrawTransitionMessage(Canvas, NSLOCTEXT("GameViewportClient", "SavingMessage", "SAVING").ToString());
			break;
		case TT_Connecting:
			DrawTransitionMessage(Canvas, NSLOCTEXT("GameViewportClient", "ConnectingMessage", "CONNECTING").ToString());
			break;
		case TT_Precaching:
			DrawTransitionMessage(Canvas, NSLOCTEXT("GameViewportClient", "PrecachingMessage", "PRECACHING").ToString());
			break;
		case TT_Paused:
			DrawTransitionMessage(Canvas, NSLOCTEXT("GameViewportClient", "PausedMessage", "PAUSED").ToString());
			break;
		case TT_WaitingToConnect:
			DrawTransitionMessage(Canvas, TEXT("Waiting to connect...")); // Temp - localization of the FString messages is broke atm. Loc this when its fixed.
			break;
		}
	}
}

void UGameViewportClient::DrawTransitionMessage(UCanvas* Canvas,const FString& Message)
{
	UFont* Font = GEngine->GetLargeFont();
	FCanvasTextItem TextItem( FVector2D::ZeroVector, FText::GetEmpty(), Font, FLinearColor::Blue);
	TextItem.EnableShadow( FLinearColor::Black );
	TextItem.Text = FText::FromString(Message);
	float XL, YL;
	Canvas->StrLen( Font , Message, XL, YL );
	Canvas->DrawItem( TextItem, 0.5f * (Canvas->ClipX - XL), 0.66f * Canvas->ClipY - YL * 0.5f );	
}

void UGameViewportClient::NotifyPlayerAdded( int32 PlayerIndex, ULocalPlayer* AddedPlayer )
{
	LayoutPlayers();

	FSlateApplication::Get().SetUserFocusToGameViewport(PlayerIndex);

	TSharedPtr< IGameLayerManager > GameLayerManager(GameLayerManagerPtr.Pin());
	if ( GameLayerManager.IsValid() )
	{
		GameLayerManager->NotifyPlayerAdded(PlayerIndex, AddedPlayer);
	}

	PlayerAddedDelegate.Broadcast( PlayerIndex );
}

void UGameViewportClient::NotifyPlayerRemoved( int32 PlayerIndex, ULocalPlayer* RemovedPlayer )
{
	LayoutPlayers();

	TSharedPtr< IGameLayerManager > GameLayerManager(GameLayerManagerPtr.Pin());
	if ( GameLayerManager.IsValid() )
	{
		GameLayerManager->NotifyPlayerRemoved(PlayerIndex, RemovedPlayer);
	}

	PlayerRemovedDelegate.Broadcast( PlayerIndex );
}

void UGameViewportClient::AddViewportWidgetContent( TSharedRef<SWidget> ViewportContent, const int32 ZOrder )
{
	TSharedPtr< SOverlay > PinnedViewportOverlayWidget( ViewportOverlayWidget.Pin() );
	if( ensure( PinnedViewportOverlayWidget.IsValid() ) )
	{
		// NOTE: Returns FSimpleSlot but we're ignoring here.  Could be used for alignment though.
		PinnedViewportOverlayWidget->AddSlot( ZOrder )
			[
				ViewportContent
			];
	}
}

void UGameViewportClient::RemoveViewportWidgetContent( TSharedRef<SWidget> ViewportContent )
{
	TSharedPtr< SOverlay > PinnedViewportOverlayWidget( ViewportOverlayWidget.Pin() );
	if( PinnedViewportOverlayWidget.IsValid() )
	{
		PinnedViewportOverlayWidget->RemoveSlot( ViewportContent );
	}
}

void UGameViewportClient::AddViewportWidgetForPlayer(ULocalPlayer* Player, TSharedRef<SWidget> ViewportContent, const int32 ZOrder)
{
	TSharedPtr< IGameLayerManager > GameLayerManager(GameLayerManagerPtr.Pin());
	if ( GameLayerManager.IsValid() )
	{
		GameLayerManager->AddWidgetForPlayer(Player, ViewportContent, ZOrder);
	}
}

void UGameViewportClient::RemoveViewportWidgetForPlayer(ULocalPlayer* Player, TSharedRef<SWidget> ViewportContent)
{
	TSharedPtr< IGameLayerManager > GameLayerManager(GameLayerManagerPtr.Pin());
	if ( GameLayerManager.IsValid() )
	{
		GameLayerManager->RemoveWidgetForPlayer(Player, ViewportContent);
	}
}

void UGameViewportClient::RemoveAllViewportWidgets()
{
	CursorWidgets.Empty();

	TSharedPtr< SOverlay > PinnedViewportOverlayWidget( ViewportOverlayWidget.Pin() );
	if( PinnedViewportOverlayWidget.IsValid() )
	{
		PinnedViewportOverlayWidget->ClearChildren();
	}

	TSharedPtr< IGameLayerManager > GameLayerManager(GameLayerManagerPtr.Pin());
	if ( GameLayerManager.IsValid() )
		{
		GameLayerManager->ClearWidgets();
	}
}

void UGameViewportClient::VerifyPathRenderingComponents()
{
	const bool bShowPaths = !!EngineShowFlags.Navigation;

	UWorld* const ViewportWorld = GetWorld();

	// make sure nav mesh has a rendering component
	ANavigationData* const NavData = (ViewportWorld && ViewportWorld->GetNavigationSystem() != nullptr)
		? ViewportWorld->GetNavigationSystem()->GetMainNavData(FNavigationSystem::DontCreate)
		: NULL;

	if(NavData && NavData->RenderingComp == NULL)
	{
		NavData->RenderingComp = NavData->ConstructRenderingComponent();
		if (NavData->RenderingComp)
		{
			NavData->RenderingComp->SetVisibility(bShowPaths);
			NavData->RenderingComp->RegisterComponent();
		}
	}

	if(NavData == NULL)
	{
		UE_LOG(LogPlayerManagement, Warning, TEXT("No NavData found when calling UGameViewportClient::VerifyPathRenderingComponents()"));
	}
}

bool UGameViewportClient::CaptureMouseOnLaunch()
{
	return GetDefault<UInputSettings>()->bCaptureMouseOnLaunch;
}

bool UGameViewportClient::Exec( UWorld* InWorld, const TCHAR* Cmd,FOutputDevice& Ar)
{
	if ( FParse::Command(&Cmd,TEXT("FORCEFULLSCREEN")) )
	{
		return HandleForceFullscreenCommand( Cmd, Ar );
	}
	else if( FParse::Command(&Cmd,TEXT("SHOW")) )
	{
		return HandleShowCommand( Cmd, Ar, InWorld );
	}
	else if( FParse::Command(&Cmd,TEXT("SHOWLAYER")) )
	{
		return HandleShowLayerCommand( Cmd, Ar, InWorld );
	}
	else if (FParse::Command(&Cmd,TEXT("VIEWMODE")))
	{
		return HandleViewModeCommand( Cmd, Ar, InWorld );
	}
	else if (FParse::Command(&Cmd, TEXT("NEXTVIEWMODE")))
	{
		return HandleNextViewModeCommand( Cmd, Ar, InWorld );
	}
	else if (FParse::Command(&Cmd, TEXT("PREVVIEWMODE")))
	{
		return HandlePrevViewModeCommand( Cmd, Ar, InWorld );
	}
	else if( FParse::Command(&Cmd,TEXT("PRECACHE")) )
	{
		return HandlePreCacheCommand( Cmd, Ar );
	}
	else if( FParse::Command(&Cmd,TEXT("TOGGLE_FULLSCREEN")) || FParse::Command(&Cmd,TEXT("FULLSCREEN")) )
	{
		return HandleToggleFullscreenCommand();
	}	
	else if( FParse::Command(&Cmd,TEXT("SETRES")) )
	{
		return HandleSetResCommand( Cmd, Ar );
	}
	else if( FParse::Command(&Cmd,TEXT("HighResShot")) )
	{
		return HandleHighresScreenshotCommand( Cmd, Ar );
	}
	else if( FParse::Command(&Cmd,TEXT("HighResShotUI")) )
	{
		return HandleHighresScreenshotUICommand( Cmd, Ar );
	}
	else if( FParse::Command(&Cmd,TEXT("SHOT")) || FParse::Command(&Cmd,TEXT("SCREENSHOT")) )
	{
		return HandleScreenshotCommand( Cmd, Ar );	
	}
	else if ( FParse::Command(&Cmd, TEXT("BUGSCREENSHOTWITHHUDINFO")) )
	{
		return HandleBugScreenshotwithHUDInfoCommand( Cmd, Ar );
	}
	else if ( FParse::Command(&Cmd,TEXT("BUGSCREENSHOT")) )
	{
		return HandleBugScreenshotCommand( Cmd, Ar );
	}
	else if( FParse::Command(&Cmd,TEXT("KILLPARTICLES")) )
	{
		return HandleKillParticlesCommand( Cmd, Ar );	
	}
	else if( FParse::Command(&Cmd,TEXT("FORCESKELLOD")) )
	{
		return HandleForceSkelLODCommand( Cmd, Ar, InWorld );
	}
	else if (FParse::Command(&Cmd, TEXT("DISPLAY")))
	{
		return HandleDisplayCommand( Cmd, Ar );
	}
	else if (FParse::Command(&Cmd, TEXT("DISPLAYALL")))
	{
		return HandleDisplayAllCommand( Cmd, Ar );
	}
	else if (FParse::Command(&Cmd, TEXT("DISPLAYALLLOCATION")))
	{
		return HandleDisplayAllLocationCommand( Cmd, Ar );
	}
	else if (FParse::Command(&Cmd, TEXT("DISPLAYALLROTATION")))
	{
		return HandleDisplayAllRotationCommand( Cmd, Ar );
	}
	else if (FParse::Command(&Cmd, TEXT("DISPLAYCLEAR")))
	{
		return HandleDisplayClearCommand( Cmd, Ar );
	}
	else if(FParse::Command(&Cmd, TEXT("TEXTUREDEFRAG")))
	{
		return HandleTextureDefragCommand( Cmd, Ar );
	}
	else if (FParse::Command(&Cmd, TEXT("TOGGLEMIPFADE")))
	{
		return HandleToggleMIPFadeCommand( Cmd, Ar );
	}
	else if (FParse::Command(&Cmd, TEXT("PAUSERENDERCLOCK")))
	{
		return HandlePauseRenderClockCommand( Cmd, Ar );
	}

	if(ProcessConsoleExec(Cmd,Ar,NULL))
	{
		return true;
	}
	else if ( GameInstance && (GameInstance->Exec(InWorld, Cmd, Ar) || GameInstance->ProcessConsoleExec(Cmd, Ar, nullptr)) )
	{
		return true;
	}
	else if( GEngine->Exec( InWorld, Cmd,Ar) )
	{
		return true;
	}
	else
	{
		return false;
	}
}

bool UGameViewportClient::HandleForceFullscreenCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	GForceFullscreen = !GForceFullscreen;
	return true;
}

bool UGameViewportClient::HandleShowCommand( const TCHAR* Cmd, FOutputDevice& Ar, UWorld* InWorld )
{
#if UE_BUILD_SHIPPING
	// don't allow show flags in net games, but on con
	if ( InWorld->GetNetMode() != NM_Standalone || (GEngine->GetWorldContextFromWorldChecked(InWorld).PendingNetGame != NULL) )
	{
		return true;
	}
	// the effects of this cannot be easily reversed, so prevent the user from playing network games without restarting to avoid potential exploits
	GDisallowNetworkTravel = true;
#endif // UE_BUILD_SHIPPING

	// First, look for skeletal mesh show commands

	bool bUpdateSkelMeshCompDebugFlags = false;
	static bool bShowPrePhysSkelBones = false;

	if(FParse::Command(&Cmd,TEXT("PREPHYSBONES")))
	{
		bShowPrePhysSkelBones = !bShowPrePhysSkelBones;
		bUpdateSkelMeshCompDebugFlags = true;
	}

	// If we changed one of the skel mesh debug show flags, set it on each of the components in the World.
	if(bUpdateSkelMeshCompDebugFlags)
	{
		for (TObjectIterator<USkeletalMeshComponent> It; It; ++It)
		{
			USkeletalMeshComponent* SkelComp = *It;
			if( SkelComp->GetScene() == InWorld->Scene )
			{
				SkelComp->bShowPrePhysBones = bShowPrePhysSkelBones;
				SkelComp->MarkRenderStateDirty();
			}
		}

		// Now we are done.
		return true;
	}

	// EngineShowFlags
	{
		int32 FlagIndex = FEngineShowFlags::FindIndexByName(Cmd);

		if(FlagIndex != -1)
		{
			bool bCanBeToggled = true;

			if(GIsEditor)
			{
				if(!FEngineShowFlags::CanBeToggledInEditor(Cmd))
				{
					bCanBeToggled = false;
				}
			}

			bool bIsACollisionFlag = FEngineShowFlags::IsNameThere(Cmd, TEXT("Collision"));

			if(bCanBeToggled)
			{
				bool bOldState = EngineShowFlags.GetSingleFlag(FlagIndex);

				EngineShowFlags.SetSingleFlag(FlagIndex, !bOldState);

				if(FEngineShowFlags::IsNameThere(Cmd, TEXT("Navigation,Cover")))
				{
					VerifyPathRenderingComponents();
				}
					
				if(FEngineShowFlags::IsNameThere(Cmd, TEXT("Volumes")))
				{
					// TODO: Investigate why this is doesn't appear to work
					if (AllowDebugViewmodes())
					{
						ToggleShowVolumes();
					}
					else
					{
						Ar.Logf(TEXT("Debug viewmodes not allowed on consoles by default.  See AllowDebugViewmodes()."));
					}
				}
			}

			if(bIsACollisionFlag)
			{
				ToggleShowCollision();
			}

			return true;
		}
	}

	// create a sorted list of showflags
	TSet<FString> LinesToSort;
	{
		struct FIterSink
		{
			FIterSink(TSet<FString>& InLinesToSort, const FEngineShowFlags InEngineShowFlags) : LinesToSort(InLinesToSort), EngineShowFlags(InEngineShowFlags)
			{
			}

			bool OnEngineShowFlag(uint32 InIndex, const FString& InName)
			{
				FString Value = FString::Printf(TEXT("%s=%d"), *InName, EngineShowFlags.GetSingleFlag(InIndex) ? 1 : 0);
				LinesToSort.Add(Value);
				return true;
			}

			TSet<FString>& LinesToSort;
			const FEngineShowFlags EngineShowFlags;
		};

		FIterSink Sink(LinesToSort, EngineShowFlags);

		FEngineShowFlags::IterateAllFlags(Sink);
	}	

	LinesToSort.Sort( TLess<FString>() );

	for(TSet<FString>::TConstIterator It(LinesToSort); It; ++It)
	{
		const FString Value = *It;

		Ar.Logf(TEXT("%s"), *Value);
	}

	return true;
}

FPopupMethodReply UGameViewportClient::OnQueryPopupMethod() const
{
	return FPopupMethodReply::UseMethod(EPopupMethod::UseCurrentWindow)
		.SetShouldThrottle(EShouldThrottle::No);
}

bool UGameViewportClient::HandleNavigation(const uint32 InUserIndex, TSharedPtr<SWidget> InDestination)
{
	if (CustomNavigationEvent.IsBound())
	{
		return CustomNavigationEvent.Execute(InUserIndex, InDestination);
	}
	return false;
}

void UGameViewportClient::ToggleShowVolumes()
{
	// Don't allow 'show collision' and 'show volumes' at the same time, so turn collision off
	if (EngineShowFlags.Volumes && EngineShowFlags.Collision)
	{
		EngineShowFlags.SetCollision(false);
		ToggleShowCollision();
	}

	// Iterate over all brushes
	for (TObjectIterator<UBrushComponent> It; It; ++It)
	{
		UBrushComponent* BrushComponent = *It;
		AVolume* Owner = Cast<AVolume>(BrushComponent->GetOwner());

		// Only bother with volume brushes that belong to the world's scene
		if (Owner && BrushComponent->GetScene() == GetWorld()->Scene && !FActorEditorUtils::IsABuilderBrush(Owner))
		{
			// We're expecting this to be in the game at this point
			check(Owner->GetWorld()->IsGameWorld());

			// Toggle visibility of this volume
			if (BrushComponent->IsVisible())
			{
				BrushComponent->SetVisibility(false);
				BrushComponent->SetHiddenInGame(true);
			}
			else
			{
				BrushComponent->SetVisibility(true);
				BrushComponent->SetHiddenInGame(false);
			}
		}
	}
}

void UGameViewportClient::ToggleShowCollision()
{
	// special case: for the Engine.Collision flag, we need to un-hide any primitive components that collide so their collision geometry gets rendered
	const bool bIsShowingCollision = EngineShowFlags.Collision;

	if (bIsShowingCollision)
	{
		// Don't allow 'show collision' and 'show volumes' at the same time, so turn collision off
		if (EngineShowFlags.Volumes)
		{
			EngineShowFlags.SetVolumes(false);
			ToggleShowVolumes();
		}
	}

#if !UE_BUILD_SHIPPING
	if (World != nullptr)
	{
		// Tell engine to create proxies for hidden components, so we can still draw collision
		World->bCreateRenderStateForHiddenComponents = bIsShowingCollision;

		// Need to recreate scene proxies when this flag changes.
		FGlobalComponentRecreateRenderStateContext Recreate;
	}
#endif // !UE_BUILD_SHIPPING


#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (EngineShowFlags.Collision)
	{
		for (FLocalPlayerIterator It((UEngine*)GetOuter(), World); It; ++It)
		{
			APlayerController* PC = It->PlayerController;
			if (PC != NULL && PC->GetPawn() != NULL)
			{
				PC->ClientMessage(FString::Printf(TEXT("!!!! Player Pawn %s Collision Info !!!!"), *PC->GetPawn()->GetName()));
				if (PC->GetPawn()->GetMovementBase())
				{
					PC->ClientMessage(FString::Printf(TEXT("Base %s"), *PC->GetPawn()->GetMovementBase()->GetName()));
				}
				TSet<AActor*> TouchingActors;
				PC->GetPawn()->GetOverlappingActors(TouchingActors);
				int32 i = 0;
				for (AActor* TouchingActor : TouchingActors)
				{
					PC->ClientMessage(FString::Printf(TEXT("Touching %d: %s"), i++, *TouchingActor->GetName()));
				}
			}
		}
	}
#endif
}

bool UGameViewportClient::HandleShowLayerCommand( const TCHAR* Cmd, FOutputDevice& Ar, UWorld* InWorld )
{
	FString LayerName = FParse::Token(Cmd, 0);
	bool bPrintValidEntries = false;

	if (LayerName.IsEmpty())
	{
		Ar.Logf(TEXT("Missing layer name."));
		bPrintValidEntries = true;
	}
	else
	{
		int32 NumActorsToggled = 0;
		FName LayerFName = FName(*LayerName);

		for (FActorIterator It(InWorld); It; ++It)
		{
			AActor* Actor = *It;
			
			if (Actor->Layers.Contains(LayerFName))
			{
				NumActorsToggled++;
				// Note: overriding existing hidden property, ideally this would be something orthogonal
				Actor->bHidden = !Actor->bHidden;

				Actor->MarkComponentsRenderStateDirty();
			}
		}

		Ar.Logf(TEXT("Toggled visibility of %u actors"), NumActorsToggled);
		bPrintValidEntries = NumActorsToggled == 0;
	}

	if (bPrintValidEntries)
	{
		TArray<FName> LayerNames;

		for (FActorIterator It(InWorld); It; ++It)
		{
			AActor* Actor = *It;

			for (int32 LayerIndex = 0; LayerIndex < Actor->Layers.Num(); LayerIndex++)
			{
				LayerNames.AddUnique(Actor->Layers[LayerIndex]);
			}
		}

		Ar.Logf(TEXT("Valid layer names:"));

		for (int32 LayerIndex = 0; LayerIndex < LayerNames.Num(); LayerIndex++)
		{
			Ar.Logf(TEXT("   %s"), *LayerNames[LayerIndex].ToString());
		}
	}

	return true;
}

bool UGameViewportClient::HandleViewModeCommand( const TCHAR* Cmd, FOutputDevice& Ar, UWorld* InWorld )
{
#if !UE_BUILD_DEBUG
	// If there isn't a cheat manager, exit out
	bool bCheatsEnabled = false;
	for (FLocalPlayerIterator It((UEngine*)GetOuter(), InWorld); It; ++It)
	{
		if (It->PlayerController != NULL && It->PlayerController->CheatManager != NULL)
		{
			bCheatsEnabled = true;
			break;
		}
	}
	if (!bCheatsEnabled)
	{
		return true;
	}
#endif
	FString ViewModeName = FParse::Token(Cmd, 0);

	if(!ViewModeName.IsEmpty())
	{
		uint32 i = 0;
		for(; i < VMI_Max; ++i)
		{
			if(ViewModeName == GetViewModeName((EViewModeIndex)i))
			{
				ViewModeIndex = i;
				Ar.Logf(TEXT("Set new viewmode: %s"), GetViewModeName((EViewModeIndex)ViewModeIndex));
				break;
			}
		}
		if(i == VMI_Max)
		{
			Ar.Logf(TEXT("Error: view mode not recognized: %s"), *ViewModeName);
		}
	}
	else
	{
		Ar.Logf(TEXT("Current view mode: %s"), GetViewModeName((EViewModeIndex)ViewModeIndex));

		FString ViewModes;
		for(uint32 i = 0; i < VMI_Max; ++i)
		{
			if(i != 0)
			{
				ViewModes += TEXT(", ");
			}
			ViewModes += GetViewModeName((EViewModeIndex)i);
		}
		Ar.Logf(TEXT("Available view modes: %s"), *ViewModes);
	}

	if (ViewModeIndex == VMI_StationaryLightOverlap)
	{
		Ar.Logf(TEXT("This view mode is currently not supported in game."));
		ViewModeIndex = VMI_Lit;
	}

	if (FPlatformProperties::SupportsWindowedMode() == false)
	{
		if(ViewModeIndex == VMI_Unlit			
			|| ViewModeIndex == VMI_StationaryLightOverlap
			|| ViewModeIndex == VMI_Lit_DetailLighting
			|| ViewModeIndex == VMI_ReflectionOverride)
		{
			Ar.Logf(TEXT("This view mode is currently not supported on consoles."));
			ViewModeIndex = VMI_Lit;
		}
	}
	if ((ViewModeIndex != VMI_Lit && ViewModeIndex != VMI_ShaderComplexity) && !AllowDebugViewmodes())
	{
		Ar.Logf(TEXT("Debug viewmodes not allowed on consoles by default.  See AllowDebugViewmodes()."));
		ViewModeIndex = VMI_Lit;
	}

	ApplyViewMode((EViewModeIndex)ViewModeIndex, true, EngineShowFlags);

	return true;
}

bool UGameViewportClient::HandleNextViewModeCommand( const TCHAR* Cmd, FOutputDevice& Ar, UWorld* InWorld )
{
#if !UE_BUILD_DEBUG
	// If there isn't a cheat manager, exit out
	bool bCheatsEnabled = false;
	for (FLocalPlayerIterator It((UEngine*)GetOuter(), InWorld); It; ++It)
	{
		if (It->PlayerController != NULL && It->PlayerController->CheatManager != NULL)
		{
			bCheatsEnabled = true;
			break;
		}
	}
	if (!bCheatsEnabled)
	{
		return true;
	}
#endif
	ViewModeIndex = ViewModeIndex + 1;

	// wrap around
	if(ViewModeIndex == VMI_Max)
	{
		ViewModeIndex = 0;
	}

	Ar.Logf(TEXT("New view mode: %s"), GetViewModeName((EViewModeIndex)ViewModeIndex));
	ApplyViewMode((EViewModeIndex)ViewModeIndex, true, EngineShowFlags);
	return true;
}

bool UGameViewportClient::HandlePrevViewModeCommand( const TCHAR* Cmd, FOutputDevice& Ar, UWorld* InWorld )
{
#if !UE_BUILD_DEBUG
	// If there isn't a cheat manager, exit out
	bool bCheatsEnabled = false;
	for (FLocalPlayerIterator It((UEngine*)GetOuter(), InWorld); It; ++It)
	{
		if (It->PlayerController != NULL && It->PlayerController->CheatManager != NULL)
		{
			bCheatsEnabled = true;
			break;
		}
	}
	if (!bCheatsEnabled)
	{
		return true;
	}
#endif
	ViewModeIndex = ViewModeIndex - 1;

	// wrap around
	if(ViewModeIndex < 0)
	{
		ViewModeIndex = VMI_Max - 1;
	}

	Ar.Logf(TEXT("New view mode: %s"), GetViewModeName((EViewModeIndex)ViewModeIndex));
	ApplyViewMode((EViewModeIndex)ViewModeIndex, true, EngineShowFlags);
	return true;
}

bool UGameViewportClient::HandlePreCacheCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	Precache();
	return true;
}

bool UGameViewportClient::SetDisplayConfiguration(const FIntPoint* Dimensions, EWindowMode::Type WindowMode)
{
	if (Viewport == NULL || ViewportFrame == NULL)
	{
		return true;
	}

	UGameEngine* GameEngine = Cast<UGameEngine>(GEngine);
	
	if (GameEngine)
	{
		UGameUserSettings* UserSettings = GameEngine->GetGameUserSettings();

		UserSettings->SetFullscreenMode(WindowMode);

		if (Dimensions)
		{
			UserSettings->SetScreenResolution(*Dimensions);
		}

		UserSettings->ApplySettings(false);
	}
	else
	{
		int32 NewX = GSystemResolution.ResX;
		int32 NewY = GSystemResolution.ResY;

		if (Dimensions)
		{
			NewX = Dimensions->X;
			NewY = Dimensions->Y;
		}
	
		FSystemResolution::RequestResolutionChange(NewX, NewY, WindowMode);
	}

	return true;
}

bool UGameViewportClient::HandleToggleFullscreenCommand()
{
	static auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.FullScreenMode"));
	check(CVar);
	auto FullScreenMode = CVar->GetValueOnGameThread() == 0 ? EWindowMode::Fullscreen : EWindowMode::WindowedFullscreen;
	FullScreenMode = Viewport->IsFullscreen() ? EWindowMode::Windowed : FullScreenMode;

	if (PLATFORM_WINDOWS && FullScreenMode == EWindowMode::Fullscreen)
	{
		// Handle fullscreen mode differently for D3D11/D3D12
		static const bool bD3D12 = FParse::Param(FCommandLine::Get(), TEXT("d3d12")) || FParse::Param(FCommandLine::Get(), TEXT("dx12"));
		if (bD3D12)
		{
			// Force D3D12 RHI to use windowed fullscreen mode
			FullScreenMode = EWindowMode::WindowedFullscreen;
		}
	}

	int32 ResolutionX = GSystemResolution.ResX;
	int32 ResolutionY = GSystemResolution.ResY;

	// Make sure the user's settings are updated after pressing Alt+Enter to toggle fullscreen.  Note
	// that we don't need to "apply" the setting change, as we already did that above directly.
	UGameEngine* GameEngine = Cast<UGameEngine>( GEngine );
	if( GameEngine )
	{
		UGameUserSettings* UserSettings = GameEngine->GetGameUserSettings();
		if( UserSettings != nullptr )
		{
			// Ensure that our desired screen size will fit on the display
			ResolutionX = UserSettings->GetScreenResolution().X;
			ResolutionY = UserSettings->GetScreenResolution().Y;
			UGameEngine::DetermineGameWindowResolution(ResolutionX, ResolutionY, FullScreenMode);

			UserSettings->SetFullscreenMode(FullScreenMode);
			UserSettings->ConfirmVideoMode();
		}
	}

	FSystemResolution::RequestResolutionChange(ResolutionX, ResolutionY, FullScreenMode);

	ToggleFullscreenDelegate.Broadcast(FullScreenMode != EWindowMode::Windowed);

	return true;
}

bool UGameViewportClient::HandleSetResCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	if(Viewport && ViewportFrame)
	{
		int32 X=FCString::Atoi(Cmd);
		const TCHAR* CmdTemp = FCString::Strchr(Cmd,'x') ? FCString::Strchr(Cmd,'x')+1 : FCString::Strchr(Cmd,'X') ? FCString::Strchr(Cmd,'X')+1 : TEXT("");
		int32 Y=FCString::Atoi(CmdTemp);
		Cmd = CmdTemp;
		EWindowMode::Type WindowMode = Viewport->GetWindowMode();

		if(FCString::Strchr(Cmd,'w') || FCString::Strchr(Cmd,'W'))
		{
			if(FCString::Strchr(Cmd, 'f') || FCString::Strchr(Cmd, 'F'))
			{
				WindowMode = EWindowMode::WindowedFullscreen;
			}
			else
			{
				WindowMode = EWindowMode::Windowed;
			}
			
		}
		else if(FCString::Strchr(Cmd,'f') || FCString::Strchr(Cmd,'F'))
		{
			WindowMode = EWindowMode::Fullscreen;
		}
		if( X && Y )
		{
			FSystemResolution::RequestResolutionChange(X, Y, WindowMode);
		}
	}
	return true;
}

bool UGameViewportClient::HandleHighresScreenshotCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	if(Viewport)
	{
		if (GetHighResScreenshotConfig().ParseConsoleCommand(Cmd, Ar))
		{
			Viewport->TakeHighResScreenShot();
		}
	}
	return true;
}

bool UGameViewportClient::HandleHighresScreenshotUICommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	// Open the highres screenshot UI. When the capture region editing works properly, we can pass CaptureRegionWidget through
	// HighResScreenshotDialog = SHighResScreenshotDialog::OpenDialog(GetWorld(), Viewport, NULL /*CaptureRegionWidget*/);
	// Disabled until mouse specification UI can be used correctly
	return true;
}


bool UGameViewportClient::HandleScreenshotCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	if(Viewport)
	{
		const bool bShowUI = FParse::Command(&Cmd, TEXT("SHOWUI"));
		const bool bAddFilenameSuffix = true;
		FScreenshotRequest::RequestScreenshot( FString(), bShowUI, bAddFilenameSuffix );

		GScreenshotResolutionX = Viewport->GetSizeXY().X;
		GScreenshotResolutionY = Viewport->GetSizeXY().Y;
	}
	return true;
}

bool UGameViewportClient::HandleBugScreenshotwithHUDInfoCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	return RequestBugScreenShot(Cmd, true);
}

bool UGameViewportClient::HandleBugScreenshotCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	return RequestBugScreenShot(Cmd, false);
}

bool UGameViewportClient::HandleKillParticlesCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	// Don't kill in the Editor to avoid potential content clobbering.
	if( !GIsEditor )
	{
		extern bool GIsAllowingParticles;
		// Deactivate system and kill existing particles.
		for( TObjectIterator<UParticleSystemComponent> It; It; ++It )
		{
			UParticleSystemComponent* ParticleSystemComponent = *It;
			ParticleSystemComponent->DeactivateSystem();
			ParticleSystemComponent->KillParticlesForced();
		}
		// No longer initialize particles from here on out.
		GIsAllowingParticles = false;
	}
	return true;
}

bool UGameViewportClient::HandleForceSkelLODCommand( const TCHAR* Cmd, FOutputDevice& Ar, UWorld* InWorld )
{
	int32 ForceLod = 0;
	if(FParse::Value(Cmd,TEXT("LOD="),ForceLod))
	{
		ForceLod++;
	}

	for (TObjectIterator<USkeletalMeshComponent> It; It; ++It)
	{
		USkeletalMeshComponent* SkelComp = *It;
		if( SkelComp->GetScene() == InWorld->Scene && !SkelComp->IsTemplate())
		{
			SkelComp->ForcedLodModel = ForceLod;
		}
	}
	return true;
}

bool UGameViewportClient::HandleDisplayCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	TCHAR ObjectName[256];
	TCHAR PropStr[256];
	if ( FParse::Token(Cmd, ObjectName, ARRAY_COUNT(ObjectName), true) &&
		FParse::Token(Cmd, PropStr, ARRAY_COUNT(PropStr), true) )
	{
		UObject* Obj = FindObject<UObject>(ANY_PACKAGE, ObjectName);
		if (Obj != NULL)
		{
			FName PropertyName(PropStr, FNAME_Find);
			if (PropertyName != NAME_None && FindField<UProperty>(Obj->GetClass(), PropertyName) != NULL)
			{
				FDebugDisplayProperty& NewProp = DebugProperties[DebugProperties.AddZeroed()];
				NewProp.Obj = Obj;
				NewProp.PropertyName = PropertyName;
			}
			else
			{
				Ar.Logf(TEXT("Property '%s' not found on object '%s'"), PropStr, *Obj->GetName());
			}
		}
		else
		{
			Ar.Logf(TEXT("Object not found"));
		}
	}

	return true;
}

bool UGameViewportClient::HandleDisplayAllCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	TCHAR ClassName[256];
	TCHAR PropStr[256];
	if (FParse::Token(Cmd, ClassName, ARRAY_COUNT(ClassName), true))
	{
		bool bValidClassToken = true;
		UClass* WithinClass = NULL;
		{
			FString ClassStr(ClassName);
			int32 DotIndex = ClassStr.Find(TEXT("."));
			if (DotIndex != INDEX_NONE)
			{
				// first part is within class
				WithinClass = FindObject<UClass>(ANY_PACKAGE, *ClassStr.Left(DotIndex));
				if (WithinClass == NULL)
				{
					Ar.Logf(TEXT("Within class not found"));
					bValidClassToken = false;
				}
				else
				{
					FCString::Strncpy(ClassName, *ClassStr.Right(ClassStr.Len() - DotIndex - 1), 256);
					bValidClassToken = FCString::Strlen(ClassName) > 0;
				}
			}
		}
		if (bValidClassToken)
		{
			FParse::Token(Cmd, PropStr, ARRAY_COUNT(PropStr), true);
			UClass* Cls = FindObject<UClass>(ANY_PACKAGE, ClassName);
			if (Cls != NULL)
			{
				FName PropertyName(PropStr, FNAME_Find);
				UProperty* Prop = PropertyName != NAME_None ? FindField<UProperty>(Cls, PropertyName) : NULL;
				{
					// add all un-GCable things immediately as that list is static
					// so then we only have to iterate over dynamic things each frame
					for (TObjectIterator<UObject> It; It; ++It)
					{
						if (!GUObjectArray.IsDisregardForGC(*It))
						{
							break;
						}
						else if (It->IsA(Cls) && !It->IsTemplate() && (WithinClass == NULL || (It->GetOuter() != NULL && It->GetOuter()->GetClass()->IsChildOf(WithinClass))))
						{
							FDebugDisplayProperty& NewProp = DebugProperties[DebugProperties.AddZeroed()];
							NewProp.Obj = *It;
							NewProp.PropertyName = PropertyName;
							if (!Prop)
							{
								NewProp.bSpecialProperty = true;
							}
						}
					}
					FDebugDisplayProperty& NewProp = DebugProperties[DebugProperties.AddZeroed()];
					NewProp.Obj = Cls;
					NewProp.WithinClass = WithinClass;
					NewProp.PropertyName = PropertyName;
					if (!Prop)
					{
						NewProp.bSpecialProperty = true;
					}
				}
			}
			else
			{
				Ar.Logf(TEXT("Object not found"));
			}
		}
	}

	return true;
}

bool UGameViewportClient::HandleDisplayAllLocationCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	TCHAR ClassName[256];
	if (FParse::Token(Cmd, ClassName, ARRAY_COUNT(ClassName), true))
	{
		UClass* Cls = FindObject<UClass>(ANY_PACKAGE, ClassName);
		if (Cls != NULL)
		{
			// add all un-GCable things immediately as that list is static
			// so then we only have to iterate over dynamic things each frame
			for (TObjectIterator<UObject> It(true); It; ++It)
			{
				if (!GUObjectArray.IsDisregardForGC(*It))
				{
					break;
				}
				else if (It->IsA(Cls))
				{
					FDebugDisplayProperty& NewProp = DebugProperties[DebugProperties.AddZeroed()];
					NewProp.Obj = *It;
					NewProp.PropertyName = NAME_Location;
					NewProp.bSpecialProperty = true;
				}
			}
			FDebugDisplayProperty& NewProp = DebugProperties[DebugProperties.AddZeroed()];
			NewProp.Obj = Cls;
			NewProp.PropertyName = NAME_Location;
			NewProp.bSpecialProperty = true;
		}
		else
		{
			Ar.Logf(TEXT("Object not found"));
		}
	}

	return true;
}

bool UGameViewportClient::HandleDisplayAllRotationCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	TCHAR ClassName[256];
	if (FParse::Token(Cmd, ClassName, ARRAY_COUNT(ClassName), true))
	{
		UClass* Cls = FindObject<UClass>(ANY_PACKAGE, ClassName);
		if (Cls != NULL)
		{
			// add all un-GCable things immediately as that list is static
			// so then we only have to iterate over dynamic things each frame
			for (TObjectIterator<UObject> It(true); It; ++It)
			{
				if (!GUObjectArray.IsDisregardForGC(*It))
				{
					break;
				}
				else if (It->IsA(Cls))
				{
					FDebugDisplayProperty& NewProp = DebugProperties[DebugProperties.AddZeroed()];
					NewProp.Obj = *It;
					NewProp.PropertyName = NAME_Rotation;
					NewProp.bSpecialProperty = true;
				}
			}
			FDebugDisplayProperty& NewProp = DebugProperties[DebugProperties.AddZeroed()];
			NewProp.Obj = Cls;
			NewProp.PropertyName = NAME_Rotation;
			NewProp.bSpecialProperty = true;
		}
		else
		{
			Ar.Logf(TEXT("Object not found"));
		}
	}

	return true;
}

bool UGameViewportClient::HandleDisplayClearCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	DebugProperties.Empty();

	return true;
}

bool UGameViewportClient::HandleTextureDefragCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	extern void appDefragmentTexturePool();
	appDefragmentTexturePool();
	return true;
}

bool UGameViewportClient::HandleToggleMIPFadeCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	GEnableMipLevelFading = (GEnableMipLevelFading >= 0.0f) ? -1.0f : 1.0f;
	Ar.Logf(TEXT("Mip-fading is now: %s"), (GEnableMipLevelFading >= 0.0f) ? TEXT("ENABLED") : TEXT("DISABLED"));
	return true;
}

bool UGameViewportClient::HandlePauseRenderClockCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	GPauseRenderingRealtimeClock = !GPauseRenderingRealtimeClock;
	Ar.Logf(TEXT("The global realtime rendering clock is now: %s"), GPauseRenderingRealtimeClock ? TEXT("PAUSED") : TEXT("RUNNING"));
	return true;
}


bool UGameViewportClient::RequestBugScreenShot(const TCHAR* Cmd, bool bDisplayHUDInfo)
{
	// Path/name is the first (and only supported) argument
	FString FileName = Cmd;

	// Handle just a plain console command (e.g. "BUGSCREENSHOT").
	if (FileName.Len() == 0)
	{
		FileName = TEXT("BugScreenShot.png");
	}

	// Handle a console command and name (e.g. BUGSCREENSHOT FOO)
	if (FileName.Contains(TEXT("/")) == false)
	{
		// Path will be <gamename>/bugit/<platform>/desc_
		const FString BaseFile = FString::Printf(TEXT("%s%s_"), *FPaths::BugItDir(), *FPaths::GetBaseFilename(FileName));

		// find the next filename in the sequence, e.g <gamename>/bugit/<platform>/desc_00000.png
		FFileHelper::GenerateNextBitmapFilename(BaseFile, TEXT("png"), FileName);
	}

	if (Viewport != NULL)
	{
		UWorld* const ViewportWorld = GetWorld();
		if (bDisplayHUDInfo && (ViewportWorld != nullptr))
		{
			for (FConstPlayerControllerIterator Iterator = ViewportWorld->GetPlayerControllerIterator(); Iterator; ++Iterator)
			{
						APlayerController* PlayerController = Iterator->Get();
				if (PlayerController && PlayerController->GetHUD())
				{
					PlayerController->GetHUD()->HandleBugScreenShot();
				}
			}
		}

		const bool bShowUI = true;
		const bool bAddFilenameSuffix = false;
		FScreenshotRequest::RequestScreenshot(FileName, true, bAddFilenameSuffix);
	}

	return true;
}

void UGameViewportClient::HandleViewportStatCheckEnabled(const TCHAR* InName, bool& bOutCurrentEnabled, bool& bOutOthersEnabled)
{
	// Check to see which viewports have this enabled (current, non-current)
	const bool bEnabled = IsStatEnabled(InName);
	if (GStatProcessingViewportClient == this && GEngine->GameViewport == this)
	{
		bOutCurrentEnabled = bEnabled;
	}
	else
	{
		bOutOthersEnabled |= bEnabled;
	}
}

void UGameViewportClient::HandleViewportStatEnabled(const TCHAR* InName)
{
	// Just enable this on the active viewport
	if (GStatProcessingViewportClient == this && GEngine->GameViewport == this)
	{
		SetStatEnabled(InName, true);
	}
}

void UGameViewportClient::HandleViewportStatDisabled(const TCHAR* InName)
{
	// Just disable this on the active viewport
	if (GStatProcessingViewportClient == this && GEngine->GameViewport == this)
	{
		SetStatEnabled(InName, false);
	}
}

void UGameViewportClient::HandleViewportStatDisableAll(const bool bInAnyViewport)
{
	// Disable all on either all or the current viewport (depending on the flag)
	if (bInAnyViewport || (GStatProcessingViewportClient == this && GEngine->GameViewport == this))
	{
		SetStatEnabled(NULL, false, true);
	}
}

void UGameViewportClient::HandleWindowDPIScaleChanged(TSharedRef<SWindow> InWindow)
{
#if WITH_EDITOR
	if (InWindow == Window)
	{
		RequestUpdateEditorScreenPercentage();
	}
#endif
}

bool UGameViewportClient::SetHardwareCursor(EMouseCursor::Type CursorShape, FName GameContentPath, FVector2D HotSpot)
{
	TSharedPtr<FHardwareCursor> HardwareCursor = HardwareCursorCache.FindRef(GameContentPath);
	if ( HardwareCursor.IsValid() == false )
	{
		HardwareCursor = MakeShared<FHardwareCursor>(FPaths::ProjectContentDir() / GameContentPath.ToString(), HotSpot);
		if ( HardwareCursor->GetHandle() == nullptr )
		{
			return false;
		}

		HardwareCursorCache.Add(GameContentPath, HardwareCursor);
	}

	HardwareCursors.Add(CursorShape, HardwareCursor);

	if ( bIsMouseOverClient )
	{
		TSharedPtr<ICursor> PlatformCursor = FSlateApplication::Get().GetPlatformCursor();
		if ( ICursor* Cursor = PlatformCursor.Get() )
		{
			Cursor->SetTypeShape(CursorShape, HardwareCursor->GetHandle());
		}
	}
	
	return true;
}

bool UGameViewportClient::IsSimulateInEditorViewport() const
{
	const FSceneViewport* GameViewport = GetGameViewport();

	return GameViewport ? GameViewport->GetPlayInEditorIsSimulate() : false;
}

#undef LOCTEXT_NAMESPACE
