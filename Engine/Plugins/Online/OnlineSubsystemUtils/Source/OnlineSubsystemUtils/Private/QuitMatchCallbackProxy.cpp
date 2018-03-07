// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "QuitMatchCallbackProxy.h"
#include "EngineGlobals.h"
#include "Engine/Engine.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemBPCallHelper.h"
#include "GameFramework/PlayerController.h"

UQuitMatchCallbackProxy::UQuitMatchCallbackProxy(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UQuitMatchCallbackProxy::~UQuitMatchCallbackProxy()
{
}

UQuitMatchCallbackProxy* UQuitMatchCallbackProxy::QuitMatch(UObject* WorldContextObject, APlayerController* PlayerController, FString MatchID, EMPMatchOutcome::Outcome Outcome, int32 TurnTimeoutInSeconds)
{
	UQuitMatchCallbackProxy* Proxy = NewObject<UQuitMatchCallbackProxy>();
	Proxy->PlayerControllerWeakPtr = PlayerController;
	Proxy->WorldContextObject = WorldContextObject;
    Proxy->MatchID = MatchID;
	Proxy->Outcome = Outcome;
	Proxy->TurnTimeoutInSeconds = TurnTimeoutInSeconds;
	return Proxy;
}

void UQuitMatchCallbackProxy::Activate()
{
	FOnlineSubsystemBPCallHelper Helper(TEXT("ConnectToService"), WorldContextObject);
	Helper.QueryIDFromPlayerController(PlayerControllerWeakPtr.Get());

	if (Helper.IsValid())
	{
		IOnlineTurnBasedPtr OnlineTurnBasedPtr = Helper.OnlineSub->GetTurnBasedInterface();
		if (OnlineTurnBasedPtr.IsValid())
		{
			FTurnBasedMatchPtr Match = OnlineTurnBasedPtr->GetMatchWithID(MatchID);
			if (Match.IsValid())
			{
				FQuitMatchSignature QuitMatchSignature;
				QuitMatchSignature.BindUObject(this, &UQuitMatchCallbackProxy::QuitMatchDelegate);
				Match->QuitMatch(Outcome, TurnTimeoutInSeconds, QuitMatchSignature);
                return;
			}
			else
			{
				FString Message = FString::Printf(TEXT("Match ID %s not found"), *MatchID);
				FFrame::KismetExecutionMessage(*Message, ELogVerbosity::Warning);
			}
		}
		else
		{
			FFrame::KismetExecutionMessage(TEXT("Turn Based Matches not supported by Online Subsystem"), ELogVerbosity::Warning);
		}
	}

	// Fail immediately
	OnFailure.Broadcast();
}

void UQuitMatchCallbackProxy::QuitMatchDelegate(FString InMatchID, bool Succeeded)
{
	if (Succeeded)
	{
		OnSuccess.Broadcast();
	}
	else
	{
		OnFailure.Broadcast();
	}
}
