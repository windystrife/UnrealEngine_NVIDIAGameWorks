// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "OculusIdentityCallbackProxy.h"
#include "OnlineSubsystemOculusPrivate.h"
#include "Online.h"

UOculusIdentityCallbackProxy::UOculusIdentityCallbackProxy(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer), LocalUserNum(0)
{
}

UOculusIdentityCallbackProxy* UOculusIdentityCallbackProxy::GetOculusIdentity(int32 LocalUserNum)
{
	UOculusIdentityCallbackProxy* Proxy = NewObject<UOculusIdentityCallbackProxy>();
	Proxy->LocalUserNum = LocalUserNum;
	Proxy->SetFlags(RF_StrongRefOnFrame);
	return Proxy;
}

void UOculusIdentityCallbackProxy::Activate()
{
	auto OculusIdentityInterface = Online::GetIdentityInterface(OCULUS_SUBSYSTEM);

	if (OculusIdentityInterface.IsValid())
	{
		DelegateHandle = Online::GetIdentityInterface()->AddOnLoginCompleteDelegate_Handle(
			0, 
			FOnLoginCompleteDelegate::CreateUObject(this, &UOculusIdentityCallbackProxy::OnLoginCompleteDelegate)
		);
		OculusIdentityInterface->AutoLogin(LocalUserNum);
	}
	else
	{
		UE_LOG_ONLINE(Warning, TEXT("Oculus platform service not available to get the Oculus ID."));
		OnFailure.Broadcast();
	}
}

void UOculusIdentityCallbackProxy::OnLoginCompleteDelegate(int32 Unused, bool bWasSuccessful, const FUniqueNetId& UserId, const FString& ErrorStr)
{
	Online::GetIdentityInterface()->ClearOnLoginCompleteDelegate_Handle(LocalUserNum, DelegateHandle);
	if (bWasSuccessful)
	{
		auto PlayerNickName = Online::GetIdentityInterface()->GetPlayerNickname(LocalUserNum);
		OnSuccess.Broadcast(UserId.ToString(), PlayerNickName);
	}
	else
	{
		OnFailure.Broadcast();
	}
}

