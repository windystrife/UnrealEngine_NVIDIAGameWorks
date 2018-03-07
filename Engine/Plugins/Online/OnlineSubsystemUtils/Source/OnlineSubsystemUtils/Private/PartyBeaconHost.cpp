// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "PartyBeaconHost.h"
#include "Misc/CommandLine.h"
#include "UObject/Package.h"
#include "Engine/NetConnection.h"
#include "OnlineSubsystemTypes.h"
#include "OnlineSubsystemUtils.h"
#include "PartyBeaconClient.h"

APartyBeaconHost::APartyBeaconHost(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer),
	State(NULL),
	bLogoutOnSessionTimeout(true)
{
	ClientBeaconActorClass = APartyBeaconClient::StaticClass();
	BeaconTypeName = ClientBeaconActorClass->GetName();

	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bAllowTickOnDedicatedServer = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
}

void APartyBeaconHost::PostInitProperties()
{
	Super::PostInitProperties();
#if !UE_BUILD_SHIPPING
	// This value is set on the CDO as well on purpose
	bLogoutOnSessionTimeout = FParse::Param(FCommandLine::Get(), TEXT("NoTimeouts")) ? false : true;
#endif
}

bool APartyBeaconHost::InitHostBeacon(int32 InTeamCount, int32 InTeamSize, int32 InMaxReservations, FName InSessionName, int32 InForceTeamNum)
{
	UE_LOG(LogBeacon, Verbose, TEXT("InitHostBeacon TeamCount:%d TeamSize:%d MaxSize:%d"), InTeamCount, InTeamSize, InMaxReservations);
	if (InMaxReservations > 0)
	{
		State = NewObject<UPartyBeaconState>(GetTransientPackage(), GetPartyBeaconHostClass());
		if (State->InitState(InTeamCount, InTeamSize, InMaxReservations, InSessionName, InForceTeamNum))
		{
			return true;
		}
	}

	return false;
}

bool APartyBeaconHost::InitFromBeaconState(UPartyBeaconState* PrevState)
{
	if (!State && PrevState)
	{
		UE_LOG(LogBeacon, Verbose, TEXT("InitFromBeaconState TeamCount:%d TeamSize:%d MaxSize:%d"), PrevState->NumTeams, PrevState->NumPlayersPerTeam, PrevState->MaxReservations);
		State = PrevState;
		return true;
	}

	return false;
}

bool APartyBeaconHost::ReconfigureTeamAndPlayerCount(int32 InNumTeams, int32 InNumPlayersPerTeam, int32 InNumReservations)
{
	bool bSuccess = false;
	if (GetOwner() != NULL && State != NULL)
	{
		bSuccess = State->ReconfigureTeamAndPlayerCount(InNumTeams, InNumPlayersPerTeam, InNumReservations);
		UE_LOG(LogBeacon, Log,
			TEXT("Beacon (%s) reconfiguring team and player count."),
			*GetBeaconType());
	}
	else
	{
		UE_LOG(LogBeacon, Warning,
			TEXT("Beacon (%s) hasn't been initialized yet, can't change team and player count."),
			*GetBeaconType());
	}

	return bSuccess;
}

void APartyBeaconHost::SetTeamAssignmentMethod(FName NewAssignmentMethod)
{
	if (State)
	{
		State->SetTeamAssignmentMethod(NewAssignmentMethod);
	}
	else
	{
		UE_LOG(LogBeacon, Warning, TEXT("SetTeamAssignmentMethod failed for beacon with no state!"));
	}
}

void APartyBeaconHost::Tick(float DeltaTime)
{
	if (State)
	{
		UWorld* World = GetWorld();
		IOnlineSessionPtr SessionInt = Online::GetSessionInterface(World);

		if (SessionInt.IsValid())
		{
			FName SessionName = State->GetSessionName();
			TArray<FPartyReservation>& Reservations = State->GetReservations();

			FNamedOnlineSession* Session = SessionInt->GetNamedSession(SessionName);
			if (Session)
			{
				TArray< TSharedPtr<const FUniqueNetId> > PlayersToLogout;
				for (int32 ResIdx = 0; ResIdx < Reservations.Num(); ResIdx++)
				{
					FPartyReservation& PartyRes = Reservations[ResIdx];

					// Determine if we have a client connection for the reservation 
					bool bIsConnectedReservation = false;
					for (int32 ClientIdx = 0; ClientIdx < ClientActors.Num(); ClientIdx++)
					{
						APartyBeaconClient* Client = Cast<APartyBeaconClient>(ClientActors[ClientIdx]);
						if (Client)
						{
							const FPartyReservation& ClientRes = Client->GetPendingReservation();
							if (ClientRes.PartyLeader == PartyRes.PartyLeader)
							{
								bIsConnectedReservation = true;
								break;
							}
						}
						else
						{
							UE_LOG(LogBeacon, Error, TEXT("Missing PartyBeaconClient found in ClientActors array"));
							ClientActors.RemoveAtSwap(ClientIdx);
							ClientIdx--;
						}
					}

					// Don't update clients that are still connected
					if (bIsConnectedReservation)
					{
						for (int32 PlayerIdx = 0; PlayerIdx < PartyRes.PartyMembers.Num(); PlayerIdx++)
						{
							FPlayerReservation& PlayerEntry = PartyRes.PartyMembers[PlayerIdx];
							PlayerEntry.ElapsedTime = 0.0f;
						}
					}
					// Once a client beacon disconnects, update the elapsed time since they were found as a registrant in the game session
					else
					{
						for (int32 PlayerIdx = 0; PlayerIdx < PartyRes.PartyMembers.Num(); PlayerIdx++)
						{
							FPlayerReservation& PlayerEntry = PartyRes.PartyMembers[PlayerIdx];

							// Determine if the player is the owner of the session	
							const bool bIsSessionOwner = Session->OwningUserId.IsValid() && (*Session->OwningUserId == *PlayerEntry.UniqueId);

							// Determine if the player member is registered in the game session
							if (SessionInt->IsPlayerInSession(SessionName, *PlayerEntry.UniqueId) ||
								// Never timeout the session owner
								bIsSessionOwner)
							{
								FUniqueNetIdMatcher PlayerMatch(*PlayerEntry.UniqueId);
								int32 FoundIdx = State->PlayersPendingJoin.IndexOfByPredicate(PlayerMatch);
								if (FoundIdx != INDEX_NONE)
								{
									UE_LOG(LogBeacon, Display, TEXT("Beacon (%s): pending player %s found in session (%s), removing."),
										*GetName(),
										*(PlayerEntry.UniqueId.ToDebugString()),
										*SessionName.ToString());

									// reset elapsed time since found
									PlayerEntry.ElapsedTime = 0.0f;
									// also remove from pending join list
									State->PlayersPendingJoin.RemoveAtSwap(FoundIdx);
								}
							}
							else
							{
								// update elapsed time
								PlayerEntry.ElapsedTime += DeltaTime;

								if (bLogoutOnSessionTimeout)
								{
									// if the player is pending it's initial join then check against TravelSessionTimeoutSecs instead
									FUniqueNetIdMatcher PlayerMatch(*PlayerEntry.UniqueId);
									const int32 FoundIdx = State->PlayersPendingJoin.IndexOfByPredicate(PlayerMatch);
									const bool bIsPlayerPendingJoin = FoundIdx != INDEX_NONE;
									// if the timeout has been exceeded then add to list of players 
									// that need to be logged out from the beacon
									if ((bIsPlayerPendingJoin && PlayerEntry.ElapsedTime > TravelSessionTimeoutSecs) ||
										(!bIsPlayerPendingJoin && PlayerEntry.ElapsedTime > SessionTimeoutSecs))
									{
										PlayersToLogout.AddUnique(PlayerEntry.UniqueId.GetUniqueNetId());
									}
								}
							}
						}
					}
				}

				if (bLogoutOnSessionTimeout)
				{
					// Logout any players that timed out
					for (int32 LogoutIdx = 0; LogoutIdx < PlayersToLogout.Num(); LogoutIdx++)
					{
						bool bFound = false;
						const TSharedPtr<const FUniqueNetId>& UniqueId = PlayersToLogout[LogoutIdx];
						float ElapsedSessionTime = 0.f;
						for (int32 ResIdx = 0; ResIdx < Reservations.Num(); ResIdx++)
						{
							const FPartyReservation& PartyRes = Reservations[ResIdx];
							for (int32 PlayerIdx = 0; PlayerIdx < PartyRes.PartyMembers.Num(); PlayerIdx++)
							{
								const FPlayerReservation& PlayerEntry = PartyRes.PartyMembers[PlayerIdx];
								if (*PlayerEntry.UniqueId == *UniqueId)
								{
									ElapsedSessionTime = PlayerEntry.ElapsedTime;
									bFound = true;
									break;
								}
							}

							if (bFound)
							{
								break;
							}
						}

						UE_LOG(LogBeacon, Display, TEXT("Beacon (%s): pending player logout due to timeout for %s, elapsed time = %0.3f, removing"),
							*GetName(),
							*UniqueId->ToDebugString(),
							ElapsedSessionTime);
						// Also remove from pending join list
						State->PlayersPendingJoin.RemoveSingleSwap(UniqueId);
						// let the beacon handle the logout and notifications/delegates
						FUniqueNetIdRepl RemovedId(UniqueId);
						HandlePlayerLogout(RemovedId);
					}
				}
			}
		}
	}
}

int32 APartyBeaconHost::GetNumPlayersOnTeam(int32 TeamIdx) const
{
	int32 Result = 0;
	if (GetOwner() != NULL && State != NULL)
	{
		Result = State->GetNumPlayersOnTeam(TeamIdx);
	}
	else
	{
		UE_LOG(LogBeacon, Warning,
			TEXT("Beacon (%s) hasn't been initialized yet, can't get team player count."),
			*GetBeaconType());
	}

	return Result;
}

int32 APartyBeaconHost::GetTeamForCurrentPlayer(const FUniqueNetId& PlayerId) const
{
	int32 TeamNum = INDEX_NONE;
	if (PlayerId.IsValid())
	{
		if (State)
		{
			TeamNum = State->GetTeamForCurrentPlayer(PlayerId);
		}
	}
	else
	{
		UE_LOG(LogBeacon, Display, TEXT("Invalid player when attempting to find team assignment"));
	}

	return TeamNum;
}

int32 APartyBeaconHost::GetPlayersOnTeam(int32 TeamIndex, TArray<FUniqueNetIdRepl>& TeamMembers) const
{
	if (State)
	{
		if (TeamIndex < State->GetNumTeams())
		{
			return State->GetPlayersOnTeam(TeamIndex, TeamMembers);
		}
		else
		{
			UE_LOG(LogBeacon, Warning, TEXT("GetPlayersOnTeam: Invalid team index %d"), TeamIndex);
		}
	}
	else
	{
		UE_LOG(LogBeacon, Warning, TEXT("GetPlayersOnTeam failed for beacon with no state!"));
	}

	return 0;
}

void APartyBeaconHost::SendReservationUpdates()
{
	if (State && ClientActors.Num() > 0)
	{
		int32 NumRemaining = State->GetRemainingReservations();
		int32 MaxReservations = State->GetMaxReservations();
		if (NumRemaining < MaxReservations)
		{
			if (NumRemaining > 0)
			{
				UE_LOG(LogBeacon, Verbose, TEXT("Sending reservation update %d"), NumRemaining);
				for (AOnlineBeaconClient* ClientActor : ClientActors)
				{
					APartyBeaconClient* PartyBeaconClient = Cast<APartyBeaconClient>(ClientActor);
					if (PartyBeaconClient)
					{
						PartyBeaconClient->ClientSendReservationUpdates(NumRemaining);
					}
				}
			}
			else
			{
				UE_LOG(LogBeacon, Verbose, TEXT("Sending reservation full"));
				for (AOnlineBeaconClient* ClientActor : ClientActors)
				{
					APartyBeaconClient* PartyBeaconClient = Cast<APartyBeaconClient>(ClientActor);
					if (PartyBeaconClient)
					{
						PartyBeaconClient->ClientSendReservationFull();
					}
				}
			}
		}
	}
}

void APartyBeaconHost::NewPlayerAdded(const FPlayerReservation& NewPlayer)
{
	if (NewPlayer.UniqueId.IsValid())
	{
		if (State)
		{
			UE_LOG(LogBeacon, Verbose, TEXT("Beacon adding pending player %s"), *NewPlayer.UniqueId.ToDebugString());
			State->PlayersPendingJoin.Add(NewPlayer.UniqueId.GetUniqueNetId());
		}
		else
		{
			UE_LOG(LogBeacon, Warning, TEXT("Beacon skipping PlayersPendingJoin for beacon with no state!"));
		}
	}
	else
	{
		UE_LOG(LogBeacon, Warning, TEXT("Beacon skipping PlayersPendingJoin for invalid player!"));
	}
}

void APartyBeaconHost::NotifyReservationEventNextFrame(FOnReservationUpdate& ReservationEvent)
{
	UWorld* World = GetWorld();
	check(World);

	// Calling this on next tick to protect against re-entrance
	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([ReservationEvent](){ ReservationEvent.ExecuteIfBound(); }));
}

void APartyBeaconHost::HandlePlayerLogout(const FUniqueNetIdRepl& PlayerId)
{
	if (PlayerId.IsValid())
	{
		UE_LOG(LogBeacon, Verbose, TEXT("HandlePlayerLogout %s"), *PlayerId->ToDebugString());

		if (State)
		{
			if (State->RemovePlayer(PlayerId))
			{
				SendReservationUpdates();
				NotifyReservationEventNextFrame(ReservationChanged);
			}
		}
	}
}

bool APartyBeaconHost::SwapTeams(const FUniqueNetIdRepl& PartyLeader, const FUniqueNetIdRepl& OtherPartyLeader)
{
	bool bSuccess = false;
	if (State)
	{
		if (State->SwapTeams(PartyLeader, OtherPartyLeader))
		{
			NotifyReservationEventNextFrame(ReservationChanged);
			bSuccess = true;
		}
	}

	return bSuccess;
}


bool APartyBeaconHost::ChangeTeam(const FUniqueNetIdRepl& PartyLeader, int32 NewTeamNum)
{
	bool bSuccess = false;
	if (State)
	{
		if (State->ChangeTeam(PartyLeader, NewTeamNum))
		{
			NotifyReservationEventNextFrame(ReservationChanged);
			bSuccess = true;
		}
	}

	return bSuccess;
}

bool APartyBeaconHost::PlayerHasReservation(const FUniqueNetId& PlayerId) const
{
	bool bHasReservation = false;
	if (State)
	{
		bHasReservation = State->PlayerHasReservation(PlayerId);
	}
	else
	{
		UE_LOG(LogBeacon, Warning,
			TEXT("Beacon (%s) hasn't been initialized yet, no reservations."),
			*GetBeaconType());
	}

	return bHasReservation;
}

bool APartyBeaconHost::GetPlayerValidation(const FUniqueNetId& PlayerId, FString& OutValidation) const
{
	OutValidation.Empty();

	bool bHasValidation = false;
	if (State)
	{
		bHasValidation = State->GetPlayerValidation(PlayerId, OutValidation);
	}
	else
	{
		UE_LOG(LogBeacon, Warning,
			TEXT("Beacon (%s) hasn't been initialized yet, no validation."),
			*GetBeaconType());
	}

	return bHasValidation;
}

bool APartyBeaconHost::GetPartyLeader(const FUniqueNetIdRepl& InPartyMemberId, FUniqueNetIdRepl& OutPartyLeaderId) const
{
	bool bHasLeader = false;
	if (State)
	{
		bHasLeader = State->GetPartyLeader(InPartyMemberId, OutPartyLeaderId);
	}
	else
	{
		UE_LOG(LogBeacon, Warning,
			TEXT("Beacon (%s) hasn't been initialized yet, no leader can be found."),
			*GetBeaconType());
	}

	return bHasLeader;
}

EPartyReservationResult::Type APartyBeaconHost::AddPartyReservation(const FPartyReservation& ReservationRequest)
{
	EPartyReservationResult::Type Result = EPartyReservationResult::GeneralError;

	if (!State || GetBeaconState() == EBeaconState::DenyRequests)
	{
		return EPartyReservationResult::ReservationDenied;
	}

	if (ReservationRequest.IsValid())
	{
		TArray<FPartyReservation>& Reservations = State->GetReservations();
		const int32 ExistingReservationIdx = State->GetExistingReservation(ReservationRequest.PartyLeader);
		if (ExistingReservationIdx != INDEX_NONE)
		{
			FPartyReservation& ExistingReservation = Reservations[ExistingReservationIdx];
			if (ReservationRequest.PartyMembers.Num() == ExistingReservation.PartyMembers.Num())
			{
				// Verify the reservations are the same
				int32 NumMatchingReservations = 0;
				for (const FPlayerReservation& NewPlayerRes : ReservationRequest.PartyMembers)
				{
					const FPlayerReservation* const PlayerRes = ExistingReservation.PartyMembers.FindByPredicate(
						[NewPlayerRes](const FPlayerReservation& ExistingPlayerRes)
					{
						return NewPlayerRes.UniqueId == ExistingPlayerRes.UniqueId;
					});

					if (PlayerRes)
					{
						NumMatchingReservations++;
					}
				}

				if (NumMatchingReservations == ExistingReservation.PartyMembers.Num())
				{
					for (const FPlayerReservation& NewPlayerRes : ReservationRequest.PartyMembers)
					{
						if (!NewPlayerRes.ValidationStr.IsEmpty())
						{
							FPlayerReservation* const PlayerRes = ExistingReservation.PartyMembers.FindByPredicate(
								[NewPlayerRes](const FPlayerReservation& ExistingPlayerRes)
							{
								return NewPlayerRes.UniqueId == ExistingPlayerRes.UniqueId;
							});

							if (PlayerRes)
							{
								// Update the validation auth strings because they may have changed with a new login 
								PlayerRes->ValidationStr = NewPlayerRes.ValidationStr;
							}
						}
					}

					SendReservationUpdates();

					// Clean up the game entities for these duplicate players
					DuplicateReservation.ExecuteIfBound(ReservationRequest);

					// Add all players back into the pending join list
					for (int32 Count = 0; Count < ReservationRequest.PartyMembers.Num(); Count++)
					{
						NewPlayerAdded(ReservationRequest.PartyMembers[Count]);
					}

					Result = EPartyReservationResult::ReservationDuplicate;
				}
				else
				{
					// Existing reservation doesn't match incoming duplicate reservation
					Result = EPartyReservationResult::IncorrectPlayerCount;
				}
			}
			else
			{
				// Existing reservation doesn't match incoming duplicate reservation
				Result = EPartyReservationResult::IncorrectPlayerCount;
			}
		}
		else
		{
			// Check for players we already have reservations for
			// Keep track of team index for existing members - if we have members on opposing teams, reject this reserveration
			bool bContainsExistingMembers = false;
			int32 ExistingMemberReservationTeamNum = INDEX_NONE;
			for (const FPlayerReservation& PartyMember : ReservationRequest.PartyMembers)
			{
				int32 MemberExistingPartyReservationIdx = State->GetExistingReservationContainingMember(PartyMember.UniqueId);
				if (MemberExistingPartyReservationIdx != INDEX_NONE)
				{
					const FPartyReservation& MemberExistingPartyReservation = Reservations[MemberExistingPartyReservationIdx];
					UE_LOG(LogBeacon, Display, TEXT("APartyBeaconHost::AddPartyReservation: Found existing reservation for party member %s"), *PartyMember.UniqueId.ToString());
					ReservationRequest.Dump();
					MemberExistingPartyReservation.Dump();
					bContainsExistingMembers = true;
				}
				else
				{
					// Is this player in the pending join list?
					FUniqueNetIdMatcher PlayerMatch(*PartyMember.UniqueId);
					int32 FoundIdx = State->PlayersPendingJoin.IndexOfByPredicate(PlayerMatch);
					if (FoundIdx != INDEX_NONE)
					{
						UE_LOG(LogBeacon, Display, TEXT("APartyBeaconHost::AddPartyReservation: Found party member %s in the pending player list"), *PartyMember.UniqueId.ToString());
						ReservationRequest.Dump();
						bContainsExistingMembers = true;
					}
				}
			}

			if (!bContainsExistingMembers)
			{
				if (State->DoesReservationFit(ReservationRequest))
				{
					bool bContinue = true;
					if (ValidatePlayers.IsBound())
					{
						bContinue = ValidatePlayers.Execute(ReservationRequest.PartyMembers);
					}

					if (bContinue)
					{
						if (State->AreTeamsAvailable(ReservationRequest))
						{
							if (State->AddReservation(ReservationRequest))
							{
								// Keep track of newly added players
								for (const FPlayerReservation& PartyMember : ReservationRequest.PartyMembers)
								{
									NewPlayerAdded(PartyMember);
								}

								SendReservationUpdates();

								NotifyReservationEventNextFrame(ReservationChanged);
								if (State->IsBeaconFull())
								{
									NotifyReservationEventNextFrame(ReservationsFull);
								}
								Result = EPartyReservationResult::ReservationAccepted;
							}
							else
							{
								// Something wrong with team assignment
								Result = EPartyReservationResult::IncorrectPlayerCount;
							}
						}
						else
						{
							// New reservation doesn't fit within a team allocation
							Result = EPartyReservationResult::PartyLimitReached;
						}
					}
					else
					{
						// Validate players failed above
						Result = EPartyReservationResult::ReservationDenied_Banned;
					}
				}
				else
				{
					// Reservation doesn't fit (party larger than team size, or not enough space in general)
					Result = EPartyReservationResult::PartyLimitReached;
				}
			}
			else
			{
				// Reservation contains players already accounted for
				// TODO:  This might be able to be smoothed out, main concern now is if we have players in other reservations on different teams or unassigned teams, and trying to keep party together
				Result = EPartyReservationResult::ReservationDenied_ContainsExistingPlayers;
			}
		}
	}
	else
	{
		// Invalid reservation
		Result = EPartyReservationResult::ReservationInvalid;
	}

	return Result;
}

EPartyReservationResult::Type APartyBeaconHost::UpdatePartyReservation(const FPartyReservation& ReservationUpdateRequest)
{
	if (UE_LOG_ACTIVE(LogBeacon, Verbose))
	{
		UE_LOG(LogBeacon, Verbose, TEXT("APartyBeaconHost::UpdatePartyReservation"));
		ReservationUpdateRequest.Dump();
	}
	EPartyReservationResult::Type Result = EPartyReservationResult::GeneralError;

	if (!State || GetBeaconState() == EBeaconState::DenyRequests)
	{
		return EPartyReservationResult::ReservationDenied;
	}

	if (ReservationUpdateRequest.IsValid())
	{
		if (!State->IsBeaconFull())
		{
			const int32 ExistingReservationIdx = State->GetExistingReservation(ReservationUpdateRequest.PartyLeader);
			if (ExistingReservationIdx != INDEX_NONE)
			{
				// Count the number of available slots for the existing reservation's team
				TArray<FPartyReservation>& Reservations = State->GetReservations();
				FPartyReservation& ExistingReservation = Reservations[ExistingReservationIdx];
				const int32 NumTeamMembers = GetNumPlayersOnTeam(ExistingReservation.TeamNum);
				const int32 NumAvailableSlotsOnTeam = FMath::Max<int32>(0, GetMaxPlayersPerTeam() - NumTeamMembers);
				int32 NumPlayersWithExistingReservation = 0;

				// Read the list of new players and remove the ones that have existing reservation entries
				TArray<FPlayerReservation> NewPlayers;
				for (int32 PlayerIdx = 0; PlayerIdx < ReservationUpdateRequest.PartyMembers.Num(); PlayerIdx++)
				{
					const FPlayerReservation& NewPlayerRes = ReservationUpdateRequest.PartyMembers[PlayerIdx];

					const int32 FormerReservationIdx = State->GetExistingReservationContainingMember(NewPlayerRes.UniqueId);
					if (FormerReservationIdx != ExistingReservationIdx)
					{
						// player reservation doesn't exist in this reservation so add it as a new player
						NewPlayers.Add(NewPlayerRes);

						if (FormerReservationIdx != INDEX_NONE)
						{
							++NumPlayersWithExistingReservation;
						}
					}
					else
					{
						// duplicate entry for this player
						UE_LOG(LogBeacon, Log, TEXT("Skipping player %s because they already have a reservation with this party"),
							*NewPlayerRes.UniqueId.ToString());
					}
				}

				// Validate that adding the new party members to this reservation entry still fits within the team size
				if ((NewPlayers.Num() - NumPlayersWithExistingReservation) <= NumAvailableSlotsOnTeam)
				{
					bool bPlayerRemovedFromReservation = false;
					if (NewPlayers.Num() > 0)
					{
						// Copy new player entries into existing reservation
						for (int32 PlayerIdx = 0; PlayerIdx < NewPlayers.Num(); PlayerIdx++)
						{
							const FPlayerReservation& PlayerRes = NewPlayers[PlayerIdx];

							// Remove players that existed in other reservations before adding to this reservation
							if (NumPlayersWithExistingReservation > 0)
							{
								const int32 FormerReservationIdx = State->GetExistingReservationContainingMember(PlayerRes.UniqueId);
								if (FormerReservationIdx != INDEX_NONE)
								{
									FPartyReservation& FormerReservation = Reservations[FormerReservationIdx];
									UE_LOG(LogBeacon, Log, TEXT("APartyBeaconHost::UpdatePartyReservation: Removing player %s from former reservation with leader %s before adding to reservation with leader %s"),
										*PlayerRes.UniqueId.ToString(), *FormerReservation.PartyLeader.ToString(), *ReservationUpdateRequest.PartyLeader.ToString());
									if (UE_LOG_ACTIVE(LogBeacon, Verbose))
									{
										FormerReservation.Dump();
									}
									int32 NumReservationsRemoved = FormerReservation.PartyMembers.RemoveAll([&PlayerRes](const FPlayerReservation& OtherRes)
									{
										return OtherRes.UniqueId == PlayerRes.UniqueId;
									});

									State->NumConsumedReservations -= NumReservationsRemoved;
									UE_LOG(LogBeacon, Verbose, TEXT("APartyBeaconHost::UpdatePartyReservation: Removed %d players, setting NumConsumedReservations to %d"), NumReservationsRemoved, State->NumConsumedReservations);

									if (NumReservationsRemoved != 0)
									{
										bPlayerRemovedFromReservation = true;
										if (FormerReservation.PartyLeader == PlayerRes.UniqueId)
										{
											UE_LOG(LogBeacon, Display, TEXT("APartyBeaconHost::UpdatePartyReservation: Leader removed, finding member to promote"));
											// Try to find a new leader for party reservation that lost its leader
											bool bAnyMemberPromoted = false;
											for (int32 FormerReservationPlayerIdx = 0; FormerReservationPlayerIdx < FormerReservation.PartyMembers.Num(); FormerReservationPlayerIdx++)
											{
												FPlayerReservation& FormerReservationPlayerEntry = FormerReservation.PartyMembers[FormerReservationPlayerIdx];
												if (FormerReservationPlayerEntry.UniqueId != FormerReservation.PartyLeader && 
													FormerReservationPlayerEntry.UniqueId.IsValid() &&
													State->GetExistingReservation(FormerReservationPlayerEntry.UniqueId) == INDEX_NONE)
												{
													// Promote to party leader (for now)
													UE_LOG(LogBeacon, Display, TEXT("APartyBeaconHost::UpdatePartyReservation: Promoting member %s to leader"), *FormerReservationPlayerEntry.UniqueId.ToString());
													FormerReservation.PartyLeader = FormerReservationPlayerEntry.UniqueId;
													bAnyMemberPromoted = true;
													break;
												}
											}
											if (!bAnyMemberPromoted)
											{
												UE_LOG(LogBeacon, Display, TEXT("APartyBeaconHost::UpdatePartyReservation: Failed to find a player to promote to leader"));
											}
											State->SanityCheckReservations(true);
										}
									}
								}

							}
							ExistingReservation.PartyMembers.Add(PlayerRes);
							// Keep track of newly added players
							NewPlayerAdded(PlayerRes);
							State->SanityCheckReservations(true);
						}

						// Update the reservation count before sending the response
						State->NumConsumedReservations += NewPlayers.Num();
						UE_LOG(LogBeacon, Verbose, TEXT("APartyBeaconHost::UpdatePartyReservation: Added %d players, setting NumConsumedReservations to %d"), NewPlayers.Num(), State->NumConsumedReservations);

						// Tell any UI and/or clients that there has been a change in the reservation state
						SendReservationUpdates();

						// Tell the owner that we've received a reservation so the UI can be updated
						NotifyReservationEventNextFrame(ReservationChanged);
						if (State->IsBeaconFull())
						{
							// If we've hit our limit, fire the delegate so the host can do the
							// next step in getting parties together
							NotifyReservationEventNextFrame(ReservationsFull);
						}

						if (bPlayerRemovedFromReservation)
						{
							State->Reservations.RemoveAll([](const FPartyReservation& Reservation)
							{
								bool bEmptyReservation = Reservation.PartyMembers.Num() == 0;
								if (bEmptyReservation)
								{
									UE_LOG(LogBeacon, Log, TEXT("Removing reservation with party leader %s because there are no more members in it"),
										*Reservation.PartyLeader.ToString());
								}
								return bEmptyReservation;
							});
							State->SanityCheckReservations(false);
						}
						
						Result = EPartyReservationResult::ReservationAccepted;
					}
					else
					{
						// Duplicate entries (or zero) so existing reservation not updated
						Result = EPartyReservationResult::ReservationDuplicate;
					}
				}
				else
				{
					// Send an invalid party size response
					Result = EPartyReservationResult::IncorrectPlayerCount;
				}
			}
			else
			{
				// Send a not found reservation response
				Result = EPartyReservationResult::ReservationNotFound;
			}
		}
		else
		{
			// Send a session full response
			Result = EPartyReservationResult::PartyLimitReached;
		}
	}
	else
	{
		// Invalid reservation
		Result = EPartyReservationResult::ReservationInvalid;
	}	

	return Result;
}

EPartyReservationResult::Type APartyBeaconHost::RemovePartyReservation(const FUniqueNetIdRepl& PartyLeader)
{
	if (State && State->RemoveReservation(PartyLeader))
	{
		CancelationReceived.ExecuteIfBound(*PartyLeader);

		SendReservationUpdates();
		NotifyReservationEventNextFrame(ReservationChanged);
		return EPartyReservationResult::ReservationRequestCanceled;
	}
	else
	{
		UE_LOG(LogBeacon, Warning, TEXT("Failed to find reservation to cancel for leader %s:"), PartyLeader.IsValid() ? *PartyLeader->ToString() : TEXT("INVALID") );
		return EPartyReservationResult::ReservationNotFound;
	}
}

void APartyBeaconHost::RegisterAuthTicket(const FUniqueNetIdRepl& InPartyMemberId, const FString& InAuthTicket)
{
	if (State)
	{
		State->RegisterAuthTicket(InPartyMemberId, InAuthTicket);
	}
	else
	{
		UE_LOG(LogBeacon, Warning,
			TEXT("Beacon (%s) hasn't been initialized yet, not able to register auth ticket."),
			*GetBeaconType());
	}
}

void APartyBeaconHost::UpdatePartyLeader(const FUniqueNetIdRepl& InPartyMemberId, const FUniqueNetIdRepl& NewPartyLeaderId)
{
	if (State)
	{
		State->UpdatePartyLeader(InPartyMemberId, NewPartyLeaderId);
	}
	else
	{
		UE_LOG(LogBeacon, Warning,
			TEXT("Beacon (%s) hasn't been initialized yet, not able to update party leader."),
			*GetBeaconType());
	}
}

bool APartyBeaconHost::DoesSessionMatch(const FString& SessionId) const
{
	if (State)
	{
		UWorld* World = GetWorld();
		FName SessionName = State->GetSessionName();

		IOnlineSessionPtr SessionInt = Online::GetSessionInterface(World);
		FNamedOnlineSession* Session = SessionInt.IsValid() ? SessionInt->GetNamedSession(SessionName) : NULL;
		if (Session && Session->SessionInfo.IsValid() && !SessionId.IsEmpty() && Session->SessionInfo->GetSessionId().ToString() == SessionId)
		{
			return true;
		}
	}

	return false;
}

void APartyBeaconHost::ProcessReservationRequest(APartyBeaconClient* Client, const FString& SessionId, const FPartyReservation& ReservationRequest)
{
	UE_LOG(LogBeacon, Verbose, TEXT("ProcessReservationRequest %s SessionId %s PartyLeader: %s PartySize: %d from (%s)"), 
		Client ? *Client->GetName() : TEXT("NULL"), 
		*SessionId,
		ReservationRequest.PartyLeader.IsValid() ? *ReservationRequest.PartyLeader->ToString() : TEXT("INVALID"),
		ReservationRequest.PartyMembers.Num(),
		Client ? *Client->GetNetConnection()->LowLevelDescribe() : TEXT("NULL"));
	if (UE_LOG_ACTIVE(LogBeacon, Verbose))
	{
		ReservationRequest.Dump();
	}

	if (Client)
	{
		EPartyReservationResult::Type Result = EPartyReservationResult::BadSessionId;
		if (DoesSessionMatch(SessionId))
		{
			Result = AddPartyReservation(ReservationRequest);
		}

		UE_LOG(LogBeacon, Verbose, TEXT("ProcessReservationRequest result: %s"), EPartyReservationResult::ToString(Result));
		if (UE_LOG_ACTIVE(LogBeacon, Verbose) &&
			(Result != EPartyReservationResult::ReservationAccepted))
		{
			DumpReservations();
		}

		Client->ClientReservationResponse(Result);
	}
}

void APartyBeaconHost::ProcessReservationUpdateRequest(APartyBeaconClient* Client, const FString& SessionId, const FPartyReservation& ReservationUpdateRequest)
{
	UE_LOG(LogBeacon, Verbose, TEXT("ProcessReservationUpdateRequest %s SessionId %s PartyLeader: %s PartySize: %d from (%s)"),
		Client ? *Client->GetName() : TEXT("NULL"),
		*SessionId,
		ReservationUpdateRequest.PartyLeader.IsValid() ? *ReservationUpdateRequest.PartyLeader->ToString() : TEXT("INVALID"),
		ReservationUpdateRequest.PartyMembers.Num(),
		Client ? *Client->GetNetConnection()->LowLevelDescribe() : TEXT("NULL"));

	if (Client)
	{
		EPartyReservationResult::Type Result = EPartyReservationResult::BadSessionId;
		if (DoesSessionMatch(SessionId))
		{
			Result = UpdatePartyReservation(ReservationUpdateRequest);
		}

		UE_LOG(LogBeacon, Verbose, TEXT("ProcessReservationUpdateRequest result: %s"), EPartyReservationResult::ToString(Result));
		if (UE_LOG_ACTIVE(LogBeacon, Verbose) &&
			(Result != EPartyReservationResult::ReservationAccepted))
		{
			DumpReservations();
			ReservationUpdateRequest.Dump();
		}

		Client->ClientReservationResponse(Result);
	}
}

void APartyBeaconHost::ProcessCancelReservationRequest(APartyBeaconClient* Client, const FUniqueNetIdRepl& PartyLeader)
{
	UE_LOG(LogBeacon, Verbose, TEXT("ProcessCancelReservationRequest %s PartyLeader: %s from (%s)"), 
		Client ? *Client->GetName() : TEXT("NULL"), 
		PartyLeader.IsValid() ? *PartyLeader->ToString() : TEXT("INVALID"),
		Client ? *Client->GetNetConnection()->LowLevelDescribe() : TEXT("NULL"));

	if (Client)
	{
		EPartyReservationResult::Type Result = RemovePartyReservation(PartyLeader);
		UE_LOG(LogBeacon, Verbose, TEXT("ProcessCancelReservationRequest result: %s"), EPartyReservationResult::ToString(Result));
		if (UE_LOG_ACTIVE(LogBeacon, Verbose) &&
			(Result != EPartyReservationResult::ReservationRequestCanceled))
		{
			DumpReservations();
		}

		Client->ClientCancelReservationResponse(Result);
	}
}

void APartyBeaconHost::DumpReservations() const
{
	UE_LOG(LogBeacon, Display, TEXT("Debug info for Beacon: %s"), *GetBeaconType());
	if (State)
	{
		State->DumpReservations();
	}
	UE_LOG(LogBeacon, Display, TEXT(""));
}
