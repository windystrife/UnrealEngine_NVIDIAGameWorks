// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "XmppJingle/XmppJingle.h"
#include "Containers/Queue.h"
#include "XmppMessages.h"

#if WITH_XMPP_JINGLE

/**
 * Xmpp presence implementation using lib tasks/signals
 */
class FXmppMessagesJingle
	: public IXmppMessages
	, public sigslot::has_slots<>
	, public FTickerObjectBase
{
public:

	// IXmppMessages

	virtual bool SendMessage(const FString& RecipientId, const FXmppMessage& Message) override;
	virtual FOnXmppMessageReceived& OnReceiveMessage() override { return OnXmppMessageReceivedDelegate; }

	// FTickerObjectBase
	
	virtual bool Tick(float DeltaTime) override;

	// FXmppMessagesJingle

	FXmppMessagesJingle(class FXmppConnectionJingle& InConnection);
	virtual ~FXmppMessagesJingle();

private:

	/** callback on pump thread when new message has been received */
	void OnSignalMessageReceived(const class FXmppMessageJingle& Message);

	// called on pump thread
	void HandlePumpStarting(buzz::XmppPump* XmppPump);
	void HandlePumpQuitting(buzz::XmppPump* XmppPump);
	void HandlePumpTick(buzz::XmppPump* XmppPump);

	// completion delegates
	FOnXmppMessageReceived OnXmppMessageReceivedDelegate;

	/** task used to receive stanzas of type=message from xmpp pump thread */
	class FXmppMessageReceiveTask* MessageRcvTask;
	/** list of incoming messages */
	TQueue<class FXmppMessage*> ReceivedMessageQueue;

	/** task used to send stanzas of type=message via xmpp pump thread */
	class FXmppMessageSendTask* MessageSendTask;
	/** list of outgoing messages */
	TQueue<class FXmppMessageJingle*> SendMessageQueue;

	/** Number of messages received in a given interval */
	int32 NumMessagesReceived;
	/** Number of messages sent in a given interval */
	int32 NumMessagesSent;

	class FXmppConnectionJingle& Connection;
	friend class FXmppConnectionJingle; 
};

#endif //WITH_XMPP_JINGLE
