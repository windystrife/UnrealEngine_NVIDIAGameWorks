// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "XmppConnection.h"
#include "XmppJingle/XmppJingle.h"
#include "Containers/Queue.h"
#include "XmppPresence.h"

#if WITH_XMPP_JINGLE

/**
 * Presence entry for a roster member
 */
class FXmppUserPresenceJingle
{
public:
	FXmppUserPresenceJingle()
		: Presence(MakeShareable(new FXmppUserPresence()))
	{}

	/** user id and node info for roster member */
	FXmppUserJid UserJid;
	/** presence info for roster member */
	TSharedRef<FXmppUserPresence> Presence;
};

/**
 * Xmpp presence implementation using webrtc lib tasks/signals
 */
class FXmppPresenceJingle
	: public IXmppPresence
	, public sigslot::has_slots<>
	, public FTickerObjectBase
{
public:

	// IXmppPresence

	virtual bool UpdatePresence(const FXmppUserPresence& Presence) override;
	virtual const FXmppUserPresence& GetPresence() const override;
	virtual bool QueryPresence(const FString& UserId) override;	
	virtual TArray<TSharedPtr<FXmppUserPresence>> GetRosterPresence(const FString& UserId) override;
	virtual void GetRosterMembers(TArray<FXmppUserJid>& Members) override;
	virtual FOnXmppPresenceReceived& OnReceivePresence() override { return OnXmppPresenceReceivedDelegate; }

	// FTickerObjectBase
	
	virtual bool Tick(float DeltaTime) override;

	// FXmppPresenceJingle

	FXmppPresenceJingle(class FXmppConnectionJingle& InConnection);
	virtual ~FXmppPresenceJingle();

	static void ConvertFromPresence(buzz::PresenceStatus& OutStatus, const FXmppUserPresence& InPresence);
	static void ConvertToPresence(FXmppUserPresence& OutPresence, const buzz::PresenceStatus& InStatus, const FXmppUserJid& InJid, const FString& InResourceOverride = FString());
	static void ConvertToMucPresence(FXmppMucPresence& OutMucPresence, const class FXmppMucPresenceStatus& InMucStatus, const FXmppUserJid& InJid);

private:

	/** callback on pump thread when presence status has been updated for a roster member */
	void OnSignalPresenceUpdate(const buzz::PresenceStatus& Status);
	/** callback on pump thread when presence status has been updated for a muc room member */
	void OnSignalMucPresenceUpdate(const class FXmppMucPresenceStatus& MucStatus);

	// called on pump thread
	void HandlePumpStarting(buzz::XmppPump* XmppPump);
	void HandlePumpQuitting(buzz::XmppPump* XmppPump);
	void HandlePumpTick(buzz::XmppPump* XmppPump);

	/** last presence update sent for the current user */
	FXmppUserPresence CachedPresence;
	/** last presence status sent for the current user */
	buzz::PresenceStatus CachedStatus;
	/** task used to kick off presence updates on xmpp pump thread */
	class FXmppPresenceOutTask* PresenceSendTask;
	/** task used to receive presence updates from xmpp pump thread */
	class FXmppPresenceReceiveTask* PresenceRcvTask;

	// completion delegates
	FOnXmppPresenceReceived OnXmppPresenceReceivedDelegate;

	/** cached presence info for roster */
	TMap<FString, FXmppUserPresenceJingle> RosterPresence;
	/** list of incoming roster user ids that have updates */
	TQueue<FString> RosterUpdates;
	/** lock for access to RosterPresence */
	FCriticalSection RosterLock;

	/** list of pending outgoing presence update requests */
	TQueue<buzz::PresenceStatus*> PresenceUpdateRequests;
	/** list of pending outgoing presence query requests */
	TQueue<FXmppUserJid> PresenceQueryRequests;

	/** Number of presence/roster updates in a given interval */
    int32 NumPresenceIn;
	/** Number of outgoing presence updates in a given interval */
	int32 NumPresenceOut;
	/** Number of presence queries made in a given interval */
	int32 NumQueryRequests;

	class FXmppConnectionJingle& Connection;
	friend class FXmppConnectionJingle; 
};

#endif //WITH_XMPP_JINGLE
