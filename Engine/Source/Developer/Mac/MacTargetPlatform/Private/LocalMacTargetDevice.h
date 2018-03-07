// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	LocalMacTargetDevice.h: Declares the TLocalMacTargetDevice class.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "ITargetDevice.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"

/**
 * Template for local Mac target devices.
 */
class FLocalMacTargetDevice
	: public ITargetDevice
{
public:

	/**
	 * Creates and initializes a new device for the specified target platform.
	 *
	 * @param InTargetPlatform - The target platform.
	 */
	FLocalMacTargetDevice( const ITargetPlatform& InTargetPlatform )
		: TargetPlatform(InTargetPlatform)
	{ }


public:

	virtual bool Connect( ) override
	{
		return true;
	}

	virtual bool Deploy( const FString& SourceFolder, FString& OutAppId ) override
	{
		OutAppId = TEXT("");

		FString PlatformName = TEXT("Mac");
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
	}

	virtual void Disconnect( )
	{ }

	virtual ETargetDeviceTypes GetDeviceType( ) const override
	{
		return ETargetDeviceTypes::Desktop;
	}

	virtual FTargetDeviceId GetId( ) const override
	{
		return FTargetDeviceId(TargetPlatform.PlatformName(), GetName());
	}

	virtual FString GetName( ) const override
	{
		return FPlatformProcess::ComputerName();
	}

	virtual FString GetOperatingSystemName( ) override
	{
		return TEXT("macOS");
	}

	virtual int32 GetProcessSnapshot( TArray<FTargetDeviceProcessInfo>& OutProcessInfos ) override
	{
		// @todo Mac: implement process snapshots
		return 0;
	}

	virtual const class ITargetPlatform& GetTargetPlatform( ) const override
	{
		return TargetPlatform;
	}

	virtual bool IsConnected( )
	{
		return true;
	}

	virtual bool IsDefault( ) const override
	{
		return true;
	}

	virtual bool Launch( const FString& AppId, EBuildConfigurations::Type BuildConfiguration, EBuildTargets::Type Target, const FString& Params, uint32* OutProcessId ) override
	{
		// build executable path
		FString PlatformName = TEXT("Mac");
		FString ExecutableName = TEXT("UE4");
		if (BuildConfiguration != EBuildConfigurations::Development)
		{
			ExecutableName += FString::Printf(TEXT("-%s-%s"), *PlatformName, EBuildConfigurations::ToString(BuildConfiguration));
		}

		FString ExecutablePath = FPaths::EngineIntermediateDir() / TEXT("Devices") / PlatformName / TEXT("Engine") / TEXT("Binaries") / PlatformName / (ExecutableName + TEXT(".app/Contents/MacOS/") + ExecutableName);

		// launch the game
		FProcHandle ProcessHandle = FPlatformProcess::CreateProc(*ExecutablePath, *Params, true, false, false, OutProcessId, 0, NULL, NULL);
		if (ProcessHandle.IsValid())
		{
			FPlatformProcess::CloseProc(ProcessHandle);
			return true;
		}
		return false;
	}

	virtual bool PowerOff( bool Force ) override
	{
		return false;
	}

	virtual bool PowerOn( ) override
	{
		return false;
	}

	virtual bool Reboot( bool bReconnect = false ) override
	{
#if PLATFORM_MAC
		NSAppleScript* Script = [[NSAppleScript alloc] initWithSource:@"tell application \"System Events\" to restart"];
		NSDictionary* ErrorDict = [NSDictionary dictionary];
		[Script executeAndReturnError: &ErrorDict];
#endif
		return true;
	}

	virtual bool Run( const FString& ExecutablePath, const FString& Params, uint32* OutProcessId ) override
	{
		FProcHandle ProcessHandle = FPlatformProcess::CreateProc(*ExecutablePath, *Params, true, false, false, OutProcessId, 0, NULL, NULL);
		if (ProcessHandle.IsValid())
		{
			FPlatformProcess::CloseProc(ProcessHandle);
			return true;
		}
		return false;
	}

	virtual bool SupportsFeature( ETargetDeviceFeatures Feature ) const override
	{
		switch (Feature)
		{
		case ETargetDeviceFeatures::MultiLaunch:
			return true;

		// @todo Mac: implement process snapshots
		case ETargetDeviceFeatures::ProcessSnapshot:
			return false;

		case ETargetDeviceFeatures::Reboot:
			return true;
		}

		return false;
	}

	virtual bool SupportsSdkVersion( const FString& VersionString ) const override
	{
		// @todo filter SDK versions
		return true;
	}

	virtual void SetUserCredentials( const FString& UserName, const FString& UserPassword ) override
	{
	}

	virtual bool GetUserCredentials( FString& OutUserName, FString& OutUserPassword ) override
	{
		return false;
	}

	virtual bool TerminateProcess( const int64 ProcessId ) override
	{
		return false;
	}


private:

	// Holds a reference to the device's target platform.
	const ITargetPlatform& TargetPlatform;
};
