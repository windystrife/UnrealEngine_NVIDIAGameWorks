// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "GlobalEditorNotification.h"
#include "UnrealClient.h"

void FGlobalEditorNotification::Tick(float DeltaTime)
{
	TickNotification(DeltaTime);
}

bool FGlobalEditorNotification::IsTickable() const
{
	return true;
}

TStatId FGlobalEditorNotification::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(FGlobalEditorNotification, STATGROUP_Tickables);
}
