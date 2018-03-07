// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "XmppChat.h"

#include "Containers/Ticker.h"
#include "Containers/Queue.h"

#if WITH_XMPP_STROPHE

class FXmppChatMessage;
class FXmppConnectionStrophe;
class FStropheStanza;

class FXmppPrivateChatStrophe
	: public IXmppChat
	, public FTickerObjectBase
{
	friend FXmppConnectionStrophe;

public:
	// FXmppPrivateChatStrophe
	FXmppPrivateChatStrophe(FXmppConnectionStrophe& InConnectionManager);
	virtual ~FXmppPrivateChatStrophe() = default;

	// XMPP Thread
	void OnDisconnect();
	bool ReceiveStanza(const FStropheStanza& IncomingStanza);

	// Game Thread

	// IXmppChat
	virtual bool SendChat(const FString& RecipientId, const FXmppChatMessage& Chat) override;
	virtual FOnXmppChatReceived& OnReceiveChat() override { return OnChatReceivedDelegate; }

	// FTickerObjectBase
	virtual bool Tick(float DeltaTime) override;

protected:
	void OnChatReceived(TUniquePtr<FXmppChatMessage>&& Chat);

protected:
	/** Connection manager controls sending data to XMPP thread */
	FXmppConnectionStrophe& ConnectionManager;

	/**
	 * Queue of Chat Messages needing to be consumed. These are Queued
	 * on the XMPP thread, and Dequeued on the Game thread
	 */
	TQueue<TUniquePtr<FXmppChatMessage>> IncomingChatMessages;

	/** Delegate for game to listen to Chat Messages */
	FOnXmppChatReceived OnChatReceivedDelegate;
};

#endif
