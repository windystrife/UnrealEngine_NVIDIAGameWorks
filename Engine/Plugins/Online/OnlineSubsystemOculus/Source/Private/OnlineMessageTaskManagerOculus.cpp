// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "OnlineMessageTaskManagerOculus.h"
#include "OnlineSubsystemOculusPrivate.h"

void FOnlineMessageTaskManagerOculus::OnReceiveMessage(ovrMessageHandle Message)
{
	auto RequestId = ovr_Message_GetRequestID(Message);
	bool bIsError = ovr_Message_IsError(Message);

	if (RequestDelegates.Contains(RequestId))
	{
		RequestDelegates[RequestId].ExecuteIfBound(Message, bIsError);

		// Remove the delegate
		RequestDelegates[RequestId].Unbind();
		RequestDelegates.Remove(RequestId);
	}
	else
	{
		auto MessageType = ovr_Message_GetType(Message);
		if (NotifDelegates.Contains(MessageType))
		{
			if (!bIsError)
			{
				NotifDelegates[MessageType].Broadcast(Message, bIsError);
			}
		}
		else
		{
			UE_LOG_ONLINE(Verbose, TEXT("Unhandled request id: %llu"), RequestId);
		}
	}
	ovr_FreeMessage(Message);
}

void FOnlineMessageTaskManagerOculus::AddRequestDelegate(ovrRequest RequestId, FOculusMessageOnCompleteDelegate&& Delegate)
{
	RequestDelegates.Emplace(RequestId, Delegate);
}

FOculusMulticastMessageOnCompleteDelegate& FOnlineMessageTaskManagerOculus::GetNotifDelegate(ovrMessageType MessageType)
{
	return NotifDelegates.FindOrAdd(MessageType);
}

void FOnlineMessageTaskManagerOculus::RemoveNotifDelegate(ovrMessageType MessageType, const FDelegateHandle& Delegate)
{
	NotifDelegates.FindOrAdd(MessageType).Remove(Delegate);
}

bool FOnlineMessageTaskManagerOculus::Tick(float DeltaTime)
{
	for (;;)
	{
		auto Message = ovr_PopMessage();
		if (!Message)
		{
			break;
		}
		OnReceiveMessage(Message);
	}
	if (DeltaTime > 4.0f) 
	{
		UE_LOG_ONLINE(Warning, TEXT("DeltaTime was %f seconds.  Time sensitive oculus notifications may time out."), DeltaTime);
	}
	return true;
}
