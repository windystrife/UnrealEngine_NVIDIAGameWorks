// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	IpConnection.cpp: Unreal IP network connection.
Notes:
	* See \msdev\vc98\include\winsock.h and \msdev\vc98\include\winsock2.h 
	  for Winsock WSAE* errors returned by Windows Sockets.
=============================================================================*/

#include "IpConnection.h"
#include "SocketSubsystem.h"

#include "IPAddress.h"
#include "Sockets.h"
#include "Net/NetworkProfiler.h"
#include "Net/DataChannel.h"

#include "PacketAudit.h"

/*-----------------------------------------------------------------------------
	Declarations.
-----------------------------------------------------------------------------*/

// Size of a UDP header.
#define IP_HEADER_SIZE     (20)
#define UDP_HEADER_SIZE    (IP_HEADER_SIZE+8)

DECLARE_CYCLE_STAT(TEXT("IpConnection InitRemoteConnection"), Stat_IpConnectionInitRemoteConnection, STATGROUP_Net);

UIpConnection::UIpConnection(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer),
	RemoteAddr(NULL),
	Socket(NULL),
	ResolveInfo(NULL)
{
}

void UIpConnection::InitBase(UNetDriver* InDriver, class FSocket* InSocket, const FURL& InURL, EConnectionState InState, int32 InMaxPacket, int32 InPacketOverhead)
{
	// Pass the call up the chain
	Super::InitBase(InDriver, InSocket, InURL, InState, 
		// Use the default packet size/overhead unless overridden by a child class
		(InMaxPacket == 0 || InMaxPacket > MAX_PACKET_SIZE) ? MAX_PACKET_SIZE : InMaxPacket,
		InPacketOverhead == 0 ? UDP_HEADER_SIZE : InPacketOverhead);

	Socket = InSocket;
	ResolveInfo = NULL;
}

void UIpConnection::InitLocalConnection(UNetDriver* InDriver, class FSocket* InSocket, const FURL& InURL, EConnectionState InState, int32 InMaxPacket, int32 InPacketOverhead)
{
	InitBase(InDriver, InSocket, InURL, InState, 
		// Use the default packet size/overhead unless overridden by a child class
		(InMaxPacket == 0 || InMaxPacket > MAX_PACKET_SIZE) ? MAX_PACKET_SIZE : InMaxPacket,
		InPacketOverhead == 0 ? UDP_HEADER_SIZE : InPacketOverhead);

	// Figure out IP address from the host URL
	bool bIsValid = false;
	// Get numerical address directly.
	RemoteAddr = InDriver->GetSocketSubsystem()->CreateInternetAddr();
	RemoteAddr->SetIp(*InURL.Host, bIsValid);
	RemoteAddr->SetPort(InURL.Port);

	// Try to resolve it if it failed
	if (bIsValid == false)
	{
		// Create thread to resolve the address.
		ResolveInfo = InDriver->GetSocketSubsystem()->GetHostByName(TCHAR_TO_ANSI(*InURL.Host));
		if (ResolveInfo == NULL)
		{
			Close();
			UE_LOG(LogNet, Verbose, TEXT("IpConnection::InitConnection: Unable to resolve %s"), *InURL.Host);
		}
	}

	// Initialize our send bunch
	InitSendBuffer();
}

void UIpConnection::InitRemoteConnection(UNetDriver* InDriver, class FSocket* InSocket, const FURL& InURL, const class FInternetAddr& InRemoteAddr, EConnectionState InState, int32 InMaxPacket, int32 InPacketOverhead)
{
	SCOPE_CYCLE_COUNTER(Stat_IpConnectionInitRemoteConnection);

	InitBase(InDriver, InSocket, InURL, InState, 
		// Use the default packet size/overhead unless overridden by a child class
		(InMaxPacket == 0 || InMaxPacket > MAX_PACKET_SIZE) ? MAX_PACKET_SIZE : InMaxPacket,
		InPacketOverhead == 0 ? UDP_HEADER_SIZE : InPacketOverhead);

	// Copy the remote IPAddress passed in
	bool bIsValid = false;
	FString IpAddrStr = InRemoteAddr.ToString(false);
	RemoteAddr = InDriver->GetSocketSubsystem()->CreateInternetAddr();
	RemoteAddr->SetIp(*IpAddrStr, bIsValid);
	RemoteAddr->SetPort(InRemoteAddr.GetPort());

	URL.Host = RemoteAddr->ToString(false);

	// Initialize our send bunch
	InitSendBuffer();

	// This is for a client that needs to log in, setup ClientLoginState and ExpectedClientLoginMsgType to reflect that
	SetClientLoginState( EClientLoginState::LoggingIn );
	SetExpectedClientLoginMsgType( NMT_Hello );
}

void UIpConnection::LowLevelSend(void* Data, int32 CountBytes, int32 CountBits)
{
	const uint8* DataToSend = reinterpret_cast<uint8*>(Data);

	if( ResolveInfo )
	{
		// If destination address isn't resolved yet, send nowhere.
		if( !ResolveInfo->IsComplete() )
		{
			// Host name still resolving.
			return;
		}
		else if( ResolveInfo->GetErrorCode() != SE_NO_ERROR )
		{
			// Host name resolution just now failed.
			UE_LOG(LogNet, Log,  TEXT("Host name resolution failed with %d"), ResolveInfo->GetErrorCode() );
			Driver->ServerConnection->State = USOCK_Closed;
			delete ResolveInfo;
			ResolveInfo = NULL;
			return;
		}
		else
		{
			uint32 Addr;
			// Host name resolution just now succeeded.
			ResolveInfo->GetResolvedAddress().GetIp(Addr);
			RemoteAddr->SetIp(Addr);
			UE_LOG(LogNet, Log, TEXT("Host name resolution completed"));
			delete ResolveInfo;
			ResolveInfo = NULL;
		}
	}


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

	bool bBlockSend = false;

#if !UE_BUILD_SHIPPING
	LowLevelSendDel.ExecuteIfBound((void*)DataToSend, CountBytes, bBlockSend);
#endif

	if (!bBlockSend)
	{
		// Send to remote.
		int32 BytesSent = 0;
		CLOCK_CYCLES(Driver->SendCycles);

		if ( CountBytes > MaxPacket )
		{
			UE_LOG( LogNet, Warning, TEXT( "UIpConnection::LowLevelSend: CountBytes > MaxPacketSize! Count: %i, MaxPacket: %i %s" ), CountBytes, MaxPacket, *Describe() );
		}

		FPacketAudit::NotifyLowLevelSend((uint8*)DataToSend, CountBytes, CountBits);

		if (CountBytes > 0)
		{
			Socket->SendTo(DataToSend, CountBytes, BytesSent, *RemoteAddr);
		}

		UNCLOCK_CYCLES(Driver->SendCycles);
		NETWORK_PROFILER(GNetworkProfiler.FlushOutgoingBunches(this));
		NETWORK_PROFILER(GNetworkProfiler.TrackSocketSendTo(Socket->GetDescription(),DataToSend,BytesSent,NumPacketIdBits,NumBunchBits,NumAckBits,NumPaddingBits,this));
	}
}

FString UIpConnection::LowLevelGetRemoteAddress(bool bAppendPort)
{
	return RemoteAddr->ToString(bAppendPort);
}

FString UIpConnection::LowLevelDescribe()
{
	TSharedRef<FInternetAddr> LocalAddr = Driver->GetSocketSubsystem()->CreateInternetAddr();
	Socket->GetAddress(*LocalAddr);
	return FString::Printf
	(
		TEXT("url=%s remote=%s local=%s state: %s"),
		*URL.Host,
		*RemoteAddr->ToString(true),
		*LocalAddr->ToString(true),
			State==USOCK_Pending	?	TEXT("Pending")
		:	State==USOCK_Open		?	TEXT("Open")
		:	State==USOCK_Closed		?	TEXT("Closed")
		:								TEXT("Invalid")
	);
}

int32 UIpConnection::GetAddrAsInt(void)
{
	uint32 OutAddr = 0;
	// Get the host byte order ip addr
	RemoteAddr->GetIp(OutAddr);
	return (int32)OutAddr;
}

int32 UIpConnection::GetAddrPort(void)
{
	int32 OutPort = 0;
	// Get the host byte order ip port
	RemoteAddr->GetPort(OutPort);
	return OutPort;
}

FString UIpConnection::RemoteAddressToString()
{
	return RemoteAddr->ToString(true);
}
