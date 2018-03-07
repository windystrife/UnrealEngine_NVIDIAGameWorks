// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Sockets.h"
#include "SocketSubsystem.h"
//#include "Net/NetworkProfiler.h"

//
// FSocket stats implementation
//

bool FSocket::SendTo(const uint8* Data, int32 Count, int32& BytesSent, const FInternetAddr& Destination)
{
//	NETWORK_PROFILER(GNetworkProfiler.TrackSocketSendTo(this,Data,BytesSent,Destination));
	UE_LOG(LogSockets, Verbose, TEXT("Socket '%s' SendTo %i Bytes"), *SocketDescription, BytesSent );
	return true;
}


bool FSocket::Send(const uint8* Data, int32 Count, int32& BytesSent)
{
//	NETWORK_PROFILER(GNetworkProfiler.TrackSocketSend(this,Data,BytesSent));
	UE_LOG(LogSockets, Verbose, TEXT("Socket '%s' Send %i Bytes"), *SocketDescription, BytesSent );
	return true;
}


bool FSocket::RecvFrom(uint8* Data, int32 BufferSize, int32& BytesRead, FInternetAddr& Source, ESocketReceiveFlags::Type Flags)
{
	if( BytesRead > 0 )
	{
		UE_LOG(LogSockets, Verbose, TEXT("Socket '%s' RecvFrom %i Bytes"), *SocketDescription, BytesRead );
	}
	return true;
}


bool FSocket::Recv(uint8* Data, int32 BufferSize, int32& BytesRead, ESocketReceiveFlags::Type Flags)
{
	if( BytesRead > 0 )
	{
		UE_LOG(LogSockets, Verbose, TEXT("Socket '%s' Recv %i Bytes"), *SocketDescription, BytesRead );
	}
	return true;
}
