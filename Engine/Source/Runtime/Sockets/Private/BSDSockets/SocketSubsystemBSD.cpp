// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "BSDSockets/SocketSubsystemBSD.h"
#include "Misc/ScopeLock.h"

#if PLATFORM_HAS_BSD_SOCKETS

#include "IPAddress.h"
#include "BSDSockets/IPAddressBSD.h"
#include "BSDSockets/SocketsBSD.h"
#include <errno.h>

FSocketBSD* FSocketSubsystemBSD::InternalBSDSocketFactory(SOCKET Socket, ESocketType SocketType, const FString& SocketDescription)
{
	// return a new socket object
	return new FSocketBSD(Socket, SocketType, SocketDescription, this);
}


FSocket* FSocketSubsystemBSD::CreateSocket(const FName& SocketType, const FString& SocketDescription, bool bForceUDP)
{
	SOCKET Socket = INVALID_SOCKET;
	FSocket* NewSocket = nullptr;
	int PlatformSpecificTypeFlags = 0;

#if PLATFORM_HAS_BSD_SOCKET_FEATURE_CLOSE_ON_EXEC
	PlatformSpecificTypeFlags = SOCK_CLOEXEC;
#endif // PLATFORM_HAS_BSD_SOCKET_FEATURE_CLOSE_ON_EXEC

	switch (SocketType.GetComparisonIndex())
	{
	case NAME_DGram:
		// Creates a data gram (UDP) socket
		Socket = socket(AF_INET, SOCK_DGRAM | PlatformSpecificTypeFlags, IPPROTO_UDP);
		NewSocket = (Socket != INVALID_SOCKET) ? InternalBSDSocketFactory(Socket, SOCKTYPE_Datagram, SocketDescription) : nullptr;
		break;

	case NAME_Stream:
		// Creates a stream (TCP) socket
		Socket = socket(AF_INET, SOCK_STREAM | PlatformSpecificTypeFlags, IPPROTO_TCP);
		NewSocket = (Socket != INVALID_SOCKET) ? InternalBSDSocketFactory(Socket, SOCKTYPE_Streaming, SocketDescription) : nullptr;
		break;

	default:
		break;
	}

	if (!NewSocket)
	{
		UE_LOG(LogSockets, Warning, TEXT("Failed to create socket %s [%s]"), *SocketType.ToString(), *SocketDescription);
	}

	return NewSocket;
}


void FSocketSubsystemBSD::DestroySocket(FSocket* Socket)
{
	delete Socket;
}


ESocketErrors FSocketSubsystemBSD::GetHostByName(const ANSICHAR* HostName, FInternetAddr& OutAddr)
{
#if PLATFORM_HAS_BSD_SOCKET_FEATURE_GETADDRINFO
	FScopeLock ScopeLock(&HostByNameSynch);
	addrinfo* AddrInfo = nullptr;

	// Limit the IP Addresses we get back to just IPv4
	addrinfo HintAddrInfo;
	FMemory::Memzero(&HintAddrInfo, sizeof(HintAddrInfo));
	HintAddrInfo.ai_family = AF_INET;

	int32 ErrorCode = getaddrinfo(HostName, nullptr, &HintAddrInfo, &AddrInfo);
	ESocketErrors SocketError = TranslateGAIErrorCode(ErrorCode);
	if (SocketError == SE_NO_ERROR)
	{
		for (; AddrInfo != nullptr; AddrInfo = AddrInfo->ai_next)
		{
			if (AddrInfo->ai_family == AF_INET)
			{
				sockaddr_in* IPv4SockAddr = reinterpret_cast<sockaddr_in*>(AddrInfo->ai_addr);
				if (IPv4SockAddr != nullptr)
				{
					uint32 HostIP = ntohl(IPv4SockAddr->sin_addr.s_addr);
					static_cast<FInternetAddrBSD&>(OutAddr).SetIp(HostIP);
					freeaddrinfo(AddrInfo);
					return SE_NO_ERROR;
				}
			}
		}
		freeaddrinfo(AddrInfo);
		return SE_HOST_NOT_FOUND;
	}
	return SocketError;
#else
	UE_LOG(LogSockets, Error, TEXT("Platform has no getaddrinfo(), but did not override FSocketSubsystem::GetHostByName()"));
	return SE_NO_RECOVERY;
#endif
}


bool FSocketSubsystemBSD::GetHostName(FString& HostName)
{
#if PLATFORM_HAS_BSD_SOCKET_FEATURE_GETHOSTNAME
	ANSICHAR Buffer[256];
	bool bRead = gethostname(Buffer,256) == 0;
	if (bRead == true)
	{
		HostName = Buffer;
	}
	return bRead;
#else
	UE_LOG(LogSockets, Error, TEXT("Platform has no gethostname(), but did not override FSocketSubsystem::GetHostName()"));
	return false;
#endif
}


const TCHAR* FSocketSubsystemBSD::GetSocketAPIName() const
{
	return TEXT("BSD");
}


TSharedRef<FInternetAddr> FSocketSubsystemBSD::CreateInternetAddr(uint32 Address, uint32 Port)
{
	TSharedRef<FInternetAddr> Result = MakeShareable(new FInternetAddrBSD);
	Result->SetIp(Address);
	Result->SetPort(Port);
	return Result;
}


ESocketErrors FSocketSubsystemBSD::GetLastErrorCode()
{
	return TranslateErrorCode(errno);
}


ESocketErrors FSocketSubsystemBSD::TranslateErrorCode(int32 Code)
{
	// @todo sockets: Windows for some reason doesn't seem to have all of the standard error messages,
	// but it overrides this function anyway - however, this 
#if !PLATFORM_HAS_BSD_SOCKET_FEATURE_WINSOCKETS

	// handle the generic -1 error
	if (Code == SOCKET_ERROR)
	{
		return GetLastErrorCode();
	}

	switch (Code)
	{
	case 0: return SE_NO_ERROR;
	case EINTR: return SE_EINTR;
	case EBADF: return SE_EBADF;
	case EACCES: return SE_EACCES;
	case EFAULT: return SE_EFAULT;
	case EINVAL: return SE_EINVAL;
	case EMFILE: return SE_EMFILE;
	case EWOULDBLOCK: return SE_EWOULDBLOCK;
	case EINPROGRESS: return SE_EINPROGRESS;
	case EALREADY: return SE_EALREADY;
	case ENOTSOCK: return SE_ENOTSOCK;
	case EDESTADDRREQ: return SE_EDESTADDRREQ;
	case EMSGSIZE: return SE_EMSGSIZE;
	case EPROTOTYPE: return SE_EPROTOTYPE;
	case ENOPROTOOPT: return SE_ENOPROTOOPT;
	case EPROTONOSUPPORT: return SE_EPROTONOSUPPORT;
	case ESOCKTNOSUPPORT: return SE_ESOCKTNOSUPPORT;
	case EOPNOTSUPP: return SE_EOPNOTSUPP;
	case EPFNOSUPPORT: return SE_EPFNOSUPPORT;
	case EAFNOSUPPORT: return SE_EAFNOSUPPORT;
	case EADDRINUSE: return SE_EADDRINUSE;
	case EADDRNOTAVAIL: return SE_EADDRNOTAVAIL;
	case ENETDOWN: return SE_ENETDOWN;
	case ENETUNREACH: return SE_ENETUNREACH;
	case ENETRESET: return SE_ENETRESET;
	case ECONNABORTED: return SE_ECONNABORTED;
	case ECONNRESET: return SE_ECONNRESET;
	case ENOBUFS: return SE_ENOBUFS;
	case EISCONN: return SE_EISCONN;
	case ENOTCONN: return SE_ENOTCONN;
	case ESHUTDOWN: return SE_ESHUTDOWN;
	case ETOOMANYREFS: return SE_ETOOMANYREFS;
	case ETIMEDOUT: return SE_ETIMEDOUT;
	case ECONNREFUSED: return SE_ECONNREFUSED;
	case ELOOP: return SE_ELOOP;
	case ENAMETOOLONG: return SE_ENAMETOOLONG;
	case EHOSTDOWN: return SE_EHOSTDOWN;
	case EHOSTUNREACH: return SE_EHOSTUNREACH;
	case ENOTEMPTY: return SE_ENOTEMPTY;
	case EUSERS: return SE_EUSERS;
	case EDQUOT: return SE_EDQUOT;
	case ESTALE: return SE_ESTALE;
	case EREMOTE: return SE_EREMOTE;
	case ENODEV: return SE_NODEV;
#if !PLATFORM_HAS_NO_EPROCLIM
	case EPROCLIM: return SE_EPROCLIM;
#endif
// 	case EDISCON: return SE_EDISCON;
// 	case SYSNOTREADY: return SE_SYSNOTREADY;
// 	case VERNOTSUPPORTED: return SE_VERNOTSUPPORTED;
// 	case NOTINITIALISED: return SE_NOTINITIALISED;

#if PLATFORM_HAS_BSD_SOCKET_FEATURE_GETHOSTNAME
	case HOST_NOT_FOUND: return SE_HOST_NOT_FOUND;
	case TRY_AGAIN: return SE_TRY_AGAIN;
	case NO_RECOVERY: return SE_NO_RECOVERY;
#endif

//	case NO_DATA: return SE_NO_DATA;
		// case : return SE_UDP_ERR_PORT_UNREACH; //@TODO Find it's replacement
	}
#endif

	UE_LOG(LogSockets, Warning, TEXT("Unhandled socket error! Error Code: %d. Returning SE_EINVAL!"), Code);
	return SE_EINVAL;
}


#endif	//PLATFORM_HAS_BSD_SOCKETS
