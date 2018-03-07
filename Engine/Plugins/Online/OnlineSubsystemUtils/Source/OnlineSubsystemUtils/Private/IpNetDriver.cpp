// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	IpNetDriver.cpp: Unreal IP network driver.
Notes:
	* See \msdev\vc98\include\winsock.h and \msdev\vc98\include\winsock2.h 
	  for Winsock WSAE* errors returned by Windows Sockets.
=============================================================================*/

#include "IpNetDriver.h"
#include "Misc/CommandLine.h"
#include "EngineGlobals.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "UObject/Package.h"
#include "PacketHandlers/StatelessConnectHandlerComponent.h"
#include "Engine/NetConnection.h"
#include "Engine/ChildConnection.h"
#include "SocketSubsystem.h"
#include "IpConnection.h"

#include "PacketAudit.h"

#include "IPAddress.h"
#include "Sockets.h"

/** For backwards compatibility with the engine stateless connect code */
#ifndef STATELESSCONNECT_HAS_RANDOM_SEQUENCE
	#define STATELESSCONNECT_HAS_RANDOM_SEQUENCE 0
#endif

/*-----------------------------------------------------------------------------
	Declarations.
-----------------------------------------------------------------------------*/

DECLARE_CYCLE_STAT(TEXT("IpNetDriver Add new connection"), Stat_IpNetDriverAddNewConnection, STATGROUP_Net);

UIpNetDriver::FOnNetworkProcessingCausingSlowFrame UIpNetDriver::OnNetworkProcessingCausingSlowFrame;

// Time before the alarm delegate is called (in seconds)
float GIpNetDriverMaxDesiredTimeSliceBeforeAlarmSecs = 1.0f;

FAutoConsoleVariableRef GIpNetDriverMaxDesiredTimeSliceBeforeAlarmSecsCVar(
	TEXT("n.IpNetDriverMaxFrameTimeBeforeAlert"),
	GIpNetDriverMaxDesiredTimeSliceBeforeAlarmSecs,
	TEXT("Time to spend processing networking data in a single frame before an alert is raised (in seconds)\n")
	TEXT("It may get called multiple times in a single frame if additional processing after a previous alert exceeds the threshold again\n")
	TEXT(" default: 1 s"));

// Time before the time taken in a single frame is printed out (in seconds)
float GIpNetDriverLongFramePrintoutThresholdSecs = 10.0f;

FAutoConsoleVariableRef GIpNetDriverLongFramePrintoutThresholdSecsCVar(
	TEXT("n.IpNetDriverMaxFrameTimeBeforeLogging"),
	GIpNetDriverLongFramePrintoutThresholdSecs,
	TEXT("Time to spend processing networking data in a single frame before an output log warning is printed (in seconds)\n")
	TEXT(" default: 10 s"));

UIpNetDriver::UIpNetDriver(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, ServerDesiredSocketReceiveBufferBytes(0x20000)
	, ServerDesiredSocketSendBufferBytes(0x20000)
	, ClientDesiredSocketReceiveBufferBytes(0x8000)
	, ClientDesiredSocketSendBufferBytes(0x8000)
{
}

bool UIpNetDriver::IsAvailable() const
{
	// IP driver always valid for now
	return true;
}

ISocketSubsystem* UIpNetDriver::GetSocketSubsystem()
{
	return ISocketSubsystem::Get();
}

FSocket * UIpNetDriver::CreateSocket()
{
	// Create UDP socket and enable broadcasting.
	ISocketSubsystem* SocketSubsystem = GetSocketSubsystem();

	if (SocketSubsystem == NULL)
	{
		UE_LOG(LogNet, Warning, TEXT("UIpNetDriver::CreateSocket: Unable to find socket subsystem"));
		return NULL;
	}

	return SocketSubsystem->CreateSocket( NAME_DGram, TEXT( "Unreal" ) );
}

int UIpNetDriver::GetClientPort()
{
	return 0;
}

bool UIpNetDriver::InitBase( bool bInitAsClient, FNetworkNotify* InNotify, const FURL& URL, bool bReuseAddressAndPort, FString& Error )
{
	if (!Super::InitBase(bInitAsClient, InNotify, URL, bReuseAddressAndPort, Error))
	{
		return false;
	}

	ISocketSubsystem* SocketSubsystem = GetSocketSubsystem();
	if (SocketSubsystem == NULL)
	{
		UE_LOG(LogNet, Warning, TEXT("Unable to find socket subsystem"));
		return false;
	}

	// Derived types may have already allocated a socket

	// Create the socket that we will use to communicate with
	Socket = CreateSocket();

	if( Socket == NULL )
	{
		Socket = 0;
		Error = FString::Printf( TEXT("WinSock: socket failed (%i)"), (int32)SocketSubsystem->GetLastErrorCode() );
		return false;
	}
	if (SocketSubsystem->RequiresChatDataBeSeparate() == false &&
		Socket->SetBroadcast() == false)
	{
		Error = FString::Printf( TEXT("%s: setsockopt SO_BROADCAST failed (%i)"), SocketSubsystem->GetSocketAPIName(), (int32)SocketSubsystem->GetLastErrorCode() );
		return false;
	}

	if (Socket->SetReuseAddr(bReuseAddressAndPort) == false)
	{
		UE_LOG(LogNet, Log, TEXT("setsockopt with SO_REUSEADDR failed"));
	}

	if (Socket->SetRecvErr() == false)
	{
		UE_LOG(LogNet, Log, TEXT("setsockopt with IP_RECVERR failed"));
	}

	// Increase socket queue size, because we are polling rather than threading
	// and thus we rely on the OS socket to buffer a lot of data.
	int32 RecvSize = bInitAsClient ? ClientDesiredSocketReceiveBufferBytes	: ServerDesiredSocketReceiveBufferBytes;
	int32 SendSize = bInitAsClient ? ClientDesiredSocketSendBufferBytes		: ServerDesiredSocketSendBufferBytes;
	Socket->SetReceiveBufferSize(RecvSize,RecvSize);
	Socket->SetSendBufferSize(SendSize,SendSize);
	UE_LOG(LogInit, Log, TEXT("%s: Socket queue %i / %i"), SocketSubsystem->GetSocketAPIName(), RecvSize, SendSize );

	// Bind socket to our port.
	LocalAddr = SocketSubsystem->GetLocalBindAddr(*GLog);
	
	LocalAddr->SetPort(bInitAsClient ? GetClientPort() : URL.Port);
	
	int32 AttemptPort = LocalAddr->GetPort();
	int32 BoundPort = SocketSubsystem->BindNextPort( Socket, *LocalAddr, MaxPortCountToTry + 1, 1 );
	if (BoundPort == 0)
	{
		Error = FString::Printf( TEXT("%s: binding to port %i failed (%i)"), SocketSubsystem->GetSocketAPIName(), AttemptPort,
			(int32)SocketSubsystem->GetLastErrorCode() );
		return false;
	}
	if( Socket->SetNonBlocking() == false )
	{
		Error = FString::Printf( TEXT("%s: SetNonBlocking failed (%i)"), SocketSubsystem->GetSocketAPIName(),
			(int32)SocketSubsystem->GetLastErrorCode());
		return false;
	}

	// Success.
	return true;
}

bool UIpNetDriver::InitConnect( FNetworkNotify* InNotify, const FURL& ConnectURL, FString& Error )
{
	if( !InitBase( true, InNotify, ConnectURL, false, Error ) )
	{
		UE_LOG(LogNet, Warning, TEXT("Failed to init net driver ConnectURL: %s: %s"), *ConnectURL.ToString(), *Error);
		return false;
	}

	// Create new connection.
	ServerConnection = NewObject<UNetConnection>(GetTransientPackage(), NetConnectionClass);
	ServerConnection->InitLocalConnection( this, Socket, ConnectURL, USOCK_Pending);
	UE_LOG(LogNet, Log, TEXT("Game client on port %i, rate %i"), ConnectURL.Port, ServerConnection->CurrentNetSpeed );

	// Create channel zero.
	GetServerConnection()->CreateChannel( CHTYPE_Control, 1, 0 );

	return true;
}

bool UIpNetDriver::InitListen( FNetworkNotify* InNotify, FURL& LocalURL, bool bReuseAddressAndPort, FString& Error )
{
	if( !InitBase( false, InNotify, LocalURL, bReuseAddressAndPort, Error ) )
	{
		UE_LOG(LogNet, Warning, TEXT("Failed to init net driver ListenURL: %s: %s"), *LocalURL.ToString(), *Error);
		return false;
	}


	InitConnectionlessHandler();

	// Update result URL.
	//LocalURL.Host = LocalAddr->ToString(false);
	LocalURL.Port = LocalAddr->GetPort();
	UE_LOG(LogNet, Log, TEXT("%s IpNetDriver listening on port %i"), *GetDescription(), LocalURL.Port );

	return true;
}

void UIpNetDriver::TickDispatch( float DeltaTime )
{
	Super::TickDispatch( DeltaTime );

	// Set the context on the world for this driver's level collection.
	const int32 FoundCollectionIndex = World ? World->GetLevelCollections().IndexOfByPredicate([this](const FLevelCollection& Collection)
	{
		return Collection.GetNetDriver() == this;
	}) : INDEX_NONE;

	FScopedLevelCollectionContextSwitch LCSwitch(FoundCollectionIndex, World);

	ISocketSubsystem* SocketSubsystem = GetSocketSubsystem();

	const double StartReceiveTime = FPlatformTime::Seconds();
	double AlarmTime = StartReceiveTime + GIpNetDriverMaxDesiredTimeSliceBeforeAlarmSecs;

	// Process all incoming packets.
	uint8 Data[MAX_PACKET_SIZE];
	uint8* DataRef = Data;
	TSharedRef<FInternetAddr> FromAddr = SocketSubsystem->CreateInternetAddr();

	for( ; Socket != NULL; )
	{
		{
			const double CurrentTime = FPlatformTime::Seconds();
			if (CurrentTime > AlarmTime)
			{
				OnNetworkProcessingCausingSlowFrame.Broadcast();

				AlarmTime = CurrentTime + GIpNetDriverMaxDesiredTimeSliceBeforeAlarmSecs;
			}
		}

		int32 BytesRead = 0;

		// Get data, if any.
		CLOCK_CYCLES(RecvCycles);
		bool bOk = Socket->RecvFrom(Data, sizeof(Data), BytesRead, *FromAddr);
		UNCLOCK_CYCLES(RecvCycles);

		if (bOk)
		{
			// Immediately stop processing, for empty packets (usually a DDoS)
			if (BytesRead == 0)
			{
				break;
			}

			FPacketAudit::NotifyLowLevelReceive(DataRef, BytesRead);
		}
		else
		{
			ESocketErrors Error = SocketSubsystem->GetLastErrorCode();
			if(Error == SE_EWOULDBLOCK ||
			   Error == SE_NO_ERROR)
			{
				// No data or no error?
				break;
			}
			else
			{
				// MalformedPacket: Client tried sending a packet that exceeded the maximum packet limit
				// enforced by the server
				if (Error == SE_EMSGSIZE)
				{
					UIpConnection* Connection = nullptr;
					if (GetServerConnection() && (*GetServerConnection()->RemoteAddr == *FromAddr))
					{
						Connection = GetServerConnection();
					}

					if (Connection != nullptr)
					{
						UE_SECURITY_LOG(Connection, ESecurityEvent::Malformed_Packet, TEXT("Received Packet with bytes > max MTU"));
					}
				}

				if( Error != SE_ECONNRESET && Error != SE_UDP_ERR_PORT_UNREACH )
				{
					UE_LOG(LogNet, Warning, TEXT("UDP recvfrom error: %i (%s) from %s"),
						(int32)Error,
						SocketSubsystem->GetSocketError(Error),
						*FromAddr->ToString(true));
					break;
				}
			}
		}
		// Figure out which socket the received data came from.
		UIpConnection* Connection = nullptr;
		UIpConnection* MyServerConnection = GetServerConnection();
		if (MyServerConnection)
		{
			if ((*MyServerConnection->RemoteAddr == *FromAddr))
			{
				Connection = MyServerConnection;
			}
			else
			{
				UE_LOG(LogNet, Warning, TEXT("Incoming ip address doesn't match expected server address: Actual: %s Expected: %s"),
					*FromAddr->ToString(true),
					MyServerConnection->RemoteAddr.IsValid() ? *MyServerConnection->RemoteAddr->ToString(true) : TEXT("Invalid"));
			}
		}
		for( int32 i=0; i<ClientConnections.Num() && !Connection; i++ )
		{
			UIpConnection* TestConnection = (UIpConnection*)ClientConnections[i]; 
            check(TestConnection);
			if(*TestConnection->RemoteAddr == *FromAddr)
			{
				Connection = TestConnection;
			}
		}

		if( bOk == false )
		{
			if( Connection )
			{
				if( Connection != GetServerConnection() )
				{
					// We received an ICMP port unreachable from the client, meaning the client is no longer running the game
					// (or someone is trying to perform a DoS attack on the client)

					// rcg08182002 Some buggy firewalls get occasional ICMP port
					// unreachable messages from legitimate players. Still, this code
					// will drop them unceremoniously, so there's an option in the .INI
					// file for servers with such flakey connections to let these
					// players slide...which means if the client's game crashes, they
					// might get flooded to some degree with packets until they timeout.
					// Either way, this should close up the usual DoS attacks.
					if ((Connection->State != USOCK_Open) || (!AllowPlayerPortUnreach))
					{
						if (LogPortUnreach)
						{
							UE_LOG(LogNet, Log, TEXT("Received ICMP port unreachable from client %s.  Disconnecting."),
								*FromAddr->ToString(true));
						}
						Connection->CleanUp();
					}
				}
			}
			else
			{
				if (LogPortUnreach)
				{
					UE_LOG(LogNet, Log, TEXT("Received ICMP port unreachable from %s.  No matching connection found."),
						*FromAddr->ToString(true));
				}
			}
		}
		else
		{
			bool bIgnorePacket = false;

			// If we didn't find a client connection, maybe create a new one.
			if( !Connection )
			{
				// Determine if allowing for client/server connections
				const bool bAcceptingConnection = Notify != nullptr && Notify->NotifyAcceptingConnection() == EAcceptConnection::Accept;

				if (bAcceptingConnection)
				{
					UE_LOG( LogNet, Log, TEXT( "NotifyAcceptingConnection accepted from: %s" ), *FromAddr->ToString( true ) );

					bool bPassedChallenge = false;
					TSharedPtr<StatelessConnectHandlerComponent> StatelessConnect;

					bIgnorePacket = true;

					if (ConnectionlessHandler.IsValid() && StatelessConnectComponent.IsValid())
					{
						StatelessConnect = StatelessConnectComponent.Pin();
						FString IncomingAddress = FromAddr->ToString(true);

						const ProcessedPacket UnProcessedPacket =
												ConnectionlessHandler->IncomingConnectionless(IncomingAddress, DataRef, BytesRead);

						bPassedChallenge = !UnProcessedPacket.bError && StatelessConnect->HasPassedChallenge(IncomingAddress);

						if (bPassedChallenge)
						{
							BytesRead = FMath::DivideAndRoundUp(UnProcessedPacket.CountBits, 8);

							if (BytesRead > 0)
							{
								DataRef = UnProcessedPacket.Data;
								bIgnorePacket = false;
							}
						}
					}
#if !UE_BUILD_SHIPPING
					else if (FParse::Param(FCommandLine::Get(), TEXT("NoPacketHandler")))
					{
						UE_LOG(LogNet, Log, TEXT("Accepting connection without handshake, due to '-NoPacketHandler'."))

						bIgnorePacket = false;
						bPassedChallenge = true;
					}
#endif
					else
					{
						UE_LOG(LogNet, Log,
								TEXT("Invalid ConnectionlessHandler (%i) or StatelessConnectComponent (%i); can't accept connections."),
								(int32)(ConnectionlessHandler.IsValid()), (int32)(StatelessConnectComponent.IsValid()));
					}

					if (bPassedChallenge)
					{
						SCOPE_CYCLE_COUNTER(Stat_IpNetDriverAddNewConnection);

						UE_LOG(LogNet, Log, TEXT("Server accepting post-challenge connection from: %s"), *FromAddr->ToString(true));

						Connection = NewObject<UIpConnection>(GetTransientPackage(), NetConnectionClass);
						check(Connection);

#if STATELESSCONNECT_HAS_RANDOM_SEQUENCE
						// Set the initial packet sequence from the handshake data
						if (StatelessConnect.IsValid())
						{
							int32 ServerSequence = 0;
							int32 ClientSequence = 0;

							StatelessConnect->GetChallengeSequence(ServerSequence, ClientSequence);

							Connection->InitSequence(ClientSequence, ServerSequence);
						}
#endif

						Connection->InitRemoteConnection( this, Socket,  FURL(), *FromAddr, USOCK_Open);

						if (Connection->Handler.IsValid())
						{
							Connection->Handler->BeginHandshaking();
						}

						Notify->NotifyAcceptedConnection( Connection );
						AddClientConnection(Connection);
					}
					else
					{
						UE_LOG( LogNet, VeryVerbose, TEXT( "Server failed post-challenge connection from: %s" ), *FromAddr->ToString( true ) );
					}
				}
				else
				{
					UE_LOG( LogNet, VeryVerbose, TEXT( "NotifyAcceptingConnection denied from: %s" ), *FromAddr->ToString( true ) );
				}
			}

			// Send the packet to the connection for processing.
			if (Connection && !bIgnorePacket)
			{
				Connection->ReceivedRawPacket( DataRef, BytesRead );
			}
		}
	}

	const double EndReceiveTime = FPlatformTime::Seconds();
	const float DeltaReceiveTime = EndReceiveTime - StartReceiveTime;

	if (DeltaReceiveTime > GIpNetDriverLongFramePrintoutThresholdSecs)
	{
		UE_LOG( LogNet, Warning, TEXT( "UIpNetDriver::TickDispatch: Took too long to receive packets. Time: %2.2f %s" ), DeltaReceiveTime, *GetName() );
	}
}

void UIpNetDriver::LowLevelSend(FString Address, void* Data, int32 CountBits)
{
	bool bValidAddress = !Address.IsEmpty();
	TSharedRef<FInternetAddr> RemoteAddr = GetSocketSubsystem()->CreateInternetAddr();

	if (bValidAddress)
	{
		RemoteAddr->SetIp(*Address, bValidAddress);
	}

	if (bValidAddress)
	{
		const uint8* DataToSend = reinterpret_cast<uint8*>(Data);

		if (ConnectionlessHandler.IsValid())
		{
			const ProcessedPacket ProcessedData =
					ConnectionlessHandler->OutgoingConnectionless(Address, (uint8*)DataToSend, CountBits);

			if (!ProcessedData.bError)
			{
				DataToSend = ProcessedData.Data;
				CountBits = ProcessedData.CountBits;
			}
			else
			{
				CountBits = 0;
			}
		}


		int32 BytesSent = 0;
		uint32 CountBytes = FMath::DivideAndRoundUp(CountBits, 8);

		if (CountBits > 0)
		{
			CLOCK_CYCLES(SendCycles);
			Socket->SendTo(DataToSend, FMath::DivideAndRoundUp(CountBits, 8), BytesSent, *RemoteAddr);
			UNCLOCK_CYCLES(SendCycles);
		}


		// @todo: Can't implement these profiling events (require UNetConnections)
		//NETWORK_PROFILER(GNetworkProfiler.FlushOutgoingBunches(/* UNetConnection */));
		//NETWORK_PROFILER(GNetworkProfiler.TrackSocketSendTo(Socket->GetDescription(),Data,BytesSent,NumPacketIdBits,NumBunchBits,
							//NumAckBits,NumPaddingBits, /* UNetConnection */));
	}
	else
	{
		UE_LOG(LogNet, Warning, TEXT("UIpNetDriver::LowLevelSend: Invalid send address '%s'"), *Address);
	}
}

void UIpNetDriver::ProcessRemoteFunction(class AActor* Actor, UFunction* Function, void* Parameters, FOutParmRec* OutParms, FFrame* Stack, class UObject* SubObject )
{
#if !UE_BUILD_SHIPPING
	bool bBlockSendRPC = false;

	SendRPCDel.ExecuteIfBound(Actor, Function, Parameters, OutParms, Stack, SubObject, bBlockSendRPC);

	if (!bBlockSendRPC)
#endif
	{
		bool bIsServer = IsServer();

		UNetConnection* Connection = NULL;
		if (bIsServer)
		{
			if ((Function->FunctionFlags & FUNC_NetMulticast))
			{
				// Multicast functions go to every client
				TArray<UNetConnection*> UniqueRealConnections;
				for (int32 i=0; i<ClientConnections.Num(); ++i)
				{
					Connection = ClientConnections[i];
					if (Connection && Connection->ViewTarget)
					{
						// Do relevancy check if unreliable.
						// Reliables will always go out. This is odd behavior. On one hand we wish to guarantee "reliables always get there". On the other
						// hand, replicating a reliable to something on the other side of the map that is non relevant seems weird. 
						//
						// Multicast reliables should probably never be used in gameplay code for actors that have relevancy checks. If they are, the 
						// rpc will go through and the channel will be closed soon after due to relevancy failing.

						bool IsRelevant = true;
						if ((Function->FunctionFlags & FUNC_NetReliable) == 0)
						{
							FNetViewer Viewer(Connection, 0.f);
							IsRelevant = Actor->IsNetRelevantFor(Viewer.InViewer, Viewer.ViewTarget, Viewer.ViewLocation);
						}
					
						if (IsRelevant)
						{
							if (Connection->GetUChildConnection() != NULL)
							{
								Connection = ((UChildConnection*)Connection)->Parent;
							}
						
							InternalProcessRemoteFunction( Actor, SubObject, Connection, Function, Parameters, OutParms, Stack, bIsServer );
						}
					}
				}			

				// Replicate any RPCs to the replay net driver so that they can get saved in network replays
				UNetDriver* NetDriver = GEngine->FindNamedNetDriver(GetWorld(), NAME_DemoNetDriver);
				if (NetDriver)
				{
					NetDriver->ProcessRemoteFunction(Actor, Function, Parameters, OutParms, Stack, SubObject);
				}
				// Return here so we don't call InternalProcessRemoteFunction again at the bottom of this function
				return;
			}
		}

		// Send function data to remote.
		Connection = Actor->GetNetConnection();
		if (Connection)
		{
			InternalProcessRemoteFunction( Actor, SubObject, Connection, Function, Parameters, OutParms, Stack, bIsServer );
		}
		else
		{
			UE_LOG(LogNet, Warning, TEXT("UIpNetDriver::ProcessRemoteFunction: No owning connection for actor %s. Function %s will not be processed."), *Actor->GetName(), *Function->GetName());
		}
	}
}

FString UIpNetDriver::LowLevelGetNetworkNumber()
{
	return LocalAddr->ToString(true);
}

void UIpNetDriver::LowLevelDestroy()
{
	Super::LowLevelDestroy();

	// Close the socket.
	if( Socket && !HasAnyFlags(RF_ClassDefaultObject) )
	{
		ISocketSubsystem* SocketSubsystem = GetSocketSubsystem();
		if( !Socket->Close() )
		{
			UE_LOG(LogExit, Log, TEXT("closesocket error (%i)"), (int32)SocketSubsystem->GetLastErrorCode() );
		}
		// Free the memory the OS allocated for this socket
		SocketSubsystem->DestroySocket(Socket);
		Socket = NULL;
		UE_LOG(LogExit, Log, TEXT("%s shut down"),*GetDescription() );
	}

}


bool UIpNetDriver::HandleSocketsCommand( const TCHAR* Cmd, FOutputDevice& Ar, UWorld* InWorld )
{
	Ar.Logf(TEXT(""));
	if (Socket != NULL)
	{
		TSharedRef<FInternetAddr> LocalInternetAddr = GetSocketSubsystem()->CreateInternetAddr();
		Socket->GetAddress(*LocalInternetAddr);
		Ar.Logf(TEXT("%s Socket: %s"), *GetDescription(), *LocalInternetAddr->ToString(true));
	}		
	else
	{
		Ar.Logf(TEXT("%s Socket: null"), *GetDescription());
	}
	return UNetDriver::Exec( InWorld, TEXT("SOCKETS"),Ar);
}

bool UIpNetDriver::Exec( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar )
{
	if (FParse::Command(&Cmd,TEXT("SOCKETS")))
	{
		return HandleSocketsCommand( Cmd, Ar, InWorld );
	}
	return UNetDriver::Exec( InWorld, Cmd,Ar);
}

UIpConnection* UIpNetDriver::GetServerConnection() 
{
	return (UIpConnection*)ServerConnection;
}

