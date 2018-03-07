// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "ITargetPlatform.h"
#include "Interfaces/ITargetDevice.h"
#include "Interfaces/TargetDeviceId.h"

#if PLATFORM_LINUX
	#include <signal.h>
	#include <pwd.h>
#endif // PLATFORM_LINUX

class FLinuxTargetDevice;
class IFileManager;
struct FProcHandle;


/**
 * Type definition for shared pointers to instances of FLinuxTargetDevice.
 */
typedef TSharedPtr<class FLinuxTargetDevice, ESPMode::ThreadSafe> FLinuxTargetDevicePtr;

/**
 * Type definition for shared references to instances of FLinuxTargetDevice.
 */
typedef TSharedRef<class FLinuxTargetDevice, ESPMode::ThreadSafe> FLinuxTargetDeviceRef;


/**
 * Implements a Linux target device.
 */
class FLinuxTargetDevice
	: public ITargetDevice
{
public:

	/**
	 * Creates and initializes a new device for the specified target platform.
	 *
	 * @param InTargetPlatform - The target platform.
	 */
	FLinuxTargetDevice( const ITargetPlatform& InTargetPlatform, const FString& InDeviceName, TFunction<void()> InSavePlatformDevices)
		: TargetPlatform(InTargetPlatform)
		, DeviceName(InDeviceName)
		, SavePlatformDevices(InSavePlatformDevices)
	{ }


public:

	virtual bool Connect( ) override
	{
		return true;
	}

	virtual bool Deploy(const FString& SourceFolder, FString& OutAppId) override
	{
#if PLATFORM_LINUX	// if running natively, support simplified, local deployment
		OutAppId = TEXT("");

		FString PlatformName = TEXT("Linux");
		FString DeploymentDir = FPaths::EngineIntermediateDir() / TEXT("Devices") / PlatformName;

		// delete previous build
		IFileManager::Get().DeleteDirectory(*DeploymentDir, false, true);

		// copy files into device directory
		TArray<FString> FileNames;

		IFileManager::Get().FindFilesRecursive(FileNames, *SourceFolder, TEXT("*.*"), true, false);

		for (int32 FileIndex = 0; FileIndex < FileNames.Num(); ++FileIndex)
		{
			const FString& SourceFilePath = FileNames[FileIndex];
			FString DestFilePath = DeploymentDir + SourceFilePath.RightChop(SourceFolder.Len());

			IFileManager::Get().Copy(*DestFilePath, *SourceFilePath);
		}

		return true;
#else
		// @todo: support deployment to a remote machine
		STUBBED("FLinuxTargetDevice::Deploy");
		return false;
#endif // PLATFORM_LINUX
	}

	virtual void Disconnect( ) override
	{ }

	virtual ETargetDeviceTypes GetDeviceType( ) const override
	{
		return ETargetDeviceTypes::Desktop;
	}

	virtual FTargetDeviceId GetId() const override
	{
		return FTargetDeviceId(TargetPlatform.PlatformName(), GetName());
	}

	virtual FString GetName( ) const override
	{
		return DeviceName;
	}

	virtual FString GetOperatingSystemName( ) override
	{
		return TEXT("GNU/Linux");
	}

	virtual int32 GetProcessSnapshot( TArray<FTargetDeviceProcessInfo>& OutProcessInfos ) override
	{
		STUBBED("FLinuxTargetDevice::GetProcessSnapshot");
		return 0;
	}

	virtual const class ITargetPlatform& GetTargetPlatform( ) const override
	{
		return TargetPlatform;
	}

	virtual bool IsConnected( ) override
	{
		return true;
	}

	virtual bool IsDefault( ) const override
	{
		return true;
	}

	virtual bool PowerOff( bool Force ) override
	{
		return false;
	}

	virtual bool PowerOn( ) override
	{
		return false;
	}

	virtual bool Launch( const FString& AppId, EBuildConfigurations::Type BuildConfiguration, EBuildTargets::Type BuildTarget, const FString& Params, uint32* OutProcessId ) override
	{
#if PLATFORM_LINUX	// if running natively, support launching in place
		// build executable path
		FString PlatformName = TEXT("Linux");
		FString ExecutablePath = FPaths::EngineIntermediateDir() / TEXT("Devices") / PlatformName / TEXT("Engine") / TEXT("Binaries") / PlatformName;

		if (BuildTarget == EBuildTargets::Game)
		{
			ExecutablePath /= TEXT("UE4Game");
		}
		else if (BuildTarget == EBuildTargets::Server)
		{
			ExecutablePath /= TEXT("UE4Server");
		}
		else if (BuildTarget == EBuildTargets::Editor)
		{
			ExecutablePath /= TEXT("UE4Editor");
		}

		if (BuildConfiguration != EBuildConfigurations::Development)
		{
			ExecutablePath += FString::Printf(TEXT("-%s-%s"), *PlatformName, EBuildConfigurations::ToString(BuildConfiguration));
		}

		// launch the game
		FProcHandle ProcessHandle = FPlatformProcess::CreateProc(*ExecutablePath, *Params, true, false, false, OutProcessId, 0, NULL, NULL);
		if (ProcessHandle.IsValid())
		{
			FPlatformProcess::CloseProc(ProcessHandle);
			return true;
		}
		return false;
#else
		// @todo: support launching on a remote machine
		STUBBED("FLinuxTargetDevice::Launch");
		return false;
#endif // PLATFORM_LINUX
	}

	virtual bool Reboot( bool bReconnect = false ) override
	{
		STUBBED("FLinuxTargetDevice::Reboot");
		return false;
	}

	virtual bool Run( const FString& ExecutablePath, const FString& Params, uint32* OutProcessId ) override
	{
#if PLATFORM_LINUX	// if running natively, support simplified, local deployment
		FProcHandle ProcessHandle = FPlatformProcess::CreateProc(*ExecutablePath, *Params, true, false, false, OutProcessId, 0, NULL, NULL);
		if (ProcessHandle.IsValid())
		{
			FPlatformProcess::CloseProc(ProcessHandle);
			return true;
		}
		return false;
#else
		// @todo: support remote run
		STUBBED("FLinuxTargetDevice::Run");
		return false;
#endif // PLATFORM_LINUX
	}

	virtual bool SupportsFeature( ETargetDeviceFeatures Feature ) const override
	{
		switch (Feature)
		{
		case ETargetDeviceFeatures::MultiLaunch:
			return true;

			// @todo to be implemented
		case ETargetDeviceFeatures::PowerOff:
			return false;

			// @todo to be implemented turning on remote PCs (wake on LAN)
		case ETargetDeviceFeatures::PowerOn:
			return false;

			// @todo to be implemented
		case ETargetDeviceFeatures::ProcessSnapshot:
			return false;

			// @todo to be implemented
		case ETargetDeviceFeatures::Reboot:
			return false;
		}

		return false;
	}

	virtual bool SupportsSdkVersion( const FString& VersionString ) const override
	{
		STUBBED("FLinuxTargetDevice::SupportsSdkVersion");
		return true;
	}

	virtual void SetUserCredentials( const FString& InUserName, const FString& InUserPassword ) override
	{
		UserName = InUserName;
		UserPassword = InUserPassword;

		if (SavePlatformDevices)
		{
			SavePlatformDevices();
		}
	}

	virtual bool GetUserCredentials( FString& OutUserName, FString& OutUserPassword ) override
	{
		OutUserName = UserName;
		OutUserPassword = UserPassword;
		return true;
	}

	virtual bool TerminateProcess( const int64 ProcessId ) override
	{
#if PLATFORM_LINUX // if running natively, just terminate the local process
		// get process path from the ProcessId
		const int32 ReadLinkSize = 1024;
		char ReadLinkCmd[ReadLinkSize] = { 0 };
		FCStringAnsi::Sprintf(ReadLinkCmd, "/proc/%lld/exe", ProcessId);
		char ProcessPath[MAX_PATH + 1] = { 0 };
		int32 Ret = readlink(ReadLinkCmd, ProcessPath, ARRAY_COUNT(ProcessPath) - 1);
		if (Ret != -1)
		{
			struct stat st;
			uid_t euid;
			stat(ProcessPath, &st);
			euid = geteuid(); // get effective uid of current application, as this user is asking to kill a process

							  // now check if we own the process
			if (st.st_uid == euid)
			{
				// terminate it (will this terminate children as well because we set their pgid?)
				kill(ProcessId, SIGTERM);
				sleep(2); // sleep in case process still remains then send a more strict signal
				kill(ProcessId, SIGKILL);
				return true;
			}
		}
#else
		// @todo: support remote termination
		STUBBED("FLinuxTargetDevice::TerminateProcess");
#endif // PLATFORM_LINUX
		return false;
	}

private:

	// Holds a reference to the device's target platform.
	const ITargetPlatform& TargetPlatform;

	/** Device display name */
	FString DeviceName;

	/** User name on the remote machine */
	FString UserName;

	/** User password on the remote machine */
	FString UserPassword;

	/** Target platform function to save device state */
	TFunction<void()> SavePlatformDevices;
};
