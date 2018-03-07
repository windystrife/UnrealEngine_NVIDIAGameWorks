// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "OculusFindSessionsCallbackProxy.h"
#include "OnlineSubsystemOculusPrivate.h"
#include "Online.h"
#include "OnlineSessionInterfaceOculus.h"
#include "OnlineSubsystemOculusPrivate.h"

UOculusFindSessionsCallbackProxy::UOculusFindSessionsCallbackProxy(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	  , Delegate(FOnFindSessionsCompleteDelegate::CreateUObject(this, &ThisClass::OnCompleted))
	  , MaxResults(0)
	  , bSearchModeratedRoomsOnly(false)
{
}

UOculusFindSessionsCallbackProxy* UOculusFindSessionsCallbackProxy::FindMatchmakingSessions(int32 MaxResults, FString OculusMatchmakingPool)
{
	UOculusFindSessionsCallbackProxy* Proxy = NewObject<UOculusFindSessionsCallbackProxy>();
	Proxy->SetFlags(RF_StrongRefOnFrame);
	Proxy->MaxResults = MaxResults;
	Proxy->OculusPool = MoveTemp(OculusMatchmakingPool);
	Proxy->bSearchModeratedRoomsOnly = false;
	return Proxy;
}

UOculusFindSessionsCallbackProxy* UOculusFindSessionsCallbackProxy::FindModeratedSessions(int32 MaxResults)
{
	UOculusFindSessionsCallbackProxy* Proxy = NewObject<UOculusFindSessionsCallbackProxy>();
	Proxy->SetFlags(RF_StrongRefOnFrame);
	Proxy->MaxResults = MaxResults;
	Proxy->bSearchModeratedRoomsOnly = true;
	return Proxy;
}

void UOculusFindSessionsCallbackProxy::Activate()
{
	auto OculusSessionInterface = Online::GetSessionInterface(OCULUS_SUBSYSTEM);

	if (OculusSessionInterface.IsValid())
	{
		DelegateHandle = OculusSessionInterface->AddOnFindSessionsCompleteDelegate_Handle(Delegate);

		SearchObject = MakeShareable(new FOnlineSessionSearch);
		SearchObject->MaxSearchResults = MaxResults;
		SearchObject->QuerySettings.Set(SEARCH_OCULUS_MODERATED_ROOMS_ONLY, bSearchModeratedRoomsOnly, EOnlineComparisonOp::Equals);

		if (!OculusPool.IsEmpty())
		{
			SearchObject->QuerySettings.Set(SETTING_OCULUS_POOL, OculusPool, EOnlineComparisonOp::Equals);
		}

		OculusSessionInterface->FindSessions(0, SearchObject.ToSharedRef());
	}
	else
	{
		UE_LOG_ONLINE(Error, TEXT("Oculus platform service not available. Skipping FindSessions."));
		TArray<FBlueprintSessionResult> Results;
		OnFailure.Broadcast(Results);
	}
}

void UOculusFindSessionsCallbackProxy::OnCompleted(bool bSuccess)
{
	auto OculusSessionInterface = Online::GetSessionInterface(OCULUS_SUBSYSTEM);

	if (OculusSessionInterface.IsValid())
	{
		OculusSessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(DelegateHandle);
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