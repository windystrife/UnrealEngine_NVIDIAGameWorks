// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "EndTurnCallbackProxy.h"
#include "Serialization/BitWriter.h"
#include "EngineGlobals.h"
#include "Engine/Engine.h"
#include "OnlineSubsystemBPCallHelper.h"
#include "Interfaces/OnlineTurnBasedInterface.h"
#include "OnlineSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "Net/RepLayout.h"

//////////////////////////////////////////////////////////////////////////
// UTurnBasedMatchEndTurnCallbackProxy

UEndTurnCallbackProxy::UEndTurnCallbackProxy(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
}

UEndTurnCallbackProxy::~UEndTurnCallbackProxy()
{
}

UEndTurnCallbackProxy* UEndTurnCallbackProxy::EndTurn(UObject* WorldContextObject, class APlayerController* PlayerController, FString MatchID, TScriptInterface<ITurnBasedMatchInterface> TurnBasedMatchInterface)
{
	UEndTurnCallbackProxy* Proxy = NewObject<UEndTurnCallbackProxy>();
	Proxy->PlayerControllerWeakPtr = PlayerController;
	Proxy->WorldContextObject = WorldContextObject;
	Proxy->MatchID = MatchID;
	Proxy->TurnBasedMatchInterface = (UTurnBasedMatchInterface*)TurnBasedMatchInterface.GetObject();
	return Proxy;
}

void UEndTurnCallbackProxy::Activate()
{
	FOnlineSubsystemBPCallHelper Helper(TEXT("ConnectToService"), WorldContextObject);
	Helper.QueryIDFromPlayerController(PlayerControllerWeakPtr.Get());

	if (Helper.IsValid())
	{
		IOnlineTurnBasedPtr TurnBasedInterface = Helper.OnlineSub->GetTurnBasedInterface();
		if (TurnBasedInterface.IsValid())
		{
			if (TurnBasedMatchInterface != nullptr)
			{
				FRepLayout RepLayout;
				RepLayout.InitFromObjectClass(TurnBasedMatchInterface->GetClass());
				FBitWriter Writer(TurnBasedInterface->GetMatchDataSize());
				RepLayout.SerializeObjectReplicatedProperties(TurnBasedMatchInterface, Writer);

				FUploadMatchDataSignature UploadMatchDataSignature;
				UploadMatchDataSignature.BindUObject(this, &UEndTurnCallbackProxy::UploadMatchDataDelegate);

				FTurnBasedMatchPtr TurnBasedMatchPtr = TurnBasedInterface->GetMatchWithID(MatchID);
				if (TurnBasedMatchPtr.IsValid())
				{
					TurnBasedMatchPtr->EndTurnWithMatchData(*Writer.GetBuffer(), 0, UploadMatchDataSignature);
					return;
				}

				FString Message = FString::Printf(TEXT("Match ID %s not found"), *MatchID);
				FFrame::KismetExecutionMessage(*Message, ELogVerbosity::Warning);
			}
			else
			{
				FFrame::KismetExecutionMessage(TEXT("No match data passed in to End Turn."), ELogVerbosity::Warning);
			}
			return;
		}
		else
		{
			FFrame::KismetExecutionMessage(TEXT("Turn based games not supported by online subsystem"), ELogVerbosity::Warning);
		}
	}

	// Fail immediately
	OnFailure.Broadcast();
}

void UEndTurnCallbackProxy::UploadMatchDataDelegate(FString InMatchID, bool Succeeded)
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
