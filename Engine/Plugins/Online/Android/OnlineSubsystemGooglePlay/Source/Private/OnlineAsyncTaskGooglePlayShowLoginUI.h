// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "OnlineAsyncTaskManager.h"
#include "OnlineExternalUIInterface.h"
#include "OnlineAsyncTaskGooglePlayAuthAction.h"
#include "OnlineSubsystemGooglePlayPackage.h"
#include "AndroidPermissionFunctionLibrary.h"

THIRD_PARTY_INCLUDES_START
// begin missing GPG dependencies
#include <vector>
// end missing GPG depdencies
#include "gpg/types.h"
#include "gpg/player_manager.h"
THIRD_PARTY_INCLUDES_END


class FOnlineSubsystemGooglePlay;

class FOnlineAsyncTaskGooglePlayShowLoginUI : public FOnlineAsyncTaskGooglePlayAuthAction
{
public:
	/**
	 * Constructor.
	 *
	 * @param InSubsystem a pointer to the owning subsystem
	 * @param InPlayerId index of the player who's logging in
	 */
	FOnlineAsyncTaskGooglePlayShowLoginUI(FOnlineSubsystemGooglePlay* InSubsystem, int InPlayerId, const FOnLoginUIClosedDelegate& InDelegate);

	// FOnlineAsyncItem
	virtual FString ToString() const override { return TEXT("ShowLoginUI"); }
	virtual void Finalize() override;
	virtual void TriggerDelegates() override;

	/** Callback from JNI when Google Client is connected */
	void ProcessGoogleClientConnectResult(bool bInSuccessful, FString AccessToken);

	void OnPermissionRequestReturn(const TArray<FString>& Permissions, const TArray<bool>& GrantResults);


private:
	/** The subsystem is the only class that should be calling OnAuthActionFinished */
	friend class FOnlineSubsystemGooglePlay;

	// FOnlineAsyncTaskGooglePlayAuthAction
	virtual void OnAuthActionFinished(gpg::AuthOperation InOp, gpg::AuthStatus InStatus) override;
	virtual void OnFetchSelfResponse(const gpg::PlayerManager::FetchSelfResponse& SelfResponse);
	virtual void Start_OnTaskThread() override;

	int PlayerId;
	FOnLoginUIClosedDelegate Delegate;
};
