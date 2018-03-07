// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "Containers/Map.h"
#include "Delegates/Delegate.h"
#include "Math/IntVector.h"
#include "GenericPlatform/GenericPlatformFile.h"

class AActor;
class Error;

// delegates for hotfixes
namespace EHotfixDelegates
{
	enum Type
	{
		Test,
	};
}


// this is an example of a hotfix arg and return value structure. Once we have other examples, it can be deleted.
struct FTestHotFixPayload
{
	FString Message;
	bool ValueToReturn;
	bool Result;
};

// Parameters passed to CrashOverrideParamsChanged used to customize crash report client behavior/appearance
struct FCrashOverrideParameters
{
	FString CrashReportClientMessageText;
};

class CORE_API FCoreDelegates
{
public:
	//hot fix delegate
	DECLARE_DELEGATE_TwoParams(FHotFixDelegate, void *, int32);

	// Callback for object property modifications
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnActorLabelChanged, AActor*);

	// delegate type for prompting the pak system to mount a new pak
	DECLARE_DELEGATE_RetVal_ThreeParams(bool, FOnMountPak, const FString&, uint32, IPlatformFile::FDirectoryVisitor*);

	// delegate type for prompting the pak system to mount a new pak
	DECLARE_DELEGATE_RetVal_OneParam(bool, FOnUnmountPak, const FString&);

	/** delegate type for opening a modal message box ( Params: EAppMsgType::Type MessageType, const FText& Text, const FText& Title ) */
	DECLARE_DELEGATE_RetVal_ThreeParams(EAppReturnType::Type, FOnModalMessageBox, EAppMsgType::Type, const FText&, const FText&);

	// Callback for handling an ensure
	DECLARE_MULTICAST_DELEGATE(FOnHandleSystemEnsure);

	// Callback for handling an error
	DECLARE_MULTICAST_DELEGATE(FOnHandleSystemError);

	// Callback for handling user login/logout.  first int is UserID, second int is UserIndex
	DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnUserLoginChangedEvent, bool, int32, int32);

	// Callback for handling safe frame area size changes
	DECLARE_MULTICAST_DELEGATE(FOnSafeFrameChangedEvent);

	// Callback for handling accepting invitations - generally for engine code
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnInviteAccepted, const FString&, const FString&);

	// Callback for accessing pak encryption key, if it exists
	DECLARE_DELEGATE_RetVal(const ANSICHAR*, FPakEncryptionKeyDelegate);

	// Callback for gathering pak signing keys, if they exist
	DECLARE_DELEGATE_TwoParams(FPakSigningKeysDelegate, FString&, FString&);

	// Callback for handling the Controller connection / disconnection
	// first param is true for a connection, false for a disconnection.
	// second param is UserID, third is UserIndex / ControllerId.
	DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnUserControllerConnectionChange, bool, FPlatformUserId, int32);

	// Callback for handling a Controller pairing change
	// first param is controller index
	// second param is NewUserPlatformId, third is OldUserPlatformId.
	DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnUserControllerPairingChange, int32 /*ControllerIndex*/, FPlatformUserId /*NewUserPlatformId*/, FPlatformUserId /*OldUserPlatformId*/);

	// Callback for platform handling when flushing async loads.
	DECLARE_MULTICAST_DELEGATE(FOnAsyncLoadingFlush);
	static FOnAsyncLoadingFlush OnAsyncLoadingFlush;

	// Callback for a game thread interruption point when a async load flushing. Used to updating UI during long loads.
	DECLARE_MULTICAST_DELEGATE(FOnAsyncLoadingFlushUpdate);
	static FOnAsyncLoadingFlushUpdate OnAsyncLoadingFlushUpdate;

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnAsyncLoadPackage, const FString&);
	static FOnAsyncLoadPackage OnAsyncLoadPackage;

	// get a hotfix delegate
	static FHotFixDelegate& GetHotfixDelegate(EHotfixDelegates::Type HotFix);

	// Callback when a user logs in/out of the platform.
	static FOnUserLoginChangedEvent OnUserLoginChangedEvent;

	// Callback when controllers disconnected / reconnected
	static FOnUserControllerConnectionChange OnControllerConnectionChange;

	// Callback when a single controller pairing changes
	static FOnUserControllerPairingChange OnControllerPairingChange;

	// Callback when a user changes the safe frame size
	static FOnSafeFrameChangedEvent OnSafeFrameChangedEvent;

	// Callback for mounting a new pak file.
	static FOnMountPak OnMountPak;

	// Callback for unmounting a pak file.
	static FOnUnmountPak OnUnmountPak;

	// Callback when an ensure has occurred
	static FOnHandleSystemEnsure OnHandleSystemEnsure;
	// Callback when an error (crash) has occurred
	static FOnHandleSystemError OnHandleSystemError;

	// Called when an actor label is changed
	static FOnActorLabelChanged OnActorLabelChanged;

	static FPakEncryptionKeyDelegate& GetPakEncryptionKeyDelegate();
	static FPakSigningKeysDelegate& GetPakSigningKeysDelegate();

#if WITH_EDITOR
	// Called before the editor displays a modal window, allowing other windows the opportunity to disable themselves to avoid reentrant calls
	static FSimpleMulticastDelegate PreModal;

	// Called after the editor dismisses a modal window, allowing other windows the opportunity to disable themselves to avoid reentrant calls
	static FSimpleMulticastDelegate PostModal;
    
    // Called before the editor displays a Slate (non-platform) modal window, allowing other windows the opportunity to disable themselves to avoid reentrant calls
    static FSimpleMulticastDelegate PreSlateModal;
    
    // Called after the editor dismisses a Slate (non-platform) modal window, allowing other windows the opportunity to disable themselves to avoid reentrant calls
    static FSimpleMulticastDelegate PostSlateModal;
    
#endif	//WITH_EDITOR
	
	// Called when an error occurred.
	static FSimpleMulticastDelegate OnShutdownAfterError;

	// Called when appInit is called, very early in startup
	static FSimpleMulticastDelegate OnInit;

	// Called at the end of UEngine::Init, right before loading PostEngineInit modules for both normal execution and commandlets
	static FSimpleMulticastDelegate OnPostEngineInit;

	// Called at the very end of engine initialization, right before the engine starts ticking. This is not called for commandlets
	static FSimpleMulticastDelegate OnFEngineLoopInitComplete;

	// Called when the application is about to exit.
	static FSimpleMulticastDelegate OnExit;

	// Called when before the application is exiting.
	static FSimpleMulticastDelegate OnPreExit;

	/** Color picker color has changed, please refresh as needed*/
	static FSimpleMulticastDelegate ColorPickerChanged;

	/** requests to open a message box */
	static FOnModalMessageBox ModalErrorMessage;

	/** Called when the user accepts an invitation to the current game */
	static FOnInviteAccepted OnInviteAccepted;

	// Called at the beginning of a frame
	static FSimpleMulticastDelegate OnBeginFrame;

	// Called at the end of a frame
	static FSimpleMulticastDelegate OnEndFrame;


	DECLARE_MULTICAST_DELEGATE_ThreeParams(FWorldOriginOffset, class UWorld*, FIntVector, FIntVector);
	/** called before world origin shifting */
	static FWorldOriginOffset PreWorldOriginOffset;
	/** called after world origin shifting */
	static FWorldOriginOffset PostWorldOriginOffset;

	/** called when the main loop would otherwise starve. */
	DECLARE_DELEGATE(FStarvedGameLoop);
	static FStarvedGameLoop StarvedGameLoop;

	/** IOS-style application lifecycle delegates */
	DECLARE_MULTICAST_DELEGATE(FApplicationLifetimeDelegate);

	// This is called when the application is about to be deactivated (e.g., due to a phone call or SMS or the sleep button).
	// The game should be paused if possible, etc...
	static FApplicationLifetimeDelegate ApplicationWillDeactivateDelegate;
	
	// Called when the application has been reactivated (reverse any processing done in the Deactivate delegate)
	static FApplicationLifetimeDelegate ApplicationHasReactivatedDelegate;

	// This is called when the application is being backgrounded (e.g., due to switching
	// to another app or closing it via the home button)
	// The game should release shared resources, save state, etc..., since it can be
	// terminated from the background state without any further warning.
	static FApplicationLifetimeDelegate ApplicationWillEnterBackgroundDelegate; // for instance, hitting the home button

	// Called when the application is returning to the foreground (reverse any processing done in the EnterBackground delegate)
	static FApplicationLifetimeDelegate ApplicationHasEnteredForegroundDelegate;

	// This *may* be called when the application is getting terminated by the OS.
	// There is no guarantee that this will ever be called on a mobile device,
	// save state when ApplicationWillEnterBackgroundDelegate is called instead.
	static FApplicationLifetimeDelegate ApplicationWillTerminateDelegate;

	/** IOS-style push notification delegates */
	DECLARE_MULTICAST_DELEGATE_OneParam(FApplicationRegisteredForRemoteNotificationsDelegate, TArray<uint8>);
	DECLARE_MULTICAST_DELEGATE_OneParam(FApplicationRegisteredForUserNotificationsDelegate, int);
	DECLARE_MULTICAST_DELEGATE_OneParam(FApplicationFailedToRegisterForRemoteNotificationsDelegate, FString);
	DECLARE_MULTICAST_DELEGATE_TwoParams(FApplicationReceivedRemoteNotificationDelegate, FString, int);
	DECLARE_MULTICAST_DELEGATE_ThreeParams(FApplicationReceivedLocalNotificationDelegate, FString, int, int);



	DECLARE_MULTICAST_DELEGATE_OneParam(FOnFConfigFileCreated, const FConfigFile *);
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnFConfigFileDeleted, const FConfigFile *);
	static FOnFConfigFileCreated OnFConfigCreated;
	static FOnFConfigFileDeleted OnFConfigDeleted;
#if WITH_EDITOR
	// called when a target platform changes it's return value of supported formats.  This is so anything caching those results can reset (like cached shaders for cooking)
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnTargetPlatformChangedSupportedFormats, const ITargetPlatform*); 
	static FOnTargetPlatformChangedSupportedFormats OnTargetPlatformChangedSupportedFormats;
#endif
	// called when the user grants permission to register for remote notifications
	static FApplicationRegisteredForRemoteNotificationsDelegate ApplicationRegisteredForRemoteNotificationsDelegate;

	// called when the user grants permission to register for notifications
	static FApplicationRegisteredForUserNotificationsDelegate ApplicationRegisteredForUserNotificationsDelegate;

	// called when the application fails to register for remote notifications
	static FApplicationFailedToRegisterForRemoteNotificationsDelegate ApplicationFailedToRegisterForRemoteNotificationsDelegate;

	// called when the application receives a remote notification
	static FApplicationReceivedRemoteNotificationDelegate ApplicationReceivedRemoteNotificationDelegate;

	// called when the application receives a local notification
	static FApplicationReceivedLocalNotificationDelegate ApplicationReceivedLocalNotificationDelegate;

	/** Sent when a device screen orientation changes */
	DECLARE_MULTICAST_DELEGATE_OneParam(FApplicationReceivedOnScreenOrientationChangedNotificationDelegate, int32);
	static FApplicationReceivedOnScreenOrientationChangedNotificationDelegate ApplicationReceivedScreenOrientationChangedNotificationDelegate;

	/** Checks to see if the stat is already enabled */
	DECLARE_MULTICAST_DELEGATE_ThreeParams(FStatCheckEnabled, const TCHAR*, bool&, bool&);
	static FStatCheckEnabled StatCheckEnabled;

	/** Sent after each stat is enabled */
	DECLARE_MULTICAST_DELEGATE_OneParam(FStatEnabled, const TCHAR*);
	static FStatEnabled StatEnabled;

	/** Sent after each stat is disabled */
	DECLARE_MULTICAST_DELEGATE_OneParam(FStatDisabled, const TCHAR*);
	static FStatDisabled StatDisabled;

	/** Sent when all stats need to be disabled */
	DECLARE_MULTICAST_DELEGATE_OneParam(FStatDisableAll, const bool);
	static FStatDisableAll StatDisableAll;

	// Called when an application is notified that the application license info has been updated.
	// The new license data should be polled and steps taken based on the results (i.e. halt application if license is no longer valid).
	DECLARE_MULTICAST_DELEGATE(FApplicationLicenseChange);
	static FApplicationLicenseChange ApplicationLicenseChange;

	/** Sent when the platform changed its laptop mode (for convertible laptops).*/
	DECLARE_MULTICAST_DELEGATE_OneParam(FPlatformChangedLaptopMode, EConvertibleLaptopMode);
	static FPlatformChangedLaptopMode PlatformChangedLaptopMode;

	/** Sent when the platform needs the user to fix headset tracking on startup (PS4 Morpheus only) */
	DECLARE_MULTICAST_DELEGATE(FVRHeadsetTrackingInitializingAndNeedsHMDToBeTrackedDelegate);
	static FVRHeadsetTrackingInitializingAndNeedsHMDToBeTrackedDelegate VRHeadsetTrackingInitializingAndNeedsHMDToBeTrackedDelegate;

	/** Sent when the platform finds that needed headset tracking on startup has completed (PS4 Morpheus only) */
	DECLARE_MULTICAST_DELEGATE(FVRHeadsetTrackingInitializedDelegate);
	static FVRHeadsetTrackingInitializedDelegate VRHeadsetTrackingInitializedDelegate;

	/** Sent when the platform requests a low-level VR recentering */
	DECLARE_MULTICAST_DELEGATE(FVRHeadsetRecenter);
	static FVRHeadsetRecenter VRHeadsetRecenter;

	/** Sent when connection to VR HMD is lost */
	DECLARE_MULTICAST_DELEGATE(FVRHeadsetLost);
	static FVRHeadsetLost VRHeadsetLost;

	/** Sent when connection to VR HMD is restored */
	DECLARE_MULTICAST_DELEGATE(FVRHeadsetReconnected);
	static FVRHeadsetReconnected VRHeadsetReconnected;

	/** Sent when connection to VR HMD connection is refused by the player */
	DECLARE_MULTICAST_DELEGATE(FVRHeadsetConnectCanceled);
	static FVRHeadsetConnectCanceled VRHeadsetConnectCanceled;

	/** Sent when the VR HMD detects that it has been put on by the player. */
	DECLARE_MULTICAST_DELEGATE(FVRHeadsetPutOnHead);
	static FVRHeadsetPutOnHead VRHeadsetPutOnHead;

	/** Sent when the VR HMD detects that it has been taken off by the player. */
	DECLARE_MULTICAST_DELEGATE(FVRHeadsetRemovedFromHead);
	static FVRHeadsetRemovedFromHead VRHeadsetRemovedFromHead;

	/** Sent when a 3DOF VR controller is recentered */
	DECLARE_MULTICAST_DELEGATE(FVRControllerRecentered);
	static FVRControllerRecentered VRControllerRecentered;

	/** Sent when application code changes the user activity hint string for analytics, crash reports, etc */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnUserActivityStringChanged, const FString&);
	static FOnUserActivityStringChanged UserActivityStringChanged;

	/** Sent when application code changes the currently active game session. The exact semantics of this will vary between games but it is useful for analytics, crash reports, etc  */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnGameSessionIDChange, const FString&);
	static FOnGameSessionIDChange GameSessionIDChanged;

	/** Sent by application code to set params that customize crash reporting behavior */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnCrashOverrideParamsChanged, const FCrashOverrideParameters&);
	static FOnCrashOverrideParamsChanged CrashOverrideParamsChanged;
	
	/** Sent by engine code when the "vanilla" status of the engine changes */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnIsVanillaProductChanged, bool);
	static FOnIsVanillaProductChanged IsVanillaProductChanged;

	// Callback for platform specific very early init code.
	DECLARE_MULTICAST_DELEGATE(FOnPreMainInit);
	static FOnPreMainInit& GetPreMainInitDelegate();
	
	/** Sent when GConfig is finished initializing */
	DECLARE_MULTICAST_DELEGATE(FConfigReadyForUse);
	static FConfigReadyForUse ConfigReadyForUse;

	/** Callback for notifications regarding changes of the rendering thread. */
	DECLARE_MULTICAST_DELEGATE(FRenderingThreadChanged)

	/** Sent just after the rendering thread has been created. */
	static FRenderingThreadChanged PostRenderingThreadCreated;
	/* Sent just before the rendering thread is destroyed. */
	static FRenderingThreadChanged PreRenderingThreadDestroyed;

	// Callback to allow custom resolution of package names. Arguments are InRequestedName, OutResolvedName.
	// Should return True of resolution occured.
	DECLARE_DELEGATE_RetVal_TwoParams(bool, FResolvePackageNameDelegate, const FString&, FString&);
	static TArray<FResolvePackageNameDelegate> PackageNameResolvers;

	// Called when module integrity has been compromised. Code should do as little as
	// possible since the app may be in an unknown state. Return 'true' to handle the
	// event and prevent the default check/ensure process occuring
	DECLARE_DELEGATE_RetVal_TwoParams(bool, FImageIntegrityChanged, const TCHAR*, int32);
	static FImageIntegrityChanged OnImageIntegrityChanged;

	// Called to request that systems free whatever memory they are able to. Called early in LoadMap.
	// Caller is responsible for flushing rendering etc. See UEngine::TrimMemory
	static FSimpleMulticastDelegate& GetMemoryTrimDelegate();

	// Called when OOM event occurs, after backup memory has been freed, so there's some hope of being effective
	static FSimpleMulticastDelegate& GetOutOfMemoryDelegate();

	enum class EOnScreenMessageSeverity : uint8
	{
		Info,
		Warning,
		Error,
	};
	typedef TMultiMap<EOnScreenMessageSeverity, FText> FSeverityMessageMap;

	// Called when displaying on screen messages (like the "Lighting needs to be rebuilt"), to let other systems add any messages as needed
	// Sample Usage:
	// void GetMyOnScreenMessages(FCoreDelegates::FSeverityMessageMap& OutMessages)
	// {
	//		OutMessages.Add(FCoreDelegates::EOnScreenMessageSeverity::Info, FText::Format(LOCTEXT("MyMessage", "My Status: {0}"), SomeStatus));
	// }
	DECLARE_MULTICAST_DELEGATE_OneParam(FGetOnScreenMessagesDelegate, FSeverityMessageMap&);
	static FGetOnScreenMessagesDelegate OnGetOnScreenMessages;

	DECLARE_DELEGATE_RetVal(bool, FIsLoadingMovieCurrentlyPlaying)
	static FIsLoadingMovieCurrentlyPlaying IsLoadingMovieCurrentlyPlaying;

private:

	// Callbacks for hotfixes
	static TArray<FHotFixDelegate> HotFixDelegates;

	// This class is only for namespace use
	FCoreDelegates() {}
};
