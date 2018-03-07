// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Layout/Visibility.h"
#include "LevelEditorPlaySettings.generated.h"

class SWindow;

/**
 * Enumerates label anchor modes.
 */
UENUM()
enum ELabelAnchorMode
{
	LabelAnchorMode_TopLeft UMETA(DisplayName="Top Left"),
	LabelAnchorMode_TopCenter UMETA(DisplayName="Top Center"),
	LabelAnchorMode_TopRight UMETA(DisplayName="Top Right"),
	LabelAnchorMode_CenterLeft UMETA(DisplayName="Center Left"),
	LabelAnchorMode_Centered UMETA(DisplayName="Centered"),
	LabelAnchorMode_CenterRight UMETA(DisplayName="Center Right"),
	LabelAnchorMode_BottomLeft UMETA(DisplayName="Bottom Left"),
	LabelAnchorMode_BottomCenter UMETA(DisplayName="Bottom Center"),
	LabelAnchorMode_BottomRight UMETA(DisplayName="Bottom Right")
};


UENUM()
enum ELaunchModeType
{
	/** Runs the map on a specified device. */
	LaunchMode_OnDevice,
};


UENUM()
enum EPlayModeLocations
{
	/** Spawns the player at the current camera location. */
	PlayLocation_CurrentCameraLocation,

	/** Spawns the player from the default player start. */
	PlayLocation_DefaultPlayerStart,
};


UENUM()
enum EPlayModeType
{
	/** Runs from within the editor. */
	PlayMode_InViewPort = 0,

	/** Runs in a new window. */
	PlayMode_InEditorFloating,

	/** Runs a mobile preview in a new process. */
	PlayMode_InMobilePreview,

	/** Runs a mobile preview targeted to a particular device in a new process. */
	PlayMode_InTargetedMobilePreview,

	/** Runs a vulkan preview in a new process. */
	PlayMode_InVulkanPreview,

	/** Runs in a new process. */
	PlayMode_InNewProcess,

	/** Runs in VR. */
	PlayMode_InVR,

	/** Simulates in viewport without possessing the player. */
	PlayMode_Simulate,

	/** The number of different Play Modes. */
	PlayMode_Count,
};


UENUM()
enum EPlayNetMode
{
	PIE_Standalone UMETA(DisplayName="Play Offline"),
	PIE_ListenServer UMETA(DisplayName="Play As Listen Server"),
	PIE_Client UMETA(DisplayName="Play As Client"),
};


/**
 * Determines whether to build the executable when launching on device. Note the equivalence between these settings and EProjectPackagingBuild.
 */
UENUM()
enum EPlayOnBuildMode
{
	/** Always build. */
	PlayOnBuild_Always UMETA(DisplayName="Always"),

	/** Never build. */
	PlayOnBuild_Never UMETA(DisplayName="Never"),

	/** Build based on project type. */
	PlayOnBuild_Default UMETA(DisplayName="If project has code, or running a locally built editor"),

	/** Build if we're using a locally built (ie. non-promoted) editor. */
	PlayOnBuild_IfEditorBuiltLocally UMETA(DisplayName="If running a locally built editor"),
};

/* Configuration to use when launching on device. */
UENUM()
enum EPlayOnLaunchConfiguration
{
	/** Launch on device with the same build configuration as the editor. */
	LaunchConfig_Default UMETA(DisplayName = "Same as Editor"),
	/** Launch on device with a Debug build configuration. */
	LaunchConfig_Debug UMETA(DisplayName = "Debug"),
	/** Launch on device with a Development build configuration. */
	LaunchConfig_Development UMETA(DisplayName = "Development"),
	/** Launch on device with a Test build configuration. */
	LaunchConfig_Test UMETA(DisplayName = "Test"),
	/** Launch on device with a Shipping build configuration. */
	LaunchConfig_Shipping UMETA(DisplayName = "Shipping"),
};

/**
 * Holds information about a screen resolution to be used for playing.
 */
USTRUCT()
struct FPlayScreenResolution
{
	GENERATED_USTRUCT_BODY()

public:

	/** The description text for this screen resolution. */
	UPROPERTY(config)
	/*FText*/FString Description;

	/** The screen resolution's width (in pixels). */
	UPROPERTY(config)
	int32 Width;

	/** The screen resolution's height (in pixels). */
	UPROPERTY(config)
	int32 Height;

	/** The screen resolution's aspect ratio (as a string). */
	UPROPERTY(config)
	FString AspectRatio;
};

/**
 * Implements the Editor's play settings.
 */
UCLASS(config=EditorPerProjectUserSettings)
class UNREALED_API ULevelEditorPlaySettings
	: public UObject
{
	GENERATED_UCLASS_BODY()

public:

	/** The PlayerStart class used when spawning the player at the current camera location. */
	UPROPERTY(globalconfig)
	FString PlayFromHerePlayerStartClassName;

public:

	/** Should Play-in-Editor automatically give mouse control to the game on PIE start (default = false). Note that this does not affect VR, which will always take focus */
	UPROPERTY(config, EditAnywhere, Category=PlayInEditor, meta=(ToolTip="Give the game mouse control when PIE starts or require a click in the viewport first"))
	bool GameGetsMouseControl;

	/** Whether to show a label for mouse control gestures in the PIE view. */
	UPROPERTY(config, EditAnywhere, Category=PlayInEditor)
	bool ShowMouseControlLabel;

	/** Location on screen to anchor the mouse control label when in PIE mode. */
	UPROPERTY(config, EditAnywhere, Category=PlayInEditor)
	TEnumAsByte<ELabelAnchorMode> MouseControlLabelPosition;

	/** Should Play-in-Viewport respect HMD orientations (default = false) */
	UPROPERTY(config, EditAnywhere, Category=PlayInEditor, meta=(ToolTip="Whether or not HMD orientation should be used when playing in viewport"))
	bool ViewportGetsHMDControl;

	/** Should we minimize the editor when VR PIE is clicked (default = true) */
	UPROPERTY(config, EditAnywhere, Category = PlayInEditor, meta = (ToolTip = "Whether or not the editor is minimized on VR PIE"))
	bool ShouldMinimizeEditorOnVRPIE;

	/** Whether to automatically recompile blueprints on PIE */
	UPROPERTY(config, EditAnywhere, Category=PlayInEditor, meta=(ToolTip="Automatically recompile blueprints used by the current level when initiating a Play In Editor session"))
	bool AutoRecompileBlueprints;

	/** Whether to play sounds during PIE */
	UPROPERTY(config, EditAnywhere, Category=PlayInEditor, meta=(ToolTip="Whether to play sounds when in a Play In Editor session"))
	bool EnableGameSound;

	/** Whether to play a sound when entering and exiting PIE */
	UPROPERTY(config, EditAnywhere, AdvancedDisplay, Category = PlayInEditor, meta = (DisplayName = "Enable PIE Enter and Exit Sounds"))
	bool EnablePIEEnterAndExitSounds;

	/** Which quality level to use when playing in editor */
	UPROPERTY(config, EditAnywhere, Category=PlayInEditor)
	int32 PlayInEditorSoundQualityLevel;

	/** True if Play In Editor should only load currently-visible levels in PIE. */
	UPROPERTY(config)
	uint32 bOnlyLoadVisibleLevelsInPIE:1;

	UPROPERTY(config, EditAnywhere, Category = PlayInEditor, meta = (DisplayName="Stream Sub-Levels during Play in Editor", ToolTip="Prefer to stream sub-levels from the disk instead of duplicating editor sub-levels"))
	uint32 bPreferToStreamLevelsInPIE:1;

public:

	/** Whether to automatically bind any active level sequences to a PIE world */
	UPROPERTY(config, EditAnywhere, Category=Sequencer)
	uint32 bBindSequencerToPIE:1;

	/** Whether to automatically bind any active level sequences to a Simulate world */
	UPROPERTY(config, EditAnywhere, Category=Sequencer)
	uint32 bBindSequencerToSimulate:1;

public:

	/** Whether to always have the PIE window on top of the parent windows. */
	UPROPERTY(config, EditAnywhere, Category = PlayInNewWindow, meta = (ToolTip="Always have the PIE window on top of the parent windows."))
	bool PIEAlwaysOnTop;

	/** The width of the new view port window in pixels (0 = use the desktop's screen resolution). */
	UPROPERTY(config, EditAnywhere, Category=PlayInNewWindow)
	int32 NewWindowWidth;

	/** The height of the new view port window in pixels (0 = use the desktop's screen resolution). */
	UPROPERTY(config, EditAnywhere, Category=PlayInNewWindow)
	int32 NewWindowHeight;

	/** The position of the new view port window on the screen in pixels. */
	UPROPERTY(config, EditAnywhere, Category=PlayInNewWindow)
	FIntPoint NewWindowPosition;

	/** Whether the new window should be centered on the screen. */
	UPROPERTY(config, EditAnywhere, Category=PlayInNewWindow)
	bool CenterNewWindow;

public:

	/** The width of the standalone game window in pixels (0 = use the desktop's screen resolution). */
	UPROPERTY(config, EditAnywhere, Category=PlayInStandaloneGame)
	int32 StandaloneWindowWidth;

	/** The height of the standalone game window in pixels (0 = use the desktop's screen resolution). */
	UPROPERTY(config, EditAnywhere, Category=PlayInStandaloneGame)
	int32 StandaloneWindowHeight;

	/** The position of the standalone game window on the screen in pixels. */
	UPROPERTY(config, EditAnywhere, Category=PlayInStandaloneGame)
	FIntPoint StandaloneWindowPosition;

	/** Whether the standalone game window should be centered on the screen. */
	UPROPERTY(config, EditAnywhere, Category=PlayInStandaloneGame)
	bool CenterStandaloneWindow;

	/** Whether sound should be disabled when playing standalone games. */
	UPROPERTY(config , EditAnywhere, Category=PlayInStandaloneGame, AdvancedDisplay)
	uint32 DisableStandaloneSound:1;

	/** Extra parameters to be include as part of the command line for the standalone game. */
	UPROPERTY(config , EditAnywhere, Category=PlayInStandaloneGame, AdvancedDisplay)
	FString AdditionalLaunchParameters;

public:

	/** Whether to build the game before launching on device. */
	UPROPERTY(config, EditAnywhere, Category = PlayOnDevice)
	TEnumAsByte<EPlayOnBuildMode> BuildGameBeforeLaunch;

	/* Which build configuration to use when launching on device. */
	UPROPERTY(config, EditAnywhere, Category = PlayOnDevice)
	TEnumAsByte<EPlayOnLaunchConfiguration> LaunchConfiguration;

	/** Whether to automatically recompile dirty Blueprints before launching */
	UPROPERTY(config, EditAnywhere, Category=PlayOnDevice)
	bool bAutoCompileBlueprintsOnLaunch;

	/** A programmatically defined custom PIE window to use */
	TWeakPtr<SWindow> CustomPIEWindow;
	
private:

	/** NetMode to use for Play In Editor. */
	UPROPERTY(config, EditAnywhere, Category=MultiplayerOptions)
	TEnumAsByte<EPlayNetMode> PlayNetMode;

	/** Spawn multiple player windows in a single instance of UE4. This will load much faster, but has potential to have more issues.  */
	UPROPERTY(config, EditAnywhere, Category=MultiplayerOptions)
	bool RunUnderOneProcess;

	/** If checked, a separate dedicated server will be launched. Otherwise the first player will act as a listen server that all other players connect to. */
	UPROPERTY(config, EditAnywhere, Category=MultiplayerOptions)
	bool PlayNetDedicated;

	/** The editor and listen server count as players, a dedicated server will not. Clients make up the remainder. */
	UPROPERTY(config, EditAnywhere, Category=MultiplayerOptions, meta=(ClampMin = "1", UIMin = "1", UIMax = "64"))
	int32 PlayNumberOfClients;

	/** What port used by the server for simple networking */
	UPROPERTY(config, EditAnywhere, Category = MultiplayerOptions)
	uint16 ServerPort;

	/** Width to use when spawning additional windows. */
	UPROPERTY(config, EditAnywhere, Category=MultiplayerOptions)
	int32 ClientWindowWidth;
	
	/**
	 * When running multiple players or a dedicated server the client need to connect to the server, this option sets how they connect
	 *
	 * If this is checked, the clients will automatically connect to the launched server, if false they will launch into the map and wait
	 */
	UPROPERTY(config, EditAnywhere, Category=MultiplayerOptions)
	bool AutoConnectToServer;

	/**
	 * When running multiple player windows in a single process, this option determines how the game pad input gets routed.
	 *
	 * If unchecked (default) then the 1st game pad is attached to the 1st window, 2nd to the 2nd window, and so on.
	 *
	 * If it is checked, the 1st game pad goes the 2nd window. The 1st window can then be controlled by keyboard/mouse, which is convenient if two people are testing on the same computer.
	 */
	UPROPERTY(config, EditAnywhere, Category=MultiplayerOptions)
	bool RouteGamepadToSecondWindow;

	/** 
	* If checked, a separate audio device is created for every player. 
	
	* If unchecked, a separate audio device is created for only the first two players and uses the main audio device for more than 2 players.
	*
	* Enabling this will allow rendering accurate audio from every player's perspective but will use more CPU. Keep this disabled on lower-perf machines.
	*/
	UPROPERTY(config, EditAnywhere, Category = MultiplayerOptions, meta=(EditCondition = "EnableGameSound"))
	bool CreateAudioDeviceForEveryPlayer;

	/** Height to use when spawning additional windows. */
	UPROPERTY(config, EditAnywhere, Category=MultiplayerOptions)
	int32 ClientWindowHeight;

	/** Additional options that will be passed to the server as URL parameters, in the format ?bIsLanMatch=1?listen - any additional command line switches should be passed in the Command Line Arguments field below. */
	UPROPERTY(config, EditAnywhere, Category=MultiplayerOptions)
	FString AdditionalServerGameOptions;

	/** Additional command line options that will be passed to standalone game instances, for example -debug */
	UPROPERTY(config, EditAnywhere, Category=MultiplayerOptions)
	FString AdditionalLaunchOptions;

public:

	// Accessors for fetching the values of multiplayer options, and returning whether the option is valid at this time
	void SetPlayNetMode( const EPlayNetMode InPlayNetMode ) { PlayNetMode = InPlayNetMode; }
	bool IsPlayNetModeActive() const { return true; }
	bool GetPlayNetMode( EPlayNetMode &OutPlayNetMode ) const { OutPlayNetMode = PlayNetMode; return IsPlayNetModeActive(); }
	EVisibility GetPlayNetModeVisibility() const { return (RunUnderOneProcess ? EVisibility::Hidden : EVisibility::Visible); }

	void SetRunUnderOneProcess( const bool InRunUnderOneProcess ) { RunUnderOneProcess = InRunUnderOneProcess; }
	bool IsRunUnderOneProcessActive() const { return true; }
	bool GetRunUnderOneProcess( bool &OutRunUnderOneProcess ) const { OutRunUnderOneProcess = RunUnderOneProcess; return IsRunUnderOneProcessActive(); }
	
	void SetPlayNetDedicated( const bool InPlayNetDedicated ) { PlayNetDedicated = InPlayNetDedicated; }
	bool IsPlayNetDedicatedActive() const { return (RunUnderOneProcess ? true : PlayNetMode == PIE_Client); }
	bool GetPlayNetDedicated( bool &OutPlayNetDedicated ) const { OutPlayNetDedicated = PlayNetDedicated; return IsPlayNetDedicatedActive(); }

	void SetPlayNumberOfClients( const int32 InPlayNumberOfClients ) { PlayNumberOfClients = InPlayNumberOfClients; }
	bool IsPlayNumberOfClientsActive() const { return (PlayNetMode != PIE_Standalone) || RunUnderOneProcess; }
	bool GetPlayNumberOfClients( int32 &OutPlayNumberOfClients ) const { OutPlayNumberOfClients = PlayNumberOfClients; return IsPlayNumberOfClientsActive(); }

	void SetServerPort(const uint16 InServerPort) { ServerPort = InServerPort; }
	bool IsServerPortActive() const { return (PlayNetMode != PIE_Standalone) || RunUnderOneProcess; }
	bool GetServerPort(uint16 &OutServerPort) const { OutServerPort = ServerPort; return IsServerPortActive(); }
	
	bool IsAutoConnectToServerActive() const { return PlayNumberOfClients > 1 || PlayNetDedicated; }
	bool GetAutoConnectToServer(bool &OutAutoConnectToServer) const { OutAutoConnectToServer = AutoConnectToServer; return IsAutoConnectToServerActive(); }
	EVisibility GetAutoConnectToServerVisibility() const { return (RunUnderOneProcess ? EVisibility::Visible : EVisibility::Hidden); }

	bool IsRouteGamepadToSecondWindowActive() const { return PlayNumberOfClients > 1; }
	bool GetRouteGamepadToSecondWindow( bool &OutRouteGamepadToSecondWindow ) const { OutRouteGamepadToSecondWindow = RouteGamepadToSecondWindow; return IsRouteGamepadToSecondWindowActive(); }
	EVisibility GetRouteGamepadToSecondWindowVisibility() const { return (RunUnderOneProcess ? EVisibility::Visible : EVisibility::Hidden); }

	bool IsAdditionalServerGameOptionsActive() const { return (PlayNetMode != PIE_Standalone) || RunUnderOneProcess; }
	bool GetAdditionalServerGameOptions( FString &OutAdditionalServerGameOptions ) const { OutAdditionalServerGameOptions = AdditionalServerGameOptions; return IsAdditionalServerGameOptionsActive(); }

	bool IsAdditionalLaunchOptionsActive() const { return true; }
	bool GetAdditionalLaunchOptions( FString &OutAdditionalLaunchOptions ) const { OutAdditionalLaunchOptions = AdditionalLaunchOptions; return IsAdditionalLaunchOptionsActive(); }
	EVisibility GetAdditionalLaunchOptionsVisibility() const { return (RunUnderOneProcess ? EVisibility::Hidden : EVisibility::Visible); }
	
	void SetClientWindowSize( const FIntPoint InClientWindowSize ) { ClientWindowWidth = InClientWindowSize.X; ClientWindowHeight = InClientWindowSize.Y; }
	bool IsClientWindowSizeActive() const { return ((PlayNetMode == PIE_Standalone && RunUnderOneProcess) ? false : (PlayNumberOfClients >= 2)); }
	bool GetClientWindowSize( FIntPoint &OutClientWindowSize ) const { OutClientWindowSize = FIntPoint(ClientWindowWidth, ClientWindowHeight); return IsClientWindowSizeActive(); }
	EVisibility GetClientWindowSizeVisibility() const { return (RunUnderOneProcess ? EVisibility::Hidden : EVisibility::Visible); }
	bool IsCreateAudioDeviceForEveryPlayer() const { return CreateAudioDeviceForEveryPlayer; }

public:

	/** The last used height for multiple instance windows (in pixels). */
	UPROPERTY(config)
	int32 MultipleInstanceLastHeight;

	/** The last used width for multiple instance windows (in pixels). */
	UPROPERTY(config)
	int32 MultipleInstanceLastWidth;

	/** The last known screen positions of multiple instance windows (in pixels). */
	UPROPERTY(config)
	TArray<FIntPoint> MultipleInstancePositions;

public:

	/** The name of the last platform that the user ran a play session on. */
	UPROPERTY(config)
	FString LastExecutedLaunchDevice;

	/** The name of the last device that the user ran a play session on. */
	UPROPERTY(config)
	FString LastExecutedLaunchName;

	/** The last type of play-on session the user ran. */
	UPROPERTY(config)
	TEnumAsByte<ELaunchModeType> LastExecutedLaunchModeType;

	/** The last type of play location the user ran. */
	UPROPERTY(config)
	TEnumAsByte<EPlayModeLocations> LastExecutedPlayModeLocation;

	/** The last type of play session the user ran. */
	UPROPERTY(config)
	TEnumAsByte<EPlayModeType> LastExecutedPlayModeType;

	/** The name of the last device that the user ran a play session on. */
	UPROPERTY(config)
	FString LastExecutedPIEPreviewDevice;
public:

	/** Collection of common screen resolutions on mobile phones. */
	UPROPERTY(config)
	TArray<FPlayScreenResolution> LaptopScreenResolutions;

	/** Collection of common screen resolutions on desktop monitors. */
	UPROPERTY(config)
	TArray<FPlayScreenResolution> MonitorScreenResolutions;

	/** Collection of common screen resolutions on mobile phones. */
	UPROPERTY(config)
	TArray<FPlayScreenResolution> PhoneScreenResolutions;

	/** Collection of common screen resolutions on tablet devices. */
	UPROPERTY(config)
	TArray<FPlayScreenResolution> TabletScreenResolutions;

	/** Collection of common screen resolutions on television screens. */
	UPROPERTY(config)
	TArray<FPlayScreenResolution> TelevisionScreenResolutions;

	// UObject interface
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	// End of UObject interface
};
