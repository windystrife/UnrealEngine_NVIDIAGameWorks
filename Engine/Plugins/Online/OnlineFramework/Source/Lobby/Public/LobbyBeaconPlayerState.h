// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "GameFramework/Info.h"
#include "GameFramework/OnlineReplStructs.h"
#include "LobbyBeaconPlayerState.generated.h"

class AOnlineBeaconClient;

/**
 * Lightweight representation of a player while connected to the game through the lobby
 * exists for the lifetime of a player whether they are in the lobby or not
 * assumption that the data here doesn't change often and locks when they actually join the server
 */
UCLASS(transient, config = Game, notplaceable)
class LOBBY_API ALobbyBeaconPlayerState : public AInfo
{
	GENERATED_UCLASS_BODY()

	//~ Begin AActor Interface
	virtual void PostInitializeComponents() override;
	virtual bool IsNetRelevantFor(const AActor* RealViewer, const AActor* ViewTarget, const FVector& SrcLocation) const override;
	//~ End AActor Interface

	/** @return true if this data structure is valid */
	bool IsValid() const;

	/** Visible friendly player name */
	UPROPERTY(Replicated)
	FText DisplayName;

	/** Player unique id */
	UPROPERTY(Replicated)
	FUniqueNetIdRepl UniqueId;

    /** Party owner id */
	UPROPERTY(ReplicatedUsing = OnRep_PartyOwner)
	FUniqueNetIdRepl PartyOwnerUniqueId;

	/** Is the player in the lobby or game */
	UPROPERTY( ReplicatedUsing = OnRep_InLobby )
	bool bInLobby;

	/** Reference to the beacon actor related to this player */
	UPROPERTY(Replicated)
	AOnlineBeaconClient* ClientActor;

	/**
	 * Delegate fired when this player state has changed in some way
	 *
	 * @param UniqueId id of the player that changed
	 */
	DECLARE_EVENT_OneParam( ALobbyBeaconPlayerState, FOnPlayerStateChanged, const FUniqueNetIdRepl& /** UniqueId */ );

	/** @return delegate fired when the state of the player has changed in some way */
	FORCEINLINE FOnPlayerStateChanged& OnPlayerStateChanged() { return PlayerStateChangedEvent; }
	/** @return delegate fired when the party owner of this player has changed */
	FORCEINLINE FOnPlayerStateChanged& OnPartyOwnerChanged() { return PartyOwnerChangedEvent; }

protected:
	
	/** Party owner has changed */
	UFUNCTION()
	void OnRep_PartyOwner();

	/** Player has joined or left the lobby */
	UFUNCTION()
	void OnRep_InLobby();

private:

	/** Delegate fired when player state changes */
	FOnPlayerStateChanged PlayerStateChangedEvent;
	/** Delegate fired when party owner changes */
	FOnPlayerStateChanged PartyOwnerChangedEvent;
};
