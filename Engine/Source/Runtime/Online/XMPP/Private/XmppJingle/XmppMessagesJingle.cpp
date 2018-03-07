// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "XmppJingle/XmppMessagesJingle.h"
#include "XmppJingle/XmppConnectionJingle.h"
#include "XmppLog.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Policies/CondensedJsonPrintPolicy.h"

#if WITH_XMPP_JINGLE

#include "webrtc/base/helpers.h"
/**
 * Holds a message for send/receive via Xmpp task
 */
class FXmppMessageJingle
{
public:
	FXmppMessageJingle(
		const buzz::Jid& InFromJid = buzz::Jid(), 
		const buzz::Jid& InToJid = buzz::Jid(), 
		const std::string& InBody = std::string()
		)
		: FromJid(InFromJid)
		, ToJid(InToJid)
		, Body(InBody)
	{}

	/** id of message sender */
	buzz::Jid FromJid;
	/** id of message recipient */
	buzz::Jid ToJid;
	/** body of the message */
	std::string Body;
};

/**
 * Task for receiving Xmpp messages
 * Does not process body
 */
class FXmppMessageReceiveTask : public buzz::XmppTask
{
public:
	explicit FXmppMessageReceiveTask(buzz::XmppTaskParentInterface* Parent)
		: buzz::XmppTask(Parent, buzz::XmppEngine::HL_TYPE)
	{}

	virtual ~FXmppMessageReceiveTask()
	{
		// task shouldn't really be deleted until done but just in case
		if (!IsDone())
		{
			Stop();
		}
	}

	virtual int ProcessStart()
	{
		// see if there are any new stanzas to process
		const buzz::XmlElement* Stanza = NextStanza();
		if (Stanza == NULL)
		{
			return STATE_BLOCKED;
		}
		ProcessMessageStanza(Stanza);
		return STATE_START;
	}

	/** signal callback for when message is received & processed */
	sigslot::signal1<const FXmppMessageJingle& /*Message*/> SignalMessageReceived;
	/** signal callback for when a message is returned, for example because the recipient is unavailable or invalid */
	sigslot::signal1<const FXmppMessageJingle& /*Message*/> SignalMessageErrorReturned;

protected:

	virtual bool HandleStanza(const buzz::XmlElement* Stanza)
	{
		const std::string ChatType = "chat";

		bool bHandled = false;
		// Skip all but stanzas of message type
		if (Stanza->Name() == buzz::QN_MESSAGE &&
			// Skip chat messages
			Stanza->Attr(buzz::QN_TYPE) != ChatType &&
			// Skip muc messages
			buzz::Jid(Stanza->Attr(buzz::QN_FROM)).domain().find("muc") == std::string::npos &&
			// Must have a valid message body
			Stanza->FirstNamed(buzz::QN_BODY) != NULL)
		{
			// Queue stanza for task processing
			QueueStanza(Stanza);
			bHandled = true;
		}
		return bHandled;
	}

	void ProcessMessageStanza(const buzz::XmlElement* Stanza)
	{
		check(Stanza != NULL);

		bool bIsErrorMessage = false;
		if (Stanza->HasAttr(buzz::QN_TYPE) && Stanza->Attr(buzz::QN_TYPE) == buzz::STR_ERROR)
		{
			bIsErrorMessage = true;
		}
		
		const buzz::XmlElement* XmlBody = Stanza->FirstNamed(buzz::QN_BODY);
		FXmppMessageJingle Message(
			buzz::Jid(Stanza->Attr(buzz::QN_FROM)),
			buzz::Jid(Stanza->Attr(buzz::QN_TO)),
			XmlBody != NULL ? XmlBody->BodyText() : std::string()
			);

		if (!bIsErrorMessage)
		{
			SignalMessageReceived(Message);
		}
		else
		{
			std::string FromString = Stanza->Attr(buzz::QN_FROM);
			UE_LOG(LogXmpp, Verbose, TEXT("Received returned message to '%s'"), UTF8_TO_TCHAR(FromString.c_str()));
			SignalMessageErrorReturned(Message);
		}
	}
};

/**
* Task for sending Xmpp messages
*/
class FXmppMessageSendTask : public buzz::XmppTask
{
public:
	explicit FXmppMessageSendTask(buzz::XmppTaskParentInterface* Parent)
		: buzz::XmppTask(Parent)
	{}

	buzz::XmppReturnStatus Send(const buzz::Jid& ToJid, const FXmppMessageJingle& Message)
	{
		if (GetState() != STATE_INIT &&
			GetState() != STATE_START)
		{
			return buzz::XMPP_RETURN_BADSTATE;
		}

		buzz::Jid ToJidFull(ToJid.node(), GetClient()->jid().domain(), ToJid.resource());
		buzz::XmlElement* Stanza = MessageToStanza(ToJidFull, Message);
		QueueStanza(Stanza);
		delete Stanza;

		return buzz::XMPP_RETURN_OK;
	}

	virtual int ProcessStart()
	{
		// see if there are any new stanzas to process
		const buzz::XmlElement* Stanza = NextStanza();
		if (Stanza == NULL)
		{
			return STATE_BLOCKED;
		}
		if (SendStanza(Stanza) != buzz::XMPP_RETURN_OK)
		{
			return STATE_ERROR;
		}
		return STATE_START;
	}

protected:

	buzz::XmlElement* MessageToStanza(const buzz::Jid& ToJid, const FXmppMessageJingle& Message)
	{
		buzz::XmlElement* Result = new buzz::XmlElement(buzz::QN_MESSAGE);

		Result->AddAttr(buzz::QN_TO, ToJid.Str());
		Result->AddAttr(buzz::QN_ID, rtc::CreateRandomString(16));

		// Add CorrelationID for tracking purposes
		FXmppJingle::AddCorrIdToStanza(*Result);

		buzz::XmlElement* Body = new buzz::XmlElement(buzz::QN_BODY);
		Body->SetBodyText(Message.Body);
		Result->AddElement(Body);

		return Result;
	}
};

FXmppMessagesJingle::FXmppMessagesJingle(class FXmppConnectionJingle& InConnection)
	: MessageRcvTask(NULL)
	, MessageSendTask(NULL)
	, NumMessagesReceived(0)
	, NumMessagesSent(0)
	, Connection(InConnection)
{
}

FXmppMessagesJingle::~FXmppMessagesJingle()
{
	FXmppMessage* RcvMessage = NULL;
	while (ReceivedMessageQueue.Dequeue(RcvMessage))
	{
		delete RcvMessage;
	}
	FXmppMessageJingle* SndMessage = NULL;
	while (SendMessageQueue.Dequeue(SndMessage))
	{
		delete SndMessage;
	}
}

static void ConvertToMessage(FXmppMessage& OutMessage, const FXmppMessageJingle& InMessageJingle)
{
	FXmppJingle::ConvertToJid(OutMessage.FromJid, InMessageJingle.FromJid);
	FXmppJingle::ConvertToJid(OutMessage.ToJid, InMessageJingle.ToJid);

	auto JsonReader = TJsonReaderFactory<>::Create(UTF8_TO_TCHAR(InMessageJingle.Body.c_str()));
	TSharedPtr<FJsonObject> JsonBody;
	if (FJsonSerializer::Deserialize(JsonReader, JsonBody) &&
		JsonBody.IsValid())
	{
		JsonBody->TryGetStringField(TEXT("type"), OutMessage.Type);
		const TSharedPtr<FJsonObject>* JsonPayload = NULL;
			if (JsonBody->TryGetObjectField(TEXT("payload"), JsonPayload) &&
			JsonPayload != NULL &&
			(*JsonPayload).IsValid())
		{
			auto JsonWriter = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR> >::Create(&OutMessage.Payload);
			FJsonSerializer::Serialize((*JsonPayload).ToSharedRef(), JsonWriter);
 			JsonWriter->Close();
		}
		else
		{
			JsonBody->TryGetStringField(TEXT("payload"), OutMessage.Payload);
		}
		FString TimestampStr;
		if (JsonBody->TryGetStringField(TEXT("timestamp"), TimestampStr))
		{
			FDateTime::ParseIso8601(*TimestampStr, OutMessage.Timestamp);
		}
	}
}

static void ConvertFromMessage(FXmppMessageJingle& OutMessageJingle, const FXmppMessage& InMessage)
{
	FXmppJingle::ConvertFromJid(OutMessageJingle.FromJid, InMessage.FromJid);
	FXmppJingle::ConvertFromJid(OutMessageJingle.ToJid, InMessage.ToJid);

	FString Body;
	auto JsonWriter = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR> >::Create(&Body);
	JsonWriter->WriteObjectStart();
	JsonWriter->WriteValue(TEXT("type"), InMessage.Type);
	JsonWriter->WriteValue(TEXT("payload"), InMessage.Payload);
	JsonWriter->WriteValue(TEXT("timestamp"), FDateTime::UtcNow().ToIso8601());
	JsonWriter->WriteObjectEnd();
	JsonWriter->Close();
	OutMessageJingle.Body = TCHAR_TO_UTF8(*Body);
}

static void DebugPrintMessage(const FXmppMessage& Message)
{
	UE_LOG(LogXmpp, Log, TEXT("   FromJid = %s"), *Message.FromJid.GetFullPath());
	UE_LOG(LogXmpp, Log, TEXT("   ToJid= %s"), *Message.ToJid.GetFullPath());
	UE_LOG(LogXmpp, Log, TEXT("   Type= %s"), *Message.Type);
	UE_LOG(LogXmpp, Log, TEXT("   Timestamp= %s"), *Message.Timestamp.ToString());
	UE_LOG(LogXmpp, Log, TEXT("   Payload= %s"), *Message.Payload);
}

bool FXmppMessagesJingle::SendMessage(const FString& RecipientId, const FXmppMessage& Message)
{
	bool bStarted = false;
	if (Connection.GetLoginStatus() == EXmppLoginStatus::LoggedIn)
	{
		FString RecipientNode;
		FString RecipientDomain;
		RecipientId.Split(TEXT("@"), &RecipientNode, &RecipientDomain);

		FXmppMessage FullMessage(Message);
		FullMessage.FromJid = FXmppUserJid(RecipientNode, RecipientDomain);

		FXmppMessageJingle* NewMessage = new FXmppMessageJingle();
		ConvertFromMessage(*NewMessage, FullMessage);
		bStarted = SendMessageQueue.Enqueue(NewMessage);
		NumMessagesSent++;
	}
	return bStarted;
}

bool FXmppMessagesJingle::Tick(float DeltaTime)
{
	while (!ReceivedMessageQueue.IsEmpty())
	{
		FXmppMessage* NewMessage = NULL;
		if (ReceivedMessageQueue.Dequeue(NewMessage))
		{
			NumMessagesReceived++;
			OnReceiveMessage().Broadcast(Connection.AsShared(), NewMessage->FromJid, MakeShareable(NewMessage));
		}
	}
	return true;
}

void FXmppMessagesJingle::HandlePumpStarting(buzz::XmppPump* XmppPump)
{
	if (MessageRcvTask == NULL)
	{
		MessageRcvTask = new FXmppMessageReceiveTask(XmppPump->client());
		MessageRcvTask->SignalMessageReceived.connect(this, &FXmppMessagesJingle::OnSignalMessageReceived);
 		MessageRcvTask->Start();
	}
	if (MessageSendTask == NULL)
	{
		MessageSendTask = new FXmppMessageSendTask(XmppPump->client());
		MessageSendTask->Start();
	}
}

void FXmppMessagesJingle::HandlePumpQuitting(buzz::XmppPump* XmppPump)
{
	// delete happens automatically when tasks are completed
	if (MessageRcvTask != NULL)
	{
		MessageRcvTask->Abort(true);
		MessageRcvTask = NULL;
	}
	if (MessageSendTask != NULL)
	{
		MessageSendTask->Abort(true);
		MessageSendTask = NULL;
	}
}

void FXmppMessagesJingle::HandlePumpTick(buzz::XmppPump* XmppPump)
{
	while (!SendMessageQueue.IsEmpty())
	{
		FXmppMessageJingle* Message = NULL;
		if (SendMessageQueue.Dequeue(Message))
		{
			// kick off the send task
			if (MessageSendTask != NULL)
			{
				MessageSendTask->Send(Message->ToJid, *Message);
			}			
			delete Message;
		}
	}
}

void FXmppMessagesJingle::OnSignalMessageReceived(const FXmppMessageJingle& MessageJingle)
{
	FXmppMessage* NewMessage = new FXmppMessage();
	ConvertToMessage(*NewMessage, MessageJingle);
	DebugPrintMessage(*NewMessage);

	ReceivedMessageQueue.Enqueue(NewMessage);
}

#endif //WITH_XMPP_JINGLE
