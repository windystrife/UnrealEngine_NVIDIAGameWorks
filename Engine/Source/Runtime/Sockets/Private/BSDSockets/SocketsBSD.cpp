// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "BSDSockets/SocketsBSD.h"

#if PLATFORM_HAS_BSD_SOCKETS

#include "BSDSockets/IPAddressBSD.h"
#include "BSDSockets/SocketSubsystemBSD.h"
//#include "Net/NetworkProfiler.h"

#if PLATFORM_HTML5 
	typedef unsigned long u_long;
#endif 


/* FSocket overrides
 *****************************************************************************/

bool FSocketBSD::Close(void)
{
	if (Socket != INVALID_SOCKET)
	{
		int32 error = closesocket(Socket);
		Socket = INVALID_SOCKET;
		return error == 0;
	}
	return false;
}


bool FSocketBSD::Bind(const FInternetAddr& Addr)
{
	return bind(Socket, (sockaddr*)(FInternetAddrBSD&)Addr, sizeof(sockaddr_in)) == 0;
}


bool FSocketBSD::Connect(const FInternetAddr& Addr)
{
	int32 Return = connect(Socket, (sockaddr*)(FInternetAddrBSD&)Addr, sizeof(sockaddr_in));
	
	check(SocketSubsystem);
	ESocketErrors Error = SocketSubsystem->TranslateErrorCode(Return);

	// EWOULDBLOCK is not an error, and EINPROGRESS is fine on initial connection as it may still be creating for nonblocking sockets
	return ((Error == SE_NO_ERROR) || (Error == SE_EWOULDBLOCK) || (Error == SE_EINPROGRESS));
}


bool FSocketBSD::Listen(int32 MaxBacklog)
{
	return listen(Socket, MaxBacklog) == 0;
}


bool FSocketBSD::WaitForPendingConnection(bool& bHasPendingConnection, const FTimespan& WaitTime)
{
	bool bHasSucceeded = false;
	bHasPendingConnection = false;

	// make sure socket has no error state
	if (HasState(ESocketBSDParam::HasError) == ESocketBSDReturn::No)
	{
		// get the read state
		ESocketBSDReturn State = HasState(ESocketBSDParam::CanRead, WaitTime);
		
		// turn the result into the outputs
		bHasSucceeded = State != ESocketBSDReturn::EncounteredError;
		bHasPendingConnection = State == ESocketBSDReturn::Yes;
	}

	return bHasSucceeded;
}


bool FSocketBSD::HasPendingData(uint32& PendingDataSize)
{
	PendingDataSize = 0;

	// make sure socket has no error state
	if (HasState(ESocketBSDParam::CanRead) == ESocketBSDReturn::Yes)
	{
#if PLATFORM_HAS_BSD_SOCKET_FEATURE_IOCTL
		// See if there is any pending data on the read socket
		if (ioctlsocket(Socket, FIONREAD, (u_long*)(&PendingDataSize)) == 0)
#endif
		{
			return (PendingDataSize > 0);
		}
	}

	return false;
}


FSocket* FSocketBSD::Accept(const FString& InSocketDescription)
{
	SOCKET NewSocket = accept(Socket, NULL, NULL);

	if (NewSocket != INVALID_SOCKET)
	{
		// we need the subclass to create the actual FSocket object
		check(SocketSubsystem);
		FSocketSubsystemBSD* BSDSystem = static_cast<FSocketSubsystemBSD*>(SocketSubsystem);
		return BSDSystem->InternalBSDSocketFactory(NewSocket, SocketType, InSocketDescription);
	}

	return NULL;
}


FSocket* FSocketBSD::Accept(FInternetAddr& OutAddr, const FString& InSocketDescription)
{
	SOCKLEN SizeOf = sizeof(sockaddr_in);
	SOCKET NewSocket = accept(Socket, *(FInternetAddrBSD*)(&OutAddr), &SizeOf);

	if (NewSocket != INVALID_SOCKET)
	{
		// we need the subclass to create the actual FSocket object
		check(SocketSubsystem);
		FSocketSubsystemBSD* BSDSystem = static_cast<FSocketSubsystemBSD*>(SocketSubsystem);
		return BSDSystem->InternalBSDSocketFactory(NewSocket, SocketType, InSocketDescription);
	}

	return NULL;
}


bool FSocketBSD::SendTo(const uint8* Data, int32 Count, int32& BytesSent, const FInternetAddr& Destination)
{
	// Write the data and see how much was written
	BytesSent = sendto(Socket, (const char*)Data, Count, 0, (FInternetAddrBSD&)Destination, sizeof(sockaddr_in));

//	NETWORK_PROFILER(FSocket::SendTo(Data,Count,BytesSent,Destination));

	bool Result = BytesSent >= 0;
	if (Result)
	{
		LastActivityTime = FDateTime::UtcNow();
	}
	return Result;
}


bool FSocketBSD::Send(const uint8* Data, int32 Count, int32& BytesSent)
{
	BytesSent = send(Socket,(const char*)Data,Count,0);

//	NETWORK_PROFILER(FSocket::Send(Data,Count,BytesSent));

	bool Result = BytesSent >= 0;
	if (Result)
	{
		LastActivityTime = FDateTime::UtcNow();
	}
	return Result;
}


bool FSocketBSD::RecvFrom(uint8* Data, int32 BufferSize, int32& BytesRead, FInternetAddr& Source, ESocketReceiveFlags::Type Flags)
{
	SOCKLEN Size = sizeof(sockaddr_in);
	sockaddr& Addr = *(FInternetAddrBSD&)Source;

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

	LastActivityTime = FDateTime::UtcNow();

	return true;
}


bool FSocketBSD::Recv(uint8* Data, int32 BufferSize, int32& BytesRead, ESocketReceiveFlags::Type Flags)
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

	LastActivityTime = FDateTime::UtcNow();

	return true;
}


bool FSocketBSD::Wait(ESocketWaitConditions::Type Condition, FTimespan WaitTime)
{
	if ((Condition == ESocketWaitConditions::WaitForRead) || (Condition == ESocketWaitConditions::WaitForReadOrWrite))
	{
		if (HasState(ESocketBSDParam::CanRead, WaitTime) == ESocketBSDReturn::Yes)
		{
			return true;
		}
	}

	if ((Condition == ESocketWaitConditions::WaitForWrite) || (Condition == ESocketWaitConditions::WaitForReadOrWrite))
	{
		if (HasState(ESocketBSDParam::CanWrite, WaitTime) == ESocketBSDReturn::Yes)
		{
			return true;
		}
	}

	return false;
}


ESocketConnectionState FSocketBSD::GetConnectionState(void)
{
	ESocketConnectionState CurrentState = SCS_ConnectionError;

	// look for an existing error
	if (HasState(ESocketBSDParam::HasError) == ESocketBSDReturn::No)
	{
		if (FDateTime::UtcNow() - LastActivityTime > FTimespan::FromSeconds(5))
		{
			// get the write state
			ESocketBSDReturn WriteState = HasState(ESocketBSDParam::CanWrite, FTimespan::FromMilliseconds(1));
			ESocketBSDReturn ReadState = HasState(ESocketBSDParam::CanRead, FTimespan::FromMilliseconds(1));
		
			// translate yes or no (error is already set)
			if (WriteState == ESocketBSDReturn::Yes || ReadState == ESocketBSDReturn::Yes)
			{
				CurrentState = SCS_Connected;
				LastActivityTime = FDateTime::UtcNow();
			}
			else if (WriteState == ESocketBSDReturn::No && ReadState == ESocketBSDReturn::No)
			{
				CurrentState = SCS_NotConnected;
			}
		}
		else
		{
			CurrentState = SCS_Connected;
		}
	}

	return CurrentState;
}


void FSocketBSD::GetAddress(FInternetAddr& OutAddr)
{
	FInternetAddrBSD& Addr = (FInternetAddrBSD&)OutAddr;
	SOCKLEN Size = sizeof(sockaddr_in);

	// Figure out what ip/port we are bound to
	bool bOk = getsockname(Socket, Addr, &Size) == 0;

	if (bOk == false)
	{
		check(SocketSubsystem);
		UE_LOG(LogSockets, Error, TEXT("Failed to read address for socket (%s)"), SocketSubsystem->GetSocketError());
	}
}


bool FSocketBSD::GetPeerAddress(FInternetAddr& OutAddr)
{
	FInternetAddrBSD& Addr = (FInternetAddrBSD&)OutAddr;
	SOCKLEN Size = sizeof(sockaddr_in);

	// Figure out what ip/port we are bound to
	int Result = getpeername(Socket, Addr, &Size);

	if (Result != 0)
	{
		check(SocketSubsystem);
		UE_LOG(LogSockets, Warning, TEXT("Failed to read address for socket (%s) with error %d"), SocketSubsystem->GetSocketError(), Result);
	}
	return Result == 0;
}

bool FSocketBSD::SetNonBlocking(bool bIsNonBlocking)
{
	u_long Value = bIsNonBlocking ? true : false;

#if PLATFORM_HTML5 
	// can't have blocking sockets.
	ensureMsgf(bIsNonBlocking, TEXT("Can't have blocking sockets on HTML5"));
    return true;
#else 

#if PLATFORM_HAS_BSD_SOCKET_FEATURE_WINSOCKETS
	return ioctlsocket(Socket,FIONBIO,&Value) == 0;
#else 
	int Flags = fcntl(Socket, F_GETFL, 0);
	//Set the flag or clear it, without destroying the other flags.
	Flags = bIsNonBlocking ? Flags | O_NONBLOCK : Flags ^ (Flags & O_NONBLOCK);
	int err = fcntl(Socket, F_SETFL, Flags);
	return (err == 0 ? true : false);
#endif
#endif 
}


bool FSocketBSD::SetBroadcast(bool bAllowBroadcast)
{
	int Param = bAllowBroadcast ? 1 : 0;
	return setsockopt(Socket,SOL_SOCKET,SO_BROADCAST,(char*)&Param,sizeof(Param)) == 0;
}


bool FSocketBSD::JoinMulticastGroup(const FInternetAddr& GroupAddress)
{
	ip_mreq imr;

	imr.imr_interface.s_addr = INADDR_ANY;
	imr.imr_multiaddr = ((sockaddr_in*)&**((FInternetAddrBSD*)(&GroupAddress)))->sin_addr;

	return (setsockopt(Socket, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*)&imr, sizeof(imr)) == 0);
}


bool FSocketBSD::LeaveMulticastGroup(const FInternetAddr& GroupAddress)
{
	ip_mreq imr;

	imr.imr_interface.s_addr = INADDR_ANY;
	imr.imr_multiaddr = ((sockaddr_in*)&**((FInternetAddrBSD*)(&GroupAddress)))->sin_addr;

	return (setsockopt(Socket, IPPROTO_IP, IP_DROP_MEMBERSHIP, (char*)&imr, sizeof(imr)) == 0);
}


bool FSocketBSD::SetMulticastLoopback(bool bLoopback)
{
	return (setsockopt(Socket, IPPROTO_IP, IP_MULTICAST_LOOP, (char*)&bLoopback, sizeof(bLoopback)) == 0);
}


bool FSocketBSD::SetMulticastTtl(uint8 TimeToLive)
{
	return (setsockopt(Socket, IPPROTO_IP, IP_MULTICAST_TTL, (char*)&TimeToLive, sizeof(TimeToLive)) == 0);
}


bool FSocketBSD::SetReuseAddr(bool bAllowReuse)
{
	int Param = bAllowReuse ? 1 : 0;
	return setsockopt(Socket,SOL_SOCKET,SO_REUSEADDR,(char*)&Param,sizeof(Param)) == 0;
}


bool FSocketBSD::SetLinger(bool bShouldLinger,int32 Timeout)
{
	linger ling;

	ling.l_onoff = bShouldLinger;
	ling.l_linger = Timeout;

	return setsockopt(Socket,SOL_SOCKET,SO_LINGER,(char*)&ling,sizeof(ling)) == 0;
}


bool FSocketBSD::SetRecvErr(bool bUseErrorQueue)
{
	// Not supported, but return true to avoid spurious log messages
	return true;
}


bool FSocketBSD::SetSendBufferSize(int32 Size,int32& NewSize)
{
	SOCKLEN SizeSize = sizeof(int32);
	bool bOk = setsockopt(Socket,SOL_SOCKET,SO_SNDBUF,(char*)&Size,sizeof(int32)) == 0;

	// Read the value back in case the size was modified
	getsockopt(Socket,SOL_SOCKET,SO_SNDBUF,(char*)&NewSize, &SizeSize);

	return bOk;
}


bool FSocketBSD::SetReceiveBufferSize(int32 Size,int32& NewSize)
{
	SOCKLEN SizeSize = sizeof(int32);
	bool bOk = setsockopt(Socket,SOL_SOCKET,SO_RCVBUF,(char*)&Size,sizeof(int32)) == 0;

	// Read the value back in case the size was modified
	getsockopt(Socket,SOL_SOCKET,SO_RCVBUF,(char*)&NewSize, &SizeSize);

	return bOk;
}


int32 FSocketBSD::GetPortNo(void)
{
	sockaddr_in Addr;

	SOCKLEN Size = sizeof(sockaddr_in);

	// Figure out what ip/port we are bound to
	bool bOk = getsockname(Socket, (sockaddr*)&Addr, &Size) == 0;
	
	if (bOk == false)
	{
		check(SocketSubsystem);
		UE_LOG(LogSockets, Error, TEXT("Failed to read address for socket (%s)"), SocketSubsystem->GetSocketError());
	}

	// Read the port number
	return ntohs(Addr.sin_port);
}


/* FSocketBSD implementation
*****************************************************************************/

ESocketBSDReturn FSocketBSD::HasState(ESocketBSDParam State, FTimespan WaitTime)
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
	case ESocketBSDParam::CanRead:
		SelectStatus = select(Socket + 1, &SocketSet, NULL, NULL, &Time);
		break;

	case ESocketBSDParam::CanWrite:
		SelectStatus = select(Socket + 1, NULL, &SocketSet, NULL, &Time);
		break;

	case ESocketBSDParam::HasError:
		SelectStatus = select(Socket + 1, NULL, NULL, &SocketSet, &Time);
		break;
	}

	// if the select returns a positive number, the socket had the state, 0 means didn't have it, and negative is API error condition (not socket's error state)
	return SelectStatus > 0 ? ESocketBSDReturn::Yes :
		SelectStatus == 0 ? ESocketBSDReturn::No :
		ESocketBSDReturn::EncounteredError;
#else
	UE_LOG(LogSockets, Fatal, TEXT("This platform doesn't support select(), but FSocketBSD::HasState was not overridden"));
	return ESocketBSDReturn::EncounteredError;
#endif
}


#endif	//PLATFORM_HAS_BSD_SOCKETS
