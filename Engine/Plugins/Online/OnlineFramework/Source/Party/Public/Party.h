// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Templates/SubclassOf.h"
#include "Engine/EngineBaseTypes.h"
#include "UObject/CoreOnline.h"
#include "GameFramework/OnlineReplStructs.h"
#include "OnlineSubsystemTypes.h"
#include "Interfaces/OnlinePartyInterface.h"
#include "PartyGameState.h"
#include "Party.generated.h"

class UGameInstance;

typedef FUniqueNetIdRepl FOnlinePartyIdRepl;
class UPartyGameState;
class UGameInstance;
class IOnlinePartyJoinInfo;

enum class EPartyType : uint8;

/**
 * Holds the basic information needed to join a party
 */
struct FPartyDetails : public TSharedFromThis<FPartyDetails>
{
	FPartyDetails(const TSharedRef<const IOnlinePartyJoinInfo>& InPartyJoinInfo, bool bInAcceptInvite = true)
		: PartyJoinInfo(InPartyJoinInfo), bAcceptInvite(bInAcceptInvite)
	{}

	virtual ~FPartyDetails() {}

	bool IsValid() const
	{
		return PartyJoinInfo->IsValid();
	}

	const TSharedRef<const FOnlinePartyId>& GetPartyId() const
	{
		return PartyJoinInfo->GetPartyId();
	}

	const FOnlinePartyTypeId GetPartyTypeId() const
	{
		return PartyJoinInfo->GetPartyTypeId();
	}

	const TSharedRef<const FUniqueNetId>& GetLeaderId() const
	{
		return PartyJoinInfo->GetLeaderId();
	}

	const FString& GetAppId() const
	{
		return PartyJoinInfo->GetAppId();
	}

	virtual FString ToString() const
	{
		return FString::Printf(
			TEXT("PartyId: %s LeaderId: %s ResKey: %s Client: %s"), 
			*GetPartyId()->ToDebugString(), 
			*GetLeaderId()->ToDebugString(),
			*GetAppId());
	}

	TSharedRef<const IOnlinePartyJoinInfo> PartyJoinInfo;
	bool bAcceptInvite;
};

///////////////////////////////////////////////////////////////////
// Completion delegates
///////////////////////////////////////////////////////////////////
namespace UPartyDelegates
{
	/**
	 * Party creation async task completed callback
	 *
	 * @param LocalUserId - id of user that initiated the request
	 * @param Result - result of the operation
	 */
	DECLARE_DELEGATE_TwoParams(FOnCreateUPartyComplete, const FUniqueNetId& /*LocalUserId*/, const ECreatePartyCompletionResult /*Result*/);
	/**
	 * Party join async task completed callback
	 *
	 * @param LocalUserId - id of user that initiated the request
	 * @param Result - result of the operation
	 * @param NotApprovedReason - client defined value describing why you were not approved
	 */
	DECLARE_DELEGATE_ThreeParams(FOnJoinUPartyComplete, const FUniqueNetId& /*LocalUserId*/, const EJoinPartyCompletionResult /*Result*/, const int32 /*NotApprovedReason*/);
	/**
	 * Query party joinability async task completed callback
	 *
	 * @param LocalUserId - id of user that initiated the request
	 * @param Result - result of the operation
	 * @param NotApprovedReason - client defined value describing why you were not approved
	 */
	DECLARE_DELEGATE_ThreeParams(FOnQueryUPartyJoinabilityComplete, const FUniqueNetId& /*LocalUserId*/, const EJoinPartyCompletionResult /*Result*/, const int32 /*NotApprovedReason*/);
	/**
	 * Party leave async task completed callback
	 *
	 * @param LocalUserId - id of user that initiated the request
	 * @param Result - result of the operation
	 */
	DECLARE_DELEGATE_TwoParams(FOnLeaveUPartyComplete, const FUniqueNetId& /*LocalUserId*/, const ELeavePartyCompletionResult /*Result*/);
}

/**
 * High level singleton for the management of parties, all parties are contained within
 */
UCLASS(config=Game, notplaceable)
class PARTY_API UParty : public UObject
{
	GENERATED_UCLASS_BODY()

private:

	/**
	 * Delegate fired when there is any data update on the party
	 *
	 * @param PartyUpdated party that updated
	 */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnPartyUpdate, UPartyGameState* /* PartyUpdated */);

	/**
	 * Delegate fired when there is any data update on a party member
	 *
	 * @param PartyState Party related to the data update
	 * @param InMemberId UniqueId of the member data update
	 */
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnPartyMemberUpdate, UPartyGameState* /* PartyState */, const FUniqueNetIdRepl& /* InMemberId */);
	
	/**
	 *	Delegate fired when the local player has left the party
	 *	
	 *	@param PartyState Party that the local player left
	 *	@param Reason The reason why the player left the party
	 */
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnPartyLeft, UPartyGameState* /* PartyState */, EMemberExitedReason /* Reason */);

	/**
	*	Delegate fired when a party member has left the party
	*
	*	@param PartyState Party that the player left
	*	@param InMmemberId UniqueId of the member that left
	*	@param Reason The reason why the player left the party
	*/
	DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnPartyMemberLeaving, UPartyGameState* /* PartyState */, const FUniqueNetIdRepl& /* InMemberId */, EMemberExitedReason /* Reason */);

	/**
	 *	Delegate fired when a party member has left the party
	 *	
	 *	@param PartyState Party that the player left
	 *	@param InMmemberId UniqueId of the member that left
	 *	@param Reason The reason why the player left the party
	 */
	DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnPartyMemberLeft, UPartyGameState* /* PartyState */, const FUniqueNetIdRepl& /* InMemberId */, EMemberExitedReason /* Reason */);

public:

	/**
	 * Initialization of the party management code, sets up listening for all party activity
	 */
	virtual void Init();

	/**
	 * Initialization specific to play in editor
	 */
	void InitPIE();

	/**
	 * Unregister delegates and clear out shared pointers to MCP objects
	 */
	void OnShutdown();

	/**
	 * Create a generic party
	 *
	 * @param InUserId user creating the party (should be primary player)
	 * @param InPartyTypeId desired party type
	 * @param InPartyConfig desired party configuration
	 * @param InCompletionDelegate delegate called upon completion
	 */
	void CreateParty(const FUniqueNetId& InUserId, const FOnlinePartyTypeId InPartyTypeId, const FPartyConfiguration& InPartyConfig, const UPartyDelegates::FOnCreateUPartyComplete& InCompletionDelegate);

	/**
	 * Join a generic party
	 *
	 * @param InUserId user joining the party (should be primary player)
	 * @param InPartyDetails credentials for the party to join
	 * @param InCompletionDelegate delegate called upon completion
	 */
	void JoinParty(const FUniqueNetId& InUserId, const FPartyDetails& InPartyDetails, const UPartyDelegates::FOnJoinUPartyComplete& InCompletionDelegate);

	/** 
	 * Query the joinability of a party before attempting to join it
	 *
	 * @param InUserId user joining the party (should be primary player)
	 * @param InPartyDetails credentials for the party to join
	 * @param InCompletionDelegate delegate called upon completion
	 */
	void QueryPartyJoinability(const FUniqueNetId& InUserId, const FPartyDetails& InPartyDetails, const UPartyDelegates::FOnQueryUPartyJoinabilityComplete& InCompletionDelegate);

	/**
	 * Leave a generic party
	 *
	 * @param InUserId user creating the party (should be primary player)
	 * @param InPartyId id of the party to leave
	 * @param InCompletionDelegate delegate called upon completion
	 */
	void LeaveParty(const FUniqueNetId& InUserId, const FOnlinePartyTypeId InPartyTypeId, const UPartyDelegates::FOnLeaveUPartyComplete& InCompletionDelegate);
	
	/**
	 * Create a persistent party (party that will hold users for the duration of app)
	 *
	 * @param InUserId user creating the party (should be primary player)
	 * @param InCompletionDelegate delegate called upon completion
	 */
	virtual void CreatePersistentParty(const FUniqueNetId& InUserId, const UPartyDelegates::FOnCreateUPartyComplete& InCompletionDelegate = UPartyDelegates::FOnCreateUPartyComplete());

	/**
	 * Join a persistent party (party that will hold users for the duration of app)
	 *
	 * @param InUserId user creating the party (should be primary player)
	 * @param InPartyDetails credentials for the party to join
	 * @param InCompletionDelegate delegate called upon completion
	 */
	virtual void JoinPersistentParty(const FUniqueNetId& InUserId, const FPartyDetails& InPartyDetails, const UPartyDelegates::FOnJoinUPartyComplete& InCompletionDelegate = UPartyDelegates::FOnJoinUPartyComplete());

	/**
	 * Leave a persistent party, a new one will need to be created for the given user afterward
	 *
	 * @param InUserId user creating the party (should be primary player)
	 * @param InCompletionDelegate delegate called upon completion
	 */
	virtual void LeavePersistentParty(const FUniqueNetId& InUserId, const UPartyDelegates::FOnLeaveUPartyComplete& InCompletionDelegate = UPartyDelegates::FOnLeaveUPartyComplete());

	/**
	 * Reestablish all party state and information upon returning to the main menu
	 * Makes sure all data is proper and notifies the UI to reconstruct the party
	 */
	void RestorePersistentPartyState();

	/**
	 * Do both LeavePersistentParty() and RestorePersistentPartyState() together
	 * Typically for error cases, consider using the above functions first
	 */
	void LeaveAndRestorePersistentParty();

	/**
	 * Kick local players from persistent party
	 *
	 * @param InCompletionDelegate delegate called upon completion
	 */
	virtual void KickFromPersistentParty(const UPartyDelegates::FOnLeaveUPartyComplete& InCompletionDelegate = UPartyDelegates::FOnLeaveUPartyComplete());

	/**
	 * Try to process any pending invites received while in the rest of the game
	 *
	 * @return true if a pending invite is available and a join will be attempted, false otherwise
	 */
	bool ProcessPendingPartyJoin();

	/**
	 * Take ownership of a pending invite and do what is necessary to get the game to a point where it can be used
	 *
	 * @param LocalUserId who the invite is to
	 * @param PartyDetails party details used for the join
	 * @param JoinCompleteDelegate delegate to call once the pending invite has been joined or in all failure cases
	 */
	void AddPendingPartyJoin(const FUniqueNetId& LocalUserId, TSharedRef<const FPartyDetails> PartyDetails, const UPartyDelegates::FOnJoinUPartyComplete& JoinCompleteDelegate);

	/** Clears the join data associated with a pending party join. */
	void ClearPendingPartyJoin();

	/** 
	 * Get the pending party join
	 *
	 * @return The pending party join info
	 */
	TSharedPtr<const FPartyDetails> GetPendingPartyJoinDetails() const;

	/**
	 * Is any local player in the given party
	 *
	 * @param PartyId party to check
	 *
	 * @return true if any local player exists in the given party, false otherwise
	 */
	bool IsInParty(TSharedPtr<const FOnlinePartyId>& PartyId) const;

	/** @return delegate fired when a party is joined */
	FOnPartyUpdate& OnPartyJoined() { return PartyJoinedDelegates; }
	/** @return delegate fired when the party is reset after returning to the frontend */
	FOnPartyUpdate& OnPartyResetForFrontend() { return PartyResetForFrontendDelegate; }
	/** @return delegate fired when a party is left */
	FOnPartyLeft& OnPartyLeft() { return PartyLeftDelegates; }

	/** @return delegate fired when a party member joins */
	FOnPartyMemberUpdate& OnPartyMemberJoined() { return PartyMemberJoined; }
	/** @return delegate fired when a party member is promoted to leader */
	FOnPartyMemberUpdate& OnPartyMemberPromoted() { return PartyMemberPromoted; }
	
	/** @return delegate fired when a party member is about to leave */
	FOnPartyMemberLeaving& OnPartyMemberLeaving() { return PartyMemberLeaving; }

	/** @return delegate fired when a party member leaves */
	FOnPartyMemberLeft& OnPartyMemberLeft() { return PartyMemberLeft; }

	/**
	 * Get the id for the current persistent party where the local players are members
	 */
	TSharedPtr<const FOnlinePartyId> GetPersistentPartyId() const { return PersistentPartyId; }
	
	/** @return the party state for a given party */
	UPartyGameState* GetParty(const FOnlinePartyId& InPartyId) const;

	/** @return the party state for a given party type */
	UPartyGameState* GetParty(const FOnlinePartyTypeId InPartyTypeId) const;

	/** @return the party state for the persistent party */
	UPartyGameState* GetPersistentParty() const;

	/** Notify the party system that travel is occurring */
	void NotifyPreClientTravel(const FString& PendingURL, ETravelType TravelType, bool bIsSeamlessTravel);

	/** @return true if the player has accepted an invite, but it hasn't been processed yet */
	bool HasPendingPartyJoin() const;

	/**
	 * Get the session name (if available) for the primary player, typically NAME_GameSession
	 *
	 * @return Session name
	 */
	FName GetPlayerSessionName() const;

protected:

	/** Pending party join */
	struct FPendingPartyJoin : public TSharedFromThis<FPendingPartyJoin>
	{
		FPendingPartyJoin(TSharedRef<const FUniqueNetId> InLocalUserId, TSharedRef<const FPartyDetails>& InPartyDetails, const UPartyDelegates::FOnJoinUPartyComplete& InDelegate)
			: LocalUserId(InLocalUserId)
			, PartyDetails(InPartyDetails)
			, Delegate(InDelegate)
		{
			ensure(LocalUserId->IsValid());
		}

		/** user that sent the invite */
		TSharedRef<const FUniqueNetId> LocalUserId;
		/** details about party to join */
		TSharedRef<const FPartyDetails> PartyDetails;

		UPartyDelegates::FOnJoinUPartyComplete Delegate;
	};

	/** game invite info that is available to join via an accepted game invite */
	TSharedPtr<FPendingPartyJoin> PendingPartyJoin;

	/**
	* Take ownership of a pending invite and do what is necessary to get the game to a point where it can be used
	*
	* @param LocalUserId who the invite is to
	* @param PartyDetails party details used for the join
	* @param JoinCompleteDelegate delegate to call once the pending invite has been joined or in all failure cases
	*/
	virtual void HandlePendingJoin() PURE_VIRTUAL(UParty::HandlePendingJoin, )

	/** 
	 * Struct containing enough information to rejoin a party
	 */
	struct FRejoinableParty : public TSharedFromThis<FRejoinableParty>
	{
		FRejoinableParty(const TSharedRef<const FOnlinePartyId>& InPartyId, const TArray<TSharedRef<const FUniqueNetId>>& InMembers) :
			PartyId(InPartyId),
			Members(InMembers)
		{}

		/** The ID of the party we want to rejoin */
		TSharedRef<const FOnlinePartyId> PartyId;
		/** List of members in the former party */
		TArray<TSharedRef<const FUniqueNetId>> Members;
	};
	/** Party we want to rejoin when we come back online */
	TSharedPtr<FRejoinableParty> RejoinableParty;

	/**
	 * Party configuration
	 */

public:

	int32 GetDefaultPartyMaxSize() const { return DefaultMaxPartySize; }

protected:

	/** Mapping of party types to party classes, up to game to define relationship */
	TMap<FOnlinePartyTypeId, TSubclassOf<UPartyGameState>> PartyClasses;

	UPROPERTY(Config)
	int32 DefaultMaxPartySize;

	/**
	 * Get the default settings for creating a persistent party
	 *
	 * @param PartyType party visibility setting
	 * @param bLeaderFriendsOnly can only the leader send invites
	 * @param bLeaderInvitesOnly can only the leader have friends join via presence
	 * @param bAllowInvites are invites currently allowed
	 */
	virtual void GetDefaultPersistentPartySettings(EPartyType& PartyType, bool& bLeaderFriendsOnly, bool& bLeaderInvitesOnly, bool& bAllowInvites);

	/**
	 * Get the default configuration used for persistent party creation
	 *
	 * @param PartyConfig [out] desired party configuration
	 */
	void GetPersistentPartyConfiguration(FPartyConfiguration& PartyConfig);

	/**
	 * Party state management
	 */
	virtual void PartyMemberExitedInternal(const FUniqueNetId& InLocalUserId, const FOnlinePartyId& InPartyId, const FUniqueNetId& InMemberId, const EMemberExitedReason InReason);

	/** 
	 * Check if we want to cache the rejoin information for a disconnected persistent party, to attempt to rejoin on reconnect
	 *
	 * @param PartyState the party we are checking
	 * @return true if we want to cache the information, false if not
	 */
	virtual bool ShouldCacheDisconnectedPersistentPartyForRejoin(UPartyGameState* PartyState);

	/** 
	 * Check if we are in a good state to try to rejoin the cached disconnected party
	 * The default implementation returns false, this must be overridden by a game implementation that can determine if the game
	 * is in a good state to leave their current party and try to rejoin the former party
	 *
	 * @param InRejoinableParty cached party information
	 * @return true if we are in a good state to start the rejoin process, false if not
	 */
	virtual bool ShouldTryRejoiningPersistentParty(const FRejoinableParty& InRejoinableParty);


private:

	/** Is leaving the persistent party already in flight */
	UPROPERTY()
	bool bLeavingPersistentParty;
	/** Array of leave persistent party delegates gathered while already leaving a persistent party */
	TArray<UPartyDelegates::FOnLeaveUPartyComplete> LeavePartyCompleteDelegates;
	/** Id of the current persistent party, only one and it should always be valid except while transferring between parties */
	TSharedPtr<const FOnlinePartyId> PersistentPartyId;
	/** Mapping of all known joined parties */
	TMap<FOnlinePartyTypeId, UPartyGameState*> JoinedParties;

	/** Delegate when parties are joined */
	FOnPartyUpdate PartyJoinedDelegates;
	/** Delegate when the party is reset after returning to the home base */
	FOnPartyUpdate PartyResetForFrontendDelegate;
	/** Delegate when parties are removed */
	FOnPartyLeft PartyLeftDelegates;

	/** Delegate when party members are added */
	FOnPartyMemberUpdate PartyMemberJoined;
	/** Delegate when party members are promoted to leader */
	FOnPartyMemberUpdate PartyMemberPromoted;
	/** Delegate when party members are removed */
	FOnPartyMemberLeaving PartyMemberLeaving;
	/** Delegate when party members are removed */
	FOnPartyMemberLeft PartyMemberLeft;

	/**
	 * Notification that a world has been loaded (need to get a UWorld to register delegates on OSS)
	 */
	void OnPostLoadMap(UWorld* = nullptr);

	/**
	 * Register with the identity interface to monitor logout for party cleanup
	 */
	void RegisterIdentityDelegates();

	/**
	 * Unregister with the identity interface
	 */
	void UnregisterIdentityDelegates();

	/**
	 * Register with the party interface to monitor party events and dispatch to specific party state
	 */
	void RegisterPartyDelegates();

	/**
	 * Unregister with the party interface 
	 */
	void UnregisterPartyDelegates();

	/**
	 * Update the persistent party leader locally (possibly telling the server)
	 *
	 * @param NewPartyLeader id of new party leader
	 */
	virtual void UpdatePersistentPartyLeader(const FUniqueNetIdRepl& NewPartyLeader);

	/**
	 * Common calls for managing parties
	 */
	void CreatePartyInternal(const FUniqueNetId& InUserId, const FOnlinePartyTypeId InPartyTypeId, const FPartyConfiguration& InPartyConfig, const UPartyDelegates::FOnCreateUPartyComplete& InCompletionDelegate);
	void JoinPartyInternal  (const FUniqueNetId& InUserId, const FPartyDetails& InPartyDetails, const UPartyDelegates::FOnJoinUPartyComplete& InCompletionDelegate);
	void LeavePartyInternal(const FUniqueNetId& InUserId, const FOnlinePartyTypeId InPartyTypeId, const UPartyDelegates::FOnLeaveUPartyComplete& InCompletionDelegate);

	/**
	 * Delegates for managing parties
	 */
	void OnCreatePartyComplete(const FUniqueNetId& LocalUserId, const ECreatePartyCompletionResult Result, const FOnlinePartyTypeId InPartyTypeId, UPartyDelegates::FOnCreateUPartyComplete CompletionDelegate);
	void OnJoinPartyComplete(const FUniqueNetId& LocalUserId, const EJoinPartyCompletionResult Result, int32 DeniedResultCode, const FOnlinePartyTypeId InPartyTypeId, UPartyDelegates::FOnJoinUPartyComplete CompletionDelegate);
	void OnLeavePartyComplete(const FUniqueNetId& LocalUserId, const ELeavePartyCompletionResult Result, const FOnlinePartyTypeId InPartyTypeId, UPartyDelegates::FOnLeaveUPartyComplete CompletionDelegate);
	void HandleJoinPersistentPartyFailure();

	/**
	 * Delegates for managing regular parties
	 */
	void OnCreatePartyInternalComplete(const FUniqueNetId& LocalUserId, const TSharedPtr<const FOnlinePartyId>& InPartyId, const ECreatePartyCompletionResult Result, const FOnlinePartyTypeId InPartyTypeId, UPartyDelegates::FOnCreateUPartyComplete CompletionDelegate);
	void OnJoinPartyInternalComplete(const FUniqueNetId& LocalUserId, const FOnlinePartyId& InPartyId, const EJoinPartyCompletionResult Result, int32 DeniedResultCode, const FOnlinePartyTypeId InPartyTypeId, UPartyDelegates::FOnJoinUPartyComplete CompletionDelegate);
	void OnQueryPartyJoinabilityComplete(const FUniqueNetId& LocalUserId, const FOnlinePartyId& InPartyId, const EJoinPartyCompletionResult Result, int32 DeniedResultCode, const FOnlinePartyTypeId InPartyTypeId, UPartyDelegates::FOnQueryUPartyJoinabilityComplete CompletionDelegate);
	void OnLeavePartyInternalComplete(const FUniqueNetId& LocalUserId, const FOnlinePartyId& InPartyId, const ELeavePartyCompletionResult Result, const FOnlinePartyTypeId InPartyTypeId, UPartyDelegates::FOnLeaveUPartyComplete CompletionDelegate);

	/**
	 * Delegates for managing persistent parties
	 */
	void OnCreatePersistentPartyComplete(const FUniqueNetId& LocalUserId, const ECreatePartyCompletionResult Result, UPartyDelegates::FOnCreateUPartyComplete CompletionDelegate);
	void OnJoinPersistentPartyComplete(const FUniqueNetId& LocalUserId, const EJoinPartyCompletionResult Result, int32 DeniedResultCode, UPartyDelegates::FOnJoinUPartyComplete CompletionDelegate);
	void OnLeavePersistentPartyComplete(const FUniqueNetId& LocalUserId, const ELeavePartyCompletionResult Result, UPartyDelegates::FOnLeaveUPartyComplete CompletionDelegate);

	/**
	 * Party state management delegates 
	 */
	void PartyConfigChangedInternal(const FUniqueNetId& InLocalUserId, const FOnlinePartyId& InPartyId, const TSharedRef<FPartyConfiguration>& InPartyConfig);
	void PartyMemberJoinedInternal(const FUniqueNetId& InLocalUserId, const FOnlinePartyId& InPartyId, const FUniqueNetId& InMemberId);
	void PartyDataReceivedInternal(const FUniqueNetId& InLocalUserId, const FOnlinePartyId& InPartyId, const TSharedRef<FOnlinePartyData>& InPartyData);
	void PartyMemberDataReceivedInternal(const FUniqueNetId& InLocalUserId, const FOnlinePartyId& InPartyId, const FUniqueNetId& InMemberId, const TSharedRef<FOnlinePartyData>& InPartyMemberData);
	void PartyJoinRequestReceivedInternal(const FUniqueNetId& InLocalUserId, const FOnlinePartyId& InPartyId, const FUniqueNetId& SenderId);
	void PartyQueryJoinabilityReceivedInternal(const FUniqueNetId& InLocalUserId, const FOnlinePartyId& InPartyId, const FUniqueNetId& SenderId);
	void PartyMemberPromotedInternal(const FUniqueNetId& InLocalUserId, const FOnlinePartyId& InPartyId, const FUniqueNetId& InNewLeaderId);
	void PartyPromotionLockoutStateChangedInternal(const FUniqueNetId& LocalUserId, const FOnlinePartyId& InPartyId, const bool bLockoutState);

	void PartyExitedInternal(const FUniqueNetId& LocalUserId, const FOnlinePartyId& InPartyId);
	void OnPersistentPartyExitedInternalCompleted(const FUniqueNetId& LocalUserId, const ECreatePartyCompletionResult Result);

	void PartyStateChanged(const FUniqueNetId& LocalUserId, const FOnlinePartyId& InPartyId, EPartyState State);

	void LeavePersistentPartyForRejoin();
	void OnLeavePersistentPartyForRejoinComplete(const FUniqueNetId& LocalUserId, const ELeavePartyCompletionResult LeaveResult);
	void OnRejoinPartyComplete(const FUniqueNetId& LocalUserId, const FOnlinePartyId& PartyId, const EJoinPartyCompletionResult Result, int32 DeniedResultCode);

	/**
	 *
	 */
	void OnCreatePesistentPartyCompletedCommon(const FUniqueNetId& LocalUserId);

	/**
	 * Identity interface status change delegates
	 */
	void OnLogoutComplete(int32 LocalUserNum, bool bWasSuccessful);
	void OnLoginStatusChanged(int32 LocalUserNum, ELoginStatus::Type OldStatus, ELoginStatus::Type NewStatus, const FUniqueNetId& NewId);

	/**
	 * Error handling to restore persistent party after leaving a different one
	 */
	void LeaveAndRestorePersistentPartyInternal();
	void OnLeavePersistentPartyAndRestore(const FUniqueNetId& LocalUserId, const ELeavePartyCompletionResult Result);

	/**
	 * Delegate handles
	 */
	FDelegateHandle PartyConfigChangedDelegateHandle;
	FDelegateHandle PartyMemberJoinedDelegateHandle;
	FDelegateHandle PartyPromotionLockoutChangedDelegateHandle;
	FDelegateHandle PartyDataReceivedDelegateHandle;
	FDelegateHandle PartyMemberDataReceivedDelegateHandle;
	FDelegateHandle PartyJoinRequestReceivedDelegateHandle;
	FDelegateHandle PartyQueryJoinabilityReceivedDelegateHandle;
	FDelegateHandle PartyMemberPromotedDelegateHandle;
	FDelegateHandle PartyMemberExitedDelegateHandle;
	FDelegateHandle PartyExitedDelegateHandle;
	FDelegateHandle PartyStateChangedDelegateHandle;

	FDelegateHandle LogoutStatusChangedDelegateHandle[MAX_LOCAL_PLAYERS];
	FDelegateHandle LogoutCompleteDelegateHandle[MAX_LOCAL_PLAYERS];

	/**
	 * Helpers
	 */

public:

	// UObject interface begin
	virtual UWorld* GetWorld() const override;
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);
	// UObject interface end

protected:

	/** @return the game instance */
	UGameInstance* GetGameInstance() const;

};

