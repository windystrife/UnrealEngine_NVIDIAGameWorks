// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "OculusNetConnection.h"
#include "OnlineSubsystemOculusPrivate.h"

#include "IPAddressOculus.h"

#include "Net/DataChannel.h"

void UOculusNetConnection::InitBase(UNetDriver* InDriver, class FSocket* InSocket, const FURL& InURL, EConnectionState InState, int32 InMaxPacket, int32 InPacketOverhead)
{
	if (bIsPassThrough)
	{
		UIpConnection::InitBase(InDriver, InSocket, InURL, InState, InMaxPacket, InPacketOverhead);
		return;
	}

	// Pass the call up the chain
	UNetConnection::InitBase(InDriver, InSocket, InURL, InState,
		// Use the default packet size/overhead unless overridden by a child class
		InMaxPacket == 0 ? MAX_PACKET_SIZE : InMaxPacket,
		/* PacketOverhead */ 1);

	// We handle our own overhead
	PacketOverhead = 0;

	// Initalize the send buffer
	InitSendBuffer();
}

void UOculusNetConnection::InitLocalConnection(UNetDriver* InDriver, class FSocket* InSocket, const FURL& InURL, EConnectionState InState, int32 InMaxPacket, int32 InPacketOverhead)
{
	if (InDriver->GetSocketSubsystem() != nullptr)
	{
		bIsPassThrough = true;
		UIpConnection::InitLocalConnection(InDriver, InSocket, InURL, InState, InMaxPacket, InPacketOverhead);
		return;
	}

	bIsPassThrough = false;
	InitBase(InDriver, InSocket, InURL, InState,
		// Use the default packet size/overhead unless overridden by a child class
		InMaxPacket == 0 ? MAX_PACKET_SIZE : InMaxPacket,
		0);
}

void UOculusNetConnection::InitRemoteConnection(UNetDriver* InDriver, class FSocket* InSocket, const FURL& InURL, const class FInternetAddr& InRemoteAddr, EConnectionState InState, int32 InMaxPacket, int32 InPacketOverhead)
{
	if (InDriver->GetSocketSubsystem() != nullptr)
	{
		bIsPassThrough = true;
		UIpConnection::InitRemoteConnection(InDriver, InSocket, InURL, InRemoteAddr, InState, InMaxPacket, InPacketOverhead);
		return;
	}

	bIsPassThrough = false;
	InitBase(InDriver, InSocket, InURL, InState,
		// Use the default packet size/overhead unless overridden by a child class
		InMaxPacket == 0 ? MAX_PACKET_SIZE : InMaxPacket,
		0);

	auto OculusAddr = static_cast<const FInternetAddrOculus&>(InRemoteAddr);
	PeerID = OculusAddr.GetID();

	// This is for a client that needs to log in, setup ClientLoginState and ExpectedClientLoginMsgType to reflect that
	SetClientLoginState(EClientLoginState::LoggingIn);
	SetExpectedClientLoginMsgType(NMT_Hello);
}

void UOculusNetConnection::LowLevelSend(void* Data, int32 CountBytes, int32 CountBits)
{
	if (bIsPassThrough)
	{
		UIpConnection::LowLevelSend(Data, CountBytes, CountBits);
		return;
	}

	check(PeerID);

	// Do not send packets over a closed connection
	// This can unintentionally re-open the connection
	if (State == EConnectionState::USOCK_Closed && ovr_Net_IsConnected(PeerID))
	{
		return;
	}

	const uint8* DataToSend = reinterpret_cast<uint8*>(Data);

	UE_LOG(LogNetTraffic, VeryVerbose, TEXT("Low level send to: %llu Count: %d"), PeerID, CountBytes);

	// Process any packet modifiers
	if (Handler.IsValid() && !Handler->GetRawSend())
	{
		const ProcessedPacket ProcessedData = Handler->Outgoing(reinterpret_cast<uint8*>(Data), CountBits);

		if (!ProcessedData.bError)
		{
			DataToSend = ProcessedData.Data;
			CountBytes = FMath::DivideAndRoundUp(ProcessedData.CountBits, 8);
		}
		else
		{
			CountBytes = 0;
		}
	}

	bool bBlockSend = false;

#if !UE_BUILD_SHIPPING
	LowLevelSendDel.ExecuteIfBound((void*)DataToSend, CountBytes, bBlockSend);
#endif

	if (!bBlockSend && CountBytes > 0)
	{
		ovr_Net_SendPacket(PeerID, static_cast<size_t>(CountBytes), DataToSend, (InternalAck) ? ovrSend_Reliable : ovrSend_Unreliable);
	}
}

FString UOculusNetConnection::LowLevelGetRemoteAddress(bool bAppendPort)
{
	if (bIsPassThrough)
	{
		return UIpConnection::LowLevelGetRemoteAddress(bAppendPort);
	}
	return FString::Printf(TEXT("%llu.oculus"), PeerID);
}

FString UOculusNetConnection::LowLevelDescribe()
{
	if (bIsPassThrough)
	{
		return UIpConnection::LowLevelDescribe();
	}
	return FString::Printf(TEXT("PeerId=%llu"), PeerID);
}

void UOculusNetConnection::FinishDestroy()
{
	if (bIsPassThrough)
	{
		UIpConnection::FinishDestroy();
		return;
	}
	// Keep track if it's this call that is closing the connection before cleanup is called
	const bool bIsClosingOpenConnection = State != EConnectionState::USOCK_Closed;
	UNetConnection::FinishDestroy();

	// If this connection was open, then close it
	if (PeerID != 0 && bIsClosingOpenConnection)
	{
		UE_LOG(LogNet, Verbose, TEXT("Oculus Net Connection closed to %llu"), PeerID);
		ovr_Net_Close(PeerID);
	}
}

FString UOculusNetConnection::RemoteAddressToString()
{
	if (bIsPassThrough)
	{
		return UIpConnection::RemoteAddressToString();
	}
	return LowLevelGetRemoteAddress(/* bAppendPort */ false);
}
