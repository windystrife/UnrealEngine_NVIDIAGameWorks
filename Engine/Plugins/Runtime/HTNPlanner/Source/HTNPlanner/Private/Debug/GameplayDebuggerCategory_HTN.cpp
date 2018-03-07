// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "Debug/GameplayDebuggerCategory_HTN.h"

#if WITH_GAMEPLAY_DEBUGGER

FGameplayDebuggerCategory_HTN::FGameplayDebuggerCategory_HTN()
{
}

TSharedRef<FGameplayDebuggerCategory> FGameplayDebuggerCategory_HTN::MakeInstance()
{
	return MakeShareable(new FGameplayDebuggerCategory_HTN());
}

void FGameplayDebuggerCategory_HTN::CollectData(APlayerController* OwnerPC, AActor* DebugActor)
{
}

void FGameplayDebuggerCategory_HTN::DrawData(APlayerController* OwnerPC, FGameplayDebuggerCanvasContext& CanvasContext)
{

}

#endif // WITH_GAMEPLAY_DEBUGGER
