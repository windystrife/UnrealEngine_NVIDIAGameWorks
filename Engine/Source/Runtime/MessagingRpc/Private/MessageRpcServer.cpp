// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MessageRpcServer.h"

#include "Async/IAsyncProgress.h"
#include "Containers/ArrayBuilder.h"
#include "Containers/Ticker.h"
#include "IMessageRpcHandler.h"
#include "MessageEndpoint.h"
#include "MessageEndpointBuilder.h"

#include "RpcMessage.h"
#include "MessageRpcDefines.h"
#include "MessageRpcMessages.h"


/* FMessageRpcServer structors
 *****************************************************************************/

FMessageRpcServer::FMessageRpcServer()
{
	MessageEndpoint = FMessageEndpoint::Builder("FMessageRpcServer")
		.WithCatchall(this, &FMessageRpcServer::HandleMessage);

	TickerHandle = FTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FMessageRpcServer::HandleTicker), 0.1f);
}


FMessageRpcServer::~FMessageRpcServer()
{
	FTicker::GetCoreTicker().RemoveTicker(TickerHandle);
}


/* IMessageRpcServer interface
 *****************************************************************************/

TSharedPtr<FMessageEndpoint, ESPMode::ThreadSafe> FMessageRpcServer::GetEndpoint() const
{
	return MessageEndpoint;
}

void FMessageRpcServer::AddHandler(const FName& RequestMessageType, const TSharedRef<IMessageRpcHandler>& Handler)
{
	Handlers.Add(RequestMessageType, Handler);
}


const FMessageAddress& FMessageRpcServer::GetAddress() const
{
	return MessageEndpoint->GetAddress();
}


FOnMessageRpcNoHandler& FMessageRpcServer::OnNoHandler()
{
	return NoHandlerDelegate;
}


/* FMessageRpcServer implementation
 *****************************************************************************/

void FMessageRpcServer::ProcessCancelation(const FMessageRpcCancel& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context)
{
	FReturnInfo ReturnInfo;

	if (Returns.RemoveAndCopyValue(Message.CallId, ReturnInfo))
	{
		ReturnInfo.Return->Cancel();
	}
}


void FMessageRpcServer::ProcessRequest(const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context)
{
	auto Message = (FRpcMessage*)Context->GetMessage();
	const FName MessageType = Context->GetMessageType();

	if (!Handlers.Contains(MessageType))
	{
		if (!NoHandlerDelegate.IsBound())
		{
			return;
		}

		NoHandlerDelegate.Execute(MessageType);
	}

	auto Handler = Handlers.FindRef(MessageType);

	if (Handler.IsValid())
	{
		FReturnInfo ReturnInfo;
		{
			ReturnInfo.ClientAddress = Context->GetSender();
			ReturnInfo.LastProgressSent = FDateTime::UtcNow();
			ReturnInfo.Return = Handler->HandleRequest(Context);
		}

		Returns.Add(Message->CallId, ReturnInfo);
	}
	else
	{
		// notify caller that call was not handled
		MessageEndpoint->Send(new FMessageRpcUnhandled(Message->CallId), Context->GetSender());
	}
}


void FMessageRpcServer::SendProgress(const FGuid& CallId, const FReturnInfo& ReturnInfo)
{
	const TSharedPtr<IAsyncProgress>& Progress = ReturnInfo.Progress;
	const TSharedPtr<IAsyncTask>& Task = ReturnInfo.Task;

	MessageEndpoint->Send(
		new FMessageRpcProgress(
			CallId,
			Progress.IsValid() ? Progress->GetCompletion().Get(-1.0f) : -1.0f,
			Progress.IsValid() ? Progress->GetStatusText() : FText::GetEmpty()
		),
		ReturnInfo.ClientAddress
	);
}


void FMessageRpcServer::SendResult(const FGuid& CallId, const FReturnInfo& ReturnInfo)
{
	FRpcMessage* Message = ReturnInfo.Return->CreateResponseMessage();
	Message->CallId = CallId;

	MessageEndpoint->Send(
		Message,
		ReturnInfo.Return->GetResponseTypeInfo(),
		nullptr,
		TArrayBuilder<FMessageAddress>().Add(ReturnInfo.ClientAddress),
		FTimespan::Zero(),
		FDateTime::MaxValue()
	);
}


/* FMessageRpcServer event handlers
 *****************************************************************************/

void FMessageRpcServer::HandleMessage(const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context)
{
	const TWeakObjectPtr<UScriptStruct>& MessageTypeInfo = Context->GetMessageTypeInfo();

	if (!MessageTypeInfo.IsValid())
	{
		return;
	}

	if (MessageTypeInfo == FMessageRpcCancel::StaticStruct())
	{
		ProcessCancelation(*static_cast<const FMessageRpcCancel*>(Context->GetMessage()), Context);
	}
	else if (MessageTypeInfo->IsChildOf(FRpcMessage::StaticStruct()))
	{
		ProcessRequest(Context);
	}
}


bool FMessageRpcServer::HandleTicker(float DeltaTime)
{
	const FDateTime UtcNow = FDateTime::UtcNow();

	for (TMap<FGuid, FReturnInfo>::TIterator It(Returns); It; ++It)
	{
		FReturnInfo& ReturnInfo = It.Value();

		if (ReturnInfo.Return->IsReady())
		{
			SendResult(It.Key(), ReturnInfo);
			It.RemoveCurrent();
		}
		else if (UtcNow - ReturnInfo.LastProgressSent > FTimespan::FromSeconds(MESSAGE_RPC_RETRY_INTERVAL * 0.25))
		{
			SendProgress(It.Key(), ReturnInfo);
			ReturnInfo.LastProgressSent = UtcNow;
		}
	}

	return true;
}
