// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "XmppJingle/XmppPresenceJingle.h"
#include "Misc/ScopeLock.h"
#include "XmppLog.h"
#include "XmppJingle/XmppConnectionJingle.h"
#include "XmppMultiUserChat.h"

#if WITH_XMPP_JINGLE

class FXmppMucPresenceStatus : 
	public buzz::MucPresenceStatus
{
public:
	void set_role(const std::string& role)
	{
		Role = role;
	}

	const std::string& get_role() const
	{
		return Role;
	}

	void set_affiliation(const std::string& affiliation)
	{
		Affiliation = affiliation;
	}

	const std::string& get_affiliation() const
	{
		return Affiliation;
	}

private:
	std::string Role;
	std::string Affiliation;
};

/**
* Task for receiving Xmpp presence
*/
class FXmppPresenceReceiveTask : public buzz::XmppTask
{
public:
	explicit FXmppPresenceReceiveTask(buzz::XmppTaskParentInterface* Parent, const std::string& InMucDomain)
		: buzz::XmppTask(Parent, buzz::XmppEngine::HL_TYPE)
		, MucDomain(InMucDomain)
	{}

	virtual ~FXmppPresenceReceiveTask()
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
		
		buzz::Jid From(Stanza->Attr(buzz::QN_FROM));
		HandlePresence(From, Stanza);
		
		return STATE_START;
	}

	/** signal callback for when presence is received & processed */
	sigslot::signal1<const buzz::PresenceStatus& /*Status*/> SignalPresenceUpdate;
	sigslot::signal1<const FXmppMucPresenceStatus& /*Status*/> MucSignalPresenceUpdate;

	/** domain for muc room endpoint from connection */
	std::string MucDomain;

protected:

	virtual bool HandleStanza(const buzz::XmlElement* Stanza)
	{
		bool bHandled = false;
		// Skip all but presence stanzas
		if (Stanza->Name() == buzz::QN_PRESENCE)
		{
			// Queue stanza for task processing
			QueueStanza(Stanza);
			bHandled = true;
		}
		return bHandled;
	}

	void HandlePresence(const buzz::Jid& From, const buzz::XmlElement* Stanza)
	{
		if (Stanza->Attr(buzz::QN_TYPE) != buzz::STR_ERROR) 
		{
			// detect muc room specific presence updates
			if (FCStringAnsi::Strnicmp(MucDomain.c_str(), From.domain().c_str(), MucDomain.length()) == 0)
			{
				FXmppMucPresenceStatus MucStatus;
				DecodeMucStatus(From, Stanza, &MucStatus);
				MucSignalPresenceUpdate(MucStatus);
			}
			else
			{
				buzz::PresenceStatus Status;
				DecodeStatus(From, Stanza, &Status);
				SignalPresenceUpdate(Status);
			}
		}
	}

	void DecodeMucStatus(
		const buzz::Jid& From,
		const buzz::XmlElement* Stanza,
		FXmppMucPresenceStatus* MucPresenceStatus)
	{
		DecodeStatus(From, Stanza, MucPresenceStatus);
		if (Stanza->Attr(buzz::QN_TYPE) != buzz::STR_UNAVAILABLE)
		{
			const buzz::XmlElement* UserElem = Stanza->FirstNamed(buzz::QN_MUC_USER_X);
			if (UserElem != nullptr)
			{
				const buzz::XmlElement* UserItemElem = UserElem->FirstNamed(buzz::QN_MUC_USER_ITEM);
				if (UserItemElem != nullptr)
				{
					MucPresenceStatus->set_role(UserItemElem->Attr(buzz::QN_ROLE));
					MucPresenceStatus->set_affiliation(UserItemElem->Attr(buzz::QN_AFFILIATION));
				}
			}
		}

	}

	void DecodeStatus(
		const buzz::Jid& From,
		const buzz::XmlElement* Stanza,
		buzz::PresenceStatus* PresenceStatus) 
	{
		PresenceStatus->set_jid(From);
		if (Stanza->Attr(buzz::QN_TYPE) == buzz::STR_UNAVAILABLE) 
		{
			PresenceStatus->set_available(false);
		}
		else 
		{
			PresenceStatus->set_available(true);
			const buzz::XmlElement* StatusElem = Stanza->FirstNamed(buzz::QN_STATUS);
			if (StatusElem != NULL) 
			{
				PresenceStatus->set_status(StatusElem->BodyText());
			}

			const buzz::XmlElement* Priority = Stanza->FirstNamed(buzz::QN_PRIORITY);
			if (Priority != NULL) 
			{
				int Pri;
				if (rtc::FromString(Priority->BodyText(), &Pri)) 
				{
					PresenceStatus->set_priority(Pri);
				}
			}

			const buzz::XmlElement* show = Stanza->FirstNamed(buzz::QN_SHOW);
			if (show == NULL || show->FirstChild() == NULL) 
			{
				PresenceStatus->set_show(buzz::PresenceStatus::SHOW_ONLINE);
			}
			else if (show->BodyText() == "away") 
			{
				PresenceStatus->set_show(buzz::PresenceStatus::SHOW_AWAY);
			}
			else if (show->BodyText() == "xa") 
			{
				PresenceStatus->set_show(buzz::PresenceStatus::SHOW_XA);
			}
			else if (show->BodyText() == "dnd") 
			{
				PresenceStatus->set_show(buzz::PresenceStatus::SHOW_DND);
			}
			else if (show->BodyText() == "chat") 
			{
				PresenceStatus->set_show(buzz::PresenceStatus::SHOW_CHAT);
			}
			else 
			{
				PresenceStatus->set_show(buzz::PresenceStatus::SHOW_ONLINE);
			}

			const buzz::XmlElement* Caps = Stanza->FirstNamed(buzz::QN_CAPS_C);
			if (Caps != NULL) 
			{
				std::string Node = Caps->Attr(buzz::QN_NODE);
				std::string Ver = Caps->Attr(buzz::QN_VER);
				std::string Exts = Caps->Attr(buzz::QN_EXT);

				PresenceStatus->set_know_capabilities(true);
				PresenceStatus->set_caps_node(Node);
				PresenceStatus->set_version(Ver);
			}

			static const buzz::StaticQName QN_DELAY = { "urn:xmpp:delay", "delay" };
			const buzz::XmlElement* Delay = Stanza->FirstNamed(QN_DELAY);
			if (Delay != NULL)
			{
				std::string Stamp = Delay->Attr(buzz::kQnStamp);
				PresenceStatus->set_sent_time(Stamp);
			}

			const buzz::XmlElement* Nick = Stanza->FirstNamed(buzz::QN_NICKNAME);
			if (Nick) 
			{
				PresenceStatus->set_nick(Nick->BodyText());
			}
		}
	}
};

class FXmppPresenceOutTask : public buzz::XmppTask
{
public:
	explicit FXmppPresenceOutTask(buzz::XmppTaskParentInterface* Parent)
		: buzz::XmppTask(Parent)
	{}
	virtual ~FXmppPresenceOutTask()
	{}

	buzz::XmppReturnStatus Send(const buzz::PresenceStatus& Status)
	{
		if (GetState() != STATE_INIT && GetState() != STATE_START)
			return buzz::XMPP_RETURN_BADSTATE;

		TUniquePtr<buzz::XmlElement> Presence{TranslateStatus(Status)};
		QueueStanza(Presence.Get());
		return buzz::XMPP_RETURN_OK;
	}

	buzz::XmppReturnStatus SendDirected(const buzz::Jid& Jid, const buzz::PresenceStatus& Status)
	{
		if (GetState() != STATE_INIT && GetState() != STATE_START)
			return buzz::XMPP_RETURN_BADSTATE;

		TUniquePtr<buzz::XmlElement> Presence{TranslateStatus(Status)};
		Presence->AddAttr(buzz::QN_TO, Jid.Str());
		QueueStanza(Presence.Get());
		return buzz::XMPP_RETURN_OK;
	}

	buzz::XmppReturnStatus SendProbe(const buzz::Jid& Jid)
	{
		if (GetState() != STATE_INIT && GetState() != STATE_START)
			return buzz::XMPP_RETURN_BADSTATE;

		buzz::XmlElement Presence{buzz::QN_PRESENCE};
		Presence.AddAttr(buzz::QN_TO, Jid.Str());
		Presence.AddAttr(buzz::QN_TYPE, "probe");

		// Add CorrelationID for tracking purposes
		FXmppJingle::AddCorrIdToStanza(Presence);

		QueueStanza(&Presence);
		return buzz::XMPP_RETURN_OK;
	}

	virtual int ProcessStart()
	{
		const buzz::XmlElement* Stanza = NextStanza();
		if (Stanza == NULL)
			return STATE_BLOCKED;

		if (SendStanza(Stanza) != buzz::XMPP_RETURN_OK)
			return STATE_ERROR;

		return STATE_START;
	}

private:
	buzz::XmlElement* TranslateStatus(const buzz::PresenceStatus& Status)
	{
		buzz::XmlElement* Result = new buzz::XmlElement(buzz::QN_PRESENCE);

		// Add CorrelationID for tracking purposes
		FXmppJingle::AddCorrIdToStanza(*Result);

		if (!Status.available())
		{
			Result->AddAttr(buzz::QN_TYPE, buzz::STR_UNAVAILABLE);
		}
		else
		{
			if (Status.show() != buzz::PresenceStatus::SHOW_ONLINE &&
				Status.show() != buzz::PresenceStatus::SHOW_OFFLINE)
			{
				Result->AddElement(new buzz::XmlElement(buzz::QN_SHOW));
				switch (Status.show())
				{
				default:
					Result->AddText(buzz::STR_SHOW_AWAY, 1);
					break;
				case buzz::PresenceStatus::SHOW_XA:
					Result->AddText(buzz::STR_SHOW_XA, 1);
					break;
				case buzz::PresenceStatus::SHOW_DND:
					Result->AddText(buzz::STR_SHOW_DND, 1);
					break;
				case buzz::PresenceStatus::SHOW_CHAT:
					Result->AddText(buzz::STR_SHOW_CHAT, 1);
					break;
				}
			}

			Result->AddElement(new buzz::XmlElement(buzz::QN_STATUS));
			Result->AddText(Status.status(), 1);

			if (!Status.nick().empty())
			{
				Result->AddElement(new buzz::XmlElement(buzz::QN_NICKNAME));
				Result->AddText(Status.nick(), 1);
			}

			std::string Pri;
			rtc::ToString(Status.priority(), &Pri);

			Result->AddElement(new buzz::XmlElement(buzz::QN_PRIORITY));
			Result->AddText(Pri, 1);

			if (Status.know_capabilities())
			{
				Result->AddElement(new buzz::XmlElement(buzz::QN_CAPS_C, true));
				Result->AddAttr(buzz::QN_NODE, Status.caps_node(), 1);
				Result->AddAttr(buzz::QN_VER, Status.version(), 1);

				std::string Caps;
				Caps.append(Status.voice_capability() ? "voice-v1" : "");
				Caps.append(Status.pmuc_capability() ? " pmuc-v1" : "");
				Caps.append(Status.video_capability() ? " video-v1" : "");
				Caps.append(Status.camera_capability() ? " camera-v1" : "");

				Result->AddAttr(buzz::QN_EXT, Caps, 1);
			}

			if (!Status.sent_time().empty())
			{
				static const buzz::StaticQName QN_DELAY = { "urn:xmpp:delay", "delay" };
				Result->AddElement(new buzz::XmlElement(QN_DELAY, true));
				Result->AddAttr(buzz::kQnStamp, Status.sent_time(), 1);
			}
		}

		return Result;
	}
};

FXmppPresenceJingle::FXmppPresenceJingle(class FXmppConnectionJingle& InConnection)
	: PresenceSendTask(NULL)
	, PresenceRcvTask(NULL)
	, NumPresenceIn(0)
	, NumPresenceOut(0)
	, NumQueryRequests(0)
	, Connection(InConnection) 
{

}

FXmppPresenceJingle::~FXmppPresenceJingle()
{
	buzz::PresenceStatus* Status = NULL;
	while (PresenceUpdateRequests.Dequeue(Status))
	{
		delete Status;
	}
}

void FXmppPresenceJingle::ConvertToPresence(FXmppUserPresence& OutPresence, const buzz::PresenceStatus& InStatus, const FXmppUserJid& InJid, const FString& InResourceOverride)
{
	OutPresence.UserJid = InJid;
	OutPresence.bIsAvailable = InStatus.available();
	OutPresence.StatusStr = UTF8_TO_TCHAR(InStatus.status().c_str());
	if (!InStatus.sent_time().empty())
	{
		//@todo samz - fix legacy time usage
		FString SentTime = UTF8_TO_TCHAR(InStatus.sent_time().c_str());
		// convert from "20141115T19:43:17" time to "2014-11-15T19:43:17" Iso8601 compatible format
		if (!SentTime.Contains(TEXT("-")) && SentTime.Len() >= 8)
		{
			SentTime.InsertAt(6, TEXT("-"));
			SentTime.InsertAt(4, TEXT("-"));
		}
		FDateTime::ParseIso8601(*SentTime, OutPresence.SentTime);
	}
	
	OutPresence.Status = EXmppPresenceStatus::Offline;
	if (InStatus.available())
	{
		switch (InStatus.show())
		{
		case buzz::PresenceStatus::SHOW_ONLINE:
			OutPresence.Status = EXmppPresenceStatus::Online;
			break;
		case buzz::PresenceStatus::SHOW_NONE:
		case buzz::PresenceStatus::SHOW_OFFLINE:
			OutPresence.Status = EXmppPresenceStatus::Offline;
			break;
		case buzz::PresenceStatus::SHOW_AWAY:
			OutPresence.Status = EXmppPresenceStatus::Away;
			break;
		case buzz::PresenceStatus::SHOW_XA:
			OutPresence.Status = EXmppPresenceStatus::ExtendedAway;
			break;
		case buzz::PresenceStatus::SHOW_DND:
			OutPresence.Status = EXmppPresenceStatus::DoNotDisturb;
			break;
		case buzz::PresenceStatus::SHOW_CHAT:
			OutPresence.Status = EXmppPresenceStatus::Chat;
			break;
		}
	}
	
	FString UnusedPlatformUserId;
	FXmppUserJid::ParseResource(InResourceOverride.IsEmpty() ? InJid.Resource : InResourceOverride, OutPresence.AppId, OutPresence.Platform, UnusedPlatformUserId);
}

void FXmppPresenceJingle::ConvertFromPresence(buzz::PresenceStatus& OutStatus, const FXmppUserPresence& InPresence)
{
	OutStatus.set_available(InPresence.bIsAvailable);
	OutStatus.set_sent_time(TCHAR_TO_UTF8(*InPresence.SentTime.ToIso8601()));
	
	OutStatus.set_show(buzz::PresenceStatus::SHOW_OFFLINE);
	if (InPresence.bIsAvailable)
	{
		switch (InPresence.Status)
		{
		case EXmppPresenceStatus::Online:
			OutStatus.set_show(buzz::PresenceStatus::SHOW_ONLINE);
			break;
		case EXmppPresenceStatus::Offline:
			OutStatus.set_show(buzz::PresenceStatus::SHOW_OFFLINE);
			break;
		case EXmppPresenceStatus::Away:
			OutStatus.set_show(buzz::PresenceStatus::SHOW_AWAY);
			break;
		case EXmppPresenceStatus::ExtendedAway:
			OutStatus.set_show(buzz::PresenceStatus::SHOW_XA);
			break;
		case EXmppPresenceStatus::DoNotDisturb:
			OutStatus.set_show(buzz::PresenceStatus::SHOW_DND);
			break;
		case EXmppPresenceStatus::Chat:
			OutStatus.set_show(buzz::PresenceStatus::SHOW_CHAT);
			break;
		}
	}
	OutStatus.set_status(TCHAR_TO_UTF8(*InPresence.StatusStr));
}

static void DebugPrintPresence(const FXmppUserPresence& Presence)
{
	UE_LOG(LogXmpp, Verbose, TEXT("   Status = %s"), EXmppPresenceStatus::ToString(Presence.Status));
	UE_LOG(LogXmpp, Verbose, TEXT("   bIsAvailable = %s"), Presence.bIsAvailable ? TEXT("true") : TEXT("false"));
	UE_LOG(LogXmpp, Verbose, TEXT("   SentTime = %s"), *Presence.SentTime.ToString());
	UE_LOG(LogXmpp, Verbose, TEXT("   AppId = %s"), *Presence.AppId);
	UE_LOG(LogXmpp, Verbose, TEXT("   Platform = %s"), *Presence.Platform);
	UE_LOG(LogXmpp, Verbose, TEXT("   StatusStr = %s"), *Presence.StatusStr);
}

bool FXmppPresenceJingle::UpdatePresence(const FXmppUserPresence& InPresence)
{
	bool bResult = false;

	CachedPresence = InPresence;
	if (PresenceSendTask != NULL)
	{
		CachedPresence.SentTime = FDateTime::UtcNow();
		ConvertFromPresence(CachedStatus, CachedPresence);

		UE_LOG(LogXmpp, Verbose, TEXT("Sending presence update for user [%s]"), UTF8_TO_TCHAR(CachedStatus.jid().node().c_str()));
		DebugPrintPresence(CachedPresence);

		bResult = PresenceUpdateRequests.Enqueue(new buzz::PresenceStatus(CachedStatus));
		NumPresenceOut++;
	}

	return bResult;
}

const FXmppUserPresence& FXmppPresenceJingle::GetPresence() const
{
	return CachedPresence;
}

bool FXmppPresenceJingle::QueryPresence(const FString& UserId)
{
	bool bResult = false;

//@todo samz - not supported properly by tigase
#if 0
	if (PresenceSendTask != NULL)
	{
		UE_LOG(LogXmpp, Verbose, TEXT("Querying presence for user [%s]"), *UserId);

		bResult = PresenceQueryRequests.Enqueue(FXmppUserJid(UserId));
		NumQueryRequests++;
	}
#endif

	return bResult;
}

TArray<TSharedPtr<FXmppUserPresence>> FXmppPresenceJingle::GetRosterPresence(const FString& UserId)
{
	FScopeLock Lock(&RosterLock);

	TArray<TSharedPtr<FXmppUserPresence>> Result;
	for (auto It = RosterPresence.CreateConstIterator(); It; ++It)
	{
		const FXmppUserPresenceJingle& PresenceJingle = It.Value();
		if (PresenceJingle.UserJid.Id == UserId)
		{
			Result.Add(PresenceJingle.Presence);
		}
	}
	return Result;
}

void FXmppPresenceJingle::GetRosterMembers(TArray<FXmppUserJid>& Members)
{
	FScopeLock Lock(&RosterLock);

	for (auto It = RosterPresence.CreateConstIterator(); It; ++It)
	{
		const FXmppUserPresenceJingle& PresenceJingle = It.Value();
		Members.AddUnique(PresenceJingle.UserJid);
	}
}

bool FXmppPresenceJingle::Tick(float DeltaTime)
{
	while (!RosterUpdates.IsEmpty())
	{
		FString UserId;
		if (RosterUpdates.Dequeue(UserId))
		{
			NumPresenceIn++;

			FScopeLock Lock(&RosterLock);
			const FXmppUserPresenceJingle* FoundEntry = RosterPresence.Find(UserId);
			if (FoundEntry != NULL)
			{
				OnReceivePresence().Broadcast(Connection.AsShared(), FoundEntry->UserJid, (*FoundEntry).Presence);
			}
		}
	}
	return true;
}

void FXmppPresenceJingle::HandlePumpStarting(buzz::XmppPump* XmppPump)
{
	if (PresenceSendTask == NULL)
	{
		PresenceSendTask = new FXmppPresenceOutTask(XmppPump->client());
		PresenceSendTask->Start();
	}
	if (PresenceRcvTask == NULL)
	{
		PresenceRcvTask = new FXmppPresenceReceiveTask(XmppPump->client(), TCHAR_TO_UTF8(*Connection.GetMucDomain()));
		PresenceRcvTask->SignalPresenceUpdate.connect(this, &FXmppPresenceJingle::OnSignalPresenceUpdate);
		PresenceRcvTask->MucSignalPresenceUpdate.connect(this, &FXmppPresenceJingle::OnSignalMucPresenceUpdate);
		PresenceRcvTask->Start();
	}
	CachedStatus.set_jid(XmppPump->client()->jid());
}

void FXmppPresenceJingle::HandlePumpQuitting(buzz::XmppPump* XmppPump)
{
	// delete happens automatically when tasks are completed
	if (PresenceRcvTask != NULL)
	{
		PresenceRcvTask->Abort(true);
		PresenceRcvTask = NULL;
	}
	if (PresenceSendTask != NULL)
	{
		PresenceSendTask->Abort(true);
		PresenceSendTask = NULL;
	}
	RosterPresence.Empty();
	CachedStatus.set_jid(buzz::Jid());
}

void FXmppPresenceJingle::HandlePumpTick(buzz::XmppPump* XmppPump)
{
	while (!PresenceUpdateRequests.IsEmpty())
	{
		buzz::PresenceStatus* NewStatus = NULL;
		if (PresenceUpdateRequests.Dequeue(NewStatus))
		{
			check(NewStatus != NULL);
			PresenceSendTask->Send(*NewStatus);
			delete NewStatus;
		}
	}
	while (!PresenceQueryRequests.IsEmpty())
	{
		FXmppUserJid QueryJid;
		if (PresenceQueryRequests.Dequeue(QueryJid))
		{
			buzz::Jid ToJid(TCHAR_TO_UTF8(*QueryJid.Id), XmppPump->client()->jid().domain(), buzz::STR_EMPTY);
			PresenceSendTask->SendProbe(ToJid);
		}
	}
}

void FXmppPresenceJingle::OnSignalPresenceUpdate(const buzz::PresenceStatus& InStatus)
{	
	FXmppUserJid UserJid;
	FXmppJingle::ConvertToJid(UserJid, InStatus.jid());

	// Don't keep presence entries missing a resource, this comes in when a new friend is added but will never get updated when user logs off
	if (UserJid.IsValid() && !UserJid.Resource.IsEmpty())
	{
		FScopeLock Lock(&RosterLock);
		
		FXmppUserPresenceJingle& RosterEntry = RosterPresence.FindOrAdd(UserJid.GetFullPath());
		ConvertToPresence(*RosterEntry.Presence, InStatus, UserJid);
		FXmppJingle::ConvertToJid(RosterEntry.UserJid, InStatus.jid());

		UE_LOG(LogXmpp, Verbose, TEXT("Received presence for user [%s]"), *UserJid.GetFullPath());
		DebugPrintPresence(*RosterEntry.Presence);

		RosterUpdates.Enqueue(UserJid.GetFullPath());
	}
	else if (UserJid.IsValid() && UserJid.Resource.IsEmpty())
	{
		UE_LOG(LogXmpp, Warning, TEXT("Ignoring presence update with empty resource. StatusJid = %s, JidFullPath = %s"), UTF8_TO_TCHAR(InStatus.jid().Str().c_str()), *UserJid.GetFullPath());
	}
}

void FXmppPresenceJingle::ConvertToMucPresence(FXmppMucPresence& OutMucPresence, const FXmppMucPresenceStatus& InMucStatus, const FXmppUserJid& InJid)
{
	const FString UserResource = FXmppUserJid::ParseMucUserResource(InJid.Resource);
	FXmppPresenceJingle::ConvertToPresence(OutMucPresence, InMucStatus, InJid, UserResource);

	OutMucPresence.Role = UTF8_TO_TCHAR(InMucStatus.get_role().c_str());
	OutMucPresence.Affiliation = UTF8_TO_TCHAR(InMucStatus.get_affiliation().c_str());
}

void FXmppPresenceJingle::OnSignalMucPresenceUpdate(const FXmppMucPresenceStatus& MucStatus)
{
	FXmppUserJid MucJid;
	FXmppJingle::ConvertToJid(MucJid, MucStatus.jid());

	UE_LOG(LogXmpp, Verbose, TEXT("Received MUC presence from [%s]"), *MucJid.GetFullPath());

	FXmppMucPresence MucPresence;
	ConvertToMucPresence(MucPresence, MucStatus, MucJid);
	Connection.MultiUserChat()->HandleMucPresence(MucPresence);
}

#endif //WITH_XMPP_JINGLE
