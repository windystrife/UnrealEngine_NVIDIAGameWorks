// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "OnlineAsyncTaskGooglePlayLogout.h"
#include "OnlineSubsystemGooglePlay.h"
#include "OnlineIdentityInterfaceGooglePlay.h"

THIRD_PARTY_INCLUDES_START
#include "gpg/game_services.h"
THIRD_PARTY_INCLUDES_END

FOnlineAsyncTaskGooglePlayLogout::FOnlineAsyncTaskGooglePlayLogout(
	FOnlineSubsystemGooglePlay* InSubsystem,
	int32 InPlayerId)
	: FOnlineAsyncTaskGooglePlayAuthAction(InSubsystem)
	, PlayerId(InPlayerId)
	, Status(gpg::AuthStatus::ERROR_NOT_AUTHORIZED)
{
}

void FOnlineAsyncTaskGooglePlayLogout::Start_OnTaskThread()
{
	// If we haven't created a GameServices object yet, do so.
	if (Subsystem->GameServicesPtr.get() == nullptr)
	{
		UE_LOG(LogOnline, Log, TEXT("FOnlineAsyncTaskGooglePlayLogout::Start_OnTaskThread: GameServicesPtr is null"));
		bWasSuccessful = false;
		bIsComplete = true;
		return;
	}

	Subsystem->GetGameServices()->SignOut();
}

void FOnlineAsyncTaskGooglePlayLogout::Finalize()
{
	// Async task manager owns the task and is responsible for cleaning it up.
	Subsystem->CurrentLogoutTask = nullptr;
}

void FOnlineAsyncTaskGooglePlayLogout::TriggerDelegates()
{
	Subsystem->GetIdentityInterface()->TriggerOnLogoutCompleteDelegates(PlayerId, bWasSuccessful);
}

void FOnlineAsyncTaskGooglePlayLogout::OnAuthActionFinished(gpg::AuthOperation InOp, gpg::AuthStatus InStatus)
{
	if (InOp == gpg::AuthOperation::SIGN_OUT)
	{
		extern void AndroidThunkCpp_GoogleClientDisconnect();
		AndroidThunkCpp_GoogleClientDisconnect();

		Status = InStatus;
		bWasSuccessful = true;
		bIsComplete = true;
	}
}
