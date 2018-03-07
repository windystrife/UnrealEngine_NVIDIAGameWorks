// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "XmppStrophe/XmppPrivateChatStrophe.h"
#include "XmppStrophe/XmppConnectionStrophe.h"
#include "XmppStrophe/XmppPresenceStrophe.h"
#include "XmppStrophe/StropheStanza.h"
#include "XmppStrophe/StropheStanzaConstants.h"

#if WITH_XMPP_STROPHE

FXmppPrivateChatStrophe::FXmppPrivateChatStrophe(FXmppConnectionStrophe& InConnectionManager)
	: ConnectionManager(InConnectionManager)
{
}

void FXmppPrivateChatStrophe::OnDisconnect()
{
	IncomingChatMessages.Empty();
}

bool FXmppPrivateChatStrophe::ReceiveStanza(const FStropheStanza& IncomingStanza)
{
	if (IncomingStanza.GetName() != Strophe::SN_MESSAGE || // Must be a message
		IncomingStanza.GetType() != Strophe::ST_CHAT || // Filter Non-Chat messages
		IncomingStanza.GetFrom().Domain.StartsWith(TEXT("muc"), ESearchCase::CaseSensitive)) // Filter MUC messages
	{
		return false;
	}

	// Potentially filter out non-friends/non-admins
	if (ConnectionManager.GetServer().bPrivateChatFriendsOnly && ConnectionManager.Presence().IsValid())
	{
		FXmppUserJid FromJid = IncomingStanza.GetFrom();
		if (FromJid.Id != TEXT("xmpp-admin"))
		{
			TArray<FXmppUserJid> RosterMembers;
			ConnectionManager.Presence()->GetRosterMembers(RosterMembers);
			if (!RosterMembers.Contains(FromJid))
			{
				// This was meant for us, but we don't want to see it
				return true;
			}
		}
	}

	TOptional<FString> BodyText = IncomingStanza.GetBodyText();
	if (!BodyText.IsSet())
	{
		// Bad data, no body
		return true;
	}

	FXmppChatMessage ChatMessage;
	ChatMessage.ToJid = IncomingStanza.GetTo();
	ChatMessage.FromJid = IncomingStanza.GetFrom();
	ChatMessage.Body = MoveTemp(BodyText.GetValue());

	// Parse Timezone
	TOptional<const FStropheStanza> StanzaDelay = IncomingStanza.GetChild(Strophe::SN_DELAY);
	if (StanzaDelay.IsSet())
	{
		if (StanzaDelay->HasAttribute(Strophe::SA_STAMP))
		{
			FString Timestamp = StanzaDelay->GetAttribute(Strophe::SA_STAMP);
			FDateTime::ParseIso8601(*Timestamp, ChatMessage.Timestamp);
		}
	}

	if (ChatMessage.Timestamp == 0)
	{
		ChatMessage.Timestamp = FDateTime::UtcNow();
	}

	IncomingChatMessages.Enqueue(MakeUnique<FXmppChatMessage>(MoveTemp(ChatMessage)));
	return true;
}

bool FXmppPrivateChatStrophe::SendChat(const FString& RecipientId, const FXmppChatMessage& Chat)
{
	if (ConnectionManager.GetLoginStatus() != EXmppLoginStatus::LoggedIn)
	{
		return false;
	}

	FStropheStanza ChatStanza(ConnectionManager, Strophe::SN_MESSAGE);
	{
		ChatStanza.SetType(Strophe::ST_CHAT);
		ChatStanza.SetTo(Chat.ToJid);
		ChatStanza.SetFrom(Chat.FromJid);
		ChatStanza.AddBodyWithText(Chat.Body);

		// Update Sent Time
		FStropheStanza DelayStanza(ConnectionManager, Strophe::SN_DELAY);
		{
			DelayStanza.SetNamespace(Strophe::SNS_DELAY);
			DelayStanza.SetAttribute(Strophe::SA_STAMP, FDateTime::UtcNow().ToIso8601());
			ChatStanza.AddChild(DelayStanza);
		}
	}

	return ConnectionManager.SendStanza(MoveTemp(ChatStanza));
}

bool FXmppPrivateChatStrophe::Tick(float DeltaTime)
{
	while (!IncomingChatMessages.IsEmpty())
	{
		TUniquePtr<FXmppChatMessage> ChatMessage;
		if (IncomingChatMessages.Dequeue(ChatMessage))
		{
			check(ChatMessage);
			OnChatReceived(MoveTemp(ChatMessage));
		}
	}

	return true;
}

void FXmppPrivateChatStrophe::OnChatReceived(TUniquePtr<FXmppChatMessage>&& Chat)
{
	TSharedRef<FXmppChatMessage> ChatRef = MakeShareable(Chat.Release());
	OnChatReceivedDelegate.Broadcast(ConnectionManager.AsShared(), ChatRef->FromJid, ChatRef);
}

#endif
