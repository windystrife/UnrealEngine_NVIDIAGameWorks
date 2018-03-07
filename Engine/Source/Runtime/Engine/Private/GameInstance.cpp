// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Engine/GameInstance.h"
#include "Misc/MessageDialog.h"
#include "Misc/CommandLine.h"
#include "GameMapsSettings.h"
#include "EngineGlobals.h"
#include "Engine/EngineTypes.h"
#include "TimerManager.h"
#include "Engine/LatentActionManager.h"
#include "Engine/World.h"
#include "AI/Navigation/NavigationSystem.h"
#include "Misc/Paths.h"
#include "UObject/CoreOnline.h"
#include "GameFramework/PlayerController.h"
#include "Engine/Engine.h"
#include "Engine/Console.h"
#include "Engine/GameEngine.h"
#include "GameFramework/GameModeBase.h"
#include "Engine/DemoNetDriver.h"
#include "Engine/NetworkObjectList.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/OnlineSession.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/GameSession.h"
#include "Net/OnlineEngineInterface.h"
#include "Kismet/GameplayStatics.h"
#include "Framework/Application/SlateApplication.h"
#include "GenericPlatform/GenericApplication.h"

#if WITH_EDITOR
#include "Settings/LevelEditorPlaySettings.h"
#include "Editor/EditorEngine.h"
#endif

UGameInstance::UGameInstance(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
, TimerManager(new FTimerManager())
, LatentActionManager(new FLatentActionManager())
{
	TimerManager->SetGameInstance(this);
}

void UGameInstance::FinishDestroy()
{
	if (TimerManager)
	{
		delete TimerManager;
		TimerManager = nullptr;
	}

	// delete operator should handle null, but maintaining pattern of TimerManager:
	if (LatentActionManager)
	{
		delete LatentActionManager;
		LatentActionManager = nullptr;
	}

	Super::FinishDestroy();
}

UWorld* UGameInstance::GetWorld() const
{
	return WorldContext ? WorldContext->World() : NULL;
}

UEngine* UGameInstance::GetEngine() const
{
	return CastChecked<UEngine>(GetOuter());
}

void UGameInstance::Init()
{
	ReceiveInit();

	if (!IsRunningCommandlet())
	{
		UClass* SpawnClass = GetOnlineSessionClass();
		OnlineSession = NewObject<UOnlineSession>(this, SpawnClass);
		if (OnlineSession)
		{
			OnlineSession->RegisterOnlineDelegates();
		}

		if (!IsDedicatedServerInstance())
		{
			TSharedPtr<GenericApplication> App = FSlateApplication::Get().GetPlatformApplication();
			if (App.IsValid())
			{
				App->RegisterConsoleCommandListener(GenericApplication::FOnConsoleCommandListener::CreateUObject(this, &ThisClass::OnConsoleInput));
			}
		}

		FNetDelegates::OnReceivedNetworkEncryptionToken.BindUObject(this, &ThisClass::ReceivedNetworkEncryptionToken);
		FNetDelegates::OnReceivedNetworkEncryptionAck.BindUObject(this, &ThisClass::ReceivedNetworkEncryptionAck);
	}
}

void UGameInstance::OnConsoleInput(const FString& Command)
{
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
	UConsole* ViewportConsole = (GEngine->GameViewport != nullptr) ? GEngine->GameViewport->ViewportConsole : nullptr;
	if (ViewportConsole)
	{
		ViewportConsole->ConsoleCommand(Command);
	}
	else
	{
		GEngine->Exec(GetWorld(), *Command);
	}
#endif
}

void UGameInstance::Shutdown()
{
	ReceiveShutdown();

	if (OnlineSession)
	{
		OnlineSession->ClearOnlineDelegates();
		OnlineSession = nullptr;
	}

	for (int32 PlayerIdx = LocalPlayers.Num() - 1; PlayerIdx >= 0; --PlayerIdx)
	{
		ULocalPlayer* Player = LocalPlayers[PlayerIdx];

		if (Player)
		{
			RemoveLocalPlayer(Player);
		}
	}

	FNetDelegates::OnReceivedNetworkEncryptionToken.Unbind();
	FNetDelegates::OnReceivedNetworkEncryptionAck.Unbind();

	// Clear the world context pointer to prevent further access.
	WorldContext = nullptr;
}

void UGameInstance::InitializeStandalone()
{
	// Creates the world context. This should be the only WorldContext that ever gets created for this GameInstance.
	WorldContext = &GetEngine()->CreateNewWorldContext(EWorldType::Game);
	WorldContext->OwningGameInstance = this;

	// In standalone create a dummy world from the beginning to avoid issues of not having a world until LoadMap gets us our real world
	UWorld* DummyWorld = UWorld::CreateWorld(EWorldType::Game, false);
	DummyWorld->SetGameInstance(this);
	WorldContext->SetCurrentWorld(DummyWorld);

	Init();
}

#if WITH_EDITOR

FGameInstancePIEResult UGameInstance::InitializeForPlayInEditor(int32 PIEInstanceIndex, const FGameInstancePIEParameters& Params)
{
	UEditorEngine* const EditorEngine = CastChecked<UEditorEngine>(GetEngine());

	// Look for an existing pie world context, may have been created before
	WorldContext = EditorEngine->GetWorldContextFromPIEInstance(PIEInstanceIndex);

	if (!WorldContext)
	{
		// If not, create a new one
		WorldContext = &EditorEngine->CreateNewWorldContext(EWorldType::PIE);
		WorldContext->PIEInstance = PIEInstanceIndex;
	}

	WorldContext->RunAsDedicated = Params.bRunAsDedicated;

	WorldContext->OwningGameInstance = this;
	
	const FString WorldPackageName = EditorEngine->EditorWorld->GetOutermost()->GetName();

	// Establish World Context for PIE World
	WorldContext->LastURL.Map = WorldPackageName;
	WorldContext->PIEPrefix = WorldContext->PIEInstance != INDEX_NONE ? UWorld::BuildPIEPackagePrefix(WorldContext->PIEInstance) : FString();

	const ULevelEditorPlaySettings* PlayInSettings = GetDefault<ULevelEditorPlaySettings>();

	// We always need to create a new PIE world unless we're using the editor world for SIE
	UWorld* NewWorld = nullptr;

	bool bNeedsGarbageCollection = false;
	const EPlayNetMode PlayNetMode = [&PlayInSettings]{ EPlayNetMode NetMode(PIE_Standalone); return (PlayInSettings->GetPlayNetMode(NetMode) ? NetMode : PIE_Standalone); }();
	const bool CanRunUnderOneProcess = [&PlayInSettings]{ bool RunUnderOneProcess(false); return (PlayInSettings->GetRunUnderOneProcess(RunUnderOneProcess) && RunUnderOneProcess); }();
	if (PlayNetMode == PIE_Client)
	{
		// We are going to connect, so just load an empty world
		NewWorld = EditorEngine->CreatePIEWorldFromEntry(*WorldContext, EditorEngine->EditorWorld, PIEMapName);
	}
	else
	{
		// Standard PIE path: just duplicate the EditorWorld
		NewWorld = EditorEngine->CreatePIEWorldByDuplication(*WorldContext, EditorEngine->EditorWorld, PIEMapName);

		// Duplication can result in unreferenced objects, so indicate that we should do a GC pass after initializing the world context
		bNeedsGarbageCollection = true;
	}

	// failed to create the world!
	if (NewWorld == nullptr)
	{
		return FGameInstancePIEResult::Failure(NSLOCTEXT("UnrealEd", "Error_FailedCreateEditorPreviewWorld", "Failed to create editor preview world."));
	}

	NewWorld->SetGameInstance(this);
	WorldContext->SetCurrentWorld(NewWorld);
	WorldContext->AddRef(EditorEngine->PlayWorld);	// Tie this context to this UEngine::PlayWorld*		// @fixme, needed still?

	// make sure we can clean up this world!
	NewWorld->ClearFlags(RF_Standalone);
	NewWorld->bKismetScriptError = Params.bAnyBlueprintErrors;

	// Do a GC pass if necessary to remove any potentially unreferenced objects
	if(bNeedsGarbageCollection)
	{
		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
	}

	Init();

	// Give the deprecated method a chance to fail as well
	FGameInstancePIEResult InitResult = FGameInstancePIEResult::Success();

	if (InitResult.IsSuccess())
	{
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		InitResult = InitializePIE(Params.bAnyBlueprintErrors, PIEInstanceIndex, Params.bRunAsDedicated) ?
			FGameInstancePIEResult::Success() :
			FGameInstancePIEResult::Failure(NSLOCTEXT("UnrealEd", "Error_CouldntInitInstance", "The game instance failed to Play/Simulate In Editor"));
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}

	return InitResult;
}


bool UGameInstance::InitializePIE(bool bAnyBlueprintErrors, int32 PIEInstance, bool bRunAsDedicated)
{
	// DEPRECATED VERSION
	return true;
}

FGameInstancePIEResult UGameInstance::StartPlayInEditorGameInstance(ULocalPlayer* LocalPlayer, const FGameInstancePIEParameters& Params)
{
	OnStart();

	UEditorEngine* const EditorEngine = CastChecked<UEditorEngine>(GetEngine());
	ULevelEditorPlaySettings const* PlayInSettings = GetDefault<ULevelEditorPlaySettings>();

	const EPlayNetMode PlayNetMode = [&PlayInSettings]{ EPlayNetMode NetMode(PIE_Standalone); return (PlayInSettings->GetPlayNetMode(NetMode) ? NetMode : PIE_Standalone); }();

	// for clients, just connect to the server
	if (PlayNetMode == PIE_Client)
	{
		FString Error;
		FURL BaseURL = WorldContext->LastURL;

		FString URLString(TEXT("127.0.0.1"));
		uint16 ServerPort = 0;
		if (PlayInSettings->GetServerPort(ServerPort))
		{
			URLString += FString::Printf(TEXT(":%hu"), ServerPort);
		}

		if (EditorEngine->Browse(*WorldContext, FURL(&BaseURL, *URLString, (ETravelType)TRAVEL_Absolute), Error) == EBrowseReturnVal::Pending)
		{
			EditorEngine->TransitionType = TT_WaitingToConnect;
		}
		else
		{
			return FGameInstancePIEResult::Failure(FText::Format(NSLOCTEXT("UnrealEd", "Error_CouldntLaunchPIEClient", "Couldn't Launch PIE Client: {0}"), FText::FromString(Error)));
		}
	}
	else
	{
		// we're going to be playing in the current world, get it ready for play
		UWorld* const PlayWorld = GetWorld();

		// make a URL
		FURL URL;
		// If the user wants to start in spectator mode, do not use the custom play world for now
		if (EditorEngine->UserEditedPlayWorldURL.Len() > 0 && !Params.bStartInSpectatorMode)
		{
			// If the user edited the play world url. Verify that the map name is the same as the currently loaded map.
			URL = FURL(NULL, *EditorEngine->UserEditedPlayWorldURL, TRAVEL_Absolute);
			if (URL.Map != PIEMapName)
			{
				// Ensure the URL map name is the same as the generated play world map name.
				URL.Map = PIEMapName;
			}
		}
		else
		{
			// The user did not edit the url, just build one from scratch.
			URL = FURL(NULL, *EditorEngine->BuildPlayWorldURL(*PIEMapName, Params.bStartInSpectatorMode), TRAVEL_Absolute);
		}

		// If a start location is specified, spawn a temporary PlayerStartPIE actor at the start location and use it as the portal.
		AActor* PlayerStart = NULL;
		if (EditorEngine->SpawnPlayFromHereStart(PlayWorld, PlayerStart, EditorEngine->PlayWorldLocation, EditorEngine->PlayWorldRotation) == false)
		{
			// failed to create "play from here" playerstart
			return FGameInstancePIEResult::Failure(NSLOCTEXT("UnrealEd", "Error_FailedCreatePlayFromHerePlayerStart", "Failed to create PlayerStart at desired starting location."));
		}

		if (!PlayWorld->SetGameMode(URL))
		{
			// Setting the game mode failed so bail 
			return FGameInstancePIEResult::Failure(NSLOCTEXT("UnrealEd", "Error_FailedCreateEditorPreviewWorld", "Failed to create editor preview world."));
		}
		
		// Make sure "always loaded" sub-levels are fully loaded
		PlayWorld->FlushLevelStreaming(EFlushLevelStreamingType::Visibility);

		PlayWorld->CreateAISystem();

		PlayWorld->InitializeActorsForPlay(URL);
		// calling it after InitializeActorsForPlay has been called to have all potential bounding boxed initialized
		UNavigationSystem::InitializeForWorld(PlayWorld, LocalPlayers.Num() > 0 ? FNavigationSystemRunMode::PIEMode : FNavigationSystemRunMode::SimulationMode);

		// @todo, just use WorldContext.GamePlayer[0]?
		if (LocalPlayer)
		{
			FString Error;
			if (!LocalPlayer->SpawnPlayActor(URL.ToString(1), Error, PlayWorld))
			{
				return FGameInstancePIEResult::Failure(FText::Format(NSLOCTEXT("UnrealEd", "Error_CouldntSpawnPlayer", "Couldn't spawn player: {0}"), FText::FromString(Error)));
			}
		}

		UGameViewportClient* const GameViewport = GetGameViewportClient();
		if (GameViewport != NULL && GameViewport->Viewport != NULL)
		{
			// Stream any levels now that need to be loaded before the game starts
			GEngine->BlockTillLevelStreamingCompleted(PlayWorld);
		}
		
		if (PlayNetMode == PIE_ListenServer)
		{
			// Add port
			uint16 ServerPort = 0;
			if (PlayInSettings->GetServerPort(ServerPort))
			{
				URL.Port = ServerPort;
			}

			// start listen server with the built URL
			PlayWorld->Listen(URL);
		}

		PlayWorld->BeginPlay();
	}

	// Give the deprecated method a chance to fail as well
	FGameInstancePIEResult StartResult = FGameInstancePIEResult::Success();

	if (StartResult.IsSuccess())
	{
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		StartResult = StartPIEGameInstance(LocalPlayer, Params.bSimulateInEditor, Params.bAnyBlueprintErrors, Params.bStartInSpectatorMode) ?
			FGameInstancePIEResult::Success() :
			FGameInstancePIEResult::Failure(NSLOCTEXT("UnrealEd", "Error_CouldntInitInstance", "The game instance failed to Play/Simulate In Editor"));
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}

	return StartResult;
}

bool UGameInstance::StartPIEGameInstance(ULocalPlayer* LocalPlayer, bool bInSimulateInEditor, bool bAnyBlueprintErrors, bool bStartInSpectatorMode)
{
	// DEPRECATED VERSION
	return true;
}
#endif


UGameViewportClient* UGameInstance::GetGameViewportClient() const
{
	FWorldContext* const WC = GetWorldContext();
	return WC ? WC->GameViewport : nullptr;
}

// This can be defined in the target.cs file to allow map overrides in shipping builds
#ifndef UE_ALLOW_MAP_OVERRIDE_IN_SHIPPING
#define UE_ALLOW_MAP_OVERRIDE_IN_SHIPPING 0
#endif

void UGameInstance::StartGameInstance()
{
	UEngine* const Engine = GetEngine();

	// Create default URL.
	// @note: if we change how we determine the valid start up map update LaunchEngineLoop's GetStartupMap()
	FURL DefaultURL;
	DefaultURL.LoadURLConfig(TEXT("DefaultPlayer"), GGameIni);

	// Enter initial world.
	EBrowseReturnVal::Type BrowseRet = EBrowseReturnVal::Failure;
	FString Error;
	
	const TCHAR* Tmp = FCommandLine::Get();

#if UE_BUILD_SHIPPING && !UE_SERVER && !UE_ALLOW_MAP_OVERRIDE_IN_SHIPPING
	// In shipping don't allow a map override unless on server
	Tmp = TEXT("");
#endif // UE_BUILD_SHIPPING && !UE_SERVER

#if !UE_SERVER
	// Parse replay name if specified on cmdline
	FString ReplayCommand;
	if ( FParse::Value( Tmp, TEXT( "-REPLAY=" ), ReplayCommand ) )
	{
		PlayReplay( ReplayCommand );
		return;
	}
#endif // !UE_SERVER

	const UGameMapsSettings* GameMapsSettings = GetDefault<UGameMapsSettings>();
	const FString& DefaultMap = GameMapsSettings->GetGameDefaultMap();

	FString PackageName;
	if (!FParse::Token(Tmp, PackageName, 0) || **PackageName == '-')
	{
		PackageName = DefaultMap + GameMapsSettings->LocalMapOptions;
	}

	FURL URL(&DefaultURL, *PackageName, TRAVEL_Partial);
	if (URL.Valid)
	{
		BrowseRet = Engine->Browse(*WorldContext, URL, Error);
	}

	// If waiting for a network connection, go into the starting level.
	if (BrowseRet == EBrowseReturnVal::Failure)
	{
		UE_LOG(LogLoad, Error, TEXT("%s"), *FString::Printf(TEXT("Failed to enter %s: %s. Please check the log for errors."), *URL.Map, *Error));

		// the map specified on the command-line couldn't be loaded.  ask the user if we should load the default map instead
		if (FCString::Stricmp(*PackageName, *DefaultMap) != 0)
		{
			const FText Message = FText::Format(NSLOCTEXT("Engine", "MapNotFound", "The map specified on the commandline '{0}' could not be found. Would you like to load the default map instead?"), FText::FromString(URL.Map));
			if (   FCString::Stricmp(*URL.Map, *DefaultMap) != 0  
				&& FMessageDialog::Open(EAppMsgType::OkCancel, Message) != EAppReturnType::Ok)
			{
				// user canceled (maybe a typo while attempting to run a commandlet)
				FPlatformMisc::RequestExit(false);
				return;
			}
			else
			{
				BrowseRet = Engine->Browse(*WorldContext, FURL(&DefaultURL, *(DefaultMap + GameMapsSettings->LocalMapOptions), TRAVEL_Partial), Error);
			}
		}
		else
		{
			const FText Message = FText::Format(NSLOCTEXT("Engine", "MapNotFoundNoFallback", "The map specified on the commandline '{0}' could not be found. Exiting."), FText::FromString(URL.Map));
			FMessageDialog::Open(EAppMsgType::Ok, Message);
			FPlatformMisc::RequestExit(false);
			return;
		}
	}

	// Handle failure.
	if (BrowseRet == EBrowseReturnVal::Failure)
	{
		UE_LOG(LogLoad, Error, TEXT("%s"), *FString::Printf(TEXT("Failed to enter %s: %s. Please check the log for errors."), *DefaultMap, *Error));
		const FText Message = FText::Format(NSLOCTEXT("Engine", "DefaultMapNotFound", "The default map '{0}' could not be found. Exiting."), FText::FromString(DefaultMap));
		FMessageDialog::Open(EAppMsgType::Ok, Message);
		FPlatformMisc::RequestExit(false);
		return;
	}

	OnStart();
}

void UGameInstance::OnStart()
{

}

bool UGameInstance::HandleOpenCommand(const TCHAR* Cmd, FOutputDevice& Ar, UWorld* InWorld)
{
	check(WorldContext && WorldContext->World() == InWorld);

	UEngine* const Engine = GetEngine();
	return Engine->HandleOpenCommand(Cmd, Ar, InWorld);
}

bool UGameInstance::Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
{
	// @todo a bunch of stuff in UEngine probably belongs here as well
	if (FParse::Command(&Cmd, TEXT("OPEN")))
	{
		return HandleOpenCommand(Cmd, Ar, InWorld);
	}

	return false;
}

ULocalPlayer* UGameInstance::CreateInitialPlayer(FString& OutError)
{
	return CreateLocalPlayer( 0, OutError, false );
}

ULocalPlayer* UGameInstance::CreateLocalPlayer(int32 ControllerId, FString& OutError, bool bSpawnActor)
{
	check(GetEngine()->LocalPlayerClass != NULL);

	ULocalPlayer* NewPlayer = NULL;
	int32 InsertIndex = INDEX_NONE;

	const int32 MaxSplitscreenPlayers = (GetGameViewportClient() != NULL) ? GetGameViewportClient()->MaxSplitscreenPlayers : 1;

	if (FindLocalPlayerFromControllerId( ControllerId ) != NULL)
	{
		OutError = FString::Printf(TEXT("A local player already exists for controller ID %d,"), ControllerId);
	}
	else if (LocalPlayers.Num() < MaxSplitscreenPlayers)
	{
		// If the controller ID is not specified then find the first available
		if (ControllerId < 0)
		{
			for (ControllerId = 0; ControllerId < MaxSplitscreenPlayers; ++ControllerId)
			{
				if (FindLocalPlayerFromControllerId( ControllerId ) == NULL)
				{
					break;
				}
			}
			check(ControllerId < MaxSplitscreenPlayers);
		}
		else if (ControllerId >= MaxSplitscreenPlayers)
		{
			UE_LOG(LogPlayerManagement, Warning, TEXT("Controller ID (%d) is unlikely to map to any physical device, so this player will not receive input"), ControllerId);
		}

		NewPlayer = NewObject<ULocalPlayer>(GetEngine(), GetEngine()->LocalPlayerClass);
		InsertIndex = AddLocalPlayer(NewPlayer, ControllerId);
		if (bSpawnActor && InsertIndex != INDEX_NONE && GetWorld() != NULL)
		{
			if (GetWorld()->GetNetMode() != NM_Client)
			{
				// server; spawn a new PlayerController immediately
				if (!NewPlayer->SpawnPlayActor("", OutError, GetWorld()))
				{
					RemoveLocalPlayer(NewPlayer);
					NewPlayer = NULL;
				}
			}
			else
			{
				// client; ask the server to let the new player join
				NewPlayer->SendSplitJoin();
			}
		}
	}
	else
	{
		OutError = FString::Printf(TEXT( "Maximum number of players (%d) already created.  Unable to create more."), MaxSplitscreenPlayers);
	}

	if (OutError != TEXT(""))
	{
		UE_LOG(LogPlayerManagement, Log, TEXT("UPlayer* creation failed with error: %s"), *OutError);
	}

	return NewPlayer;
}

int32 UGameInstance::AddLocalPlayer(ULocalPlayer* NewLocalPlayer, int32 ControllerId)
{
	if (NewLocalPlayer == NULL)
	{
		return INDEX_NONE;
	}

	const int32 InsertIndex = LocalPlayers.Num();

	// Add to list
	LocalPlayers.AddUnique(NewLocalPlayer);

	// Notify the player he/she was added
	NewLocalPlayer->PlayerAdded(GetGameViewportClient(), ControllerId);

	// Notify the viewport that we added a player (so it can update splitscreen settings, etc)
	if ( GetGameViewportClient() != NULL )
	{
		GetGameViewportClient()->NotifyPlayerAdded(InsertIndex, NewLocalPlayer);
	}

	return InsertIndex;
}

bool UGameInstance::RemoveLocalPlayer(ULocalPlayer* ExistingPlayer)
{
	// FIXME: Notify server we want to leave the game if this is an online game
	if (ExistingPlayer->PlayerController != NULL)
	{
		// FIXME: Do this all inside PlayerRemoved?
		ExistingPlayer->PlayerController->CleanupGameViewport();

		// Destroy the player's actors.
		if ( ExistingPlayer->PlayerController->Role == ROLE_Authority )
		{
			ExistingPlayer->PlayerController->Destroy();
		}
	}

	// Remove the player from the context list
	const int32 OldIndex = LocalPlayers.Find(ExistingPlayer);

	if (ensure(OldIndex != INDEX_NONE))
	{
		ExistingPlayer->PlayerRemoved();
		LocalPlayers.RemoveAt(OldIndex);

		// Notify the viewport so the viewport can do the fixups, resize, etc
		if (GetGameViewportClient() != NULL)
		{
			GetGameViewportClient()->NotifyPlayerRemoved(OldIndex, ExistingPlayer);
		}
	}

	// Disassociate this viewport client from the player.
	// Do this after notifications, as some of them require the ViewportClient.
	ExistingPlayer->ViewportClient = NULL;

	UE_LOG(LogPlayerManagement, Log, TEXT("UGameInstance::RemovePlayer: Removed player %s with ControllerId %i at index %i (%i remaining players)"), *ExistingPlayer->GetName(), ExistingPlayer->GetControllerId(), OldIndex, LocalPlayers.Num());

	return true;
}

void UGameInstance::DebugCreatePlayer(int32 ControllerId)
{
#if !UE_BUILD_SHIPPING
	FString Error;
	CreateLocalPlayer(ControllerId, Error, true);
	if (Error.Len() > 0)
	{
		UE_LOG(LogPlayerManagement, Error, TEXT("Failed to DebugCreatePlayer: %s"), *Error);
	}
#endif
}

void UGameInstance::DebugRemovePlayer(int32 ControllerId)
{
#if !UE_BUILD_SHIPPING

	ULocalPlayer* const ExistingPlayer = FindLocalPlayerFromControllerId(ControllerId);
	if (ExistingPlayer != NULL)
	{
		RemoveLocalPlayer(ExistingPlayer);
	}
#endif
}

int32 UGameInstance::GetNumLocalPlayers() const
{
	return LocalPlayers.Num();
}

ULocalPlayer* UGameInstance::GetLocalPlayerByIndex(const int32 Index) const
{
	return LocalPlayers[Index];
}

APlayerController* UGameInstance::GetFirstLocalPlayerController(UWorld* World) const
{
	if (World == nullptr)
	{
		for (ULocalPlayer* Player : LocalPlayers)
		{
			// Returns the first non-null UPlayer::PlayerController without filtering by UWorld.
			if (Player && Player->PlayerController)
			{
				// return first non-null entry
				return Player->PlayerController;
			}
		}
	}
	else
	{
		// Only return a local PlayerController from the given World.
		for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
		{
			if (*Iterator != nullptr && (*Iterator)->IsLocalController())
			{
				return Iterator->Get();
			}
		}
	}

	// didn't find one
	return nullptr;
}

APlayerController* UGameInstance::GetPrimaryPlayerController() const
{
	UWorld* World = GetWorld();
	check(World);

	APlayerController* PrimaryController = nullptr;
	for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		APlayerController* NextPlayer = Iterator->Get();
		if (NextPlayer && NextPlayer->PlayerState && NextPlayer->PlayerState->UniqueId.IsValid() && NextPlayer->IsPrimaryPlayer())
		{
			PrimaryController = NextPlayer;
			break;
		}
	}

	return PrimaryController;
}

TSharedPtr<const FUniqueNetId> UGameInstance::GetPrimaryPlayerUniqueId() const
{
	ULocalPlayer* PrimaryLP = nullptr;

	TArray<ULocalPlayer*>::TConstIterator LocalPlayerIt = GetLocalPlayerIterator();
	for (; LocalPlayerIt && *LocalPlayerIt; ++LocalPlayerIt)
	{
		PrimaryLP = *LocalPlayerIt;
		if (PrimaryLP && PrimaryLP->PlayerController && PrimaryLP->PlayerController->IsPrimaryPlayer())
		{
			break;
		}
	}

	TSharedPtr<const FUniqueNetId> LocalUserId = nullptr;
	if (PrimaryLP)
	{
		LocalUserId = PrimaryLP->GetPreferredUniqueNetId();
	}

	return LocalUserId;
}

ULocalPlayer* UGameInstance::FindLocalPlayerFromControllerId(const int32 ControllerId) const
{
	for (ULocalPlayer * LP : LocalPlayers)
	{
		if (LP && (LP->GetControllerId() == ControllerId))
		{
			return LP;
		}
	}

	return nullptr;
}

ULocalPlayer* UGameInstance::FindLocalPlayerFromUniqueNetId(const FUniqueNetId& UniqueNetId) const
{
	for (ULocalPlayer* Player : LocalPlayers)
	{
		if (Player == NULL)
		{
			continue;
		}

		TSharedPtr<const FUniqueNetId> OtherUniqueNetId = Player->GetPreferredUniqueNetId();

		if (!OtherUniqueNetId.IsValid())
		{
			continue;
		}

		if (*OtherUniqueNetId == UniqueNetId)
		{
			// Match
			return Player;
		}
	}

	// didn't find one
	return nullptr;
}

ULocalPlayer* UGameInstance::FindLocalPlayerFromUniqueNetId(TSharedPtr<const FUniqueNetId> UniqueNetId) const
{
	if (!UniqueNetId.IsValid())
	{
		return nullptr;
	}

	return FindLocalPlayerFromUniqueNetId(*UniqueNetId);
}

ULocalPlayer* UGameInstance::GetFirstGamePlayer() const
{
	return (LocalPlayers.Num() > 0) ? LocalPlayers[0] : nullptr;
}

void UGameInstance::CleanupGameViewport()
{
	// Clean up the viewports that have been closed.
	for(int32 idx = LocalPlayers.Num()-1; idx >= 0; --idx)
	{
		ULocalPlayer *Player = LocalPlayers[idx];

		if(Player && Player->ViewportClient && !Player->ViewportClient->Viewport)
		{
			RemoveLocalPlayer( Player );
		}
	}
}

TArray<class ULocalPlayer*>::TConstIterator	UGameInstance::GetLocalPlayerIterator() const
{
	return LocalPlayers.CreateConstIterator();
}

const TArray<class ULocalPlayer*>& UGameInstance::GetLocalPlayers() const
{
	return LocalPlayers;
}

void UGameInstance::StartRecordingReplay(const FString& Name, const FString& FriendlyName, const TArray<FString>& AdditionalOptions)
{
	if ( FParse::Param( FCommandLine::Get(),TEXT( "NOREPLAYS" ) ) )
	{
		UE_LOG( LogDemo, Warning, TEXT( "UGameInstance::StartRecordingReplay: Rejected due to -noreplays option" ) );
		return;
	}

	UWorld* CurrentWorld = GetWorld();

	if ( CurrentWorld == nullptr )
	{
		UE_LOG( LogDemo, Warning, TEXT( "UGameInstance::StartRecordingReplay: GetWorld() is null" ) );
		return;
	}

	if ( CurrentWorld->WorldType == EWorldType::PIE )
	{
		UE_LOG(LogDemo, Warning, TEXT("UGameInstance::StartRecordingReplay: Function called while running a PIE instance, this is disabled."));
		return;
	}

	if ( CurrentWorld->DemoNetDriver && CurrentWorld->DemoNetDriver->IsPlaying() )
	{
		UE_LOG(LogDemo, Warning, TEXT("UGameInstance::StartRecordingReplay: A replay is already playing, cannot begin recording another one."));
		return;
	}

	FURL DemoURL;
	FString DemoName = Name;
	
	DemoName.ReplaceInline( TEXT( "%m" ), *CurrentWorld->GetMapName() );

	// replace the current URL's map with a demo extension
	DemoURL.Map = DemoName;
	DemoURL.AddOption( *FString::Printf( TEXT( "DemoFriendlyName=%s" ), *FriendlyName ) );

	for ( const FString& Option : AdditionalOptions )
	{
		DemoURL.AddOption(*Option);
	}

	bool bDestroyedDemoNetDriver = false;
	if (!CurrentWorld->DemoNetDriver || !CurrentWorld->DemoNetDriver->bRecordMapChanges || !CurrentWorld->DemoNetDriver->IsRecordingPaused())
	{
		CurrentWorld->DestroyDemoNetDriver();
		bDestroyedDemoNetDriver = true; 

		const FName NAME_DemoNetDriver(TEXT("DemoNetDriver"));

		if (!GEngine->CreateNamedNetDriver(CurrentWorld, NAME_DemoNetDriver, NAME_DemoNetDriver))
		{
			UE_LOG(LogDemo, Warning, TEXT("RecordReplay: failed to create demo net driver!"));
			return;
		}

		CurrentWorld->DemoNetDriver = Cast< UDemoNetDriver >(GEngine->FindNamedNetDriver(CurrentWorld, NAME_DemoNetDriver));
	}

	check(CurrentWorld->DemoNetDriver != nullptr);

	CurrentWorld->DemoNetDriver->SetWorld( CurrentWorld );

	// Set the new demo driver as the current collection's driver
	FLevelCollection* CurrentLevelCollection = CurrentWorld->FindCollectionByType(ELevelCollectionType::DynamicSourceLevels);
	if (CurrentLevelCollection)
	{
		CurrentLevelCollection->SetDemoNetDriver(CurrentWorld->DemoNetDriver);
	}

	FString Error;

	if (bDestroyedDemoNetDriver)
	{
		if (!CurrentWorld->DemoNetDriver->InitListen(CurrentWorld, DemoURL, false, Error))
		{
			UE_LOG(LogDemo, Warning, TEXT("Demo recording - InitListen failed: %s"), *Error);
			CurrentWorld->DemoNetDriver = NULL;
			return;
		}
	}
	else if (!CurrentWorld->DemoNetDriver->ContinueListen(DemoURL))
	{
		UE_LOG(LogDemo, Warning, TEXT("Demo recording - ContinueListen failed"));
		CurrentWorld->DemoNetDriver = NULL;
		return;
	}

	UE_LOG(LogDemo, Log, TEXT( "Num Network Actors: %i" ), CurrentWorld->DemoNetDriver->GetNetworkObjectList().GetActiveObjects().Num() );
}

void UGameInstance::StopRecordingReplay()
{
	UWorld* CurrentWorld = GetWorld();

	if ( CurrentWorld == nullptr )
	{
		UE_LOG( LogDemo, Warning, TEXT( "UGameInstance::StopRecordingReplay: GetWorld() is null" ) );
		return;
	}

	bool LoadDefaultMap = false;

	if ( CurrentWorld->DemoNetDriver && CurrentWorld->DemoNetDriver->IsPlaying() )
	{
		LoadDefaultMap = true;
	}

	CurrentWorld->DestroyDemoNetDriver();

	if ( LoadDefaultMap )
	{
		GEngine->BrowseToDefaultMap(*GetWorldContext());
	}
}

void UGameInstance::PlayReplay(const FString& Name, UWorld* WorldOverride, const TArray<FString>& AdditionalOptions)
{
	UWorld* CurrentWorld = WorldOverride != nullptr ? WorldOverride : GetWorld();

	if ( CurrentWorld == nullptr )
	{
		UE_LOG( LogDemo, Warning, TEXT( "UGameInstance::PlayReplay: GetWorld() is null" ) );
		return;
	}

	if ( CurrentWorld->WorldType == EWorldType::PIE )
	{
		UE_LOG( LogDemo, Warning, TEXT( "UGameInstance::PlayReplay: Function called while running a PIE instance, this is disabled." ) );
		return;
	}

	CurrentWorld->DestroyDemoNetDriver();

	FURL DemoURL;
	UE_LOG( LogDemo, Log, TEXT( "PlayReplay: Attempting to play demo %s" ), *Name );

	DemoURL.Map = Name;
	
	for ( const FString& Option : AdditionalOptions )
	{
		DemoURL.AddOption(*Option);
	}

	const FName NAME_DemoNetDriver( TEXT( "DemoNetDriver" ) );

	if ( !GEngine->CreateNamedNetDriver( CurrentWorld, NAME_DemoNetDriver, NAME_DemoNetDriver ) )
	{
		UE_LOG(LogDemo, Warning, TEXT( "PlayReplay: failed to create demo net driver!" ) );
		return;
	}

	CurrentWorld->DemoNetDriver = Cast< UDemoNetDriver >( GEngine->FindNamedNetDriver( CurrentWorld, NAME_DemoNetDriver ) );

	check( CurrentWorld->DemoNetDriver != NULL );

	CurrentWorld->DemoNetDriver->SetWorld( CurrentWorld );

	FString Error;

	if ( !CurrentWorld->DemoNetDriver->InitConnect( CurrentWorld, DemoURL, Error ) )
	{
		UE_LOG(LogDemo, Warning, TEXT( "Demo playback failed: %s" ), *Error );
		CurrentWorld->DestroyDemoNetDriver();
	}
	else
	{
		FCoreUObjectDelegates::PostDemoPlay.Broadcast();
	}
}

void UGameInstance::AddUserToReplay(const FString& UserString)
{
	UWorld* CurrentWorld = GetWorld();

	if ( CurrentWorld != nullptr && CurrentWorld->DemoNetDriver != nullptr )
	{
		CurrentWorld->DemoNetDriver->AddUserToReplay( UserString );
	}
}

void UGameInstance::ReceivedNetworkEncryptionToken(const FString& EncryptionToken, const FOnEncryptionKeyResponse& Delegate)
{
	FEncryptionKeyResponse Response(EEncryptionResponse::Failure, TEXT("ReceivedNetworkEncryptionToken not implemented"));
	Delegate.ExecuteIfBound(Response);
}

void UGameInstance::ReceivedNetworkEncryptionAck(const FOnEncryptionKeyResponse& Delegate)
{
	FEncryptionKeyResponse Response(EEncryptionResponse::Failure, TEXT("ReceivedNetworkEncryptionAck not implemented"));
	Delegate.ExecuteIfBound(Response);
}

TSubclassOf<UOnlineSession> UGameInstance::GetOnlineSessionClass()
{
	return UOnlineSession::StaticClass();
}

bool UGameInstance::IsDedicatedServerInstance() const
{
	if (IsRunningDedicatedServer())
	{
		return true;
	}
	else
	{
		return WorldContext ? WorldContext->RunAsDedicated : false;
	}
}

FName UGameInstance::GetOnlinePlatformName() const
{
	return UOnlineEngineInterface::Get()->GetDefaultOnlineSubsystemName();
}

bool UGameInstance::ClientTravelToSession(int32 ControllerId, FName InSessionName)
{
	UWorld* World = GetWorld();

	FString URL;
	if (UOnlineEngineInterface::Get()->GetResolvedConnectString(World, InSessionName, URL))
	{
		ULocalPlayer* LP = GEngine->GetLocalPlayerFromControllerId(World, ControllerId);
		APlayerController* PC = LP ? LP->PlayerController : nullptr;
		if (PC)
		{
			PC->ClientTravel(URL, TRAVEL_Absolute);
			return true;
		}
		else
		{
			UE_LOG(LogGameSession, Warning, TEXT("Failed to find local player for controller id %d"), ControllerId);
		}
	}
	else
	{
		UE_LOG(LogGameSession, Warning, TEXT("Failed to resolve session connect string for %s"), *InSessionName.ToString());
	}

	return false;
}

void UGameInstance::NotifyPreClientTravel(const FString& PendingURL, ETravelType TravelType, bool bIsSeamlessTravel)
{
	OnNotifyPreClientTravel().Broadcast(PendingURL, TravelType, bIsSeamlessTravel);
}

void UGameInstance::PreloadContentForURL(FURL InURL)
{
	// Preload game mode and other content if needed here
}

AGameModeBase* UGameInstance::CreateGameModeForURL(FURL InURL)
{
	UWorld* World = GetWorld();
	// Init the game info.
	FString Options(TEXT(""));
	TCHAR GameParam[256] = TEXT("");
	FString	Error = TEXT("");
	AWorldSettings* Settings = World->GetWorldSettings();
	for (int32 i = 0; i < InURL.Op.Num(); i++)
	{
		Options += TEXT("?");
		Options += InURL.Op[i];
		FParse::Value(*InURL.Op[i], TEXT("GAME="), GameParam, ARRAY_COUNT(GameParam));
	}

	UGameEngine* const GameEngine = Cast<UGameEngine>(GEngine);

	// Get the GameMode class. Start by using the default game type specified in the map's worldsettings.  It may be overridden by settings below.
	TSubclassOf<AGameModeBase> GameClass = Settings->DefaultGameMode;

	// If there is a GameMode parameter in the URL, allow it to override the default game type
	if (GameParam[0])
	{
		FString const GameClassName = UGameMapsSettings::GetGameModeForName(FString(GameParam));

		// If the gamename was specified, we can use it to fully load the pergame PreLoadClass packages
		if (GameEngine)
		{
			GameEngine->LoadPackagesFully(World, FULLYLOAD_Game_PreLoadClass, *GameClassName);
		}

		// Don't overwrite the map's world settings if we failed to load the value off the command line parameter
		TSubclassOf<AGameModeBase> GameModeParamClass = LoadClass<AGameModeBase>(nullptr, *GameClassName);
		if (GameModeParamClass)
		{
			GameClass = GameModeParamClass;
		}
		else
		{
			UE_LOG(LogLoad, Warning, TEXT("Failed to load game mode '%s' specified by URL options."), *GameClassName);
		}
	}

	// Next try to parse the map prefix
	if (!GameClass)
	{
		FString MapName = InURL.Map;
		FString MapNameNoPath = FPaths::GetBaseFilename(MapName);
		if (MapNameNoPath.StartsWith(PLAYWORLD_PACKAGE_PREFIX))
		{
			const int32 PrefixLen = UWorld::BuildPIEPackagePrefix(WorldContext->PIEInstance).Len();
			MapNameNoPath = MapNameNoPath.Mid(PrefixLen);
		}

		FString const GameClassName = UGameMapsSettings::GetGameModeForMapName(FString(MapNameNoPath));

		if (!GameClassName.IsEmpty())
		{
			if (GameEngine)
			{
				GameEngine->LoadPackagesFully(World, FULLYLOAD_Game_PreLoadClass, *GameClassName);
			}

			TSubclassOf<AGameModeBase> GameModeParamClass = LoadClass<AGameModeBase>(nullptr, *GameClassName);
			if (GameModeParamClass)
			{
				GameClass = GameModeParamClass;
			}
			else
			{
				UE_LOG(LogLoad, Warning, TEXT("Failed to load game mode '%s' specified by prefixed map name %s."), *GameClassName, *MapNameNoPath);
			}
		}
	}

	// Fall back to game default
	if (!GameClass)
	{
		GameClass = LoadClass<AGameModeBase>(nullptr, *UGameMapsSettings::GetGlobalDefaultGameMode());
	}

	if (!GameClass)
	{
		// Fall back to raw GameMode
		GameClass = AGameModeBase::StaticClass();
	}
	else
	{
		// See if game instance wants to override it
		GameClass = OverrideGameModeClass(GameClass, FPaths::GetBaseFilename(InURL.Map), Options, *InURL.Portal);
	}

	// no matter how the game was specified, we can use it to load the PostLoadClass packages
	if (GameEngine)
	{
		GameEngine->LoadPackagesFully(World, FULLYLOAD_Game_PostLoadClass, GameClass->GetPathName());
		GameEngine->LoadPackagesFully(World, FULLYLOAD_Game_PostLoadClass, TEXT("LoadForAllGameModes"));
	}

	// Spawn the GameMode.
	UE_LOG(LogLoad, Log, TEXT("Game class is '%s'"), *GameClass->GetName());
	FActorSpawnParameters SpawnInfo;
	SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnInfo.ObjectFlags |= RF_Transient;	// We never want to save game modes into a map

	return World->SpawnActor<AGameModeBase>(GameClass, SpawnInfo);
}

TSubclassOf<AGameModeBase> UGameInstance::OverrideGameModeClass(TSubclassOf<AGameModeBase> GameModeClass, const FString& MapName, const FString& Options, const FString& Portal) const
{
	 return GameModeClass;
}