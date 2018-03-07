// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "IPAddressX.h"
#include "Sockets/IPAddressRaw.h" 


FInternetAddrX::FInternetAddrX () 
{
	Pimpl =  new FInternetAddrRaw(); 
}

void FInternetAddrX::SetIp( uint32 InAddr )  /** * Sets the ip address from a string ("A.B.C.D") * * @param InAddr the string containing the  ip address to use */ 
{
	Pimpl->SetIp(InAddr);
}
	
void FInternetAddrX::SetIp(const TCHAR* InAddr, bool& bIsValid) 
{
	int32 A, B, C, D;
	int32 Port = 0;

	FString AddressString = InAddr;

	TArray<FString> PortTokens;
	AddressString.ParseIntoArray(PortTokens, TEXT(":"), true);

	// look for a port number
	if (PortTokens.Num() > 1)
	{
		Port = FCString::Atoi(*PortTokens[1]);
	}

	// now split the part before the : into a.b.c.d
	TArray<FString> AddrTokens;
	PortTokens[0].ParseIntoArray(AddrTokens, TEXT("."), true);

	if (AddrTokens.Num() < 4)
	{
		bIsValid = false;
		return;
	}

	A = FCString::Atoi(*AddrTokens[0]);
	B = FCString::Atoi(*AddrTokens[1]);
	C = FCString::Atoi(*AddrTokens[2]);
	D = FCString::Atoi(*AddrTokens[3]);

	// Make sure the address was valid
	if ((A & 0xFF) == A && (B & 0xFF) == B && (C & 0xFF) == C && (D & 0xFF) == D)
	{
		SetIp((A << 24) | (B << 16) | (C << 8) | (D << 0));

		if (Port != 0)
		{
			SetPort(Port);
		}

		bIsValid = true;
	}
	else
	{
		//debugf(TEXT("Invalid IP address string (%s) passed to SetIp"),InAddr);
		bIsValid = false;
	}
}


void FInternetAddrX::GetIp( uint32& OutAddr ) const 
{
	Pimpl->GetIp(OutAddr);
}

void FInternetAddrX::SetPort( int32 InPort )
{
	Pimpl->SetPort( InPort );
}

void FInternetAddrX::GetPort( int32& OutPort ) const 
{
	Pimpl->GetPort( OutPort);
}

int32 FInternetAddrX::GetPort( void ) const 
{
	 int32 OutPort = 0; 
	 Pimpl->GetPort(OutPort); 
	 return OutPort; 
}

void FInternetAddrX::SetAnyAddress( void )
{
	Pimpl->SetAnyAddress(); 
}

void FInternetAddrX::SetBroadcastAddress()
{
	Pimpl->SetBroadcastAddress(); 
}

FString FInternetAddrX::ToString( bool bAppendPort ) const 
{
	uint32 Ip = 0; 
	GetIp(Ip);
	// Get the individual bytes
	const int32 A = (Ip >> 24) & 0xFF;
	const int32 B = (Ip >> 16) & 0xFF;
	const int32 C = (Ip >>  8) & 0xFF;
	const int32 D = (Ip >>  0) & 0xFF;
	if (bAppendPort)
	{
		return FString::Printf(TEXT("%i.%i.%i.%i:%i"),A,B,C,D,GetPort());
	}
	else
	{
		return FString::Printf(TEXT("%i.%i.%i.%i"),A,B,C,D);
	}
}


bool FInternetAddrX::IsValid() const 
{
	return Pimpl->IsValid(); 
}
