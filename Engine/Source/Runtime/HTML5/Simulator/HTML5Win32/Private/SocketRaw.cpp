// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SocketRaw.h"
#include "IPAddressRaw.h"
#pragma warning( disable : 4530 )
#define WIN32_LEAN_AND_MEAN
#include <WinSock2.h>
#include <ws2tcpip.h>

#include <cstring>
#include <cassert>
#include <stdio.h>
#include "RawData.h"
#include <crtdbg.h>

/* Current Socket State */ 
	namespace SocketRawEnum 
	{
		enum Param
		{
			CanRead,
			CanWrite,
			HasError,
		};
		enum Return
		{
			Yes,
			No,
			EncounteredError,
		};
	} 

	SocketRawEnum::Return SocketCurrentState ( SOCKET Socket , SocketRawEnum::Param State, unsigned int WaitTimeMSec = 0)
	{
		timeval Time;
		Time.tv_sec = (int)WaitTimeMSec/1000;
		Time.tv_usec = WaitTimeMSec;
		fd_set SocketSet;
		// Set up the socket sets we are interested in (Just this one)
		FD_ZERO(&SocketSet);
		FD_SET(Socket, &SocketSet);
		// Check the status of the state
		int  SelectStatus = 0;
		switch (State)
		{
		case SocketRawEnum::CanRead:
			SelectStatus = select(Socket + 1, &SocketSet, NULL, NULL, &Time);
			break;
		case SocketRawEnum::CanWrite:
			SelectStatus = select(Socket + 1, NULL, &SocketSet, NULL, &Time);
			break;
		case SocketRawEnum::HasError:
			SelectStatus = select(Socket + 1, NULL, NULL, &SocketSet, &Time);
			break;
		}

		// If the select returns a positive number, the socket had the state. 0 means didn't have it and negative is API error condition (Not socket's error state).
		return SelectStatus > 0  ? SocketRawEnum::Yes : 
			SelectStatus == 0 ? SocketRawEnum::No : 
			SocketRawEnum::EncounteredError;
	}

bool FSocketRaw::Close()
{
	if (SocketRawData->socket != INVALID_SOCKET)
	{
		int error = closesocket(SocketRawData->socket);
		SocketRawData->socket = INVALID_SOCKET;
		return error == 0;
	}
	return false;
}

int FSocketRaw::Bind( const FInternetAddrRaw& Addr )
{
	const FInternetAddrRawData* data = Addr.GetInternalData();
	return bind(SocketRawData->socket, (const sockaddr*)&(data->Addr), sizeof(sockaddr_in)); 
}

unsigned int FSocketRaw::Connect( const FInternetAddrRaw& Addr )
{
	const FInternetAddrRawData* data = Addr.GetInternalData();
	int Return = connect(SocketRawData->socket, (const sockaddr*)&(data->Addr), sizeof(sockaddr_in));
	return Return;
}

bool FSocketRaw::Listen( unsigned int MaxBacklog )
{
	return listen(SocketRawData->socket, MaxBacklog) == 0 ; 
}

bool FSocketRaw::WaitForPendingConnection( bool& bHasPendingConnection, const FTimespan& WaitTime)
{
	bool bHasSucceeded = false;
	bHasPendingConnection = false;

	// Make sure socket has no error state
	if (SocketCurrentState(SocketRawData->socket, SocketRawEnum::HasError) == SocketRawEnum::No)
	{
		// Get the read state
		SocketRawEnum::Return State = SocketCurrentState(SocketRawData->socket, SocketRawEnum::CanRead, (unsigned int)WaitTime.GetTotalMilliseconds());
		// Turn the result into the outputs
		bHasSucceeded = State != SocketRawEnum::EncounteredError;
		bHasPendingConnection = State == SocketRawEnum::Yes;
	}

	return bHasSucceeded;
}

bool FSocketRaw::HasPendingData( unsigned int& PendingDataSize )
{
	if (SocketCurrentState(SocketRawData->socket, SocketRawEnum::CanRead) == SocketRawEnum::Yes)
	{
		if (ioctlsocket(SocketRawData->socket, FIONREAD, (u_long*)(&PendingDataSize)) == 0)
		{
			return (PendingDataSize > 0);
		}
	}

	return false; 
}

FSocketRaw* FSocketRaw::Accept()
{
	SOCKET NewSocket = accept(SocketRawData->socket,NULL,NULL);

	if ( NewSocket != INVALID_SOCKET )
	{
		FSocketRawData *RawData = new FSocketRawData();
		RawData->socket = NewSocket; 
		return new FSocketRaw(RawData); 
	}

	return NULL; 
}

FSocketRaw* FSocketRaw::Accept( FInternetAddrRaw& OutAddr )
{
	int  SizeOf = sizeof(sockaddr_in);
	const FInternetAddrRawData *RawData = OutAddr.GetInternalData(); 
	SOCKET NewSocket = accept(SocketRawData->socket, (sockaddr*)&(RawData->Addr), &SizeOf);

	if (NewSocket != INVALID_SOCKET)
	{
		FSocketRawData *RawData = new FSocketRawData();
		RawData->socket = NewSocket; 
		return new FSocketRaw(RawData); 
	}

	return NULL;
}

FSocketRaw::FSocketRaw( FSocketRawData* InData )
{
	SocketRawData = InData; 
}

FSocketRaw::FSocketRaw(bool IsTCP)
{
	SocketRawData = new FSocketRawData(); 
	if ( IsTCP )
	{
		 SocketRawData->socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	}
	else 
	{
		 SocketRawData->socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	}

	if (SocketRawData->socket == INVALID_SOCKET)
	{
		 delete SocketRawData; 
		 SocketRawData = NULL;
	}

}

bool FSocketRaw::SendTo( const unsigned char* Data, unsigned int Count, unsigned int & BytesSent, const FInternetAddrRaw& Destination )
{
	const FInternetAddrRawData *RawData = Destination.GetInternalData(); 
	BytesSent = sendto(SocketRawData->socket, (const char*)Data, Count, 0, (sockaddr*)&(RawData->Addr), sizeof(sockaddr_in));
	return BytesSent >= 0;
}

bool FSocketRaw::Send( const unsigned char* Data, unsigned int Count, unsigned int& BytesSent )
{
	BytesSent = send(SocketRawData->socket,(const char*)Data,Count,0);
	return BytesSent >= 0;
}

bool FSocketRaw::RecvFrom( unsigned char* Data, unsigned int BufferSize, unsigned int& BytesRead, FInternetAddrRaw& Source, unsigned int Flags )
{
	int Size = sizeof(sockaddr_in);
	sockaddr* Addr = (sockaddr*)&(Source.GetInternalData()->Addr);

	// Read into the buffer and set the source address
	int read = recvfrom(SocketRawData->socket, (char*)Data, BufferSize, Flags, Addr, &Size);
	BytesRead = read; 
	return read >= 0;
}

bool FSocketRaw::Recv( unsigned char* Data, unsigned int BufferSize, unsigned int& BytesRead, unsigned int Flags )
{
	BytesRead = recv(SocketRawData->socket, (char*)Data, BufferSize, Flags);
	return BytesRead >= 0;
}

bool FSocketRaw::WaitForRead( int WaitTime )
{
	if (SocketCurrentState(SocketRawData->socket,SocketRawEnum::CanRead, WaitTime) == SocketRawEnum::Yes)
	{
		return true;
	}
	return false; 
}

bool FSocketRaw::WaitForWrite( int WaitTime )
{
	if (SocketCurrentState(SocketRawData->socket,SocketRawEnum::CanWrite, WaitTime) == SocketRawEnum::Yes)
	{
		return true;
	}
	return false; 
}

bool FSocketRaw::WaitForReadWrite( int WaitTime )
{
	return WaitForRead( WaitTime ) || WaitForWrite(WaitTime);
}

bool FSocketRaw::GetAddress( FInternetAddrRaw& OutAddr )
{
	sockaddr *Addr = (sockaddr *)&OutAddr.GetInternalData()->Addr;
	int Size = sizeof(sockaddr_in);
	
	return getsockname(SocketRawData->socket, Addr, &Size) == 0;
}

bool FSocketRaw::SetNonBlocking( bool bIsNonBlocking /*= true*/ )
{
	u_long Value = bIsNonBlocking ? true : false;
	return ioctlsocket(SocketRawData->socket,FIONBIO,&Value) == 0;
#if 0 
	int Flags = fcntl(Socket, F_GETFL, 0);
	// Set the flag or clear it (without destroying the other flags).
	Flags = bIsNonBlocking ? Flags | O_NONBLOCK : Flags ^ (Flags & O_NONBLOCK);
	int err = fcntl(Socket, F_SETFL, Flags);
	return (err == 0 ? true : false);
#endif
}

bool FSocketRaw::SetBroadcast( bool bAllowBroadcast /*= true*/ )
{
	int Param = bAllowBroadcast ? 1 : 0;
	return setsockopt(SocketRawData->socket,SOL_SOCKET,SO_BROADCAST,(char*)&Param,sizeof(Param)) == 0;
}

bool FSocketRaw::JoinMulticastGroup( const FInternetAddrRaw& GroupAddress )
{
	ip_mreq imr;
	const FInternetAddrRawData* RawData = GroupAddress.GetInternalData(); 

	imr.imr_interface.s_addr = INADDR_ANY;
	imr.imr_multiaddr = RawData->Addr.sin_addr;

	return (setsockopt(SocketRawData->socket, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*)&imr, sizeof(imr)) == 0);
}

bool FSocketRaw::LeaveMulticastGroup( const FInternetAddrRaw& GroupAddress )
{
	ip_mreq imr;
	const FInternetAddrRawData* RawData = GroupAddress.GetInternalData(); 

	imr.imr_interface.s_addr = INADDR_ANY;
	imr.imr_multiaddr = RawData->Addr.sin_addr;

	return (setsockopt(SocketRawData->socket, IPPROTO_IP, IP_DROP_MEMBERSHIP, (char*)&imr, sizeof(imr)) == 0);
}

bool FSocketRaw::SetMulticastLoopback( bool bLoopback )
{
	return (setsockopt(SocketRawData->socket, IPPROTO_IP, IP_MULTICAST_LOOP, (char*)&bLoopback, sizeof(bLoopback)) == 0);

}

bool FSocketRaw::SetMulticastTtl( unsigned int TimeToLive )
{
	return (setsockopt( SocketRawData->socket, IPPROTO_IP, IP_MULTICAST_TTL, (char*)&TimeToLive, sizeof(TimeToLive)) == 0);
}

bool FSocketRaw::SetReuseAddr( bool bAllowReuse /*= true*/ )
{
	int Param = bAllowReuse ? 1 : 0;
	return setsockopt(SocketRawData->socket,SOL_SOCKET,SO_REUSEADDR,(char*)&Param,sizeof(Param)) == 0;
}

bool FSocketRaw::SetLinger( bool bShouldLinger /*= true*/, int Timeout /*= 0*/ )
{
	linger ling;

	ling.l_onoff = bShouldLinger;
	ling.l_linger = (u_short)Timeout;

	return setsockopt(SocketRawData->socket,SOL_SOCKET,SO_LINGER,(char*)&ling,sizeof(ling)) == 0;
}



bool FSocketRaw::SetSendBufferSize( unsigned int Size, unsigned int & NewSize )
{
	int SizeSize = sizeof(SOCKLEN);
	bool bOk = setsockopt(SocketRawData->socket,SOL_SOCKET,SO_RCVBUF,(char*)&Size,sizeof(SOCKLEN)) == 0;

	// Read the value back in case the size was modified
	getsockopt(SocketRawData->socket,SOL_SOCKET,SO_RCVBUF,(char*)&NewSize, &SizeSize);

	return bOk;
}

bool FSocketRaw::SetReceiveBufferSize( unsigned int Size, unsigned int & NewSize )
{
	int SizeSize = sizeof(SOCKLEN);
	bool bOk = setsockopt(SocketRawData->socket,SOL_SOCKET,SO_RCVBUF,(char*)&Size,sizeof(SOCKLEN)) == 0;

	// Read the value back in case the size was modified
	getsockopt(SocketRawData->socket,SOL_SOCKET,SO_RCVBUF,(char*)&NewSize, &SizeSize);

	return bOk;
}

unsigned int FSocketRaw::GetPortNo(bool& Succeeded )
{
	sockaddr_in Addr;

	SOCKLEN Size = sizeof(sockaddr_in);

	// Figure out what ip/port we are bound to
	Succeeded = getsockname(SocketRawData->socket, (sockaddr*)&Addr, &Size) == 0;
	// Read the port number
	return ntohs(Addr.sin_port);
}

bool FSocketRaw::IsValid()
{
	if ( SocketRawData && SocketRawData->socket != INVALID_SOCKET )
		return true; 
	return false; 
}

bool FSocketRaw::GetHostByName( const char *NAME, FInternetAddrRaw& Address )
{
	addrinfo* AddrInfo = nullptr;

	// We are only interested in IPv4 addresses.
	addrinfo HintAddrInfo;
	memset(&HintAddrInfo, 0, sizeof(HintAddrInfo));
	HintAddrInfo.ai_family = AF_INET;

	int ErrorCode = getaddrinfo(NAME, nullptr, &HintAddrInfo, &AddrInfo);
	if (ErrorCode != 0)
	{
		if (AddrInfo->ai_addr == nullptr)
		{
			return false;
		}

		sockaddr_in* IPv4Address = reinterpret_cast<sockaddr_in*>(AddrInfo->ai_addr);
		unsigned int HostIP = ntohl(IPv4Address->sin_addr.s_addr);
		Address.SetIp(HostIP);
		return true;
	}
	return false;
}

bool FSocketRaw::GetHostName(const char *NAME )
{
	bool bRead = gethostname(const_cast<char *>(NAME),256) == 0;
	return bRead;
}

WSADATA wsaData;
bool FSocketRaw::Init()
{
	_CrtSetReportMode( _CRT_ASSERT, _CRTDBG_MODE_FILE | _CRTDBG_MODE_WNDW );
	_CrtSetReportFile( _CRT_ASSERT, _CRTDBG_FILE_STDERR );

	int iResult;
	// Initialize Winsock
	iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
	if (iResult != 0) {
		printf("WSAStartup failed: %d\n", iResult);
		return false;
	}
	return true;
}






