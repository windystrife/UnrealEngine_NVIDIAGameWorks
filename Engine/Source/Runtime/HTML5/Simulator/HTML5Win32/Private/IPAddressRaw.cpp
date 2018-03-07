// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "IPAddressRaw.h"


#define WIN32_LEAN_AND_MEAN
#include <WinSock2.h>

#include <cstring>
#include "RawData.h"

#pragma comment(lib, "Ws2_32.lib")


FInternetAddrRaw::FInternetAddrRaw()
{
	Data = new FInternetAddrRawData(); 
}

void FInternetAddrRaw::SetIp( unsigned int InAddr )
{
	Data->Addr.sin_addr.s_addr = htonl(InAddr); 
}


void FInternetAddrRaw::GetIp( unsigned int& OutAddr )
{
	OutAddr = ntohl(Data->Addr.sin_addr.s_addr); 
}


void FInternetAddrRaw::SetPort( int InPort )
{
	Data->Addr.sin_port = htons((u_short)InPort); 
}


void FInternetAddrRaw::GetPort( int& OutPort )
{
	OutPort = ntohs(Data->Addr.sin_port);
}


bool FInternetAddrRaw::IsValid()
{
	return Data->Addr.sin_addr.s_addr != 0;
}


void FInternetAddrRaw::SetAnyAddress()
{
	SetIp(INADDR_ANY);
	SetPort(0);
}


void FInternetAddrRaw::SetBroadcastAddress()
{
	SetIp(INADDR_BROADCAST);
	SetPort(0);
}

const FInternetAddrRawData* FInternetAddrRaw::GetInternalData() const 
{
	return Data; 
}

FInternetAddrRawData* FInternetAddrRaw::GetInternalData()
{
	return Data; 
}

