// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SocketSubsystem.h"

#if PLATFORM_HAS_BSD_SOCKET_FEATURE_WINSOCKETS
	#include "WindowsHWrapper.h"
	#include "Windows/AllowWindowsPlatformTypes.h"

	#include <winsock2.h>
	#include <ws2tcpip.h>

	typedef int32 SOCKLEN;

	#include "Windows/HideWindowsPlatformTypes.h"
#else
#if PLATFORM_SWITCH
	#include "Switch/SwitchSocketApiWrapper.h"
#else
	#include <unistd.h>
	#include <sys/socket.h>
#if PLATFORM_HAS_BSD_SOCKET_FEATURE_IOCTL
	#include <sys/ioctl.h>
#endif
	#include <netinet/in.h>
	#include <arpa/inet.h>
#if PLATFORM_HAS_BSD_SOCKET_FEATURE_GETHOSTNAME
	#include <netdb.h>
#endif

	#define ioctlsocket ioctl
#endif

	#define SOCKET_ERROR -1
	#define INVALID_SOCKET -1

	typedef socklen_t SOCKLEN;
	typedef int32 SOCKET;
	typedef sockaddr_in SOCKADDR_IN;
	typedef struct timeval TIMEVAL;

	inline int32 closesocket(SOCKET Socket)
	{
		shutdown(Socket, SHUT_RDWR); // gracefully shutdown if connected
		return close(Socket);
	}

#endif

// Since the flag constants may have different values per-platform, translate into corresponding system constants.
// For example, MSG_WAITALL is 0x8 on Windows, but 0x100 on other platforms.
inline int TranslateFlags(ESocketReceiveFlags::Type Flags)
{
	int TranslatedFlags = 0;

	if (Flags & ESocketReceiveFlags::Peek)
	{
		TranslatedFlags |= MSG_PEEK;
#if PLATFORM_HAS_BSD_SOCKET_FEATURE_MSG_DONTWAIT
		TranslatedFlags |= MSG_DONTWAIT;
#endif // PLATFORM_HAS_BSD_SOCKET_FEATURE_MSG_DONTWAIT
	}

	if (Flags & ESocketReceiveFlags::WaitAll)
	{
		TranslatedFlags |= MSG_WAITALL;
	}

	return TranslatedFlags;
}


/**
 * Standard BSD specific socket subsystem implementation (common to both IPv4 and IPv6)
 */
class FSocketSubsystemBSDCommon
	: public ISocketSubsystem
{
protected:

	/**
	 * Translates return values of getaddrinfo() to socket error enum
	 */
	inline ESocketErrors TranslateGAIErrorCode(int32 Code) const
	{
#if PLATFORM_HAS_BSD_SOCKET_FEATURE_GETADDRINFO
		switch (Code)
		{
			// getaddrinfo() has its own error codes
			case EAI_AGAIN:			return SE_TRY_AGAIN;
			case EAI_BADFLAGS:		return SE_EINVAL;
			case EAI_FAIL:			return SE_NO_RECOVERY;
			case EAI_FAMILY:		return SE_EAFNOSUPPORT;
			case EAI_MEMORY:		return SE_ENOBUFS;
			case EAI_NONAME:		return SE_HOST_NOT_FOUND;
			case EAI_SERVICE:		return SE_EPFNOSUPPORT;
			case EAI_SOCKTYPE:		return SE_ESOCKTNOSUPPORT;
#if PLATFORM_HAS_BSD_SOCKET_FEATURE_WINSOCKETS
			case WSANO_DATA:		return SE_NO_DATA;
			case WSANOTINITIALISED: return SE_NOTINITIALISED;
#else			
			case EAI_NODATA:		return SE_NO_DATA;
			case EAI_ADDRFAMILY:	return SE_ADDRFAMILY;
			case EAI_SYSTEM:		return SE_SYSTEM;
#endif
			case 0:					break; // 0 means success
			default:
				UE_LOG(LogSockets, Warning, TEXT("Unhandled getaddrinfo() socket error! Code: %d"), Code);
				return SE_EINVAL;
		}
#endif // PLATFORM_HAS_BSD_SOCKET_FEATURE_GETADDRINFO

		return SE_NO_ERROR;
	};
};
