// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Templates/SubclassOf.h"
#include "GameFramework/Actor.h"
#include "UObject/CoreOnline.h"
#include "GameFramework/OnlineReplStructs.h"
#include "GameFramework/Info.h"
#include "PlayerState.generated.h"

/**
 * Struct containing one seconds worth of accumulated ping data (for averaging)
 * NOTE: Maximum PingCount is 7, and maximum PingSum is 8191 (1170*7)
 */
struct PingAvgData
{
	/** The sum of all accumulated pings (used to calculate avg later) */
	uint16	PingSum : 13;

	/** The number of accumulated pings */
	uint8	PingCount : 3;

	/** Default constructor */
	PingAvgData()
		: PingSum(0)
		, PingCount(0)
	{
	}
};

/**
 * A PlayerState is created for every player on a server (or in a standalone game).
 * PlayerStates are replicated to all clients, and contain network game relevant information about the player, such as playername, score, etc.
 */
UCLASS(BlueprintType, Blueprintable, notplaceable)
class ENGINE_API APlayerState : public AInfo
{
	GENERATED_UCLASS_BODY()

	/** Player's current score. */
	UPROPERTY(replicatedUsing=OnRep_Score, BlueprintReadOnly, Category=PlayerState)
	float Score;

	/** Replicated compressed ping for this player (holds ping in msec divided by 4) */
	UPROPERTY(replicated, BlueprintReadOnly, Category=PlayerState)
	uint8 Ping;

	/** Player name, or blank if none. */
	UPROPERTY(replicatedUsing=OnRep_PlayerName, BlueprintReadOnly, Category=PlayerState)
	FString PlayerName;

	/** Previous player name.  Saved on client-side to detect player name changes. */
	FString OldName;

	/** Unique net id number. Actual value varies based on current online subsystem, use it only as a guaranteed unique number per player. */
	UPROPERTY(replicated, BlueprintReadOnly, Category=PlayerState)
	int32 PlayerId;

	/** Whether this player is currently a spectator */
	UPROPERTY(replicated, BlueprintReadOnly, Category=PlayerState)
	uint32 bIsSpectator:1;

	/** Whether this player can only ever be a spectator */
	UPROPERTY(replicated)
	uint32 bOnlySpectator:1;

	/** True if this PlayerState is associated with an AIController */
	UPROPERTY(replicated, BlueprintReadOnly, Category=PlayerState)
	uint32 bIsABot:1;

	/** client side flag - whether this player has been welcomed or not (player entered message) */
	uint32 bHasBeenWelcomed:1;

	/** Means this PlayerState came from the GameMode's InactivePlayerArray */
	UPROPERTY(replicatedUsing=OnRep_bIsInactive)
	uint32 bIsInactive:1;

	/** indicates this is a PlayerState from the previous level of a seamless travel,
	 * waiting for the player to finish the transition before creating a new one
	 * this is used to avoid preserving the PlayerState in the InactivePlayerArray if the player leaves
	 */
	UPROPERTY(replicated)
	uint32 bFromPreviousLevel:1;

	/** Elapsed time on server when this PlayerState was first created.  */
	UPROPERTY(replicated)
	int32 StartTime;

	/** This is used for sending game agnostic messages that can be localized */
	UPROPERTY()
	TSubclassOf<class ULocalMessage> EngineMessageClass;

	/** Exact ping as float (rounded and compressed in replicated Ping) */
	float ExactPing;

	/** Used to match up InactivePlayerState with rejoining playercontroller. */
	UPROPERTY()
	FString SavedNetworkAddress;

	/** The id used by the network to uniquely identify a player.
	 * NOTE: the internals of this property should *never* be exposed to the player as it's transient
	 * and opaque in meaning (ie it might mean date/time followed by something else).
	 * It is OK to use and pass around this property, though. */
	UPROPERTY(replicatedUsing=OnRep_UniqueId)
	FUniqueNetIdRepl UniqueId; 

	/** The session that the player needs to join/remove from as it is created/leaves */
	FName SessionName;

private:
	/**
	 * Stores the last 4 seconds worth of ping data (one second per 'bucket').
	 * It is stored in this manner, to allow calculating a moving average,
	 * without using up a lot of space, while also being tolerant of changes in ping update frequency
	 */
	PingAvgData		PingBucket[4];

	/** The current PingBucket index that is being filled */
	uint8			CurPingBucket;

	/** The timestamp for when the current PingBucket began filling */
	float			CurPingBucketTimestamp;

public:
	/** Replication Notification Callbacks */
	UFUNCTION()
	virtual void OnRep_Score();

	UFUNCTION()
	virtual void OnRep_PlayerName();

	UFUNCTION()
	virtual void OnRep_bIsInactive();

	UFUNCTION()
	virtual void OnRep_UniqueId();

	//~ Begin AActor Interface
	virtual void PostInitializeComponents() override; 
	virtual void Destroyed() override;
	virtual void Reset() override;
	virtual FString GetHumanReadableName() const override;
	//~ End AActor Interface


	/** Called by Controller when its PlayerState is initially replicated. */
	virtual void ClientInitialize(class AController* C);

	/**
	 * Receives ping updates for the client (both clientside and serverside), from the net driver
	 * NOTE: This updates much more frequently clientside, thus the clientside ping will often be different to what the server displays
	 */
	virtual void UpdatePing(float InPing);

	/** Recalculates the replicated Ping value once per second (both clientside and serverside), based upon collected ping data */
	virtual void RecalculateAvgPing();

	/**
	 * Returns true if should broadcast player welcome/left messages.
	 * Current conditions: must be a human player a network game 
	 */
	virtual bool ShouldBroadCastWelcomeMessage(bool bExiting = false);

	/** set the player name to S */
	virtual void SetPlayerName(const FString& S);

	/** 
	 * Associate an online unique id with this player
	 * @param InUniqueId the unique id associated with this player
	 */
	virtual void SetUniqueId(const TSharedPtr<const FUniqueNetId>& InUniqueId);

	/** 
	 * Register a player with the online subsystem
	 * @param bWasFromInvite was this player invited directly
	 */
	virtual void RegisterPlayerWithSession(bool bWasFromInvite);

	/** Unregister a player with the online subsystem */
	virtual void UnregisterPlayerWithSession();

	/** Create duplicate PlayerState (for saving Inactive PlayerState)	*/
	virtual class APlayerState* Duplicate();

	/** Called on the server when the owning player has disconnected, by default this method destroys this player state */
	virtual void OnDeactivated();

	/** Called on the server when the owning player has reconnected and this player state is added to the active players array */
	virtual void OnReactivated();

	/** called by seamless travel when initializing a player on the other side - copy properties to the new PlayerState that should persist */
	virtual void SeamlessTravelTo(class APlayerState* NewPlayerState);

	/** return true if PlayerState is primary (ie. non-splitscreen) player */
	virtual bool IsPrimaryPlayer() const;

	virtual void GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const override;

	/** calls OverrideWith and triggers OnOverrideWith for BP extension */
	void DispatchOverrideWith(APlayerState* PlayerState);

	void DispatchCopyProperties(APlayerState* PlayerState);

protected:

	virtual void OverrideWith(APlayerState* PlayerState);

	/** Copy properties which need to be saved in inactive PlayerState */
	virtual void CopyProperties(APlayerState* PlayerState);

	/*
	* Can be implemented in Blueprint Child to move more properties from old to new PlayerState when reconnecting
	*
	* @param OldPlayerState		Old PlayerState, which we use to fill the new one with
	*/
	UFUNCTION(BlueprintImplementableEvent, Category = PlayerState, meta = (DisplayName = "OverrideWith"))
	void ReceiveOverrideWith(APlayerState* OldPlayerState);

	/*
	* Can be implemented in Blueprint Child to move more properties from old to new PlayerState when traveling to a new level
	*
	* @param NewPlayerState		New PlayerState, which we fill with the current properties
	*/
	UFUNCTION(BlueprintImplementableEvent, Category = PlayerState, meta = (DisplayName = "CopyProperties"))
	void ReceiveCopyProperties(APlayerState* NewPlayerState);

private:
	// Hidden functions that don't make sense to use on this class.
	HIDE_ACTOR_TRANSFORM_FUNCTIONS();
};



