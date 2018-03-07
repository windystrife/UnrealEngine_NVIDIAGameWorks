// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "XmppJingle/XmppJingle.h"
#include "Containers/Queue.h"
#include "XmppChat.h"
#include "XmppMultiUserChat.h"

#if WITH_XMPP_JINGLE

#define MAX_MESSAGE_HISTORY 50

/**
 * Info cached about a joined/created room
 */
class FXmppRoomJingle
{
public:
	enum ERoomStatus
	{
		NotJoined,
		Joined,
		CreatePending,
		JoinPrivatePending,
		JoinPublicPending,
		ExitPending
	};

	FXmppRoomJingle()
		: Status(NotJoined)
	{
	}

	void AddNewMessage(const TSharedRef<FXmppChatMessage>& ChatMessage)
	{
		LastMessages.Add(ChatMessage);
		if (LastMessages.Num() > MAX_MESSAGE_HISTORY)
		{
			LastMessages.RemoveAt(0);
		}
	}
	
	ERoomStatus Status;
	FXmppRoomInfo RoomInfo;
	TArray<FXmppChatMemberRef> Members;
	TArray<TSharedRef<FXmppChatMessage> > LastMessages;
};

/**
 * Room operation to queue for pump thread consumption
 */
class FXmppChatRoomOp
{
public:
	FXmppChatRoomOp(const FXmppRoomId& InRoomId)
		: RoomId(InRoomId)
	{}
	virtual ~FXmppChatRoomOp() {};
	virtual bool AllowCreateRoom() const { return false; }
	virtual bool AllowJoinRoom() const { return false; }
	virtual class FXmppChatRoomOpResult* Process(buzz::XmppChatroomModule* XmppRoom, buzz::XmppPump* XmppPump) = 0;
	virtual class FXmppChatRoomOpResult* ProcessError(const FString& ErrorStr) = 0;

	FXmppRoomId RoomId;
};

/**
 * Room operation result queued for game thread consumption
 */
class FXmppChatRoomOpResult
{
public:
	FXmppChatRoomOpResult(const FXmppRoomId& InRoomId, bool InbWasSuccessful, const FString& InErrorStr)
		: RoomId(InRoomId)
		, bWasSuccessful(InbWasSuccessful)
		, ErrorStr(InErrorStr)
	{}

	virtual ~FXmppChatRoomOpResult() {}
	virtual void Process(class FXmppMultiUserChatJingle& Muc) = 0;

	FXmppRoomId RoomId;
	bool bWasSuccessful;
	FString ErrorStr;
};


/**
*  Room configuration types
*/
enum class EConfigureRoomTypeJingle : uint8
{
	NoCallback, // Trigger no callbacks.  Currently used for global chat config.
	UseCreateCallback, // New room config, trigger create callback when done
	UseConfigCallback // Change existing room, trigger config callback when done
};

typedef std::vector<std::pair<std::string, std::string>> FRoomFeatureValuePairs;
typedef std::pair<std::string, std::string> FRoomFeatureValuePair;

/**
 * Xmpp MUC (Multi User Chat) implementation using webrtc lib tasks/signals
 */
class FXmppMultiUserChatJingle
	: public IXmppMultiUserChat
	, public buzz::XmppChatroomHandler
	, public FTickerObjectBase
	, public sigslot::has_slots < >
{
public:

	// IXmppMultiUserChat

	virtual bool CreateRoom(const FXmppRoomId& RoomId, const FString& Nickname, const FXmppRoomConfig& RoomConfig) override;
	virtual bool ConfigureRoom(const FXmppRoomId& RoomId, const FXmppRoomConfig& RoomConfig) override;
	virtual bool RefreshRoomInfo(const FXmppRoomId& RoomId) override;
	virtual bool JoinPublicRoom(const FXmppRoomId& RoomId, const FString& Nickname) override;
	virtual bool JoinPrivateRoom(const FXmppRoomId& RoomId, const FString& Nickname, const FString& Password) override;
	virtual bool RegisterMember(const FXmppRoomId& RoomId, const FString& Nickname) override;
	virtual bool UnregisterMember(const FXmppRoomId& RoomId, const FString& Nickname) override;
	virtual bool ExitRoom(const FXmppRoomId& RoomId) override;
	virtual bool SendChat(const FXmppRoomId& RoomId, const FString& MsgBody, const FString& ChatInfo) override;
	virtual void GetJoinedRooms(TArray<FXmppRoomId>& OutRooms) override;
	virtual bool GetRoomInfo(const FXmppRoomId& RoomId, FXmppRoomInfo& OutRoomInfo) override;
	virtual bool GetMembers(const FXmppRoomId& RoomId, TArray<FXmppChatMemberRef>& OutMembers) override;
	virtual FXmppChatMemberPtr GetMember(const FXmppRoomId& RoomId, const FXmppUserJid& MemberJid) override;
	virtual bool GetLastMessages(const FXmppRoomId& RoomId, int32 NumMessages, TArray< TSharedRef<FXmppChatMessage> >& OutMessages) override;
	virtual void HandleMucPresence(const FXmppMucPresence& MemberPresence) override;
	virtual void DumpMultiUserChatState() const override;
	
	virtual FOnXmppRoomCreateComplete& OnRoomCreated() override { return OnXmppRoomCreateCompleteDelegate; }
	virtual FOnXmppRoomConfigureComplete& OnRoomConfigured() override { return OnXmppRoomConfiguredDelegate; }
	virtual FOnXmppRoomInfoRefreshComplete& OnRoomInfoRefreshed() override { return OnXmppRoomInfoRefreshedDelegate; }
	virtual FOnXmppRoomJoinPublicComplete& OnJoinPublicRoom() override { return OnXmppRoomJoinPublicCompleteDelegate; }
	virtual FOnXmppRoomJoinPrivateComplete& OnJoinPrivateRoom() override { return OnXmppRoomJoinPrivateCompleteDelegate; }
	virtual FOnXmppRoomExitComplete& OnExitRoom() override { return OnXmppRoomExitCompleteDelegate; }
	virtual FOnXmppRoomMemberJoin& OnRoomMemberJoin() override { return OnXmppRoomMemberJoinDelegate; }
	virtual FOnXmppRoomMemberExit& OnRoomMemberExit() override { return OnXmppRoomMemberExitDelegate; }
	virtual FOnXmppRoomMemberChanged& OnRoomMemberChanged() override { return OnXmppRoomMemberChangedDelegate; }
	virtual FOnXmppRoomChatReceived& OnRoomChatReceived() override { return OnXmppRoomChatReceivedDelegate; }

	// buzz::XmppChatroomHandler

	virtual void ChatroomEnteredStatus(buzz::XmppChatroomModule* RoomModule, const buzz::XmppPresence* Presence, buzz::XmppChatroomEnteredStatus EnterStatus) override;
	virtual void ChatroomExitedStatus(buzz::XmppChatroomModule* RoomModule, buzz::XmppChatroomExitedStatus ExitStatus) override;
	virtual void MemberEntered(buzz::XmppChatroomModule* RoomModule, const buzz::XmppChatroomMember* Member) override;
	virtual void MemberExited(buzz::XmppChatroomModule* RoomModule, const buzz::XmppChatroomMember* Member) override;
	virtual void MemberChanged(buzz::XmppChatroomModule* RoomModule, const buzz::XmppChatroomMember* Member) override;
	virtual void MessageReceived(buzz::XmppChatroomModule* RoomModule, const buzz::XmlElement& ChatXml) override;
	
	// FTickerObjectBase
	
	virtual bool Tick(float DeltaTime) override;

	// FXmppMultiUserChatJingle

	FXmppMultiUserChatJingle(class FXmppConnectionJingle& InConnection);
	virtual ~FXmppMultiUserChatJingle();

	/** callback on pump thread when new muc config query has been received */
	void OnSignalConfigQueryResponseReceived(class FXmppConfigQueryResponseJingle* ConfigQueryResponse);
	/** callback on pump thread when new muc config response has been received */
	void OnSignalConfigResponseReceived(class FXmppConfigResponseJingle* ConfigResponse);
	/** callback on pump thread when muc room info refresh has been received */
	void OnSignalRoomInfoRefreshReceived(buzz::MucRoomDiscoveryTask* RefreshTask, bool bExists, const std::string& Name, const std::string& RoomId, const std::set<std::string>& Features, const std::map<std::string, std::string>& ExtendedInfo);

private:

	/** list of incoming muc config query responses */
	TQueue<class FXmppConfigQueryResponseJingle*> ReceivedConfigQueryResponseQueue;
	/** list of incoming muc config responses */
	TQueue<class FXmppConfigResponseJingle*> ReceivedConfigResponseQueue;
	/** list of incoming muc info refresh responses */
	TQueue<class FXmppRoomInfoRefreshResponseJingle*> ReceivedRoomInfoRefreshResponseQueue;

	// called on pump thread
	void HandlePumpStarting(buzz::XmppPump* XmppPump);
	void HandlePumpQuitting(buzz::XmppPump* XmppPump);
	void HandlePumpTick(buzz::XmppPump* XmppPump);
	
	// pump thread processing of chat room op
	void ProcessPendingOp(FXmppChatRoomOp* PendingOp, buzz::XmppPump* XmppPump);
	// game thread processing of chat room op result
	void ProcessResultOp(FXmppChatRoomOpResult* ResultOp, class FXmppMultiUserChatJingle& Muc);

	// Configure room w/ flags for internal use to determine which callbacks to trigger
	bool InternalConfigureRoom(const FXmppRoomId& RoomId, const FXmppRoomConfig& RoomConfig, EConfigureRoomTypeJingle RoomConfigurationType);
	void InternalHandleJoinedRoom(const FXmppChatMemberPtr& SelfMember, FXmppRoomJingle* XmppRoom);

	// Update logging while joining a room
	void JoinRoomStart();
	void JoinRoomFinish();

	TMap<FXmppRoomId, FXmppRoomJingle> Chatrooms;
	// PendingRoomCreateConfigs used by friend OpResult classes
	TMap<FXmppRoomId, FXmppRoomConfig> PendingRoomCreateConfigs;
	FCriticalSection ChatroomsLock;

	TMap<FXmppRoomId, buzz::XmppChatroomModule*> XmppRoomModules;
	
	TQueue<FXmppChatRoomOp*> PendingOpQueue;
	TQueue<FXmppChatRoomOpResult*> ResultOpQueue;

	class FXmppConnectionJingle& Connection;
	friend class FXmppConnectionJingle; 

	// IXmppMultiUserChat delegates
	FOnXmppRoomCreateComplete OnXmppRoomCreateCompleteDelegate;
	FOnXmppRoomConfigureComplete OnXmppRoomConfiguredDelegate;
	FOnXmppRoomInfoRefreshComplete OnXmppRoomInfoRefreshedDelegate;
	FOnXmppRoomJoinPublicComplete OnXmppRoomJoinPublicCompleteDelegate;
	FOnXmppRoomJoinPrivateComplete OnXmppRoomJoinPrivateCompleteDelegate;
	FOnXmppRoomExitComplete OnXmppRoomExitCompleteDelegate;
	FOnXmppRoomMemberJoin OnXmppRoomMemberJoinDelegate;
	FOnXmppRoomMemberExit OnXmppRoomMemberExitDelegate;
	FOnXmppRoomMemberChanged OnXmppRoomMemberChangedDelegate;
	FOnXmppRoomChatReceived OnXmppRoomChatReceivedDelegate;

	// Number of Muc room op requests generated */
	int32 NumOpRequests;
	/** Number of Muc room op responses generated */
	int32 NumMucResponses;
	/** Number of times verbosity was increased during MUC creation */
	int32 VerbosityIncreasedCount;
	/** Log verbosity prior to increasing it during MUC creation */
	ELogVerbosity::Type OriginalLogVerbosity;

	friend class FXmppChatRoomOpResult;
	friend class FXmppChatRoomCreateOpResult;
	friend class FXmppChatRoomConfigOpResult;
	friend class FXmppChatRoomInfoRefreshOpResult;
	friend class FXmppChatRoomInfoRefreshOpResult;
	friend class FXmppChatRoomJoinPublicOpResult;
	friend class FXmppChatRoomJoinPrivateOpResult;
	friend class FXmppChatRoomExitOpResult;
	friend class FXmppChatRoomMemberChangedOpResult;
	friend class FXmppChatRoomMemberEnteredOpResult;
	friend class FXmppChatRoomMemberExitedOpResult;
	friend class FXmppChatRoomMessageReceivedOpResult;
};

#endif //WITH_XMPP_JINGLE
