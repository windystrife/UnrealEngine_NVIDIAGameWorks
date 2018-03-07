// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Queue.h"
#include "XmppConnection.h"
#include "Containers/Queue.h"
#include "Containers/Ticker.h"

class FXmppConnectionStrophe;
class FStropheStanza;

#if WITH_XMPP_STROPHE

struct FXmppPingReceivedInfo
{
public:
	FXmppUserJid FromJid;
	FString PingId;

public:
	FXmppPingReceivedInfo()
	{
	}

	FXmppPingReceivedInfo(FXmppUserJid&& InFromJid, FString&& InPingId)
		: FromJid(MoveTemp(InFromJid))
		, PingId(MoveTemp(InPingId))
	{
	}
};

class FXmppPingStrophe
	: public FTickerObjectBase
{
public:
	// FXmppPingStrophe
	FXmppPingStrophe(FXmppConnectionStrophe& InConnectionManager);
	virtual ~FXmppPingStrophe() = default;
	
	// FTickerObjectBase
	virtual bool Tick(float DeltaTime) override;

	/** Clear any caches on disconnect */
	void OnDisconnect();

	/** Determine if an incoming stanza is a ping stanza */
	bool ReceiveStanza(const FStropheStanza& IncomingStanza);
	
protected:
	/** Process parsing a ping we have received and queue it to be replied to */
	bool HandlePingStanza(const FStropheStanza& PingStanza);
	
	/** Queue a reply to a specific ping we received */
	void ReplyToPing(FXmppPingReceivedInfo&& ReceivedPing);

private:
	/** Connection manager controls sending data to XMPP thread */
	FXmppConnectionStrophe& ConnectionManager;

	/** Queued pings we have received but haven't processed */
	TQueue<FXmppPingReceivedInfo> IncomingPings;
};

#endif
