// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "IOSTargetDevice.h"

#include "HAL/PlatformProcess.h"
#include "IOSMessageProtocol.h"
#include "Interfaces/ITargetPlatform.h"
#include "MessageEndpoint.h"
#include "MessageEndpointBuilder.h"


FIOSTargetDevice::FIOSTargetDevice(const ITargetPlatform& InTargetPlatform)
	: TargetPlatform(InTargetPlatform)
	, DeviceEndpoint()
	, AppId()
	, bCanReboot(false)
	, bCanPowerOn(false)
	, bCanPowerOff(false)
	, DeviceType(ETargetDeviceTypes::Indeterminate)
{
	DeviceId = FTargetDeviceId(TargetPlatform.PlatformName(), FPlatformProcess::ComputerName());
	DeviceName = FPlatformProcess::ComputerName();
	MessageEndpoint = FMessageEndpoint::Builder("FIOSTargetDevice").Build();
}


bool FIOSTargetDevice::Connect()
{
	// @todo zombie - Probably need to write a specific ConnectTo(IpAddr) function for setting up a RemoteEndpoint for talking to the Daemon
	// Returning true since, if this exists, a device exists.

	return true;
}

bool FIOSTargetDevice::Deploy(const FString& SourceFolder, FString& OutAppId)
{
	return false;
}

void FIOSTargetDevice::Disconnect()
{
}

int32 FIOSTargetDevice::GetProcessSnapshot(TArray<FTargetDeviceProcessInfo>& OutProcessInfos)
{
	return 0;
}

ETargetDeviceTypes FIOSTargetDevice::GetDeviceType() const
{
	return DeviceType;
}

FTargetDeviceId FIOSTargetDevice::GetId() const
{
	return DeviceId;
}

FString FIOSTargetDevice::GetName() const
{
	return DeviceName;
}

FString FIOSTargetDevice::GetOperatingSystemName()
{
	return TargetPlatform.PlatformName();
}

const class ITargetPlatform& FIOSTargetDevice::GetTargetPlatform() const
{
	return TargetPlatform;
}

bool FIOSTargetDevice::IsConnected()
{
	return true;
}

bool FIOSTargetDevice::IsDefault() const
{
	return true;
}
	
bool FIOSTargetDevice::Launch(const FString& InAppId, EBuildConfigurations::Type InBuildConfiguration, EBuildTargets::Type BuildTarget, const FString& Params, uint32* OutProcessId)
{
#if !PLATFORM_MAC
	MessageEndpoint->Send(new FIOSLaunchDaemonLaunchApp(InAppId, Params), DeviceEndpoint);
	return true;
#else
	//Set return to false on Mac, since we could not find a way to do remote deploy/launch.
	return false;
#endif // !PLATFORM_MAC
}

bool FIOSTargetDevice::PowerOff(bool Force)
{
	// @todo zombie - Supported by the Daemon?

	return false;
}

bool FIOSTargetDevice::PowerOn()
{
	// @todo zombie - Supported by the Daemon?

	return false;
}

bool FIOSTargetDevice::Reboot(bool bReconnect)
{
	// @todo zombie - Supported by the Daemon?

	return false;
}

bool FIOSTargetDevice::Run(const FString& ExecutablePath, const FString& Params, uint32* OutProcessId)
{
#if !PLATFORM_MAC
	// The executable path usually looks something like this: directory/<gamename>.stub
	// We just need <gamename>, so strip that out.
	int32 LastPeriodPos = ExecutablePath.Find( TEXT("."), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
	int32 LastBackslashPos = ExecutablePath.Find( TEXT("\\"), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
	int32 LastSlashPos = ExecutablePath.Find( TEXT("/"), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
	int32 TrimPos = LastBackslashPos > LastSlashPos ? LastBackslashPos : LastSlashPos;
	if ( TrimPos > LastPeriodPos )
	{
		// Ignore any periods in the path before the last "/" or "\"
		LastPeriodPos = ExecutablePath.Len() - 1;
	}

	if ( TrimPos == INDEX_NONE )
	{
		TrimPos = 0; // take the whole string from the beginning
	}
	else
	{
		// increment to one character beyond the slash
		TrimPos++;
	}

	int32 Count = LastPeriodPos - TrimPos;
	FString NewAppId = ExecutablePath.Mid(TrimPos, Count); // remove the ".stub" and the proceeding directory to get the game name

	SetAppId(NewAppId);
	MessageEndpoint->Send(new FIOSLaunchDaemonLaunchApp(AppId, Params), DeviceEndpoint);
	return true;
#else
	//Set return to false on Mac, since we could not find a way to do remote deploy/launch.
	return false;
#endif // !PLATFORM_MAC
}

bool FIOSTargetDevice::SupportsFeature(ETargetDeviceFeatures Feature) const
{
	switch (Feature)
	{
	case ETargetDeviceFeatures::Reboot:
		return bCanReboot;

	case ETargetDeviceFeatures::PowerOn:
		return bCanPowerOn;

	case ETargetDeviceFeatures::PowerOff:
		return bCanPowerOff;

	case ETargetDeviceFeatures::ProcessSnapshot:
		return false;

	default:
		return false;
	}
}

bool FIOSTargetDevice::SupportsSdkVersion(const FString& VersionString) const
{
	return true;
}

bool FIOSTargetDevice::TerminateProcess(const int64 ProcessId)
{
	return false;
}

void FIOSTargetDevice::SetUserCredentials(const FString& UserName, const FString& UserPassword)
{
}

bool FIOSTargetDevice::GetUserCredentials(FString& OutUserName, FString& OutUserPassword)
{
	return false;
}
