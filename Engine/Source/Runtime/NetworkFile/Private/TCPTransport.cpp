// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "TCPTransport.h"
#include "NetworkMessage.h"
#include "SocketSubsystem.h"
#include "IPAddress.h"
#include "Sockets.h"
#include "MultichannelTcpSocket.h"
#include "NetworkPlatformFile.h"

FTCPTransport::FTCPTransport()
	:FileSocket(NULL)
	,MCSocket(NULL)
{
}

bool FTCPTransport::Initialize(const TCHAR* InHostIp)
{
	ISocketSubsystem* SSS = ISocketSubsystem::Get();

	FString HostIp = InHostIp;


	// the protocol isn't required for tcp it's assumed by default
	HostIp.RemoveFromStart(TEXT("tcp://"));

	// convert the string to a ip addr structure
	// DEFAULT_TCP_FILE_SERVING_PORT is overridden 
	TSharedRef<FInternetAddr> Addr = SSS->CreateInternetAddr(0, DEFAULT_TCP_FILE_SERVING_PORT);
	bool bIsValid;

	Addr->SetIp(*HostIp, bIsValid);

	if (bIsValid)
	{
		// create the socket
		FileSocket = SSS->CreateSocket(NAME_Stream, TEXT("FNetworkPlatformFile tcp"));

		// try to connect to the server
		if (FileSocket == nullptr || FileSocket->Connect(*Addr) == false)
		{
			// on failure, shut it all down
			SSS->DestroySocket(FileSocket);
			FileSocket = NULL;
			UE_LOG(LogNetworkPlatformFile, Error, TEXT("Failed to connect to file server at %s."), *Addr->ToString(true));
		}
	}

#if USE_MCSOCKET_FOR_NFS
	MCSocket = new FMultichannelTcpSocket(FileSocket, 64 * 1024 * 1024);
#endif

	return ( FileSocket || MCSocket );
}


bool FTCPTransport::SendPayloadAndReceiveResponse(TArray<uint8>& In, TArray<uint8>& Out)
{
	bool SendResult = false; 

#if USE_MCSOCKET_FOR_NFS
		SendResult = FNFSMessageHeader::WrapAndSendPayload(In, FSimpleAbstractSocket_FMultichannelTCPSocket(MCSocket, NFS_Channels::Main));
#else 	
		SendResult = FNFSMessageHeader::WrapAndSendPayload(In, FSimpleAbstractSocket_FSocket(FileSocket));
#endif 
	
	if (!SendResult)
		return false; 

	FArrayReader Response; 
	bool RetResult = false; 
#if USE_MCSOCKET_FOR_NFS
	RetResult = FNFSMessageHeader::ReceivePayload(Response, FSimpleAbstractSocket_FMultichannelTCPSocket(MCSocket, NFS_Channels::Main));
#else
	RetResult = FNFSMessageHeader::ReceivePayload(Response, FSimpleAbstractSocket_FSocket(FileSocket));
#endif 

	if (RetResult)
	{
		Out.Append( Response.GetData(), Response.Num()); 
		return true;
	}

	return false; 
}


bool FTCPTransport::ReceiveResponse( TArray<uint8> &Out )
{
	FArrayReader Response;
	bool RetResult = true;
#if USE_MCSOCKET_FOR_NFS
	RetResult &= FNFSMessageHeader::ReceivePayload(Response, FSimpleAbstractSocket_FMultichannelTCPSocket(MCSocket, NFS_Channels::Main));
#else
	RetResult &= FNFSMessageHeader::ReceivePayload(Response, FSimpleAbstractSocket_FSocket(FileSocket));
#endif

	if (RetResult)
	{
		Out.Append( Response.GetData(), Response.Num()); 
		return true;
	}

	return false;
}

FTCPTransport::~FTCPTransport()
{
	// on failure, shut it all down
	delete MCSocket;
	MCSocket = NULL;
	ISocketSubsystem::Get()->DestroySocket(FileSocket);
	FileSocket = NULL;
}

