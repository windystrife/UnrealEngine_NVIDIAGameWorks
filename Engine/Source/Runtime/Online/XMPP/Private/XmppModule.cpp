// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "XmppModule.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Modules/ModuleManager.h"
#include "XmppMessages.h"
#include "XmppPubSub.h"
#include "XmppLog.h"
#include "XmppTests.h"
#include "XmppNull.h"
#if WITH_XMPP_JINGLE
#include "XmppJingle/XmppJingle.h"
#elif WITH_XMPP_STROPHE
#include "XmppStrophe/XmppStrophe.h"
#endif

DEFINE_LOG_CATEGORY(LogXmpp);

// FXmppModule

IMPLEMENT_MODULE(FXmppModule, XMPP);

FXmppModule* FXmppModule::Singleton = nullptr;

void FXmppModule::StartupModule()
{
	Singleton = this;

	bEnabled = true;
	GConfig->GetBool(TEXT("XMPP"), TEXT("bEnabled"), bEnabled, GEngineIni);

	if (bEnabled)
	{
#if WITH_XMPP_JINGLE
		FXmppJingle::Init();
#elif WITH_XMPP_STROPHE
		FXmppStrophe::Init();
#endif
	}
}

void FXmppModule::ShutdownModule()
{
	for (auto It = ActiveConnections.CreateIterator(); It; ++It)
	{
		CleanupConnection(It.Value());
	}

	if (bEnabled)
	{
#if WITH_XMPP_JINGLE
		FXmppJingle::Cleanup();
#elif WITH_XMPP_STROPHE
		FXmppStrophe::Cleanup();
#endif
	}

	Singleton = nullptr;
}

bool FXmppModule::HandleXmppCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	FXmppServer XmppServer;
	XmppServer.bUseSSL = true;
	XmppServer.AppId = TEXT("ue_xmpp_test");

	//@todo sz1 - for debug only
	FString ConfigOverride;
	FParse::Value(FCommandLine::Get(), TEXT("EPICAPP="), ConfigOverride);
	if (ConfigOverride.IsEmpty())
	{
		FParse::Value(FCommandLine::Get(), TEXT("EPICENV="), ConfigOverride);
	}
	if (ConfigOverride.IsEmpty())
	{
		FParse::Value(FCommandLine::Get(), TEXT("MCPCONFIG="), ConfigOverride);
	}

	XmppServer.ServerAddr = TEXT("127.0.0.1");
	XmppServer.Domain = TEXT("localhost.net");
	XmppServer.ServerPort = 5222;

	if (FParse::Command(&Cmd, TEXT("Test")))
	{
		FString UserName = FParse::Token(Cmd, false);
		FString Password = FParse::Token(Cmd, false);

		// deletes itself when done with test tasks
		(new FXmppTest())->Test(UserName, Password, XmppServer);

		return true;
	}
	else if (FParse::Command(&Cmd, TEXT("Login")))
	{
		FString UserName = FParse::Token(Cmd, false);
		FString Password = FParse::Token(Cmd, false);

		if (UserName.IsEmpty() || Password.IsEmpty())
		{
			UE_LOG(LogXmpp, Warning, TEXT("Usage: XMPP Login <username> <password>"));
		}
		else
		{
			TSharedPtr<IXmppConnection> Connection = GetConnection(UserName);
			if (Connection.IsValid() &&
				Connection->GetLoginStatus() == EXmppLoginStatus::LoggedIn)
			{
				UE_LOG(LogXmpp, Warning, TEXT("Already logged in as <%s>"), *UserName);
			}
			else
			{
				Connection = CreateConnection(UserName);
				Connection->SetServer(XmppServer);
				Connection->Login(UserName, Password);
			}
		}
		return true;
	}
	else if (FParse::Command(&Cmd, TEXT("Logout")))
	{
		FString UserName = FParse::Token(Cmd, false);

		if (UserName.IsEmpty())
		{
			UE_LOG(LogXmpp, Warning, TEXT("Usage: XMPP Logout <username>"));
		}
		else
		{
			TSharedPtr<IXmppConnection> Connection = GetConnection(UserName);
			if (!Connection.IsValid())
			{
				UE_LOG(LogXmpp, Warning, TEXT("No logged in user found for <%s>"), *UserName);
			}
			else
			{
				Connection->Logout();
				//RemoveConnection(Connection.ToSharedRef());
			}
		}
		return true;
	}
	else if (FParse::Command(&Cmd, TEXT("Presence")))
	{
		FString UserName = FParse::Token(Cmd, false);	// Assumes user is already logged in
		FString OnlineStr = FParse::Token(Cmd, false);	// "ONLINE, AWAY, DND, OFFLINE, etc"
		FString StatusStr = FParse::Token(Cmd, false);	// "status message treated as raw string"

		if (UserName.IsEmpty() || OnlineStr.IsEmpty() || StatusStr.IsEmpty())
		{
			UE_LOG(LogXmpp, Warning, TEXT("Usage: XMPP Presence <username> <ONLINE,AWAY,DND,OFFLINE,XA> <status text>"));
		}
		else
		{
			TSharedPtr<IXmppConnection> Connection = GetConnection(UserName);
			if (!Connection.IsValid())
			{
				UE_LOG(LogXmpp, Warning, TEXT("No logged in user found for <%s>"), *UserName);
			}
			else
			{
				if (Connection->Presence().IsValid())
				{
					FXmppUserPresence XmppPresence = Connection->Presence()->GetPresence();
					XmppPresence.bIsAvailable = true;
					if (OnlineStr.Equals(TEXT("ONLINE"), ESearchCase::IgnoreCase))
					{
						XmppPresence.Status = EXmppPresenceStatus::Online;
					}
					else if (OnlineStr.Equals(TEXT("AWAY"), ESearchCase::IgnoreCase))
					{
						XmppPresence.Status = EXmppPresenceStatus::Away;
					}
					else if (OnlineStr.Equals(TEXT("DND"), ESearchCase::IgnoreCase))
					{
						XmppPresence.Status = EXmppPresenceStatus::DoNotDisturb;
					}
					else if (OnlineStr.Equals(TEXT("OFFLINE"), ESearchCase::IgnoreCase))
					{
						XmppPresence.Status = EXmppPresenceStatus::Offline;
					}
					else if (OnlineStr.Equals(TEXT("XA"), ESearchCase::IgnoreCase))
					{
						XmppPresence.Status = EXmppPresenceStatus::ExtendedAway;
					}
					if (!StatusStr.IsEmpty())
					{
						XmppPresence.StatusStr = StatusStr;
					}
					Connection->Presence()->UpdatePresence(XmppPresence);
				}
			}
		}
		return true;
	}
	else if (FParse::Command(&Cmd, TEXT("PresenceQuery")))
	{
		FString UserName = FParse::Token(Cmd, false);	// Assumes user is already logged in
		FString RosterUser = FParse::Token(Cmd, false);	// User must be in current roster

		if (UserName.IsEmpty() || RosterUser.IsEmpty())
		{
			UE_LOG(LogXmpp, Warning, TEXT("Usage: XMPP PresenceQuery <username> <rosteruser>"));
		}
		else
		{
			TSharedPtr<IXmppConnection> Connection = GetConnection(UserName);
			if (!Connection.IsValid())
			{
				UE_LOG(LogXmpp, Warning, TEXT("No logged in user found for <%s>"), *UserName);
			}
			else
			{
				if (Connection->Presence().IsValid())
				{
					Connection->Presence()->QueryPresence(RosterUser);
				}
			}
		}
		return true;
	}
	else if (FParse::Command(&Cmd, TEXT("Message")))
	{
		FString UserName = FParse::Token(Cmd, false);	// Assumes user is already logged in
		FString Recipient = FParse::Token(Cmd, false);	// User to send message to
		FString Payload = FParse::Token(Cmd, false);	// payload to send

		if (UserName.IsEmpty() || Recipient.IsEmpty())
		{
			UE_LOG(LogXmpp, Warning, TEXT("Usage: XMPP Message <username> <recipient> <optional payload string>"));
		}
		else
		{
			TSharedPtr<IXmppConnection> Connection = GetConnection(UserName);
			if (!Connection.IsValid())
			{
				UE_LOG(LogXmpp, Warning, TEXT("No logged in user found for <%s>"), *UserName);
			}
			else
			{
				if (Connection->Messages().IsValid())
				{
					FXmppMessage Message;
					Message.FromJid.Id = UserName;
					Message.ToJid.Id = Recipient;
					Message.Type = TEXT("test");
					Message.Payload = Payload;
					Connection->Messages()->SendMessage(Recipient, Message);
				}
			}
		}
		return true;
	}
	else if (FParse::Command(&Cmd, TEXT("Chat")))
	{
		FString UserName = FParse::Token(Cmd, false);	// Assumes user is already logged in
		FString Recipient = FParse::Token(Cmd, false);	// User to send message to
		FString Body = FParse::Token(Cmd, false);		// payload to send

		if (UserName.IsEmpty() || Recipient.IsEmpty())
		{
			UE_LOG(LogXmpp, Warning, TEXT("Usage: XMPP Chat <username> <recipient> <body>"));
		}
		else
		{
			TSharedPtr<IXmppConnection> Connection = GetConnection(UserName);
			if (!Connection.IsValid())
			{
				UE_LOG(LogXmpp, Warning, TEXT("No logged in user found for <%s>"), *UserName);
			}
			else
			{
				if (Connection->PrivateChat().IsValid())
				{
					FXmppChatMessage ChatMessage;
					ChatMessage.FromJid.Id = UserName;
					ChatMessage.ToJid.Id = Recipient;
					ChatMessage.Body = Body;
					Connection->PrivateChat()->SendChat(Recipient, ChatMessage);
				}
			}
		}
		return true;
	}
	else if (FParse::Command(&Cmd, TEXT("Muc")))
	{
		if (FParse::Command(&Cmd, TEXT("Create")))
		{
			FString UserName = FParse::Token(Cmd, false);	// Assumes user is already logged in
			FString RoomId = FParse::Token(Cmd, false);		// room to create
			FString IsPrivate = FParse::Token(Cmd, false);		// whether to make room private
			FString Password = FParse::Token(Cmd, false);		// room secret, if private

			if (UserName.IsEmpty() || RoomId.IsEmpty() || (IsPrivate == TEXT("1") && Password.IsEmpty()))
			{
				UE_LOG(LogXmpp, Warning, TEXT("Usage: XMPP Muc Create <username> <room> <private 1/0> <password>"));
			}
			else
			{
				TSharedPtr<IXmppConnection> Connection = GetConnection(UserName);
				if (!Connection.IsValid())
				{
					UE_LOG(LogXmpp, Warning, TEXT("No logged in user found for <%s>"), *UserName);
				}
				else
				{
					if (Connection->MultiUserChat().IsValid())
					{
						FXmppRoomConfig RoomConfig;
						RoomConfig.RoomName = TEXT("Test") + RoomId;
						RoomConfig.bIsPrivate = false;
						RoomConfig.bIsPersistent = false;
						if (IsPrivate == TEXT("1"))
						{
							RoomConfig.bIsPrivate = true;
						}
						RoomConfig.Password = Password;
						Connection->MultiUserChat()->OnRoomCreated().AddRaw(this, &FXmppModule::OnXmppRoomCreated);
						Connection->MultiUserChat()->OnRoomConfigured().AddRaw(this, &FXmppModule::OnXmppRoomConfigured);
						Connection->MultiUserChat()->CreateRoom(RoomId, UserName, RoomConfig);
					}
				}
			}
		}
		else if (FParse::Command(&Cmd, TEXT("Join")))
		{
			FString UserName = FParse::Token(Cmd, false);	// Assumes user is already logged in
			FString RoomId = FParse::Token(Cmd, false);		// room to join
			FString Nickname = FParse::Token(Cmd, false);	// nickname to join (must be unique or join will fail)
			FString Password = FParse::Token(Cmd, false);	// optional password if joining private

			if (UserName.IsEmpty() || RoomId.IsEmpty() || Nickname.IsEmpty())
			{
				UE_LOG(LogXmpp, Warning, TEXT("Usage: XMPP Muc Join <username> <room> <nickname> <?password>"));
			}
			else
			{
				TSharedPtr<IXmppConnection> Connection = GetConnection(UserName);
				if (!Connection.IsValid())
				{
					UE_LOG(LogXmpp, Warning, TEXT("No logged in user found for <%s>"), *UserName);
				}
				else
				{
					if (Connection->MultiUserChat().IsValid())
					{
						if (Password.IsEmpty())
						{
							Connection->MultiUserChat()->JoinPublicRoom(RoomId, Nickname);
						}
						else
						{
							Connection->MultiUserChat()->JoinPrivateRoom(RoomId, Nickname, Password);
						}
					}
				}
			}
		}
		else if (FParse::Command(&Cmd, TEXT("Exit")))
		{
			FString UserName = FParse::Token(Cmd, false);	// Assumes user is already logged in
			FString RoomId = FParse::Token(Cmd, false);		// room to join

			if (UserName.IsEmpty() || RoomId.IsEmpty())
			{
				UE_LOG(LogXmpp, Warning, TEXT("Usage: XMPP Muc Exit <username> <room>"));
			}
			else
			{
				TSharedPtr<IXmppConnection> Connection = GetConnection(UserName);
				if (!Connection.IsValid())
				{
					UE_LOG(LogXmpp, Warning, TEXT("No logged in user found for <%s>"), *UserName);
				}
				else
				{
					if (Connection->MultiUserChat().IsValid())
					{
						Connection->MultiUserChat()->ExitRoom(RoomId);
					}
				}
			}
		}
		else if (FParse::Command(&Cmd, TEXT("Config")))
		{
			FString UserName = FParse::Token(Cmd, false);	// Assumes user is already logged in
			FString RoomId = FParse::Token(Cmd, false);		// room to configure
			FString IsPrivate = FParse::Token(Cmd, false);		// whether to make room private
			FString Password = FParse::Token(Cmd, false);		// room secret, if private

			if (UserName.IsEmpty() || RoomId.IsEmpty() || (IsPrivate == TEXT("1") && Password.IsEmpty()))
			{
				UE_LOG(LogXmpp, Warning, TEXT("Usage: XMPP Muc Config <username> <room> <subject> <private 1/0> <password>"));
			}
			else
			{
				TSharedPtr<IXmppConnection> Connection = GetConnection(UserName);
				if (!Connection.IsValid())
				{
					UE_LOG(LogXmpp, Warning, TEXT("No logged in user found for <%s>"), *UserName);
				}
				else
				{
					if (Connection->MultiUserChat().IsValid())
					{
						FXmppRoomConfig RoomConfig;
						RoomConfig.bIsPrivate = false;
						if (IsPrivate == TEXT("1"))
						{
							RoomConfig.bIsPrivate = true;
						}
						RoomConfig.Password = Password;
						Connection->MultiUserChat()->ConfigureRoom(RoomId, RoomConfig);
					}
				}
			}
		}
		else if (FParse::Command(&Cmd, TEXT("Refresh")))
		{
			FString UserName = FParse::Token(Cmd, false);	// Assumes user is already logged in
			FString RoomId = FParse::Token(Cmd, false);		// roomid

			if (UserName.IsEmpty() || RoomId.IsEmpty())
			{
				UE_LOG(LogXmpp, Warning, TEXT("Usage: XMPP Muc Refresh <username> <room>"));
			}
			else
			{
				TSharedPtr<IXmppConnection> Connection = GetConnection(UserName);
				if (!Connection.IsValid())
				{
					UE_LOG(LogXmpp, Warning, TEXT("No logged in user found for <%s>"), *UserName);
				}
				else
				{
					if (Connection->MultiUserChat().IsValid())
					{
						Connection->MultiUserChat()->RefreshRoomInfo(RoomId);
					}
				}
			}
		}
		else if (FParse::Command(&Cmd, TEXT("Chat")))
		{
			FString UserName = FParse::Token(Cmd, false);	// Assumes user is already logged in
			FString RoomId = FParse::Token(Cmd, false);		// room to join
			FString Body = FParse::Token(Cmd, false);		// payload to send
			FString ExtraInfo = FParse::Token(Cmd, false);	// optional extra info to send (typically json payload)

			if (UserName.IsEmpty() || RoomId.IsEmpty() || Body.IsEmpty())
			{
				UE_LOG(LogXmpp, Warning, TEXT("Usage: XMPP Muc Chat <username> <room> <body>"));
			}
			else
			{
				TSharedPtr<IXmppConnection> Connection = GetConnection(UserName);
				if (!Connection.IsValid())
				{
					UE_LOG(LogXmpp, Warning, TEXT("No logged in user found for <%s>"), *UserName);
				}
				else
				{
					if (Connection->MultiUserChat().IsValid())
					{
						Connection->MultiUserChat()->SendChat(RoomId, Body, ExtraInfo);
					}
				}
			}
		}

		return true;
	}
	else if (FParse::Command(&Cmd, TEXT("PubSub")))
	{
		if (FParse::Command(&Cmd, TEXT("Create")))
		{
			FString UserName = FParse::Token(Cmd, false);	// Assumes user is already logged in
			FString NodeId = FParse::Token(Cmd, false);		// node to create

			if (UserName.IsEmpty() || NodeId.IsEmpty())
			{
				UE_LOG(LogXmpp, Warning, TEXT("Usage: XMPP PubSub Create <username> <node>"));
			}
			else
			{
				TSharedPtr<IXmppConnection> Connection = GetConnection(UserName);
				if (!Connection.IsValid())
				{
					UE_LOG(LogXmpp, Warning, TEXT("No logged in user found for <%s>"), *UserName);
				}
				else
				{
					if (Connection->PubSub().IsValid())
					{
						FXmppPubSubConfig PubSubConfig;
						Connection->PubSub()->CreateNode(NodeId, PubSubConfig);
					}
				}
			}
		}
		else if (FParse::Command(&Cmd, TEXT("Destroy")))
		{
			FString UserName = FParse::Token(Cmd, false);	// Assumes user is already logged in
			FString NodeId = FParse::Token(Cmd, false);		// node to destroy

			if (UserName.IsEmpty() || NodeId.IsEmpty())
			{
				UE_LOG(LogXmpp, Warning, TEXT("Usage: XMPP PubSub Destroy <username> <node>"));
			}
			else
			{
				TSharedPtr<IXmppConnection> Connection = GetConnection(UserName);
				if (!Connection.IsValid())
				{
					UE_LOG(LogXmpp, Warning, TEXT("No logged in user found for <%s>"), *UserName);
				}
				else
				{
					if (Connection->PubSub().IsValid())
					{
						Connection->PubSub()->DestroyNode(NodeId);
					}
				}
			}
		}
		else if (FParse::Command(&Cmd, TEXT("Subscribe")))
		{
			FString UserName = FParse::Token(Cmd, false);	// Assumes user is already logged in
			FString NodeId = FParse::Token(Cmd, false);		// node to subscribe to

			if (UserName.IsEmpty() || NodeId.IsEmpty())
			{
				UE_LOG(LogXmpp, Warning, TEXT("Usage: XMPP PubSub Subscribe <username> <node>"));
			}
			else
			{
				TSharedPtr<IXmppConnection> Connection = GetConnection(UserName);
				if (!Connection.IsValid())
				{
					UE_LOG(LogXmpp, Warning, TEXT("No logged in user found for <%s>"), *UserName);
				}
				else
				{
					if (Connection->PubSub().IsValid())
					{
						Connection->PubSub()->Subscribe(NodeId);
					}
				}
			}
		}
		else if (FParse::Command(&Cmd, TEXT("Unsubscribe")))
		{
			FString UserName = FParse::Token(Cmd, false);	// Assumes user is already logged in
			FString NodeId = FParse::Token(Cmd, false);		// node to subscribe to

			if (UserName.IsEmpty() || NodeId.IsEmpty())
			{
				UE_LOG(LogXmpp, Warning, TEXT("Usage: XMPP PubSub Unsubscribe <username> <node>"));
			}
			else
			{
				TSharedPtr<IXmppConnection> Connection = GetConnection(UserName);
				if (!Connection.IsValid())
				{
					UE_LOG(LogXmpp, Warning, TEXT("No logged in user found for <%s>"), *UserName);
				}
				else
				{
					if (Connection->PubSub().IsValid())
					{
						Connection->PubSub()->Unsubscribe(NodeId);
					}
				}
			}
		}
		else if (FParse::Command(&Cmd, TEXT("Publish")))
		{
			FString UserName = FParse::Token(Cmd, false);	// Assumes user is already logged in
			FString NodeId = FParse::Token(Cmd, false);		// node to subscribe to
			FString PayloadStr = FParse::Token(Cmd, false);	// message payload string

			if (UserName.IsEmpty() || NodeId.IsEmpty() || PayloadStr.IsEmpty())
			{
				UE_LOG(LogXmpp, Warning, TEXT("Usage: XMPP PubSub Publish <username> <node> <text>"));
			}
			else
			{
				TSharedPtr<IXmppConnection> Connection = GetConnection(UserName);
				if (!Connection.IsValid())
				{
					UE_LOG(LogXmpp, Warning, TEXT("No logged in user found for <%s>"), *UserName);
				}
				else
				{
					if (Connection->PubSub().IsValid())
					{
						FXmppPubSubMessage Message;
						Message.Payload = PayloadStr;
						Connection->PubSub()->PublishMessage(NodeId, Message);
					}
				}
			}
		}

		return true;
	}
	else if (FParse::Command(&Cmd, TEXT("LogVerbosity")))
	{
		FString Verbosity = FParse::Token(Cmd, false);

		const FString NAME_NoLogging(TEXT("NoLogging"));
		const FString NAME_Fatal(TEXT("Fatal"));
		const FString NAME_Error(TEXT("Error"));
		const FString NAME_Warning(TEXT("Warning"));
		const FString NAME_Display(TEXT("Display"));
		const FString NAME_Log(TEXT("Log"));
		const FString NAME_Verbose(TEXT("Verbose"));
		const FString NAME_VeryVerbose(TEXT("VeryVerbose"));

		if (Verbosity == NAME_NoLogging)
		{
			UE_SET_LOG_VERBOSITY(LogXmpp, NoLogging);
		}
		else if (Verbosity == NAME_Fatal)
		{
			UE_SET_LOG_VERBOSITY(LogXmpp, Fatal);
		}
		else if (Verbosity == NAME_Error)
		{
			UE_SET_LOG_VERBOSITY(LogXmpp, Error);
		}
		else if (Verbosity == NAME_Warning)
		{
			UE_SET_LOG_VERBOSITY(LogXmpp, Warning);
		}
		else if (Verbosity == NAME_Display)
		{
			UE_SET_LOG_VERBOSITY(LogXmpp, Display);
		}
		else if (Verbosity == NAME_Log)
		{
			UE_SET_LOG_VERBOSITY(LogXmpp, Log);
		}
		else if (Verbosity == NAME_Verbose)
		{
			UE_SET_LOG_VERBOSITY(LogXmpp, Verbose);
		}
		else if (Verbosity == NAME_VeryVerbose)
		{
			UE_SET_LOG_VERBOSITY(LogXmpp, VeryVerbose);
		}
		return true;
	}

	return false;
}

bool FXmppModule::Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
{
	// Ignore any execs that don't start with Xmpp
	if (FParse::Command(&Cmd, TEXT("XMPP")))
	{
		return HandleXmppCommand( Cmd, Ar );
	}
	return false;
}

FXmppModule& FXmppModule::Get()
{
	if (Singleton == nullptr)
	{
		check(IsInGameThread());
		FModuleManager::LoadModuleChecked<FXmppModule>("XMPP");
	}
	check(Singleton != nullptr);
	return *Singleton;
}

bool FXmppModule::IsAvailable()
{
	return Singleton != nullptr;
}

TSharedRef<IXmppConnection> FXmppModule::CreateConnection(const FString& UserId)
{
	TSharedPtr<IXmppConnection> Connection = GetConnection(UserId);
	if (Connection.IsValid())
	{
		return Connection.ToSharedRef();
	}
	else
	{
#if WITH_XMPP_JINGLE
		if (bEnabled)
		{
			Connection = FXmppJingle::CreateConnection();
		}
		else
#elif WITH_XMPP_STROPHE
		if (bEnabled)
		{
			Connection = FXmppStrophe::CreateConnection();
		}
		else
#endif
		{
			Connection = FXmppNull::CreateConnection();
		}

		return ActiveConnections.Add(UserId, Connection.ToSharedRef());
	}
}

TSharedPtr<IXmppConnection> FXmppModule::GetConnection(const FString& UserId) const
{
	TSharedPtr<IXmppConnection> Result;

	const TSharedRef<IXmppConnection>* Found = ActiveConnections.Find(UserId);
	if (Found != nullptr)
	{
		Result = *Found;
	}

	return Result;
}

void FXmppModule::RemoveConnection(const FString& UserId)
{
	TSharedPtr<IXmppConnection> Existing = GetConnection(UserId);
	if (Existing.IsValid())
	{
		CleanupConnection(Existing.ToSharedRef());
		// If we found a TSharedPtr, keep on a removal list until the next tick so it doesn't get destroyed while ticking other things that depend on it
		PendingRemovals.Add(Existing);
	}

	ActiveConnections.Remove(UserId);
}

void FXmppModule::ProcessPendingRemovals()
{
	if (PendingRemovals.Num() > 0)
	{
		PendingRemovals.Empty();
	}
}

void FXmppModule::RemoveConnection(const TSharedRef<IXmppConnection>& Connection)
{
	for (auto It = ActiveConnections.CreateIterator(); It; ++It)
	{
		if (It.Value() == Connection)
		{
			CleanupConnection(Connection);
			It.RemoveCurrent();
			break;
		}
	}
}

void FXmppModule::CleanupConnection(const TSharedRef<class IXmppConnection>& Connection)
{

}

void FXmppModule::OnXmppRoomCreated(const TSharedRef<IXmppConnection>& Connection, bool bSuccess, const FXmppRoomId& RoomId, const FString& Error)
{
	Connection->MultiUserChat()->OnRoomCreated().RemoveAll(this);

	UE_LOG(LogXmpp, Log, TEXT("FXmppModule::OnXmppRoomCreated - entered - user(%s) room(%s)"), *Connection->GetUserJid().Id, *RoomId);
}

void FXmppModule::OnXmppRoomConfigured(const TSharedRef<IXmppConnection>& Connection, bool bSuccess, const FXmppRoomId& RoomId, const FString& Error)
{
	Connection->MultiUserChat()->OnRoomConfigured().RemoveAll(this);

	UE_LOG(LogXmpp, Log, TEXT("FXmppModule::OnXmppRoomConfigured - entered - user(%s) room(%s)"), *Connection->GetUserJid().Id, *RoomId);
}

// temp until WebRTC linked w/OpenSSL
#if 0 // PLATFORM_WINDOWS || PLATFORM_LINUX || PLATFORM_MAC

// newer OpenSSL version have these as #defines
// WebRTC was linked with boringSSL which does not use the following functions as #defined
// so un-doing the #defines and making them live functions to get linking errors fixed...

#include "openssl/bio.h"

#undef BIO_set_retry_read
extern "C" void BIO_set_retry_read(BIO *b)
{
	BIO_set_flags(  b, (BIO_FLAGS_READ | BIO_FLAGS_SHOULD_RETRY));
}

#undef BIO_set_retry_write
extern "C" void BIO_set_retry_write(BIO *b)
{
	BIO_set_flags(b, (BIO_FLAGS_WRITE | BIO_FLAGS_SHOULD_RETRY));
}

#undef BIO_clear_retry_flags
extern "C" void BIO_clear_retry_flags(BIO *b)
{
	BIO_clear_flags(b, (BIO_FLAGS_RWS | BIO_FLAGS_SHOULD_RETRY));
}

#include "openssl/evp.h"

#undef OpenSSL_add_all_algorithms
extern "C" void OpenSSL_add_all_algorithms(void)
{
	OPENSSL_add_all_algorithms_noconf();
	//	OPENSSL_add_all_algorithms_conf(void);
}

#define DWORD uint32 // FUnusableType DWORD
#include "openssl/ssl.h"

#undef DTLSv1_get_timeout
extern "C" long DTLSv1_get_timeout(SSL *ssl, void *parg)
{
	return SSL_ctrl(ssl, DTLS_CTRL_GET_TIMEOUT, 0, (void *)parg);
}

#undef DTLSv1_handle_timeout
extern "C" long DTLSv1_handle_timeout(SSL *ssl)
{
	return SSL_ctrl(ssl, DTLS_CTRL_HANDLE_TIMEOUT, 0, NULL);
}

#undef SSL_set_mode
extern "C" long SSL_set_mode(SSL *ssl, long larg)
{
	return SSL_ctrl((ssl), SSL_CTRL_MODE, (larg), NULL);
}

#undef SSL_CTX_set_read_ahead
extern "C" long SSL_CTX_set_read_ahead(SSL_CTX *ctx, long larg)
{
	return SSL_CTX_ctrl(ctx, SSL_CTRL_SET_READ_AHEAD, larg, NULL);
}

#endif
