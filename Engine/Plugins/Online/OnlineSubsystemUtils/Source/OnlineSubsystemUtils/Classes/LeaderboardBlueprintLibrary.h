// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "LeaderboardBlueprintLibrary.generated.h"

class APlayerController;

/**
 * A beacon host used for taking reservations for an existing game session
 */
UCLASS()
class ONLINESUBSYSTEMUTILS_API ULeaderboardBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

	/** Writes an integer value to the specified leaderboard */
	UFUNCTION(BlueprintCallable, Category = "Online|Leaderboard")
	static bool WriteLeaderboardInteger(APlayerController* PlayerController, FName StatName, int32 StatValue);

private:
	static bool WriteLeaderboardObject(APlayerController* PlayerController, class FOnlineLeaderboardWrite& WriteObject);
};
