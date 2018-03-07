// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SocketSubsystemHTML5Simulator.h"
#include "ModuleManager.h"
#include "SocketX.h"
#include "IPAddressX.h"
#include "Sockets/SocketRaw.h"
#include "Sockets/IPAddressRaw.h"

FSocketSubsystemHTML5* FSocketSubsystemHTML5::SocketSingleton = NULL;

FName CreateSocketSubsystem( FSocketSubsystemModule& SocketSubsystemModule )
{
 	FName SubsystemName(TEXT("HTML5"));
	FSocketSubsystemHTML5* SocketSubsystem = FSocketSubsystemHTML5::Create();
	FString Error;
	if (SocketSubsystem->Init(Error))
	{
		SocketSubsystemModule.RegisterSocketSubsystem(SubsystemName, SocketSubsystem);
		return SubsystemName;
	}
	else
	{
		FSocketSubsystemHTML5::Destroy();
		return NAME_None;
	}

}

void DestroySocketSubsystem( FSocketSubsystemModule& SocketSubsystemModule )
{
	SocketSubsystemModule.UnregisterSocketSubsystem(FName(TEXT("HTML5")));
	FSocketSubsystemHTML5::Destroy();
}

/** 
 * Singleton interface for the Android socket subsystem 
 * @return the only instance of the Android socket subsystem
 */
FSocketSubsystemHTML5* FSocketSubsystemHTML5::Create()
{
	if (SocketSingleton == NULL)
	{
		SocketSingleton = new FSocketSubsystemHTML5();
	}

	return SocketSingleton; 
	check(0);
}

/** 
 * Destroy the singleton Android socket subsystem
 */
void FSocketSubsystemHTML5::Destroy()
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
bool FSocketSubsystemHTML5::Init(FString& Error)
{
	FSocketRaw::Init(); 
	return true;
}

/**
 * Performs HTML5 specific socket clean up
 */
void FSocketSubsystemHTML5::Shutdown(void)
{
}


/**
 * @return Whether the device has a properly configured network device or not
 */
bool FSocketSubsystemHTML5::HasNetworkDevice()
{
	return true;
}

class FSocket* FSocketSubsystemHTML5::CreateSocket( const FName& SocketType, const FString& SocketDescription, bool bForceUDP /*= false */ )
{
	FSocketX* NewSocket = NULL;
	switch (SocketType.GetComparisonIndex())
	{
	case NAME_DGram:
		// Creates a data gram (UDP) socket 
		NewSocket = new FSocketX( new FSocketRaw (false), SOCKTYPE_Datagram, SocketDescription, this); 
		break;
	case NAME_Stream:
		// Creates a stream (TCP) socket
		NewSocket = new FSocketX( new FSocketRaw (true), SOCKTYPE_Datagram, SocketDescription, this); 
		break;
	}

	return NewSocket->IsValid() ? NewSocket : NULL;
}

void FSocketSubsystemHTML5::DestroySocket( class FSocket* Socket )
{
	delete Socket; 
}

ESocketErrors FSocketSubsystemHTML5::GetHostByName( const ANSICHAR* HostName, FInternetAddr& OutAddr )
{
	FInternetAddrX* addr = (FInternetAddrX*)&OutAddr; 
	return FSocketRaw::GetHostByName( (HostName), *(addr->GetPimpl()) ) ? ESocketErrors::SE_NO_ERROR : ESocketErrors::SE_HOST_NOT_FOUND; 
}

bool FSocketSubsystemHTML5::RequiresChatDataBeSeparate()
{
	return false; 
}

bool FSocketSubsystemHTML5::RequiresEncryptedPackets()
{
	return false; 
}

bool FSocketSubsystemHTML5::GetHostName( FString& HostName )
{
	 return FSocketRaw::GetHostName(TCHAR_TO_ANSI(*HostName)); 
}

TSharedRef<FInternetAddr> FSocketSubsystemHTML5::CreateInternetAddr( uint32 Address/*=0*/, uint32 Port/*=0 */ )
{
	TSharedRef<FInternetAddr> Result = MakeShareable(new FInternetAddrX);
	Result->SetIp(Address);
	Result->SetPort(Port);
	return Result;
}

const TCHAR* FSocketSubsystemHTML5::GetSocketAPIName() const
{
	return TEXT("HTML5");
}

ESocketErrors FSocketSubsystemHTML5::GetLastErrorCode()
{
	return TranslateErrorCode(0); 
}

ESocketErrors FSocketSubsystemHTML5::TranslateErrorCode( int32 Code )
{
	// expect no errors. 
	check( Code == 0 ); 
	return SE_NO_ERROR; 
}

bool FSocketSubsystemHTML5::GetLocalAdapterAddresses( TArray<TSharedPtr<FInternetAddr> >& OutAdresses )
{
	bool bCanBindAll;
	OutAdresses.Add(GetLocalHostAddr(*GLog, bCanBindAll));
	return true;
} 