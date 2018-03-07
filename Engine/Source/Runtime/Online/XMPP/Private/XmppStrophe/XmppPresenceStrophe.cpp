// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "XmppStrophe/XmppPresenceStrophe.h"
#include "XmppStrophe/XmppConnectionStrophe.h"
#include "XmppStrophe/StropheStanza.h"
#include "XmppStrophe/StropheStanzaConstants.h"

#if WITH_XMPP_STROPHE

FXmppPresenceStrophe::FXmppPresenceStrophe(FXmppConnectionStrophe& InConnectionManager)
	: ConnectionManager(InConnectionManager)
{

}

void FXmppPresenceStrophe::OnDisconnect()
{
	if (RosterMembers.Num() > 0)
	{
		RosterMembers.Empty();
	}
	IncomingPresenceUpdates.Empty();
}

bool FXmppPresenceStrophe::ReceiveStanza(const FStropheStanza& IncomingStanza)
{
	if (IncomingStanza.GetName() != Strophe::SN_PRESENCE)
	{
		return false;
	}

	FXmppUserJid FromJid = IncomingStanza.GetFrom();

	bool bIsMucPresence = FromJid.Domain.Equals(ConnectionManager.GetMucDomain(), ESearchCase::CaseSensitive);
	if (bIsMucPresence)
	{
		// Our MultiUserChat interface will handle this stanza
		return false;
	}
	else if (!FromJid.Resource.IsEmpty())
	{
		// Skip user presence updates that are missing a resource
		return true;
	}

	// We build this into a MucPresence and slice it down to a UserPresence if it's a user presence
	FXmppMucPresence Presence;

	Presence.UserJid = MoveTemp(FromJid);

	if (IncomingStanza.GetType() == Strophe::ST_UNAVAILABLE)
	{
		Presence.bIsAvailable = false;
	}
	else
	{
		Presence.bIsAvailable = true;

		TOptional<const FStropheStanza> StatusTextStanza = IncomingStanza.GetChild(Strophe::SN_STATUS);
		if (StatusTextStanza.IsSet())
		{
			Presence.StatusStr = StatusTextStanza->GetText();
		}

		Presence.Status = EXmppPresenceStatus::Online;

		TOptional<const FStropheStanza> StatusEnumStanza = IncomingStanza.GetChild(Strophe::SN_SHOW);
		if (StatusEnumStanza.IsSet())
		{
			FString StatusEnum(StatusEnumStanza->GetText());
			if (StatusEnum == TEXT("away"))
			{
				Presence.Status = EXmppPresenceStatus::Away;
			}
			else if (StatusEnum == TEXT("chat"))
			{
				Presence.Status = EXmppPresenceStatus::Chat;
			}
			else if (StatusEnum == TEXT("dnd"))
			{
				Presence.Status = EXmppPresenceStatus::DoNotDisturb;
			}
			else if (StatusEnum == TEXT("xa"))
			{
				Presence.Status = EXmppPresenceStatus::ExtendedAway;
			}
		}

		TOptional<const FStropheStanza> TimestampStanza = IncomingStanza.GetChild(Strophe::SN_DELAY);
		if (TimestampStanza.IsSet())
		{
			FDateTime::ParseIso8601(*TimestampStanza->GetText(), Presence.SentTime);
		}

		FString UnusedPlatformUserId;
		Presence.UserJid.ParseResource(Presence.AppId, Presence.Platform, UnusedPlatformUserId);
	}

	return IncomingPresenceUpdates.Enqueue(MakeUnique<FXmppUserPresence>(MoveTemp(Presence)));
}

bool FXmppPresenceStrophe::UpdatePresence(const FXmppUserPresence& NewPresence)
{
	FStropheStanza PresenceStanza(ConnectionManager, Strophe::SN_PRESENCE);

	if (NewPresence.bIsAvailable)
	{
		// Update Availability
		{
			FStropheStanza AvailabilityStanza(ConnectionManager, Strophe::SN_SHOW);
			switch (NewPresence.Status)
			{
			case EXmppPresenceStatus::Away:
				AvailabilityStanza.SetText(TEXT("away"));
				break;
			case EXmppPresenceStatus::Chat:
				AvailabilityStanza.SetText(TEXT("chat"));
				break;
			case EXmppPresenceStatus::DoNotDisturb:
				AvailabilityStanza.SetText(TEXT("dnd"));
				break;
			case EXmppPresenceStatus::ExtendedAway:
				AvailabilityStanza.SetText(TEXT("xa"));
				break;
			}

			PresenceStanza.AddChild(AvailabilityStanza);
		}

		// Update Status String
		{
			FStropheStanza StatusStanza(ConnectionManager, Strophe::SN_STATUS);
			StatusStanza.SetText(NewPresence.StatusStr);
			PresenceStanza.AddChild(StatusStanza);
		}

		// Update Sent Time
		{
			FStropheStanza DelayStanza(ConnectionManager, Strophe::SN_DELAY);
			DelayStanza.SetNamespace(Strophe::SNS_DELAY);
			DelayStanza.SetAttribute(Strophe::SA_STAMP, FDateTime::UtcNow().ToIso8601());
			PresenceStanza.AddChild(DelayStanza);
		}
	}
	else
	{
		PresenceStanza.SetType(Strophe::ST_UNAVAILABLE);
	}

	const bool bSuccess = ConnectionManager.SendStanza(MoveTemp(PresenceStanza));
	if (bSuccess)
	{
		CachedPresence = NewPresence;
	}

	return bSuccess;
}

const FXmppUserPresence& FXmppPresenceStrophe::GetPresence() const
{
	return CachedPresence;
}

bool FXmppPresenceStrophe::QueryPresence(const FString& UserId)
{
	// Not supported by tigase
	return false;
}

TArray<TSharedPtr<FXmppUserPresence>> FXmppPresenceStrophe::GetRosterPresence(const FString& UserId)
{
	TArray<TSharedPtr<FXmppUserPresence>> OutRoster;
	for (const TMap<FString, TSharedRef<FXmppUserPresence>>::ElementType& Pair : RosterMembers)
	{
		if (Pair.Value->UserJid.Id == UserId)
		{
			OutRoster.Emplace(Pair.Value);
		}
	}

	return OutRoster;
}

void FXmppPresenceStrophe::GetRosterMembers(TArray<FXmppUserJid>& Members)
{
	Members.Empty(RosterMembers.Num());
	for (const TMap<FString, TSharedRef<FXmppUserPresence>>::ElementType& Pair : RosterMembers)
	{
		Members.Emplace(Pair.Value->UserJid);
	}
}

bool FXmppPresenceStrophe::Tick(float DeltaTime)
{
	while (!IncomingPresenceUpdates.IsEmpty())
	{
		TUniquePtr<FXmppUserPresence> PresencePtr;
		if (IncomingPresenceUpdates.Dequeue(PresencePtr))
		{
			check(PresencePtr.IsValid());
			OnPresenceUpdate(MoveTemp(PresencePtr));
		}
	}

	return true;
}

void FXmppPresenceStrophe::OnPresenceUpdate(TUniquePtr<FXmppUserPresence>&& NewPresencePtr)
{
	TSharedRef<FXmppUserPresence> Presence = MakeShareable(NewPresencePtr.Release());

	RosterMembers.Emplace(Presence->UserJid.GetFullPath(), Presence);
	OnXmppPresenceReceivedDelegate.Broadcast(ConnectionManager.AsShared(), Presence->UserJid, Presence);
}

#endif
