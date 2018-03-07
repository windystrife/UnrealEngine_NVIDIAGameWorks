// Copyright 2016 Oculus VR, LLC All Rights reserved.

#include "OculusNetDriver.h"
#include "OnlineSubsystemOculusPrivate.h"

#include "IPAddressOculus.h"
#include "OculusNetConnection.h"
#include "OnlineSubsystemOculus.h"
#include "Engine/Engine.h"
#include "Engine/ChildConnection.h"


bool UOculusNetDriver::IsAvailable() const
{
	// Net driver won't work if the online subsystem doesn't exist
	IOnlineSubsystem* OculusSubsystem = IOnlineSubsystem::Get(OCULUS_SUBSYSTEM);
	if (OculusSubsystem)
	{
		return true;
	}
	return false;
}

ISocketSubsystem* UOculusNetDriver::GetSocketSubsystem()
{
	if (bIsPassthrough)
	{
		return UIpNetDriver::GetSocketSubsystem();
	}
	/** Not used */
	return nullptr;
}

bool UOculusNetDriver::InitBase(bool bInitAsClient, FNetworkNotify* InNotify, const FURL& URL, bool bReuseAddressAndPort, FString& Error)
{
	if (bIsPassthrough)
	{
		return UIpNetDriver::InitBase(bInitAsClient, InNotify, URL, bReuseAddressAndPort, Error);
	}

	if (!UNetDriver::InitBase(bInitAsClient, InNotify, URL, bReuseAddressAndPort, Error))
	{
		return false;
	}

	if (InitialConnectTimeout == 0.0)
	{
		UE_LOG(LogNet, Warning, TEXT("InitalConnectTimeout was set to %f"), InitialConnectTimeout);
		InitialConnectTimeout = 120.0;
	}

	if (ConnectionTimeout == 0.0)
	{
		UE_LOG(LogNet, Warning, TEXT("ConnectionTimeout was set to %f"), ConnectionTimeout);
		ConnectionTimeout = 120.0;
	}

	if (KeepAliveTime == 0.0)
	{
		UE_LOG(LogNet, Warning, TEXT("KeepAliveTime was set to %f"), KeepAliveTime);
		KeepAliveTime = 0.2;
	}

	if (SpawnPrioritySeconds == 0.0)
	{
		UE_LOG(LogNet, Warning, TEXT("SpawnPrioritySeconds was set to %f"), SpawnPrioritySeconds);
		SpawnPrioritySeconds = 1.0;
	}

	if (RelevantTimeout == 0.0)
	{
		UE_LOG(LogNet, Warning, TEXT("RelevantTimeout was set to %f"), RelevantTimeout);
		RelevantTimeout = 5.0;
	}

	if (ServerTravelPause == 0.0)
	{
		UE_LOG(LogNet, Warning, TEXT("ServerTravelPause was set to %f"), ServerTravelPause);
		ServerTravelPause = 4.0;
	}

	// Listen for network state
	if (!NetworkingConnectionStateChangeDelegateHandle.IsValid())
	{
		auto OnlineSubsystem = static_cast<FOnlineSubsystemOculus*>(IOnlineSubsystem::Get(OCULUS_SUBSYSTEM));
		NetworkingConnectionStateChangeDelegateHandle =
			OnlineSubsystem->GetNotifDelegate(ovrMessage_Notification_Networking_ConnectionStateChange)
			.AddUObject(this, &UOculusNetDriver::OnNetworkingConnectionStateChange);
	}

	return true;
}

bool UOculusNetDriver::InitConnect(FNetworkNotify* InNotify, const FURL& ConnectURL, FString& Error)
{
	UE_LOG(LogNet, Verbose, TEXT("Connecting to host: %s"), *ConnectURL.ToString(true));

	auto OculusAddr = new FInternetAddrOculus(ConnectURL);
	if (!OculusAddr->IsValid())
	{
		UE_LOG(LogNet, Verbose, TEXT("Init as IPNetDriver connect"));
		bIsPassthrough = true;
		return UIpNetDriver::InitConnect(InNotify, ConnectURL, Error);
	}

	if (!InitBase(true, InNotify, ConnectURL, false, Error))
	{
		return false;
	}

	// Create an unreal connection to the server
	UOculusNetConnection* Connection = NewObject<UOculusNetConnection>(NetConnectionClass);
	check(Connection);
	TSharedRef<FInternetAddr> InternetAddr = MakeShareable(OculusAddr);
	Connection->InitRemoteConnection(this, nullptr, ConnectURL, *InternetAddr, ovr_Net_IsConnected(OculusAddr->GetID()) ? USOCK_Open : USOCK_Pending);

	ServerConnection = Connection;
	Connections.Add(OculusAddr->GetID(), Connection);

	// Connect via OVR_Networking
	ovr_Net_Connect(OculusAddr->GetID());

	// Create the control channel so we can send the Hello message
	ServerConnection->CreateChannel(CHTYPE_Control, true, INDEX_NONE);

	return true;
}

bool UOculusNetDriver::InitListen(FNetworkNotify* InNotify, FURL& LocalURL, bool bReuseAddressAndPort, FString& Error)
{
	if (LocalURL.HasOption(TEXT("bIsLanMatch")))
	{
		UE_LOG(LogNet, Verbose, TEXT("Init as IPNetDriver listen server"));
		bIsPassthrough = true;
		return Super::InitListen(InNotify, LocalURL, bReuseAddressAndPort, Error);
	}

	if (!InitBase(false, InNotify, LocalURL, bReuseAddressAndPort, Error))
	{
		return false;
	}

	// Listen for incoming peers
	if (!PeerConnectRequestDelegateHandle.IsValid())
	{
		auto OnlineSubsystem = static_cast<FOnlineSubsystemOculus*>(IOnlineSubsystem::Get(OCULUS_SUBSYSTEM));
		PeerConnectRequestDelegateHandle =
			OnlineSubsystem->GetNotifDelegate(ovrMessage_Notification_Networking_PeerConnectRequest)
			.AddUObject(this, &UOculusNetDriver::OnNewNetworkingPeerRequest);
	}

	UE_LOG(LogNet, Verbose, TEXT("Init as a listen server"));

	return true;
}

void UOculusNetDriver::TickDispatch(float DeltaTime)
{
	if (bIsPassthrough)
	{
		UIpNetDriver::TickDispatch(DeltaTime);
		return;
	}

	UNetDriver::TickDispatch(DeltaTime);

	// Process all incoming packets.
	for (;;) 
	{
		auto Packet = ovr_Net_ReadPacket();
		if (!Packet) 
		{
			break;
		}
		auto PeerID = ovr_Packet_GetSenderID(Packet);
		auto PacketSize = static_cast<int32>(ovr_Packet_GetSize(Packet));

		if (Connections.Contains(PeerID))
		{
			auto Connection = Connections[PeerID];
			if (Connection->State == EConnectionState::USOCK_Open)
			{
				UE_LOG(LogNet, VeryVerbose, TEXT("Got a raw packet of size %d"), PacketSize);

				auto Data = (uint8 *)ovr_Packet_GetBytes(Packet);
				Connection->ReceivedRawPacket(Data, PacketSize);
			}
			else 
			{
				// This can happen on non-seamless map travels
				UE_LOG(LogNet, Verbose, TEXT("Got a packet but the connection is closed to: %llu"), PeerID);
			}
		}
		else
		{
			UE_LOG(LogNet, Warning, TEXT("There is no connection to: %llu"), PeerID);
		}
		ovr_Packet_Free(Packet);
	}
}

void UOculusNetDriver::OnNewNetworkingPeerRequest(ovrMessageHandle Message, bool bIsError)
{
	auto NetworkingPeer = ovr_Message_GetNetworkingPeer(Message);
	auto PeerID = ovr_NetworkingPeer_GetID(NetworkingPeer);

	if (AddNewClientConnection(PeerID)) 
	{
		// Accept the connection
		UE_LOG(LogNet, Verbose, TEXT("Accepting peer request: %llu"), PeerID);
		ovr_Net_Accept(PeerID);
	}
}

bool UOculusNetDriver::AddNewClientConnection(ovrID PeerID)
{
	// Ignore the peer if not accepting new connections
	if (Notify->NotifyAcceptingConnection() != EAcceptConnection::Accept)
	{
		UE_LOG(LogNet, Warning, TEXT("Not accepting more new connections"));
		return false;
	}

	UE_LOG(LogNet, Verbose, TEXT("New incoming peer request: %llu"), PeerID);

	// Create an unreal connection to the client
	UOculusNetConnection* Connection = NewObject<UOculusNetConnection>(NetConnectionClass);
	check(Connection);
	auto OculusAddr = new FInternetAddrOculus(PeerID);
	TSharedRef<FInternetAddr> InternetAddr = MakeShareable(OculusAddr);
	Connection->InitRemoteConnection(this, nullptr, FURL(), *InternetAddr, ovr_Net_IsConnected(PeerID) ? USOCK_Open : USOCK_Pending);

	AddClientConnection(Connection);

	Connections.Add(PeerID, Connection);

	// Accept the connection
	Notify->NotifyAcceptedConnection(Connection);

	return true;
}

void UOculusNetDriver::OnNetworkingConnectionStateChange(ovrMessageHandle Message, bool bIsError)
{

	auto NetworkingPeer = ovr_Message_GetNetworkingPeer(Message);

	auto PeerID = ovr_NetworkingPeer_GetID(NetworkingPeer);

	auto State = ovr_NetworkingPeer_GetState(NetworkingPeer);

	UE_LOG(LogNet, Verbose, TEXT("%llu changed network connection state"), PeerID);

	if (!Connections.Contains(PeerID))
	{
		UE_LOG(LogNet, Warning, TEXT("Peer ID not found in connections: %llu"), PeerID);
		return;
	}

	auto Connection = Connections[PeerID];
	if (State == ovrPeerState_Connected)
	{
		// Use ovr_Net_IsConnected as the source of truth of the actual connection
		if (ovr_Net_IsConnected(PeerID))
		{
			// Connections in a state of Closed will not have a NetDriver
			// They will hit a nullptr exception if processing packets
			if (Connection->State == EConnectionState::USOCK_Closed)
			{
				UE_LOG(LogNet, Warning, TEXT("Cannot reopen a closed connection to %llu"), PeerID);

				// Better to close the underlying connection if we hit this state
				ovr_Net_Close(PeerID);
			}
			else
			{
				UE_LOG(LogNet, Verbose, TEXT("%llu is connected"), PeerID);
				Connection->State = EConnectionState::USOCK_Open;
			}
		}
		else
		{
			UE_LOG(LogNet, Verbose, TEXT("Notification said %llu is open, but connection is closed.  Ignoring potentially old notification"), PeerID);
		}
	}
	else if (State == ovrPeerState_Closed)
	{
		// Use ovr_Net_IsConnected as the source of truth of the actual connection
		if (!ovr_Net_IsConnected(PeerID))
		{
			if (Connection->State == EConnectionState::USOCK_Pending && !IsServer())
			{
				// Treat the pending case as if the connection timed out and try again
				UE_LOG(LogNet, Verbose, TEXT("Notification said %llu is closed, but connection is still pending.  Ignoring potentially old notification and retry the connection"), PeerID);
				ovr_Net_Connect(PeerID);
			}
			else
			{
				UE_LOG(LogNet, Verbose, TEXT("%llu is closed"), PeerID);
				Connection->State = EConnectionState::USOCK_Closed;
			}
		}
		else
		{
			UE_LOG(LogNet, Verbose, TEXT("Notification said %llu is closed, but connection is still open.  Ignoring potentially old notification"), PeerID);
		}
	}
	else if (State == ovrPeerState_Timeout)
	{
		if (Connection->State == EConnectionState::USOCK_Pending && !IsServer())
		{
			UE_LOG(LogNet, Verbose, TEXT("Retrying connection to %llu"), PeerID);
			ovr_Net_Connect(PeerID);
		}
		else
		{
			UE_LOG(LogNet, Warning, TEXT("%llu timed out"), PeerID);
			Connection->State = EConnectionState::USOCK_Closed;
		}
	}
	else
	{
		UE_LOG(LogNet, Warning, TEXT("%llu is in an unknown state"), PeerID);
	}
}

void UOculusNetDriver::Shutdown()
{
	if (bIsPassthrough)
	{
		UIpNetDriver::Shutdown();
		return;
	}
	UNetDriver::Shutdown();

	UE_LOG(LogNet, Verbose, TEXT("Oculus Net Driver shutdown"));

	auto OnlineSubsystem = static_cast<FOnlineSubsystemOculus*>(IOnlineSubsystem::Get(OCULUS_SUBSYSTEM));
	if (PeerConnectRequestDelegateHandle.IsValid())
	{
		OnlineSubsystem->RemoveNotifDelegate(ovrMessage_Notification_Networking_PeerConnectRequest, PeerConnectRequestDelegateHandle);
		PeerConnectRequestDelegateHandle.Reset();
	}
	if (NetworkingConnectionStateChangeDelegateHandle.IsValid())
	{
		OnlineSubsystem->RemoveNotifDelegate(ovrMessage_Notification_Networking_ConnectionStateChange, NetworkingConnectionStateChangeDelegateHandle);
		NetworkingConnectionStateChangeDelegateHandle.Reset();
	}

	// Ensure all current connections are closed now
	for (auto& Connection : Connections)
	{
		ovrID PeerID = Connection.Key;
		if (ovr_Net_IsConnected(PeerID))
		{
			UE_LOG(LogNet, Verbose, TEXT("Closing open connection to: %llu"), PeerID);
			ovr_Net_Close(PeerID);
		}
	}
}

bool UOculusNetDriver::IsNetResourceValid()
{
	if (bIsPassthrough)
	{
		return UIpNetDriver::IsNetResourceValid();
	}

	if (!IsAvailable())
	{
		return false;
	}

	// The listen server is always available
	if (IsServer())
	{
		return true;
	}

	// The clients need to wait until the connection is established before sending packets
	return ServerConnection->State == EConnectionState::USOCK_Open;
}
