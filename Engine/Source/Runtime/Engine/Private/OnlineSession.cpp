// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	OnlineSession.cpp: Online session related implementations
	(creating/joining/leaving/destroying sessions)
=============================================================================*/

#include "GameFramework/OnlineSession.h"
#include "EngineGlobals.h"
#include "Engine/Engine.h"

UOnlineSession::UOnlineSession(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UOnlineSession::HandleDisconnect(UWorld *World, UNetDriver *NetDriver)
{
	// Let the engine cleanup this disconnect
	GEngine->HandleDisconnect(World, NetDriver);
}
