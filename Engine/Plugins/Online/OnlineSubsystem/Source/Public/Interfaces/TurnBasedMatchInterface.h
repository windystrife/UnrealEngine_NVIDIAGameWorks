// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Interface.h"
#include "TurnBasedMatchInterface.generated.h"

UINTERFACE(Blueprintable)
class UTurnBasedMatchInterface : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};

class ITurnBasedMatchInterface
{
	GENERATED_IINTERFACE_BODY()

public:
	UFUNCTION(BlueprintImplementableEvent, Category = "Online|TurnBased")
	void OnMatchReceivedTurn(const FString& Match, bool bDidBecomeActive);

	UFUNCTION(BlueprintImplementableEvent, Category = "Online|TurnBased")
	void OnMatchEnded(const FString& Match);
};
