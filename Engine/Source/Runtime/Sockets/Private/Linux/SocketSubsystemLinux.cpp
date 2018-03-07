// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Linux/SocketSubsystemLinux.h"
#include "Misc/CommandLine.h"
#include "SocketSubsystemModule.h"
#include "IPAddress.h"

#include <net/if.h>

FSocketSubsystemLinux* FSocketSubsystemLinux::SocketSingleton = NULL;

FName CreateSocketSubsystem( FSocketSubsystemModule& SocketSubsystemModule )
{
	FName SubsystemName(TEXT("LINUX"));
	// Create and register our singleton factor with the main online subsystem for easy access
	FSocketSubsystemLinux* SocketSubsystem = FSocketSubsystemLinux::Create();
	FString Error;
	if (SocketSubsystem->Init(Error))
	{
		SocketSubsystemModule.RegisterSocketSubsystem(SubsystemName, SocketSubsystem);
		return SubsystemName;
	}
	else
	{
		FSocketSubsystemLinux::Destroy();
		return NAME_None;
	}
}

void DestroySocketSubsystem( FSocketSubsystemModule& SocketSubsystemModule )
{
	SocketSubsystemModule.UnregisterSocketSubsystem(FName(TEXT("LINUX")));
	FSocketSubsystemLinux::Destroy();
}

/** 
 * Singleton interface for the Android socket subsystem 
 * @return the only instance of the Android socket subsystem
 */
FSocketSubsystemLinux* FSocketSubsystemLinux::Create()
{
	if (SocketSingleton == NULL)
	{
		SocketSingleton = new FSocketSubsystemLinux();
	}

	return SocketSingleton;
}

/** 
 * Destroy the singleton Android socket subsystem
 */
void FSocketSubsystemLinux::Destroy()
{
	if (SocketSingleton != NULL)
	{
		SocketSingleton->Shutdown();
		delete SocketSingleton;
		SocketSingleton = NULL;
	}
}

/**
 * Does Linux platform initialization of the sockets library
 *
 * @param Error a string that is filled with error information
 *
 * @return TRUE if initialized ok, FALSE otherwise
 */
bool FSocketSubsystemLinux::Init(FString& Error)
{
	return true;
}

/**
 * Performs Android specific socket clean up
 */
void FSocketSubsystemLinux::Shutdown(void)
{
}


/**
 * @return Whether the device has a properly configured network device or not
 */
bool FSocketSubsystemLinux::HasNetworkDevice()
{
	// @TODO: implement
	return true;
}

TSharedRef<FInternetAddr> FSocketSubsystemLinux::GetLocalHostAddr(FOutputDevice& Out, bool& bCanBindAll)
{
	// get parent address first
	TSharedRef<FInternetAddr> Addr = FSocketSubsystemBSD::GetLocalHostAddr(Out, bCanBindAll);

	// If the address is not a loopback one (or none), return it.
	uint32 ParentIp = 0;
	Addr->GetIp(ParentIp); // will return in host order
	if (ParentIp != 0 && (ParentIp & 0xff000000) != 0x7f000000)
	{
		return Addr;
	}

	// If superclass got the address from command line, honor that override
	TCHAR Home[256]=TEXT("");
	if (FParse::Value(FCommandLine::Get(),TEXT("MULTIHOME="),Home,ARRAY_COUNT(Home)))
	{
		TSharedRef<FInternetAddr> TempAddr = CreateInternetAddr();
		bool bIsValid = false;
		TempAddr->SetIp(Home, bIsValid);
		if (bIsValid)
		{
			return Addr;
		}
	}

	// we need to go deeper...  (see http://unixhelp.ed.ac.uk/CGI/man-cgi?netdevice+7)
	int TempSocket = socket(PF_INET, SOCK_STREAM, 0);
	if (TempSocket)
	{
		ifreq IfReqs[8];
		
		ifconf IfConfig;
		FMemory::Memzero(IfConfig);
		IfConfig.ifc_req = IfReqs;
		IfConfig.ifc_len = sizeof(IfReqs);
		
		int Result = ioctl(TempSocket, SIOCGIFCONF, &IfConfig);
		if (Result == 0)
		{
			for (int32 IdxReq = 0; IdxReq < ARRAY_COUNT(IfReqs); ++IdxReq)
			{
				// grab the first non-loobpack one which is up
				int ResultFlags = ioctl(TempSocket, SIOCGIFFLAGS, &IfReqs[IdxReq]);
				if (ResultFlags == 0 && 
					(IfReqs[IdxReq].ifr_flags & IFF_UP) && 
					(IfReqs[IdxReq].ifr_flags & IFF_LOOPBACK) == 0)
				{
					int32 NetworkAddr = reinterpret_cast<sockaddr_in *>(&IfReqs[IdxReq].ifr_addr)->sin_addr.s_addr;
					Addr->SetIp(ntohl(NetworkAddr));
					break;
				}
			}
		}
		else
		{
			int ErrNo = errno;
			UE_LOG(LogSockets, Warning, TEXT("ioctl( ,SIOGCIFCONF, ) failed, errno=%d (%s)"), ErrNo, ANSI_TO_TCHAR(strerror(ErrNo)));
		}
		
		close(TempSocket);
	}

	return Addr;
}
