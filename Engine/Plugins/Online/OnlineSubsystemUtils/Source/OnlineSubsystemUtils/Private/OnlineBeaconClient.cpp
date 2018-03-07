// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "OnlineBeaconClient.h"
#include "TimerManager.h"
#include "GameFramework/OnlineReplStructs.h"
#include "OnlineBeaconHostObject.h"
#include "EngineGlobals.h"
#include "Engine/Engine.h"
#include "PacketHandlers/StatelessConnectHandlerComponent.h"
#include "Engine/PackageMapClient.h"
#include "Engine/LocalPlayer.h"
#include "Net/DataChannel.h"
#include "Misc/NetworkVersion.h"

#define BEACON_RPC_TIMEOUT 15.0f

AOnlineBeaconClient::AOnlineBeaconClient(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer),
	BeaconOwner(nullptr),
	BeaconConnection(nullptr),
	ConnectionState(EBeaconConnectionState::Invalid)
{
	NetDriverName = FName(TEXT("BeaconDriverClient"));
	bOnlyRelevantToOwner = true;
}

FString AOnlineBeaconClient::GetBeaconType() const
{
	return GetClass()->GetName();
}

AOnlineBeaconHostObject* AOnlineBeaconClient::GetBeaconOwner() const
{
	return BeaconOwner;
}

void AOnlineBeaconClient::SetBeaconOwner(AOnlineBeaconHostObject* InBeaconOwner)
{
	BeaconOwner = InBeaconOwner;
}

const AActor* AOnlineBeaconClient::GetNetOwner() const
{
	return BeaconOwner;
}

class UNetConnection* AOnlineBeaconClient::GetNetConnection() const
{
	return BeaconConnection;
}

bool AOnlineBeaconClient::DestroyNetworkActorHandled()
{
	if (BeaconConnection && BeaconConnection->State != USOCK_Closed)
	{
		// This will be cleaned up in ~2 sec by UNetConnection Tick
		BeaconConnection->bPendingDestroy = true;
		return true;
	}

	// The UNetConnection is gone or has been closed (NetDriver destroyed) and needs to go away now
	return false;
}

const FUniqueNetIdRepl& AOnlineBeaconClient::GetUniqueId() const
{
	if (BeaconConnection)
	{
		return BeaconConnection->PlayerId;
	}

	static FUniqueNetIdRepl EmptyId;
	return EmptyId;
}

EBeaconConnectionState AOnlineBeaconClient::GetConnectionState() const
{
	return ConnectionState;
}

void AOnlineBeaconClient::SetConnectionState(EBeaconConnectionState NewConnectionState)
{
	ConnectionState = NewConnectionState;
}

bool AOnlineBeaconClient::InitClient(FURL& URL)
{
	bool bSuccess = false;

	if(URL.Valid)
	{
		if (InitBase() && NetDriver)
		{
			FString Error;
			if (NetDriver->InitConnect(this, URL, Error))
			{
				BeaconConnection = NetDriver->ServerConnection;

				// Kick off the connection handshake
				bool bSentHandshake = false;

				if (BeaconConnection->Handler.IsValid())
				{
					BeaconConnection->Handler->BeginHandshaking(
						FPacketHandlerHandshakeComplete::CreateUObject(this, &AOnlineBeaconClient::SendInitialJoin));

					bSentHandshake = true;
				}

				SetConnectionState(EBeaconConnectionState::Pending);

				NetDriver->SetWorld(GetWorld());
				NetDriver->Notify = this;
				NetDriver->InitialConnectTimeout = BeaconConnectionInitialTimeout;
				NetDriver->ConnectionTimeout = BeaconConnectionTimeout;


				if (!bSentHandshake)
				{
					SendInitialJoin();
				}


				bSuccess = true;
			}
			else
			{
				// error initializing the network stack...
				UE_LOG(LogBeacon, Log, TEXT("AOnlineBeaconClient::InitClient failed"));
				SetConnectionState(EBeaconConnectionState::Invalid);
				OnFailure();
			}
		}
	}

	return bSuccess;
}

void AOnlineBeaconClient::SetEncryptionToken(const FString& InEncryptionToken)
{
	EncryptionToken = InEncryptionToken;
}

void AOnlineBeaconClient::SendInitialJoin()
{
	if (ensure(NetDriver != nullptr && NetDriver->ServerConnection != nullptr))
	{
		uint8 IsLittleEndian = uint8(PLATFORM_LITTLE_ENDIAN);
		check(IsLittleEndian == !!IsLittleEndian); // should only be one or zero

		uint32 LocalNetworkVersion = FNetworkVersion::GetLocalNetworkVersion();

		if (CVarNetAllowEncryption.GetValueOnGameThread() == 0)
		{
			EncryptionToken.Reset();
		}

		FNetControlMessage<NMT_Hello>::Send(NetDriver->ServerConnection, IsLittleEndian, LocalNetworkVersion, EncryptionToken);

		NetDriver->ServerConnection->FlushNet();
	}
}

void AOnlineBeaconClient::OnFailure()
{
	UE_LOG(LogBeacon, Verbose, TEXT("Client beacon (%s) connection failure, handling connection timeout."), *GetName());
	SetConnectionState(EBeaconConnectionState::Invalid);
	HostConnectionFailure.ExecuteIfBound();
	Super::OnFailure();
}


/// @cond DOXYGEN_WARNINGS

void AOnlineBeaconClient::ClientOnConnected_Implementation()
{
	SetConnectionState(EBeaconConnectionState::Open);
	BeaconConnection->State = USOCK_Open;

	Role = ROLE_Authority;
	SetReplicates(true);
	SetAutonomousProxy(true);

	// Fail safe for connection to server but no client connection RPC
	GetWorldTimerManager().ClearTimer(TimerHandle_OnFailure);

	// Call the overloaded function for this client class
	OnConnected();
}

/// @endcond

bool AOnlineBeaconClient::UseShortConnectTimeout() const
{
	return ConnectionState == EBeaconConnectionState::Open;
}

void AOnlineBeaconClient::DestroyBeacon()
{
	SetConnectionState(EBeaconConnectionState::Closed);

	UWorld* World = GetWorld();
	if (World)
	{
		// Fail safe for connection to server but no client connection RPC
		GetWorldTimerManager().ClearTimer(TimerHandle_OnFailure);
	}

	Super::DestroyBeacon();
}

void AOnlineBeaconClient::OnNetCleanup(UNetConnection* Connection)
{
	ensure(Connection == BeaconConnection);
	SetConnectionState(EBeaconConnectionState::Closed);

	AOnlineBeaconHostObject* BeaconHostObject = GetBeaconOwner();
	if (BeaconHostObject)
	{
		BeaconHostObject->NotifyClientDisconnected(this);
	}

	BeaconConnection = nullptr;
	Destroy(true);
}

void AOnlineBeaconClient::NotifyControlMessage(UNetConnection* Connection, uint8 MessageType, class FInBunch& Bunch)
{
	if(NetDriver->ServerConnection)
	{
		check(Connection == NetDriver->ServerConnection);

		// We are the client
#if !(UE_BUILD_SHIPPING && WITH_EDITOR)
		UE_LOG(LogBeacon, Log, TEXT("%s[%s] Client received: %s"), *GetName(), *Connection->GetName(), FNetControlMessageInfo::GetName(MessageType));
#endif
		switch (MessageType)
		{
		case NMT_EncryptionAck:
			{
				if (FNetDelegates::OnReceivedNetworkEncryptionAck.IsBound())
				{
					TWeakObjectPtr<UNetConnection> WeakConnection = Connection;
					FNetDelegates::OnReceivedNetworkEncryptionAck.Execute(FOnEncryptionKeyResponse::CreateUObject(this, &ThisClass::FinalizeEncryptedConnection, WeakConnection));
				}
				else
				{
					// Force close the session
					UE_LOG(LogBeacon, Warning, TEXT("%s: No delegate available to handle encryption ack, disconnecting."), *Connection->GetName());
					OnFailure();
				}
				break;
			}
		case NMT_BeaconWelcome:
			{
				Connection->ClientResponse = TEXT("0");
				FNetControlMessage<NMT_Netspeed>::Send(Connection, Connection->CurrentNetSpeed);

				FString BeaconType = GetBeaconType();
				if (!BeaconType.IsEmpty())
				{
					FUniqueNetIdRepl UniqueIdRepl;

					ULocalPlayer* LocalPlayer = GEngine->GetFirstGamePlayer(GetWorld());
					if (LocalPlayer)
					{
						// Send the player unique Id at login
						UniqueIdRepl = LocalPlayer->GetPreferredUniqueNetId();
					}

					FNetControlMessage<NMT_BeaconJoin>::Send(Connection, BeaconType, UniqueIdRepl);
					NetDriver->ServerConnection->FlushNet();
				}
				else
				{
					// Force close the session
					UE_LOG(LogBeacon, Log, TEXT("Beacon close from invalid beacon type"));
					OnFailure();
				}
				break;
			}
		case NMT_BeaconAssignGUID:
			{
				FNetworkGUID NetGUID;
				FNetControlMessage<NMT_BeaconAssignGUID>::Receive(Bunch, NetGUID);
				if (NetGUID.IsValid())
				{
					Connection->Driver->GuidCache->RegisterNetGUID_Client( NetGUID, this );

					FString BeaconType = GetBeaconType();
					FNetControlMessage<NMT_BeaconNetGUIDAck>::Send(Connection, BeaconType);
					// Server will send ClientOnConnected() when it gets this control message

					// Fail safe for connection to server but no client connection RPC
					FTimerDelegate TimerDelegate = FTimerDelegate::CreateUObject(this, &AOnlineBeaconClient::OnFailure);
					GetWorldTimerManager().SetTimer(TimerHandle_OnFailure, TimerDelegate, BEACON_RPC_TIMEOUT, false);
				}
				else
				{
					// Force close the session
					UE_LOG(LogBeacon, Log, TEXT("Beacon close from invalid NetGUID"));
					OnFailure();
				}
				break;
			}
		case NMT_Upgrade:
			{
				// Report mismatch.
				uint32 RemoteNetworkVersion;
				FNetControlMessage<NMT_Upgrade>::Receive(Bunch, RemoteNetworkVersion);
				// Upgrade
				const FString ConnectionError = NSLOCTEXT("Engine", "ClientOutdated", "The match you are trying to join is running an incompatible version of the game.  Please try upgrading your game version.").ToString();
				GEngine->BroadcastNetworkFailure(GetWorld(), NetDriver, ENetworkFailure::OutdatedClient, ConnectionError);
				break;
			}
		case NMT_Failure:
			{
				FString ErrorMsg;
				FNetControlMessage<NMT_Failure>::Receive(Bunch, ErrorMsg);
				if (ErrorMsg.IsEmpty())
				{
					ErrorMsg = NSLOCTEXT("NetworkErrors", "GenericBeaconConnectionFailed", "Beacon Connection Failed.").ToString();
				}

				// Force close the session
				UE_LOG(LogBeacon, Log, TEXT("Beacon close from NMT_Failure %s"), *ErrorMsg);
				OnFailure();
				break;
			}
		case NMT_BeaconJoin:
		case NMT_BeaconNetGUIDAck:
		default:
			{
				// Force close the session
				UE_LOG(LogBeacon, Log, TEXT("Beacon close from unexpected control message"));
				OnFailure();
				break;
			}
		}
	}	
}

void AOnlineBeaconClient::FinalizeEncryptedConnection(const FEncryptionKeyResponse& Response, TWeakObjectPtr<UNetConnection> WeakConnection)
{
	UNetConnection* Connection = WeakConnection.Get();
	if (Connection)
	{
		if (Connection->State != USOCK_Invalid && Connection->State != USOCK_Closed && Connection->Driver)
		{
			if (Response.Response == EEncryptionResponse::Success)
			{
				Connection->EnableEncryptionWithKey(Response.EncryptionKey);
			}
			else
			{
				FString ResponseStr(Lex::ToString(Response.Response));
				UE_LOG(LogBeacon, Warning, TEXT("AOnlineBeaconClient::FinalizeEncryptedConnection: encryption failure [%s] %s"), *ResponseStr, *Response.ErrorMsg);
				OnFailure();
			}
		}
		else
		{
			UE_LOG(LogBeacon, Warning, TEXT("AOnlineBeaconClient::FinalizeEncryptedConnection: connection in invalid state. %s"), *Connection->Describe());
			OnFailure();
		}
	}
	else
	{
		UE_LOG(LogBeacon, Warning, TEXT("AOnlineBeaconClient::FinalizeEncryptedConnection: Connection is null."));
		OnFailure();
	}
}
