// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Templates/SubclassOf.h"
#include "HAL/IConsoleManager.h"
#include "Templates/Casts.h"
#include "UObject/CoreOnline.h"
#include "GameFramework/OnlineReplStructs.h"
#include "Containers/Queue.h"
#include "PartyBeaconState.h"
#include "OnlineSubsystemTypes.h"
#include "OnlineSessionSettings.h"
#include "Interfaces/OnlinePartyInterface.h"
#include "PartyGameState.generated.h"

class APartyBeaconClient;
class ULocalPlayer;
class UParty;
class UPartyMemberState;

typedef FUniqueNetIdRepl FOnlinePartyIdRepl;
class FOnlineSessionSearchResult;
class UParty;
class UPartyMemberState;
class APartyBeaconClient;
enum class EMemberExitedReason;

namespace PartyConsoleVariables
{
	extern PARTY_API TAutoConsoleVariable<int32> CVarAcceptJoinsDuringLoad;
}

UENUM(BlueprintType)
enum class EPartyType : uint8
{
	/** This party is public (not really supported right now) */
	Public,
	/** This party is joinable by friends */
	FriendsOnly,
	/** This party requires an invite from an existing party member */
	Private
};

UENUM(BlueprintType)
enum class EJoinPartyDenialReason : uint8
{
	/** No denial, matches success internally */
	NoReason = 0,
	/** Party leader is busy or at inopportune time to allow joins */
	Busy,
	/** Party is full */
	PartyFull,
	/** Game is full, but not party */
	GameFull,
	/** Asked a non party leader to join game, shouldn't happen */
	NotPartyLeader,
	/** Party has been marked as private and the join request is revoked */
	PartyPrivate,
	/** Player is still in tutorials and no able to do invites */
	NeedsTutorial
};

enum class EApprovalAction : uint8
{
	/** Immediately approve the request */
	Approve = 0,
	/** Enqueue the request */
	Enqueue,
	/** Enqueue the request and start the beacon if necessary */
	EnqueueAndStartBeacon,
	/** Deny the request */
	Deny
};

/**
 * Current state of the party
 */
USTRUCT()
struct FPartyState
{
	GENERATED_USTRUCT_BODY();

	/** What type of joinable party this is */
	UPROPERTY()
	EPartyType PartyType;

	/** Only the leader can have friends join via presence */
	UPROPERTY()
	bool bLeaderFriendsOnly;

	/** Only the leader can invite party members */
	UPROPERTY()
	bool bLeaderInvitesOnly;

	/** Are invites allowed at all? */
	UPROPERTY()
	bool bInvitesDisabled;

	FPartyState() :
		PartyType(EPartyType::FriendsOnly),
		bLeaderFriendsOnly(false),
		bLeaderInvitesOnly(false),
		bInvitesDisabled(false)
	{
	}

	virtual ~FPartyState()
	{
	}

	/** Reset party back to defaults */
	virtual void Reset(bool bIsLeader)
	{}
};

/**
 * Party game state that contains all information relevant to the communication within a party
 * Keeps all players in sync with the state of the party and its individual members
 */
UCLASS(config=Game, notplaceable)
class PARTY_API UPartyGameState : public UObject
{
	GENERATED_UCLASS_BODY()

	// Begin UObject Interface
	virtual void BeginDestroy() override;
	// End UObject Interface

private:

	/**
	 * Delegate fired when global party data changes
	 * 
	 * @param PartyData the new party data
	 */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnPartyDataChanged, const FPartyState& /* PartyData */);

	/**
	 * Delegate fired when a party member's data changes
	 * 
	 * @param MemberId id of the member whose data has changed
	 * @param PlayerData the new party member data
	 */
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnPartyMemberDataChanged, const FUniqueNetIdRepl& /* MemberId */, const UPartyMemberState* /* PlayerData */);

	/**
	 * Delegate fired when a party type changes
	 * 
	 * @param NewPartyType new party type
	 */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnPartyTypeChanged, EPartyType /* NewPartyType */);

	/**
	 * Delegate fired when a join via presence permissions change
	 * 
	 * @param bLeaderFriendsOnly only leader friends can join
	 */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnLeaderFriendsOnlyChanged, bool /* bLeaderFriendsOnly */);

	/**
	 * Delegate fired when leader invite permissions change
	 * 
	 * @param bLeaderInviteOnly only leader invites are allowed
	 */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnLeaderInvitesOnlyChanged, bool /* bLeaderInviteOnly */);

	/**
	 * Delegate fired when invites disabled changes
	 * 
	 * @param bInvitesDisabled invites are disabled
	 */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnInvitesDisabledChanged, bool /* bInvitesDisabled */);

	/**
	 * Delegate fired when a party's configuration is updated
	 * 
	 * @param NewConfiguration Updated configuration
	 */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnPartyConfigurationChanged, const FPartyConfiguration& /* NewConfiguration */);


protected:

	/** 
	 * Some property of the player changed
	 *
	 * @param MemberId id of the member whose property has changed
	 */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnPartyMemberPropertyChanged, const FUniqueNetIdRepl& /** MemberId */);

public:

	// UObject interface begin
	virtual UWorld* GetWorld() const override;
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);
	// UObject interface end

	/**
	 * Notification that the game is about to travel to another map/server
	 */
	virtual void PreClientTravel();

	/**
	 * Reset this party game state when back to the frontend
	 */
	virtual bool ResetForFrontend();

	/**
	 * Unregister delegates and clear out shared pointers to MCP objects
	 */
	virtual void OnShutdown();

	/** @return true if local player is party leader, false otherwise */
	bool IsLocalPartyLeader();

	/** @return the party id of this party */
	FOnlinePartyTypeId GetPartyTypeId() const;

	/** @return the party id of this party */
	TSharedPtr<const FOnlinePartyId> GetPartyId() const;

	/** @return the joinable type of party this is */
	EPartyType GetPartyType() const { return PartyStateRef->PartyType; }
	/** @return true if only leader friends are allowed to join */
	bool IsLeaderFriendsOnly() const { return PartyStateRef->bLeaderFriendsOnly; }
	/** @return true if only leader can invite people */
	bool IsLeaderInvitesOnly() const { return PartyStateRef->bLeaderInvitesOnly; }

	/**
	 * Set the type of joinable party this is
	 *
	 * @param InPartyType joinable party state
	 * @param bLeaderFriendsOnly only leader friends can join via presence
	 * @param bLeaderInvitesOnly only leader can send invites to others
	 */
	void SetPartyType(EPartyType InPartyType, bool bLeaderFriendsOnly, bool bLeaderInvitesOnly);

	/**
	 * Disable (or re-enable) the ability to send party invites
	 *
	 * @param bInvitesDisabled invites are allowed to be sent
	 */
	void SetInvitesDisabled(bool bInvitesDisabled);

	/**
	 * Set stay with party on disconnects or not
	 */
	void StayWithPartyOnExit(bool bInStayWithParty);

	/** @return whether or not to stay with the party on disconnect */
	bool ShouldStayWithPartyOnExit() const;

	/** @return the current size of the party */
	int32 GetPartySize() const;

	/**
	 * Set the max size of the party
	 */
	void SetPartyMaxSize(int32 NewSize);

	/** @return the max size of the party, INDEX_NONE if invalid */
	int32 GetPartyMaxSize() const;

	/** @return True if the party is currently full */
	bool IsPartyFull() const;

	/**
	 * Look at current data and update whether or not this party should be accepting members (party leader only)
	 */
	virtual void UpdateAcceptingMembers();

	/**
	 * Set if this party is accepting members (disables remote presence joins)
	 * NOTE: prefer calling UpdateAcceptingMembers instead as it does some work to make the right decision
	 *
	 * @param bIsAcceptingMembers true if currently accepting, false otherwise
	 * @param DenialReason reason for not accepting members to communicate via presence
	 */
	void SetAcceptingMembers(bool bIsAcceptingMembers, EJoinPartyDenialReason DenialReason);

	/**
	 * Check if we are currently accepting members.
	 * 
	 * @param Optional pointer to a Denial reason that is set when the party is not joinable
	 */
	bool IsAcceptingMembers(EJoinPartyDenialReason* const DenialReason = nullptr) const;

	/**
	 * Check if you have permission to send invites to this party
	 *
	 * @return true if you have permission
	 */
	bool CanInvite() const;

	/** @return the party leader for this party */
	TSharedPtr<const FUniqueNetId> GetPartyLeader() const;

	/**
	 * Promote a new party leader, demoting the existing leader in the process
	 *
	 * @param NewPartyLeader id of the player to promote
	 */
	void PromoteMember(const FUniqueNetIdRepl& NewPartyLeader);

	/**
	 * Kick a member of the party, must be the party leader
	 *
	 * @param PartyMemberToKick id of the player to kick
	 */
	void KickMember(const FUniqueNetIdRepl& PartyMemberToKick);

	/** 
	 * Get a party member in this party
	 *
	 * @param InUniqueId party member id to look for
	 *
	 * @return party member specified by the given unique id 
	 */
	UPartyMemberState* GetPartyMember(const FUniqueNetIdRepl& InUniqueId) const;

	/**
	 * Get all party members in this party
	 *
	 * @param PartyMembers [out] array of all party members in this party
	 */
	void GetAllPartyMembers(TArray<UPartyMemberState*>& PartyMembers) const;

	/**
	 * Get the chat room ID for this party
	 *
	 * @return The chat room ID
	 */
	virtual FChatRoomId GetChatRoomID() const;

	template< typename TPartyMemberState >
	void GetTypedPartyMembers(TArray< TPartyMemberState* >& PartyMembers) const
	{
		PartyMembers.Empty(PartyMembersState.Num());
		for (auto Iter = PartyMembersState.CreateConstIterator(); Iter; ++Iter)
		{
			PartyMembers.Add(Cast< TPartyMemberState >(Iter.Value()));
		}
	}

	/** @return Unique ID of the user who created or joined this room (not the party leader) */
	const FUniqueNetIdRepl& GetOwningUserId() const { return OwningUserId; }

	/** @return delegate fired when global party data has changed */
	FOnPartyDataChanged& OnPartyDataChanged() { return PartyDataChanged; }
	/** @return delegate fired when an individual party member's data has changed */
	FOnPartyMemberDataChanged& OnPartyMemberDataChanged() { return PartyMemberDataChanged; }

	/** @return delegate fired when the party type has changed */
	FOnPartyTypeChanged& OnPartyTypeChanged() { return PartyTypeChanged; }
	/** @return delegate fired when the leader based join permissions have changed */
	FOnLeaderFriendsOnlyChanged& OnLeaderFriendsOnlyChanged() { return LeaderFriendsOnlyChanged; }
	/** @return delegate fired when the leader based invite permissions have changed */
	FOnLeaderInvitesOnlyChanged& OnLeaderInvitesOnlyChanged() { return LeaderInvitesOnlyChanged; }
	/** @return delegate fired when invite permissions have changed */
	FOnInvitesDisabledChanged& OnInvitesDisabledChanged() { return InvitesDisabledChanged; }
	/** @return delegate fired when the party configuration has changed */
	FOnPartyConfigurationChanged& OnPartyConfigurationChanged() { return PartyConfigurationChanged; }
	
protected:

	template<typename T>
	void InitPartyState(T* InPartyState)
	{
		PartyStateRef = InPartyState;
		PartyStateRefDef = T::StaticStruct();

		PartyStateRefScratch = (FPartyState*)FMemory::Malloc(PartyStateRefDef->GetCppStructOps()->GetSize());
		PartyStateRefDef->InitializeStruct(PartyStateRefScratch);
	}

	/**
	 * Initialize a party that is newly created and a local player owns it
	 *
	 * @param LocalUserID party owner/leader (may change later if user promotes another leader)
	 * @param InParty reference to the basic party info from the interface
	 */
	virtual void InitFromCreate(const FUniqueNetId& LocalUserId, TSharedPtr<const FOnlineParty>& InParty);

	/**
	 * Initialize a party that has been joined and a local player is simply a member
	 *
	 * @param LocalUserID local primary player that joined the party
	 * @param InParty reference to the basic party info from the interface
	 */
	void InitFromJoin(const FUniqueNetId& LocalUserId, TSharedPtr<const FOnlineParty>& InParty);

protected:

	/** Reflection data for child USTRUCT */
	UPROPERTY()
	const UScriptStruct* PartyStateRefDef;

	/**
	 * Pointer to child USTRUCT that holds the current state of party member (set via InitPartyMemberState)
	 *
	 * Cached data for the party, only modifiable by the party leader
	 * Reference to the data structure defined in a child class
	 */
	FPartyState* PartyStateRef;

	/** User who created or joined this room (not the party leader) */
	UPROPERTY()
	FUniqueNetIdRepl OwningUserId;

	/** Current party configuration (shadow internal interface) */
	FPartyConfiguration CurrentConfig;

	/** Debug boolean to shadow the CurrentConfig accepting members value */
	UPROPERTY()
	bool bDebugAcceptingMembers;

	/** Reference to party info within OSS */
	TSharedPtr<const FOnlineParty> OssParty;

	/** Is leader promotion available at the moment */
	bool bPromotionLockoutState;
	
	/** Should the play stay with the party on exit */
	UPROPERTY()
	bool bStayWithPartyOnDisconnect;

	/** Class of party state to be used for parties*/
	UPROPERTY(Transient)
	TSubclassOf<UPartyMemberState> PartyMemberStateClass;

	/**
	 * Cached data for all the players in the existing persistent party
	 */
	TMap<FUniqueNetIdRepl, UPartyMemberState*> PartyMembersState;

	/** Current game session this party is in, if applicable */
	FOnlineSessionSearchResult CurrentSession;

	/** Reservation beacon class for getting server approval for new party members while in a game */
	UPROPERTY(Transient)
	TSubclassOf<APartyBeaconClient> ReservationBeaconClientClass;
	/** Reservation beacon client instance while getting approval for new party members*/
	UPROPERTY(Transient)
	APartyBeaconClient* ReservationBeaconClient;

	/**
	 * Holds information about party members needing approval with the game server
	 */
	struct FPendingMemberApproval
	{
		FUniqueNetIdRepl RecipientId;
		FUniqueNetIdRepl SenderId;
	};

	/** All currently pending approvals for new members */
	TQueue<FPendingMemberApproval> PendingApprovals;

	/**
	 * Delegates for party data changes
	 */
	FOnPartyDataChanged PartyDataChanged;
	FOnPartyMemberDataChanged PartyMemberDataChanged;

	/**
	 * Delegates for party visibility/presence/invite permission changes
	 */
	FOnPartyTypeChanged PartyTypeChanged;
	FOnLeaderFriendsOnlyChanged LeaderFriendsOnlyChanged;
	FOnLeaderInvitesOnlyChanged LeaderInvitesOnlyChanged;
	FOnInvitesDisabledChanged InvitesDisabledChanged;
	FOnPartyConfigurationChanged PartyConfigurationChanged;

	/**
	 * Common initialization for a newly instantiated party
	 */
	void Init(const FUniqueNetId& LocalUserId, TSharedPtr<const FOnlineParty>& InParty);

	/** Register for game related delegates that affect the party */
	virtual void RegisterFrontendDelegates();
	/** Unregister from game related delegates that affect the party */
	virtual void UnregisterFrontendDelegates();

	/** Resets the party state back to defaults */
	virtual void ResetPartyState();
	/** Reset the party size back to some game determined default */
	virtual void ResetPartySize();

	/** Resets the local players state to defaults */
	void ResetLocalPlayerState();
	
	/**
	 * Generic conversion of the party data structure for passing to other players (only party leader can call this)
	 *
	 * @param InLocalUserId local player making the request, needs to be the party leader
	 */
	void UpdatePartyData(const FUniqueNetIdRepl& InLocalUserId);

	/**
	 * Generic conversion of a single party member's data structure for passing to other players
	 *
	 * @param InLocalUserId local player making the request
	 * @param InPartyData reference to the new state of their party member data
	 */
	void UpdatePartyMemberState(const FUniqueNetIdRepl& InLocalUserId, const UPartyMemberState* InPartyPlayerData);

	/**
	 * Create and initialize a party member state value for a local player prior to transmission to other party members
	 */
	virtual UPartyMemberState* InitPartyMemberStateFromLocalPlayer(const ULocalPlayer* LocalPlayer);

	/**
	 * Send all local player data to other party members, typically the initial call
	 */
	void SendLocalPlayerPartyData();

	/** @return Is this player in a joinable game or no game at all (meant for local use only, remote perspective is elsewhere) */
	virtual bool IsInJoinableGameState() const;

	/**
	 * Compare old party data to new party data, triggering appropriate delegates
	 *
	 * @param OldPartyData old view of party data
	 * @param NewPartyData new view of party data
	 */
	virtual void ComparePartyData(const FPartyState& OldPartyData, const FPartyState& NewPartyData);

	/**
	 * Apply current party configuration settings stored at this level to the lower level interface code
	 *
	 * @param bResetAccessKey should the update reset the access key (invalidates existing invites/presence)
	 */
	void UpdatePartyConfig(bool bResetAccessKey = false);
	void OnUpdatePartyConfigComplete(const FUniqueNetId& LocalUserId, const FOnlinePartyId& InPartyId, const EUpdateConfigCompletionResult);

	/**
	 * Called for all existing party members when a party configuration setting changes
	 *
	 * @param InPartyConfig new party configuration settings
	 */
	void HandlePartyConfigChanged(const TSharedRef<FPartyConfiguration>& InPartyConfig);

	/**
	 * Called for all existing party members when a new party member joins
	 *
	 * @param InMemberId id of the newly joined member
	 */
	virtual void HandlePartyMemberJoined(const FUniqueNetId& InMemberId);

	/**
	 * Called for all existing party members when an existing party member leaves
	 *
	 * @param InMemberId id of the leaving member
	 * @param Reason reason the member left
	 */
	virtual void HandlePartyMemberLeft(const FUniqueNetId& InMemberId, EMemberExitedReason Reason);

	/**
	 * Called for all existing party members when an existing member is promoted to leader
	 *
	 * @param InMemberId id of the member promoted to leader
	 */
	virtual void HandlePartyMemberPromoted(const FUniqueNetId& InMemberId);

	/**
	 * Called for all existing party members when the party state has changed by the leader
	 *
	 * @param InPartyData changed data for the party (not necessarily all state data)
	 */
	void HandlePartyDataReceived(const TSharedRef<FOnlinePartyData>& InPartyData);

	/**
	 * Called fro all existing party members when an individual party member's data has changed
	 *
	 * @param InMemberId member id for whom the data has changed
	 * @param InPartyMemberData changed data for the party member (not necessarily all state data)
	 */
	void HandlePartyMemberDataReceived(const FUniqueNetId& InMemberId, const TSharedRef<FOnlinePartyData>& InPartyMemberData);

	/**
	 * Called from the party interface internals whenever there is a period of time that promoting other members is not allowed
	 * Typically during players joining/leaving or other moments when figuring out a leader is important
	 *
	 * @param bNewLockoutState current state of promotion lockout
	 */
	void HandleLockoutPromotionStateChange(bool bNewLockoutState);
	
	/**
	 * Called at leaving initiation, clear out delegates and anything that might be done before actually leaving
	 */
	void HandleLeavingParty();

	/**
	 * Called when leaving is complete, cleanup as this state will be deleted immediately afterward
	 * 
	 * @param Reason reason local players were removed from party
	 */
	virtual void HandleRemovedFromParty(EMemberExitedReason Reason);

	/** Party leader delegates */
	void OnPartyMemberPromoted(const FUniqueNetId& LocalUserId, const FOnlinePartyId& InPartyId, const FUniqueNetId& InMemberId, const EPromoteMemberCompletionResult Result);
	void OnPartyMemberKicked(const FUniqueNetId& LocalUserId, const FOnlinePartyId& InPartyId, const FUniqueNetId& InMemberId, const EKickMemberCompletionResult Result);

	/**
	 * New member approvals
	 */

	/**
	 * Called on the party leader to approve requests for incoming potential party members
	 *
	 * @param RecipientId whom this request is for (party leader)
	 * @param SendingId whom this request is from
	 */
	void HandlePartyJoinRequestReceived(const FUniqueNetId& RecipientId, const FUniqueNetId& SenderId);

	/**
	 * Called on the party leader to do a quick determination of whether the party is joinable
	 *
	 * @param RecipientId whom this request is for (party leader)
	 * @param SendingId whom this request is from
	 */
	void HandlePartyQueryJoinabilityRequestReceived(const FUniqueNetId& RecipientId, const FUniqueNetId& SenderId);

	/**
	 * Game specific decision making about party approvals
	 *
	 * @param RecipientId whom this request is for (party leader)
	 * @param SendingId whom this request is from
	 * @param [out] Reason for rejection, if applicable
	 *
	 * @return approval action for this join party request
	 */
	virtual EApprovalAction ProcessJoinRequest(const FUniqueNetId& RecipientId, const FUniqueNetId& SenderId, EJoinPartyDenialReason& DenialReason) const;

	/**
	 * Unilaterally reject all pending join requests 
	 */
	void RejectAllPendingJoinRequests();

	/**
	 * Create a reservation beacon and connect to the server to get approval for new party members
	 * Only relevant while in an active game, not required while pre lobby / game
	 */
	void ConnectToReservationBeacon();
	
	/** Delegate fired when there is a failure to connect to the server reservation beacon */
	void OnReservationBeaconUpdateConnectionFailure();

	/**
	 * Delegate fired when there is a response for a party reservation update
	 *
	 * @param ReservationResponse response from the server regarding a new party member's approval
	 */
	void OnReservationBeaconUpdateResponseReceived(EPartyReservationResult::Type ReservationResponse);

	/**
	 * Delegate fired when there are reservation updates on the server (not used by party)
	 *
	 * @param NumRemaining number of remaining reservations in the game
	 */
	void OnReservationCountUpdate(int32 NumRemaining);

	/** Cleanup the reservation beacon client when new approvals are complete */
	void CleanupReservationBeacon();

	/**
	 * Helpers
	 */

	/**
	 * Create a new party member
	 *
	 * @param InMemberId unique id of the player joining
	 *
	 * @return new party member state object
	 */
	UPartyMemberState* CreateNewPartyMember(const FUniqueNetId& InMemberId);

	/** @return the party singleton that manages all parties */
	UParty* GetPartyOuter() const;

	/** 
	 * Get the current session info
	 * 
	 * @param SessionName session name to get information for
	 * @param URL the connect URL for the current session
	 * @param SessionId the current session Id
	 */
	void GetSessionInfo(FName SessionName, FString& URL, FString& SessionId) const;

private:
	
	virtual void PrePromoteMember();

	/** Scratch copy of child USTRUCT for handling replication comparisons */
	FPartyState* PartyStateRefScratch;

	friend UParty;
	friend UPartyMemberState;
};

inline const TCHAR* ToString(EPartyType Type)
{
	switch (Type)
	{
	case EPartyType::Public:
	{
		return TEXT("Public");
	}
	case EPartyType::FriendsOnly:
	{
		return TEXT("FriendsOnly");
	}
	case EPartyType::Private:
	{
		return TEXT("Private");
	}
	default:
	{
		return TEXT("Unknown");
	}
	}
}

inline const TCHAR* ToString(EJoinPartyDenialReason Type)
{
	switch (Type)
	{
	case EJoinPartyDenialReason::NoReason:
	{
		return TEXT("NoReason");
	}
	case EJoinPartyDenialReason::Busy:
	{
		return TEXT("Busy");
	}
	case EJoinPartyDenialReason::PartyFull:
	{
		return TEXT("PartyFull");
	}
	case EJoinPartyDenialReason::GameFull:
	{
		return TEXT("GameFull");
	}
	case EJoinPartyDenialReason::NotPartyLeader:
	{
		return TEXT("NotPartyLeader");
	}
	case EJoinPartyDenialReason::PartyPrivate:
	{
		return TEXT("PartyPrivate");
	}
	case EJoinPartyDenialReason::NeedsTutorial:
	{
		return TEXT("NeedsTutorial");
	}
	default:
	{
		return TEXT("Unknown");
	}
	}
}

inline const TCHAR* ToString(EApprovalAction Type)
{
	switch (Type)
	{
	case EApprovalAction::Approve:
	{
		return TEXT("Approve");
	}
	case EApprovalAction::Enqueue:
	{
		return TEXT("Enqueue");
	}
	case EApprovalAction::EnqueueAndStartBeacon:
	{
		return TEXT("EnqueueAndStartBeacon");
	}
	case EApprovalAction::Deny:
	{
		return TEXT("Deny");
	}
	default:
	{
		return TEXT("Unknown");
	}
	}
}
