// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Party.h"
#include "TimerManager.h"
#include "Engine/GameInstance.h"
#include "PartyModule.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/PlayerController.h"

#include "OnlineSubsystemUtils.h"

#include "PartyMemberState.h"

#define LOCTEXT_NAMESPACE "Parties"

namespace PartyConsoleVariables
{
	// CVars
	TAutoConsoleVariable<int32> CVarPartyEnableAutoRejoin(
		TEXT("Party.CVarEnableAutoRejoin"),
		1,
		TEXT("Enable automatic rejoining of parties\n")
		TEXT("1 Enables. 0 disables."),
		ECVF_Default);
}

UParty::UParty(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer),
	bLeavingPersistentParty(false)
{
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
	}
}

void UParty::Init()
{
	UGameInstance* GameInstance = GetGameInstance();
	check(GameInstance);
	GameInstance->OnNotifyPreClientTravel().AddUObject(this, &ThisClass::NotifyPreClientTravel);

	FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &ThisClass::OnPostLoadMap);
}

void UParty::InitPIE()
{
	OnPostLoadMap();
}

void UParty::OnPostLoadMap(UWorld*)
{
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		RegisterIdentityDelegates();
		RegisterPartyDelegates();
	}
}

void UParty::OnShutdown()
{
	for (const auto& PartyKeyValue : JoinedParties)
	{
		UPartyGameState* Party = PartyKeyValue.Value;
		if (Party)
		{
			Party->OnShutdown();
		}
	}

	JoinedParties.Empty();

	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		FCoreUObjectDelegates::PostLoadMapWithWorld.RemoveAll(this);

		UnregisterIdentityDelegates();
		UnregisterPartyDelegates();
	}

	UGameInstance* GameInstance = GetGameInstance();
	if (GameInstance)
	{
		GameInstance->OnNotifyPreClientTravel().RemoveAll(this);
	}
}

void UParty::OnLogoutComplete(int32 LocalUserNum, bool bWasSuccessful)
{
	if (JoinedParties.Num())
	{
		UE_LOG(LogParty, Log, TEXT("OnLogoutComplete: Party cleanup on logout"));
		TArray<FOnlinePartyTypeId> PartiesToRemove;
		for (const auto& PartyKeyValue : JoinedParties)
		{
			PartiesToRemove.Add(PartyKeyValue.Key);
		}
		for (const auto& PartyKey : PartiesToRemove)
		{
			UPartyGameState** FoundParty = JoinedParties.Find(PartyKey);
			if (FoundParty)
			{
				UPartyGameState* Party = *FoundParty;
				if (Party && Party->GetPartyId().IsValid())
				{
					if (Party->GetPartyId().IsValid())
					{
						UE_LOG(LogParty, Log, TEXT("[%s] Removed"), *(Party->GetPartyId()->ToDebugString()));
					}
					else
					{
						UE_LOG(LogParty, Log, TEXT("Removed - Invalid party Id"));
					}
					Party->HandleRemovedFromParty(EMemberExitedReason::Left);
				}
				JoinedParties.Remove(PartyKey);
			}
		}

		ensure(JoinedParties.Num() == 0);
		JoinedParties.Empty();
	}
}

void UParty::OnLoginStatusChanged(int32 LocalUserNum, ELoginStatus::Type OldStatus, ELoginStatus::Type NewStatus, const FUniqueNetId& NewId)
{
	if (NewStatus == ELoginStatus::NotLoggedIn)
	{
		if (JoinedParties.Num())
		{
			UE_LOG(LogParty, Log, TEXT("OnLoginStatusChanged: Party cleanup on logout"));
			TArray<FOnlinePartyTypeId> PartiesToRemove;
			for (const auto& PartyKeyValue : JoinedParties)
			{
				PartiesToRemove.Add(PartyKeyValue.Key);
			}
			for (const auto& PartyKey : PartiesToRemove)
			{
				UPartyGameState** FoundParty = JoinedParties.Find(PartyKey);
				if (FoundParty)
				{
					UPartyGameState* Party = *FoundParty;
					if (Party)
					{
						TSharedPtr<const FOnlinePartyId> PartyId = Party->GetPartyId();
						FString PartyIdString = PartyId.IsValid() ? PartyId->ToDebugString() : TEXT("");
						UE_LOG(LogParty, Log, TEXT("[%s] Removed"), *PartyIdString);
						Party->HandleRemovedFromParty(EMemberExitedReason::Left);
					}
					JoinedParties.Remove(PartyKey);
				}
			}

			ensure(JoinedParties.Num() == 0);
			JoinedParties.Empty();
		}
	}

	ClearPendingPartyJoin();
}

void UParty::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	Super::AddReferencedObjects(InThis, Collector);

	UParty* This = CastChecked<UParty>(InThis);
	check(This);

	TArray<UPartyGameState*> Parties;
	This->JoinedParties.GenerateValueArray(Parties);
	Collector.AddReferencedObjects(Parties);
}

void UParty::RegisterIdentityDelegates()
{
	UWorld* const World = GetWorld();
	if (ensure(World))
	{
		const IOnlineIdentityPtr IdentityInt = Online::GetIdentityInterface(World);
		if (IdentityInt.IsValid())
		{
			// Unbind and then rebind
			UnregisterIdentityDelegates();

			FOnLogoutCompleteDelegate OnLogoutCompleteDelegate;
			OnLogoutCompleteDelegate.BindUObject(this, &ThisClass::OnLogoutComplete);

			FOnLoginStatusChangedDelegate OnLoginStatusChangedDelegate;
			OnLoginStatusChangedDelegate.BindUObject(this, &ThisClass::OnLoginStatusChanged);

			for (int32 LocalPlayerId = 0; LocalPlayerId < MAX_LOCAL_PLAYERS; LocalPlayerId++)
			{
				LogoutCompleteDelegateHandle[LocalPlayerId] = IdentityInt->AddOnLogoutCompleteDelegate_Handle(LocalPlayerId, OnLogoutCompleteDelegate);
				LogoutStatusChangedDelegateHandle[LocalPlayerId] = IdentityInt->AddOnLoginStatusChangedDelegate_Handle(LocalPlayerId, OnLoginStatusChangedDelegate);
			}
		}
	}
	else
	{
		UE_LOG(LogParty, Warning, TEXT("UParty::RegisterIdentityDelegates: Missing World!"));
	}
}

void UParty::UnregisterIdentityDelegates()
{
	UWorld* const World = GetWorld();
	if (World)
	{
		const IOnlineIdentityPtr IdentityInt = Online::GetIdentityInterface(World);
		if (IdentityInt.IsValid())
		{
			for (int32 LocalPlayerId = 0; LocalPlayerId < MAX_LOCAL_PLAYERS; LocalPlayerId++)
			{
				if (LogoutCompleteDelegateHandle[LocalPlayerId].IsValid())
				{
					IdentityInt->ClearOnLogoutCompleteDelegate_Handle(LocalPlayerId, LogoutCompleteDelegateHandle[LocalPlayerId]);
				}

				if (LogoutStatusChangedDelegateHandle[LocalPlayerId].IsValid())
				{
					IdentityInt->ClearOnLoginStatusChangedDelegate_Handle(LocalPlayerId, LogoutStatusChangedDelegateHandle[LocalPlayerId]);
				}
			}
		}
	}
}

void UParty::RegisterPartyDelegates()
{
	UWorld* const World = GetWorld();
	if (ensure(World))
	{
		const IOnlinePartyPtr PartyInt = Online::GetPartyInterface(World);
		if (PartyInt.IsValid())
		{
			// Unbind and then rebind
			UnregisterPartyDelegates();

			PartyConfigChangedDelegateHandle = PartyInt->AddOnPartyConfigChangedDelegate_Handle(FOnPartyConfigChangedDelegate::CreateUObject(this, &ThisClass::PartyConfigChangedInternal));
			PartyMemberJoinedDelegateHandle = PartyInt->AddOnPartyMemberJoinedDelegate_Handle(FOnPartyMemberJoinedDelegate::CreateUObject(this, &ThisClass::PartyMemberJoinedInternal));
			PartyDataReceivedDelegateHandle = PartyInt->AddOnPartyDataReceivedDelegate_Handle(FOnPartyDataReceivedDelegate::CreateUObject(this, &ThisClass::PartyDataReceivedInternal));
			PartyMemberDataReceivedDelegateHandle = PartyInt->AddOnPartyMemberDataReceivedDelegate_Handle(FOnPartyMemberDataReceivedDelegate::CreateUObject(this, &ThisClass::PartyMemberDataReceivedInternal));
			PartyJoinRequestReceivedDelegateHandle = PartyInt->AddOnPartyJoinRequestReceivedDelegate_Handle(FOnPartyJoinRequestReceivedDelegate::CreateUObject(this, &ThisClass::PartyJoinRequestReceivedInternal));
			PartyQueryJoinabilityReceivedDelegateHandle = PartyInt->AddOnQueryPartyJoinabilityReceivedDelegate_Handle(FOnPartyJoinRequestReceivedDelegate::CreateUObject(this, &ThisClass::PartyQueryJoinabilityReceivedInternal));
			PartyMemberPromotedDelegateHandle = PartyInt->AddOnPartyMemberPromotedDelegate_Handle(FOnPartyMemberPromotedDelegate::CreateUObject(this, &ThisClass::PartyMemberPromotedInternal));
			PartyMemberExitedDelegateHandle = PartyInt->AddOnPartyMemberExitedDelegate_Handle(FOnPartyMemberExitedDelegate::CreateUObject(this, &ThisClass::PartyMemberExitedInternal));
			PartyPromotionLockoutChangedDelegateHandle = PartyInt->AddOnPartyPromotionLockoutChangedDelegate_Handle(FOnPartyPromotionLockoutChangedDelegate::CreateUObject(this, &ThisClass::PartyPromotionLockoutStateChangedInternal));
			PartyExitedDelegateHandle = PartyInt->AddOnPartyExitedDelegate_Handle(FOnPartyExitedDelegate::CreateUObject(this, &ThisClass::PartyExitedInternal));
			PartyStateChangedDelegateHandle = PartyInt->AddOnPartyStateChangedDelegate_Handle(FOnPartyStateChangedDelegate::CreateUObject(this, &ThisClass::PartyStateChanged));
		}
	}
}

void UParty::UnregisterPartyDelegates()
{
	UWorld* const World = GetWorld();
	if (World)
	{
		const IOnlinePartyPtr PartyInt = Online::GetPartyInterface(World);
		if (PartyInt.IsValid())
		{
			PartyInt->ClearOnPartyConfigChangedDelegate_Handle(PartyConfigChangedDelegateHandle);
			PartyInt->ClearOnPartyMemberJoinedDelegate_Handle(PartyMemberJoinedDelegateHandle);
			PartyInt->ClearOnPartyDataReceivedDelegate_Handle(PartyDataReceivedDelegateHandle);
			PartyInt->ClearOnPartyMemberDataReceivedDelegate_Handle(PartyMemberDataReceivedDelegateHandle);
			PartyInt->ClearOnPartyJoinRequestReceivedDelegate_Handle(PartyJoinRequestReceivedDelegateHandle);
			PartyInt->ClearOnQueryPartyJoinabilityReceivedDelegate_Handle(PartyQueryJoinabilityReceivedDelegateHandle);
			PartyInt->ClearOnPartyMemberPromotedDelegate_Handle(PartyMemberPromotedDelegateHandle);
			PartyInt->ClearOnPartyMemberExitedDelegate_Handle(PartyMemberExitedDelegateHandle);
			PartyInt->ClearOnPartyPromotionLockoutChangedDelegate_Handle(PartyPromotionLockoutChangedDelegateHandle);
			PartyInt->ClearOnPartyExitedDelegate_Handle(PartyExitedDelegateHandle);
			PartyInt->ClearOnPartyStateChangedDelegate_Handle(PartyStateChangedDelegateHandle);
		}
	}
}

void UParty::NotifyPreClientTravel(const FString& PendingURL, ETravelType TravelType, bool bIsSeamlessTravel)
{
	for (auto& JoinedParty : JoinedParties)
	{
		UPartyGameState* PartyState = JoinedParty.Value;
		PartyState->PreClientTravel();
	}
}

bool UParty::HasPendingPartyJoin() const
{
	return PendingPartyJoin.IsValid();
}

FName UParty::GetPlayerSessionName() const
{
	UGameInstance* GameInstance = Cast<UGameInstance>(GetOuter());
	if (GameInstance)
	{
		APlayerController* PlayerController = GameInstance->GetPrimaryPlayerController();
		if (PlayerController && PlayerController->PlayerState)
		{
			return PlayerController->PlayerState->SessionName;
		}
	}
	return NAME_GameSession;
}

UPartyGameState* UParty::GetParty(const FOnlinePartyId& InPartyId) const
{
	UPartyGameState* ResultPtr = nullptr;

	for (auto& JoinedParty : JoinedParties)
	{
		TSharedPtr<const FOnlinePartyId> PartyId = JoinedParty.Value->GetPartyId();
		if (*PartyId == InPartyId)
		{
			ResultPtr = JoinedParty.Value;
			break;
		}
	}

	return ResultPtr;
}

UPartyGameState* UParty::GetParty(const FOnlinePartyTypeId InPartyTypeId) const
{
	UPartyGameState* ResultPtr = nullptr;

	UPartyGameState* const * PartyPtr = JoinedParties.Find(InPartyTypeId);
	ResultPtr = PartyPtr ? *PartyPtr : nullptr;
	
	return ResultPtr;
}

UPartyGameState* UParty::GetPersistentParty() const
{
	UPartyGameState* ResultPtr = nullptr;

	UPartyGameState* const * PartyPtr = JoinedParties.Find(IOnlinePartySystem::GetPrimaryPartyTypeId());
	ResultPtr = PartyPtr ? *PartyPtr : nullptr;

	return ResultPtr;
}

void UParty::PartyConfigChangedInternal(const FUniqueNetId& InLocalUserId, const FOnlinePartyId& InPartyId, const TSharedRef<FPartyConfiguration>& InPartyConfig)
{
	UE_LOG(LogParty, Log, TEXT("[%s] Party config changed"), *InPartyId.ToString());

	UPartyGameState* PartyState = GetParty(InPartyId);
	if (PartyState)
	{
		PartyState->HandlePartyConfigChanged(InPartyConfig);
	}
	else
	{
		UE_LOG(LogParty, Warning, TEXT("[%s]: Missing party state during config change"), *InPartyId.ToString());
	}
}

void UParty::PartyMemberJoinedInternal(const FUniqueNetId& InLocalUserId, const FOnlinePartyId& InPartyId, const FUniqueNetId& InMemberId)
{
	UE_LOG(LogParty, Log, TEXT("[%s] Player %s joined"), *InPartyId.ToString(), *InMemberId.ToString());

	UPartyGameState* PartyState = GetParty(InPartyId);
	if (PartyState)
	{
		PartyState->HandlePartyMemberJoined(InMemberId);
	}
	else
	{
		UE_LOG(LogParty, Warning, TEXT("[%s]: Missing party state player: %s"), *InPartyId.ToString(), *InMemberId.ToString());
	}
}

void UParty::PartyDataReceivedInternal(const FUniqueNetId& InLocalUserId, const FOnlinePartyId& InPartyId, const TSharedRef<FOnlinePartyData>& InPartyData)
{
	UE_LOG(LogParty, Log, TEXT("[%s] party data received"), *InPartyId.ToString());

	UPartyGameState* PartyState = GetParty(InPartyId);
	if (PartyState)
	{
		PartyState->HandlePartyDataReceived(InPartyData);
	}
	else
	{
		UE_LOG(LogParty, Warning, TEXT("[%s]: Missing party state to apply data."), *InPartyId.ToString());
	}
}

void UParty::PartyMemberDataReceivedInternal(const FUniqueNetId& InLocalUserId, const FOnlinePartyId& InPartyId, const FUniqueNetId& InMemberId, const TSharedRef<FOnlinePartyData>& InPartyMemberData)
{
	UE_LOG(LogParty, Log, TEXT("[%s] Player %s data received"), *InPartyId.ToString(), *InMemberId.ToString());

	UPartyGameState* PartyState = GetParty(InPartyId);
	if (PartyState)
	{
		PartyState->HandlePartyMemberDataReceived(InMemberId, InPartyMemberData);
	}
	else
	{
		UE_LOG(LogParty, Warning, TEXT("[%s]: Missing party state to apply data for player %s"), *InPartyId.ToString(), *InMemberId.ToString());
	}
}

void UParty::PartyJoinRequestReceivedInternal(const FUniqueNetId& InLocalUserId, const FOnlinePartyId& InPartyId, const FUniqueNetId& SenderId)
{
	UPartyGameState* PartyState = GetParty(InPartyId);
	if (PartyState)
	{
		PartyState->HandlePartyJoinRequestReceived(InLocalUserId, SenderId);
	}
	else
	{
		UE_LOG(LogParty, Warning, TEXT("[%s]: Missing party state to process join request."), *InPartyId.ToString());
	}
}

void UParty::PartyQueryJoinabilityReceivedInternal(const FUniqueNetId& InLocalUserId, const FOnlinePartyId& InPartyId, const FUniqueNetId& SenderId)
{
	UPartyGameState* PartyState = GetParty(InPartyId);
	if (PartyState)
	{
		PartyState->HandlePartyQueryJoinabilityRequestReceived(InLocalUserId, SenderId);
	}
	else
	{
		UE_LOG(LogParty, Warning, TEXT("[%s]: Missing party state to process join request."), *InPartyId.ToString());
	}
}

void UParty::PartyMemberPromotedInternal(const FUniqueNetId& InLocalUserId, const FOnlinePartyId& InPartyId, const FUniqueNetId& InNewLeaderId)
{
	if (InLocalUserId == InNewLeaderId)
	{
		UE_LOG(LogParty, Log, TEXT("[%s] [%s] local member promoted"), *InPartyId.ToString(), *InNewLeaderId.ToString());
	}
	else
	{
		UE_LOG(LogParty, Log, TEXT("[%s] [%s] remote member promoted"), *InPartyId.ToString(), *InNewLeaderId.ToString());
	}

	UPartyGameState* PartyState = GetParty(InPartyId);
	if (!PartyState)
	{
		UE_LOG(LogParty, Warning, TEXT("[%s]: Missing party state during member change."), *InPartyId.ToString());
	}

	if (PartyState)
	{
		PartyState->HandlePartyMemberPromoted(InNewLeaderId);
	}

	if (PersistentPartyId.IsValid() &&
		(InPartyId == *PersistentPartyId))
	{
		FUniqueNetIdRepl NewPartyLeader;
		NewPartyLeader.SetUniqueNetId(InNewLeaderId.AsShared());
		UpdatePersistentPartyLeader(NewPartyLeader);
	}
}

void UParty::PartyMemberExitedInternal(const FUniqueNetId& InLocalUserId, const FOnlinePartyId& InPartyId, const FUniqueNetId& InMemberId, const EMemberExitedReason InReason)
{
	if (InLocalUserId == InMemberId)
	{
		UE_LOG(LogParty, Log, TEXT("[%s] [%s] local member removed. Reason: %s"), *InPartyId.ToString(), *InMemberId.ToString(), ToString(InReason));
	}
	else
	{
		UE_LOG(LogParty, Log, TEXT("[%s] [%s] remote member exited. Reason: %s"), *InPartyId.ToString(), *InMemberId.ToString(), ToString(InReason));
	}

	if (InLocalUserId == InMemberId)
	{
		if (InReason == EMemberExitedReason::Left)
		{
			// EMemberExitedReason::Left -> local player chose to leave, handled by leave completion delegate
		}
		else
		{
			UPartyGameState* PartyState = GetParty(InPartyId);

			// EMemberExitedReason::Kicked -> party/leader kicked you
			if (PartyState)
			{
				FOnlinePartyTypeId PartyTypeId = PartyState->GetPartyTypeId();
				PartyState->HandleRemovedFromParty(InReason);
				JoinedParties.Remove(PartyTypeId);
			}
			else
			{
				UE_LOG(LogParty, Warning, TEXT("[%s]: Missing party state during local player exit."), *InPartyId.ToString());
			}
		}

		// If the removal was the persistent party, make sure we are in a good state
		if (PersistentPartyId.IsValid() &&
			(InPartyId == *PersistentPartyId))
		{
			RestorePersistentPartyState();
		}
	}
	else
	{
		UPartyGameState* PartyState = GetParty(InPartyId);
		if (!PartyState)
		{
			UE_LOG(LogParty, Warning, TEXT("[%s]: Missing party state during remote player exit."), *InPartyId.ToString());
		}

		if (PartyState)
		{
			// EMemberExitedReason::Left -> player chose to leave
			// EMemberExitedReason::Removed -> player was removed forcibly (timeout from disconnect, etc)
			// EMemberExitedReason::Kicked -> party/leader kicked this player
			// EMemberExitedReason::Unknown -> bad but try to cleanup anyway
			PartyState->HandlePartyMemberLeft(InMemberId, InReason);
		}
	}
}

void UParty::PartyPromotionLockoutStateChangedInternal(const FUniqueNetId& LocalUserId, const FOnlinePartyId& InPartyId, const bool bLockoutState)
{
	UE_LOG(LogParty, Log, TEXT("[%s] party lockout state changed to %s"), *InPartyId.ToString(), bLockoutState ? TEXT("true") : TEXT("false"));

	UPartyGameState* PartyState = GetParty(InPartyId);
	if (PartyState)
	{
		PartyState->HandleLockoutPromotionStateChange(bLockoutState);
	}
	else
	{
		UE_LOG(LogParty, Warning, TEXT("[%s]: Missing party state during lockout call"), *InPartyId.ToString());
	}
}

void UParty::PartyExitedInternal(const FUniqueNetId& LocalUserId, const FOnlinePartyId& InPartyId)
{
	UE_LOG(LogParty, Log, TEXT("PartyExitedInternal: [%s] exited party %s"), *InPartyId.ToString(), *(InPartyId.ToDebugString()));

	UPartyGameState* PartyState = GetParty(InPartyId);
	if (PartyState != nullptr && (PartyState->OssParty->PartyTypeId == IOnlinePartySystem::GetPrimaryPartyTypeId()))
	{
		TSharedRef<FPartyConfiguration> Config = PartyState->OssParty->Config;
		PartyState->HandleLeavingParty();
		PartyState->HandleRemovedFromParty(EMemberExitedReason::Left);
		JoinedParties.Remove(IOnlinePartySystem::GetPrimaryPartyTypeId());

		UPartyDelegates::FOnCreateUPartyComplete CompletionDelegate;
		CompletionDelegate.BindUObject(this, &ThisClass::OnPersistentPartyExitedInternalCompleted);
		CreatePartyInternal(LocalUserId, IOnlinePartySystem::GetPrimaryPartyTypeId(), *Config, CompletionDelegate);
	}
	else
	{
		UE_LOG(LogParty, Display, TEXT("[%s]: Missing party state during exit"), *InPartyId.ToString());
	}
}

void UParty::OnPersistentPartyExitedInternalCompleted(const FUniqueNetId& LocalUserId, const ECreatePartyCompletionResult Result)
{
	if (Result == ECreatePartyCompletionResult::Succeeded)
	{
		OnCreatePesistentPartyCompletedCommon(LocalUserId);

		UPartyGameState** Party = JoinedParties.Find(IOnlinePartySystem::GetPrimaryPartyTypeId());

		if (Party != nullptr)
		{
			PartyResetForFrontendDelegate.Broadcast(*Party);
		}
	}

	if (Result != ECreatePartyCompletionResult::Succeeded)
	{
		UE_LOG(LogParty, Warning, TEXT("Error when attempting to recreate persistent party on reconnection error=%s"), ToString(Result));
	}
}

void UParty::PartyStateChanged(const FUniqueNetId& LocalUserId, const FOnlinePartyId& InPartyId, EPartyState State)
{
	UE_LOG(LogParty, Verbose, TEXT("PartyStateChanged: [%s] state changed to %s"), *InPartyId.ToString(), ToString(State));

	UPartyGameState* PartyState = GetParty(InPartyId);
	if (PartyState != nullptr && (PartyState->OssParty->PartyTypeId == IOnlinePartySystem::GetPrimaryPartyTypeId()))
	{
		if (State == EPartyState::Disconnected)
		{
			// If we have other members in our party, then we will try to rejoin this when we come back online
			if (PartyConsoleVariables::CVarPartyEnableAutoRejoin.GetValueOnGameThread() &&
				ShouldCacheDisconnectedPersistentPartyForRejoin(PartyState))
			{
				UE_LOG(LogParty, Log, TEXT("PartyStateChanged: [%s] Caching party for rejoin"), *InPartyId.ToString());
				TArray<TSharedRef<const FUniqueNetId>> MemberIds;
				
				TArray<UPartyMemberState*> PartyMembers;
				PartyState->GetAllPartyMembers(PartyMembers);
				for (UPartyMemberState* PartyMember : PartyMembers)
				{
					if (PartyMember->UniqueId.IsValid() && *PartyMember->UniqueId != LocalUserId)
					{
						MemberIds.Add(PartyMember->UniqueId.GetUniqueNetId().ToSharedRef());
					}
				}
				RejoinableParty = MakeShareable(new FRejoinableParty(PartyState->GetPartyId().ToSharedRef(), MemberIds));
			}
		}
		else if (State == EPartyState::Active)
		{
			// If we are still in our party of one and have a rejoinable party, try to rejoin
			if (RejoinableParty.IsValid())
			{
				if (PartyState->GetPartySize() == 1)
				{
					if (ShouldTryRejoiningPersistentParty(*RejoinableParty))
					{
						LeavePersistentPartyForRejoin();
					}
					else
					{
						// This is the only time we would try to rejoin
						RejoinableParty = nullptr;
					}
				}
				else
				{
					// We have a new party, no need to try to rejoin the previous party
					RejoinableParty = nullptr;
				}
			}
		}
	}
}

bool UParty::ShouldCacheDisconnectedPersistentPartyForRejoin(UPartyGameState* PartyState)
{
	if (!RejoinableParty.IsValid())
	{
		int32 PartySize = PartyState->GetPartySize();
		if (PartySize > 1)
		{
			UE_LOG(LogParty, VeryVerbose, TEXT("ShouldCacheDisconnectedPartyForRejoin: [%s] Considering party for rejoining"), *PartyState->GetPartyId()->ToString());
			return true;
		}
		else
		{
			UE_LOG(LogParty, VeryVerbose, TEXT("ShouldCacheDisconnectedPartyForRejoin: [%s] Not enough members (%d) to want to rejoin this party"), *PartyState->GetPartyId()->ToString(), PartySize);
		}
	}
	else
	{
		UE_LOG(LogParty, VeryVerbose, TEXT("ShouldCacheDisconnectedPartyForRejoin: [%s] Already have rejoinable party"), *PartyState->GetPartyId()->ToString());
	}
	return false;
}

bool UParty::ShouldTryRejoiningPersistentParty(const FRejoinableParty& InRejoinableParty)
{
	// Game specific logic should determine if we are in a state where we should leave our current party to try to join the former party
	// Note that we are not guaranteed to successfully join the former party (e.g., it became full, all members left, etc)
	return false;
}

void UParty::LeavePersistentPartyForRejoin()
{
	UE_LOG(LogParty, Display, TEXT("UParty::LeavePersistentPartyForRejoin"));
	check(RejoinableParty.IsValid());

	const UGameInstance* const GameInstance = GetGameInstance();
	check(GameInstance);

	const TSharedPtr<const FUniqueNetId> LocalUserId = GameInstance->GetPrimaryPlayerUniqueId();
	if (ensure(LocalUserId.IsValid() && LocalUserId->IsValid()))
	{
		UPartyDelegates::FOnLeaveUPartyComplete CompletionDelegate;
		CompletionDelegate.BindUObject(this, &ThisClass::OnLeavePersistentPartyForRejoinComplete);
		LeavePersistentParty(*LocalUserId, CompletionDelegate);
	}
}

void UParty::OnLeavePersistentPartyForRejoinComplete(const FUniqueNetId& LocalUserId, const ELeavePartyCompletionResult LeaveResult)
{
	EJoinPartyCompletionResult JoinResult = EJoinPartyCompletionResult::UnknownClientFailure;
	FString ErrorMsg;

	IOnlinePartyPtr PartyInt = Online::GetPartyInterface(GetWorld());
	if (PartyInt.IsValid())
	{
		if (RejoinableParty.IsValid())
		{
			FOnJoinPartyComplete CompletionDelegate;
			CompletionDelegate.BindUObject(this, &ThisClass::OnRejoinPartyComplete);
			PartyInt->RejoinParty(LocalUserId, *RejoinableParty->PartyId, IOnlinePartySystem::GetPrimaryPartyTypeId(), RejoinableParty->Members, CompletionDelegate);
			JoinResult = EJoinPartyCompletionResult::Succeeded;
		}
		else
		{
			JoinResult = EJoinPartyCompletionResult::JoinInfoInvalid;
			ErrorMsg = TEXT("No rejoinable party");
		}
	}
	else
	{
		JoinResult = EJoinPartyCompletionResult::UnknownClientFailure;
		ErrorMsg = FString::Printf(TEXT("No party interface during OnLeavePersistentPartyForRejoinComplete()"));
	}

	if (JoinResult != EJoinPartyCompletionResult::Succeeded)
	{
		UE_LOG(LogParty, Warning, TEXT("%s"), *ErrorMsg);
		RejoinableParty.Reset();
		HandleJoinPersistentPartyFailure();
	}
}

void UParty::OnRejoinPartyComplete(const FUniqueNetId& LocalUserId, const FOnlinePartyId& PartyId, const EJoinPartyCompletionResult Result, int32 DeniedResultCode)
{
	if (Result != EJoinPartyCompletionResult::LoggedOut)
	{
		// If we were logged out in the middle of this operation, attempt again when we reconnect
		RejoinableParty.Reset();
	}
	if (Result == EJoinPartyCompletionResult::Succeeded)
	{
		UPartyDelegates::FOnJoinUPartyComplete CompletionDelegate;
		CompletionDelegate.BindUObject(this, &ThisClass::OnJoinPersistentPartyComplete, UPartyDelegates::FOnJoinUPartyComplete());
		OnJoinPartyInternalComplete(LocalUserId, PartyId, Result, DeniedResultCode, IOnlinePartySystem::GetPrimaryPartyTypeId(), CompletionDelegate);
	}
	else
	{
		HandleJoinPersistentPartyFailure();
	}
}

void UParty::CreatePartyInternal(const FUniqueNetId& InUserId, const FOnlinePartyTypeId InPartyTypeId, const FPartyConfiguration& InPartyConfig, const UPartyDelegates::FOnCreateUPartyComplete& InCompletionDelegate)
{
	ECreatePartyCompletionResult Result = ECreatePartyCompletionResult::UnknownClientFailure;
	FString ErrorMsg;

	UWorld* World = GetWorld();
	IOnlinePartyPtr PartyInt = Online::GetPartyInterface(World);
	if (PartyInt.IsValid())
	{
		FOnCreatePartyComplete CompletionDelegate;
		CompletionDelegate.BindUObject(this, &ThisClass::OnCreatePartyInternalComplete, InPartyTypeId, InCompletionDelegate);
		PartyInt->CreateParty(InUserId, InPartyTypeId, InPartyConfig, CompletionDelegate);
		Result = ECreatePartyCompletionResult::Succeeded;
	}
	else
	{
		Result = ECreatePartyCompletionResult::UnknownClientFailure;
		ErrorMsg = FString::Printf(TEXT("No party interface during JoinParty()"));
	}

	if (Result != ECreatePartyCompletionResult::Succeeded)
	{
		UE_LOG(LogParty, Warning, TEXT("%s"), *ErrorMsg);
		InCompletionDelegate.ExecuteIfBound(InUserId, Result);
	}
}

void UParty::OnCreatePartyInternalComplete(const FUniqueNetId& LocalUserId, const TSharedPtr<const FOnlinePartyId>& InPartyId, const ECreatePartyCompletionResult Result, const FOnlinePartyTypeId InPartyTypeId, UPartyDelegates::FOnCreateUPartyComplete InCompletionDelegate)
{
	const FString PartyIdDebugString = InPartyId.IsValid() ? InPartyId->ToDebugString() : TEXT("Invalid");
	UE_LOG(LogParty, Display, TEXT("OnCreatePartyInternalComplete() %s %s"), *PartyIdDebugString, ToString(Result));

	ECreatePartyCompletionResult LocalResult = Result;
	if (Result == ECreatePartyCompletionResult::Succeeded)
	{
		UWorld* World = GetWorld();
		IOnlinePartyPtr PartyInt = Online::GetPartyInterface(World);
		if (PartyInt.IsValid())
		{
			TSharedPtr<const FOnlineParty> Party = PartyInt->GetParty(LocalUserId, InPartyTypeId);
			if (ensure(Party.IsValid()))
			{
				TSubclassOf<UPartyGameState>* PartyGameStateClass = PartyClasses.Find(InPartyTypeId);
				if (PartyGameStateClass != nullptr)
				{
					UPartyGameState* NewParty = NewObject<UPartyGameState>(this, *PartyGameStateClass);

					// Add right away so future delegate broadcasts have this available
					JoinedParties.Add(InPartyTypeId, NewParty);

					// Initialize and trigger delegates
					NewParty->InitFromCreate(LocalUserId, Party);

					LocalResult = ECreatePartyCompletionResult::Succeeded;
				}
				else
				{
					LocalResult = ECreatePartyCompletionResult::UnknownClientFailure;
				}
			}
			else
			{
				LocalResult = ECreatePartyCompletionResult::UnknownClientFailure;
			}
		}
		else
		{
			LocalResult = ECreatePartyCompletionResult::UnknownClientFailure;
		}
	}

	if (LocalResult != ECreatePartyCompletionResult::Succeeded)
	{
		UE_LOG(LogParty, Warning, TEXT("Error when creating party %s error=%s"), *PartyIdDebugString, ToString(LocalResult));
	}

	InCompletionDelegate.ExecuteIfBound(LocalUserId, LocalResult);
}

void UParty::JoinPartyInternal(const FUniqueNetId& InUserId, const FPartyDetails& InPartyDetails, const UPartyDelegates::FOnJoinUPartyComplete& InCompletionDelegate)
{
	EJoinPartyCompletionResult Result = EJoinPartyCompletionResult::UnknownClientFailure;
	FString ErrorMsg;

	IOnlinePartyPtr PartyInt = Online::GetPartyInterface(GetWorld());
	if (PartyInt.IsValid())
	{
		if (InPartyDetails.IsValid())
		{
			const FOnlinePartyId& PartyId = *InPartyDetails.GetPartyId();
			// High level party data check
			UPartyGameState* PartyState = GetParty(PartyId);
			// Interface level party data check should not be out of sync
			TSharedPtr<const FOnlineParty> Party = PartyInt->GetParty(InUserId, PartyId);
			if (!PartyState)
			{
				if (!Party.IsValid())
				{
					FOnJoinPartyComplete CompletionDelegate;
					CompletionDelegate.BindUObject(this, &ThisClass::OnJoinPartyInternalComplete, InPartyDetails.GetPartyTypeId(), InCompletionDelegate);
					PartyInt->JoinParty(InUserId, *(InPartyDetails.PartyJoinInfo), CompletionDelegate);
					Result = EJoinPartyCompletionResult::Succeeded;
				}
				else
				{
					Result = EJoinPartyCompletionResult::AlreadyJoiningParty;
					ErrorMsg = FString::Printf(TEXT("Already joining party %s, not joining again."), *InPartyDetails.GetPartyId()->ToString());
				}
			}
			else
			{
				Result = EJoinPartyCompletionResult::AlreadyInParty;
				ErrorMsg = FString::Printf(TEXT("Already in party %s, not joining again."), *InPartyDetails.GetPartyId()->ToString());
			}
		}
		else
		{
			Result = EJoinPartyCompletionResult::JoinInfoInvalid;
			ErrorMsg = FString::Printf(TEXT("Invalid party details, cannot join. Details: %s"), *InPartyDetails.ToString());
		}
	}
	else
	{
		Result = EJoinPartyCompletionResult::UnknownClientFailure;
		ErrorMsg = FString::Printf(TEXT("No party interface during JoinParty()"));
	}

	if (Result != EJoinPartyCompletionResult::Succeeded)
	{
		UE_LOG(LogParty, Warning, TEXT("%s"), *ErrorMsg);
		InCompletionDelegate.ExecuteIfBound(InUserId, Result, 0);
	}
}

void UParty::OnJoinPartyInternalComplete(const FUniqueNetId& LocalUserId, const FOnlinePartyId& InPartyId, const EJoinPartyCompletionResult Result, int32 DeniedResultCode, const FOnlinePartyTypeId InPartyTypeId, UPartyDelegates::FOnJoinUPartyComplete CompletionDelegate)
{
	const FString PartyIdDebugString = InPartyId.ToDebugString();
	UE_LOG(LogParty, Display, TEXT("OnJoinPartyInternalComplete() %s %s."), *PartyIdDebugString, ToString(Result));

	EJoinPartyCompletionResult LocalResult = Result;
	if (Result == EJoinPartyCompletionResult::Succeeded)
	{
		UWorld* World = GetWorld();
		IOnlinePartyPtr PartyInt = Online::GetPartyInterface(World);
		if (PartyInt.IsValid())
		{
			TSharedPtr<const FOnlineParty> Party = PartyInt->GetParty(LocalUserId, InPartyTypeId);
			if (Party.IsValid())
			{
				TSubclassOf<UPartyGameState>* PartyGameStateClass = PartyClasses.Find(InPartyTypeId);
				if (PartyGameStateClass != nullptr)
				{
					UPartyGameState* NewParty = NewObject<UPartyGameState>(this, *PartyGameStateClass);

					// Add right away so future delegate broadcasts have this available
					JoinedParties.Add(InPartyTypeId, NewParty);

					// Initialize and trigger delegates
					NewParty->InitFromJoin(LocalUserId, Party);
				}
				else
				{
					LocalResult = EJoinPartyCompletionResult::AlreadyInPartyOfSpecifiedType;
				}
			}
			else
			{
				LocalResult = EJoinPartyCompletionResult::UnknownClientFailure;
			}
		}
		else
		{
			LocalResult = EJoinPartyCompletionResult::UnknownClientFailure;
		}
	}

	if (LocalResult != EJoinPartyCompletionResult::Succeeded)
	{
		UE_LOG(LogParty, Warning, TEXT("Error when joining party %s error=%s"), *PartyIdDebugString, ToString(LocalResult));
	}
	
	int32 OutDeniedResultCode = (Result == EJoinPartyCompletionResult::NotApproved) ? DeniedResultCode : 0;
	CompletionDelegate.ExecuteIfBound(LocalUserId, LocalResult, OutDeniedResultCode);
}

void UParty::LeavePartyInternal(const FUniqueNetId& InUserId, const FOnlinePartyTypeId InPartyTypeId, const UPartyDelegates::FOnLeaveUPartyComplete& InCompletionDelegate)
{
	ELeavePartyCompletionResult Result = ELeavePartyCompletionResult::UnknownClientFailure;
	FString ErrorMsg;

	IOnlinePartyPtr PartyInt = Online::GetPartyInterface(GetWorld());
	if (PartyInt.IsValid())
	{
		// Get the party by type here (don't rely on interface structures here because they can be missing during disconnects)
		UPartyGameState* PartyState = GetParty(InPartyTypeId);
		if (PartyState)
		{
			PartyState->HandleLeavingParty();

			TSharedPtr<const FOnlinePartyId> PartyId = PartyState->GetPartyId();
			if (ensure(PartyId.IsValid()))
			{
				FOnLeavePartyComplete CompletionDelegate;
				CompletionDelegate.BindUObject(this, &ThisClass::OnLeavePartyInternalComplete, InPartyTypeId, InCompletionDelegate);
				PartyInt->LeaveParty(InUserId, *PartyId, CompletionDelegate);
				Result = ELeavePartyCompletionResult::Succeeded;
			}
			else
			{
				UE_LOG(LogParty, Log, TEXT("LeavePartyInternal:  Removing party because we cannot call LeaveParty (missing ID)"));
				// Manual cleanup here because we can't call the above delegate
				PartyState->HandleRemovedFromParty(EMemberExitedReason::Left);
				JoinedParties.Remove(InPartyTypeId);
			}
		}
		else
		{
			Result = ELeavePartyCompletionResult::UnknownParty;
			ErrorMsg = FString::Printf(TEXT("Party not found in LeaveParty()"));
		}
	}
	else
	{
		Result = ELeavePartyCompletionResult::UnknownClientFailure;
		ErrorMsg = FString::Printf(TEXT("No party interface during LeaveParty()"));
	}

	if (Result != ELeavePartyCompletionResult::Succeeded)
	{
		UE_LOG(LogParty, Warning, TEXT("%s"), *ErrorMsg);
		InCompletionDelegate.ExecuteIfBound(InUserId, Result);
	}
}

void UParty::OnLeavePartyInternalComplete(const FUniqueNetId& LocalUserId, const FOnlinePartyId& InPartyId, const ELeavePartyCompletionResult Result, const FOnlinePartyTypeId InPartyTypeId, UPartyDelegates::FOnLeaveUPartyComplete CompletionDelegate)
{
	const FString PartyIdDebugString(InPartyId.ToDebugString());
	UE_LOG(LogParty, Display, TEXT("OnLeavePartyInternalComplete() %s %s."), *PartyIdDebugString, ToString(Result));

	UPartyGameState* const PartyState = GetParty(InPartyTypeId);
	if (PartyState)
	{
		PartyState->HandleRemovedFromParty(EMemberExitedReason::Left);
		JoinedParties.Remove(InPartyTypeId);
	}
	else
	{
		UE_LOG(LogParty, Warning, TEXT("OnLeavePartyInternalComplete: Missing party state %s"), *PartyIdDebugString);
	}

	CompletionDelegate.ExecuteIfBound(LocalUserId, Result);
}

void UParty::CreateParty(const FUniqueNetId& InUserId, const FOnlinePartyTypeId InPartyTypeId, const FPartyConfiguration& InPartyConfig, const UPartyDelegates::FOnCreateUPartyComplete& InCompletionDelegate)
{
	UPartyDelegates::FOnCreateUPartyComplete CompletionDelegate;
	CompletionDelegate.BindUObject(this, &ThisClass::OnCreatePartyComplete, InPartyTypeId, InCompletionDelegate);
	CreatePartyInternal(InUserId, InPartyTypeId, InPartyConfig, CompletionDelegate);
}

void UParty::OnCreatePartyComplete(const FUniqueNetId& LocalUserId, const ECreatePartyCompletionResult Result, const FOnlinePartyTypeId InPartyTypeId, UPartyDelegates::FOnCreateUPartyComplete InCompletionDelegate)
{
	UE_LOG(LogParty, Display, TEXT("OnCreatePartyComplete() type(0x%08x) %s"), InPartyTypeId.GetValue(), ToString(Result));

	InCompletionDelegate.ExecuteIfBound(LocalUserId, Result);
}

void UParty::JoinParty(const FUniqueNetId& InUserId, const FPartyDetails& InPartyDetails, const UPartyDelegates::FOnJoinUPartyComplete& InCompletionDelegate)
{
	UPartyDelegates::FOnJoinUPartyComplete CompletionDelegate;
	CompletionDelegate.BindUObject(this, &ThisClass::OnJoinPartyComplete, InPartyDetails.GetPartyTypeId(), InCompletionDelegate);
	JoinPartyInternal(InUserId, InPartyDetails, CompletionDelegate);
}

void UParty::OnJoinPartyComplete(const FUniqueNetId& LocalUserId, EJoinPartyCompletionResult Result, int32 DeniedResultCode, const FOnlinePartyTypeId InPartyTypeId, UPartyDelegates::FOnJoinUPartyComplete InCompletionDelegate)
{
	UE_LOG(LogParty, Display, TEXT("OnJoinPartyComplete() type(0x%08x) %s"), InPartyTypeId.GetValue(), ToString(Result));

	int32 OutDeniedResultCode = (Result == EJoinPartyCompletionResult::NotApproved) ? DeniedResultCode : 0;
	InCompletionDelegate.ExecuteIfBound(LocalUserId, Result, OutDeniedResultCode);
}

void UParty::QueryPartyJoinability(const FUniqueNetId& InUserId, const FPartyDetails& InPartyDetails, const UPartyDelegates::FOnQueryUPartyJoinabilityComplete& InCompletionDelegate)
{
	EJoinPartyCompletionResult Result = EJoinPartyCompletionResult::UnknownClientFailure;
	FString ErrorMsg;

	IOnlinePartyPtr PartyInt = Online::GetPartyInterface(GetWorld());
	if (PartyInt.IsValid())
	{
		if (InPartyDetails.IsValid())
		{
			const FOnlinePartyId& PartyId = *InPartyDetails.GetPartyId();
			// High level party data check
			UPartyGameState* PartyState = GetParty(PartyId);
			// Interface level party data check should not be out of sync
			TSharedPtr<const FOnlineParty> Party = PartyInt->GetParty(InUserId, PartyId);
			if (!PartyState)
			{
				if (!Party.IsValid())
				{
					FOnQueryPartyJoinabilityComplete CompletionDelegate;
					CompletionDelegate.BindUObject(this, &ThisClass::OnQueryPartyJoinabilityComplete, InPartyDetails.GetPartyTypeId(), InCompletionDelegate);
					PartyInt->QueryPartyJoinability(InUserId, *(InPartyDetails.PartyJoinInfo), CompletionDelegate);
					Result = EJoinPartyCompletionResult::Succeeded;
				}
				else
				{
					Result = EJoinPartyCompletionResult::AlreadyJoiningParty;
					ErrorMsg = FString::Printf(TEXT("Already joining party %s, not joining again."), *InPartyDetails.GetPartyId()->ToString());
				}
			}
			else
			{
				Result = EJoinPartyCompletionResult::AlreadyInParty;
				ErrorMsg = FString::Printf(TEXT("Already in party %s, not joining again."), *InPartyDetails.GetPartyId()->ToString());
			}
		}
		else
		{
			Result = EJoinPartyCompletionResult::JoinInfoInvalid;
			ErrorMsg = FString::Printf(TEXT("Invalid party details, cannot join. Details: %s"), *InPartyDetails.ToString());
		}
	}
	else
	{
		Result = EJoinPartyCompletionResult::UnknownClientFailure;
		ErrorMsg = FString::Printf(TEXT("No party interface during JoinParty()"));
	}

	if (Result != EJoinPartyCompletionResult::Succeeded)
	{
		UE_LOG(LogParty, Warning, TEXT("%s"), *ErrorMsg);
		InCompletionDelegate.ExecuteIfBound(InUserId, Result, 0);
	}
}

void UParty::OnQueryPartyJoinabilityComplete(const FUniqueNetId& LocalUserId, const FOnlinePartyId& InPartyId, const EJoinPartyCompletionResult Result, int32 DeniedResultCode, const FOnlinePartyTypeId InPartyTypeId, UPartyDelegates::FOnQueryUPartyJoinabilityComplete InCompletionDelegate)
{
	UE_LOG(LogParty, Display, TEXT("UParty::OnQueryPartyJoinabilityComplete: type(0x%08x) %s"), InPartyTypeId.GetValue(), ToString(Result));

	int32 OutDeniedResultCode = (Result == EJoinPartyCompletionResult::NotApproved) ? DeniedResultCode : 0;
	InCompletionDelegate.ExecuteIfBound(LocalUserId, Result, OutDeniedResultCode);
}

void UParty::LeaveParty(const FUniqueNetId& InUserId, const FOnlinePartyTypeId InPartyTypeId, const UPartyDelegates::FOnLeaveUPartyComplete& InCompletionDelegate)
{
	UE_LOG(LogParty, Display, TEXT("UParty::LeaveParty: type(0x%08x)"), InPartyTypeId.GetValue());
	UPartyDelegates::FOnLeaveUPartyComplete CompletionDelegate;
	CompletionDelegate.BindUObject(this, &ThisClass::OnLeavePartyComplete, InPartyTypeId, InCompletionDelegate);
	LeavePartyInternal(InUserId, InPartyTypeId, CompletionDelegate);
}

void UParty::OnLeavePartyComplete(const FUniqueNetId& LocalUserId, const ELeavePartyCompletionResult Result, const FOnlinePartyTypeId InPartyTypeId, UPartyDelegates::FOnLeaveUPartyComplete InCompletionDelegate)
{
	UE_LOG(LogParty, Display, TEXT("UParty::OnLeavePartyComplete: type(0x%08x) %s"), InPartyTypeId.GetValue(), ToString(Result));

	InCompletionDelegate.ExecuteIfBound(LocalUserId, Result);
}

void UParty::GetDefaultPersistentPartySettings(EPartyType& PartyType, bool& bLeaderFriendsOnly, bool& bLeaderInvitesOnly, bool& bAllowInvites)
{
	PartyType = EPartyType::Public;
	bLeaderInvitesOnly = false;
	bLeaderFriendsOnly = false;
	bAllowInvites = true;
}

void UParty::GetPersistentPartyConfiguration(FPartyConfiguration& PartyConfig)
{
	EPartyType PartyType = EPartyType::Public;
	bool bLeaderInvitesOnly = false;
	bool bLeaderFriendsOnly = false;
	bool bAllowInvites = true;
	GetDefaultPersistentPartySettings(PartyType, bLeaderFriendsOnly, bLeaderInvitesOnly, bAllowInvites);

	bool bIsPrivate = (PartyType == EPartyType::Private);

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

	PartyConfig.JoinRequestAction = EJoinRequestAction::Manual;
	PartyConfig.bIsAcceptingMembers = bIsPrivate ? false : true;
	PartyConfig.bShouldRemoveOnDisconnection = true;
	PartyConfig.PresencePermissions = PresencePermissions;
	PartyConfig.InvitePermissions = bAllowInvites ? (bLeaderInvitesOnly ? PartySystemPermissions::EInvitePermissions::Leader : PartySystemPermissions::EInvitePermissions::Anyone) : PartySystemPermissions::EInvitePermissions::Noone;

	PartyConfig.MaxMembers = DefaultMaxPartySize;
}

void UParty::CreatePersistentParty(const FUniqueNetId& InUserId, const UPartyDelegates::FOnCreateUPartyComplete& InCompletionDelegate)
{
	if (PersistentPartyId.IsValid())
	{
		UE_LOG(LogParty, Warning, TEXT("Existing persistent party %s found when creating a new one."), *PersistentPartyId->ToString());
	}

	PersistentPartyId = nullptr;

	FPartyConfiguration PartyConfig;
	GetPersistentPartyConfiguration(PartyConfig);

	UPartyDelegates::FOnCreateUPartyComplete CompletionDelegate;
	CompletionDelegate.BindUObject(this, &ThisClass::OnCreatePersistentPartyComplete, InCompletionDelegate);
	CreatePartyInternal(InUserId, IOnlinePartySystem::GetPrimaryPartyTypeId(), PartyConfig, CompletionDelegate);
}

void UParty::OnCreatePersistentPartyComplete(const FUniqueNetId& LocalUserId, const ECreatePartyCompletionResult Result, UPartyDelegates::FOnCreateUPartyComplete CompletionDelegate)
{
	UE_LOG(LogParty, Display, TEXT("OnCreatePersistentPartyComplete() %s"), ToString(Result));

	if (Result == ECreatePartyCompletionResult::Succeeded)
	{
		OnCreatePesistentPartyCompletedCommon(LocalUserId);
	}

	CompletionDelegate.ExecuteIfBound(LocalUserId, Result);
}

void UParty::OnCreatePesistentPartyCompletedCommon(const FUniqueNetId& LocalUserId)
{
	UWorld* World = GetWorld();
	check(World);

	IOnlinePartyPtr PartyInt = Online::GetPartyInterface(World);
	if (ensure(PartyInt.IsValid()))
	{
		TSharedPtr<const FOnlineParty> Party = PartyInt->GetParty(LocalUserId, IOnlinePartySystem::GetPrimaryPartyTypeId());
		if (ensure(Party.IsValid()))
		{
			PersistentPartyId = Party->PartyId;
		}
	}

	ensure(PersistentPartyId.IsValid());
	UPartyGameState* PersistentParty = GetPersistentParty();
	if (ensure(PersistentParty != nullptr))
	{
		EPartyType PartyType = EPartyType::Public;
		bool bLeaderInvitesOnly = false;
		bool bLeaderFriendsOnly = false;
		bool bAllowInvites = true;
		GetDefaultPersistentPartySettings(PartyType, bLeaderFriendsOnly, bLeaderInvitesOnly, bAllowInvites);

		PersistentParty->SetPartyType(PartyType, bLeaderFriendsOnly, bLeaderInvitesOnly);
		PersistentParty->SetInvitesDisabled(!bAllowInvites);

		TSharedPtr<const FUniqueNetId> PartyLeaderPtr = PersistentParty->GetPartyLeader();
		ensure(PartyLeaderPtr.IsValid());

		FUniqueNetIdRepl PartyLeader(PartyLeaderPtr);
		UpdatePersistentPartyLeader(PartyLeader);
	}
}

void UParty::JoinPersistentParty(const FUniqueNetId& InUserId, const FPartyDetails& InPartyDetails, const UPartyDelegates::FOnJoinUPartyComplete& InCompletionDelegate)
{
	if (PersistentPartyId.IsValid())
	{
		UE_LOG(LogParty, Warning, TEXT("Existing persistent party %s found when joining a new one."), *PersistentPartyId->ToString());
	}

	PersistentPartyId = nullptr;

	UPartyDelegates::FOnJoinUPartyComplete CompletionDelegate;
	CompletionDelegate.BindUObject(this, &ThisClass::OnJoinPersistentPartyComplete, InCompletionDelegate);
	JoinPartyInternal(InUserId, InPartyDetails, CompletionDelegate);
}

void UParty::OnJoinPersistentPartyComplete(const FUniqueNetId& LocalUserId, EJoinPartyCompletionResult Result, int32 DeniedResultCode, UPartyDelegates::FOnJoinUPartyComplete CompletionDelegate)
{
	UWorld* const World = GetWorld();
	IOnlinePartyPtr PartyInt = Online::GetPartyInterface(World);
	if (PartyInt.IsValid())
	{
		if (Result == EJoinPartyCompletionResult::Succeeded)
		{
			TSharedPtr<const FOnlineParty> Party = PartyInt->GetParty(LocalUserId, IOnlinePartySystem::GetPrimaryPartyTypeId());
			if (ensure(Party.IsValid()))
			{
				PersistentPartyId = Party->PartyId;
			}
		}
	}

	const FString PartyIdDebugString = PersistentPartyId.IsValid() ? PersistentPartyId->ToDebugString() : TEXT("Invalid");
	UE_LOG(LogParty, Display, TEXT("OnJoinPersistentPartyComplete() %s %s %d"), *PartyIdDebugString, ToString(Result), DeniedResultCode);

	const int32 OutDeniedResultCode = (Result == EJoinPartyCompletionResult::NotApproved) ? DeniedResultCode : 0;
	CompletionDelegate.ExecuteIfBound(LocalUserId, Result, OutDeniedResultCode);

	if (Result == EJoinPartyCompletionResult::Succeeded)
	{
		ensure(PersistentPartyId.IsValid());
		UPartyGameState* const PersistentParty = GetPersistentParty();
		if (PersistentParty)
		{
			const TSharedPtr<const FUniqueNetId> PartyLeaderPtr = PersistentParty->GetPartyLeader();
			if (PartyLeaderPtr.IsValid())
			{
				const FUniqueNetIdRepl PartyLeader(PartyLeaderPtr);
				UpdatePersistentPartyLeader(PartyLeader);
			}
			else
			{
				UE_LOG(LogParty, Warning, TEXT("OnJoinPersistentPartyComplete [%s]: Failed to update party leader"), *PersistentPartyId->ToString());
			}
		}
		else
		{
			UE_LOG(LogParty, Warning, TEXT("OnJoinPersistentPartyComplete [%s]: Failed to find party state object"), PersistentPartyId.IsValid() ? *PersistentPartyId->ToString() : TEXT("INVALID"));
		}
	}
	else
	{
		if (Result != EJoinPartyCompletionResult::AlreadyJoiningParty)
		{
			if (World)
			{
				// Try to get back to a good state
				HandleJoinPersistentPartyFailure();
			}
		}
		else
		{
			UE_LOG(LogParty, Verbose, TEXT("OnJoinPersistentPartyComplete [%s]: already joining party."), PersistentPartyId.IsValid() ? *PersistentPartyId->ToString() : TEXT("INVALID"));
		}
	}
}

void UParty::HandleJoinPersistentPartyFailure()
{
	RestorePersistentPartyState();
}

void UParty::LeavePersistentParty(const FUniqueNetId& InUserId, const UPartyDelegates::FOnLeaveUPartyComplete& InCompletionDelegate)
{
	if (PersistentPartyId.IsValid())
	{
		if (!bLeavingPersistentParty)
		{
			UE_LOG(LogParty, Verbose, TEXT("LeavePersistentParty %s"), *PersistentPartyId->ToDebugString());

			UPartyDelegates::FOnLeaveUPartyComplete CompletionDelegate;
			CompletionDelegate.BindUObject(this, &ThisClass::OnLeavePersistentPartyComplete, InCompletionDelegate);

			bLeavingPersistentParty = true;
			LeavePartyInternal(InUserId, IOnlinePartySystem::GetPrimaryPartyTypeId(), CompletionDelegate);
		}
		else
		{
			LeavePartyCompleteDelegates.Add(InCompletionDelegate);

			UE_LOG(LogParty, Verbose, TEXT("LeavePersistentParty %d extra delegates"), LeavePartyCompleteDelegates.Num());
		}
	}
	else
	{
		UE_LOG(LogParty, Warning, TEXT("No party during LeavePersistentParty()"));
		InCompletionDelegate.ExecuteIfBound(InUserId, ELeavePartyCompletionResult::UnknownParty);
	}
}

void UParty::OnLeavePersistentPartyComplete(const FUniqueNetId& LocalUserId, const ELeavePartyCompletionResult Result, UPartyDelegates::FOnLeaveUPartyComplete CompletionDelegate)
{
	UE_LOG(LogParty, Display, TEXT("UParty::OnLeavePersistentPartyComplete: %s"), ToString(Result));

	ensure(bLeavingPersistentParty);
	bLeavingPersistentParty = false;
	PersistentPartyId = nullptr;

	CompletionDelegate.ExecuteIfBound(LocalUserId, Result);

	const TArray<UPartyDelegates::FOnLeaveUPartyComplete> DelegatesCopy = MoveTemp(LeavePartyCompleteDelegates);
	LeavePartyCompleteDelegates.Empty();

	// fire delegates for any/all LeavePersistentParty calls made while this one was in flight
	for (const UPartyDelegates::FOnLeaveUPartyComplete& ExtraDelegate : DelegatesCopy)
	{
		ExtraDelegate.ExecuteIfBound(LocalUserId, Result);
	}
}

void UParty::RestorePersistentPartyState()
{
	if (!GIsRequestingExit)
	{
		if (!bLeavingPersistentParty)
		{
			UWorld* const World = GetWorld();
			const IOnlinePartyPtr PartyInt = Online::GetPartyInterface(World);
			if (PartyInt.IsValid())
			{
				UGameInstance* const GameInstance = GetGameInstance();
				check(GameInstance);

				const TSharedPtr<const FUniqueNetId> LocalUserId = GameInstance->GetPrimaryPlayerUniqueId();
				if (LocalUserId.IsValid() && LocalUserId->IsValid())
				{
					UPartyGameState* const PersistentParty = GetPersistentParty();

					// Check for existing party and create a new one if necessary
					const bool bFoundExistingPersistentParty = PersistentParty ? true : false;
					if (bFoundExistingPersistentParty)
					{
						// In a party already, make sure the UI is aware of its state
						if (PersistentParty->ResetForFrontend())
						{
							OnPartyResetForFrontend().Broadcast(PersistentParty);
						}
						else
						{
							// There was an issue resetting the party, so leave 
							LeaveAndRestorePersistentParty();
						}
					}
					else
					{
						PersistentPartyId = nullptr;

						// Create a new party
						CreatePersistentParty(*LocalUserId);
					}
				}
				else
				{
					UE_LOG(LogParty, Log, TEXT("RestorePersistentPartyState: Missing primary player id, ignoring"));
				}
			}
			else
			{
				UE_LOG(LogParty, Log, TEXT("RestorePersistentPartyState: Missing party interface"));
			}
		}
		else
		{
			UE_LOG(LogParty, Log, TEXT("RestorePersistentPartyState: Can't restore while leaving party, ignoring"));
		}
	}
}

void UParty::UpdatePersistentPartyLeader(const FUniqueNetIdRepl& NewPartyLeader)
{
}

bool UParty::IsInParty(TSharedPtr<const FOnlinePartyId>& PartyId) const
{
	bool bFoundParty = false;

	UWorld* World = GetWorld();
	check(World);

	IOnlinePartyPtr PartyInt = Online::GetPartyInterface(World);
	if (PartyInt.IsValid())
	{
		UGameInstance* GameInstance = GetGameInstance();
		check(GameInstance);

		TSharedPtr<const FUniqueNetId> LocalUserId = GameInstance->GetPrimaryPlayerUniqueId();
		if (ensure(LocalUserId.IsValid() && LocalUserId->IsValid()))
		{
			TArray<TSharedRef<const FOnlinePartyId>> LocalJoinedParties;
			PartyInt->GetJoinedParties(*LocalUserId, LocalJoinedParties);
			for (const auto& JoinedParty : LocalJoinedParties)
			{
				if (*JoinedParty == *PartyId)
				{
					bFoundParty = true;
					break;
				}
			}
		}
	}

	return bFoundParty;
}

void UParty::KickFromPersistentParty(const UPartyDelegates::FOnLeaveUPartyComplete& InCompletionDelegate)
{
	UE_LOG(LogParty, Display, TEXT("UParty::KickFromPersistentParty"));
	const TSharedPtr<const FOnlinePartyId> LocalPersistentPartyId = GetPersistentPartyId();
	UPartyGameState* const PersistentParty = GetPersistentParty();
	if (LocalPersistentPartyId.IsValid() && PersistentParty)
	{
		if (PersistentParty->GetPartySize() > 1)
		{
			const UGameInstance* const GameInstance = GetGameInstance();
			check(GameInstance);

			const TSharedPtr<const FUniqueNetId> LocalUserId = GameInstance->GetPrimaryPlayerUniqueId();
			if (ensure(LocalUserId.IsValid() && LocalUserId->IsValid()))
			{
				// Leave the party (restored in frontend)
				LeavePersistentParty(*LocalUserId, InCompletionDelegate);
			}
		}
		else
		{
			// Just block new joining until back in the frontend
			PersistentParty->SetAcceptingMembers(false, EJoinPartyDenialReason::Busy);
		}
	}
}

void UParty::LeaveAndRestorePersistentParty()
{
	if (!bLeavingPersistentParty)
	{
		bLeavingPersistentParty = true;
		UWorld* World = GetWorld();

		FTimerDelegate LeaveAndRestorePersistentPartyNextTick;
		LeaveAndRestorePersistentPartyNextTick.BindUObject(this, &ThisClass::LeaveAndRestorePersistentPartyInternal);
		World->GetTimerManager().SetTimerForNextTick(LeaveAndRestorePersistentPartyNextTick);
	}
	else
	{
		UE_LOG(LogParty, Verbose, TEXT("Already leaving persistent party, ignoring"));
	}
}

void UParty::LeaveAndRestorePersistentPartyInternal()
{
	const UGameInstance* const GameInstance = GetGameInstance();
	check(GameInstance);

	const TSharedPtr<const FUniqueNetId> PrimaryUserId = GameInstance->GetPrimaryPlayerUniqueId();
	
	// Unset this here, LeavePersistentParty requires this to be false.
	// Even if we do not end up calling LeavePersistentParty because PrimaryUserId became invalid, reset because we are not leaving the party
	ensure(bLeavingPersistentParty);
	bLeavingPersistentParty = false;
	
	if (PrimaryUserId.IsValid() && PrimaryUserId->IsValid())
	{
		UPartyDelegates::FOnLeaveUPartyComplete CompletionDelegate;
		CompletionDelegate.BindUObject(this, &ThisClass::OnLeavePersistentPartyAndRestore);

		LeavePersistentParty(*PrimaryUserId, CompletionDelegate);
	}
}

void UParty::OnLeavePersistentPartyAndRestore(const FUniqueNetId& LocalUserId, const ELeavePartyCompletionResult Result)
{
	UE_LOG(LogParty, Display, TEXT("OnLeavePersistentPartyAndRestore Result: %s"), ToString(Result));

	RestorePersistentPartyState();
}

void UParty::AddPendingPartyJoin(const FUniqueNetId& LocalUserId, TSharedRef<const FPartyDetails> PartyDetails, const UPartyDelegates::FOnJoinUPartyComplete& JoinCompleteDelegate)
{
	if (LocalUserId.IsValid() && PartyDetails->IsValid())
	{
		if (!HasPendingPartyJoin())
		{
			PendingPartyJoin = MakeShareable(new FPendingPartyJoin(LocalUserId.AsShared(), PartyDetails, JoinCompleteDelegate));
		}
	}
}

void UParty::ClearPendingPartyJoin()
{
	PendingPartyJoin.Reset();
}

TSharedPtr<const FPartyDetails> UParty::GetPendingPartyJoinDetails() const
{
	if (HasPendingPartyJoin())
	{
		return PendingPartyJoin->PartyDetails;
	}
	return nullptr;
}

bool UParty::ProcessPendingPartyJoin()
{
	if (HasPendingPartyJoin())
	{
		HandlePendingJoin();
		return true;
	}

	return false;
}

UWorld* UParty::GetWorld() const
{
	UGameInstance* GameInstance = Cast<UGameInstance>(GetOuter());
	if (GameInstance)
	{
		return GameInstance->GetWorld();
	}
	
	return nullptr;
}

UGameInstance* UParty::GetGameInstance() const
{
	return Cast<UGameInstance>(GetOuter());
}

#undef LOCTEXT_NAMESPACE
