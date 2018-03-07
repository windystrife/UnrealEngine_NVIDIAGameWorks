// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "LaunchCommand.h"
#include "IPluginManager.h"
#include "ITargetPlatform.h"
#include "ITargetPlatformManagerModule.h"
#include "TaskGraphInterfaces.h"
#include "Misc/CommandLine.h"
#include "Containers/Ticker.h"
#include "HAL/PlatformProcess.h"
#include "Modules/ModuleManager.h"
#include "Misc/CoreMisc.h"

bool FLaunchCommand::Run( const FString& Params )
{
	bool bLaunched = false;

	// get the target device
	FString DevicesList;
	FParse::Value(FCommandLine::Get(), TEXT("-DEVICE="), DevicesList);

	// get the executable to launch
	FString Executable;
	FParse::Value(FCommandLine::Get(), TEXT("-EXE="), Executable);

	ITargetPlatformManagerModule* TPM = GetTargetPlatformManager();

	if (!TPM)
	{
		return false;
	}

	// Initialize the messaging subsystem so we can do device discovery.
	FModuleManager::Get().LoadModuleChecked("Messaging");

	// load plug-in modules
	// @todo: allow for better plug-in support in standalone Slate apps
	IPluginManager::Get().LoadModulesForEnabledPlugins(ELoadingPhase::PreDefault);

	while (!DevicesList.IsEmpty())
	{
		FString Device;
		if (!DevicesList.Split(TEXT("+"), &Device, &DevicesList))
		{
			Device = DevicesList;
			DevicesList.Empty();
		}

		double DeltaTime = 0.0;
		double LastTime = FPlatformTime::Seconds();
		static int32 MasterDisableChangeTagStartFrame = -1;

		// We track the message sent time because we have to keep updating the loop until
		// the message is *actually sent*. (ie all packets queued, sent, buffer flushed, etc.)
		double MessageSentTime = 0.0;
		bool bMessageSent = false;

		while (!GIsRequestingExit && ((MessageSentTime > LastTime + 1.0) || (MessageSentTime <= 0.1)))
		{
			FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
			FTicker::GetCoreTicker().Tick(DeltaTime);
			FPlatformProcess::Sleep(0);

			DeltaTime = FPlatformTime::Seconds() - LastTime;
			LastTime = FPlatformTime::Seconds();

			if (!bMessageSent)
			{
				const TArray<ITargetPlatform*>& Platforms = TPM->GetActiveTargetPlatforms();

				FString Platform;
				FString DeviceName;
				Device.Split(TEXT("@"), &Platform, &DeviceName);
				FTargetDeviceId DeviceId(Platform, DeviceName);
				ITargetDevicePtr TargetDevice;

				for (int32 Index = 0; Index < Platforms.Num(); ++Index)
				{
					TargetDevice = Platforms[Index]->GetDevice(DeviceId);

					if (TargetDevice.IsValid())
					{
						uint32 OutId;

						if (TargetDevice->Run(Executable, Params, &OutId))
						{
							MessageSentTime = LastTime;
							bMessageSent = true;
							bLaunched = true;
						}
						else
						{
							MessageSentTime = LastTime;
							bMessageSent = true;
						}
					}
				}
			}
		}
	}

	return bLaunched;
}
