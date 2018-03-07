// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "OnlineKeyValuePair.h"

class FUniqueNetId;

typedef FOnlineKeyValuePairs< FName, FVariantData > FOnlineEventParms;

/**
 *	IOnlineEvents - Interface class for events
 */
class IOnlineEvents
{
public:

	/**
	 * Trigger an event by name
	 *
	 * @param PlayerId	- Player to trigger the event for
	 * @param EventName - Name of the event
	 * @param Parms		- The parameter list to be passed into the event
	 *
	 * @return Whether the event was successfully triggered
	 */
	virtual bool TriggerEvent( const FUniqueNetId& PlayerId, const TCHAR* EventName, const FOnlineEventParms& Parms ) = 0;

	/**
	 * Quick way to send a valid PlayerSessionId with every event, required for Xbox One
	 *
	 * @param PlayerId the unique id of the player this session is associated with
	 * @param PlayerSessionId A GUID unique to this player session
	 */
	virtual void SetPlayerSessionId(const FUniqueNetId& PlayerId, const FGuid& PlayerSessionId) = 0;
};
