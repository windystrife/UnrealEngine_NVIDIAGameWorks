// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "XmppStrophe/XmppPingStrophe.h"
#include "XmppStrophe/StropheStanza.h"
#include "XmppStrophe/StropheStanzaConstants.h"
#include "XmppStrophe/XmppConnectionStrophe.h"

#if WITH_XMPP_STROPHE

FXmppPingStrophe::FXmppPingStrophe(FXmppConnectionStrophe& InConnectionManager)
	: ConnectionManager(InConnectionManager)
{
}

bool FXmppPingStrophe::Tick(float DeltaTime)
{
	// Process our ping queue and send pongs
	while (!IncomingPings.IsEmpty())
	{
		FXmppPingReceivedInfo ReceivedPing;
		if (IncomingPings.Dequeue(ReceivedPing))
		{
			ReplyToPing(MoveTemp(ReceivedPing));
		}
	}

	// Continue ticking
	return true;
}

void FXmppPingStrophe::OnDisconnect()
{
	// Clear out pending pongs when we disconnect
	IncomingPings.Empty();
}

bool FXmppPingStrophe::ReceiveStanza(const FStropheStanza& IncomingStanza)
{
	// All ping stanzas are IQ
	if (IncomingStanza.GetName() != Strophe::SN_IQ) // Must be an IQ element
	{
		return false;
	}

	// Store StanzaType for multiple comparisons
	const FString StanzaType = IncomingStanza.GetType();

	const bool bIsErrorType = StanzaType == Strophe::ST_ERROR;
	// Check if this is a ping stanza type (type of "get" or "error")
	if (!bIsErrorType && StanzaType != Strophe::ST_GET)
	{
		return false;
	}

	// Check if we have a ping child in the ping namespace
	if (!IncomingStanza.HasChildByNameAndNamespace(Strophe::SN_PING, Strophe::SNS_PING))
	{
		return false;
	}

	// Ignore ping errors (it means whoever we pinged just didn't support pings)
	if (bIsErrorType)
	{
		return true;
	}

	return HandlePingStanza(IncomingStanza);
}

bool FXmppPingStrophe::HandlePingStanza(const FStropheStanza& PingStanza)
{
	IncomingPings.Enqueue(FXmppPingReceivedInfo(PingStanza.GetFrom(), PingStanza.GetId()));
	return true;
}

void FXmppPingStrophe::ReplyToPing(FXmppPingReceivedInfo&& ReceivedPing)
{
	if (ConnectionManager.GetLoginStatus() != EXmppLoginStatus::LoggedIn)
	{
		return;
	}

	FStropheStanza PingReply(ConnectionManager, Strophe::SN_IQ);
	{
		PingReply.SetFrom(ConnectionManager.GetUserJid());
		PingReply.SetTo(ReceivedPing.FromJid);
		PingReply.SetId(MoveTemp(ReceivedPing.PingId));
		PingReply.SetType(Strophe::ST_RESULT);
	}

	ConnectionManager.SendStanza(MoveTemp(PingReply));
}

#endif
