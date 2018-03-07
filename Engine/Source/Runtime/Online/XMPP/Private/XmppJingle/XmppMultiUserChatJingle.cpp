// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "XmppJingle/XmppMultiUserChatJingle.h"
#include "XmppLog.h"
#include "Misc/ScopeLock.h"
#include "Misc/ConfigCacheIni.h"
#include "Logging/LogScopedVerbosityOverride.h"
#include "XmppJingle/XmppConnectionJingle.h"


#if WITH_XMPP_JINGLE

inline FXmppChatMemberPtr FindExistingRoomMember(FXmppRoomJingle& XmppRoom, const FXmppUserJid& MemberJid)
{
	for (auto Member : XmppRoom.Members)
	{
		if (Member->MemberJid == MemberJid)
		{
			return Member;
		}
	}

	return nullptr;
}

/**
 * Response struct when a room config query task completes
 */
class FXmppConfigQueryResponseJingle
{
public:
	FXmppConfigQueryResponseJingle(FXmppRoomId InRoomId, bool InbSuccess, FString InErrorStr)
		: RoomId(InRoomId)
		, bSuccess(InbSuccess)
		, ErrorStr(InErrorStr)
	{}

	FXmppRoomId RoomId;
	bool bSuccess;
	FString ErrorStr;
};

/**
 * Response struct when a room config task completes
 */
class FXmppConfigResponseJingle
{
public:
	FXmppConfigResponseJingle(FXmppRoomId InRoomId, EConfigureRoomTypeJingle InRoomConfigurationType, bool InbSuccess, FString InErrorStr)
		: RoomId(InRoomId)
		, RoomConfigurationType(InRoomConfigurationType)
		, bSuccess(InbSuccess)
		, ErrorStr(InErrorStr)
	{}

	FXmppRoomId RoomId;
	EConfigureRoomTypeJingle RoomConfigurationType;
	bool bSuccess;
	FString ErrorStr;
};

/**
 * Response struct when a roominfo refresh task completes
 */
class FXmppRoomInfoRefreshResponseJingle
{
public:
	FXmppRoomInfoRefreshResponseJingle(FXmppRoomInfo InRoomInfo, bool InbSuccess, FString InErrorStr)
		: RoomInfo(InRoomInfo)
		, bSuccess(InbSuccess)
		, ErrorStr(InErrorStr)
	{}

	FXmppRoomInfo RoomInfo;
	bool bSuccess;
	FString ErrorStr;
};

/**
 * Task to query for configuration of a newly created room.  Required before sending configuration.
 */
class FXmppMucRoomQueryConfigTask : public buzz::IqTask
{
public:
	FXmppMucRoomQueryConfigTask(
		buzz::XmppTaskParentInterface* Parent,
		const buzz::Jid& RoomJid,
		FXmppRoomId InRoomId
		)
		: buzz::IqTask(Parent, buzz::STR_GET, RoomJid, MakeRequest())
		, RoomId(InRoomId)
	{
	}

	/** signal callback for when config query response is received & processed */
	sigslot::signal1<FXmppConfigQueryResponseJingle* /*ConfigQueryResponse*/> SignalConfigQueryReceived;

protected:
	virtual void HandleResult(const buzz::XmlElement* Stanza) override
	{
		UE_LOG(LogXmpp, VeryVerbose, TEXT("Handling result in queryconfigtask"));
		SignalConfigQueryReceived(new FXmppConfigQueryResponseJingle(RoomId, true, TEXT("")));
	}

private:
	static buzz::XmlElement* MakeRequest()
	{
		buzz::XmlElement* OwnerConfigQuery = new buzz::XmlElement(buzz::QN_MUC_OWNER_QUERY, true);

		// Add CorrelationID for tracking purposes
		FXmppJingle::AddCorrIdToStanza(*OwnerConfigQuery);

		return OwnerConfigQuery;
	}

	FXmppRoomId RoomId;
};

/**
 * Task to configure a newly created room
 */
class FXmppMucRoomConfigTask : public buzz::IqTask
{
public:
	FXmppMucRoomConfigTask(
		buzz::XmppTaskParentInterface* Parent,
		const buzz::Jid& RoomJid,
		FXmppRoomId InRoomId,
		EConfigureRoomTypeJingle InRoomConfigurationType,
		const FRoomFeatureValuePairs& RoomFeatureValuePairs
		)
		: buzz::IqTask(Parent, buzz::STR_SET, RoomJid, MakeFeaturesRequest(RoomFeatureValuePairs))
		, RoomId(InRoomId)
		, RoomConfigurationType(InRoomConfigurationType)
	{	
	}

	/** signal callback for when config attempt response is received & processed */
	sigslot::signal1<FXmppConfigResponseJingle* /*ConfigResponse*/> SignalConfigReceived;

protected:
	virtual void HandleResult(const buzz::XmlElement* Stanza) override
	{
		UE_LOG(LogXmpp, VeryVerbose, TEXT("Handling result in configtask"));
		
		// @todo en verify the stanza represents successful configuration like below
		// <iq from = "041173@muc.gamedev.ol.epicgames.net" to = "ced12d441326430c9d3d1242ea4810ac@gamedev.ol.epicgames.net/34a02cf8f4414e29b15921876da36f9a-26C546824E063DA2BD0A24866F1BC44C" xmlns = "jabber:client" type = "result" id = "10" / >

		// @todo en May need to pass back room info and original room config that was sent?
		SignalConfigReceived(new FXmppConfigResponseJingle(RoomId, RoomConfigurationType, true, TEXT("")));
	}

private:
	// Preserve DefaultRequest code to use server defaults.  Not currently used but likely we will want the option available
	static buzz::XmlElement* MakeDefaultRequest()
	{
		buzz::XmlElement* OwnerQuery = new buzz::XmlElement(buzz::QN_MUC_OWNER_QUERY, true);

		// Add CorrelationID for tracking purposes
		FXmppJingle::AddCorrIdToStanza(*OwnerQuery);

		buzz::XmlElement* XForm = new buzz::XmlElement(buzz::QN_XDATA_X, true);
		XForm->SetAttr(buzz::QN_TYPE, buzz::STR_SUBMIT);
		OwnerQuery->AddElement(XForm);
		return OwnerQuery;
	}

	static buzz::XmlElement* MakeFeaturesRequest(
		const FRoomFeatureValuePairs& RoomFeatureValuePairs)
	{
		buzz::XmlElement* OwnerQuery = new buzz::XmlElement(buzz::QN_MUC_OWNER_QUERY, true);

		// Add CorrelationID for tracking purposes
		FXmppJingle::AddCorrIdToStanza(*OwnerQuery);

		buzz::XmlElement* XForm = new buzz::XmlElement(buzz::QN_XDATA_X, true);
		XForm->SetAttr(buzz::QN_TYPE, buzz::STR_SUBMIT);

		// Add roomconfig as the form type being submitted
		buzz::XmlElement* FormTypeField = new buzz::XmlElement(buzz::QN_XDATA_FIELD, false);
		FormTypeField->SetAttr(buzz::QN_VAR, buzz::STR_FORM_TYPE);
		FormTypeField->SetAttr(buzz::QN_TYPE, buzz::STR_TEXT_SINGLE);

		buzz::XmlElement* FormTypeValue = new buzz::XmlElement(buzz::QN_XDATA_VALUE, false);
		FormTypeValue->SetBodyText(buzz::STR_MUC_ROOMCONFIG);

		FormTypeField->AddElement(FormTypeValue);
		XForm->AddElement(FormTypeField);

		// Setup other feature fields
		for (FRoomFeatureValuePairs::const_iterator FeaturePairIt = RoomFeatureValuePairs.begin()
			; FeaturePairIt != RoomFeatureValuePairs.end()
			; ++FeaturePairIt)
		{
			std::string FeatureStr = FeaturePairIt->first;
			std::string ValueStr = FeaturePairIt->second;
			buzz::XmlElement* FeatureField = new buzz::XmlElement(buzz::QN_XDATA_FIELD, false);
			FeatureField->SetAttr(buzz::QN_VAR, FeatureStr);
			FeatureField->SetAttr(buzz::QN_TYPE, buzz::STR_TEXT_SINGLE);

			buzz::XmlElement* FeatureValue = new buzz::XmlElement(buzz::QN_XDATA_VALUE, false);
			FeatureValue->SetBodyText(ValueStr);

			FeatureField->AddElement(FeatureValue);
			XForm->AddElement(FeatureField);
		}

		OwnerQuery->AddElement(XForm);
		return OwnerQuery;
	}

	FXmppRoomId RoomId;
	EConfigureRoomTypeJingle RoomConfigurationType;
};

FXmppMultiUserChatJingle::FXmppMultiUserChatJingle(class FXmppConnectionJingle& InConnection)
	: Connection(InConnection)
	, NumOpRequests(0)
	, NumMucResponses(0)
	, VerbosityIncreasedCount(0)
	, OriginalLogVerbosity(ELogVerbosity::NoLogging)
{
}

FXmppMultiUserChatJingle::~FXmppMultiUserChatJingle()
{
	FXmppRoomInfoRefreshResponseJingle* RefreshResp = nullptr;
	while (ReceivedRoomInfoRefreshResponseQueue.Dequeue(RefreshResp))
	{
		delete RefreshResp;
	}
	FXmppConfigQueryResponseJingle* ConfigQueryResp = nullptr;
	while (ReceivedConfigQueryResponseQueue.Dequeue(ConfigQueryResp))
	{
		delete ConfigQueryResp;
	}
	FXmppConfigResponseJingle* ConfigResp = nullptr;
	while (ReceivedConfigResponseQueue.Dequeue(ConfigResp))
	{
		delete ConfigResp;
	}
	FXmppChatRoomOpResult* RoomOpRes = nullptr;
	while (ResultOpQueue.Dequeue(RoomOpRes))
	{
		delete RoomOpRes;
	}
	FXmppChatRoomOp* PendingRoomOp = nullptr;
	while (PendingOpQueue.Dequeue(PendingRoomOp))
	{
		delete PendingRoomOp;
	}
}

/**
 * Result from operation for creating a room
 */
class FXmppChatRoomCreateOpResult : public FXmppChatRoomOpResult
{
public:
	FXmppChatRoomCreateOpResult(const FXmppRoomId& InRoomId, bool bInIsOwner, bool InbWasSuccessful, const FString& InErrorStr)
		: FXmppChatRoomOpResult(InRoomId, InbWasSuccessful, InErrorStr)
		, bIsOwner(bInIsOwner)
	{}

	bool bIsOwner;

	virtual void Process(class FXmppMultiUserChatJingle& Muc) override
	{
		if (!bWasSuccessful)
		{
			UE_LOG(LogXmpp, Warning, TEXT("MUC: CreateRoom [%s] failed. %s"), *RoomId, *ErrorStr);
		}
		else
		{
			UE_LOG(LogXmpp, Verbose, TEXT("MUC: CreateRoom [%s] succeeded."), *RoomId);
		}

		{
			FScopeLock Lock(&Muc.ChatroomsLock);
			FXmppRoomJingle* XmppRoom = Muc.Chatrooms.Find(RoomId);
			if (XmppRoom != nullptr)
			{
				XmppRoom->Status = bWasSuccessful ? FXmppRoomJingle::Joined : FXmppRoomJingle::NotJoined;
			}
		}

		const FXmppRoomConfig* RoomCreateConfig = Muc.PendingRoomCreateConfigs.Find(RoomId);
		if (RoomCreateConfig == nullptr)
		{
			bWasSuccessful = false;
			// @todo EN should exit room here if we can't configure it!
		}

		if (bIsOwner && bWasSuccessful)
		{
			Muc.InternalConfigureRoom(RoomId, FXmppRoomConfig(*RoomCreateConfig), EConfigureRoomTypeJingle::UseCreateCallback); //-V595
		}
		else
		{
			// Either failed or not owner, creation is done
			Muc.JoinRoomFinish();
			Muc.OnRoomCreated().Broadcast(Muc.Connection.AsShared(), bWasSuccessful, RoomId, ErrorStr);
		}

		if (RoomCreateConfig != nullptr)
		{
			// We always want to configure new rooms, so pass in the create we cached off earlier
			Muc.PendingRoomCreateConfigs.Remove(RoomId);
		}
	}

};

/**
 * Operation for creating a room
 */
class FXmppChatRoomCreateOp : public FXmppChatRoomOp
{
public:
	FXmppChatRoomCreateOp(const FXmppRoomId& InRoomId, const FString& InNickname)
		: FXmppChatRoomOp(InRoomId)
		, Nickname(InNickname)
	{}

	virtual bool AllowCreateRoom() const override { return true; }
	
	virtual FXmppChatRoomOpResult* Process(buzz::XmppChatroomModule* XmppRoom, buzz::XmppPump* XmppPump) override
	{
		if (XmppRoom->set_nickname(TCHAR_TO_UTF8(*Nickname)) != buzz::XMPP_RETURN_OK)
		{
			return ProcessError(FString::Printf(TEXT("failed set_nickname nickname=%s"), *Nickname));
		}
		else if (XmppRoom->RequestEnterChatroom(std::string(), std::string(), std::string()) != buzz::XMPP_RETURN_OK)
		{
			return ProcessError(FString::Printf(TEXT("failed RequestEnterChatroom room=%s"), *RoomId));
		}

        // Success, RequestEnterChatroom will trigger ChatroomEnteredStatus where things will proceed
		return nullptr;
	}

	virtual FXmppChatRoomOpResult* ProcessError(const FString& ErrorStr)
	{
		UE_LOG(LogXmpp, Verbose, TEXT("ChatRoomCreateOp returning error CreateOpResult for room %s"), *RoomId);
		return new FXmppChatRoomCreateOpResult(RoomId, false, false, ErrorStr);
	}

	FString Nickname;
};


void FXmppMultiUserChatJingle::JoinRoomStart()
{
	// Increase log verbosity if necessary
	bool bIncreaseVerbosity = false;
	if (GConfig->GetBool(TEXT("XMPP"), TEXT("IncreaseVerbosityDuringMUCJoin"), bIncreaseVerbosity, GEngineIni) && bIncreaseVerbosity)
	{
		VerbosityIncreasedCount++;
		if (VerbosityIncreasedCount == 1)
		{
#if !NO_LOGGING
			OriginalLogVerbosity = UE_GET_LOG_VERBOSITY(LogXmpp);

			LogXmpp.SetVerbosity(ELogVerbosity::VeryVerbose);
			UE_LOG(LogXmpp, VeryVerbose, TEXT("Increasing LogXmpp verbosity to VeryVerbose during room join"));
#endif
		}
	}
}

void FXmppMultiUserChatJingle::JoinRoomFinish()
{
	// Decrease log verbosity if necessary
	bool bIncreaseVerbosity = false;
	if (GConfig->GetBool(TEXT("XMPP"), TEXT("IncreaseVerbosityDuringMUCJoin"), bIncreaseVerbosity, GEngineIni) && bIncreaseVerbosity)
	{
		ensure(VerbosityIncreasedCount > 0);
		VerbosityIncreasedCount--;
		if (VerbosityIncreasedCount == 0)
		{
#if !NO_LOGGING
			UE_LOG(LogXmpp, VeryVerbose, TEXT("Decreasing LogXmpp verbosity after room join completion"));
			LogXmpp.SetVerbosity(OriginalLogVerbosity);
			OriginalLogVerbosity = ELogVerbosity::NoLogging;
#endif
		}
	}
}

bool FXmppMultiUserChatJingle::CreateRoom(const FXmppRoomId& RoomId, const FString& Nickname, const FXmppRoomConfig& RoomConfig)
{
	bool bResult = false;

	FString ErrorStr = TEXT("");

	if (RoomId.IsEmpty())
	{
		ErrorStr = TEXT("no valid room id");
	}
	else if (Nickname.IsEmpty())
	{
		ErrorStr = TEXT("no valid nickname");
	}
	else if (Connection.GetLoginStatus() != EXmppLoginStatus::LoggedIn)
	{
		ErrorStr = TEXT("not connected");
	}

	if (ErrorStr.IsEmpty())
	{
		FScopeLock Lock(&ChatroomsLock);
		FXmppRoomJingle& XmppRoom = Chatrooms.FindOrAdd(RoomId);
		if (XmppRoom.RoomInfo.Id.IsEmpty())
		{
			XmppRoom.RoomInfo.Id = RoomId;
		}
		if (XmppRoom.Status == FXmppRoomJingle::Joined)
		{
			ErrorStr = FString::Printf(TEXT("already joined room %s"), *RoomId);
		}
		else if (XmppRoom.Status != FXmppRoomJingle::NotJoined)
		{
			ErrorStr = FString::Printf(TEXT("operation pending for room %s"), *RoomId);
		}
		else
		{
			JoinRoomStart();

			// marked as pending until op is processed
			XmppRoom.Status = FXmppRoomJingle::CreatePending;
			// cache off the config for use after the room is created & ready to be configured
			PendingRoomCreateConfigs.Add(RoomId, FXmppRoomConfig(RoomConfig));
			// queue the create op
			UE_LOG(LogXmpp, Verbose, TEXT("Queueing FXmppChatRoomCreateOp for room %s"), *RoomId);
			bResult = PendingOpQueue.Enqueue(new FXmppChatRoomCreateOp(RoomId, Nickname));
			if (!bResult)
			{
				JoinRoomFinish();
			}
		}
	}

	if (!bResult)
	{
		UE_LOG(LogXmpp, Warning, TEXT("MUC: CreateRoom failed. %s"), *ErrorStr);
		// trigger delegates on error
		OnRoomCreated().Broadcast(Connection.AsShared(), false, RoomId, ErrorStr);
	}

	return bResult;
}

/**
 * Result from operation for joining a public room
 */
class FXmppChatRoomJoinPublicOpResult : public FXmppChatRoomOpResult
{
public:
	FXmppChatRoomJoinPublicOpResult(const FXmppRoomId& InRoomId, bool InbWasSuccessful, const FString& InErrorStr)
		: FXmppChatRoomOpResult(InRoomId, InbWasSuccessful, InErrorStr)
	{}

	virtual void Process(class FXmppMultiUserChatJingle& Muc) override
	{
		if (!bWasSuccessful)
		{
			UE_LOG(LogXmpp, Warning, TEXT("MUC: JoinPublicRoom [%s] failed. %s"), *RoomId, *ErrorStr);
		}
		else
		{
			UE_LOG(LogXmpp, Verbose, TEXT("MUC: JoinPublicRoom [%s] succeeded."), *RoomId);
		}

		Muc.JoinRoomFinish();
		Muc.OnJoinPublicRoom().Broadcast(Muc.Connection.AsShared(), bWasSuccessful, RoomId, ErrorStr);

		FScopeLock Lock(&Muc.ChatroomsLock);
		FXmppRoomJingle* XmppRoom = Muc.Chatrooms.Find(RoomId);
		if (XmppRoom != nullptr)
		{
			XmppRoom->Status = bWasSuccessful ? FXmppRoomJingle::Joined : FXmppRoomJingle::NotJoined;
		}
	}
};

/**
 * Operation for joining a public room
 */
class FXmppChatRoomJoinPublicOp : public FXmppChatRoomOp
{
public:
	FXmppChatRoomJoinPublicOp(const FXmppRoomId& InRoomId, const FString& InNickname)
		: FXmppChatRoomOp(InRoomId)
		, Nickname(InNickname)
	{}

	virtual bool AllowJoinRoom() const { return true; }

	virtual FXmppChatRoomOpResult* Process(buzz::XmppChatroomModule* XmppRoom, buzz::XmppPump* XmppPump) override
	{
		if (XmppRoom->set_nickname(TCHAR_TO_UTF8(*Nickname)) != buzz::XMPP_RETURN_OK)
		{
			return ProcessError(FString::Printf(TEXT("failed set_nickname nickname=%s"), *Nickname));
		}
		else if (XmppRoom->RequestEnterChatroom(std::string(), std::string(), std::string()) != buzz::XMPP_RETURN_OK)
		{
			return ProcessError(FString::Printf(TEXT("failed RequestEnterChatroom room=%s"), *RoomId));
		}
		return nullptr;
	}

	virtual FXmppChatRoomOpResult* ProcessError(const FString& ErrorStr)
	{
		UE_LOG(LogXmpp, Verbose, TEXT("ChatRoomJoinPublicOp returning error JoinPublicOpResult for room %s"), *RoomId);
		return new FXmppChatRoomJoinPublicOpResult(RoomId, false, ErrorStr);
	}

	FString Nickname;
};

bool FXmppMultiUserChatJingle::JoinPublicRoom(const FXmppRoomId& RoomId, const FString& Nickname)
{
	bool bResult = false;
	FString ErrorStr;

	if (RoomId.IsEmpty())
	{
		ErrorStr = TEXT("no valid room id");
	}
	else if (Nickname.IsEmpty())
	{
		ErrorStr = TEXT("no valid nickname");
	}
	else if (Connection.GetLoginStatus() != EXmppLoginStatus::LoggedIn)
	{
		ErrorStr = TEXT("not connected");
	}

	if (ErrorStr.IsEmpty())
	{
		FScopeLock Lock(&ChatroomsLock);
		FXmppRoomJingle& XmppRoom = Chatrooms.FindOrAdd(RoomId);
		if (XmppRoom.RoomInfo.Id.IsEmpty())
		{
			XmppRoom.RoomInfo.Id = RoomId;
		}
		if (XmppRoom.Status == FXmppRoomJingle::Joined)
		{
			ErrorStr = FString::Printf(TEXT("already joined room %s"), *RoomId);
		}
		else if (XmppRoom.Status != FXmppRoomJingle::NotJoined)
		{
			ErrorStr = FString::Printf(TEXT("operation pending for room %s"), *RoomId);
		}
		else
		{
			JoinRoomStart();
			// marked as pending until op is processed
			XmppRoom.Status = FXmppRoomJingle::JoinPublicPending;
			// queue the join op
			UE_LOG(LogXmpp, Verbose, TEXT("MUC: Queuing FXmppChatRoomJoinPublicOp for room %s"), *RoomId);
			bResult = PendingOpQueue.Enqueue(new FXmppChatRoomJoinPublicOp(RoomId, Nickname));
			if (!bResult)
			{
				JoinRoomFinish();
			}
		}
	}

	if (!bResult)
	{
		UE_LOG(LogXmpp, Verbose, TEXT("MUC: JoinPublicRoom failed. %s"), *ErrorStr);
		// trigger delegates on error
		OnJoinPublicRoom().Broadcast(Connection.AsShared(), bResult, RoomId, ErrorStr);
	}

	return bResult;
}

/**
 * Result from operation for joining a private room
 */
class FXmppChatRoomJoinPrivateOpResult : public FXmppChatRoomOpResult
{
public:
	FXmppChatRoomJoinPrivateOpResult(const FXmppRoomId& InRoomId, bool InbWasSuccessful, const FString& InErrorStr)
		: FXmppChatRoomOpResult(InRoomId, InbWasSuccessful, InErrorStr)
	{}

	virtual void Process(class FXmppMultiUserChatJingle& Muc) override
	{
		if (!bWasSuccessful)
		{
			UE_LOG(LogXmpp, Warning, TEXT("MUC: JoinPrivateRoom [%s] failed. %s"), *RoomId, *ErrorStr);
		}
		else
		{
			UE_LOG(LogXmpp, Verbose, TEXT("MUC: JoinPrivateRoom [%s] succeeded."), *RoomId);
		}

		Muc.JoinRoomFinish();
		Muc.OnJoinPrivateRoom().Broadcast(Muc.Connection.AsShared(), bWasSuccessful, RoomId, ErrorStr);

		FScopeLock Lock(&Muc.ChatroomsLock);
		FXmppRoomJingle* XmppRoom = Muc.Chatrooms.Find(RoomId);
		if (XmppRoom != nullptr)
		{
			XmppRoom->Status = bWasSuccessful ? FXmppRoomJingle::Joined : FXmppRoomJingle::NotJoined;
		}
	}
};

/**
 * Operation for joining a private room
 */
class FXmppChatRoomJoinPrivateOp : public FXmppChatRoomJoinPublicOp
{
public:
	FXmppChatRoomJoinPrivateOp(const FXmppRoomId& InRoomId, const FString& InNickname, const FString& InPassword)
		: FXmppChatRoomJoinPublicOp(InRoomId, InNickname)
		, Password(InPassword)
	{}

	virtual bool AllowJoinRoom() const { return true; }

	virtual FXmppChatRoomOpResult* Process(buzz::XmppChatroomModule* XmppRoom, buzz::XmppPump* XmppPump) override
	{
		if (XmppRoom->set_nickname(TCHAR_TO_UTF8(*Nickname)) != buzz::XMPP_RETURN_OK)
		{
			return ProcessError(FString::Printf(TEXT("failed set_nickname nickname=%s"), *Nickname));
		}
		else if (XmppRoom->RequestEnterChatroom(TCHAR_TO_UTF8(*Password), std::string(), std::string()) != buzz::XMPP_RETURN_OK)
		{
			return ProcessError(FString::Printf(TEXT("failed RequestEnterChatroom room=%s"), *RoomId));
		}
		return nullptr;
	}

	virtual FXmppChatRoomOpResult* ProcessError(const FString& ErrorStr)
	{
		UE_LOG(LogXmpp, Verbose, TEXT("ChatRoomJoinPrivateOp returning error JoinPrivateOpResult for room %s"), *RoomId);
		return new FXmppChatRoomJoinPrivateOpResult(RoomId, false, ErrorStr);
	}

	FString Password;
};

bool FXmppMultiUserChatJingle::JoinPrivateRoom(const FXmppRoomId& RoomId, const FString& Nickname, const FString& Password)
{
	bool bResult = false;
	FString ErrorStr;

	if (RoomId.IsEmpty())
	{
		ErrorStr = TEXT("no valid room id");
	}
	else if (Nickname.IsEmpty())
	{
		ErrorStr = TEXT("no valid nickname");
	}
	else if (Connection.GetLoginStatus() != EXmppLoginStatus::LoggedIn)
	{
		ErrorStr = TEXT("not connected");
	}

	if (ErrorStr.IsEmpty())
	{
		FScopeLock Lock(&ChatroomsLock);
		FXmppRoomJingle& XmppRoom = Chatrooms.FindOrAdd(RoomId);
		if (XmppRoom.RoomInfo.Id.IsEmpty())
		{
			XmppRoom.RoomInfo.Id = RoomId;
		}
		if (XmppRoom.Status == FXmppRoomJingle::Joined)
		{
			ErrorStr = FString::Printf(TEXT("already joined room %s"), *RoomId);
		}
		else if (XmppRoom.Status != FXmppRoomJingle::NotJoined)
		{
			ErrorStr = FString::Printf(TEXT("operation pending for room %s"), *RoomId);
		}
		else
		{
			JoinRoomStart();

			// marked as pending until op is processed
			XmppRoom.Status = FXmppRoomJingle::JoinPrivatePending;
			// queue the join op
			UE_LOG(LogXmpp, Verbose, TEXT("MUC: Queuing FXmppChatRoomJoinPrivateOp for room %s"), *RoomId);
			bResult = PendingOpQueue.Enqueue(new FXmppChatRoomJoinPrivateOp(RoomId, Nickname, Password));
			if (!bResult)
			{
				JoinRoomFinish();
			}
		}
	}

	if (!bResult)
	{
		UE_LOG(LogXmpp, Warning, TEXT("MUC: JoinPrivateRoom failed. %s"), *ErrorStr);
		// trigger delegates on error
		OnJoinPrivateRoom().Broadcast(Connection.AsShared(), bResult, RoomId, ErrorStr);
	}

	return bResult;
}

bool FXmppMultiUserChatJingle::RegisterMember(const FXmppRoomId& RoomId, const FString& Nickname)
{
	return false;

	// @todo samz
	// register membership for a members only room

// <iq from='crone1@shakespeare.lit/desktop'   //admin
//     id='uuid_1'
//     to='coven@chat.shakespeare.lit'
//     type='set'>
//   <query xmlns='http://jabber.org/protocol/muc#admin'>
//     <item affiliation='member'
//           jid='hag66@shakespeare.lit'
//           nick='thirdwitch'/>
//   </query>
// </iq>

}

bool FXmppMultiUserChatJingle::UnregisterMember(const FXmppRoomId& RoomId, const FString& Nickname)
{
	return false;

	//@todo samz
	// revoke membership for a members only room

// <iq from='crone1@shakespeare.lit/desktop'
//     id='uuid_2'
//     to='coven@chat.shakespeare.lit'
//     type='set'>
//   <query xmlns='http://jabber.org/protocol/muc#admin'>
//     <item affiliation='none'
//           jid='hag66@shakespeare.lit'/>
//   </query>
// </iq>

}

/**
 * Operation to query chat room configs
 */
class FXmppChatRoomConfigQueryOp : public FXmppChatRoomOp
{
public:
	FXmppChatRoomConfigQueryOp(FXmppMultiUserChatJingle& InMuc, const FXmppRoomId& InRoomId)
		: FXmppChatRoomOp(InRoomId)
		, Muc(InMuc)
	{}

	virtual FXmppChatRoomOpResult* Process(buzz::XmppChatroomModule* XmppRoom, buzz::XmppPump* XmppPump) override
	{
		FXmppMucRoomQueryConfigTask* RoomQueryConfigTask = new FXmppMucRoomQueryConfigTask(XmppPump->client(), XmppRoom->chatroom_jid(), RoomId);
		RoomQueryConfigTask->SignalConfigQueryReceived.connect(&Muc, &FXmppMultiUserChatJingle::OnSignalConfigQueryResponseReceived);
		RoomQueryConfigTask->Start();
		return nullptr;
	}
	
	virtual FXmppChatRoomOpResult* ProcessError(const FString& ErrorStr) 
	{
		return nullptr;
	}

	FXmppMultiUserChatJingle& Muc;
};

/**
 * Result from operation for configuring a room
 */
class FXmppChatRoomConfigOpResult : public FXmppChatRoomOpResult
{
public:
	FXmppChatRoomConfigOpResult(const FXmppRoomId& InRoomId, EConfigureRoomTypeJingle InRoomConfigurationType, bool InbWasSuccessful, const FString& InErrorStr)
		: FXmppChatRoomOpResult(InRoomId, InbWasSuccessful, InErrorStr)
		, RoomConfigurationType(InRoomConfigurationType)
	{}

	virtual void Process(class FXmppMultiUserChatJingle& Muc) override
	{
		if (!bWasSuccessful)
		{
			UE_LOG(LogXmpp, Warning, TEXT("MUC: ConfigureRoom [%s] failed. %s"), *RoomId, *ErrorStr);
		}
		else
		{
			UE_LOG(LogXmpp, Verbose, TEXT("MUC: ConfigureRoom [%s] succeeded."), *RoomId);
		}

		Muc.JoinRoomFinish();
		// Only call the appropriate callback for which op was requested
		if (RoomConfigurationType == EConfigureRoomTypeJingle::UseCreateCallback)
		{
			Muc.OnRoomCreated().Broadcast(Muc.Connection.AsShared(), bWasSuccessful, RoomId, ErrorStr);
		}
		else if(RoomConfigurationType == EConfigureRoomTypeJingle::UseConfigCallback)
		{
			Muc.OnRoomConfigured().Broadcast(Muc.Connection.AsShared(), bWasSuccessful, RoomId, ErrorStr);
		}
	}

	const EConfigureRoomTypeJingle RoomConfigurationType;
};

/**
 * Operation for configuring a new chat room
 */
class FXmppChatRoomConfigOp : public FXmppChatRoomOp
{
public:
	/**
	 * Default constructor to use server default config & not trigger any create/config callbacks
	 */
	FXmppChatRoomConfigOp(FXmppMultiUserChatJingle& InMuc, const FXmppRoomId& InRoomId, EConfigureRoomTypeJingle InRoomConfigurationType)
		: FXmppChatRoomOp(InRoomId)
		, Muc(InMuc)
		, RoomConfigurationType(InRoomConfigurationType)
	{}

	FXmppChatRoomConfigOp(FXmppMultiUserChatJingle& InMuc, const FXmppRoomId& InRoomId, EConfigureRoomTypeJingle InRoomConfigurationType, const FXmppRoomConfig& InRoomConfig)
		: FXmppChatRoomOp(InRoomId)
		, Muc(InMuc)
		, RoomConfigurationType(InRoomConfigurationType)
		, RoomConfig(InRoomConfig)
	{}

	virtual FXmppChatRoomOpResult* Process(buzz::XmppChatroomModule* XmppRoom, buzz::XmppPump* XmppPump) override
	{
		FRoomFeatureValuePairs RoomFeatureValuePairs; // @todo en - make this a single vector of feature/value pairs
		static const std::string QN_ROOMNAME = buzz::STR_MUC_ROOMCONFIG_ROOMNAME;
		static const std::string QN_MUC_DESCRIPTION = "muc#roomconfig_roomdesc";
		static const std::string QN_MUC_PERSISTENT = "muc#roomconfig_persistentroom";
		static const std::string QN_MUC_MAXHISTORY = "muc#maxhistoryfetch";
		static const std::string QN_MUC_CHANGESUBJECT = "muc#roomconfig_changesubject";
		static const std::string QN_MUC_ANONYMITY = "muc#roomconfig_anonymity";
		static const std::string QN_MUC_MEMBERSONLY = "muc#roomconfig_membersonly";
		static const std::string QN_MUC_MODERATED = "muc#roomconfig_moderatedroom";
		static const std::string QN_MUC_PUBLICROOM = "muc#roomconfig_publicroom";
		static const std::string QN_MUC_PASSWORDPROTECTED = "muc#roomconfig_passwordprotectedroom";
		static const std::string QN_MUC_ROOMSECRET = "muc#roomconfig_roomsecret";

		//@todo samz - hide private rooms

		RoomFeatureValuePairs.push_back(FRoomFeatureValuePair(QN_ROOMNAME, TCHAR_TO_UTF8(*RoomConfig.RoomName)));
		RoomFeatureValuePairs.push_back(FRoomFeatureValuePair(QN_MUC_DESCRIPTION, TCHAR_TO_UTF8(*RoomConfig.RoomDesc)));
		RoomFeatureValuePairs.push_back(FRoomFeatureValuePair(QN_MUC_PERSISTENT, RoomConfig.bIsPersistent ? "1" : "0"));
		RoomFeatureValuePairs.push_back(FRoomFeatureValuePair(QN_MUC_MAXHISTORY, TCHAR_TO_UTF8(*FString::Printf(TEXT("%d"), RoomConfig.MaxMsgHistory))));
		RoomFeatureValuePairs.push_back(FRoomFeatureValuePair(QN_MUC_CHANGESUBJECT, RoomConfig.bAllowChangeSubject ? "1" : "0"));
		RoomFeatureValuePairs.push_back(FRoomFeatureValuePair(QN_MUC_ANONYMITY, TCHAR_TO_UTF8(*FXmppRoomConfig::ConvertRoomAnonymityToString(RoomConfig.RoomAnonymity))));
		RoomFeatureValuePairs.push_back(FRoomFeatureValuePair(QN_MUC_MEMBERSONLY, RoomConfig.bIsMembersOnly ? "1" : "0"));
		RoomFeatureValuePairs.push_back(FRoomFeatureValuePair(QN_MUC_MODERATED, RoomConfig.bIsModerated ? "1" : "0"));
		RoomFeatureValuePairs.push_back(FRoomFeatureValuePair(QN_MUC_PUBLICROOM, RoomConfig.bAllowPublicSearch ? "1" : "0"));

		// Set values dependent on privacy
		RoomFeatureValuePairs.push_back(FRoomFeatureValuePair(QN_MUC_PASSWORDPROTECTED, RoomConfig.bIsPrivate ? "1" : "0"));
		if (RoomConfig.bIsPrivate)
		{
			RoomFeatureValuePairs.push_back(FRoomFeatureValuePair(QN_MUC_ROOMSECRET, TCHAR_TO_UTF8(*RoomConfig.Password)));
		}

		FXmppMucRoomConfigTask* RoomConfigTask = new FXmppMucRoomConfigTask(XmppPump->client(), XmppRoom->chatroom_jid(), RoomId, RoomConfigurationType, RoomFeatureValuePairs);
		RoomConfigTask->SignalConfigReceived.connect(&Muc, &FXmppMultiUserChatJingle::OnSignalConfigResponseReceived);
		RoomConfigTask->Start();

		return nullptr;
	}

	virtual FXmppChatRoomOpResult* ProcessError(const FString& ErrorStr) 
	{
		UE_LOG(LogXmpp, Verbose, TEXT("ChatRoomConfigOp returning error ChatRoomConfigOpResult for room %s"), *RoomId);
		return new FXmppChatRoomConfigOpResult(RoomId, RoomConfigurationType, false, ErrorStr);
	}

	FXmppMultiUserChatJingle& Muc;
	EConfigureRoomTypeJingle RoomConfigurationType;
	FXmppRoomConfig RoomConfig;
};

bool FXmppMultiUserChatJingle::ConfigureRoom(const FXmppRoomId& RoomId, const FXmppRoomConfig& RoomConfig)
{
	return InternalConfigureRoom(RoomId, RoomConfig, EConfigureRoomTypeJingle::UseConfigCallback);
}

bool FXmppMultiUserChatJingle::InternalConfigureRoom(const FXmppRoomId& RoomId, const FXmppRoomConfig& RoomConfig, EConfigureRoomTypeJingle RoomConfigurationType)
{
	bool bResult = false;
	FString ErrorStr;

	FScopeLock Lock(&ChatroomsLock);
	FXmppRoomJingle* XmppRoom = Chatrooms.Find(RoomId);
	if (XmppRoom == nullptr)
	{
		ErrorStr = FString::Printf(TEXT("couldnt find room %s"), *RoomId);
	}
	else if (XmppRoom->Status != FXmppRoomJingle::Joined)
	{
		ErrorStr = FString::Printf(TEXT("have not joined room %s"), *RoomId);
	}
	else if (Connection.GetLoginStatus() != EXmppLoginStatus::LoggedIn)
	{
		ErrorStr = TEXT("not connected");
	}
//	@todo EN are these userId values the same?  Currently ownerid isn't filled out here at all.   Where does RoomInfo come from?
// 	else if (XmppRoom->RoomInfo.OwnerId != Connection.GetUserJid().Id)
// 	{
// 		ErrorStr = FString::Printf(TEXT("attempt to configure room when not the owner %s %s"), *RoomId, *XmppRoom->RoomInfo.OwnerId);
// 	}
	else
	{
		// now go configure the xmpp room with our configuration settings
		// hack - send a config request but we won't actually listen for it or parse it, since we hardcode to 1 server's features
		UE_LOG(LogXmpp, Verbose, TEXT("ConfigureRoom queuing FXmppChatRoomConfigQueryOp for room %s"), *RoomId);
		PendingOpQueue.Enqueue(new FXmppChatRoomConfigQueryOp(*this, RoomId));

		// Queue sending the room config, include whether to fire roomcreate delegate when it finishes or fails
		UE_LOG(LogXmpp, Verbose, TEXT("ConfigureRoom queuing FXmppChatRoomConfigOp for room %s"), *RoomId);
		bResult = PendingOpQueue.Enqueue(new FXmppChatRoomConfigOp(*this, RoomId, RoomConfigurationType, RoomConfig));
	}

	if (!bResult)
	{
		UE_LOG(LogXmpp, Warning, TEXT("MUC: ConfigureRoom failed. %s"), *ErrorStr);

		// trigger the delegate for the request that the caller made, either create or configure
		if (RoomConfigurationType == EConfigureRoomTypeJingle::UseCreateCallback)
		{
			JoinRoomFinish();
			OnRoomCreated().Broadcast(Connection.AsShared(), false, RoomId, ErrorStr);
		}
		else if (RoomConfigurationType == EConfigureRoomTypeJingle::UseConfigCallback)
		{
			OnRoomConfigured().Broadcast(Connection.AsShared(), false, RoomId, ErrorStr);
		}
	}

	return bResult;
}

/**
 * Result from operation for refreshing room info
 */
class FXmppChatRoomInfoRefreshOpResult : public FXmppChatRoomOpResult
{
public:
	FXmppChatRoomInfoRefreshOpResult(const FXmppRoomId& InRoomId, bool InbWasSuccessful, const FString& InErrorStr)
		: FXmppChatRoomOpResult(InRoomId, InbWasSuccessful, InErrorStr)
	{}

	virtual void Process(class FXmppMultiUserChatJingle& Muc) override
	{
		if (!bWasSuccessful)
		{
			UE_LOG(LogXmpp, Warning, TEXT("MUC: ConfigureRoom [%s] failed. %s"), *RoomId, *ErrorStr);
		}
		else
		{
			UE_LOG(LogXmpp, Verbose, TEXT("MUC: ConfigureRoom [%s] succeeded."), *RoomId);
		}

		Muc.OnRoomInfoRefreshed().Broadcast(Muc.Connection.AsShared(), bWasSuccessful, RoomId, ErrorStr);
	}
};

/**
 * Operation for refreshing chatroom info / features
 */
class FXmppChatRoomInfoRefreshOp : public FXmppChatRoomOp
{
public:
	FXmppChatRoomInfoRefreshOp(FXmppMultiUserChatJingle& InMuc, const FXmppRoomId& InRoomId)
		: FXmppChatRoomOp(InRoomId)
		, Muc(InMuc)
	{}

	virtual FXmppChatRoomOpResult* Process(buzz::XmppChatroomModule* XmppRoom, buzz::XmppPump* XmppPump) override
	{
		buzz::MucRoomDiscoveryTask* RoomInfoRefreshTask = new buzz::MucRoomDiscoveryTask(XmppPump->client(), XmppRoom->chatroom_jid());
		RoomInfoRefreshTask->SignalResult.connect(&Muc, &FXmppMultiUserChatJingle::OnSignalRoomInfoRefreshReceived);
		RoomInfoRefreshTask->Start();

		return nullptr;
	}

	virtual FXmppChatRoomOpResult* ProcessError(const FString& ErrorStr)
	{
		UE_LOG(LogXmpp, Verbose, TEXT("ChatRoomInfoRefreshOp returning error ChatRoomInfoRefreshOpResult for room %s"), *RoomId);
		return new FXmppChatRoomInfoRefreshOpResult(RoomId, false, ErrorStr);
	}

	FXmppMultiUserChatJingle& Muc;
};

bool FXmppMultiUserChatJingle::RefreshRoomInfo(const FXmppRoomId& RoomId)
{
	bool bResult = false;
	FString ErrorStr;

	FScopeLock Lock(&ChatroomsLock);
	FXmppRoomJingle* XmppRoom = Chatrooms.Find(RoomId);
	if (XmppRoom == nullptr)
	{
		ErrorStr = FString::Printf(TEXT("couldnt find room %s"), *RoomId);
	}
	else if (XmppRoom->Status != FXmppRoomJingle::Joined)
	{
		ErrorStr = FString::Printf(TEXT("have not joined room %s"), *RoomId);
	}
	else if (Connection.GetLoginStatus() != EXmppLoginStatus::LoggedIn)
	{
		ErrorStr = TEXT("not connected");
	}
	else
	{
		// now go refresh the roominfo
		UE_LOG(LogXmpp, Verbose, TEXT("RefreshRoomInfo queuing FXmppChatRoomInfoRefreshOp for room %s"), *RoomId);
		bResult = PendingOpQueue.Enqueue(new FXmppChatRoomInfoRefreshOp(*this, RoomId));
	}

	if (!bResult)
	{
		UE_LOG(LogXmpp, Warning, TEXT("MUC: RefreshRoomInfo failed. %s"), *ErrorStr);

		// trigger delegates for refresh on error
		OnRoomInfoRefreshed().Broadcast(Connection.AsShared(), false, RoomId, ErrorStr);
	}

	return bResult;
}

/**
 * Result from operation for exiting a room
 */
class FXmppChatRoomExitOpResult : public FXmppChatRoomOpResult
{
public:
	FXmppChatRoomExitOpResult(const FXmppRoomId& InRoomId, bool InbWasSuccessful, const FString& InErrorStr)
		: FXmppChatRoomOpResult(InRoomId, InbWasSuccessful, InErrorStr)
	{}

	virtual void Process(class FXmppMultiUserChatJingle& Muc) override
	{
		Muc.OnExitRoom().Broadcast(Muc.Connection.AsShared(), bWasSuccessful, RoomId, ErrorStr);

		// clear out cached room
		FScopeLock Lock(&Muc.ChatroomsLock);
		Muc.Chatrooms.Remove(RoomId);
	}
};

/**
 * Operation for exiting a room
 */
class FXmppChatRoomExitOp : public FXmppChatRoomOp
{
public:
	FXmppChatRoomExitOp(const FXmppRoomId& InRoomId)
		: FXmppChatRoomOp(InRoomId)
	{}

	virtual FXmppChatRoomOpResult* Process(buzz::XmppChatroomModule* XmppRoom, buzz::XmppPump* XmppPump) override
	{
		if (XmppRoom->RequestExitChatroom() != buzz::XMPP_RETURN_OK)
		{
			return ProcessError(FString::Printf(TEXT("failed RequestExitChatroom room=%s"), *RoomId));
		}
		return nullptr;
	}

	virtual FXmppChatRoomOpResult* ProcessError(const FString& ErrorStr)
	{
		UE_LOG(LogXmpp, Verbose, TEXT("ChatRoomExitOp returning error ChatRoomExitOpResult for room %s"), *RoomId);
		return new FXmppChatRoomExitOpResult(RoomId, false, ErrorStr);
	}
};

bool FXmppMultiUserChatJingle::ExitRoom(const FXmppRoomId& RoomId)
{
	bool bResult = false;
	FString ErrorStr;

	FScopeLock Lock(&ChatroomsLock);
	FXmppRoomJingle* XmppRoom = Chatrooms.Find(RoomId);
	if (XmppRoom == nullptr)
	{
		ErrorStr = FString::Printf(TEXT("couldnt find room %s"), *RoomId);
	}
	else if(XmppRoom->Status != FXmppRoomJingle::ExitPending)
	{
		XmppRoom->Status = FXmppRoomJingle::ExitPending;
		// queue the exit op
		UE_LOG(LogXmpp, Verbose, TEXT("ExitRoom queuing FXmppChatRoomExitOp for room %s"), *RoomId);
		bResult = PendingOpQueue.Enqueue(new FXmppChatRoomExitOp(RoomId));
	}
	else
	{
		UE_LOG(LogXmpp, Verbose, TEXT("ExitRoom operation pending for room %s"), *RoomId);
		// Don't trigger the delegates it will ruin the original call that is waiting for the actual success/failure.
        bResult = true;
	}

	if (!bResult)
	{
		UE_LOG(LogXmpp, Warning, TEXT("MUC: ExitRoom failed. %s"), *ErrorStr);
		// trigger delegates on error
		OnExitRoom().Broadcast(Connection.AsShared(), bResult, RoomId, ErrorStr);
	}

	return bResult;
}

/**
 * Operation for sending a chat message to a room
 */
class FXmppChatRoomSendChatOp : public FXmppChatRoomOp
{
public:
	FXmppChatRoomSendChatOp(const FXmppRoomId& InRoomId, const FString& InMsgBody, const FString& InChatInfoJson)
		: FXmppChatRoomOp(InRoomId)
		, MsgBody(InMsgBody)
		, ChatInfoJson(InChatInfoJson)
	{}

	virtual FXmppChatRoomOpResult* Process(buzz::XmppChatroomModule* XmppRoom, buzz::XmppPump* XmppPump) override
	{
		static const std::string ChatType = "groupchat";
		static const buzz::StaticQName QN_CHATINFO = { buzz::NS_CLIENT, "chat-info" };

		buzz::XmlElement Message(buzz::QN_MESSAGE);
		Message.AddAttr(buzz::QN_TO, XmppRoom->chatroom_jid().Str());
		Message.AddAttr(buzz::QN_TYPE, ChatType);

		// Add CorrelationID for tracking purposes
		FXmppJingle::AddCorrIdToStanza(Message);

		// Message body
		buzz::XmlElement* Body = new buzz::XmlElement(buzz::QN_BODY);
		Body->SetBodyText(TCHAR_TO_UTF8(*MsgBody));
		Message.AddElement(Body);

		// Add chat info for extra processing
		if (!ChatInfoJson.IsEmpty())
		{
			buzz::XmlElement* ChatInfoElement = new buzz::XmlElement(QN_CHATINFO);
			ChatInfoElement->SetBodyText(TCHAR_TO_UTF8(*ChatInfoJson));
			Message.AddElement(ChatInfoElement);
		}

		if (XmppRoom->SendMessage(Message) != buzz::XMPP_RETURN_OK)
		{
			return ProcessError(FString::Printf(TEXT("failed SendMessage to room=%s"), *RoomId));
		}
		return nullptr;
	}

	virtual FXmppChatRoomOpResult* ProcessError(const FString& ErrorStr)
	{
		//@todo sz1 - queue result
		return nullptr;
	}

	// message payload which is visible
	FString MsgBody;
	// optional json payload for extra info associated with chat message
	FString ChatInfoJson;
};

bool FXmppMultiUserChatJingle::SendChat(const FXmppRoomId& RoomId, const FString& MsgBody, const FString& ChatInfo)
{
	bool bResult = false;
	FString ErrorStr;

	FScopeLock Lock(&ChatroomsLock);
	FXmppRoomJingle* XmppRoom = Chatrooms.Find(RoomId);
	if (XmppRoom == nullptr)
	{
		ErrorStr = FString::Printf(TEXT("couldnt find room %s"), *RoomId);
	}
	else if (XmppRoom->Status != FXmppRoomJingle::Joined)
	{
		ErrorStr = FString::Printf(TEXT("have not joined room %s"), *RoomId);
	}
	else if (Connection.GetLoginStatus() != EXmppLoginStatus::LoggedIn)
	{
		ErrorStr = TEXT("not connected");
	}
	else
	{
		// queue the chat message op
		UE_LOG(LogXmpp, Verbose, TEXT("SendChat queuing FXmppChatRoomSendChatOp for room %s"), *RoomId);
		bResult = PendingOpQueue.Enqueue(new FXmppChatRoomSendChatOp(RoomId, MsgBody, ChatInfo));
	}

	if (!bResult)
	{
		UE_LOG(LogXmpp, Warning, TEXT("MUC: SendChat failed. %s"), *ErrorStr);
	}

	return bResult;
}

void FXmppMultiUserChatJingle::GetJoinedRooms(TArray<FXmppRoomId>& OutRooms)
{
	FScopeLock Lock(&ChatroomsLock);
	for (auto Entry : Chatrooms)
	{
		const FXmppRoomJingle& Room = Entry.Value;
		if (Room.Status == FXmppRoomJingle::Joined)
		{
			OutRooms.Add(Room.RoomInfo.Id);
		}
	}
}

bool FXmppMultiUserChatJingle::GetRoomInfo(const FXmppRoomId& RoomId, FXmppRoomInfo& OutRoomInfo)
{
	bool bResult = false;

	FScopeLock Lock(&ChatroomsLock);
	for (auto Entry : Chatrooms)
	{
		const FXmppRoomJingle& Room = Entry.Value;
		if (Room.RoomInfo.Id == RoomId)
		{
			OutRoomInfo = Room.RoomInfo;
			bResult = true;
			break;
		}
	}
	return bResult;
}

bool FXmppMultiUserChatJingle::GetMembers(const FXmppRoomId& RoomId, TArray<FXmppChatMemberRef>& OutMembers)
{
	bool bResult = false;

	FScopeLock Lock(&ChatroomsLock);
	for (auto Entry : Chatrooms)
	{
		const FXmppRoomJingle& Room = Entry.Value;
		if (Room.RoomInfo.Id == RoomId)
		{
			OutMembers = Room.Members;
			bResult = true;
			break;
		}
	}
	return bResult;
}

FXmppChatMemberPtr FXmppMultiUserChatJingle::GetMember(const FXmppRoomId& RoomId, const FXmppUserJid& MemberJid)
{
	FXmppChatMemberPtr Result;

	FScopeLock Lock(&ChatroomsLock);
	for (auto Entry : Chatrooms)
	{
		const FXmppRoomJingle& Room = Entry.Value;
		if (Room.RoomInfo.Id == RoomId)
		{
			for (auto Member : Room.Members)
			{
				if (Member->MemberJid == MemberJid)
				{
					Result = Member;
					break;
				}
			}
		}
	}
	return Result;
}

bool FXmppMultiUserChatJingle::GetLastMessages(const FXmppRoomId& RoomId, int32 NumMessages, TArray< TSharedRef<FXmppChatMessage> >& OutMessages)
{
	bool bResult = false;

	FScopeLock Lock(&ChatroomsLock);
	for (auto Entry : Chatrooms)
	{
		const FXmppRoomJingle& Room = Entry.Value;
		if (Room.RoomInfo.Id == RoomId)
		{
			// copy all
			if (NumMessages < 0)
			{
				OutMessages = Room.LastMessages;
			}
			else
			// copy only up to NumMessages
			{
				for (int32 Idx = 0; Idx < Room.LastMessages.Num() && Idx < NumMessages; Idx++)
				{
					OutMessages.Add(Room.LastMessages[Idx]);
				}
			}
			bResult = true;
			break;
		}
	}
	return bResult;
}

bool FXmppMultiUserChatJingle::Tick(float DeltaTime)
{
	while (!ResultOpQueue.IsEmpty())
	{
		FXmppChatRoomOpResult* ResultOp = nullptr;
		if (ResultOpQueue.Dequeue(ResultOp) &&
			ResultOp != nullptr)
		{
			NumOpRequests++;
			ProcessResultOp(ResultOp, *this);
			delete ResultOp;
		}
	}

	while (!ReceivedConfigQueryResponseQueue.IsEmpty())
	{
		FXmppConfigQueryResponseJingle* ConfigQueryTaskResponse = nullptr;
		if (ReceivedConfigQueryResponseQueue.Dequeue(ConfigQueryTaskResponse))
		{
			NumMucResponses++;
			// We currently ignore this feature list response because we only work with 1 xmpp server so far & hardcode to those features
			UE_LOG(LogXmpp, Verbose, TEXT("Received config query response %d for room %s"), ConfigQueryTaskResponse->bSuccess, *ConfigQueryTaskResponse->RoomId);
			delete ConfigQueryTaskResponse;
		}
	}

	while (!ReceivedConfigResponseQueue.IsEmpty())
	{
		FXmppConfigResponseJingle* ConfigTaskResponse = nullptr;
		if (ReceivedConfigResponseQueue.Dequeue(ConfigTaskResponse))
		{
			NumMucResponses++;
			UE_LOG(LogXmpp, Verbose, TEXT("Received config response %d for room %s"), ConfigTaskResponse->bSuccess, *ConfigTaskResponse->RoomId);
			if (ConfigTaskResponse->RoomConfigurationType == EConfigureRoomTypeJingle::UseCreateCallback)
			{
				JoinRoomFinish();
				OnRoomCreated().Broadcast(Connection.AsShared(), ConfigTaskResponse->bSuccess, ConfigTaskResponse->RoomId, ConfigTaskResponse->ErrorStr);
			}
			else if (ConfigTaskResponse->RoomConfigurationType == EConfigureRoomTypeJingle::UseConfigCallback)
			{
				OnRoomConfigured().Broadcast(Connection.AsShared(), ConfigTaskResponse->bSuccess, ConfigTaskResponse->RoomId, ConfigTaskResponse->ErrorStr);
			}
			delete ConfigTaskResponse;
		}
	}
	
	while (!ReceivedRoomInfoRefreshResponseQueue.IsEmpty())
	{
		FXmppRoomInfoRefreshResponseJingle* RefreshTaskResponse = nullptr;
		if (ReceivedRoomInfoRefreshResponseQueue.Dequeue(RefreshTaskResponse))
		{
			NumMucResponses++;
			UE_LOG(LogXmpp, Verbose, TEXT("Received refresh room info response %d for room %s"), RefreshTaskResponse->bSuccess, *RefreshTaskResponse->RoomInfo.Id);
			OnRoomInfoRefreshed().Broadcast(Connection.AsShared(), RefreshTaskResponse->bSuccess, RefreshTaskResponse->RoomInfo.Id, RefreshTaskResponse->ErrorStr);
			delete RefreshTaskResponse;
		}
	}

	return true;
}

void FXmppMultiUserChatJingle::HandlePumpStarting(buzz::XmppPump* XmppPump)
{
	
}

void FXmppMultiUserChatJingle::HandlePumpQuitting(buzz::XmppPump* XmppPump)
{
	// remove any pending tasks
	while (!PendingOpQueue.IsEmpty())
	{
		FXmppChatRoomOp* PendingOp = nullptr;
		if (PendingOpQueue.Dequeue(PendingOp) &&
			PendingOp != nullptr)
		{
			FXmppChatRoomOpResult* OpResult = PendingOp->ProcessError(TEXT("failed to process - shutting down"));
			if (OpResult != nullptr)
			{
				ResultOpQueue.Enqueue(OpResult);
			}
			delete PendingOp;
		}
	}
	// make sure all chat room tasks are cleaned up
	for (auto It(XmppRoomModules.CreateIterator()); It; ++It)
	{
		buzz::XmppChatroomModule* XmppRoomModule = It.Value();
		delete XmppRoomModule;
		It.RemoveCurrent();
	}

	// clear out chat rooms on xmpp shutdown
	FScopeLock Lock(&ChatroomsLock);
	Chatrooms.Empty();
}

void FXmppMultiUserChatJingle::HandlePumpTick(buzz::XmppPump* XmppPump)
{
	if (Connection.GetLoginStatus() == EXmppLoginStatus::LoggedIn)
	{
		// process any pending ops on the xmpp thread
		while (!PendingOpQueue.IsEmpty())
		{
			FXmppChatRoomOp* PendingOp = nullptr;
			if (PendingOpQueue.Dequeue(PendingOp) &&
				PendingOp != nullptr)
			{
				ProcessPendingOp(PendingOp, XmppPump);
				delete PendingOp;
			}
		}
	}
	else
	{
		// not logged in so process all pending operations as error
		while (!PendingOpQueue.IsEmpty())
		{
			FXmppChatRoomOp* PendingOp = nullptr;
			if (PendingOpQueue.Dequeue(PendingOp) &&
				PendingOp != nullptr)
			{
				FXmppChatRoomOpResult* OpResult = PendingOp->ProcessError(TEXT("failed to process - not connected"));
				if (OpResult != nullptr)
				{
					ResultOpQueue.Enqueue(OpResult);
				}
				delete PendingOp;
			}
		}
	}
}

void FXmppMultiUserChatJingle::OnSignalConfigQueryResponseReceived(FXmppConfigQueryResponseJingle* ConfigQueryResponse)
{
	UE_LOG(LogXmpp, Verbose, TEXT("Enqueuing ConfigQueryResponse"));
	ReceivedConfigQueryResponseQueue.Enqueue(ConfigQueryResponse);
}

void FXmppMultiUserChatJingle::OnSignalConfigResponseReceived(FXmppConfigResponseJingle* ConfigResponse)
{
	UE_LOG(LogXmpp, Verbose, TEXT("Enqueuing ConfigResponse"));
	ReceivedConfigResponseQueue.Enqueue(ConfigResponse);
}

void FXmppMultiUserChatJingle::OnSignalRoomInfoRefreshReceived(buzz::MucRoomDiscoveryTask* RefreshTask, bool bExists, const std::string& Name, const std::string& RoomId, const std::set<std::string>& Features, const std::map<std::string, std::string>& ExtendedInfo)
{
	bool bSuccess = true;
	FString ErrorStr;
	FXmppRoomInfo RoomInfo;
	FString NameFStr = UTF8_TO_TCHAR(Name.c_str());
	FString RoomIdFStr = UTF8_TO_TCHAR(RoomId.c_str());

	if (!bExists)
	{
		bSuccess = false;
		ErrorStr = FString::Printf(TEXT("RoomInfoRefresh: Room does not exist for room id: %s / name: %s"), *RoomIdFStr, *NameFStr);
	}
	else
	{
		for (std::string Feature : Features)
		{
			FString FeatureFStr = UTF8_TO_TCHAR(Feature.c_str());
			UE_LOG(LogXmpp, VeryVerbose, TEXT("RoomInfoRefresh: Room %s has feature %s"), *RoomIdFStr, *FeatureFStr);
		}
		for (std::map<std::string, std::string>::const_iterator ExtendedInfoIt = ExtendedInfo.begin(); ExtendedInfoIt != ExtendedInfo.end(); ++ExtendedInfoIt)
		{
			std::pair<std::string, std::string> InfoRow = *ExtendedInfoIt;
			FString FirstFStr = UTF8_TO_TCHAR(InfoRow.first.c_str());
			FString SecondFStr = UTF8_TO_TCHAR(InfoRow.second.c_str());
			UE_LOG(LogXmpp, VeryVerbose, TEXT("RoomInfoRefresh: Room %s has ext info %s %s"), *RoomIdFStr, *FirstFStr, *SecondFStr);
		}
	}

	// @todo EN update info for this room before queuing sending response up the chain for delegates?
	UE_LOG(LogXmpp, Verbose, TEXT("Queueing FXmppRoomInfoRefreshResponse for room %s"), *RoomIdFStr);
	FXmppRoomInfoRefreshResponseJingle* RefreshResponse = new FXmppRoomInfoRefreshResponseJingle(RoomInfo, bSuccess, ErrorStr);
	ReceivedRoomInfoRefreshResponseQueue.Enqueue(RefreshResponse);
}

void FXmppMultiUserChatJingle::ProcessPendingOp(FXmppChatRoomOp* PendingOp, buzz::XmppPump* XmppPump)
{
	check(!IsInGameThread());

	FString ErrorStr;

	buzz::XmppChatroomModule* XmppRoom = nullptr;
	buzz::XmppChatroomModule** Existing = XmppRoomModules.Find(PendingOp->RoomId);
	if (Existing != nullptr)
	{
		XmppRoom = *Existing;
	}
	else if (PendingOp->AllowCreateRoom() ||
			 PendingOp->AllowJoinRoom())
	{
		XmppRoom = XmppRoomModules.Add(PendingOp->RoomId, buzz::XmppChatroomModule::Create());
		
		buzz::Jid RoomJid(
			TCHAR_TO_UTF8(*PendingOp->RoomId),
			TCHAR_TO_UTF8(*Connection.GetMucDomain()),
			std::string());

		if (XmppRoom->set_chatroom_jid(RoomJid) != buzz::XMPP_RETURN_OK)
		{
			ErrorStr = TEXT("failed set_chatroom_jid");
		}
		else if (XmppRoom->set_chatroom_handler(this) != buzz::XMPP_RETURN_OK)
		{
			ErrorStr = TEXT("failed set_chatroom_handler");
		}
		else if (XmppRoom->RegisterEngine(XmppPump->client()->engine()) != buzz::XMPP_RETURN_OK)
		{
			ErrorStr = TEXT("failed RegisterEngine");
		}
		if (!ErrorStr.IsEmpty())
		{
			UE_LOG(LogXmpp, Warning, TEXT("Muc create failed for chatroom jid=%s. %s"), 
				UTF8_TO_TCHAR(RoomJid.Str().c_str()), *ErrorStr);

			XmppRoomModules.Remove(PendingOp->RoomId);
			delete XmppRoom;
			XmppRoom = nullptr;
		}
	}
	// result from processing
	FXmppChatRoomOpResult* OpResult = nullptr;
	if (XmppRoom != nullptr)
	{
		OpResult = PendingOp->Process(XmppRoom, XmppPump);
	}
	else
	{
		OpResult = PendingOp->ProcessError(ErrorStr);
	}
	if (OpResult != nullptr)
	{
		// queue for processing result on game thread
		ResultOpQueue.Enqueue(OpResult);
	}
}

void FXmppMultiUserChatJingle::ProcessResultOp(FXmppChatRoomOpResult* ResultOp, class FXmppMultiUserChatJingle& Muc)
{
	check(IsInGameThread());
	check(ResultOp != nullptr);

	ResultOp->Process(Muc);
}

static void DecodeChatMemberPresence(FXmppChatMember& OutChatMember, const buzz::XmppPresence* Presence)
{
	if (Presence)
	{
		const buzz::XmlElement* Xml = Presence->raw_xml();
		if (Xml)
		{
			const buzz::XmlChild* XChild = Xml->FirstNamed(buzz::QN_MUC_USER_X);
			if (XChild && XChild->AsElement())
			{
				const buzz::XmlElement* Element = XChild->AsElement();
				const buzz::XmlChild* ItemChild = Element->FirstNamed(buzz::QN_MUC_USER_ITEM);
				if (ItemChild && ItemChild->AsElement())
				{
					const buzz::XmlElement* ChildElement = ItemChild->AsElement();

					const FString AffiliationAttr(UTF8_TO_TCHAR(ChildElement->Attr(buzz::QN_AFFILIATION).c_str()));
					OutChatMember.Affiliation = EXmppChatMemberAffiliation::ToType(AffiliationAttr);

					const FString RoleAttr(UTF8_TO_TCHAR(ChildElement->Attr(buzz::QN_ROLE).c_str()));
					OutChatMember.Role = EXmppChatMemberRole::ToType(RoleAttr);
				}
			}
		}
	}
}

static void ConvertToChatMember(FXmppChatMember& OutChatMember, const buzz::XmppChatroomMember& InChatMemberJingle)
{
	FXmppJingle::ConvertToJid(OutChatMember.MemberJid, InChatMemberJingle.member_jid());
	OutChatMember.Nickname = UTF8_TO_TCHAR(InChatMemberJingle.name().c_str());
	if (InChatMemberJingle.presence() != nullptr)
	{
		const buzz::XmppPresence* TmpPresence = InChatMemberJingle.presence();
		DecodeChatMemberPresence(OutChatMember, TmpPresence);
	}
}

static const TCHAR* RoomEnterStatusToStr(buzz::XmppChatroomEnteredStatus EnterStatus)
{
	switch (EnterStatus)
	{
	case buzz::XMPP_CHATROOM_ENTERED_SUCCESS:
		return TEXT("XMPP_CHATROOM_ENTERED_SUCCESS");
	case buzz::XMPP_CHATROOM_ENTERED_FAILURE_NICKNAME_CONFLICT:
		return TEXT("XMPP_CHATROOM_ENTERED_FAILURE_NICKNAME_CONFLICT");
	case buzz::XMPP_CHATROOM_ENTERED_FAILURE_PASSWORD_REQUIRED:
		return TEXT("XMPP_CHATROOM_ENTERED_FAILURE_PASSWORD_REQUIRED");
	case buzz::XMPP_CHATROOM_ENTERED_FAILURE_PASSWORD_INCORRECT:
		return TEXT("XMPP_CHATROOM_ENTERED_FAILURE_PASSWORD_INCORRECT");
	case buzz::XMPP_CHATROOM_ENTERED_FAILURE_NOT_A_MEMBER:
		return TEXT("XMPP_CHATROOM_ENTERED_FAILURE_NOT_A_MEMBER");
	case buzz::XMPP_CHATROOM_ENTERED_FAILURE_MEMBER_BANNED:
		return TEXT("XMPP_CHATROOM_ENTERED_FAILURE_MEMBER_BANNED");
	case buzz::XMPP_CHATROOM_ENTERED_FAILURE_MAX_USERS:
		return TEXT("XMPP_CHATROOM_ENTERED_FAILURE_MAX_USERS");
	case buzz::XMPP_CHATROOM_ENTERED_FAILURE_ROOM_LOCKED:
		return TEXT("XMPP_CHATROOM_ENTERED_FAILURE_ROOM_LOCKED");
	case buzz::XMPP_CHATROOM_ENTERED_FAILURE_MEMBER_BLOCKED:
		return TEXT("XMPP_CHATROOM_ENTERED_FAILURE_MEMBER_BLOCKED");
	case buzz::XMPP_CHATROOM_ENTERED_FAILURE_MEMBER_BLOCKING:
		return TEXT("XMPP_CHATROOM_ENTERED_FAILURE_MEMBER_BLOCKING");
	case buzz::XMPP_CHATROOM_ENTERED_FAILURE_OUTDATED_CLIENT:
		return TEXT("XMPP_CHATROOM_ENTERED_FAILURE_OUTDATED_CLIENT");
	case buzz::XMPP_CHATROOM_ENTERED_FAILURE_UNSPECIFIED:
		return TEXT("XMPP_CHATROOM_ENTERED_FAILURE_UNSPECIFIED");
	};
	return TEXT("");
}

/**
 * Result from room member update
 */
class FXmppChatRoomMemberChangedOpResult : public FXmppChatRoomOpResult
{
public:
	FXmppChatRoomMemberChangedOpResult(const FXmppUserJid& InMemberJid, const FXmppRoomId& InRoomId)
		: FXmppChatRoomOpResult(InRoomId, true, FString())
		, MemberJid(InMemberJid)
	{}

	virtual void Process(class FXmppMultiUserChatJingle& Muc) override
	{
		Muc.OnRoomMemberChanged().Broadcast(Muc.Connection.AsShared(), RoomId, MemberJid);
	}

	FXmppUserJid MemberJid;
};

/**
 * Result from room member join
 */
class FXmppChatRoomMemberEnteredOpResult : public FXmppChatRoomOpResult
{
public:
	FXmppChatRoomMemberEnteredOpResult(const FXmppUserJid& InMemberJid, const FXmppRoomId& InRoomId)
		: FXmppChatRoomOpResult(InRoomId, true, FString())
		, MemberJid(InMemberJid)
	{}

	virtual void Process(class FXmppMultiUserChatJingle& Muc) override
	{
		Muc.OnRoomMemberJoin().Broadcast(Muc.Connection.AsShared(), RoomId, MemberJid);
	}

	FXmppUserJid MemberJid;
};

void FXmppMultiUserChatJingle::MemberEntered(
	buzz::XmppChatroomModule* RoomModule, const buzz::XmppChatroomMember* XmppMember)
{
	UE_LOG(LogXmpp, VeryVerbose, TEXT("MUC: MemberEntered room=%s [%s]"),
		UTF8_TO_TCHAR(RoomModule->chatroom_jid().Str().c_str()),
		UTF8_TO_TCHAR(XmppMember->member_jid().Str().c_str()));

	UE_LOG(LogXmpp, VeryVerbose, TEXT("MUC: MemberEntered presence [%s]"), XmppMember->presence() != nullptr ? UTF8_TO_TCHAR(XmppMember->presence()->status().c_str()) : TEXT("null"));

	FXmppRoomId RoomId(UTF8_TO_TCHAR(RoomModule->chatroom_jid().node().c_str()));

	FScopeLock Lock(&ChatroomsLock);
	FXmppRoomJingle& XmppRoom = Chatrooms.FindOrAdd(RoomId);
	if (XmppRoom.Status == FXmppRoomJingle::NotJoined)
	{
		UE_LOG(LogXmpp, Verbose, TEXT("MUC: Added unknown room based on receiving member presence!"));
	}

	UE_LOG(LogXmpp, Verbose, TEXT("MUC: MemberEntered room=%s, status=%d"), UTF8_TO_TCHAR(RoomModule->chatroom_jid().Str().c_str()), (int32)XmppRoom.Status);

	FXmppUserJid MemberJid;
	FXmppJingle::ConvertToJid(MemberJid, XmppMember->member_jid());

	// Check for existing member
	FXmppChatMemberPtr UpdatedMember = FindExistingRoomMember(XmppRoom, MemberJid);
	if (!UpdatedMember.IsValid())
	{
		UpdatedMember = MakeShareable(new FXmppChatMember());
		XmppRoom.Members.Add(UpdatedMember.ToSharedRef());
	}

	// Jid, nickname, and other details assigned inside
	ConvertToChatMember(*UpdatedMember, *XmppMember);
	UE_LOG(LogXmpp, Verbose, TEXT("Queueing FXmppChatRoomMemberChangedOpResult and FXmppChatRoomMemberEnteredOpResult for member %s in room %s"), *MemberJid.Id, *RoomId);
	ResultOpQueue.Enqueue(new FXmppChatRoomMemberEnteredOpResult(MemberJid, RoomId));
}

/**
 * Result from room member exit
 */
class FXmppChatRoomMemberExitedOpResult : public FXmppChatRoomOpResult
{
public:
	FXmppChatRoomMemberExitedOpResult(const FXmppUserJid& InMemberJid, const FXmppRoomId& InRoomId)
		: FXmppChatRoomOpResult(InRoomId, true, FString())
		, MemberJid(InMemberJid)
	{}

	virtual void Process(class FXmppMultiUserChatJingle& Muc) override
	{
		Muc.OnRoomMemberExit().Broadcast(Muc.Connection.AsShared(), RoomId, MemberJid);

		FScopeLock Lock(&Muc.ChatroomsLock);
		FXmppRoomJingle* XmppRoom = Muc.Chatrooms.Find(RoomId);
		if (XmppRoom != nullptr)
		{
			for (int32 Idx = 0; Idx < XmppRoom->Members.Num(); Idx++)
			{
				if (XmppRoom->Members[Idx]->MemberJid == MemberJid)
				{
					XmppRoom->Members.RemoveAt(Idx--);
					break;
				}
			}
		}
	}

	FXmppUserJid MemberJid;
};

/**
 * Result from room chat
 */
class FXmppChatRoomMessageReceivedOpResult : public FXmppChatRoomOpResult
{
public:
	FXmppChatRoomMessageReceivedOpResult(const FXmppRoomId& InRoomId, const TSharedRef<FXmppChatMessage>& InChatMessage)
		: FXmppChatRoomOpResult(InRoomId, true, FString())
		, ChatMessage(InChatMessage)
	{}

	virtual void Process(class FXmppMultiUserChatJingle& Muc) override
	{
		Muc.OnRoomChatReceived().Broadcast(Muc.Connection.AsShared(), RoomId, ChatMessage->FromJid, ChatMessage);
	}

	TSharedRef<FXmppChatMessage> ChatMessage;
};

void FXmppMultiUserChatJingle::InternalHandleJoinedRoom(const FXmppChatMemberPtr& SelfMember, FXmppRoomJingle* XmppRoom)
{
	for (auto Member : XmppRoom->Members)
	{
		if (SelfMember != Member)
		{
			ResultOpQueue.Enqueue(new FXmppChatRoomMemberEnteredOpResult(Member->MemberJid, XmppRoom->RoomInfo.Id));
		}
	}
	for (auto Message : XmppRoom->LastMessages)
	{
		ResultOpQueue.Enqueue(new FXmppChatRoomMessageReceivedOpResult(XmppRoom->RoomInfo.Id, Message));
	}
}

void FXmppMultiUserChatJingle::ChatroomEnteredStatus(
	buzz::XmppChatroomModule* RoomModule, const buzz::XmppPresence* Presence, buzz::XmppChatroomEnteredStatus EnterStatus)
{
	bool bWasSuccessful = false;

	UE_LOG(LogXmpp, Log, TEXT("MUC: ChatroomEnteredStatus room=%s [%s]"), 
		UTF8_TO_TCHAR(RoomModule->chatroom_jid().Str().c_str()),
		RoomEnterStatusToStr(EnterStatus));

	FXmppRoomId RoomId(UTF8_TO_TCHAR(RoomModule->chatroom_jid().node().c_str()));

	FScopeLock Lock(&ChatroomsLock);
	FXmppRoomJingle* XmppRoom = Chatrooms.Find(RoomId);	
	if (XmppRoom != nullptr)
	{
		FString ErrorStr;
		FXmppRoomJingle::ERoomStatus LastStatus = XmppRoom->Status;
		FXmppChatMemberPtr MyChatMember = nullptr;
		if (EnterStatus == buzz::XMPP_CHATROOM_ENTERED_SUCCESS)
		{
			// update status for successful join, defer create status update until ownership determined via HandleMucPresence
			if (LastStatus != FXmppRoomJingle::CreatePending)
			{
				XmppRoom->Status = FXmppRoomJingle::Joined;
			}

			UE_LOG(LogXmpp, Verbose, TEXT("MUC: ChatroomEnteredStatus room=%s, status=%d"), UTF8_TO_TCHAR(RoomModule->chatroom_jid().Str().c_str()), (int32)XmppRoom->Status);

			// Don't empty existing members, may have already been received via MemberEntered()
			// XmppRoom->Members.Empty();

			{
				// Add local user
				FXmppUserJid MyMemberJid;
				FXmppJingle::ConvertToJid(MyMemberJid, RoomModule->member_jid());

				MyChatMember = FindExistingRoomMember(*XmppRoom, MyMemberJid);
				if (!MyChatMember.IsValid())
				{
					// Add user that requested the join to the room member list
					MyChatMember = MakeShareable(new FXmppChatMember());
					MyChatMember->MemberJid = MyMemberJid;
					XmppRoom->Members.Add(MyChatMember.ToSharedRef());
				}

				MyChatMember->Nickname = UTF8_TO_TCHAR(RoomModule->nickname().c_str());
				DecodeChatMemberPresence(*MyChatMember, Presence);
			}
			
			// Update room members
			buzz::XmppChatroomMemberEnumerator* Enumerator = nullptr;
			if (RoomModule->CreateMemberEnumerator(&Enumerator) == buzz::XMPP_RETURN_OK &&
				Enumerator != nullptr)
			{
				while (Enumerator->Next())
				{
					buzz::XmppChatroomMember* XmppMember = Enumerator->current();
					if (XmppMember != nullptr)
					{
						FXmppUserJid NewMemberJid;
						FXmppJingle::ConvertToJid(NewMemberJid, XmppMember->member_jid());
						
						FXmppChatMemberPtr NewChatMember = FindExistingRoomMember(*XmppRoom, NewMemberJid);
						if (!NewChatMember.IsValid())
						{
							NewChatMember = MakeShareable(new FXmppChatMember());
							XmppRoom->Members.Add(NewChatMember.ToSharedRef());
						}
							
						// Jid, nickname, and other details assigned inside
						ConvertToChatMember(*NewChatMember, *XmppMember);
						UE_LOG(LogXmpp, Log, TEXT("ChatroomEnteredStatus - existing member [%s] %s %s"), *RoomId, *NewChatMember->Nickname, *NewChatMember->MemberJid.Id);
					}
				}
			}
			bWasSuccessful = true;
		}
		else
		{
			ErrorStr = FString::Printf(TEXT("EnterStatus=%s"), RoomEnterStatusToStr(EnterStatus));
			// failed to join 
			XmppRoom->Status = FXmppRoomJingle::NotJoined;
		}
		 
        // Create/JoinPublic/JoinPrivate all should have set one of the pending states
		check(LastStatus != FXmppRoomJingle::NotJoined);

		const bool bJoinPending = LastStatus == FXmppRoomJingle::JoinPublicPending || LastStatus == FXmppRoomJingle::JoinPrivatePending;
		const bool bCreatePending = LastStatus == FXmppRoomJingle::CreatePending;
		if (bJoinPending)
		{
			if (LastStatus == FXmppRoomJingle::JoinPublicPending)
			{
				UE_LOG(LogXmpp, Log, TEXT("ChatroomEnteredStatus - queueing ChatRoomJoinPublicOpResult for %s"), *RoomId);
				ResultOpQueue.Enqueue(new FXmppChatRoomJoinPublicOpResult(RoomId, bWasSuccessful, ErrorStr));
			}
			else if (LastStatus == FXmppRoomJingle::JoinPrivatePending)
			{
				UE_LOG(LogXmpp, Log, TEXT("ChatroomEnteredStatus - queueing ChatRoomJoinPrivateOpResult result for %s"), *RoomId);
				ResultOpQueue.Enqueue(new FXmppChatRoomJoinPrivateOpResult(RoomId, bWasSuccessful, ErrorStr));
			}
			// if you just entered add the member entered ops for users that already entered 
			if (bWasSuccessful)
			{
				InternalHandleJoinedRoom(MyChatMember, XmppRoom);
			}
		}
		else if (bCreatePending)
		{
			if (!bWasSuccessful)
			{
				UE_LOG(LogXmpp, Log, TEXT("ChatroomEnteredStatus - queueing room create op result for %s"), *RoomId);
				// This is the only callback for RequestEnterRoom in FXmppChatRoom<X>Op::Process() to see if room was created vs. joined
				ResultOpQueue.Enqueue(new FXmppChatRoomCreateOpResult(RoomId, false, false, ErrorStr));
			}
			else
			{
				// room config handled after ownership is detected
			}
		}

	}
}

static const TCHAR* RoomExitStatusToStr(buzz::XmppChatroomExitedStatus ExitStatus)
{
	switch (ExitStatus)
	{
	case buzz::XMPP_CHATROOM_EXITED_REQUESTED:
		return TEXT("XMPP_CHATROOM_EXITED_REQUESTED");
	case buzz::XMPP_CHATROOM_EXITED_BANNED:
		return TEXT("XMPP_CHATROOM_EXITED_BANNED");
	case buzz::XMPP_CHATROOM_EXITED_KICKED:
		return TEXT("XMPP_CHATROOM_EXITED_KICKED");
	case buzz::XMPP_CHATROOM_EXITED_NOT_A_MEMBER:
		return TEXT("XMPP_CHATROOM_EXITED_NOT_A_MEMBER");
	case buzz::XMPP_CHATROOM_EXITED_SYSTEM_SHUTDOWN:
		return TEXT("XMPP_CHATROOM_EXITED_SYSTEM_SHUTDOWN");
	case buzz::XMPP_CHATROOM_EXITED_UNSPECIFIED:
		return TEXT("XMPP_CHATROOM_EXITED_UNSPECIFIED");
	};
	return TEXT("");
}

void FXmppMultiUserChatJingle::ChatroomExitedStatus(
	buzz::XmppChatroomModule* RoomModule, buzz::XmppChatroomExitedStatus ExitStatus)
{
	UE_LOG(LogXmpp, Log, TEXT("MUC: ChatroomExitedStatus room=%s [%s]"),
		UTF8_TO_TCHAR(RoomModule->chatroom_jid().Str().c_str()),
		RoomExitStatusToStr(ExitStatus));

	FXmppRoomId RoomId(UTF8_TO_TCHAR(RoomModule->chatroom_jid().node().c_str()));

	FScopeLock Lock(&ChatroomsLock);
	FXmppRoomJingle* XmppRoom = Chatrooms.Find(RoomId);
	if (XmppRoom != nullptr)
	{
		FString ErrorStr = FString::Printf(TEXT("EnterStatus=%s"), RoomExitStatusToStr(ExitStatus));
		ResultOpQueue.Enqueue(new FXmppChatRoomExitOpResult(RoomId, true, ErrorStr));
	}
}

void FXmppMultiUserChatJingle::MemberExited(
	buzz::XmppChatroomModule* RoomModule, const buzz::XmppChatroomMember* XmppMember)
{
	UE_LOG(LogXmpp, VeryVerbose, TEXT("MUC: MemberExited room=%s [%s]"),
		UTF8_TO_TCHAR(RoomModule->chatroom_jid().Str().c_str()),
		UTF8_TO_TCHAR(XmppMember->member_jid().Str().c_str()));

	FXmppUserJid MemberJid;
	FXmppJingle::ConvertToJid(MemberJid, XmppMember->member_jid());

	FXmppRoomId RoomId(UTF8_TO_TCHAR(RoomModule->chatroom_jid().node().c_str()));

	FScopeLock Lock(&ChatroomsLock);
	FXmppRoomJingle* XmppRoom = Chatrooms.Find(RoomId);
	if (XmppRoom != nullptr)
	{
		if (XmppRoom->Status == FXmppRoomJingle::Joined)
		{
			UE_LOG(LogXmpp, Verbose, TEXT("Queueing FXmppChatRoomMemberChangedOpResult and FXmppChatRoomMemberExitedOpResult for member %s in room %s"), *MemberJid.Id, *RoomId);
			ResultOpQueue.Enqueue(new FXmppChatRoomMemberExitedOpResult(MemberJid, RoomId));
		}
	}
}

void FXmppMultiUserChatJingle::MemberChanged(
	buzz::XmppChatroomModule* RoomModule, const buzz::XmppChatroomMember* XmppMember)
{
	UE_LOG(LogXmpp, VeryVerbose, TEXT("MUC: MemberChanged room=%s [%s]"),
		UTF8_TO_TCHAR(RoomModule->chatroom_jid().Str().c_str()),
		UTF8_TO_TCHAR(XmppMember->member_jid().Str().c_str()));

	FXmppRoomId RoomId(UTF8_TO_TCHAR(RoomModule->chatroom_jid().node().c_str()));

	FScopeLock Lock(&ChatroomsLock);
	FXmppRoomJingle& XmppRoom = Chatrooms.FindOrAdd(RoomId);
	if (XmppRoom.Status == FXmppRoomJingle::NotJoined)
	{
		UE_LOG(LogXmpp, Verbose, TEXT("MUC: Added unknown room based on receiving member changed event!"));
	}

	UE_LOG(LogXmpp, Verbose, TEXT("MUC: MemberChanged room=%s, status=%d"), UTF8_TO_TCHAR(RoomModule->chatroom_jid().Str().c_str()), (int32)XmppRoom.Status);

	FXmppUserJid MemberJid;
	FXmppJingle::ConvertToJid(MemberJid, XmppMember->member_jid());

	// Check for existing member
	FXmppChatMemberPtr UpdatedMember = FindExistingRoomMember(XmppRoom, MemberJid);
	if (!UpdatedMember.IsValid())
	{
		UpdatedMember = MakeShareable(new FXmppChatMember());
		XmppRoom.Members.Add(UpdatedMember.ToSharedRef());
	}

	// Jid, nickname, and other details assigned inside
	ConvertToChatMember(*UpdatedMember, *XmppMember);
	UE_LOG(LogXmpp, Verbose, TEXT("Queueing FXmppChatRoomMemberChangedOpResult member %s in room %s"), *MemberJid.Id, *RoomId);
	ResultOpQueue.Enqueue(new FXmppChatRoomMemberChangedOpResult(MemberJid, RoomId));
}

void FXmppMultiUserChatJingle::MessageReceived(
	buzz::XmppChatroomModule* RoomModule, const buzz::XmlElement& ChatXml)
{
	UE_LOG(LogXmpp, VeryVerbose, TEXT("MUC: MessageReceived"));

	FXmppRoomId RoomId(UTF8_TO_TCHAR(RoomModule->chatroom_jid().node().c_str()));

	FScopeLock Lock(&ChatroomsLock);
	FXmppRoomJingle* XmppRoom = Chatrooms.Find(RoomId);
	if (XmppRoom != nullptr)
	{
		const buzz::XmlElement* XmlBody = ChatXml.FirstNamed(buzz::QN_BODY);
		if (XmlBody != nullptr)
		{
			TSharedRef<FXmppChatMessage> ChatMessage(new FXmppChatMessage());
			FXmppJingle::ConvertToJid(ChatMessage->FromJid, buzz::Jid(ChatXml.Attr(buzz::QN_FROM)));
			FXmppJingle::ConvertToJid(ChatMessage->ToJid, buzz::Jid(ChatXml.Attr(buzz::QN_TO)));
			ChatMessage->Body = UTF8_TO_TCHAR(XmlBody->BodyText().c_str());

			UE_LOG(LogXmpp, VeryVerbose, TEXT("MUC: MessageReceived"));

			static const buzz::StaticQName QN_DELAY = { "urn:xmpp:delay", "delay" };
			const buzz::XmlElement* Delay = ChatXml.FirstNamed(QN_DELAY);
			if (Delay != nullptr)
			{
				FDateTime::ParseIso8601(UTF8_TO_TCHAR(Delay->Attr(buzz::kQnStamp).c_str()), ChatMessage->Timestamp);
			}
			else
			{
				ChatMessage->Timestamp = FDateTime::UtcNow();
			}
			XmppRoom->AddNewMessage(ChatMessage);

			if (XmppRoom->Status == FXmppRoomJingle::Joined)
			{
				UE_LOG(LogXmpp, Verbose, TEXT("Queueing FXmppChatRoomMessageReceivedOpResult in room %s"), *RoomId);
				ResultOpQueue.Enqueue(new FXmppChatRoomMessageReceivedOpResult(RoomId, ChatMessage));
			}
		}
	}
}

void FXmppMultiUserChatJingle::HandleMucPresence(const FXmppMucPresence& MemberPresence)
{
	UE_LOG(LogXmpp, VeryVerbose, TEXT("MUC: HandleMucPresence: jid=%s nick=%s roomid=%s role=%s affiliation=%s"), *MemberPresence.UserJid.GetFullPath(), *MemberPresence.GetNickName(), *MemberPresence.GetRoomId(), *MemberPresence.Role, *MemberPresence.Affiliation);

	FScopeLock Lock(&ChatroomsLock);
	FXmppRoomJingle* XmppRoom = Chatrooms.Find(MemberPresence.GetRoomId());
	if (XmppRoom != nullptr &&
		XmppRoom->Status == FXmppRoomJingle::CreatePending &&
		MemberPresence.GetNickName().Contains(Connection.GetUserJid().Id))
	{
		const bool bIsOwner = MemberPresence.Affiliation == FString(TEXT("owner"));
		UE_LOG(LogXmpp, Log, TEXT("ChatroomEnteredStatus - queueing room create op result for %s"), *MemberPresence.GetRoomId());
		// This is the only callback for RequestEnterRoom in FXmppChatRoom<X>Op::Process() to see if room was created vs. joined
		ResultOpQueue.Enqueue(new FXmppChatRoomCreateOpResult(MemberPresence.GetRoomId(), bIsOwner, true, FString()));
	}
	else
	{
		UE_LOG(LogXmpp, VeryVerbose, TEXT("MUC: HandleMucPresence IGNORED: room=%s status=%d connjid=%s"), XmppRoom ? TEXT("found") : TEXT("not found"), XmppRoom ? XmppRoom->Status : -1, *Connection.GetUserJid().Id);
	}
}

void FXmppMultiUserChatJingle::DumpMultiUserChatState() const
{
	LOG_SCOPE_VERBOSITY_OVERRIDE(LogXmpp, ELogVerbosity::Display);
	FScopeLock Lock(const_cast<FCriticalSection*>(&ChatroomsLock));
	for (TMap<FXmppRoomId, FXmppRoomJingle>::TConstIterator RoomIt(Chatrooms); RoomIt; ++RoomIt)
	{
		const FXmppRoomId& RoomId = RoomIt.Key();
		const FXmppRoomJingle& XmppRoom = RoomIt.Value();

		UE_LOG(LogXmpp, Display, TEXT("RoomId: %s"), *RoomId);
		UE_LOG(LogXmpp, Display, TEXT(" Owner: %s Subj: %s Priv: %d"), *XmppRoom.RoomInfo.OwnerId, *XmppRoom.RoomInfo.Subject, XmppRoom.RoomInfo.bIsPrivate);
		UE_LOG(LogXmpp, Display, TEXT(" Status: %d"), (int32)XmppRoom.Status);

		UE_LOG(LogXmpp, Display, TEXT(" Members: %d"), XmppRoom.Members.Num());
		for (const FXmppChatMemberRef& Member : XmppRoom.Members)
		{
			UE_LOG(LogXmpp, Display, TEXT("  %s"), *Member->ToDebugString());
		}
	}
}

#endif //WITH_XMPP_JINGLE
