// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BSDSockets/SocketSubsystemBSDPrivate.h"
#include "IPAddress.h"

#if PLATFORM_HAS_BSD_SOCKETS


/**
 * Represents an internet ip address, using the relatively standard SOCKADDR_IN structure. All data is in network byte order
 */
class FInternetAddrBSD : public FInternetAddr
{
protected:
	/** The internet ip address structure */
	sockaddr_in Addr;

public:
	/**
	 * Constructor. Sets address to default state
	 */
	FInternetAddrBSD()
	{
		FMemory::Memzero(&Addr,sizeof(sockaddr_in));
		Addr.sin_family = AF_INET;
	}
	/**
	 * Sets the ip address from a host byte order uint32
	 *
	 * @param InAddr the new address to use (must convert to network byte order)
	 */
	virtual void SetIp(uint32 InAddr) override
	{
		Addr.sin_addr.s_addr = htonl(InAddr);
	}

	/**
	 * Sets the ip address from a string ("A.B.C.D")
	 *
	 * @param InAddr the string containing the new ip address to use
	 */
	virtual void SetIp(const TCHAR* InAddr, bool& bIsValid) override
	{
		int32 Port = 0;

		FString AddressString = InAddr;
		if (AddressString.IsEmpty())
		{
			bIsValid = false;
			return;
		}

		TArray<FString> PortTokens;
		AddressString.ParseIntoArray(PortTokens, TEXT(":"), true);
		
		// look for a port number
		if (PortTokens.Num() > 1)
		{
			Port = FCString::Atoi(*PortTokens[1]);
		}

		// Check if it's a valid IPv4 address, and if it is convert
		in_addr IPv4Addr;
		const auto InAddrAnsi = StringCast<ANSICHAR>(*(PortTokens[0]));
		if (inet_pton(AF_INET, InAddrAnsi.Get(), &IPv4Addr))
		{
			if (Port != 0)
			{
				SetPort(Port);
			}

			bIsValid = true;

			SetIp(IPv4Addr);
		}
		else
		{
			//debugf(TEXT("Invalid IP address string (%s) passed to SetIp"),InAddr);
			bIsValid = false;
		}
	}

	/**
	 * Sets the ip address using a network byte order ip address
	 *
	 * @param IpAddr the new ip address to use
	 */
	void SetIp(const in_addr& IpAddr)
 	{
 		Addr.sin_addr = IpAddr;
 	}


	/**
	 * Copies the network byte order ip address to a host byte order dword
	 *
	 * @param OutAddr the out param receiving the ip address
	 */
	virtual void GetIp(uint32& OutAddr) const override
	{ 
		OutAddr = ntohl(Addr.sin_addr.s_addr);
	}

	/**
	 * Copies the network byte order ip address 
	 *
	 * @param OutAddr the out param receiving the ip address
	 */
	void GetIp(in_addr& OutAddr) const
 	{
 		OutAddr = Addr.sin_addr;
 	}

	/**
	 * Sets the port number from a host byte order int
	 *
	 * @param InPort the new port to use (must convert to network byte order)
	 */
	virtual void SetPort(int32 InPort) override
	{
		Addr.sin_port = htons(InPort);
	}

	/**
	 * Copies the port number from this address and places it into a host byte order int
	 *
	 * @param OutPort the host byte order int that receives the port
	 */
	virtual void GetPort(int32& OutPort) const override
	{
		OutPort = ntohs(Addr.sin_port);
	}

	/**
	 * Returns the port number from this address in host byte order
	 */
	virtual int32 GetPort(void) const override
	{
		return ntohs(Addr.sin_port);
	}

	/** Sets the address to be any address */
	virtual void SetAnyAddress(void) override
	{
		SetIp(INADDR_ANY);
		SetPort(0);
	}

	/** Sets the address to broadcast */
	virtual void SetBroadcastAddress() override
	{
		SetIp(INADDR_BROADCAST);
		SetPort(0);
	}

	/**
	 * Converts this internet ip address to string form
	 *
	 * @param bAppendPort whether to append the port information or not
	 */
	virtual FString ToString(bool bAppendPort) const override
	{
		uint32 LocalAddr = ntohl(Addr.sin_addr.s_addr);

		// Get the individual bytes
		const int32 A = (LocalAddr >> 24) & 0xFF;
		const int32 B = (LocalAddr >> 16) & 0xFF;
		const int32 C = (LocalAddr >>  8) & 0xFF;
		const int32 D = (LocalAddr >>  0) & 0xFF;
		if (bAppendPort)
		{
			return FString::Printf(TEXT("%i.%i.%i.%i:%i"),A,B,C,D,GetPort());
		}
		else
		{
			return FString::Printf(TEXT("%i.%i.%i.%i"),A,B,C,D);
		}
	}

	/**
	 * Compares two internet ip addresses for equality
	 *
	 * @param Other the address to compare against
	 */
	virtual bool operator==(const FInternetAddr& Other) const override
	{
		FInternetAddrBSD& OtherBSD = (FInternetAddrBSD&)Other;
		return Addr.sin_addr.s_addr == OtherBSD.Addr.sin_addr.s_addr &&
			Addr.sin_port == OtherBSD.Addr.sin_port &&
			Addr.sin_family == OtherBSD.Addr.sin_family;
	}

	/**
	 * Is this a well formed internet address
	 *
	 * @return true if a valid IP, false otherwise
	 */
	virtual bool IsValid() const override
	{
		return Addr.sin_addr.s_addr != 0;
	}

 	operator sockaddr*(void)
 	{
 		return (sockaddr*)&Addr;
 	}

 	operator const sockaddr*(void) const
 	{
 		return (const sockaddr*)&Addr;
 	}
};

#endif	//PLATFORM_HAS_BSD_SOCKETS
