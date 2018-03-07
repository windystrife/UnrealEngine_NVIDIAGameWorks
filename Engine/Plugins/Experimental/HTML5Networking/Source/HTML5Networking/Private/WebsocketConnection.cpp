// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "HTML5NetworkingPrivate.h"

#include "IPAddress.h"
#include "Sockets.h"
#include "Net/NetworkProfiler.h"
#include "Net/DataChannel.h"

#include "WebSocketConnection.h"
#include "WebSocketNetDriver.h"
#include "WebSocket.h"
#include "Runtime/PacketHandlers/PacketHandler/Public/PacketHandler.h"
#include "PacketHandlers/StatelessConnectHandlerComponent.h"

/*-----------------------------------------------------------------------------
Declarations.
-----------------------------------------------------------------------------*/

// Size of a UDP header.
#define IP_HEADER_SIZE     (20)
#define UDP_HEADER_SIZE    (IP_HEADER_SIZE+8)
#define WINSOCK_MAX_PACKET (512)

UWebSocketConnection::UWebSocketConnection(const FObjectInitializer& ObjectInitializer) :
Super(ObjectInitializer),
WebSocket(NULL)
{
}

void UWebSocketConnection::InitBase(UNetDriver* InDriver, class FSocket* InSocket, const FURL& InURL, EConnectionState InState, int32 InMaxPacket, int32 InPacketOverhead)
{
	// Pass the call up the chain
	Super::InitBase(InDriver, InSocket, InURL, InState,
		// Use the default packet size/overhead unless overridden by a child class
		InMaxPacket == 0 ? WINSOCK_MAX_PACKET : InMaxPacket,
		InPacketOverhead == 0 ? UDP_HEADER_SIZE : InPacketOverhead);

}

void UWebSocketConnection::InitLocalConnection(UNetDriver* InDriver, class FSocket* InSocket, const FURL& InURL, EConnectionState InState, int32 InMaxPacket, int32 InPacketOverhead)
{
	InitBase(InDriver, InSocket, InURL, InState,
		// Use the default packet size/overhead unless overridden by a child class
		InMaxPacket == 0 ? WINSOCK_MAX_PACKET : InMaxPacket,
		InPacketOverhead == 0 ? UDP_HEADER_SIZE : InPacketOverhead);

	// Figure out IP address from the host URL

	// Initialize our send bunch
	InitSendBuffer();
}

void UWebSocketConnection::InitRemoteConnection(UNetDriver* InDriver, class FSocket* InSocket, const FURL& InURL, const class FInternetAddr& InRemoteAddr, EConnectionState InState, int32 InMaxPacket, int32 InPacketOverhead)
{
	InitBase(InDriver, InSocket, InURL, InState,
		// Use the default packet size/overhead unless overridden by a child class
		InMaxPacket == 0 ? WINSOCK_MAX_PACKET : InMaxPacket,
		InPacketOverhead == 0 ? UDP_HEADER_SIZE : InPacketOverhead);

	// Initialize our send bunch
	InitSendBuffer();

	// This is for a client that needs to log in, setup ClientLoginState and ExpectedClientLoginMsgType to reflect that
	SetClientLoginState(EClientLoginState::LoggingIn);
	SetExpectedClientLoginMsgType(NMT_Hello);
}

void UWebSocketConnection::LowLevelSend(void* Data, int32 CountBytes, int32 CountBits)
{
	const uint8* DataToSend = reinterpret_cast<uint8*>(Data);

	// Process any packet modifiers
	if (Handler.IsValid() && !Handler->GetRawSend())
	{
		const ProcessedPacket ProcessedData = Handler->Outgoing(reinterpret_cast<uint8*>(Data), CountBits);

		if (!ProcessedData.bError)
		{
			DataToSend = ProcessedData.Data;
			CountBytes = FMath::DivideAndRoundUp(ProcessedData.CountBits, 8);
			CountBits = ProcessedData.CountBits;
		}
		else
		{
			CountBytes = 0;
			CountBits = 0;
		}
	}

	if ( CountBytes > MaxPacket )
	{
		UE_LOG( LogNet, Warning, TEXT( "UWebSocketConnection::LowLevelSend: CountBytes > MaxPacketSize! Count: %i, MaxPacket: %i %s" ), CountBytes, MaxPacket, *Describe() );
	}

	bool bBlockSend = false;

#if !UE_BUILD_SHIPPING
	LowLevelSendDel.ExecuteIfBound((void*)DataToSend, CountBytes, bBlockSend);
#endif

	if (!bBlockSend && CountBytes > 0)
	{
		WebSocket->Send((uint8*)DataToSend, CountBytes);
	}
}

FString UWebSocketConnection::LowLevelGetRemoteAddress(bool bAppendPort)
{
	return WebSocket->RemoteEndPoint(bAppendPort);
}

FString UWebSocketConnection::LowLevelDescribe()
{
	return FString::Printf
		(
		TEXT(" remote=%s local=%s state: %s"),
		*WebSocket->RemoteEndPoint(true),
		*WebSocket->LocalEndPoint(true),
		State == USOCK_Pending ? TEXT("Pending")
		: State == USOCK_Open ? TEXT("Open")
		: State == USOCK_Closed ? TEXT("Closed")
		: TEXT("Invalid")
		);
}

void UWebSocketConnection::SetWebSocket(FWebSocket* InWebSocket)
{
	WebSocket = InWebSocket; 
}

FWebSocket* UWebSocketConnection::GetWebSocket()
{
	return WebSocket;
}

void UWebSocketConnection::Tick()
{
	UNetConnection::Tick();
	WebSocket->Tick();
}

void UWebSocketConnection::FinishDestroy()
{
	Super::FinishDestroy();
	delete WebSocket; 
	WebSocket = NULL;

}

void UWebSocketConnection::ReceivedRawPacket(void* Data,int32 Count)
{
	if (Count == 0 ||   // nothing to process
		Driver == NULL) // connection closing
	{
		return;
	}

	uint8* DataRef = reinterpret_cast<uint8*>(Data);
	if ( bChallengeHandshake )
	{
		// Process all incoming packets.
		if (Driver->ConnectionlessHandler.IsValid() && Driver->StatelessConnectComponent.IsValid())
		{
			const ProcessedPacket UnProcessedPacket =
									Driver->ConnectionlessHandler->IncomingConnectionless(LowLevelGetRemoteAddress(true), DataRef, Count);
	
			TSharedPtr<StatelessConnectHandlerComponent> StatelessConnect = Driver->StatelessConnectComponent.Pin();

			if (!UnProcessedPacket.bError && StatelessConnect->HasPassedChallenge(LowLevelGetRemoteAddress(true)))
			{
				UE_LOG(LogNet, Log, TEXT("Server accepting post-challenge connection from: %s"), *LowLevelGetRemoteAddress(true));
				// Set the initial packet sequence from the handshake data
				if (StatelessConnectComponent.IsValid())
				{
					int32 ServerSequence = 0;
					int32 ClientSequence = 0;
					StatelessConnect->GetChallengeSequence(ServerSequence, ClientSequence);
					InitSequence(ClientSequence, ServerSequence);
				}

				if (Handler.IsValid())
				{
					Handler->BeginHandshaking();
				}

				bChallengeHandshake = false; // i.e. bPassedChallenge
				UE_LOG(LogNet, Warning, TEXT("UWebSocketConnection::bChallengeHandshake: %s"), *LowLevelDescribe());
				Count = FMath::DivideAndRoundUp(UnProcessedPacket.CountBits, 8);
				if (Count > 0)
				{
					DataRef = UnProcessedPacket.Data;
				}
				else
				{
					return; // NO FURTHER DATA TO PROCESS
				}
			}
			else
			{
				// WARNING: if here, it might be during (bInitialConnect) - which needs to be processed (ReceivedRawPacket)
				//return;
			}
		}
	}

	UNetConnection::ReceivedRawPacket(DataRef,Count);
}

int32 UWebSocketConnection::GetAddrAsInt()
{
	// Get the host byte order ip addr
	struct sockaddr_in* sock = WebSocket->GetRemoteAddr();
	return (int32)ntohl(sock->sin_addr.s_addr);
}

int32 UWebSocketConnection::GetAddrPort()
{
	// Get the host byte order ip port
	struct sockaddr_in* sock = WebSocket->GetRemoteAddr();
	return (int32)ntohs(sock->sin_port);
}

FString UWebSocketConnection::RemoteAddressToString()
{
	return WebSocket->RemoteEndPoint(true);
}
