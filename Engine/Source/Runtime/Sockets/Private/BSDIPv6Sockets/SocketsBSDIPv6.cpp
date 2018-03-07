// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "BSDIPv6Sockets/SocketsBSDIPv6.h"

#if PLATFORM_HAS_BSD_IPV6_SOCKETS

#include "BSDIPv6Sockets/IPAddressBSDIPv6.h"
#include "BSDIPv6Sockets/SocketSubsystemBSDIPv6.h"
//#include "Net/NetworkProfiler.h"

bool FSocketBSDIPv6::Close(void)
{
	if (Socket != INVALID_SOCKET)
	{
		int32 error = closesocket(Socket);
		Socket = INVALID_SOCKET;
		return error == 0;
	}
	return false;
}


bool FSocketBSDIPv6::Bind(const FInternetAddr& Addr)
{
	return bind(Socket, (sockaddr*)(FInternetAddrBSDIPv6&)Addr, sizeof(sockaddr_in6)) == 0;
}


bool FSocketBSDIPv6::Connect(const FInternetAddr& Addr)
{
	int32 Return = connect(Socket, (sockaddr*)(FInternetAddrBSDIPv6&)Addr, sizeof(sockaddr_in6));
	
	check(SocketSubsystem);
	ESocketErrors Error = SocketSubsystem->TranslateErrorCode(Return);

	// "would block" is not an error
	return ((Error == SE_NO_ERROR) || (Error == SE_EWOULDBLOCK));
}


bool FSocketBSDIPv6::Listen(int32 MaxBacklog)
{
	return listen(Socket, MaxBacklog) == 0;
}

EIPv6SocketInternalState::Return FSocketBSDIPv6::HasState(EIPv6SocketInternalState::Param State, FTimespan WaitTime)
{
#if PLATFORM_HAS_BSD_SOCKET_FEATURE_SELECT
	// convert WaitTime to a timeval
	timeval Time;
	Time.tv_sec = (int32)WaitTime.GetTotalSeconds();
	Time.tv_usec = WaitTime.GetFractionMicro();

	fd_set SocketSet;

	// Set up the socket sets we are interested in (just this one)
	FD_ZERO(&SocketSet);
	FD_SET(Socket, &SocketSet);

	// Check the status of the state
	int32 SelectStatus = 0;
	switch (State)
	{
		case EIPv6SocketInternalState::CanRead:
			SelectStatus = select(Socket + 1, &SocketSet, NULL, NULL, &Time);
			break;
		case EIPv6SocketInternalState::CanWrite:
			SelectStatus = select(Socket + 1, NULL, &SocketSet, NULL, &Time);
			break;
		case EIPv6SocketInternalState::HasError:
			SelectStatus = select(Socket + 1, NULL, NULL, &SocketSet, &Time);
			break;
	}

	// if the select returns a positive number, the socket had the state, 0 means didn't have it, and negative is API error condition (not socket's error state)
	return SelectStatus > 0  ? EIPv6SocketInternalState::Yes : 
		   SelectStatus == 0 ? EIPv6SocketInternalState::No : 
		   EIPv6SocketInternalState::EncounteredError;
#else
	UE_LOG(LogSockets, Fatal, TEXT("This platform doesn't support select(), but FSocketBSD::HasState was not overridden"));
	return EIPv6SocketInternalState::EncounteredError;
#endif
}

bool FSocketBSDIPv6::WaitForPendingConnection(bool& bHasPendingConnection, const FTimespan& WaitTime)
{
	bool bHasSucceeded = false;
	bHasPendingConnection = false;

	// make sure socket has no error state
	if (HasState(EIPv6SocketInternalState::HasError) == EIPv6SocketInternalState::No)
	{
		// get the read state
		EIPv6SocketInternalState::Return State = HasState(EIPv6SocketInternalState::CanRead, WaitTime);
		
		// turn the result into the outputs
		bHasSucceeded = State != EIPv6SocketInternalState::EncounteredError;
		bHasPendingConnection = State == EIPv6SocketInternalState::Yes;
	}

	return bHasSucceeded;
}


bool FSocketBSDIPv6::HasPendingData(uint32& PendingDataSize)
{
	// make sure socket has no error state
	if (HasState(EIPv6SocketInternalState::CanRead) == EIPv6SocketInternalState::Yes)
	{
#if PLATFORM_HAS_BSD_SOCKET_FEATURE_IOCTL
		// See if there is any pending data on the read socket
		if(ioctlsocket(Socket, FIONREAD, (u_long*)(&PendingDataSize)) == 0)
#endif
		{
			return (PendingDataSize > 0);
		}
	}

	return false;
}


FSocket* FSocketBSDIPv6::Accept(const FString& SocketDescription)
{
	SOCKET NewSocket = accept(Socket,NULL,NULL);

	if (NewSocket != INVALID_SOCKET)
	{
		// we need the subclass to create the actual FSocket object
		check(SocketSubsystem);
		FSocketSubsystemBSDIPv6* BSDSystem = static_cast<FSocketSubsystemBSDIPv6*>(SocketSubsystem);
		return BSDSystem->InternalBSDSocketFactory(NewSocket, SocketType, SocketDescription);
	}

	return NULL;
}


FSocket* FSocketBSDIPv6::Accept(FInternetAddr& OutAddr, const FString& SocketDescription)
{
	SOCKLEN SizeOf = sizeof(sockaddr_in6);
	SOCKET NewSocket = accept(Socket, *(FInternetAddrBSDIPv6*)(&OutAddr), &SizeOf);

	if (NewSocket != INVALID_SOCKET)
	{
		// we need the subclass to create the actual FSocket object
		check(SocketSubsystem);
		FSocketSubsystemBSDIPv6* BSDSystem = static_cast<FSocketSubsystemBSDIPv6*>(SocketSubsystem);
		return BSDSystem->InternalBSDSocketFactory(NewSocket, SocketType, SocketDescription);
	}

	return NULL;
}


bool FSocketBSDIPv6::SendTo(const uint8* Data, int32 Count, int32& BytesSent, const FInternetAddr& Destination)
{
	// Write the data and see how much was written
	sockaddr* ToAddr = (FInternetAddrBSDIPv6&)Destination;
	sockaddr_in6* ToAddr6 = (sockaddr_in6*)ToAddr;

	BytesSent = sendto(Socket, (const char*)Data, Count, 0, (FInternetAddrBSDIPv6&)Destination, sizeof(sockaddr_in6));
	if(BytesSent == SOCKET_ERROR)
	{
		ESocketErrors SockError = SocketSubsystem->GetLastErrorCode();
		UE_LOG(LogSockets, Log, TEXT("sendto error: (ESocketErrors:%d)"), SockError);
	}
//	NETWORK_PROFILER(FSocket::SendTo(Data,Count,BytesSent,Destination));

	return BytesSent >= 0;
}


bool FSocketBSDIPv6::Send(const uint8* Data, int32 Count, int32& BytesSent)
{
	BytesSent = send(Socket,(const char*)Data,Count,0);

//	NETWORK_PROFILER(FSocket::Send(Data,Count,BytesSent));

	return BytesSent >= 0;
}


bool FSocketBSDIPv6::RecvFrom(uint8* Data, int32 BufferSize, int32& BytesRead, FInternetAddr& Source, ESocketReceiveFlags::Type Flags)
{
	SOCKLEN Size = sizeof(sockaddr_in6);
	sockaddr& Addr = *(FInternetAddrBSDIPv6&)Source;

	const int TranslatedFlags = TranslateFlags(Flags);

	// Read into the buffer and set the source address
	BytesRead = recvfrom(Socket, (char*)Data, BufferSize, TranslatedFlags, &Addr, &Size);

//	NETWORK_PROFILER(FSocket::RecvFrom(Data,BufferSize,BytesRead,Source));

	if (BytesRead < 0 && SocketSubsystem->TranslateErrorCode(BytesRead) == SE_EWOULDBLOCK)
	{
		// EWOULDBLOCK is not an error condition
		BytesRead = 0;
	}
	else if (BytesRead <= 0) // 0 means gracefully closed
	{
		BytesRead = 0;
		return false;
	}

	return true;
}


bool FSocketBSDIPv6::Recv(uint8* Data, int32 BufferSize, int32& BytesRead, ESocketReceiveFlags::Type Flags)
{
	const int TranslatedFlags = TranslateFlags(Flags);
	BytesRead = recv(Socket, (char*)Data, BufferSize, TranslatedFlags);

//	NETWORK_PROFILER(FSocket::Recv(Data,BufferSize,BytesRead));

	if (BytesRead < 0 && SocketSubsystem->TranslateErrorCode(BytesRead) == SE_EWOULDBLOCK)
	{
		// EWOULDBLOCK is not an error condition
		BytesRead = 0;
	}
	else if (BytesRead <= 0) // 0 means gracefully closed
	{
		BytesRead = 0;
		return false;
	}

	return true;
}

bool FSocketBSDIPv6::Wait(ESocketWaitConditions::Type Condition, FTimespan WaitTime)
{
	if ((Condition == ESocketWaitConditions::WaitForRead) || (Condition == ESocketWaitConditions::WaitForReadOrWrite))
	{
		if (HasState(EIPv6SocketInternalState::CanRead, WaitTime) == EIPv6SocketInternalState::Yes)
		{
			return true;
		}
	}

	if ((Condition == ESocketWaitConditions::WaitForWrite) || (Condition == ESocketWaitConditions::WaitForReadOrWrite))
	{
		if (HasState(EIPv6SocketInternalState::CanWrite, WaitTime) == EIPv6SocketInternalState::Yes)
		{
			return true;
		}
	}

	return false;
}


ESocketConnectionState FSocketBSDIPv6::GetConnectionState(void)
{
	ESocketConnectionState CurrentState = SCS_ConnectionError;

	// look for an existing error
	if (HasState(EIPv6SocketInternalState::HasError) == EIPv6SocketInternalState::No)
	{
		// get the write state
		EIPv6SocketInternalState::Return State = HasState(EIPv6SocketInternalState::CanWrite);
		
		// translate yes or no (error is already set)
		if (State == EIPv6SocketInternalState::Yes)
		{
			CurrentState = SCS_Connected;
		}
		else if (State == EIPv6SocketInternalState::No)
		{
			CurrentState = SCS_NotConnected;
		}
	}

	return CurrentState;
}


void FSocketBSDIPv6::GetAddress(FInternetAddr& OutAddr)
{
	FInternetAddrBSDIPv6& Addr = (FInternetAddrBSDIPv6&)OutAddr;
	SOCKLEN Size = sizeof(sockaddr_in6);

	// Figure out what ip/port we are bound to
	bool bOk = getsockname(Socket, Addr, &Size) == 0;

	if (bOk == false)
	{
		check(SocketSubsystem);
		UE_LOG(LogSockets, Error, TEXT("Failed to read address for socket (%s)"), SocketSubsystem->GetSocketError());
	}
}

bool FSocketBSDIPv6::GetPeerAddress(FInternetAddr& OutAddr)
{
	FInternetAddrBSDIPv6& Addr = (FInternetAddrBSDIPv6&)OutAddr;
	SOCKLEN Size = sizeof(sockaddr_in6);

	// Figure out what ip/port we are bound to
	int Result = getpeername(Socket, Addr, &Size);

	if (Result != 0)
	{
		check(SocketSubsystem);
		UE_LOG(LogSockets, Warning, TEXT("Failed to read address for socket (%s) with error %d"), SocketSubsystem->GetSocketError(), Result);
	}
	return Result == 0;
}


bool FSocketBSDIPv6::SetNonBlocking(bool bIsNonBlocking)
{
	u_long Value = bIsNonBlocking ? true : false;

#if PLATFORM_HAS_BSD_SOCKET_FEATURE_WINSOCKETS
	return ioctlsocket(Socket,FIONBIO,&Value) == 0;
#else
	int Flags = fcntl(Socket, F_GETFL, 0);
	//Set the flag or clear it, without destroying the other flags.
	Flags = bIsNonBlocking ? Flags | O_NONBLOCK : Flags ^ (Flags & O_NONBLOCK);
	int err = fcntl(Socket, F_SETFL, Flags);
	return (err == 0 ? true : false);
#endif
}


bool FSocketBSDIPv6::SetBroadcast(bool bAllowBroadcast)
{
	int Param = bAllowBroadcast ? 1 : 0;
	return setsockopt(Socket,SOL_SOCKET,SO_BROADCAST,(char*)&Param,sizeof(Param)) == 0;
}


bool FSocketBSDIPv6::JoinMulticastGroup(const FInternetAddr& GroupAddress)
{
	ipv6_mreq imr;

	imr.ipv6mr_interface = 0;
	imr.ipv6mr_multiaddr = ((sockaddr_in6*)&**((FInternetAddrBSDIPv6*)(&GroupAddress)))->sin6_addr;

	return (setsockopt(Socket, IPPROTO_IPV6, IP_ADD_MEMBERSHIP, (char*)&imr, sizeof(imr)) == 0);
}


bool FSocketBSDIPv6::LeaveMulticastGroup(const FInternetAddr& GroupAddress)
{
	ipv6_mreq imr;

	imr.ipv6mr_interface = 0;
	imr.ipv6mr_multiaddr = ((sockaddr_in6*)&**((FInternetAddrBSDIPv6*)(&GroupAddress)))->sin6_addr;

	return (setsockopt(Socket, IPPROTO_IPV6, IP_DROP_MEMBERSHIP, (char*)&imr, sizeof(imr)) == 0);
}


bool FSocketBSDIPv6::SetMulticastLoopback(bool bLoopback)
{
	return (setsockopt(Socket, IPPROTO_IPV6, IP_MULTICAST_LOOP, (char*)&bLoopback, sizeof(bLoopback)) == 0);
}


bool FSocketBSDIPv6::SetMulticastTtl(uint8 TimeToLive)
{
	return (setsockopt(Socket, IPPROTO_IPV6, IP_MULTICAST_TTL, (char*)&TimeToLive, sizeof(TimeToLive)) == 0);
}


bool FSocketBSDIPv6::SetReuseAddr(bool bAllowReuse)
{
	int Param = bAllowReuse ? 1 : 0;
	return setsockopt(Socket,SOL_SOCKET,SO_REUSEADDR,(char*)&Param,sizeof(Param)) == 0;
}


bool FSocketBSDIPv6::SetLinger(bool bShouldLinger,int32 Timeout)
{
	linger ling;

	ling.l_onoff = bShouldLinger;
	ling.l_linger = Timeout;

	return setsockopt(Socket,SOL_SOCKET,SO_LINGER,(char*)&ling,sizeof(ling)) == 0;
}


bool FSocketBSDIPv6::SetRecvErr(bool bUseErrorQueue)
{
	// Not supported, but return true to avoid spurious log messages
	return true;
}


bool FSocketBSDIPv6::SetSendBufferSize(int32 Size,int32& NewSize)
{
	SOCKLEN SizeSize = sizeof(int32);
	bool bOk = setsockopt(Socket,SOL_SOCKET,SO_SNDBUF,(char*)&Size,sizeof(int32)) == 0;

	// Read the value back in case the size was modified
	getsockopt(Socket,SOL_SOCKET,SO_SNDBUF,(char*)&NewSize, &SizeSize);

	return bOk;
}


bool FSocketBSDIPv6::SetReceiveBufferSize(int32 Size,int32& NewSize)
{
	SOCKLEN SizeSize = sizeof(int32);
	bool bOk = setsockopt(Socket,SOL_SOCKET,SO_RCVBUF,(char*)&Size,sizeof(int32)) == 0;

	// Read the value back in case the size was modified
	getsockopt(Socket,SOL_SOCKET,SO_RCVBUF,(char*)&NewSize, &SizeSize);

	return bOk;
}


int32 FSocketBSDIPv6::GetPortNo(void)
{
	sockaddr_in6 Addr;

	SOCKLEN Size = sizeof(sockaddr_in6);

	// Figure out what ip/port we are bound to
	bool bOk = getsockname(Socket, (sockaddr*)&Addr, &Size) == 0;
	
	if (bOk == false)
	{
		check(SocketSubsystem);
		UE_LOG(LogSockets, Error, TEXT("Failed to read address for socket (%s)"), SocketSubsystem->GetSocketError());
	}

	// Read the port number
	return ntohs(Addr.sin6_port);
}

bool FSocketBSDIPv6::SetIPv6Only(bool bIPv6Only)
{
	int v6only = bIPv6Only ? 1 : 0;
	bool bOk = setsockopt(Socket,IPPROTO_IPV6,IPV6_V6ONLY,(char*) &v6only,sizeof( v6only )) == 0;

	if(bOk == false)
	{
		check(SocketSubsystem);
		UE_LOG(LogSockets, Error, TEXT("Failed to set sock opt for socket (%s)"), SocketSubsystem->GetSocketError());
	}

	return bOk;
}

#endif
