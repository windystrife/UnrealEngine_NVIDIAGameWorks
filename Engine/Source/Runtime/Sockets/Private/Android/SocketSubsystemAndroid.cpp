// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SocketSubsystemAndroid.h"
#include "SocketSubsystemModule.h"
#include "IPAddress.h"
#include "Misc/Parse.h"
#include "Misc/CommandLine.h"
#include "ModuleManager.h"

#include <sys/ioctl.h>
#include <net/if.h>

FSocketSubsystemAndroid* FSocketSubsystemAndroid::SocketSingleton = NULL;

FName CreateSocketSubsystem( FSocketSubsystemModule& SocketSubsystemModule )
{
	FName SubsystemName(TEXT("ANDROID"));
	// Create and register our singleton factor with the main online subsystem for easy access
	FSocketSubsystemAndroid* SocketSubsystem = FSocketSubsystemAndroid::Create();
	FString Error;
	if (SocketSubsystem->Init(Error))
	{
		SocketSubsystemModule.RegisterSocketSubsystem(SubsystemName, SocketSubsystem);
		return SubsystemName;
	}
	else
	{
		FSocketSubsystemAndroid::Destroy();
		return NAME_None;
	}
}

void DestroySocketSubsystem( FSocketSubsystemModule& SocketSubsystemModule )
{
	SocketSubsystemModule.UnregisterSocketSubsystem(FName(TEXT("ANDROID")));
	FSocketSubsystemAndroid::Destroy();
}

/** 
 * Singleton interface for the Android socket subsystem 
 * @return the only instance of the Android socket subsystem
 */
FSocketSubsystemAndroid* FSocketSubsystemAndroid::Create()
{
	if (SocketSingleton == NULL)
	{
		SocketSingleton = new FSocketSubsystemAndroid();
	}

	return SocketSingleton;
}

/** 
 * Destroy the singleton Android socket subsystem
 */
void FSocketSubsystemAndroid::Destroy()
{
	if (SocketSingleton != NULL)
	{
		SocketSingleton->Shutdown();
		delete SocketSingleton;
		SocketSingleton = NULL;
	}
}

/**
 * Does Android platform initialization of the sockets library
 *
 * @param Error a string that is filled with error information
 *
 * @return TRUE if initialized ok, FALSE otherwise
 */
bool FSocketSubsystemAndroid::Init(FString& Error)
{
	return true;
}

/**
 * Performs Android specific socket clean up
 */
void FSocketSubsystemAndroid::Shutdown(void)
{
}


/**
 * @return Whether the device has a properly configured network device or not
 */
bool FSocketSubsystemAndroid::HasNetworkDevice()
{
	return true;
}


/**
* @return Label explicitly as Android as behavior is slightly different for BSD @refer GetLocalHostAddr
*/
const TCHAR* FSocketSubsystemAndroid::GetSocketAPIName() const
{
	return TEXT("BSD_Android");
}


TSharedRef<FInternetAddr> FSocketSubsystemAndroid::GetLocalHostAddr(FOutputDevice& Out, bool& bCanBindAll)
{
	// Get parent address first
	TSharedRef<FInternetAddr> Addr = FSocketSubsystemBSD::GetLocalHostAddr(Out, bCanBindAll);

	// If the address is not a loopback one (or none), return it.
	// NOTE:
	// Depreciated function gethostname() returns 'localhost' on (all?) Android devices
	// Which in turn means that FSocketSubsystemBSD::GetLocalHostAddr() resolves to 127.0.0.1
	// Getting info from android.net.wifi.WifiManager is a little messy due to UE4 modular architecture and JNI
	// IPv4 code using ioctl(.., SIOCGIFCONF, ..) actually based on FSocketSubsytemLinux::GetLocalHostAddr works fine for now...
	//
	// Also NOTE: Network can flip out behind applications back when connectivity changes. eg. Move out of wifi range.
	// This seems to recover OK between matches as subsystems are reinited each session Host/Join.

	uint32 ParentIp = 0;
	Addr->GetIp(ParentIp); // will return in host order
	if (ParentIp != 0 && (ParentIp & 0xff000000) != 0x7f000000)
	{
		return Addr;
	}

	// If superclass got the address from command line, honor that override
	TCHAR Home[256] = TEXT("");
	if (FParse::Value(FCommandLine::Get(), TEXT("MULTIHOME="), Home, ARRAY_COUNT(Home)))
	{
		TSharedRef<FInternetAddr> TempAddr = CreateInternetAddr();
		bool bIsValid = false;
		TempAddr->SetIp(Home, bIsValid);
		if (bIsValid)
		{
			UE_LOG(LogSockets, Warning, TEXT("FSocketSubsystemAndroid::GetLocalHostAddr Using MULTIHOME"));
			return Addr;
		}
	}

	// We need to go deeper...
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
			int32 WifiAddress = 0;
			int32 CellularAddress = 0;
			int32 OtherAddress = 0;

			for (int32 IdxReq = 0; IdxReq < ARRAY_COUNT(IfReqs); ++IdxReq)
			{
				// Examine interfaces that are up and not loop back
				int ResultFlags = ioctl(TempSocket, SIOCGIFFLAGS, &IfReqs[IdxReq]);
				if (ResultFlags == 0 &&
					(IfReqs[IdxReq].ifr_flags & IFF_UP) &&
					(IfReqs[IdxReq].ifr_flags & IFF_LOOPBACK) == 0)
				{
					if (strcmp(IfReqs[IdxReq].ifr_name, "wlan0") == 0)
					{
						// 'Usually' wifi, Prefer wifi
						WifiAddress = reinterpret_cast<sockaddr_in *>(&IfReqs[IdxReq].ifr_addr)->sin_addr.s_addr;
						break;
					}
					else if (strcmp(IfReqs[IdxReq].ifr_name, "rmnet0") == 0)
					{
						// 'Usually' cellular
						CellularAddress = reinterpret_cast<sockaddr_in *>(&IfReqs[IdxReq].ifr_addr)->sin_addr.s_addr;
					}
					else if (OtherAddress == 0)
					{
						// First alternate found
						OtherAddress = reinterpret_cast<sockaddr_in *>(&IfReqs[IdxReq].ifr_addr)->sin_addr.s_addr;
					}
				}
			}

			// Prioritize results found
			if (WifiAddress != 0)
			{
				// Prefer Wifi
				Addr->SetIp(ntohl(WifiAddress));
				UE_LOG(LogSockets, Log, TEXT("(%s) Wifi Adapter IP %s"), GetSocketAPIName(), *Addr->ToString(false));
			}
			else if (CellularAddress != 0)
			{
				// Then cellular
				Addr->SetIp(ntohl(CellularAddress));
				UE_LOG(LogSockets, Log, TEXT("(%s) Cellular Adapter IP %s"), GetSocketAPIName(), *Addr->ToString(false));
			}
			else if (OtherAddress != 0)
			{
				// Then whatever else was found
				Addr->SetIp(ntohl(OtherAddress));
				UE_LOG(LogSockets, Log, TEXT("(%s) Adapter IP %s"), GetSocketAPIName(), *Addr->ToString(false));
			}
			else
			{
				// Give up
				Addr->SetIp(0x7f000001);  // 127.0.0.1
				UE_LOG(LogSockets, Warning, TEXT("(%s) NO 'UP' ADAPTER FOUND! using: %s"), GetSocketAPIName(), *Addr->ToString(false));
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
