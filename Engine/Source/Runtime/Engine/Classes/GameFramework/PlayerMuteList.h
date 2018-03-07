// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/CoreOnline.h"
#include "PlayerMuteList.generated.h"

struct FUniqueNetIdRepl;

/**
 * Container responsible for managing the mute state of a player controller
 * at the gameplay level (VoiceInterface handles actual muting)
 */
USTRUCT()
struct FPlayerMuteList
{
	GENERATED_USTRUCT_BODY()

public:

	FPlayerMuteList() :
		bHasVoiceHandshakeCompleted(false),
		VoiceChannelIdx(0)
	{
	}

	/** List of player id's muted explicitly by the player */
	TArray< TSharedRef<const FUniqueNetId> > VoiceMuteList;
	/** List of player id's muted for gameplay reasons (teams, spectators, etc) */
	TArray< TSharedRef<const FUniqueNetId> > GameplayVoiceMuteList;
	/** Combined list of the above for efficient processing of voice packets */
	TArray< TSharedRef<const FUniqueNetId> > VoicePacketFilter;
	/** Has server and client handshake completed */
	UPROPERTY()
	bool bHasVoiceHandshakeCompleted;
	UPROPERTY()
	int32 VoiceChannelIdx;

public:

	/**
	 * Tell the server to mute a given player
	 *
	 * @param OwningPC player controller that would like to mute another player
	 * @param MuteId player id that should be muted
	 */
	void ServerMutePlayer(class APlayerController* OwningPC, const FUniqueNetIdRepl& MuteId);

	/**
	 * Tell the server to unmute a given player
	 *
	 * @param OwningPC player controller that would like to unmute another player
	 * @param UnmuteId player id that should be unmuted
	 */
	void ServerUnmutePlayer(class APlayerController* OwningPC, const FUniqueNetIdRepl& UnmuteId);

	/**
	 * Tell the client to mute a given player
	 *
	 * @param OwningPC player controller that would like to mute another player
	 * @param MuteId player id that should be muted
	 */
	void ClientMutePlayer(class APlayerController* OwningPC, const FUniqueNetIdRepl& MuteId);

	/**
	 * Tell the client to unmute a given player
	 *
	 * @param OwningPC player controller that would like to unmute another player
	 * @param UnmuteId player id that should be unmuted
	 */
	void ClientUnmutePlayer(class APlayerController* OwningPC, const FUniqueNetIdRepl& UnmuteId);

	/**
	 * Server muting based on gameplay rules
	 *
	 * @param OwningPC player controller that would like to mute another player
	 * @param MuteId player id that should be muted
	 */
	void GameplayMutePlayer(class APlayerController* OwningPC, const FUniqueNetIdRepl& MuteId);

	/**
	 * Server unmuting based on gameplay rules
	 *
	 * @param OwningPC player controller that would like to unmute another player
	 * @param UnmuteId player id that should be unmuted
	 */
	void GameplayUnmutePlayer(class APlayerController* OwningPC, const FUniqueNetIdRepl& UnmuteId);

	/**
	 * Is a given player currently muted
	 * 
	 * @param PlayerId player to query mute state
	 *
	 * @return true if the playerid is muted, false otherwise
	 */
	bool IsPlayerMuted(const class FUniqueNetId& PlayerId);
};

/** Dump out information about all player controller mute state */
ENGINE_API FString DumpMutelistState(UWorld* World);
