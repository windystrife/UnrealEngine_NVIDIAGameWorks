// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/**
 * Player: Corresponds to a real player (a local camera or remote net player).
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Player.generated.h"

class APlayerController;

UCLASS(MinimalAPI, transient, config=Engine)
class UPlayer : public UObject, public FExec
{
	GENERATED_UCLASS_BODY()

	/** The actor this player controls. */
	UPROPERTY(transient)
	class APlayerController* PlayerController;

	// Net variables.
	/** the current speed of the connection */
	UPROPERTY()
	int32 CurrentNetSpeed;

	/** @todo document */
	UPROPERTY(globalconfig)
	int32 ConfiguredInternetSpeed;
	
	/** @todo document */
	UPROPERTY(globalconfig)
	int32 ConfiguredLanSpeed;

public:
	//~ Begin FExec Interface.
	ENGINE_API virtual bool Exec( UWorld* InWorld, const TCHAR* Cmd,FOutputDevice& Ar) override;
	//~ End FExec Interface.

	/**
	 * Dynamically assign Controller to Player and set viewport.
	 *
	 * @param    PC - new player controller to assign to player
	 */
	ENGINE_API virtual void SwitchController( class APlayerController* PC );

	/**
	 * Executes the Exec() command
	 *
	 * @param Command command to execute (string of commands optionally separated by a | (pipe))
	 * @param bWriteToLog write out to the log
	 */
	ENGINE_API FString ConsoleCommand(const FString& Cmd, bool bWriteToLog = true);

	/**
	 * Gets the player controller in the given world for this player.
	 *
	 * @param InWorld The world in which to search for player controllers.
	 *
	 * @return The controller associated with this player in InWorld, if one exists.
	 */
	ENGINE_API APlayerController* GetPlayerController(UWorld* InWorld) const;
};
