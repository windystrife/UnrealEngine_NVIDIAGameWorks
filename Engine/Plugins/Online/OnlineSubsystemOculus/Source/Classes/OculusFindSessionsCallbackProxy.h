// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Object.h"
#include "OnlineBlueprintCallProxyBase.h"
#include "OnlineSessionInterface.h"
#include "FindSessionsCallbackProxy.h"
#include "OculusFindSessionsCallbackProxy.generated.h"

/**
 * Exposes FindSession of the Platform SDK for blueprint use.
 */
UCLASS(MinimalAPI)
class UOculusFindSessionsCallbackProxy : public UOnlineBlueprintCallProxyBase
{
	GENERATED_UCLASS_BODY()

	// Called when there is a successful query
	UPROPERTY(BlueprintAssignable)
	FBlueprintFindSessionsResultDelegate OnSuccess;

	// Called when there is an unsuccessful query
	UPROPERTY(BlueprintAssignable)
	FBlueprintFindSessionsResultDelegate OnFailure;

	// Searches for matchmaking room sessions with the oculus online subsystem
	UFUNCTION(BlueprintCallable, Category = "Oculus|Session", meta = (BlueprintInternalUseOnly = "true"))
	static UOculusFindSessionsCallbackProxy* FindMatchmakingSessions(int32 MaxResults, FString OculusMatchmakingPool);

	// Searches for moderated room sessions with the oculus online subsystem
	UFUNCTION(BlueprintCallable, Category = "Oculus|Session", meta = (BlueprintInternalUseOnly = "true"))
	static UOculusFindSessionsCallbackProxy* FindModeratedSessions(int32 MaxResults);

	// UOnlineBlueprintCallProxyBase interface
	virtual void Activate() override;
	// End of UOnlineBlueprintCallProxyBase interface

private:
	// Internal callback when the session search completes, calls out to the public success/failure callbacks
	void OnCompleted(bool bSuccess);

private:

	// The delegate executed by the online subsystem
	FOnFindSessionsCompleteDelegate Delegate;

	// Handle to the registered OnFindSessionsComplete delegate
	FDelegateHandle DelegateHandle;

	// Object to track search results
	TSharedPtr<FOnlineSessionSearch> SearchObject;

	// Maximum number of results to return
	int MaxResults;

	// Optional: if searching within a matchmaking pool
	FString OculusPool;

	bool bSearchModeratedRoomsOnly;
};
