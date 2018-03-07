// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Templates/SubclassOf.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Info.h"
#include "GameFramework/GameModeBase.h"
#include "GameStateBase.generated.h"

class APlayerState;
class ASpectatorPawn;

ENGINE_API DECLARE_LOG_CATEGORY_EXTERN(LogGameState, Log, All);

class AGameModeBase;
class ASpectatorPawn;
class APlayerState;
class AController;

/**
 * GameStateBase is a class that manages the game's global state, and is spawned by GameModeBase.
 * It exists on both the client and the server and is fully replicated.
 */
UCLASS(config=Game, notplaceable, BlueprintType, Blueprintable)
class ENGINE_API AGameStateBase : public AInfo
{
	GENERATED_UCLASS_BODY()

	//~=============================================================================
	// General accessors and variables

	/** Class of the server's game mode, assigned by GameModeBase. */
	UPROPERTY(Transient, BlueprintReadOnly, Category=GameState, ReplicatedUsing=OnRep_GameModeClass)
	TSubclassOf<AGameModeBase>  GameModeClass;

	/** Instance of the current game mode, exists only on the server. For non-authority clients, this will be NULL. */
	UPROPERTY(Transient, BlueprintReadOnly, Category=GameState)
	AGameModeBase* AuthorityGameMode;

	/** Class used by spectators, assigned by GameModeBase. */
	UPROPERTY(Transient, BlueprintReadOnly, Category=GameState, ReplicatedUsing=OnRep_SpectatorClass)
	TSubclassOf<ASpectatorPawn> SpectatorClass;

	/** Array of all PlayerStates, maintained on both server and clients (PlayerStates are always relevant) */
	UPROPERTY(Transient, BlueprintReadOnly, Category=GameState)
	TArray<APlayerState*> PlayerArray;

	/** Allow game states to react to asset packages being loaded asynchronously */
	virtual void AsyncPackageLoaded(UObject* Package) {}

	/** Helper to return the default object of the GameModeBase class corresponding to this GameState. This object is not safe to modify. */
	const AGameModeBase* GetDefaultGameMode() const;

	/** Helper template to returns the GameModeBase default object cast to the right type */
	template< class T >
	const T* GetDefaultGameMode() const
	{
		return Cast<T>(GetDefaultGameMode());
	}

	/** Returns the simulated TimeSeconds on the server, will be synchronized on client and server */
	UFUNCTION(BlueprintCallable, Category=GameState)
	virtual float GetServerWorldTimeSeconds() const;

	/** Returns true if the world has started play (called BeginPlay on actors) */
	UFUNCTION(BlueprintCallable, Category=GameState)
	virtual bool HasBegunPlay() const;

	/** Returns true if the world has started match (called MatchStarted callbacks) */
	UFUNCTION(BlueprintCallable, Category=GameState)
	virtual bool HasMatchStarted() const;

	/** Returns the time that should be used as when a player started */
	UFUNCTION(BlueprintCallable, Category=GameState)
	virtual float GetPlayerStartTime(AController* Controller) const;

	/** Returns how much time needs to be spent before a player can respawn */
	UFUNCTION(BlueprintCallable, Category=GameState)
	virtual float GetPlayerRespawnDelay(AController* Controller) const;

	virtual void GetLifetimeReplicatedProps(TArray< FLifetimeProperty > &OutLifetimeProps) const;

	//~=============================================================================
	// Interaction with GameModeBase

	/** Called when the GameClass property is set (at startup for the server, after the variable has been replicated on clients) */
	virtual void ReceivedGameModeClass();

	/** Called when the SpectatorClass property is set (at startup for the server, after the variable has been replicated on clients) */
	virtual void ReceivedSpectatorClass();

	/** Called during seamless travel transition twice (once when the transition map is loaded, once when destination map is loaded) */
	virtual void SeamlessTravelTransitionCheckpoint(bool bToTransitionMap);

	/** Add PlayerState to the PlayerArray */
	virtual void AddPlayerState(APlayerState* PlayerState);

	/** Remove PlayerState from the PlayerArray. */
	virtual void RemovePlayerState(APlayerState* PlayerState);

	/** Called by game mode to set the started play bool */
	virtual void HandleBeginPlay();

	//~ Begin AActor Interface
	virtual void PostInitializeComponents() override;
	//~ End AActor Interface

protected:

	/** GameModeBase class notification callback. */
	UFUNCTION()
	virtual void OnRep_GameModeClass();

	/** Callback when we receive the spectator class */
	UFUNCTION()
	virtual void OnRep_SpectatorClass();

	/** By default calls BeginPlay and StartMatch */
	UFUNCTION()
	virtual void OnRep_ReplicatedHasBegunPlay();

	/** Replicated when GameModeBase->StartPlay has been called so the client will also start play */
	UPROPERTY(Transient, ReplicatedUsing = OnRep_ReplicatedHasBegunPlay)
	bool bReplicatedHasBegunPlay;

	/** Called periodically to update ReplicatedWorldTimeSeconds */
	virtual void UpdateServerTimeSeconds();

	/** Allows clients to calculate ServerWorldTimeSecondsDelta */
	UFUNCTION()
	virtual void OnRep_ReplicatedWorldTimeSeconds();

	/** Server TimeSeconds. Useful for syncing up animation and gameplay. */
	UPROPERTY(Transient, ReplicatedUsing=OnRep_ReplicatedWorldTimeSeconds)
	float ReplicatedWorldTimeSeconds;

	/** The difference from the local world's TimeSeconds and the server world's TimeSeconds. */
	UPROPERTY(Transient)
	float ServerWorldTimeSecondsDelta;

	/** Frequency that the server updates the replicated TimeSeconds from the world. Set to zero to disable periodic updates. */
	UPROPERTY(EditDefaultsOnly, Category=GameState)
	float ServerWorldTimeSecondsUpdateFrequency;

	/** Handle for efficient management of the UpdateServerTimeSeconds timer */
	FTimerHandle TimerHandle_UpdateServerTimeSeconds;

private:
	// Hidden functions that don't make sense to use on this class.
	HIDE_ACTOR_TRANSFORM_FUNCTIONS();

	friend class UDemoNetDriver;
};



