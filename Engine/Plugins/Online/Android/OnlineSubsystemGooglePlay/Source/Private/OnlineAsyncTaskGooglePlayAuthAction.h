// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "OnlineAsyncTaskManager.h"
#include "OnlineSubsystemGooglePlayPackage.h"

#include "gpg/status.h"
#include "gpg/types.h"

class FOnlineSubsystemGooglePlay;

class FOnlineAsyncTaskGooglePlayAuthAction : public FOnlineAsyncTaskBasic<FOnlineSubsystemGooglePlay>
{
public:
	/**
	 * Constructor.
	 *
	 * @param InSubsystem a pointer to the owning subsystem
	 * @param InPlayerId index of the player who's logging in
	 */
	FOnlineAsyncTaskGooglePlayAuthAction(FOnlineSubsystemGooglePlay* InSubsystem);

	// FOnlineAsyncTask
	virtual void Tick() override;

	// FOnlineAsyncItem
	virtual FString ToString() const override { return TEXT("AuthAction"); }

private:
	/**
	 * The OnAuthActionFinished callback is handled globally in FOnlineSubsystemGooglePlay.
	 * The subsystem is responsible for forwarding the call to any pending auth task.
	 *
	 * @param InOp Forwarded from Google's callback, indicates whether this was a sign-in or a sign-out
	 * @param InStatus Forwarded from Google's callback, indicates whether the operation succeeded
	 */
	virtual void OnAuthActionFinished(gpg::AuthOperation InOp, gpg::AuthStatus InStatus) = 0;

	/**
	 * This function will be called once during the task's first tick on the online thread.
	 * Usually this is the best place to make the system API call to start the async operation.
	 */
	virtual void Start_OnTaskThread() {}

	/** Tracks whether or not this task has ticked at least once */
	bool bInit;
};
