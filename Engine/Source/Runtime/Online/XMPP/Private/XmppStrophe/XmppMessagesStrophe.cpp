// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "XmppStrophe/XmppMessagesStrophe.h"
#include "XmppStrophe/XmppConnectionStrophe.h"
#include "XmppStrophe/StropheStanza.h"
#include "XmppStrophe/StropheConnection.h"
#include "XmppStrophe/StropheContext.h"
#include "XmppStrophe/StropheStanzaConstants.h"
#include "XmppStrophe/XmppStrophe.h"
#include "XmppLog.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Misc/Guid.h"

#if WITH_XMPP_STROPHE

FXmppMessagesStrophe::FXmppMessagesStrophe(FXmppConnectionStrophe& InConnectionManager)
	: ConnectionManager(InConnectionManager)
{
}

void FXmppMessagesStrophe::OnDisconnect()
{
	IncomingMessages.Empty();
}

bool FXmppMessagesStrophe::ReceiveStanza(const FStropheStanza& IncomingStanza)
{
	if (IncomingStanza.GetName() != Strophe::SN_MESSAGE || // Must be a message
		IncomingStanza.GetType() == Strophe::ST_CHAT || // Filter Chat messages
		IncomingStanza.GetType() == Strophe::ST_GROUPCHAT) // Filter MUC messages
	{
		return false;
	}

	const TOptional<const FStropheStanza> ErrorStanza = IncomingStanza.GetChild(Strophe::SN_ERROR);
	if (ErrorStanza.IsSet())
	{
		return HandleMessageErrorStanza(ErrorStanza.GetValue());
	}

	return HandleMessageStanza(IncomingStanza);
}

bool FXmppMessagesStrophe::HandleMessageStanza(const FStropheStanza& IncomingStanza)
{
	FXmppMessage Message;
	Message.FromJid = IncomingStanza.GetFrom();
	Message.ToJid = IncomingStanza.GetTo();

	TOptional<FString> BodyText = IncomingStanza.GetBodyText();
	if (!BodyText.IsSet())
	{
		return true;
	}

	auto JsonReader = TJsonReaderFactory<>::Create(MoveTemp(BodyText.GetValue()));
	TSharedPtr<FJsonObject> JsonBody;
	if (FJsonSerializer::Deserialize(JsonReader, JsonBody) &&
		JsonBody.IsValid())
	{
		JsonBody->TryGetStringField(TEXT("type"), Message.Type);
		const TSharedPtr<FJsonObject>* JsonPayload = NULL;
			if (JsonBody->TryGetObjectField(TEXT("payload"), JsonPayload) &&
			JsonPayload != NULL &&
			(*JsonPayload).IsValid())
		{
			auto JsonWriter = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR> >::Create(&Message.Payload);
			FJsonSerializer::Serialize((*JsonPayload).ToSharedRef(), JsonWriter);
			JsonWriter->Close();
		}
		else
		{
			JsonBody->TryGetStringField(TEXT("payload"), Message.Payload);
		}
		FString TimestampStr;
		if (JsonBody->TryGetStringField(TEXT("timestamp"), TimestampStr))
		{
			FDateTime::ParseIso8601(*TimestampStr, Message.Timestamp);
		}
	}

	IncomingMessages.Enqueue(MakeUnique<FXmppMessage>(MoveTemp(Message)));
	return true;
}

bool FXmppMessagesStrophe::HandleMessageErrorStanza(const FStropheStanza& ErrorStanza)
{
	TArray<FStropheStanza> ErrorList = ErrorStanza.GetChildren();
	if (ErrorList.Num() > 0)
	{
		for (const FStropheStanza& ErrorItem : ErrorList)
		{
			const FString ErrorName = ErrorItem.GetName();
			FString OutError;

			if (ErrorName == Strophe::SN_RECIPIENT_UNAVAILABLE)
			{
				OutError = TEXT("The recipient is no longer available.");
			}
			else
			{
				const FString ErrorStanzaText = ErrorItem.GetText();
				OutError = FString::Printf(TEXT("Unknown Error %s: %s"), *ErrorName, *ErrorStanzaText);
			}

			UE_LOG(LogXmpp, Error, TEXT("Message: Received error %s"), *OutError);
		}
	}
	else
	{
		UE_LOG(LogXmpp, Warning, TEXT("Received unknown message stanza error"));
	}
	return true;
}

bool FXmppMessagesStrophe::SendMessage(const FString& RecipientId, const FXmppMessage& Message)
{
	if (ConnectionManager.GetLoginStatus() != EXmppLoginStatus::LoggedIn)
	{
		return false;
	}

	const FXmppUserJid ToJid(Message.ToJid.Id, ConnectionManager.GetServer().Domain, Message.ToJid.Resource);
	const FXmppUserJid FromJid(ConnectionManager.GetUserJid());

	FStropheStanza MessageStanza(ConnectionManager, Strophe::SN_MESSAGE);
	{
		MessageStanza.SetId(FGuid::NewGuid().ToString());
		MessageStanza.SetTo(ToJid);
		MessageStanza.SetFrom(FromJid);

		FString StanzaText;
		auto JsonWriter = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR> >::Create(&StanzaText);
		JsonWriter->WriteObjectStart();
		JsonWriter->WriteValue(TEXT("type"), Message.Type);
		JsonWriter->WriteValue(TEXT("payload"), Message.Payload);
		JsonWriter->WriteValue(TEXT("timestamp"), FDateTime::UtcNow().ToIso8601());
		JsonWriter->WriteObjectEnd();
		JsonWriter->Close();

		MessageStanza.AddBodyWithText(StanzaText);
	}

	return ConnectionManager.SendStanza(MoveTemp(MessageStanza));
}

bool FXmppMessagesStrophe::Tick(float DeltaTime)
{
	while (!IncomingMessages.IsEmpty())
	{
		TUniquePtr<FXmppMessage> Message;
		if (IncomingMessages.Dequeue(Message))
		{
			OnMessageReceived(MoveTemp(Message));
		}
	}

	return true;
}

void FXmppMessagesStrophe::OnMessageReceived(TUniquePtr<FXmppMessage>&& Message)
{
	const TSharedRef<FXmppMessage> MessageRef = MakeShareable(Message.Release());
	OnMessageReceivedDelegate.Broadcast(ConnectionManager.AsShared(), MessageRef->FromJid, MessageRef);
}

#endif
