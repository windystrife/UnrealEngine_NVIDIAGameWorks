// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "XmppJingle/XmppPubSubJingle.h"
#include "XmppJingle/XmppConnectionJingle.h"
#include "XmppLog.h"
#include "Misc/ScopeLock.h"

#if WITH_XMPP_JINGLE

bool FXmppPubSubJingle::CreateNode(const FXmppPubSubId& NodeId, const FXmppPubSubConfig& NodeConfig)
{
	bool bResult = false;
	FString ErrorStr;

	if (NodeId.IsEmpty())
	{
		ErrorStr = FString::Printf(TEXT("no valid pubsub node id"));
	}

	if (ErrorStr.IsEmpty())
	{
		FScopeLock Lock(&PubSubNodesLock);

		FXmppPubSubNodeJingle& XmppNode = PubSubNodes.FindOrAdd(NodeId);
		if (XmppNode.NodeInfo.Id.IsEmpty())
		{
			XmppNode.NodeInfo.Id = NodeId;
		}
		if (XmppNode.Status == FXmppPubSubNodeJingle::Subscribed)
		{
			ErrorStr = TEXT("already subscribed to node");
		}
		else if (XmppNode.Status != FXmppPubSubNodeJingle::NotSubscribed)
		{
			ErrorStr = TEXT("operation pending for node");
		}
		else
		{
			// marked as pending until op is processed
			XmppNode.Status = FXmppPubSubNodeJingle::CreatePending;
			// queue the join op
			//bResult = PendingOpQueue.Enqueue(new FXmppPubSubCreateNodeOp(NodeId, NodeConfig));
		}
	}

	if (!bResult)
	{
		UE_LOG(LogXmpp, Warning, TEXT("PubSub: CreateNode failed for node=%s error=%s"), 
			*NodeId, *ErrorStr);

		// trigger delegates on error
		OnCreateNodeComplete().Broadcast(Connection.AsShared(), bResult, NodeId, ErrorStr);
	}

	return bResult;
}

bool FXmppPubSubJingle::ConfigureNode(const FXmppPubSubId& NodeId, const FXmppPubSubConfig& NodeConfig)
{
	bool bStarted = false;

	return bStarted;
}

bool FXmppPubSubJingle::DestroyNode(const FXmppPubSubId& NodeId)
{
	bool bStarted = false;

	return bStarted;
}

bool FXmppPubSubJingle::QueryNode(const FXmppPubSubId& NodeId)
{
	bool bStarted = false;

	return bStarted;
}

bool FXmppPubSubJingle::QuerySubscriptions()
{
	bool bStarted = false;

	return bStarted;
}

bool FXmppPubSubJingle::Subscribe(const FXmppPubSubId& NodeId)
{
	bool bStarted = false;

	return bStarted;
}

bool FXmppPubSubJingle::Unsubscribe(const FXmppPubSubId& NodeId)
{
	bool bStarted = false;

	return bStarted;
}

bool FXmppPubSubJingle::PublishMessage(const FXmppPubSubId& NodeId, const FXmppPubSubMessage& Message)
{
	bool bStarted = false;

	return bStarted;
}

TArray<FXmppPubSubId> FXmppPubSubJingle::GetOwnedNodes() const
{
	TArray<FXmppPubSubId> Result;

	return Result;
}

TArray<FXmppPubSubId> FXmppPubSubJingle::GetSubscribedNodes() const
{
	TArray<FXmppPubSubId> Result;

	return Result;
}

TSharedPtr<FXmppPubSubNode> FXmppPubSubJingle::GetNodeInfo(const FXmppPubSubId& NodeId) const
{
	TSharedPtr<FXmppPubSubNode> Result;

	return Result;
}

bool FXmppPubSubJingle::GetLastMessages(const FXmppPubSubId& NodeId, int32 NumMessages, TArray< TSharedRef<FXmppPubSubMessage> >& OutMessages) const
{
	bool bFound = false;

	return bFound;
}

FXmppPubSubJingle::FXmppPubSubJingle(class FXmppConnectionJingle& InConnection)
	: Connection(InConnection)
{

}

FXmppPubSubJingle::~FXmppPubSubJingle()
{

}

bool FXmppPubSubJingle::Tick(float DeltaTime)
{
	while (!ResultOpQueue.IsEmpty())
	{
		FXmppPubSubOpResult* ResultOp = NULL;
		if (ResultOpQueue.Dequeue(ResultOp) &&
			ResultOp != NULL)
		{
			ProcessResultOp(ResultOp, *this);
		}
	}
	return true;
}

void FXmppPubSubJingle::HandlePumpStarting(buzz::XmppPump* XmppPump)
{

}

void FXmppPubSubJingle::HandlePumpQuitting(buzz::XmppPump* XmppPump)
{
	// remove any pending tasks
	while (!PendingOpQueue.IsEmpty())
	{
		FXmppPubSubOp* PendingOp = NULL;
		if (PendingOpQueue.Dequeue(PendingOp) &&
			PendingOp != NULL)
		{
			PendingOp->ProcessError(TEXT("failed to process due to shutdown"));
			delete PendingOp;
		}
	}
}

void FXmppPubSubJingle::HandlePumpTick(buzz::XmppPump* XmppPump)
{
	while (!PendingOpQueue.IsEmpty())
	{
		FXmppPubSubOp* PendingOp = NULL;
		if (PendingOpQueue.Dequeue(PendingOp) &&
			PendingOp != NULL)
		{
			ProcessPendingOp(PendingOp, XmppPump);
			delete PendingOp;
		}
	}
}

void FXmppPubSubJingle::ProcessPendingOp(FXmppPubSubOp* PendingOp, buzz::XmppPump* XmppPump)
{

}

void FXmppPubSubJingle::ProcessResultOp(FXmppPubSubOpResult* ResultOp, class FXmppPubSubJingle& PubSub)
{

}

#endif //WITH_XMPP_JINGLE
