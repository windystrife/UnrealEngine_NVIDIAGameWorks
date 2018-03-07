// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "OnlineAsyncTaskGooglePlayLogin.h"
#include "OnlineSubsystemGooglePlay.h"

THIRD_PARTY_INCLUDES_START
#include "gpg/builder.h"
#include "gpg/debug.h"
#include "gpg/types.h"
THIRD_PARTY_INCLUDES_END

FOnlineAsyncTaskGooglePlayLogin::FOnlineAsyncTaskGooglePlayLogin(
	FOnlineSubsystemGooglePlay* InSubsystem,
	int InPlayerId,
	const FOnCompletedDelegate& InDelegate)
	: FOnlineAsyncTaskGooglePlayAuthAction(InSubsystem)
	, PlayerId(InPlayerId)
	, Status(gpg::AuthStatus::ERROR_NOT_AUTHORIZED)
	, Delegate(InDelegate)
{
}

void FOnlineAsyncTaskGooglePlayLogin::Start_OnTaskThread()
{
	UE_LOG(LogOnline, Log, TEXT("FOnlineAsyncTaskGooglePlayLogin::Start_OnTaskThread"));

	// If we haven't created a GameServices object yet, do so.
	if (Subsystem->GameServicesPtr.get() == nullptr)
	{
		UE_LOG(LogOnline, Log, TEXT("FOnlineAsyncTaskGooglePlayLogin::Start_OnTaskThread initializing game services"));
		// Store the Subsystem pointer locally so that the OnAuthActionFinished lambda can capture it
		FOnlineSubsystemGooglePlay* LocalSubsystem = Subsystem;

		Subsystem->GameServicesPtr = gpg::GameServices::Builder()
			.SetDefaultOnLog(gpg::LogLevel::VERBOSE)
			.SetOnAuthActionStarted([](gpg::AuthOperation Op) {
				UE_LOG(LogOnline, Log, TEXT("GPG OnAuthActionStarted: %s"), *FString(DebugString(Op).c_str()));
			})
			.SetOnAuthActionFinished([LocalSubsystem](gpg::AuthOperation Op, gpg::AuthStatus LocalStatus) {
				UE_LOG(LogOnline, Log, TEXT("GPG OnAuthActionFinished: %s, AuthStatus: %s"),
					*FString(DebugString(Op).c_str()),
					*FString(DebugString(LocalStatus).c_str()));
				LocalSubsystem->OnAuthActionFinished(Op, LocalStatus);
			})
			.AddOauthScope("https://www.googleapis.com/auth/plus.login")
			.Create(Subsystem->PlatformConfiguration);
	}
	else if(Subsystem->GameServicesPtr->IsAuthorized())
	{
		UE_LOG(LogOnline, Log, TEXT("FOnlineAsyncTaskGooglePlayLogin::Start_OnTaskThread already authorized"));
		// We have a GameServices object and the user is authorized, nothing else to do.
		Status = gpg::AuthStatus::VALID;
		bWasSuccessful = true;
		bIsComplete = true;
	}
	else
	{
		UE_LOG(LogOnline, Log, TEXT("FOnlineAsyncTaskGooglePlayLogin::Start_OnTaskThread not authorized"));
		// We have created the GameServices object but the user isn't authorized.
		bWasSuccessful = false;
		bIsComplete = true;
	}
}

void FOnlineAsyncTaskGooglePlayLogin::Finalize()
{
	UE_LOG(LogOnline, Log, TEXT("FOnlineAsyncTaskGooglePlayLogin: Finalize."));
	// Async task manager owns the task and is responsible for cleaning it up.
	Subsystem->CurrentLoginTask = nullptr;
}

void FOnlineAsyncTaskGooglePlayLogin::TriggerDelegates()
{
	UE_LOG(LogOnline, Log, TEXT("FOnlineAsyncTaskGooglePlayLogin: TriggerDelegates. %d"), bWasSuccessful);
	Delegate.ExecuteIfBound();
}

void FOnlineAsyncTaskGooglePlayLogin::OnAuthActionFinished(gpg::AuthOperation InOp, gpg::AuthStatus InStatus)
{
	UE_LOG(LogOnline, Log, TEXT("FOnlineAsyncTaskGooglePlayLogin::OnAuthActionFinished %d %d"), (int32)InOp, (int32)Status);
	if (InOp == gpg::AuthOperation::SIGN_IN)
	{
		Status = InStatus;
		bWasSuccessful = Status == gpg::AuthStatus::VALID;
		bIsComplete = true;
	}
}
