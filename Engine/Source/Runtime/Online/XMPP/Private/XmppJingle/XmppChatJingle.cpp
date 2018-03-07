// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "XmppJingle/XmppChatJingle.h"
#include "XmppJingle/XmppConnectionJingle.h"
#include "XmppPresence.h"
#include "XmppLog.h"

#if WITH_XMPP_JINGLE

/**
 * Holds a chat message for send/receive via Xmpp task
 */
class FXmppChatMessageJingle
{
public:
	FXmppChatMessageJingle(
		const buzz::Jid& InFromJid = buzz::Jid(), 
		const buzz::Jid& InToJid = buzz::Jid(), 
		const std::string& InBody = std::string(),
		const std::string& InTimestamp = std::string()
		)
		: FromJid(InFromJid)
		, ToJid(InToJid)
		, Body(InBody)
		, Timestamp(InTimestamp)
	{}

	/** id of message sender */
	buzz::Jid FromJid;
	/** id of message recipient */
	buzz::Jid ToJid;
	/** body of the chat message */
	std::string Body;
	/** timestamp */
	std::string Timestamp;
};

/**
 * Task for receiving Xmpp chat messages
 */
class FXmppChatReceiveTask : public buzz::XmppTask
{
public:
	explicit FXmppChatReceiveTask(buzz::XmppTaskParentInterface* Parent, FXmppConnectionJingle* InConnection)
		: buzz::XmppTask(Parent, buzz::XmppEngine::HL_TYPE)
		, Connection(InConnection)
	{}

	virtual ~FXmppChatReceiveTask()
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
		ProcessChatStanza(Stanza);
		return STATE_START;
	}

	/** signal callback for when chat message is received & processed */
	sigslot::signal1<const FXmppChatMessageJingle& /*Chat*/> SignalChatReceived;

protected:

	virtual bool HandleStanza(const buzz::XmlElement* Stanza)
	{
		const std::string ChatType = "chat";

		bool bHandled = false;
		// Skip all but stanzas of message type
		if (Stanza->Name() == buzz::QN_MESSAGE &&
			// Skip all but chat messages
			Stanza->Attr(buzz::QN_TYPE) == ChatType &&
			// Must have a valid message body
			Stanza->FirstNamed(buzz::QN_BODY) != NULL)
		{
			// Queue stanza for task processing
			QueueStanza(Stanza);
			bHandled = true;
		}
		return bHandled;
	}

	void ProcessChatStanza(const buzz::XmlElement* Stanza)
	{
		check(Stanza != NULL);

		const buzz::XmlElement* XmlBody = Stanza->FirstNamed(buzz::QN_BODY);
		
		static const buzz::StaticQName QN_DELAY = { "urn:xmpp:delay", "delay" };
		const buzz::XmlElement* Delay = Stanza->FirstNamed(QN_DELAY);

		buzz::Jid FromJidBuzz(Stanza->Attr(buzz::QN_FROM));

		bool bMessageAllowed = true;
		if (Connection->GetServer().bPrivateChatFriendsOnly && Connection->Presence().IsValid())
		{
			FXmppUserJid FromJid;
			FXmppJingle::ConvertToJid(FromJid, FromJidBuzz);
			if (FromJid.Id.Compare(TEXT("xmpp-admin"), ESearchCase::IgnoreCase) != 0)
			{
				TArray<FXmppUserJid> RosterMembers;
				Connection->Presence()->GetRosterMembers(RosterMembers);
				if (!RosterMembers.Contains(FromJid))
				{
					bMessageAllowed = false;
				}
			}
		}

		if (bMessageAllowed)
		{
			FXmppChatMessageJingle ChatMessage(
				FromJidBuzz,
				buzz::Jid(Stanza->Attr(buzz::QN_TO)),
				XmlBody != NULL ? XmlBody->BodyText() : std::string(),
				Delay != NULL ? Delay->Attr(buzz::kQnStamp) : std::string()
				);

			SignalChatReceived(ChatMessage);
		}
	}

	FXmppConnectionJingle* Connection;
};

/**
* Task for sending Xmpp chat messages
*/
class FXmppChatSendTask : public buzz::XmppTask
{
public:
	explicit FXmppChatSendTask(buzz::XmppTaskParentInterface* Parent)
		: buzz::XmppTask(Parent)
	{}

	buzz::XmppReturnStatus Send(const buzz::Jid& ToJid, const FXmppChatMessageJingle& ChatMessage)
	{
		if (GetState() != STATE_INIT &&
			GetState() != STATE_START)
		{
			return buzz::XMPP_RETURN_BADSTATE;
		}

		buzz::Jid ToJidFull(ToJid.node(), GetClient()->jid().domain(), buzz::STR_EMPTY);
		buzz::XmlElement* Stanza = ChatToStanza(ToJidFull, ChatMessage);
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

	buzz::XmlElement* ChatToStanza(const buzz::Jid& ToJid, const FXmppChatMessageJingle& Message)
	{
		buzz::XmlElement* Result = new buzz::XmlElement(buzz::QN_MESSAGE);

		const std::string ChatType = "chat";

		Result->AddAttr(buzz::QN_TO, ToJid.Str());
		Result->AddAttr(buzz::QN_TYPE, ChatType);

		// Add CorrelationID for tracking purposes
		FXmppJingle::AddCorrIdToStanza(*Result);

		buzz::XmlElement* Body = new buzz::XmlElement(buzz::QN_BODY);
		Body->SetBodyText(Message.Body);
		Result->AddElement(Body);

		return Result;
	}
};

FXmppChatJingle::FXmppChatJingle(class FXmppConnectionJingle& InConnection)
	: ChatRcvTask(NULL)
	, ChatSendTask(NULL)
	, Connection(InConnection)
{
}

FXmppChatJingle::~FXmppChatJingle()
{
	FXmppChatMessage* RcvMessage = NULL;
	while (ReceivedChatQueue.Dequeue(RcvMessage))
	{
		delete RcvMessage;
	}
	FXmppChatMessageJingle* SndMessage = NULL;
	while (SendChatQueue.Dequeue(SndMessage))
	{
		delete SndMessage;
	}
}

static void ConvertToMessage(FXmppChatMessage& OutMessage, const FXmppChatMessageJingle& InMessageJingle)
{
	FXmppJingle::ConvertToJid(OutMessage.FromJid, InMessageJingle.FromJid);
	FXmppJingle::ConvertToJid(OutMessage.ToJid, InMessageJingle.ToJid);
	OutMessage.Body = UTF8_TO_TCHAR(InMessageJingle.Body.c_str());
	if (!InMessageJingle.Timestamp.empty())
	{
		FDateTime::ParseIso8601(UTF8_TO_TCHAR(InMessageJingle.Timestamp.c_str()), OutMessage.Timestamp);
	}
	else
	{
		OutMessage.Timestamp = FDateTime::UtcNow();
	}
}

static void ConvertFromMessage(FXmppChatMessageJingle& OutMessageJingle, const FXmppChatMessage& InMessage)
{
	FXmppJingle::ConvertFromJid(OutMessageJingle.FromJid, InMessage.FromJid);
	FXmppJingle::ConvertFromJid(OutMessageJingle.ToJid, InMessage.ToJid);
	OutMessageJingle.Body = TCHAR_TO_UTF8(*InMessage.Body);
}

static void DebugPrintChat(const FXmppChatMessage& ChatMessage)
{
	UE_LOG(LogXmpp, Log, TEXT("   FromJid = %s"), *ChatMessage.FromJid.GetFullPath());
	UE_LOG(LogXmpp, Log, TEXT("   ToJid= %s"), *ChatMessage.ToJid.GetFullPath());
	UE_LOG(LogXmpp, Log, TEXT("   Body= %s"), *ChatMessage.Body);
}

bool FXmppChatJingle::SendChat(const FString& RecipientId, const FXmppChatMessage& ChatMessage)
{
	FXmppChatMessageJingle* NewChat = new FXmppChatMessageJingle();
	ConvertFromMessage(*NewChat, ChatMessage);
	bool bChatSent = SendChatQueue.Enqueue(NewChat);
	bChatSent ? NumSentChat++ : 0;
	return bChatSent;
}

bool FXmppChatJingle::Tick(float DeltaTime)
{
	while (!ReceivedChatQueue.IsEmpty())
	{
		FXmppChatMessage* ChatMessage = NULL;
		if (ReceivedChatQueue.Dequeue(ChatMessage))
		{
			NumReceivedChat++;
			OnReceiveChat().Broadcast(Connection.AsShared(), ChatMessage->FromJid, MakeShareable(ChatMessage));
		}
	}
	return true;
}

void FXmppChatJingle::HandlePumpStarting(buzz::XmppPump* XmppPump)
{
	if (ChatRcvTask == NULL)
	{
		ChatRcvTask = new FXmppChatReceiveTask(XmppPump->client(), &Connection);
		ChatRcvTask->SignalChatReceived.connect(this, &FXmppChatJingle::OnSignalChatReceived);
 		ChatRcvTask->Start();
	}
	if (ChatSendTask == NULL)
	{
		ChatSendTask = new FXmppChatSendTask(XmppPump->client());
		ChatSendTask->Start();
	}
}

void FXmppChatJingle::HandlePumpQuitting(buzz::XmppPump* XmppPump)
{
	// delete happens automatically when tasks are completed
	if (ChatRcvTask != NULL)
	{
		ChatRcvTask->Abort(true);
		ChatRcvTask = NULL;
	}
	if (ChatSendTask != NULL)
	{
		ChatSendTask->Abort(true);
		ChatSendTask = NULL;
	}
}

void FXmppChatJingle::HandlePumpTick(buzz::XmppPump* XmppPump)
{
	while (!SendChatQueue.IsEmpty())
	{
		FXmppChatMessageJingle* ChatMessage = NULL;
		if (SendChatQueue.Dequeue(ChatMessage))
		{
			// kick off the send task
			if (ChatSendTask != NULL)
			{
				ChatSendTask->Send(ChatMessage->ToJid, *ChatMessage);
			}			
			delete ChatMessage;
		}
	}
}

void FXmppChatJingle::OnSignalChatReceived(const FXmppChatMessageJingle& ChatMessageJingle)
{
	FXmppChatMessage* NewMessage = new FXmppChatMessage();
	ConvertToMessage(*NewMessage, ChatMessageJingle);
	DebugPrintChat(*NewMessage);

	ReceivedChatQueue.Enqueue(NewMessage);
}

#endif //WITH_XMPP_JINGLE
