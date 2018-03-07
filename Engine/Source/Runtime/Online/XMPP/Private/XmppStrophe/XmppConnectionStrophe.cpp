// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "XmppStrophe/XmppConnectionStrophe.h"
#include "XmppStrophe/XmppMessagesStrophe.h"
#include "XmppStrophe/XmppMultiUserChatStrophe.h"
#include "XmppStrophe/XmppPingStrophe.h"
#include "XmppStrophe/XmppPresenceStrophe.h"
#include "XmppStrophe/XmppPrivateChatStrophe.h"
#include "XmppStrophe/XmppPubSubStrophe.h"
#include "XmppStrophe/StropheContext.h"
#include "XmppStrophe/StropheConnection.h"
#include "XmppStrophe/StropheStanza.h"
#include "XmppStrophe/StropheStanzaConstants.h"
#include "XmppStrophe/StropheError.h"
#include "XmppLog.h"

#if WITH_XMPP_STROPHE

FXmppConnectionStrophe::FXmppConnectionStrophe()
	: LoginStatus(EXmppLoginStatus::NotStarted)
{
	MessagesStrophe = MakeShared<FXmppMessagesStrophe, ESPMode::ThreadSafe>(*this);
	MultiUserChatStrophe = MakeShared<FXmppMultiUserChatStrophe, ESPMode::ThreadSafe>(*this);
	PingStrophe = MakeShared<FXmppPingStrophe, ESPMode::ThreadSafe>(*this);
	PresenceStrophe = MakeShared<FXmppPresenceStrophe, ESPMode::ThreadSafe>(*this);
	PrivateChatStrophe = MakeShared<FXmppPrivateChatStrophe, ESPMode::ThreadSafe>(*this);
	PubSubStrophe = MakeShared<FXmppPubSubStrophe, ESPMode::ThreadSafe>(*this);
}

void FXmppConnectionStrophe::SetServer(const FXmppServer& NewServerConfiguration)
{
	ServerConfiguration = NewServerConfiguration;
	ServerConfiguration.ClientResource = FXmppUserJid::CreateResource(ServerConfiguration.AppId, ServerConfiguration.Platform, ServerConfiguration.PlatformUserId);
}

const FXmppServer& FXmppConnectionStrophe::GetServer() const
{
	return ServerConfiguration;
}

void FXmppConnectionStrophe::Login(const FString& UserId, const FString& Auth)
{
	FXmppUserJid NewJid(UserId, ServerConfiguration.Domain, ServerConfiguration.ClientResource);
	if (!NewJid.IsValid())
	{
		UE_LOG(LogXmpp, Error, TEXT("Invalid Jid %s"), *UserJid.GetFullPath());
		return;
	}

	UE_LOG(LogXmpp, Log, TEXT("Starting Login on connection"));
	UE_LOG(LogXmpp, Log, TEXT("  Server = %s:%d"), *ServerConfiguration.ServerAddr, ServerConfiguration.ServerPort);
	UE_LOG(LogXmpp, Log, TEXT("  User = %s"), *NewJid.GetFullPath());

	if (LoginStatus == EXmppLoginStatus::ProcessingLogin)
	{
		UE_LOG(LogXmpp, Warning, TEXT("Still processing last login"));
	}
	else if (LoginStatus == EXmppLoginStatus::ProcessingLogout)
	{
		UE_LOG(LogXmpp, Warning, TEXT("Still processing last logout"));
	}
	else if (LoginStatus == EXmppLoginStatus::LoggedIn)
	{
		UE_LOG(LogXmpp, Warning, TEXT("Already logged in"));
	}
	else
	{
		// Close down any existing thread
		if (StropheThread.IsValid())
		{
			Logout();
		}

		UserJid = MoveTemp(NewJid);
		MucDomain = FString::Printf(TEXT("muc.%s"), *ServerConfiguration.Domain);

		StartXmppThread(UserJid, Auth);
	}
}

void FXmppConnectionStrophe::Logout()
{
	if (StropheThread.IsValid())
	{
		StopXmppThread();
	}

	MessagesStrophe->OnDisconnect();
	MultiUserChatStrophe->OnDisconnect();
	PingStrophe->OnDisconnect();
	PresenceStrophe->OnDisconnect();
	PrivateChatStrophe->OnDisconnect();
	PubSubStrophe->OnDisconnect();
}

EXmppLoginStatus::Type FXmppConnectionStrophe::GetLoginStatus() const
{
	if (LoginStatus == EXmppLoginStatus::LoggedIn)
	{
		return EXmppLoginStatus::LoggedIn;
	}
	else
	{
		return EXmppLoginStatus::LoggedOut;
	}
}

const FXmppUserJid& FXmppConnectionStrophe::GetUserJid() const
{
	return UserJid;
}

IXmppMessagesPtr FXmppConnectionStrophe::Messages()
{
	return MessagesStrophe;
}

IXmppMultiUserChatPtr FXmppConnectionStrophe::MultiUserChat()
{
	return MultiUserChatStrophe;
}

IXmppPresencePtr FXmppConnectionStrophe::Presence()
{
	return PresenceStrophe;
}

IXmppChatPtr FXmppConnectionStrophe::PrivateChat()
{
	return PrivateChatStrophe;
}

IXmppPubSubPtr FXmppConnectionStrophe::PubSub()
{
	return PubSubStrophe;
}

bool FXmppConnectionStrophe::Tick(float DeltaTime)
{
	// Logout if we've been requested to from the XMPP Thread
	if (RequestLogout)
	{
		RequestLogout = false;
		Logout();
	}

	while (!IncomingLoginStatusChanges.IsEmpty())
	{
		EXmppLoginStatus::Type OldLoginStatus = LoginStatus;
		EXmppLoginStatus::Type NewLoginStatus;
		if (IncomingLoginStatusChanges.Dequeue(NewLoginStatus))
		{
			if (OldLoginStatus != NewLoginStatus)
			{
				UE_LOG(LogXmpp, Log, TEXT("Strophe XMPP thread received LoginStatus change Was: %s Now: %s"), EXmppLoginStatus::ToString(OldLoginStatus), EXmppLoginStatus::ToString(NewLoginStatus));

				// New login status needs to be set before calling below delegates
				LoginStatus = NewLoginStatus;

				if (NewLoginStatus == EXmppLoginStatus::LoggedIn)
				{
					UE_LOG(LogXmpp, Log, TEXT("Logged IN JID=%s"), *GetUserJid().GetFullPath());
					if (OldLoginStatus == EXmppLoginStatus::ProcessingLogin)
					{
						OnLoginComplete().Broadcast(GetUserJid(), true, FString());
					}
					OnLoginChanged().Broadcast(GetUserJid(), EXmppLoginStatus::LoggedIn);
				}
				else if (NewLoginStatus == EXmppLoginStatus::LoggedOut)
				{
					UE_LOG(LogXmpp, Log, TEXT("Logged OUT JID=%s"), *GetUserJid().GetFullPath());
					if (OldLoginStatus == EXmppLoginStatus::ProcessingLogin)
					{
						OnLoginComplete().Broadcast(GetUserJid(), false, FString());
					}
					else if (OldLoginStatus == EXmppLoginStatus::ProcessingLogout)
					{
						OnLogoutComplete().Broadcast(GetUserJid(), true, FString());
					}
					if (OldLoginStatus == EXmppLoginStatus::LoggedIn ||
						OldLoginStatus == EXmppLoginStatus::ProcessingLogout)
					{
						OnLoginChanged().Broadcast(GetUserJid(), EXmppLoginStatus::LoggedOut);
					}
				}
			}
		}
	}

	return true;
}

bool FXmppConnectionStrophe::SendStanza(FStropheStanza&& Stanza)
{
	if (LoginStatus != EXmppLoginStatus::LoggedIn)
	{
		return false;
	}

	if (!StropheThread.IsValid())
	{
		return false;
	}

	return StropheThread->SendStanza(MoveTemp(Stanza));
}

void FXmppConnectionStrophe::StartXmppThread(const FXmppUserJid& ConnectionUser, const FString& ConnectionAuth)
{
	UE_LOG(LogXmpp, Log, TEXT("Starting Strophe XMPP thread"));

	StropheThread = MakeUnique<FXmppStropheThread>(*this, ConnectionUser, ConnectionAuth, ServerConfiguration);
}

void FXmppConnectionStrophe::StopXmppThread()
{
	UE_LOG(LogXmpp, Log, TEXT("Stopping Strophe XMPP thread"));

	StropheThread.Reset();
}

void FXmppConnectionStrophe::ReceiveConnectionStateChange(FStropheConnectionEvent StateChange)
{
	EXmppLoginStatus::Type NewLoginStatus = EXmppLoginStatus::LoggedOut;
	switch (StateChange)
	{
	case FStropheConnectionEvent::Connect:
	case FStropheConnectionEvent::RawConnect:
		NewLoginStatus = EXmppLoginStatus::LoggedIn;
		break;
	case FStropheConnectionEvent::Disconnect:
	case FStropheConnectionEvent::Fail:
		NewLoginStatus = EXmppLoginStatus::LoggedOut;
		RequestLogout = true;
		break;
	}

	UE_LOG(LogXmpp, Log, TEXT("Strophe XMPP thread received state change Was: %s Now: %s"), EXmppLoginStatus::ToString(LoginStatus), EXmppLoginStatus::ToString(NewLoginStatus));

	QueueNewLoginStatus(NewLoginStatus);
}

void FXmppConnectionStrophe::ReceiveConnectionError(const FStropheError& Error, FStropheConnectionEvent Event)
{
	UE_LOG(LogXmpp, Error, TEXT("Received Strophe XMPP Stanza %s with error %s"), *Error.GetStanza().GetName(), *Error.GetErrorString());
}

void FXmppConnectionStrophe::ReceiveStanza(const FStropheStanza& Stanza)
{
	UE_LOG(LogXmpp, Verbose, TEXT("Received Strophe XMPP Stanza %s"), *Stanza.GetName());

	// If ReceiveStanza returns true, the stanza has been consumed and we need to return

	if (MessagesStrophe.IsValid())
	{
		if (MessagesStrophe->ReceiveStanza(Stanza))
		{
			UE_LOG(LogXmpp, VeryVerbose, TEXT("%s Stanza handled by Messages"), *Stanza.GetName());
			return;
		}
	}
	if (MultiUserChatStrophe.IsValid())
	{
		if (MultiUserChatStrophe->ReceiveStanza(Stanza))
		{
			UE_LOG(LogXmpp, VeryVerbose, TEXT("%s Stanza handled by MultiUserChat"), *Stanza.GetName());
			return;
		}
	}
	if (PingStrophe.IsValid())
	{
		if (PingStrophe->ReceiveStanza(Stanza))
		{
			UE_LOG(LogXmpp, VeryVerbose, TEXT("%s Stanza handled by Ping"), *Stanza.GetName());
			return;
		}
	}
	if (PresenceStrophe.IsValid())
	{
		if (PresenceStrophe->ReceiveStanza(Stanza))
		{
			UE_LOG(LogXmpp, VeryVerbose, TEXT("%s Stanza handled by Presence"), *Stanza.GetName());
			return;
		}
	}
	if (PrivateChatStrophe.IsValid())
	{
		if (PrivateChatStrophe->ReceiveStanza(Stanza))
		{
			UE_LOG(LogXmpp, VeryVerbose, TEXT("%s Stanza handled by PrivateChat"), *Stanza.GetName());
			return;
		}
	}
	if (PubSubStrophe.IsValid())
	{
		if (PubSubStrophe->ReceiveStanza(Stanza))
		{
			UE_LOG(LogXmpp, VeryVerbose, TEXT("%s Stanza handled by PubSub"), *Stanza.GetName());
			return;
		}
	}

	checkfSlow(false, TEXT("Unhandled XMPP stanza %s"), *Stanza.GetName());
	UE_LOG(LogXmpp, Warning, TEXT("%s Stanza left unhandled"), *Stanza.GetName());
}

void FXmppConnectionStrophe::QueueNewLoginStatus(EXmppLoginStatus::Type NewStatus)
{
	IncomingLoginStatusChanges.Enqueue(NewStatus);
}

#endif
