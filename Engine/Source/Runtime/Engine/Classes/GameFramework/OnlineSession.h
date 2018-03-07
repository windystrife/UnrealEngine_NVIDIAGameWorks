// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/**
 * Everything a local player will use to manage an online session.
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/CoreOnline.h"
#include "OnlineSession.generated.h"

class FOnlineSessionSearchResult;

UCLASS(config=Game)
class ENGINE_API UOnlineSession : public UObject
{
	GENERATED_UCLASS_BODY()

public:

	/** Register all delegates needed to manage online sessions.  */
	virtual void RegisterOnlineDelegates() {};

	/** Tear down all delegates used to manage online sessions.	 */
	virtual void ClearOnlineDelegates() {};

	/** Called to tear down any online sessions and return to main menu	 */
	virtual void HandleDisconnect(UWorld *World, class UNetDriver *NetDriver);

	/** Start the online session specified */
	virtual void StartOnlineSession(FName SessionName) {};

	/** End the online session specified */
	virtual void EndOnlineSession(FName SessionName) {};

	/** Called when a user accepts an invite */
	virtual void OnSessionUserInviteAccepted(const bool bWasSuccess, const int32 ControllerId, TSharedPtr< const FUniqueNetId > UserId, const FOnlineSessionSearchResult & InviteResult) {};

	/** Called when the play together system event is received on PS4 */
	virtual void OnPlayTogetherEventReceived(int32 UserIndex, TArray<TSharedPtr<const FUniqueNetId>> UserIdList) {};
};



