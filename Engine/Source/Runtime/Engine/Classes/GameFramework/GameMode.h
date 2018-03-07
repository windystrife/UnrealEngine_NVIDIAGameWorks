// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Templates/SubclassOf.h"
#include "GameFramework/GameModeBase.h"
#include "GameMode.generated.h"

class APlayerState;
class ULocalMessage;
class UNetDriver;

/** Possible state of the current match, where a match is all the gameplay that happens on a single map */
namespace MatchState
{
	extern ENGINE_API const FName EnteringMap;			// We are entering this map, actors are not yet ticking
	extern ENGINE_API const FName WaitingToStart;		// Actors are ticking, but the match has not yet started
	extern ENGINE_API const FName InProgress;			// Normal gameplay is occurring. Specific games will have their own state machine inside this state
	extern ENGINE_API const FName WaitingPostMatch;		// Match has ended so we aren't accepting new players, but actors are still ticking
	extern ENGINE_API const FName LeavingMap;			// We are transitioning out of the map to another location
	extern ENGINE_API const FName Aborted;				// Match has failed due to network issues or other problems, cannot continue

	// If a game needs to add additional states, you may need to override HasMatchStarted and HasMatchEnded to deal with the new states
	// Do not add any states before WaitingToStart or after WaitingPostMatch
}

/**
 * GameMode is a subclass of GameModeBase that behaves like a multiplayer match-based game.
 * It has default behavior for picking spawn points and match state.
 * If you want a simpler base, inherit from GameModeBase instead.
 */
UCLASS()
class ENGINE_API AGameMode : public AGameModeBase
{
	GENERATED_UCLASS_BODY()

	// Code to deal with the match state machine

	/** Returns the current match state, this is an accessor to protect the state machine flow */
	UFUNCTION(BlueprintCallable, Category="Game")
	FName GetMatchState() const { return MatchState; }

	/** Returns true if the match state is InProgress or other gameplay state */
	UFUNCTION(BlueprintCallable, Category="Game")
	virtual bool IsMatchInProgress() const;

	/** Returns true if the match state is WaitingPostMatch or later */
	UFUNCTION(BlueprintCallable, Category="Game")
	virtual bool HasMatchEnded() const;

	/** Transition from WaitingToStart to InProgress. You can call this manually, will also get called if ReadyToStartMatch returns true */
	UFUNCTION(BlueprintCallable, Category="Game")
	virtual void StartMatch();

	/** Transition from InProgress to WaitingPostMatch. You can call this manually, will also get called if ReadyToEndMatch returns true */
	UFUNCTION(BlueprintCallable, Category="Game")
	virtual void EndMatch();

	/** Restart the game, by default travel to the current map */
	UFUNCTION(BlueprintCallable, Category="Game")
	virtual void RestartGame();

	/** Report that a match has failed due to unrecoverable error */
	UFUNCTION(BlueprintCallable, Category="Game")
	virtual void AbortMatch();

protected:

	/** What match state we are currently in */
	UPROPERTY(Transient)
	FName MatchState;

	/** Updates the match state and calls the appropriate transition functions */
	virtual void SetMatchState(FName NewState);

	/** Overridable virtual function to dispatch the appropriate transition functions before GameState and Blueprints get SetMatchState calls. */
	virtual void OnMatchStateSet();

	/** Implementable event to respond to match state changes */
	UFUNCTION(BlueprintImplementableEvent, Category="Game", meta=(DisplayName="OnSetMatchState"))
	void K2_OnSetMatchState(FName NewState);

	// Games should override these functions to deal with their game specific logic

	/** Called when the state transitions to WaitingToStart */
	virtual void HandleMatchIsWaitingToStart();

	/** @return True if ready to Start Match. Games should override this */
	UFUNCTION(BlueprintNativeEvent, Category="Game")
	bool ReadyToStartMatch();

	/** Called when the state transitions to InProgress */
	virtual void HandleMatchHasStarted();

	/** @return true if ready to End Match. Games should override this */
	UFUNCTION(BlueprintNativeEvent, Category="Game")
	bool ReadyToEndMatch();

	/** Called when the map transitions to WaitingPostMatch */
	virtual void HandleMatchHasEnded();

	/** Called when the match transitions to LeavingMap */
	virtual void HandleLeavingMap();

	/** Called when the match transitions to Aborted */
	virtual void HandleMatchAborted();

public:

	/** Whether the game should immediately start when the first player logs in. Affects the default behavior of ReadyToStartMatch */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="GameMode")
	uint32 bDelayedStart : 1;

	/** Current number of spectators. */
	UPROPERTY(BlueprintReadOnly, Category=GameMode)
	int32 NumSpectators;    

	/** Current number of human players. */
	UPROPERTY(BlueprintReadOnly, Category=GameMode)
	int32 NumPlayers;    

	/** number of non-human players (AI controlled but participating as a player). */
	UPROPERTY(BlueprintReadOnly, Category=GameMode)
	int32 NumBots;    

	/** Minimum time before player can respawn after dying. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=GameMode, meta=(DisplayName="Minimum Respawn Delay"))
	float MinRespawnDelay;

	/** Number of players that are still traveling from a previous map */
	UPROPERTY(BlueprintReadOnly, Category=GameMode)
	int32 NumTravellingPlayers;

	/** Contains strings describing localized game agnostic messages. */
	UPROPERTY()
	TSubclassOf<class ULocalMessage> EngineMessageClass;

	/** PlayerStates of players who have disconnected from the server (saved in case they reconnect) */
	UPROPERTY()
	TArray<class APlayerState*> InactivePlayerArray;    

protected:

	/** Time a playerstate will stick around in an inactive state after a player logout */
	UPROPERTY(EditAnywhere, Category=GameMode)
	float InactivePlayerStateLifeSpan;

	/** If true, dedicated servers will record replays when HandleMatchHasStarted/HandleMatchHasStopped is called */
	UPROPERTY(config)
	bool bHandleDedicatedServerReplays;

public:

	/** Get local address */
	virtual FString GetNetworkNumber();
	
	/** Will remove the controller from the correct count then add them to the spectator count. **/
	void PlayerSwitchedToSpectatorOnly(APlayerController* PC);

	/** Removes the passed in player controller from the correct count for player/spectator/tranistioning **/
	void RemovePlayerControllerFromPlayerCount(APlayerController* PC);

	DEPRECATED(4.14, "Deprecated in favor of PreloadContentForURL on GameInstance")
	virtual FString GetDefaultGameClassPath(const FString& MapName, const FString& Options, const FString& Portal) const;

	DEPRECATED(4.14, "Deprecated in favor of OverrideGameModeClass on GameInstance")
	virtual TSubclassOf<AGameMode> GetGameModeClass(const FString& MapName, const FString& Options, const FString& Portal) const;

	DEPRECATED(4.14, "Deprecated in favor of GetGameModeForName on GameMapsSettings")
	static FString StaticGetFullGameClassName(FString const& Str);

	/** Return true if we want to travel_absolute, used by RestartGame by default */
	virtual bool GetTravelType();

	DEPRECATED(4.14, "SendPlayer is not in use, call ClientTravel directly instead")
	virtual void SendPlayer( APlayerController* aPlayer, const FString& URL );
	
	DEPRECATED(4.14, "StartNewPlayer has been split into InitializeHUDForPlayer and HandleStartingNewPlayer")
	virtual void StartNewPlayer(APlayerController* NewPlayer);

	/** Exec command to broadcast a string to all players */
	UFUNCTION(Exec, BlueprintCallable, Category = AI)
	virtual void Say(const FString& Msg);

	/** Alters the synthetic bandwidth limit for a running game. */
	DEPRECATED(4.17, "AsyncIOBandwidthLimit is no longer configurable")
	UFUNCTION(exec)	
	virtual void SetBandwidthLimit(float AsyncIOBandwidthLimit) {}

	/** Broadcast a string to all players. */
	virtual void Broadcast( AActor* Sender, const FString& Msg, FName Type = NAME_None );

	/**
	 * Broadcast a localized message to all players.
	 * Most message deal with 0 to 2 related PlayerStates.
	 * The LocalMessage class defines how the PlayerState's and optional actor are used.
	 */
	virtual void BroadcastLocalized( AActor* Sender, TSubclassOf<ULocalMessage> Message, int32 Switch = 0, APlayerState* RelatedPlayerState_1 = NULL, APlayerState* RelatedPlayerState_2 = NULL, UObject* OptionalObject = NULL );

	/** Add PlayerState to the inactive list, remove from the active list */
	virtual void AddInactivePlayer(APlayerState* PlayerState, APlayerController* PC);

	/** Attempt to find and associate an inactive PlayerState with entering PC.  
	  * @Returns true if a PlayerState was found and associated with PC. */
	virtual bool FindInactivePlayer(APlayerController* PC);

	/** Override PC's PlayerState with the values in OldPlayerState as part of the inactive player handling */
	virtual void OverridePlayerState(APlayerController* PC, APlayerState* OldPlayerState);

	/** SetViewTarget of player control on server change */
	virtual void SetSeamlessTravelViewTarget(APlayerController* PC);

	/** Called when this PC is in cinematic mode, and its matinee is canceled by the user. */
	virtual void MatineeCancelled();

	/**
	 * Called from CommitMapChange before unloading previous level. Used for asynchronous level streaming
	 * @param PreviousMapName - Name of the previous persistent level
	 * @param NextMapName - Name of the persistent level being streamed to
	 */
	virtual void PreCommitMapChange(const FString& PreviousMapName, const FString& NextMapName);

	/** Called from CommitMapChange after unloading previous level and loading new level+sublevels. Used for asynchronous level streaming */
	virtual void PostCommitMapChange();

	/** Called when a connection closes before getting to PostLogin() */
	virtual void NotifyPendingConnectionLost();

	/** Handles when a player is disconnected, before the session does */
	virtual void HandleDisconnect(UWorld* InWorld, UNetDriver* NetDriver);

	//~ Begin AActor Interface
	virtual void Tick(float DeltaSeconds) override;
	//~ End AActor Interface

	//~ Begin AGameModeBase Interface
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void StartPlay() override;
	virtual bool HasMatchStarted() const override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;
	virtual int32 GetNumPlayers() override;
	virtual int32 GetNumSpectators() override;
	virtual bool IsHandlingReplays() override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	virtual bool PlayerCanRestart_Implementation(APlayerController* Player) override;
	virtual void PostSeamlessTravel() override;
	virtual void HandleSeamlessTravelPlayer(AController*& C) override;
	virtual void InitSeamlessTravelPlayer(AController* NewController) override;
	virtual bool CanServerTravel(const FString& URL, bool bAbsolute) override;
	virtual void StartToLeaveMap() override;
	virtual bool SpawnPlayerFromSimulate(const FVector& NewLocation, const FRotator& NewRotation) override;
	//~ End AGameModeBase Interface
};
