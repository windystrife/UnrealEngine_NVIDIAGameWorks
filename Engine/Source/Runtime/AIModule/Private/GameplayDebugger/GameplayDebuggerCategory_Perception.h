// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_GAMEPLAY_DEBUGGER

#include "CoreMinimal.h"
#include "GameplayDebuggerCategory.h"

class AActor;
class APlayerController;

class FGameplayDebuggerCategory_Perception : public FGameplayDebuggerCategory
{
public:
	FGameplayDebuggerCategory_Perception();

	virtual void CollectData(APlayerController* OwnerPC, AActor* DebugActor) override;

	static TSharedRef<FGameplayDebuggerCategory> MakeInstance();
};

#endif // WITH_GAMEPLAY_DEBUGGER
