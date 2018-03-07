// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "PartyGameState.h"
#include "Engine/LocalPlayer.h"
#include "UnrealEngine.h"
#include "PartyModule.h"
#include "PartyMemberState.h"
#include "Party.h"
#include "PartyBeaconClient.h"
#include "OnlineSubsystemUtils.h"

namespace PartyConsoleVariables
{
	// CVars
	TAutoConsoleVariable<int32> CVarAcceptJoinsDuringLoad(
		TEXT("Party.CVarAcceptJoinsDuringLoad"),
		1,
		TEXT("Enables joins while leader is trying to load into a game\n")
		TEXT("1 Enables. 0 disables."),
		ECVF_Default);
}


UPartyGameState::UPartyGameState(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer),
	PartyStateRefDef(nullptr),
	PartyStateRef(nullptr),
	OwningUserId(nullptr),
	bDebugAcceptingMembers(false),
	OssParty(nullptr),
	bPromotionLockoutState(false),
	bStayWithPartyOnDisconnect(false),
	ReservationBeaconClientClass(nullptr),
	ReservationBeaconClient(nullptr)
{
	PartyMemberStateClass = UPartyMemberState::StaticClass();
	ReservationBeaconClientClass = APartyBeaconClient::StaticClass();

	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
	}
}

void UPartyGameState::BeginDestroy()
{
	Super::BeginDestroy();
	OnShutdown();
}

void UPartyGameState::OnShutdown()
{
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		UnregisterFrontendDelegates();
	}

	OssParty.Reset();
	PartyMembersState.Empty();

	if (PartyStateRefDef)
	{
		if (PartyStateRefScratch)
		{
			PartyStateRefDef->DestroyStruct(PartyStateRefScratch);
			FMemory::Free(PartyStateRefScratch);
			PartyStateRefScratch = nullptr;
		}
		PartyStateRefDef = nullptr;
	}

	PartyStateRef = nullptr;
}

/*static*/ void UPartyGameState::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	Super::AddReferencedObjects(InThis, Collector);

	UPartyGameState* This = CastChecked<UPartyGameState>(InThis);
	check(This);

	TArray<UPartyMemberState*> PartyMembers;
	This->PartyMembersState.GenerateValueArray(PartyMembers);
	Collector.AddReferencedObjects(PartyMembers);
}

void UPartyGameState::RegisterFrontendDelegates()
{
	UWorld* World = GetWorld();

	UnregisterFrontendDelegates();
}

void UPartyGameState::UnregisterFrontendDelegates()
{
}

void UPartyGameState::ResetPartyState()
{
	if (PartyStateRef)
	{
		// Reset Party State
		PartyStateRef->Reset(IsLocalPartyLeader());
	}
}

void UPartyGameState::ResetPartySize()
{
	UParty* Party = GetPartyOuter();
	SetPartyMaxSize(Party->GetDefaultPartyMaxSize());
}

void UPartyGameState::ResetLocalPlayerState()
{
	UWorld* World = GetWorld();
	check(World);

	for (FLocalPlayerIterator It(GEngine, World); It; ++It)
	{
		ULocalPlayer* LP = *It;
		if (LP)
		{
			FUniqueNetIdRepl UniqueId(LP->GetPreferredUniqueNetId());
			if (UniqueId.IsValid())
			{
				UPartyMemberState** LocalPartyMemberStatePtr = PartyMembersState.Find(UniqueId);
				UPartyMemberState* LocalPartyMemberState = LocalPartyMemberStatePtr ? *LocalPartyMemberStatePtr : nullptr;
				if (LocalPartyMemberState)
				{
					LocalPartyMemberState->Reset();
				}
			}
		}
	}
}

void UPartyGameState::Init(const FUniqueNetId& LocalUserId, TSharedPtr<const FOnlineParty>& InParty)
{
	if (ensure(InParty.IsValid()))
	{
		if (ensure(LocalUserId.IsValid()))
		{
			OwningUserId.SetUniqueNetId(LocalUserId.AsShared());
			OssParty = InParty;

			CurrentConfig = *OssParty->Config;
			bDebugAcceptingMembers = CurrentConfig.bIsAcceptingMembers;

			// Last since it needs the party info/id set first
			RegisterFrontendDelegates();
		}
		else
		{
			UE_LOG(LogParty, Warning, TEXT("Init: Invalid owning user id!"));
		}
	}
	else
	{
		UE_LOG(LogParty, Warning, TEXT("Init: Invalid party!"));
	}
}

void UPartyGameState::InitFromCreate(const FUniqueNetId& LocalUserId, TSharedPtr<const FOnlineParty>& InParty)
{
	if (ensure(InParty.IsValid()))
	{
		Init(LocalUserId, InParty);

		// Setup initial data for the party
		ResetPartyState();

		FUniqueNetIdRepl MemberId(LocalUserId.AsShared());
		UpdatePartyData(MemberId);

		// Make sure we create the local player's entry before broadcasting the join
		SendLocalPlayerPartyData();

		// Broadcast join
		UParty* Party = GetPartyOuter();
		check(Party);
		Party->OnPartyJoined().Broadcast(this);
	}
	else
	{
		UE_LOG(LogParty, Warning, TEXT("InitFromCreate: Invalid party!"));
	}
}

void UPartyGameState::InitFromJoin(const FUniqueNetId& LocalUserId, TSharedPtr<const FOnlineParty>& InParty)
{
	if (ensure(InParty.IsValid()))
	{
		Init(LocalUserId, InParty);

		// Make sure we create the local player's entry before broadcasting the join
		SendLocalPlayerPartyData();

		// Broadcast join
		UParty* Party = GetPartyOuter();
		check(Party);
		Party->OnPartyJoined().Broadcast(this);
	}
	else
	{
		UE_LOG(LogParty, Warning, TEXT("InitFromJoin: Invalid party!"));
	}
}

void UPartyGameState::PreClientTravel()
{
	if (!PartyConsoleVariables::CVarAcceptJoinsDuringLoad.GetValueOnGameThread())
	{
		// Possibly deal with pending approvals?
		RejectAllPendingJoinRequests();
	}
	CleanupReservationBeacon();

	UnregisterFrontendDelegates();
}

bool UPartyGameState::ResetForFrontend()
{
	UE_LOG(LogParty, Verbose, TEXT("Resetting parties for frontend"));

	bool bSuccess = false;
	bool bPendingApprovalsReprocessed = false;

	CleanupReservationBeacon();

	if (ensure(OssParty.IsValid()))
	{
		if (ensure(OwningUserId.IsValid()))
		{
			UWorld* World = GetWorld();
			IOnlinePartyPtr PartyInt = Online::GetPartyInterface(World);
			if (ensure(PartyInt.IsValid()))
			{
				TSharedPtr<const FOnlineParty> ExistingParty = PartyInt->GetParty(*OwningUserId, *OssParty->PartyId);
				if (ExistingParty.IsValid())
				{
					bSuccess = true;

					Init(*OwningUserId, ExistingParty);
					bStayWithPartyOnDisconnect = false;

					ResetPartyState();
					ResetLocalPlayerState();

					const bool bIsPartyLeader = IsLocalPartyLeader();

					// Refresh local player data
					SendLocalPlayerPartyData();

					// Remove members we have, but lower level doesn't know about
					for (const auto& PartyMember : PartyMembersState)
					{
						const FUniqueNetIdRepl& MemberId = PartyMember.Key;
						if (MemberId.IsValid())
						{
							TSharedPtr<FOnlinePartyMember> CheckPartyMember = PartyInt->GetPartyMember(*OwningUserId, *OssParty->PartyId, *MemberId);
							if (!CheckPartyMember.IsValid())
							{
								UE_LOG(LogParty, Verbose, TEXT("[%s] Player %s left during fixup"), *OssParty->PartyId->ToString(), *MemberId.ToString());
								HandlePartyMemberLeft(*MemberId, EMemberExitedReason::Left);
							}
						}
					}

					// Add members we don't have, but lower level does
					TArray<TSharedRef<FOnlinePartyMember>> PartyMembers;
					PartyInt->GetPartyMembers(*OwningUserId, *OssParty->PartyId, PartyMembers);
					for (const auto& PartyMember : PartyMembers)
					{
						TSharedRef<const FUniqueNetId> MemberId = PartyMember->GetUserId();

						FUniqueNetIdRepl UniqueId(MemberId);
						UPartyMemberState** CurrentPartyMemberDataPtr = PartyMembersState.Find(UniqueId);
						UPartyMemberState* CurrentPartyMemberData = CurrentPartyMemberDataPtr ? *CurrentPartyMemberDataPtr : nullptr;
						if (!CurrentPartyMemberData)
						{
							TSharedPtr<FOnlinePartyData> PartyMemberData = PartyInt->GetPartyMemberData(*OwningUserId, *OssParty->PartyId, *MemberId);
							if (PartyMemberData.IsValid())
							{
								UE_LOG(LogParty, Verbose, TEXT("[%s] Player %s data received during fixup"), *OssParty->PartyId->ToString(), *UniqueId.ToString());
								HandlePartyMemberDataReceived(*MemberId, PartyMemberData.ToSharedRef());
							}
						}
					}

					if (bIsPartyLeader)
					{
						UpdatePartyData(OwningUserId);
						ResetPartySize();
						UpdateAcceptingMembers();
					}

					// Re-process any outstanding approval requests now that we are not connected to the reservation beacon anymore
					bPendingApprovalsReprocessed = true;
					if (!PendingApprovals.IsEmpty())
					{
						UE_LOG(LogParty, Verbose, TEXT("Reprocessing pending approvals as we are no longer connected to the reservation beacon"));
						FPendingMemberApproval PendingApproval;
						TQueue<FPendingMemberApproval> ExistingPendingApprovals;
						while (PendingApprovals.Dequeue(PendingApproval))
						{
							ExistingPendingApprovals.Enqueue(PendingApproval);
						}

						while (ExistingPendingApprovals.Dequeue(PendingApproval))
						{
							HandlePartyJoinRequestReceived(*PendingApproval.RecipientId, *PendingApproval.SenderId);
						}

					}
				}
				else
				{
					UE_LOG(LogParty, Warning, TEXT("Party interface can't find party during reset!"));
				}
			}
			else
			{
				UE_LOG(LogParty, Warning, TEXT("Invalid party interface during reset!"));
			}
		}
		else
		{
			UE_LOG(LogParty, Warning, TEXT("Invalid owning user during reset!"));
		}
	}
	else
	{
		UE_LOG(LogParty, Warning, TEXT("Invalid party info during reset!"));
	}

	if (!bPendingApprovalsReprocessed && !PendingApprovals.IsEmpty())
	{
		UE_LOG(LogParty, Verbose, TEXT("Rejecting pending approvals as we are no longer connected to the reservation beacon"));
		RejectAllPendingJoinRequests();
	}

	if (!bSuccess)
	{
		ResetPartyState();
		ResetLocalPlayerState();
		UnregisterFrontendDelegates();
	}

	return bSuccess;
}

UPartyMemberState* UPartyGameState::CreateNewPartyMember(const FUniqueNetId& InMemberId)
{
	UPartyMemberState* NewPartyMemberState = nullptr;

	if (ensure(InMemberId.IsValid()))
	{
		UWorld* World = GetWorld();
		check(World);
		IOnlinePartyPtr PartyInt = Online::GetPartyInterface(World);
		if (ensure(PartyInt.IsValid()))
		{
			TSharedPtr<FOnlinePartyMember> PartyMember = PartyInt->GetPartyMember(*OwningUserId, *OssParty->PartyId, InMemberId);
			if (PartyMember.IsValid())
			{
				NewPartyMemberState = NewObject<UPartyMemberState>(this, PartyMemberStateClass);
				NewPartyMemberState->UniqueId.SetUniqueNetId(PartyMember->GetUserId());
				NewPartyMemberState->DisplayName = FText::FromString(PartyMember->GetDisplayName());
			}
			else
			{
				UE_LOG(LogParty, Warning, TEXT("CreateNewPartyMember: Invalid party member %s"), *InMemberId.ToString());
			}
		}
		else
		{
			UE_LOG(LogParty, Warning, TEXT("CreateNewPartyMember: No party interface."));
		}
	}
	else
	{
		UE_LOG(LogParty, Warning, TEXT("CreateNewPartyMember: Invalid member id."));
	}

	return NewPartyMemberState;
}

void UPartyGameState::HandlePartyConfigChanged( const TSharedRef<FPartyConfiguration>& InPartyConfig )
{
	UE_LOG(LogParty, VeryVerbose, TEXT("[%s] HandlePartyConfigChanged"), OssParty.IsValid() ? *OssParty->PartyId->ToString() : TEXT("INVALID"));
	if (ensure(OssParty.IsValid()))
	{
		CurrentConfig = *OssParty->Config;
		bDebugAcceptingMembers = CurrentConfig.bIsAcceptingMembers;
	}
}

void UPartyGameState::HandlePartyMemberJoined(const FUniqueNetId& InMemberId)
{
	UE_LOG(LogParty, VeryVerbose, TEXT("[%s] HandlePartyMemberJoined %s"), OssParty.IsValid() ? *OssParty->PartyId->ToString() : TEXT("INVALID"), *InMemberId.ToString());

	TSharedRef<const FUniqueNetId> IdRef = InMemberId.AsShared();
	FUniqueNetIdRepl MemberId(IdRef);
	if (MemberId.IsValid())
	{
		UPartyMemberState** NewPartyMemberPtr = PartyMembersState.Find(MemberId);
		UPartyMemberState* NewPartyMember = NewPartyMemberPtr ? *NewPartyMemberPtr : nullptr;
		if (!NewPartyMember)
		{
			NewPartyMember = CreateNewPartyMember(InMemberId);
			if (NewPartyMember)
			{
				PartyMembersState.Add(MemberId, NewPartyMember);
			}
		}

		if (ensure(NewPartyMember))
		{
			if (!NewPartyMember->bHasAnnouncedJoin)
			{
				// Both local and remote players will announce joins
				UParty* Party = GetPartyOuter();
				check(Party);
				Party->OnPartyMemberJoined().Broadcast(this, MemberId);
				NewPartyMember->bHasAnnouncedJoin = true;
			}
		}

		UpdateAcceptingMembers();

		UWorld* World = GetWorld();
		check(World);
		IOnlinePartyPtr PartyInt = Online::GetPartyInterface(World);
		if (ensure(PartyInt.IsValid()))
		{
			PartyInt->ApproveUserForRejoin(*OwningUserId, *OssParty->PartyId, InMemberId);
		}
	}
}

void UPartyGameState::HandlePartyMemberLeft(const FUniqueNetId& InMemberId, EMemberExitedReason Reason)
{
	UE_LOG(LogParty, VeryVerbose, TEXT("[%s] HandlePartyMemberLeft %s"), OssParty.IsValid() ? *OssParty->PartyId->ToString() : TEXT("INVALID"), *InMemberId.ToString());

	if (ensure(InMemberId.IsValid()))
	{
		TSharedRef<const FUniqueNetId> IdRef = InMemberId.AsShared();
		FUniqueNetIdRepl MemberId(IdRef);
		
		UParty* Party = GetPartyOuter();
		if (ensure(Party))
		{
			Party->OnPartyMemberLeaving().Broadcast(this, MemberId, Reason);
		}

		PartyMembersState.Remove(MemberId);

		if (Party)
		{ 
			Party->OnPartyMemberLeft().Broadcast(this, MemberId, Reason);
		}
		
		// Update party join state, will cause a failure on leader promotion currently
		// because we can't tell the difference between "expected leader" and "actually the new leader"
		UpdateAcceptingMembers();

		UWorld* World = GetWorld();
		check(World);
		if (Reason != EMemberExitedReason::Removed)
		{
			IOnlinePartyPtr PartyInt = Online::GetPartyInterface(World);
			if (ensure(PartyInt.IsValid()))
			{
				PartyInt->RemoveUserForRejoin(*OwningUserId, *OssParty->PartyId, InMemberId);
			}
		}
		else
		{
			// TODO:  Add a timer to remove players eventually
		}
	}
}

void UPartyGameState::HandlePartyMemberPromoted(const FUniqueNetId& InMemberId)
{
	ensure(OssParty.IsValid());
	UE_LOG(LogParty, VeryVerbose, TEXT("[%s] HandlePartyMemberPromoted %s"), OssParty.IsValid() ? *OssParty->PartyId->ToString() : TEXT("INVALID"), *InMemberId.ToString());

	if (ensure(InMemberId.IsValid()))
	{
		UParty* Party = GetPartyOuter();
		if (ensure(Party))
		{
			TSharedRef<const FUniqueNetId> IdRef = InMemberId.AsShared();
			FUniqueNetIdRepl MemberId(IdRef);
			Party->OnPartyMemberPromoted().Broadcast(this, MemberId);
		}
	}

	// Now that the leader is gone and a new leader established, make sure the accepting state is correct
	UpdateAcceptingMembers();
}

void UPartyGameState::ComparePartyData(const FPartyState& OldPartyData, const FPartyState& NewPartyData)
{
	// Client passenger view delegates, leader won't get these because they are driving
	if (!IsLocalPartyLeader())
	{
		if (OldPartyData.PartyType != NewPartyData.PartyType)
		{
			OnPartyTypeChanged().Broadcast(NewPartyData.PartyType);
		}

		if (OldPartyData.bLeaderFriendsOnly != NewPartyData.bLeaderFriendsOnly)
		{
			OnLeaderFriendsOnlyChanged().Broadcast(NewPartyData.bLeaderFriendsOnly);
		}

		if (OldPartyData.bLeaderInvitesOnly != NewPartyData.bLeaderInvitesOnly)
		{
			OnLeaderInvitesOnlyChanged().Broadcast(NewPartyData.bLeaderInvitesOnly);
		}
	}
}

void UPartyGameState::HandlePartyDataReceived(const TSharedRef<FOnlinePartyData>& InPartyData)
{
	UE_LOG(LogParty, VeryVerbose, TEXT("[%s] HandlePartyDataReceived"), OssParty.IsValid() ? *OssParty->PartyId->ToString() : TEXT("INVALID"));

	UWorld* World = GetWorld();
	check(World);
	IOnlinePartyPtr PartyInt = Online::GetPartyInterface(World);
	if (ensure(PartyInt.IsValid()))
	{
		check(PartyStateRefDef && PartyStateRef);
		if (FVariantDataConverter::VariantMapToUStruct(InPartyData->GetKeyValAttrs(), PartyStateRefDef, PartyStateRefScratch, 0, CPF_Transient | CPF_RepSkip))
		{
			ComparePartyData(*PartyStateRef, *PartyStateRefScratch);

			ensure(PartyStateRefDef->GetCppStructOps()->Copy(PartyStateRef, PartyStateRefScratch, 1));
			OnPartyDataChanged().Broadcast(*PartyStateRef);
		}
		else
		{
			UE_LOG(LogParty, Warning, TEXT("Failed to serialize party data!"));
		}
	}
}

void UPartyGameState::HandlePartyMemberDataReceived(const FUniqueNetId& InMemberId, const TSharedRef<FOnlinePartyData>& InPartyMemberData)
{
	UE_LOG(LogParty, VeryVerbose, TEXT("[%s] HandlePartyMemberDataReceived %s"), OssParty.IsValid() ? *OssParty->PartyId->ToString() : TEXT("INVALID"), *InMemberId.ToString());

	UWorld* World = GetWorld();
	check(World);
	IOnlinePartyPtr PartyInt = Online::GetPartyInterface(World);
	if (ensure(PartyInt.IsValid()))
	{
		FUniqueNetIdRepl UniqueId(InMemberId.AsShared());
		UPartyMemberState** CurrentPartyMemberPtr = PartyMembersState.Find(UniqueId);
		UPartyMemberState* CurrentPartyMember = CurrentPartyMemberPtr ? *CurrentPartyMemberPtr : nullptr;
		if (!CurrentPartyMember)
		{
			CurrentPartyMember = CreateNewPartyMember(InMemberId);
			if (CurrentPartyMember)
			{
				PartyMembersState.Add(UniqueId, CurrentPartyMember);
			}
		}

		if (ensure(CurrentPartyMember))
		{
			if (!CurrentPartyMember->bHasAnnouncedJoin)
			{
				// Both local and remote players will announce joins
				UParty* Party = GetPartyOuter();
				check(Party);
				Party->OnPartyMemberJoined().Broadcast(this, UniqueId);
				CurrentPartyMember->bHasAnnouncedJoin = true;
			}

			check(CurrentPartyMember->MemberStateRefDef && CurrentPartyMember->MemberStateRefScratch);

			if (FVariantDataConverter::VariantMapToUStruct(InPartyMemberData->GetKeyValAttrs(), CurrentPartyMember->MemberStateRefDef, CurrentPartyMember->MemberStateRef, 0, CPF_Transient | CPF_RepSkip))
			{
				// Broadcast property changes
				CurrentPartyMember->ComparePartyMemberData(*CurrentPartyMember->MemberStateRefScratch);
				// Copy out the old data
				ensure(CurrentPartyMember->MemberStateRefDef->GetCppStructOps()->Copy(CurrentPartyMember->MemberStateRefScratch, CurrentPartyMember->MemberStateRef, 1));
				OnPartyMemberDataChanged().Broadcast(CurrentPartyMember->UniqueId, CurrentPartyMember);
			}
			else
			{
				UE_LOG(LogParty, Warning, TEXT("[%s] Failed to serialize party member data!"), *InMemberId.ToString());
				ensure(CurrentPartyMember->MemberStateRefDef->GetCppStructOps()->Copy(CurrentPartyMember->MemberStateRef, CurrentPartyMember->MemberStateRefScratch, 1));
			}
		}
	}
}

void UPartyGameState::HandleLeavingParty()
{
	// Stop listening for delegates as we leave
	UnregisterFrontendDelegates();
}

void UPartyGameState::HandleRemovedFromParty(EMemberExitedReason Reason)
{
	// Trigger delegate first
	UParty* Party = GetPartyOuter();
	if (ensure(Party))
	{
		Party->OnPartyLeft().Broadcast(this, Reason);
	}

	// Cleanup
	OnShutdown();
}

void UPartyGameState::HandleLockoutPromotionStateChange(bool bNewLockoutState)
{
	bPromotionLockoutState = bNewLockoutState;
}

EApprovalAction UPartyGameState::ProcessJoinRequest(const FUniqueNetId& RecipientId, const FUniqueNetId& SenderId, EJoinPartyDenialReason& DenialReason) const
{
	if (IsInJoinableGameState())
	{
		return EApprovalAction::Approve;
	}
	else
	{
		DenialReason = EJoinPartyDenialReason::GameFull;
		return EApprovalAction::Deny;
	}
}

void UPartyGameState::HandlePartyJoinRequestReceived(const FUniqueNetId& RecipientId, const FUniqueNetId& SenderId)
{
#if 0 // auto approve requests for debugging error case
	UWorld* World = GetWorld();
	IOnlinePartyPtr PartyInt = Online::GetPartyInterface(World);
	if (PartyInt.IsValid())
	{
		PartyInt->ApproveJoinRequest(RecipientId, *Party->PartyId, SenderId, true);
	}
	return;
#endif

	EApprovalAction ApprovalAction = EApprovalAction::Deny;
	EJoinPartyDenialReason DenialReason = EJoinPartyDenialReason::Busy;

	if (IsLocalPartyLeader())
	{
		int32 NumPartyMembers = GetPartySize();
		int32 MaxPartyMembers = CurrentConfig.MaxMembers;
		if (NumPartyMembers < MaxPartyMembers)
		{
			// Give the game a chance to accept or deny this player
			ApprovalAction = ProcessJoinRequest(RecipientId, SenderId, DenialReason);
		}
		else
		{
			DenialReason = EJoinPartyDenialReason::PartyFull;
		}
	}
	else
	{
		// Party leader has changed
		DenialReason = EJoinPartyDenialReason::NotPartyLeader;
	}

	if (ApprovalAction == EApprovalAction::Enqueue ||
		ApprovalAction == EApprovalAction::EnqueueAndStartBeacon)
	{
		// Enqueue for a more opportune time
		UE_LOG(LogParty, Verbose, TEXT("[%s] Enqueuing approval request for %s"), OssParty.IsValid() ? *OssParty->PartyId->ToString() : TEXT("INVALID"), *SenderId.ToString());
		
		FPendingMemberApproval PendingApproval;
		PendingApproval.RecipientId.SetUniqueNetId(RecipientId.AsShared());
		PendingApproval.SenderId.SetUniqueNetId(SenderId.AsShared());
		PendingApprovals.Enqueue(PendingApproval);

		if (ReservationBeaconClient == nullptr &&
			ApprovalAction == EApprovalAction::EnqueueAndStartBeacon)
		{
			ConnectToReservationBeacon();
		}
	}
	else
	{
		bool bApproveRequest = (ApprovalAction == EApprovalAction::Approve);
		if (bApproveRequest)
		{
			DenialReason = EJoinPartyDenialReason::NoReason;
		}

		// Respond now
		UE_LOG(LogParty, Verbose, TEXT("[%s] Responding to approval request for %s with %s"), OssParty.IsValid() ? *OssParty->PartyId->ToString() : TEXT("INVALID"), *SenderId.ToString(), bApproveRequest ? TEXT("approved") : TEXT("denied"));
		
		UWorld* World = GetWorld();
		IOnlinePartyPtr PartyInt = Online::GetPartyInterface(World);
		if (PartyInt.IsValid() && OssParty.IsValid())
		{
			PartyInt->ApproveJoinRequest(RecipientId, *OssParty->PartyId, SenderId, bApproveRequest, (int32)DenialReason);
		}
	}
}

void UPartyGameState::HandlePartyQueryJoinabilityRequestReceived(const FUniqueNetId& RecipientId, const FUniqueNetId& SenderId)
{
	EApprovalAction ApprovalAction = EApprovalAction::Deny;
	EJoinPartyDenialReason DenialReason = EJoinPartyDenialReason::Busy;

	if (IsLocalPartyLeader())
	{
		int32 NumPartyMembers = GetPartySize();
		int32 MaxPartyMembers = CurrentConfig.MaxMembers;
		if (NumPartyMembers < MaxPartyMembers)
		{
			// Give the game a chance to accept or deny this player
			ApprovalAction = ProcessJoinRequest(RecipientId, SenderId, DenialReason);
		}
		else
		{
			DenialReason = EJoinPartyDenialReason::PartyFull;
		}
	}
	else
	{
		// Party leader has changed
		DenialReason = EJoinPartyDenialReason::NotPartyLeader;
	}

	bool bApproveRequest = ApprovalAction == EApprovalAction::Approve ||
		ApprovalAction == EApprovalAction::Enqueue ||
		ApprovalAction == EApprovalAction::EnqueueAndStartBeacon;
	if (bApproveRequest)
	{
		DenialReason = EJoinPartyDenialReason::NoReason;
	}

	// Respond now
	UE_LOG(LogParty, Verbose, TEXT("[%s] Responding to approval request for %s with %s"), OssParty.IsValid() ? *OssParty->PartyId->ToString() : TEXT("INVALID"), *SenderId.ToString(), bApproveRequest ? TEXT("approved") : TEXT("denied"));

	UWorld* World = GetWorld();
	IOnlinePartyPtr PartyInt = Online::GetPartyInterface(World);
	if (PartyInt.IsValid() && OssParty.IsValid())
	{
		PartyInt->RespondToQueryJoinability(RecipientId, *OssParty->PartyId, SenderId, bApproveRequest, (int32)DenialReason);
	}
}

FOnlinePartyTypeId UPartyGameState::GetPartyTypeId() const
{
	FOnlinePartyTypeId Result;

	if (ensure(OssParty.IsValid()))
	{
		Result = OssParty->PartyTypeId;
	}

	return Result;
}

TSharedPtr<const FOnlinePartyId> UPartyGameState::GetPartyId() const
{
	if (ensure(OssParty.IsValid()))
	{
		return OssParty->PartyId;
	}

	return nullptr;
}

void UPartyGameState::SetPartyType(EPartyType InPartyType, bool bLeaderFriendsOnly, bool bLeaderInvitesOnly)
{
	if (IsLocalPartyLeader())
	{
		check(PartyStateRef);
		if (PartyStateRef->PartyType != InPartyType ||
			PartyStateRef->bLeaderFriendsOnly != bLeaderFriendsOnly ||
			PartyStateRef->bLeaderInvitesOnly != bLeaderInvitesOnly)
		{
			bool bIsPrivate = (InPartyType == EPartyType::Private);

			PartySystemPermissions::EPresencePermissions PresencePermissions;
			if (bLeaderFriendsOnly)
			{
				if (bIsPrivate)
				{
					PresencePermissions = PartySystemPermissions::FriendsInviteOnly;
				}
				else
				{
					PresencePermissions = PartySystemPermissions::FriendsOnly;
				}
			}
			else
			{
				if (bIsPrivate)
				{
					PresencePermissions = PartySystemPermissions::PublicInviteOnly;
				}
				else
				{
					PresencePermissions = PartySystemPermissions::Public;
				}
			}

			CurrentConfig.PresencePermissions = PresencePermissions;
			if (PartyStateRef->bInvitesDisabled)
			{
				CurrentConfig.InvitePermissions = PartySystemPermissions::EInvitePermissions::Noone;
			}
			else
			{
				CurrentConfig.InvitePermissions = bLeaderInvitesOnly ? PartySystemPermissions::EInvitePermissions::Leader : PartySystemPermissions::EInvitePermissions::Anyone;
			}

			UpdatePartyConfig(bIsPrivate);

			EPartyType OldPartyType = PartyStateRef->PartyType;
			bool bOldLeaderFriendsOnly = PartyStateRef->bLeaderFriendsOnly;
			bool bOldLeaderInvitesOnly = PartyStateRef->bLeaderInvitesOnly;

			// Replicate the party settings to other party members
			PartyStateRef->PartyType = InPartyType;
			PartyStateRef->bLeaderFriendsOnly = bLeaderFriendsOnly;
			PartyStateRef->bLeaderInvitesOnly = bLeaderInvitesOnly;
			UpdatePartyData(OwningUserId);

			// Refresh accepting members, taking everything into account
			UpdateAcceptingMembers();

			// Notify the local player of the changes
			if (OldPartyType != InPartyType)
			{
				OnPartyTypeChanged().Broadcast(InPartyType);
			}

			if (bOldLeaderFriendsOnly != bLeaderFriendsOnly)
			{
				OnLeaderFriendsOnlyChanged().Broadcast(bLeaderFriendsOnly);
			}

			if (bOldLeaderInvitesOnly != bLeaderInvitesOnly)
			{
				OnLeaderInvitesOnlyChanged().Broadcast(bLeaderInvitesOnly);
			}
		}
	}
	else
	{
		UE_LOG(LogParty, Warning, TEXT("Non party leader trying to set party permissions!"));
	}
}

void UPartyGameState::SetInvitesDisabled(bool bInvitesDisabled)
{
	if (IsLocalPartyLeader())
	{
		check(PartyStateRef);
		if (PartyStateRef->bInvitesDisabled != bInvitesDisabled)
		{
			if (bInvitesDisabled)
			{
				CurrentConfig.InvitePermissions = PartySystemPermissions::EInvitePermissions::Noone;
			}
			else
			{
				CurrentConfig.InvitePermissions = PartyStateRef->bLeaderInvitesOnly ? PartySystemPermissions::EInvitePermissions::Leader : PartySystemPermissions::EInvitePermissions::Anyone;
			}

			UpdatePartyConfig();

			// Replicate the party settings to other party members
			PartyStateRef->bInvitesDisabled = bInvitesDisabled;
			UpdatePartyData(OwningUserId);

			// Refresh accepting members, taking everything into account
			UpdateAcceptingMembers();

			OnInvitesDisabledChanged().Broadcast(bInvitesDisabled);
		}
	}
	else
	{
		UE_LOG(LogParty, Verbose, TEXT("Non party leader trying to set invites disabled!"));
	}
}

void UPartyGameState::StayWithPartyOnExit(bool bInStayWithParty)
{
	bStayWithPartyOnDisconnect = bInStayWithParty;
}

bool UPartyGameState::ShouldStayWithPartyOnExit() const
{
	return bStayWithPartyOnDisconnect;
}

int32 UPartyGameState::GetPartySize() const
{
	return PartyMembersState.Num();
}

void UPartyGameState::SetPartyMaxSize(int32 NewSize)
{
	if (ensure(OssParty.IsValid()))
	{
		if (CurrentConfig.MaxMembers != NewSize)
		{
			UParty* Party = GetPartyOuter();
			CurrentConfig.MaxMembers = FMath::Clamp(NewSize, 1, Party->GetDefaultPartyMaxSize());
			UpdatePartyConfig();
		}
	}
	else
	{
		UE_LOG(LogParty, Warning, TEXT("Invalid party updating party size!"));
	}
}

int32 UPartyGameState::GetPartyMaxSize() const
{
	if (ensure(OssParty.IsValid()))
	{
		return OssParty->Config->MaxMembers;
	}
	else
	{
		UE_LOG(LogParty, Warning, TEXT("Invalid party getting party size!"));
	}

	// Invalid party info
	return INDEX_NONE;
}

bool UPartyGameState::IsPartyFull() const
{
	return GetPartySize() >= GetPartyMaxSize();
}

void UPartyGameState::UpdateAcceptingMembers()
{
	if (IsLocalPartyLeader())
	{
		EJoinPartyDenialReason DenialReason = EJoinPartyDenialReason::NoReason;
		bool bCurrentlyAcceptingMembers = false;

		// Look at game joinability (in game with permission or no game at all)
		if (IsInJoinableGameState())
		{
			// Make sure the party isn't full
			int32 NumPartyMembers = GetPartySize();
			int32 MaxPartyMembers = CurrentConfig.MaxMembers;
			if (NumPartyMembers < MaxPartyMembers)
			{
				// Look at party joinability
				switch (PartyStateRef->PartyType)
				{
					case EPartyType::Public:
					case EPartyType::FriendsOnly:
						bCurrentlyAcceptingMembers = true;
						break;
					case EPartyType::Private:
						// Party is private, invite required
						DenialReason = EJoinPartyDenialReason::PartyPrivate;
					default:
						break;
				}
			}
			else
			{
				// Party is full
				DenialReason = EJoinPartyDenialReason::PartyFull;
			}
		}
		else
		{
			DenialReason = EJoinPartyDenialReason::GameFull;
		}

		SetAcceptingMembers(bCurrentlyAcceptingMembers, DenialReason);
	}
	else
	{
		UE_LOG(LogParty, Warning, TEXT("Non party leader trying to update accepting members!"));
	}
}

void UPartyGameState::SetAcceptingMembers(bool bIsAcceptingMembers, EJoinPartyDenialReason DenialReason)
{
	if (IsLocalPartyLeader())
	{
		ensure(OssParty.IsValid());

		int32 NumPartyMembers = GetPartySize();
		int32 MaxPartyMembers = CurrentConfig.MaxMembers;
		bool bIsRoomInParty = (NumPartyMembers < MaxPartyMembers);

		bool bCanAcceptMembers = bIsAcceptingMembers && bIsRoomInParty;

		int32 NewDenialReason = (int32)(bCanAcceptMembers ? EJoinPartyDenialReason::NoReason : DenialReason);
		if (CurrentConfig.bIsAcceptingMembers != bCanAcceptMembers || CurrentConfig.NotAcceptingMembersReason != NewDenialReason)
		{
			bDebugAcceptingMembers = bCanAcceptMembers;
			CurrentConfig.bIsAcceptingMembers = bDebugAcceptingMembers;
			CurrentConfig.NotAcceptingMembersReason = NewDenialReason;
			UpdatePartyConfig();
		}
	}
	else
	{
		UE_LOG(LogParty, Warning, TEXT("Non party leader trying to set accepting members!"));
	}
}

bool UPartyGameState::IsAcceptingMembers(EJoinPartyDenialReason* const DenialReason /*= nullptr*/) const
{
	if (CurrentConfig.bIsAcceptingMembers)
	{
		// Accepting members
		if (DenialReason != nullptr)
		{
			*DenialReason = EJoinPartyDenialReason::NoReason;
		}
		return true;
	}
	else
	{
		// Not accepting members
		if (DenialReason != nullptr)
		{
			*DenialReason = static_cast<EJoinPartyDenialReason>(CurrentConfig.NotAcceptingMembersReason);
		}
		return false;
	}
}

bool UPartyGameState::IsInJoinableGameState() const
{
	bool bInGame = false;
	bool bGameJoinable = false;

	UWorld* World = GetWorld();
	IOnlineSessionPtr SessionInt = Online::GetSessionInterface(World);
	if (ensure(SessionInt.IsValid()))
	{
		bool bGamePublicJoinable = false;
		bool bGameFriendJoinable = false;
		bool bGameInviteOnly = false;
		bool bGameAllowInvites = false;

		FNamedOnlineSession* GameSession = SessionInt->GetNamedSession(NAME_GameSession);
		if (GameSession != NULL &&
			GameSession->GetJoinability(bGamePublicJoinable, bGameFriendJoinable, bGameInviteOnly, bGameAllowInvites))
		{
			bInGame = true;

			TSharedPtr<FOnlineSessionInfo> UserSessionInfo = GameSession->SessionInfo;
			if (UserSessionInfo.IsValid())
			{
				// User's game is joinable in some way if any of this is true
				bGameJoinable = bGamePublicJoinable || bGameFriendJoinable || bGameInviteOnly;
			}
		}
	}

	return !bInGame || (bInGame && bGameJoinable);
}

bool UPartyGameState::CanInvite() const
{
	if (ensure(OssParty.IsValid()))
	{
		return OssParty->CanLocalUserInvite(*OwningUserId);
	}
	return false;
}

void UPartyGameState::UpdatePartyConfig(bool bResetAccessKey)
{
	if (IsLocalPartyLeader())
	{
		ensure(OssParty.IsValid());

		UWorld* World = GetWorld();
		check(World);
		IOnlinePartyPtr PartyInt = Online::GetPartyInterface(World);
		if (ensure(PartyInt.IsValid()))
		{
			FOnUpdatePartyComplete CompletionDelegate;
			CompletionDelegate.BindUObject(this, &ThisClass::OnUpdatePartyConfigComplete);
			if (!PartyInt->UpdateParty(*OwningUserId, *OssParty->PartyId, CurrentConfig, bResetAccessKey, CompletionDelegate))
			{
				UE_LOG(LogParty, Warning, TEXT("[%s] Failed to update party"), *OssParty->PartyId->ToString());
			}
		}
		else
		{
			UE_LOG(LogParty, Warning, TEXT("[%s] Invalid party interface updating party size"), *OssParty->PartyId->ToString());
		}
	}
	else
	{
		UE_LOG(LogParty, Warning, TEXT("Non party leader trying to update party config!"));
	}
}

void UPartyGameState::OnUpdatePartyConfigComplete(const FUniqueNetId& LocalUserId, const FOnlinePartyId& InPartyId, const EUpdateConfigCompletionResult Result)
{
	const FString PartyIdDebugString = InPartyId.ToDebugString();
	UE_LOG(LogParty, Verbose, TEXT("[%s] Party config updated %s"), *PartyIdDebugString, ToString(Result));

	if (ensure(OssParty.IsValid()))
	{
		CurrentConfig = *OssParty->Config;
		bDebugAcceptingMembers = CurrentConfig.bIsAcceptingMembers;

		OnPartyConfigurationChanged().Broadcast(CurrentConfig);
	}
}

bool UPartyGameState::IsLocalPartyLeader()
{
	if (ensure(OwningUserId.IsValid()))
	{
		TSharedPtr<const FUniqueNetId> LeaderId = GetPartyLeader();
		if (LeaderId.IsValid())
		{
			if (*OwningUserId == *LeaderId)
			{
				return true;
			}
		}
		else
		{
			UE_LOG(LogParty, Warning, TEXT("Unable to determine party leader!"));
		}
	}
	else
	{
		UE_LOG(LogParty, Warning, TEXT("Invalid owning user id trying to determine party leader!"));
	}

	return false;
}

TSharedPtr<const FUniqueNetId> UPartyGameState::GetPartyLeader() const
{
	TSharedPtr<const FUniqueNetId> ReturnValue = nullptr;
	if (ensure(OssParty.IsValid()))
	{
		ReturnValue = OssParty->LeaderId;
	}

	return ReturnValue;
}

UPartyMemberState* UPartyGameState::GetPartyMember(const FUniqueNetIdRepl& InUniqueId) const
{
	if (InUniqueId.IsValid())
	{
		UPartyMemberState* const* PartyMemberPtr = PartyMembersState.Find(InUniqueId);
		return PartyMemberPtr ? *PartyMemberPtr : nullptr;
	}
	
	return nullptr;
}

void UPartyGameState::GetAllPartyMembers(TArray<UPartyMemberState*>& PartyMembers) const
{
	PartyMembersState.GenerateValueArray(PartyMembers);
}

FChatRoomId UPartyGameState::GetChatRoomID() const
{
	ensure(OssParty.IsValid());
	return OssParty->RoomId;
}

void UPartyGameState::OnPartyMemberPromoted(const FUniqueNetId& LocalUserId, const FOnlinePartyId& InPartyId, const FUniqueNetId& InMemberId, const EPromoteMemberCompletionResult Result)
{
	FString PartyIdDebugString = InPartyId.ToDebugString();
	FString MemberIdDebugString = InMemberId.ToDebugString();
	UE_LOG(LogParty, Verbose, TEXT("[%s] Player %s promoted %s %s"), *PartyIdDebugString, *LocalUserId.ToString(), *MemberIdDebugString, ToString(Result));
}

void UPartyGameState::PromoteMember(const FUniqueNetIdRepl& NewPartyLeader)
{
	if (IsLocalPartyLeader())
	{
		ensure(OssParty.IsValid());
		if (ensure(NewPartyLeader.IsValid()))
		{
			if (!bPromotionLockoutState)
			{
				UWorld* World = GetWorld();
				check(World);
				IOnlinePartyPtr PartyInt = Online::GetPartyInterface(World);
				if (ensure(PartyInt.IsValid()))
				{
					// Do any internal updates
					PrePromoteMember();

					FOnPromotePartyMemberComplete CompletionDelegate;
					CompletionDelegate.BindUObject(this, &ThisClass::OnPartyMemberPromoted);
					PartyInt->PromoteMember(*OwningUserId, *OssParty->PartyId, *NewPartyLeader, CompletionDelegate);
				}
			}
			else
			{
				UE_LOG(LogParty, Verbose, TEXT("[%s] Promote member feature locked out."), *OssParty->PartyId->ToString());
			}
		}
		else
		{
			UE_LOG(LogParty, Warning, TEXT("Trying to promote invalid party member to leader!"));
		}
	}
	else
	{
		UE_LOG(LogParty, Warning, TEXT("Non party leader trying to promote party leader!"));
	}
}

void UPartyGameState::PrePromoteMember()
{
}

void UPartyGameState::OnPartyMemberKicked(const FUniqueNetId& LocalUserId, const FOnlinePartyId& InPartyId, const FUniqueNetId& InMemberId, const EKickMemberCompletionResult Result)
{
	FString PartyIdDebugString = InPartyId.ToDebugString();
	FString MemberIdDebugString = InMemberId.ToDebugString();
	UE_LOG(LogParty, Verbose, TEXT("[%s] Player %s kicked %s %s"), *PartyIdDebugString, *LocalUserId.ToString(), *MemberIdDebugString, ToString(Result));
}

void UPartyGameState::KickMember(const FUniqueNetIdRepl& PartyMemberToKick)
{
	if (IsLocalPartyLeader())
	{
		ensure(OssParty.IsValid());
		if (ensure(PartyMemberToKick.IsValid()))
		{
			UPartyMemberState** LocalPartyMemberStatePtr = PartyMembersState.Find(PartyMemberToKick);
			if (LocalPartyMemberStatePtr && *LocalPartyMemberStatePtr)
			{
				UWorld* World = GetWorld();
				check(World);
				IOnlinePartyPtr PartyInt = Online::GetPartyInterface(World);
				if (ensure(PartyInt.IsValid()))
				{
					FOnKickPartyMemberComplete CompletionDelegate;
					CompletionDelegate.BindUObject(this, &ThisClass::OnPartyMemberKicked);
					PartyInt->KickMember(*OwningUserId, *OssParty->PartyId, *PartyMemberToKick, CompletionDelegate);
				}
			}
			else
			{
				UE_LOG(LogParty, Warning, TEXT("Trying to kick player that is not in your party!"));
			}
		}
		else
		{
			UE_LOG(LogParty, Warning, TEXT("Trying to kick invalid party member!"));
		}
	}
	else
	{
		UE_LOG(LogParty, Warning, TEXT("Non party leader trying to kick party member!"));
	}
}

UPartyMemberState* UPartyGameState::InitPartyMemberStateFromLocalPlayer(const ULocalPlayer* LocalPlayer)
{
	UPartyMemberState* LocalPartyMemberState = nullptr;
	TSharedPtr<const FUniqueNetId> UniqueNetId = LocalPlayer->GetPreferredUniqueNetId();
	if (UniqueNetId.IsValid())
	{
		UPartyMemberState** LocalPartyMemberStatePtr = PartyMembersState.Find(UniqueNetId);
		LocalPartyMemberState = LocalPartyMemberStatePtr ? *LocalPartyMemberStatePtr : nullptr;
		if (!LocalPartyMemberState)
		{
			LocalPartyMemberState = CreateNewPartyMember(*UniqueNetId);
			if (LocalPartyMemberState)
			{
				PartyMembersState.Add(UniqueNetId, LocalPartyMemberState);
			}
		}
	}
	return LocalPartyMemberState;
}

void UPartyGameState::SendLocalPlayerPartyData()
{
	UWorld* World = GetWorld();
	check(World);

	for (FLocalPlayerIterator It(GEngine, World); It; ++It)
	{
		ULocalPlayer* LP = *It;
		if (LP)
		{
			const UPartyMemberState* MemberStatePtr = InitPartyMemberStateFromLocalPlayer(LP);
			if (MemberStatePtr != nullptr)
			{
				UpdatePartyMemberState(OwningUserId, MemberStatePtr);
			}
		}
	}
}

void UPartyGameState::UpdatePartyData(const FUniqueNetIdRepl& InLocalUserId)
{
	if (IsLocalPartyLeader())
	{
		ensure(OwningUserId == InLocalUserId);

		TSharedPtr<const FOnlinePartyId> PartyId = GetPartyId();
		if (PartyId.IsValid())
		{
			UWorld* World = GetWorld();
			check(World);

			IOnlinePartyPtr PartyInt = Online::GetPartyInterface(World);
			if (ensure(PartyInt.IsValid()))
			{
				FOnlinePartyData PartyData;
				ensure(PartyStateRefDef != nullptr && PartyStateRef != nullptr);
				if (FVariantDataConverter::UStructToVariantMap(PartyStateRefDef, PartyStateRef, PartyData.GetKeyValAttrs(), 0, CPF_Transient | CPF_RepSkip))
				{
					PartyInt->UpdatePartyData(*OwningUserId, *PartyId, PartyData);
				}
				else
				{
					UE_LOG(LogParty, Warning, TEXT("UpdatePartyData: Failed to update party data!"));
				}
			}
			else
			{
				UE_LOG(LogParty, Warning, TEXT("UpdatePartyData: Invalid party interface!"));
			}
		}
		else
		{
			UE_LOG(LogParty, Warning, TEXT("UpdatePartyData: Invalid internal party!"));
		}
	}
	else
	{
		UE_LOG(LogParty, Warning, TEXT("Non party leader trying to update party state!"));
	}
}

void UPartyGameState::UpdatePartyMemberState(const FUniqueNetIdRepl& InLocalUserId, const UPartyMemberState* InPartyMemberState)
{
	if (ensure(InLocalUserId.IsValid()))
	{
		if (ensure(InPartyMemberState))
		{
			TSharedPtr<const FOnlinePartyId> PartyId = GetPartyId();
			if (PartyId.IsValid())
			{
				UWorld* World = GetWorld();
				check(World);

				IOnlinePartyPtr PartyInt = Online::GetPartyInterface(World);
				if (ensure(PartyInt.IsValid()))
				{
					FOnlinePartyData PartyMemberData;
					ensure(InPartyMemberState->MemberStateRefDef != nullptr && InPartyMemberState->MemberStateRef != nullptr);
					if (FVariantDataConverter::UStructToVariantMap(InPartyMemberState->MemberStateRefDef, InPartyMemberState->MemberStateRef, PartyMemberData.GetKeyValAttrs(), 0, CPF_Transient | CPF_RepSkip))
					{
						PartyInt->UpdatePartyMemberData(*InLocalUserId, *PartyId, PartyMemberData);
					}
					else
					{
						UE_LOG(LogParty, Warning, TEXT("UpdatePartyMemberState: Failed to update party member data!"));
					}
				}
				else
				{
					UE_LOG(LogParty, Warning, TEXT("UpdatePartyMemberState: Invalid party interface!"));
				}
			}
			else
			{
				UE_LOG(LogParty, Warning, TEXT("UpdatePartyMemberState: Invalid internal party!"));
			}
		}
		else
		{
			UE_LOG(LogParty, Warning, TEXT("UpdatePartyMemberState: NULL party member state!"));
		}
	}
	else
	{
		UE_LOG(LogParty, Warning, TEXT("UpdatePartyMemberState: Invalid local user!"));
	}
}

void UPartyGameState::GetSessionInfo(FName SessionName, FString& URL, FString& SessionId) const
{
	UWorld* World = GetWorld();
	check(World);

	IOnlineSessionPtr SessionInt = Online::GetSessionInterface(World);
	if (ensure(SessionInt.IsValid()))
	{
		ensure(SessionInt->GetResolvedConnectString(SessionName, URL, NAME_BeaconPort));

		FNamedOnlineSession* Session = SessionInt->GetNamedSession(SessionName);
		if (Session)
		{
			SessionId = Session->GetSessionIdStr();
		}
	}
}

void UPartyGameState::ConnectToReservationBeacon()
{
	if (IsLocalPartyLeader())
	{
		FPendingMemberApproval NextApproval;
		if (PendingApprovals.Peek(NextApproval))
		{
			bool bStartedConnection = false;

			UWorld* World = GetWorld();
			check(World);

			// Reconnect to the reservation beacon to maintain our place in the game (just until actual joined, holds place for all party members)
			ReservationBeaconClient = World->SpawnActor<APartyBeaconClient>(ReservationBeaconClientClass);
			if (ReservationBeaconClient)
			{
				UE_LOG(LogParty, Verbose, TEXT("Created party reservation beacon %s."), *ReservationBeaconClient->GetName());

				ReservationBeaconClient->OnHostConnectionFailure().BindUObject(this, &ThisClass::OnReservationBeaconUpdateConnectionFailure);
				ReservationBeaconClient->OnReservationRequestComplete().BindUObject(this, &ThisClass::OnReservationBeaconUpdateResponseReceived);
				ReservationBeaconClient->OnReservationCountUpdate().BindUObject(this, &ThisClass::OnReservationCountUpdate);

				FPlayerReservation NewPlayerRes;
				NewPlayerRes.UniqueId = NextApproval.SenderId;

				TArray<FPlayerReservation> PlayersToAdd;
				PlayersToAdd.Add(NewPlayerRes);

				FUniqueNetIdRepl PartyLeader(GetPartyLeader());

				UParty* Party = GetPartyOuter();
				check(Party);
				FName SessionName = Party->GetPlayerSessionName();

				FString URL;
				FString SessionId;
				GetSessionInfo(SessionName, URL, SessionId);

				if (!URL.IsEmpty() && !SessionId.IsEmpty())
				{
					bStartedConnection = ReservationBeaconClient->RequestReservationUpdate(URL, SessionId, PartyLeader, PlayersToAdd);
				}
				else
				{
					UE_LOG(LogParty, Warning, TEXT("UPartyGameState::ConnectToReservationBeacon: URL ('%s') or SessionId ('%s') is empty"), *URL, *SessionId);
				}
			}
			else
			{
				UE_LOG(LogParty, Warning, TEXT("UPartyGameState::ConnectToReservationBeacon: Failed to spawn APartyBeaconClient"));
			}
			if (!bStartedConnection)
			{
				OnReservationBeaconUpdateConnectionFailure();
			}
		}
	}
	else
	{
		UE_LOG(LogParty, Warning, TEXT("ConnectToReservationBeacon: Non party leader trying to connect to reservation beacon!"));
	}
}

void UPartyGameState::RejectAllPendingJoinRequests()
{
	UWorld* World = GetWorld();
	check(World);
	IOnlinePartyPtr PartyInt = Online::GetPartyInterface(World);
	TSharedPtr<const FOnlinePartyId> PartyId = GetPartyId();

	bool bValidInterface = PartyInt.IsValid() && PartyId.IsValid();

	bool bEmpty = false;
	FPendingMemberApproval PendingApproval;

	do
	{
		bEmpty = !PendingApprovals.Dequeue(PendingApproval);
		if (!bEmpty && bValidInterface)
		{
			UE_LOG(LogParty, Verbose, TEXT("[%s] Responding to approval request for %s with denied"), *PartyId->ToString(), *PendingApproval.SenderId.ToString());
			PartyInt->ApproveJoinRequest(*PendingApproval.RecipientId, *PartyId, *PendingApproval.SenderId, false, (int32)EJoinPartyDenialReason::Busy);
		}
	} while (!bEmpty);
}

void UPartyGameState::OnReservationBeaconUpdateConnectionFailure()
{
	UE_LOG(LogParty, Verbose, TEXT("Reservation update beacon failure %s."), ReservationBeaconClient ? *ReservationBeaconClient->GetName() : *FString());

	// empty the queue, denying all requests
	RejectAllPendingJoinRequests();
	CleanupReservationBeacon();
}

void UPartyGameState::OnReservationBeaconUpdateResponseReceived(EPartyReservationResult::Type ReservationResponse)
{
	UE_LOG(LogParty, Verbose, TEXT("OnReservationBeaconUpdateResponseReceived %s"), EPartyReservationResult::ToString(ReservationResponse));

	if (ReservationResponse == EPartyReservationResult::ReservationAccepted ||
		ReservationResponse == EPartyReservationResult::ReservationDuplicate)
	{
		UWorld* World = GetWorld();
		check(World);

		IOnlinePartyPtr PartyInt = Online::GetPartyInterface(World);
		TSharedPtr<const FOnlinePartyId> PartyId = GetPartyId();

		bool bValidInterface = PartyInt.IsValid() && PartyId.IsValid();

		// There should be at least the one
		FPendingMemberApproval PendingApproval;
		if (ensure(PendingApprovals.Dequeue(PendingApproval)))
		{
			if (bValidInterface)
			{
				UE_LOG(LogParty, Verbose, TEXT("[%s] Responding to approval request for %s with approved"), *PartyId->ToString(), *PendingApproval.SenderId.ToString());
				PartyInt->ApproveJoinRequest(*PendingApproval.RecipientId, *PartyId, *PendingApproval.SenderId, true);
			}
		}

		// Check if there are any more while we are connected
		FPendingMemberApproval NextApproval;
		if (PendingApprovals.Peek(NextApproval))
		{
			if (ensure(ReservationBeaconClient))
			{
				FUniqueNetIdRepl PartyLeader(GetPartyLeader());

				FPlayerReservation NewPlayerRes;
				NewPlayerRes.UniqueId = NextApproval.SenderId;

				TArray<FPlayerReservation> PlayersToAdd;
				PlayersToAdd.Add(NewPlayerRes);

				ReservationBeaconClient->RequestReservationUpdate(PartyLeader, PlayersToAdd);
			}
			else
			{
				UE_LOG(LogParty, Warning, TEXT("UPartyGameState::OnReservationBeaconUpdateResponseReceived: ReservationBeaconClient is null while trying to process more requests"));
				RejectAllPendingJoinRequests();
			}
		}
		else
		{
			CleanupReservationBeacon();
		}
	}
	else
	{
		// empty the queue, denying all requests
		RejectAllPendingJoinRequests();
		CleanupReservationBeacon();
	}
}

void UPartyGameState::OnReservationCountUpdate(int32 NumRemaining)
{
}

void UPartyGameState::CleanupReservationBeacon()
{
	if (ReservationBeaconClient)
	{
		UE_LOG(LogParty, Verbose, TEXT("Party reservation beacon cleanup while in state %s, pending approvals: %s"), ToString(ReservationBeaconClient->GetConnectionState()), !PendingApprovals.IsEmpty() ? TEXT("true") : TEXT("false"));

		ReservationBeaconClient->OnHostConnectionFailure().Unbind();
		ReservationBeaconClient->OnReservationRequestComplete().Unbind();
		ReservationBeaconClient->OnReservationCountUpdate().Unbind();
		ReservationBeaconClient->DestroyBeacon();
		ReservationBeaconClient = nullptr;
	}
}

UWorld* UPartyGameState::GetWorld() const
{
	UParty* Party = GetPartyOuter();
	if (Party)
	{
		return Party->GetWorld();
	}
	
	return nullptr;
}

UParty* UPartyGameState::GetPartyOuter() const
{
	return GetTypedOuter<UParty>();
}
