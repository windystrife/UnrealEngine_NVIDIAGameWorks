// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "XmppMessages.h"

#include "Containers/Ticker.h"
#include "Containers/Queue.h"

#if WITH_XMPP_STROPHE

class FXmppConnectionStrophe;
class FStropheStanza;

class FXmppMessagesStrophe
	: public IXmppMessages
	, public FTickerObjectBase
{
public:
	// FXmppMessagesStrophe
	FXmppMessagesStrophe(FXmppConnectionStrophe& InConnectionManager);
	~FXmppMessagesStrophe() = default;

	// XMPP Thread
	void OnDisconnect();
	bool ReceiveStanza(const FStropheStanza& IncomingStanza);

	bool HandleMessageStanza(const FStropheStanza& IncomingStanza);
	bool HandleMessageErrorStanza(const FStropheStanza& ErrorStanza);

	// Game Thread

	// IXmppMessages
	virtual bool SendMessage(const FString& RecipientId, const FXmppMessage& Message) override;
	virtual FOnXmppMessageReceived& OnReceiveMessage() override { return OnMessageReceivedDelegate; }

	// FTickerObjectBase
	virtual bool Tick(float DeltaTime) override;

protected:
	void OnMessageReceived(TUniquePtr<FXmppMessage>&& Message);

protected:
	/** Connection manager controls sending data to XMPP thread */
	FXmppConnectionStrophe& ConnectionManager;

	/**
	 * Queue of Messages needing to be consumed. These are Queued
	 * on the XMPP thread, and Dequeued on the Game thread
	 */
	TQueue<TUniquePtr<FXmppMessage>> IncomingMessages;

	/** Delegate for game to listen to messages */
	FOnXmppMessageReceived OnMessageReceivedDelegate;
};

#endif
