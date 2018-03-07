// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"

/** Secondary condition to check before considering the replication of a lifetime property. */
UENUM(BlueprintType)
enum ELifetimeCondition
{
	COND_None = 0							UMETA(DisplayName = "None"),							// This property has no condition, and will send anytime it changes
	COND_InitialOnly = 1					UMETA(DisplayName = "Initial Only"),					// This property will only attempt to send on the initial bunch
	COND_OwnerOnly = 2						UMETA(DisplayName = "Owner Only"),						// This property will only send to the actor's owner
	COND_SkipOwner = 3						UMETA(DisplayName = "Skip Owner"),						// This property send to every connection EXCEPT the owner
	COND_SimulatedOnly = 4					UMETA(DisplayName = "Simulated Only"),					// This property will only send to simulated actors
	COND_AutonomousOnly = 5					UMETA(DisplayName = "Autonomous Only"),					// This property will only send to autonomous actors
	COND_SimulatedOrPhysics = 6				UMETA(DisplayName = "Simulated Or Physics"),			// This property will send to simulated OR bRepPhysics actors
	COND_InitialOrOwner = 7					UMETA(DisplayName = "Initial Or Owner"),				// This property will send on the initial packet, or to the actors owner
	COND_Custom = 8							UMETA(DisplayName = "Custom"),							// This property has no particular condition, but wants the ability to toggle on/off via SetCustomIsActiveOverride
	COND_ReplayOrOwner = 9					UMETA(DisplayName = "Replay Or Owner"),					// This property will only send to the replay connection, or to the actors owner
	COND_ReplayOnly = 10					UMETA(DisplayName = "Replay Only"),						// This property will only send to the replay connection
	COND_SimulatedOnlyNoReplay = 11			UMETA(DisplayName = "Simulated Only No Replay"),		// This property will send to actors only, but not to replay connections
	COND_SimulatedOrPhysicsNoReplay = 12	UMETA(DisplayName = "Simulated Or Physics No Replay"),	// This property will send to simulated Or bRepPhysics actors, but not to replay connections
	COND_SkipReplay = 13					UMETA(DisplayName = "Skip Replay"),	// This property will not send to the replay connection
	COND_Max = 14							UMETA(Hidden)
};


enum ELifetimeRepNotifyCondition
{
	REPNOTIFY_OnChanged = 0,		// Only call the property's RepNotify function if it changes from the local value
	REPNOTIFY_Always = 1,		// Always Call the property's RepNotify function when it is received from the server
};
