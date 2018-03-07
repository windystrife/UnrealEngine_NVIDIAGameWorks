// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "PartyBeaconClient.h"
#include "OnlineSubsystemUtils.h"
#include "PartyBeaconHost.h"

#if !UE_BUILD_SHIPPING
namespace BeaconConsoleVariables
{
	/** Time to delay delegates firing a reservation request response */
	TAutoConsoleVariable<float> CVarDelayReservationResponse(
		TEXT("beacon.DelayReservationResponse"),
		0.0f,
		TEXT("Delay time between received response and notification\n")
		TEXT("Time in secs"),
		ECVF_Default);

	/** Time to delay delegates firing a cancel reservation request response */
	TAutoConsoleVariable<float> CVarDelayCancellationResponse(
		TEXT("beacon.DelayCancellationResponse"),
		0.0f,
		TEXT("Delay time between received cancel response and notification\n")
		TEXT("Time in secs"),
		ECVF_Default);

	/** Time to delay delegates firing a reservation update response */
	TAutoConsoleVariable<float> CVarDelayUpdateResponse(
		TEXT("beacon.DelayUpdateResponse"),
		0.0f,
		TEXT("Delay time between received update response and notification\n")
		TEXT("Time in secs"),
		ECVF_Default);

	/** Time to delay delegates firing a reservation full response */
	TAutoConsoleVariable<float> CVarDelayFullResponse(
		TEXT("beacon.DelayFullResponse"),
		0.0f,
		TEXT("Delay time between received full response and notification\n")
		TEXT("Time in secs"),
		ECVF_Default);
}
#endif

/** Max time to wait for a response from the server for CancelReservation */
#define CANCEL_FAILSAFE 5.0f

APartyBeaconClient::APartyBeaconClient(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer),
	RequestType(EClientRequestType::NonePending),
	bPendingReservationSent(false),
	bCancelReservation(false)
{
}

void APartyBeaconClient::BeginDestroy()
{
	ClearTimers();
	Super::BeginDestroy();
}

void APartyBeaconClient::ClearTimers()
{
	UWorld* World = GetWorld();
	if (World)
	{
		if (PendingResponseTimerHandle.IsValid())
		{
			UE_LOG(LogBeacon, Verbose, TEXT("ClearTimers: Pending reservation response cleared."));
		}

		if (PendingCancelResponseTimerHandle.IsValid())
		{
			UE_LOG(LogBeacon, Verbose, TEXT("ClearTimers: Pending cancel response cleared."));
		}

		if (PendingReservationUpdateTimerHandle.IsValid())
		{
			UE_LOG(LogBeacon, Verbose, TEXT("ClearTimers: Pending reservation update cleared."));
		}

		if (PendingReservationFullTimerHandle.IsValid())
		{
			UE_LOG(LogBeacon, Verbose, TEXT("ClearTimers: Pending reservation full cleared."));
		}

		if (CancelRPCFailsafe.IsValid())
		{
			UE_LOG(LogBeacon, Verbose, TEXT("ClearTimers: Cancel failsafe cleared."));
		}

		FTimerManager& TM = World->GetTimerManager();
		TM.ClearTimer(PendingResponseTimerHandle);
		TM.ClearTimer(PendingCancelResponseTimerHandle);
		TM.ClearTimer(PendingReservationUpdateTimerHandle);
		TM.ClearTimer(PendingReservationFullTimerHandle);
		TM.ClearTimer(CancelRPCFailsafe);

		PendingResponseTimerHandle.Invalidate();
		PendingCancelResponseTimerHandle.Invalidate();
		PendingReservationUpdateTimerHandle.Invalidate();
		PendingReservationFullTimerHandle.Invalidate();
		CancelRPCFailsafe.Invalidate();
	}
}

bool APartyBeaconClient::RequestReservation(const FString& ConnectInfoStr, const FString& InSessionId, const FUniqueNetIdRepl& RequestingPartyLeader, const TArray<FPlayerReservation>& PartyMembers)
{
	bool bSuccess = false;

	FURL ConnectURL(NULL, *ConnectInfoStr, TRAVEL_Absolute);
	if (InitClient(ConnectURL))
	{
		DestSessionId = InSessionId;
		PendingReservation.PartyLeader = RequestingPartyLeader;
		PendingReservation.PartyMembers = PartyMembers;
		bPendingReservationSent = false;
		RequestType = EClientRequestType::ExistingSessionReservation;
		bSuccess = true;
	}
	else
	{
		UE_LOG(LogBeacon, Warning, TEXT("RequestReservation: Failure to init client beacon with %s."), *ConnectURL.ToString());
		RequestType = EClientRequestType::NonePending;
	}

	if (!bSuccess)
	{
		OnFailure();
	}

	return bSuccess;
}

bool APartyBeaconClient::RequestReservation(const FOnlineSessionSearchResult& DesiredHost, const FUniqueNetIdRepl& RequestingPartyLeader, const TArray<FPlayerReservation>& PartyMembers)
{
	bool bSuccess = false;

	if (DesiredHost.IsValid())
	{
		UWorld* World = GetWorld();

		IOnlineSubsystem* OnlineSub = Online::GetSubsystem(World);
		if (OnlineSub)
		{
			IOnlineSessionPtr SessionInt = OnlineSub->GetSessionInterface();
			if (SessionInt.IsValid())
			{
				FString ConnectInfo;
				if (SessionInt->GetResolvedConnectString(DesiredHost, NAME_BeaconPort, ConnectInfo))
				{
					FString SessionId = DesiredHost.Session.SessionInfo->GetSessionId().ToString();
					return RequestReservation(ConnectInfo, SessionId, RequestingPartyLeader, PartyMembers);
				}
			}
		}
	}

	if (!bSuccess)
	{
		OnFailure();
	}

	return bSuccess;
}

bool APartyBeaconClient::RequestReservationUpdate(const FUniqueNetIdRepl& RequestingPartyLeader, const TArray<FPlayerReservation>& PlayersToAdd)
{
	bool bWasStarted = false;

	EBeaconConnectionState MyConnectionState = GetConnectionState();
	if (ensure(MyConnectionState == EBeaconConnectionState::Open))
	{
		RequestType = EClientRequestType::ReservationUpdate;
		PendingReservation.PartyLeader = RequestingPartyLeader;
		PendingReservation.PartyMembers = PlayersToAdd;
		ServerUpdateReservationRequest(DestSessionId, PendingReservation);
		bPendingReservationSent = true;
		bWasStarted = true;
	}

	return bWasStarted;
}

bool APartyBeaconClient::RequestReservationUpdate(const FString& ConnectInfoStr, const FString& InSessionId, const FUniqueNetIdRepl& RequestingPartyLeader, const TArray<FPlayerReservation>& PlayersToAdd)
{
	bool bWasStarted = false;

	if (!ConnectInfoStr.IsEmpty() && !InSessionId.IsEmpty())
	{
		EBeaconConnectionState MyConnectionState = GetConnectionState();
		if (MyConnectionState != EBeaconConnectionState::Open)
		{
			// create a new pending reservation for these players in the same way as a new reservation request
			bWasStarted = RequestReservation(ConnectInfoStr, InSessionId, RequestingPartyLeader, PlayersToAdd);
			if (bWasStarted)
			{
				// Treat this reservation as an update to an existing reservation on the host
				RequestType = EClientRequestType::ReservationUpdate;
			}
		}
		else if (MyConnectionState == EBeaconConnectionState::Open)
		{
			bWasStarted = RequestReservationUpdate(RequestingPartyLeader, PlayersToAdd);
		}
	}
	else
	{
		UE_LOG(LogBeacon, Warning, TEXT("APartyBeaconClient::RequestReservationUpdate: Missing ConnectInfoStr ('%s') or SessionId ('%s')."), *ConnectInfoStr, *InSessionId);
	}

	return bWasStarted;
}

bool APartyBeaconClient::RequestReservationUpdate(const FOnlineSessionSearchResult& DesiredHost, const FUniqueNetIdRepl& RequestingPartyLeader, const TArray<FPlayerReservation>& PlayersToAdd)
{
	bool bWasStarted = false;

	EBeaconConnectionState MyConnectionState = GetConnectionState();
	if (MyConnectionState != EBeaconConnectionState::Open)
	{
		// create a new pending reservation for these players in the same way as a new reservation request
		bWasStarted = RequestReservation(DesiredHost, RequestingPartyLeader, PlayersToAdd);
		if (bWasStarted)
		{
			// Treat this reservation as an update to an existing reservation on the host
			RequestType = EClientRequestType::ReservationUpdate;
		}
	}
	else if (MyConnectionState == EBeaconConnectionState::Open)
	{
		RequestReservationUpdate(RequestingPartyLeader, PlayersToAdd);
	}

	return bWasStarted;
}

void APartyBeaconClient::CancelReservation()
{
	if (ensure(PendingReservation.PartyLeader.IsValid()))
	{
		bCancelReservation = true;

		// Clear out any pending response handling, only the cancel matters
		ClearTimers();

		if (bPendingReservationSent)
		{
			UE_LOG(LogBeacon, Verbose, TEXT("Sending cancel reservation request."));
			ServerCancelReservationRequest(PendingReservation.PartyLeader);

			// In case the server is loading or unresponsive (ie no host beacon)
			FTimerDelegate TimerDelegate;
			TimerDelegate.BindUObject(this, &ThisClass::OnCancelledFailsafe);
			
			UWorld* World = GetWorld();
			check(World);
			World->GetTimerManager().SetTimer(CancelRPCFailsafe, TimerDelegate, CANCEL_FAILSAFE, false);
		}
		else
		{
			UE_LOG(LogBeacon, Verbose, TEXT("Reservation request never sent, no need to send cancelation request."));
			OnCancelledComplete();
		}
	}
	else
	{
		UE_LOG(LogBeacon, Verbose, TEXT("Unable to cancel reservation request with invalid party leader."));
		OnCancelledComplete();
	}
}

void APartyBeaconClient::OnConnected()
{
	if (!bCancelReservation)
	{
		if (RequestType == EClientRequestType::ExistingSessionReservation)
		{
			UE_LOG(LogBeacon, Verbose, TEXT("Party beacon connection established, sending join reservation request."));
			ServerReservationRequest(DestSessionId, PendingReservation);
			bPendingReservationSent = true;
		}
		else if (RequestType == EClientRequestType::ReservationUpdate)
		{
			UE_LOG(LogBeacon, Verbose, TEXT("Party beacon connection established, sending reservation update request."));
			ServerUpdateReservationRequest(DestSessionId, PendingReservation);
			bPendingReservationSent = true;
		}
		else
		{
			UE_LOG(LogBeacon, Warning, TEXT("Failed to handle reservation request type %s"), ToString(RequestType));
			OnFailure();
		}
	}
	else
	{
		UE_LOG(LogBeacon, Verbose, TEXT("Reservation request previously canceled, nothing sent."));
		OnCancelledComplete();
	}
}

void APartyBeaconClient::OnCancelledFailsafe()
{
	ClientCancelReservationResponse_Implementation(EPartyReservationResult::ReservationRequestCanceled);
}
 
void APartyBeaconClient::OnCancelledComplete()
{
	ReservationRequestComplete.ExecuteIfBound(EPartyReservationResult::ReservationRequestCanceled);
	RequestType = EClientRequestType::NonePending;
	bCancelReservation = false;
}

void APartyBeaconClient::OnFailure()
{
	ClearTimers();
	RequestType = EClientRequestType::NonePending;
	Super::OnFailure();
}

/// @cond DOXYGEN_WARNINGS

bool APartyBeaconClient::ServerReservationRequest_Validate(const FString& SessionId, const FPartyReservation& Reservation)
{
	return !SessionId.IsEmpty() && Reservation.PartyLeader.IsValid() && Reservation.PartyMembers.Num() > 0;
}

void APartyBeaconClient::ServerReservationRequest_Implementation(const FString& SessionId, const FPartyReservation& Reservation)
{
	APartyBeaconHost* BeaconHost = Cast<APartyBeaconHost>(GetBeaconOwner());
	if (BeaconHost)
	{
		PendingReservation = Reservation;
		RequestType = EClientRequestType::ExistingSessionReservation;
		BeaconHost->ProcessReservationRequest(this, SessionId, Reservation);
	}
}

bool APartyBeaconClient::ServerUpdateReservationRequest_Validate(const FString& SessionId, const FPartyReservation& ReservationUpdate)
{
	return !SessionId.IsEmpty() && ReservationUpdate.PartyLeader.IsValid() && ReservationUpdate.PartyMembers.Num() > 0;
}

void APartyBeaconClient::ServerUpdateReservationRequest_Implementation(const FString& SessionId, const FPartyReservation& ReservationUpdate)
{
	APartyBeaconHost* BeaconHost = Cast<APartyBeaconHost>(GetBeaconOwner());
	if (BeaconHost)
	{
		PendingReservation = ReservationUpdate;
		RequestType = EClientRequestType::ReservationUpdate;
		BeaconHost->ProcessReservationUpdateRequest(this, SessionId, ReservationUpdate);
	}
}

bool APartyBeaconClient::ServerCancelReservationRequest_Validate(const FUniqueNetIdRepl& PartyLeader)
{
	return true;
}

void APartyBeaconClient::ServerCancelReservationRequest_Implementation(const FUniqueNetIdRepl& PartyLeader)
{
	APartyBeaconHost* BeaconHost = Cast<APartyBeaconHost>(GetBeaconOwner());
	if (BeaconHost)
	{
		bCancelReservation = true;
		BeaconHost->ProcessCancelReservationRequest(this, PartyLeader);
	}
}

void APartyBeaconClient::ClientReservationResponse_Implementation(EPartyReservationResult::Type ReservationResponse)
{
	if (!bCancelReservation)
	{
#if !UE_BUILD_SHIPPING
		const float Rate = BeaconConsoleVariables::CVarDelayReservationResponse.GetValueOnGameThread();
#else
		const float Rate = 0.0f;
#endif
		if (Rate > 0.0f)
		{
			UE_LOG(LogBeacon, Verbose, TEXT("Party beacon response received %s, waiting %fs to notify"), EPartyReservationResult::ToString(ReservationResponse), Rate);

			FTimerDelegate TimerDelegate;
			TimerDelegate.BindLambda([this, ReservationResponse]()
			{
				ProcessReservationResponse(ReservationResponse);
			});

			PendingResponseTimerHandle = DelayResponse(TimerDelegate, Rate);
		}
		else
		{
			ProcessReservationResponse(ReservationResponse);
		}
	}
	else
	{
		UE_LOG(LogBeacon, Verbose, TEXT("Party beacon response received %s, ignored due to cancel in progress"), EPartyReservationResult::ToString(ReservationResponse));
		// Cancel RPC or failsafe timer will trigger the cancel
	}
}

/// @endcond

void APartyBeaconClient::ProcessReservationResponse(EPartyReservationResult::Type ReservationResponse)
{
	if (!bCancelReservation)
	{
		UE_LOG(LogBeacon, Verbose, TEXT("Party beacon response received %s"), EPartyReservationResult::ToString(ReservationResponse));
		ReservationRequestComplete.ExecuteIfBound(ReservationResponse);
		RequestType = EClientRequestType::NonePending;
	}
	else
	{
		UE_LOG(LogBeacon, Verbose, TEXT("Party beacon response received %s, ignored due to cancel in progress"), EPartyReservationResult::ToString(ReservationResponse));
		// Cancel RPC or failsafe timer will trigger the cancel
	}
}

/// @cond DOXYGEN_WARNINGS

void APartyBeaconClient::ClientCancelReservationResponse_Implementation(EPartyReservationResult::Type ReservationResponse)
{
	ensure(bCancelReservation);

	// Clear out any pending response handling (including failsafe timer)
	ClearTimers();
#if !UE_BUILD_SHIPPING	
	const float Rate = BeaconConsoleVariables::CVarDelayCancellationResponse.GetValueOnGameThread();
#else
	const float Rate = 0.0f;
#endif
	if (Rate > 0.0f)
	{
		UE_LOG(LogBeacon, Verbose, TEXT("Party beacon cancellation response received %s, waiting %fs to notify"), EPartyReservationResult::ToString(ReservationResponse), Rate);

		FTimerDelegate TimerDelegate;
		TimerDelegate.BindLambda([this, ReservationResponse]()
		{
			ProcessCancelReservationResponse(ReservationResponse);
		});

		PendingCancelResponseTimerHandle = DelayResponse(TimerDelegate, Rate);
	}
	else
	{
		ProcessCancelReservationResponse(ReservationResponse);
	}
}

/// @endcond

void APartyBeaconClient::ProcessCancelReservationResponse(EPartyReservationResult::Type ReservationResponse)
{
	ensure(ReservationResponse == EPartyReservationResult::ReservationRequestCanceled || ReservationResponse == EPartyReservationResult::ReservationNotFound);
	ensure(bCancelReservation);
	OnCancelledComplete();
}

/// @cond DOXYGEN_WARNINGS

void APartyBeaconClient::ClientSendReservationUpdates_Implementation(int32 NumRemainingReservations)
{
	if (!bCancelReservation)
	{
#if !UE_BUILD_SHIPPING
		const float Rate = BeaconConsoleVariables::CVarDelayUpdateResponse.GetValueOnGameThread();
#else
		const float Rate = 0.0f;
#endif
		if (Rate > 0.0f)
		{
			UE_LOG(LogBeacon, Verbose, TEXT("Party beacon reservations remaining %d, waiting %fs to notify"), NumRemainingReservations, Rate);

			FTimerDelegate TimerDelegate;
			TimerDelegate.BindLambda([this, NumRemainingReservations]()
			{
				ProcessReservationUpdate(NumRemainingReservations);
			});

			PendingReservationUpdateTimerHandle = DelayResponse(TimerDelegate, Rate);
		}
		else
		{
			ProcessReservationUpdate(NumRemainingReservations);
		}
	}
}

/// @endcond

void APartyBeaconClient::ProcessReservationUpdate(int32 NumRemainingReservations)
{
	UE_LOG(LogBeacon, Verbose, TEXT("Party beacon reservations remaining %d"), NumRemainingReservations);
	ReservationCountUpdate.ExecuteIfBound(NumRemainingReservations);
}

/// @cond DOXYGEN_WARNINGS

void APartyBeaconClient::ClientSendReservationFull_Implementation()
{
	if (!bCancelReservation)
	{
#if !UE_BUILD_SHIPPING
		const float Rate = BeaconConsoleVariables::CVarDelayFullResponse.GetValueOnGameThread();
#else
		const float Rate = 0.0f;
#endif
		if (Rate > 0.0f)
		{
			UE_LOG(LogBeacon, Verbose, TEXT("Party beacon reservations full, waiting %fs to notify"), Rate);

			FTimerDelegate TimerDelegate;
			TimerDelegate.BindLambda([this]()
			{
				ProcessReservationFull();
			});

			PendingReservationFullTimerHandle = DelayResponse(TimerDelegate, Rate);
		}
		else
		{
			ProcessReservationFull();
		}
	}
}

/// @endcond

void APartyBeaconClient::ProcessReservationFull()
{
	UE_LOG(LogBeacon, Verbose, TEXT("Party beacon reservations full"));
	ReservationFull.ExecuteIfBound();
}

FTimerHandle APartyBeaconClient::DelayResponse(FTimerDelegate& Delegate, float Delay)
{
	FTimerHandle TimerHandle;

	UWorld* World = GetWorld();
	if (ensure(World != nullptr))
	{
		World->GetTimerManager().SetTimer(TimerHandle, Delegate, Delay, false);
	}

	return TimerHandle;
}
