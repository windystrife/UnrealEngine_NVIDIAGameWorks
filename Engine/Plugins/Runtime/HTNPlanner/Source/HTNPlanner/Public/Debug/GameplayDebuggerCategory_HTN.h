// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_GAMEPLAY_DEBUGGER

#include "GameplayDebuggerCategory.h"

class ABotObjectiveGraph;

class FGameplayDebuggerCategory_HTN : public FGameplayDebuggerCategory
{
public:
	FGameplayDebuggerCategory_HTN();

	virtual void CollectData(APlayerController* OwnerPC, AActor* DebugActor) override;
	virtual void DrawData(APlayerController* OwnerPC, FGameplayDebuggerCanvasContext& CanvasContext) override;

	static TSharedRef<FGameplayDebuggerCategory> MakeInstance();

protected:
};

#endif // WITH_GAMEPLAY_DEBUGGER
