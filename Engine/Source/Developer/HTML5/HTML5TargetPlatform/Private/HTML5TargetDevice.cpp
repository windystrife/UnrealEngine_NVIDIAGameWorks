// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "HTML5TargetDevice.h"
#include "Interfaces/ITargetPlatform.h"



/* ITargetDevice interface
 *****************************************************************************/

bool FHTML5TargetDevice::Connect( )
{
	return false;
}


bool FHTML5TargetDevice::Deploy( const FString& SourceFolder, FString& OutAppId )
{
	return false;
}


void FHTML5TargetDevice::Disconnect( )
{
}

FTargetDeviceId FHTML5TargetDevice::GetId( ) const
{
	return FTargetDeviceId(TargetPlatform.PlatformName(), Path);
}


FString FHTML5TargetDevice::GetName( ) const
{
	return Name;
}


FString FHTML5TargetDevice::GetOperatingSystemName( )
{
	return TEXT("HTML5 Browser");
}


int32 FHTML5TargetDevice::GetProcessSnapshot( TArray<FTargetDeviceProcessInfo>& OutProcessInfos ) 
{
	return OutProcessInfos.Num();
}


bool FHTML5TargetDevice::Launch( const FString& AppId, EBuildConfigurations::Type BuildConfiguration, EBuildTargets::Type BuildTarget, const FString& Params, uint32* OutProcessId )
{
	return Run(AppId, Params, OutProcessId);
}


bool FHTML5TargetDevice::PowerOff( bool Force )
{
	return false;
}


bool FHTML5TargetDevice::PowerOn( )
{
	return false;
}



bool FHTML5TargetDevice::Reboot( bool bReconnect )
{
	return false;
}


bool FHTML5TargetDevice::Run( const FString& ExecutablePath, const FString& Params, uint32* OutProcessId )
{
	return false;
}


bool FHTML5TargetDevice::SupportsFeature( ETargetDeviceFeatures Feature ) const
{
	switch (Feature)
	{
	case ETargetDeviceFeatures::MultiLaunch:
		return true;

	default:
		return false;
	}
}


bool FHTML5TargetDevice::SupportsSdkVersion( const FString& VersionString ) const
{
	// @todo filter SDK versions

	return true;
}

void FHTML5TargetDevice::SetUserCredentials( const FString& UserName, const FString& UserPassword )
{
}

bool FHTML5TargetDevice::GetUserCredentials( FString& OutUserName, FString& OutUserPassword )
{
	return false;
}

bool FHTML5TargetDevice::TerminateProcess( const int64 ProcessId )
{
	return false;
}
