// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "FindSessionsCallbackProxy.h"
#include "EngineGlobals.h"
#include "Engine/Engine.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemBPCallHelper.h"
#include "GameFramework/PlayerController.h"

//////////////////////////////////////////////////////////////////////////
// UFindSessionsCallbackProxy

UFindSessionsCallbackProxy::UFindSessionsCallbackProxy(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, Delegate(FOnFindSessionsCompleteDelegate::CreateUObject(this, &ThisClass::OnCompleted))
	, bUseLAN(false)
{
}

UFindSessionsCallbackProxy* UFindSessionsCallbackProxy::FindSessions(UObject* WorldContextObject, class APlayerController* PlayerController, int MaxResults, bool bUseLAN)
{
	UFindSessionsCallbackProxy* Proxy = NewObject<UFindSessionsCallbackProxy>();
	Proxy->PlayerControllerWeakPtr = PlayerController;
	Proxy->bUseLAN = bUseLAN;
	Proxy->MaxResults = MaxResults;
	Proxy->WorldContextObject = WorldContextObject;
	return Proxy;
}

void UFindSessionsCallbackProxy::Activate()
{
	FOnlineSubsystemBPCallHelper Helper(TEXT("FindSessions"), WorldContextObject);
	Helper.QueryIDFromPlayerController(PlayerControllerWeakPtr.Get());

	if (Helper.IsValid())
	{
		auto Sessions = Helper.OnlineSub->GetSessionInterface();
		if (Sessions.IsValid())
		{
			DelegateHandle = Sessions->AddOnFindSessionsCompleteDelegate_Handle(Delegate);
			
			SearchObject = MakeShareable(new FOnlineSessionSearch);
			SearchObject->MaxSearchResults = MaxResults;
			SearchObject->bIsLanQuery = bUseLAN;
			SearchObject->QuerySettings.Set(SEARCH_PRESENCE, true, EOnlineComparisonOp::Equals);

			Sessions->FindSessions(*Helper.UserID, SearchObject.ToSharedRef());

			// OnQueryCompleted will get called, nothing more to do now
			return;
		}
		else
		{
			FFrame::KismetExecutionMessage(TEXT("Sessions not supported by Online Subsystem"), ELogVerbosity::Warning);
		}
	}

	// Fail immediately
	TArray<FBlueprintSessionResult> Results;
	OnFailure.Broadcast(Results);
}

void UFindSessionsCallbackProxy::OnCompleted(bool bSuccess)
{
	FOnlineSubsystemBPCallHelper Helper(TEXT("FindSessionsCallback"), WorldContextObject);
	Helper.QueryIDFromPlayerController(PlayerControllerWeakPtr.Get());

	if (Helper.IsValid())
	{
		auto Sessions = Helper.OnlineSub->GetSessionInterface();
		if (Sessions.IsValid())
		{
			Sessions->ClearOnFindSessionsCompleteDelegate_Handle(DelegateHandle);
		}
	}

	TArray<FBlueprintSessionResult> Results;

	if (bSuccess && SearchObject.IsValid())
	{
		for (auto& Result : SearchObject->SearchResults)
		{
			FBlueprintSessionResult BPResult;
			BPResult.OnlineResult = Result;
			Results.Add(BPResult);
		}

		OnSuccess.Broadcast(Results);
	}
	else
	{
		OnFailure.Broadcast(Results);
	}
}

int32 UFindSessionsCallbackProxy::GetPingInMs(const FBlueprintSessionResult& Result)
{
	return Result.OnlineResult.PingInMs;
}

FString UFindSessionsCallbackProxy::GetServerName(const FBlueprintSessionResult& Result)
{
	return Result.OnlineResult.Session.OwningUserName;
}

int32 UFindSessionsCallbackProxy::GetCurrentPlayers(const FBlueprintSessionResult& Result)
{
	return Result.OnlineResult.Session.SessionSettings.NumPublicConnections - Result.OnlineResult.Session.NumOpenPublicConnections;
}

int32 UFindSessionsCallbackProxy::GetMaxPlayers(const FBlueprintSessionResult& Result)
{
	return Result.OnlineResult.Session.SessionSettings.NumPublicConnections;
}
