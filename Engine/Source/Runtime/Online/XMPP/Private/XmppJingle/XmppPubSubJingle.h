// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "XmppJingle/XmppJingle.h"
#include "Containers/Ticker.h"
#include "Containers/Queue.h"
#include "XmppPubSub.h"

#if WITH_XMPP_JINGLE

#define MAX_MESSAGE_HISTORY 50

/**
 * Info cached about a joined/subscribed node
 */
class FXmppPubSubNodeJingle
{
public:
	enum ENodeStatus
	{
		NotSubscribed,
		CreatePending,
		DestroyPending,
		SubscribePending,
		UnsubscribePending,
		Subscribed
	};

	FXmppPubSubNodeJingle()
		: Status(NotSubscribed)
	{}

	void AddNewMessage(const TSharedRef<FXmppPubSubMessage>& Message)
	{
		LastMessages.Add(Message);
		if (LastMessages.Num() > MAX_MESSAGE_HISTORY)
		{
			LastMessages.RemoveAt(0);
		}
	}

	ENodeStatus Status;
	FXmppPubSubNode NodeInfo;
	TArray<TSharedRef<FXmppPubSubMessage>> LastMessages;
};

/**
 * PubSub operation to queue for pump thread consumption
 */
class FXmppPubSubOp
{
public:
	FXmppPubSubOp(const FXmppPubSubId& InNodeId)
		: NodeId(InNodeId)
	{}
	virtual ~FXmppPubSubOp() {};
	virtual class FXmppPubSubOpResult* Process(buzz::XmppPump* XmppPump) = 0;
	virtual class FXmppPubSubOpResult* ProcessError(const FString& ErrorStr) = 0;

	FXmppPubSubId NodeId;
};

/**
 * PubSub operation result queued for game thread consumption
 */
class FXmppPubSubOpResult
{
public:
	FXmppPubSubOpResult(const FXmppPubSubId& InNodeId, bool bInWasSuccessful, const FString& InErrorStr)
		: NodeId(InNodeId)
		, bWasSuccessful(bInWasSuccessful)
		, ErrorStr(InErrorStr)
	{}

	virtual ~FXmppPubSubOpResult() {}
	virtual void Process(class FXmppPubSubJingle& Muc) = 0;

	FXmppPubSubId NodeId;
	bool bWasSuccessful;
	FString ErrorStr;
};

/**
 * Xmpp PubSub (publish/subscribe) implementation using webrtc lib tasks/signals
 */
class FXmppPubSubJingle
	: public IXmppPubSub
	, public FTickerObjectBase
{
public:

	// IXmppPubSub

	virtual bool CreateNode(const FXmppPubSubId& NodeId, const FXmppPubSubConfig& NodeConfig) override;
	virtual bool ConfigureNode(const FXmppPubSubId& NodeId, const FXmppPubSubConfig& NodeConfig) override;
	virtual bool DestroyNode(const FXmppPubSubId& NodeId) override;
	virtual bool QueryNode(const FXmppPubSubId& NodeId) override;
	virtual bool QuerySubscriptions() override;
	virtual bool Subscribe(const FXmppPubSubId& NodeId) override;
	virtual bool Unsubscribe(const FXmppPubSubId& NodeId) override;
	virtual bool PublishMessage(const FXmppPubSubId& NodeId, const FXmppPubSubMessage& Message) override;
	virtual TArray<FXmppPubSubId> GetOwnedNodes() const override;
	virtual TArray<FXmppPubSubId> GetSubscribedNodes() const override;
	virtual TSharedPtr<FXmppPubSubNode> GetNodeInfo(const FXmppPubSubId& NodeId) const override;
	virtual bool GetLastMessages(const FXmppPubSubId& NodeId, int32 NumMessages, TArray< TSharedRef<FXmppPubSubMessage> >& OutMessages) const override;

	virtual FOnXmppPubSubCreateNodeComplete& OnCreateNodeComplete() override { return OnXmppPubSubCreateNodeCompleteDelegate; }
	virtual FOnXmppPubSubConfigureNodeComplete& OnConfigureNodeComplete() override { return OnXmppPubSubConfigureNodeCompleteDelegate; }
	virtual FOnXmppPubSubDestroyNodeComplete& OnDestroyNodeComplete() override { return OnXmppPubSubDestroyNodeCompleteDelegate; }
	virtual FOnXmppPubSubQueryNodeComplete& OnQueryNodeComplete() override { return OnXmppPubSubQueryNodeCompleteDelegate; }
	virtual FOnXmppPubSubQuerySubscriptionsComplete& OnQuerySubscriptionsComplete() override { return OnXmppPubSubQuerySubscriptionsCompleteDelegate; }
	virtual FOnXmppPubSubSubscribed& OnSubscribed() override { return OnXmppPubSubSubscribedDelegate; }
	virtual FOnXmppPubSubUnsubscribed& OnUnsubscribed() override { return OnXmppPubSubUnsubscribedDelegate; }
	virtual FOnXmppPubSubMessageReceived& OnMessageReceived() override { return OnXmppPubSubMessageReceivedDelegate; }

	// FTickerObjectBase
	
	virtual bool Tick(float DeltaTime) override;

	// FXmppPubSubJingle

	FXmppPubSubJingle(class FXmppConnectionJingle& InConnection);
	virtual ~FXmppPubSubJingle();

private:

	// called on pump thread
	void HandlePumpStarting(buzz::XmppPump* XmppPump);
	void HandlePumpQuitting(buzz::XmppPump* XmppPump);
	void HandlePumpTick(buzz::XmppPump* XmppPump);
	
	// pump thread processing of pubsub op
	void ProcessPendingOp(FXmppPubSubOp* PendingOp, buzz::XmppPump* XmppPump);
	// game thread processing of pubsub op result
	void ProcessResultOp(FXmppPubSubOpResult* ResultOp, class FXmppPubSubJingle& PubSub);
	
	TQueue<FXmppPubSubOp*> PendingOpQueue;
	TQueue<FXmppPubSubOpResult*> ResultOpQueue;

	TMap<FXmppPubSubId, FXmppPubSubNodeJingle> PubSubNodes;
	FCriticalSection PubSubNodesLock;

	class FXmppConnectionJingle& Connection;
	friend class FXmppConnectionJingle; 

	// IXmppPubSub delegates

	FOnXmppPubSubCreateNodeComplete OnXmppPubSubCreateNodeCompleteDelegate;
	FOnXmppPubSubConfigureNodeComplete OnXmppPubSubConfigureNodeCompleteDelegate;
	FOnXmppPubSubDestroyNodeComplete OnXmppPubSubDestroyNodeCompleteDelegate;
	FOnXmppPubSubQueryNodeComplete OnXmppPubSubQueryNodeCompleteDelegate;
	FOnXmppPubSubQuerySubscriptionsComplete OnXmppPubSubQuerySubscriptionsCompleteDelegate;
	FOnXmppPubSubSubscribed OnXmppPubSubSubscribedDelegate;
	FOnXmppPubSubUnsubscribed OnXmppPubSubUnsubscribedDelegate;
	FOnXmppPubSubMessageReceived OnXmppPubSubMessageReceivedDelegate;

	friend class FXmppPubSubOp;
	friend class FXmppPubSubOpResult;
};

#endif //WITH_XMPP_JINGLE
