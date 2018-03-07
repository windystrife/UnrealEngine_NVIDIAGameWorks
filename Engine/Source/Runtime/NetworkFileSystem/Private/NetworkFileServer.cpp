// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "NetworkFileServer.h"
#include "HAL/RunnableThread.h"
#include "Misc/OutputDeviceRedirector.h"
#include "IPAddress.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "NetworkMessage.h"
#include "NetworkFileSystemLog.h"
#include "NetworkFileServerConnection.h"

class FNetworkFileServerClientConnectionThreaded
	:  public FNetworkFileServerClientConnection
	  ,protected FRunnable 
{
public:

	FNetworkFileServerClientConnectionThreaded(FSocket* InSocket, const FNetworkFileDelegateContainer* NetworkFileDelegates, const TArray<ITargetPlatform*>& InActiveTargetPlatforms )
		:  FNetworkFileServerClientConnection( NetworkFileDelegates, InActiveTargetPlatforms)
		  ,Socket(InSocket)
	{
		Running.Set(true);
		StopRequested.Reset();
	
#if UE_BUILD_DEBUG
		// this thread needs more space in debug builds as it tries to log messages and such
		const static uint32 NetworkFileServerThreadSize = 2 * 1024 * 1024; 
#else
		const static uint32 NetworkFileServerThreadSize = 1 * 1024 * 1024; 
#endif
		WorkerThread = FRunnableThread::Create(this, TEXT("FNetworkFileServerClientConnection"), NetworkFileServerThreadSize, TPri_AboveNormal);
	}


	virtual bool Init() override
	{
		return true; 
	}

	virtual uint32 Run() override
	{
		while (!StopRequested.GetValue())
		{
			  // read a header and payload pair
			  FArrayReader Payload; 
			  if (!FNFSMessageHeader::ReceivePayload(Payload, FSimpleAbstractSocket_FSocket(Socket)))
				  break; 
			  // now process the contents of the payload
			  if ( !FNetworkFileServerClientConnection::ProcessPayload(Payload) )
			  {
				  // give the processing of the payload a chance to terminate the connection
				  // failed to process message
				  UE_LOG(LogFileServer, Warning, TEXT("Unable to process payload terminating connection"));
				  break;
			  }
		  }

		  return true;
	}

	virtual bool SendPayload( TArray<uint8> &Out ) override
	{
#if USE_MCSOCKET_FOR_NFS
		return FNFSMessageHeader::WrapAndSendPayload(Out, FSimpleAbstractSocket_FMultichannelTCPSocket(MCSocket, NFS_Channels::Main));
#else
		return FNFSMessageHeader::WrapAndSendPayload(Out, FSimpleAbstractSocket_FSocket(Socket));
#endif
	}

	virtual void Stop() override
	{
		StopRequested.Set(true);
	}

	virtual void Exit() override
	{
		Socket->Close();
		ISocketSubsystem::Get()->DestroySocket(Socket);
		Running.Set(false); 
	}

	bool IsRunning()
	{
		return  (Running.GetValue() != 0); 
	}

	void GetAddress( FInternetAddr& Addr )
	{
		Socket->GetAddress(Addr);
	}

	void GetPeerAddress( FInternetAddr& Addr )
	{
		Socket->GetPeerAddress(Addr);
	}

	~FNetworkFileServerClientConnectionThreaded()
	{
		WorkerThread->Kill(true);
	}

private: 
	FSocket* Socket; 
	FThreadSafeCounter StopRequested;
	FThreadSafeCounter Running;
	FRunnableThread* WorkerThread; 
};



/* FNetworkFileServer constructors
 *****************************************************************************/

FNetworkFileServer::FNetworkFileServer( int32 InPort, FNetworkFileDelegateContainer InNetworkFileDelegateContainer,
	const TArray<ITargetPlatform*>& InActiveTargetPlatforms )
	:ActiveTargetPlatforms(InActiveTargetPlatforms)
{
	if(InPort <0)
	{
		InPort = DEFAULT_TCP_FILE_SERVING_PORT;
	}

	Running.Set(false);
	StopRequested.Set(false);
	UE_LOG(LogFileServer, Warning, TEXT("Unreal Network File Server starting up..."));

	NetworkFileDelegates = InNetworkFileDelegateContainer;

	// make sure sockets are going
	ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get();

	if(!SocketSubsystem)
	{
		UE_LOG(LogFileServer, Error, TEXT("Could not get socket subsystem."));
	}
	else
	{
		// create a server TCP socket
		Socket = SocketSubsystem->CreateSocket(NAME_Stream, TEXT("FNetworkFileServer tcp-listen"));

		if(!Socket)
		{
			UE_LOG(LogFileServer, Error, TEXT("Could not create listen socket."));
		}
		else
		{
			// listen on any IP address
			ListenAddr = SocketSubsystem->GetLocalBindAddr(*GLog);

			ListenAddr->SetPort(InPort);
			Socket->SetReuseAddr();

			// bind to the address
			if (!Socket->Bind(*ListenAddr))
			{
				UE_LOG(LogFileServer, Warning, TEXT("Failed to bind listen socket %s in FNetworkFileServer"), *ListenAddr->ToString(true));
			}
			// listen for connections
			else if (!Socket->Listen(16))
			{
				UE_LOG(LogFileServer, Warning, TEXT("Failed to listen on socket %s in FNetworkFileServer"), *ListenAddr->ToString(true));
			}
			else
			{
				// set the port on the listen address to be the same as the port on the socket
				int32 port = Socket->GetPortNo();
				check((InPort == 0 && port != 0) || port == InPort);
				ListenAddr->SetPort(port);

				// now create a thread to accept connections
				Thread = FRunnableThread::Create(this, TEXT("FNetworkFileServer"), 8 * 1024, TPri_AboveNormal);

				UE_LOG(LogFileServer, Display, TEXT("Unreal Network File Server is ready for client connections on %s!"), *ListenAddr->ToString(true));
			}
		}
	}

 }

FNetworkFileServer::~FNetworkFileServer()
{
	// Kill the running thread.
	if( Thread != NULL )
	{
		Thread->Kill(true);

		delete Thread;
		Thread = NULL;
	}

	// We are done with the socket.
	Socket->Close();
	ISocketSubsystem::Get()->DestroySocket(Socket);
	Socket = NULL;
}


/* FRunnable overrides
 *****************************************************************************/

uint32 FNetworkFileServer::Run( )
{
	Running.Set(true); 
	// go until requested to be done
	while (!StopRequested.GetValue())
	{
		bool bReadReady = false;

		// clean up closed connections
		for (int32 ConnectionIndex = 0; ConnectionIndex < Connections.Num(); ++ConnectionIndex)
		{
			FNetworkFileServerClientConnectionThreaded* Connection = Connections[ConnectionIndex];

			if (!Connection->IsRunning())
			{
				UE_LOG(LogFileServer, Display, TEXT( "Client %s disconnected." ), *Connection->GetDescription() );
				Connections.RemoveAtSwap(ConnectionIndex);
				delete Connection;
			}
		}

		// check for incoming connections
		if (Socket->WaitForPendingConnection(bReadReady, FTimespan::FromSeconds(0.25f)))
		{
			if (bReadReady)
			{
				FSocket* ClientSocket = Socket->Accept(TEXT("Remote Console Connection"));

				if (ClientSocket != NULL)
				{
					TSharedPtr<FInternetAddr> Addr = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
					ClientSocket->GetAddress(*Addr);
					TSharedPtr<FInternetAddr> PeerAddr = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
					ClientSocket->GetPeerAddress(*PeerAddr);

					for (auto PreviousConnection : Connections)
					{
						TSharedPtr<FInternetAddr> PreviousAddr = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();;
						PreviousConnection->GetAddress(*PreviousAddr);
						TSharedPtr<FInternetAddr> PreviousPeerAddr = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();;
						PreviousConnection->GetPeerAddress(*PreviousPeerAddr);
						if ((*Addr == *PreviousAddr) &&
							(*PeerAddr == *PreviousPeerAddr))
						{
							// kill the connection 
							PreviousConnection->Stop();
							UE_LOG(LogFileServer, Warning, TEXT("Killing client connection %s because new client connected from same address."), *PreviousConnection->GetDescription());
						}
					}

					FNetworkFileServerClientConnectionThreaded* Connection = new FNetworkFileServerClientConnectionThreaded(ClientSocket, &NetworkFileDelegates, ActiveTargetPlatforms);
					Connections.Add(Connection);
					UE_LOG(LogFileServer, Display, TEXT("Client %s connected."), *Connection->GetDescription());
				}
			}
		}
		else
		{
			FPlatformProcess::Sleep(0.25f);
		}
	}

	return 0;
}


void FNetworkFileServer::Exit( )
{
	// close all connections
	for (int32 ConnectionIndex = 0; ConnectionIndex < Connections.Num(); ConnectionIndex++)
	{
		delete Connections[ConnectionIndex];
	}

	Connections.Empty();
}


/* INetworkFileServer overrides
 *****************************************************************************/

FString FNetworkFileServer::GetSupportedProtocol() const
{
	return FString("tcp");
}


bool FNetworkFileServer::GetAddressList( TArray<TSharedPtr<FInternetAddr> >& OutAddresses ) const
{
	if (ListenAddr.IsValid())
	{
		FString ListenAddressString = ListenAddr->ToString(true);

		if (ListenAddressString.StartsWith(TEXT("0.0.0.0")))
		{
			if (ISocketSubsystem::Get()->GetLocalAdapterAddresses(OutAddresses))
			{
				for (int32 AddressIndex = 0; AddressIndex < OutAddresses.Num(); ++AddressIndex)
				{
					OutAddresses[AddressIndex]->SetPort(ListenAddr->GetPort());
				}
			}
		}
		else
		{
			OutAddresses.Add(ListenAddr);
		}
	}

	return (OutAddresses.Num() > 0);
}


bool FNetworkFileServer::IsItReadyToAcceptConnections(void) const
{
	return (Running.GetValue() != 0); 
}

int32 FNetworkFileServer::NumConnections(void) const
{
	return Connections.Num(); 
}

void FNetworkFileServer::Shutdown(void)
{
	Stop();
}
