// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved

#include "IOSTargetPlatform.h"
#include "HAL/PlatformProcess.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"

struct FDeviceNotificationCallbackInformation
{
	FString UDID;
	FString	DeviceName;
	uint32 msgType;
};

class FDeviceSyslogRelay : public FRunnable
{
public:

	FDeviceSyslogRelay(FString const& inDeviceID)
	{
		deviceID = inDeviceID;
		Stopping = false;
	}

	virtual bool Init() override
	{
		return true;
	}

	virtual uint32 Run() override
	{
		// Tell DeploymentServer.exe to start collecting logs
		FString ExecutablePath = FString::Printf(TEXT("%sBinaries/DotNET/IOS"), *FPaths::EngineDir());
		FString Filename = TEXT("DeploymentServer.exe");

		// Construct command line
		FString CommandLine = FString::Printf(TEXT("listentodevice -device %s"), *deviceID);

		// execute the command
		void* WritePipe;
		void* ReadPipe;
		FPlatformProcess::CreatePipe(ReadPipe, WritePipe);
		FProcHandle ProcessHandle = FPlatformProcess::CreateProc(*(ExecutablePath / Filename), *CommandLine, false, true, true, NULL, 0, *ExecutablePath, WritePipe);
		FString	lastPartialLine;
		while (FPlatformProcess::IsProcRunning(ProcessHandle) && !Stopping)
		{
			FString CurData = lastPartialLine + FPlatformProcess::ReadPipe(ReadPipe);
			CurData.TrimStartInline();
			if (CurData.Len() > 0)
			{
				// separate out each line
				TArray<FString> LogLines;
				CurData = CurData.Replace(TEXT("\r"), TEXT("\n"));

				bool	endsWithNewline = CurData.EndsWith(TEXT("\n"));

				CurData.ParseIntoArray(LogLines, TEXT("\n"), true);

				for (int32 StringIndex = 0; StringIndex < LogLines.Num() - 1; ++StringIndex)
				{
					UE_LOG(LogIOS, Log, TEXT("%s"), *LogLines[StringIndex]);
				}

				if(endsWithNewline)
				{
					UE_LOG(LogIOS, Log, TEXT("%s"), *LogLines[LogLines.Num() - 1]);
					lastPartialLine = "";
				}
				else
				{
					lastPartialLine = LogLines[LogLines.Num() - 1];
				}
			}
			else
			{
				FPlatformProcess::Sleep(0.1);
			}
		}

		FPlatformProcess::Sleep(0.25);
		FString lastData = FPlatformProcess::ReadPipe(ReadPipe);
		if (lastData.Len() > 0)
		{
			UE_LOG(LogIOS, Log, TEXT("%s"), *lastData);
		}

		FPlatformProcess::ClosePipe(ReadPipe, WritePipe);
		FPlatformProcess::TerminateProc(ProcessHandle);

		return 0;
	}

	virtual void Stop() override
	{
		Stopping = true;
	}

	virtual void Exit() override
	{}


private:
	bool Stopping;
	FString	deviceID;
};

class FIOSDevice
{
public:
    FIOSDevice(FString InID, FString InName)
		: UDID(InID)
		, Name(InName)
    {
		// BHP - Disabling the ios syslog relay because it depends on DeploymentServer running which makes it unwritable 
		//	which causes problems when packaging because this library gets rebuilt but it can't overwrite it which
		//	causes errors and other bad behavior - will probably need to move this functionality into its own dll
		#if 1
			syslogRelay = nullptr;
			syslogRelayThread = nullptr;
		#else
			syslogRelay = new FDeviceSyslogRelay(InID);
			syslogRelayThread = FRunnableThread::Create(syslogRelay, TEXT("FIOSDevice.syslogRelay"), 128 * 1024, TPri_Normal);
		#endif
    }
    
	~FIOSDevice()
	{
		if(syslogRelay != nullptr && syslogRelayThread != nullptr)
		{
			syslogRelay->Stop();
			syslogRelayThread->WaitForCompletion();
			delete syslogRelayThread;
			delete syslogRelay;
			syslogRelay = NULL;
			syslogRelayThread = NULL;
		}
	}

	FString SerialNumber() const
	{
		return UDID;
	}

private:
    FString UDID;
	FString Name;
	FDeviceSyslogRelay*	syslogRelay;
	FRunnableThread*	syslogRelayThread;
};

/**
 * Delegate type for devices being connected or disconnected from the machine
 *
 * The first parameter is newly added or removed device
 */
DECLARE_MULTICAST_DELEGATE_OneParam(FDeviceNotification, void*)

class FDeviceQueryTask
	: public FRunnable
{
public:
	FDeviceQueryTask()
		: Stopping(false)
		, bCheckDevices(true)
	{}

	virtual bool Init() override
	{
		return true;
	}

	virtual uint32 Run() override
	{
		while (!Stopping)
		{
			if (bCheckDevices)
			{
				// BHP - Turning off device check to prevent it from interfering with packaging
				//QueryDevices();
			}

			FPlatformProcess::Sleep(5.0f);
		}

		return 0;
	}

	virtual void Stop() override
	{
		Stopping = true;
	}

	virtual void Exit() override
	{}

	FDeviceNotification& OnDeviceNotification()
	{
		return DeviceNotification;
	}

	void Enable(bool OnOff)
	{
		bCheckDevices = OnOff;
	}

private:
	bool ExecuteDSCommand(const FString& CommandLine, FString* OutStdOut, FString* OutStdErr) const
	{
		FString ExecutablePath = FString::Printf(TEXT("%sBinaries/DotNET/IOS"), *FPaths::EngineDir());
		FString Filename = TEXT("DeploymentServer.exe");

		// execute the command
		int32 ReturnCode;
		FString DefaultError;

		// make sure there's a place for error output to go if the caller specified nullptr
		if (!OutStdErr)
		{
			OutStdErr = &DefaultError;
		}

		void* WritePipe;
		void* ReadPipe;
		FPlatformProcess::CreatePipe(ReadPipe, WritePipe);
		FProcHandle ProcessHandle = FPlatformProcess::CreateProc(*(ExecutablePath / Filename), *CommandLine, false, true, true, NULL, 0, *ExecutablePath, WritePipe);
		while (FPlatformProcess::IsProcRunning(ProcessHandle))
		{
			FString NewLine = FPlatformProcess::ReadPipe(ReadPipe);
			if (NewLine.Len() > 0)
			{
				// process the string to break it up in to lines
				*OutStdOut += NewLine;
			}

			FPlatformProcess::Sleep(0.25);
		}
		FString NewLine = FPlatformProcess::ReadPipe(ReadPipe);
		if (NewLine.Len() > 0)
		{
			// process the string to break it up in to lines
			*OutStdOut += NewLine;
		}

		FPlatformProcess::Sleep(0.25);
		FPlatformProcess::ClosePipe(ReadPipe, WritePipe);

		if (!FPlatformProcess::GetProcReturnCode(ProcessHandle, &ReturnCode))
		{
			return false;
		}

		if (ReturnCode != 0)
		{
			FPlatformMisc::LowLevelOutputDebugStringf(TEXT("The DeploymentServer command '%s' failed to run. Return code: %d, Error: %s\n"), *CommandLine, ReturnCode, **OutStdErr);

			return false;
		}

		return true;
	}

	void QueryDevices()
	{
		FString StdOut;
		// get the list of devices
		if (!ExecuteDSCommand(TEXT("listdevices"), &StdOut, nullptr))
		{
			return;
		}

		// separate out each line
		TArray<FString> DeviceStrings;
		StdOut = StdOut.Replace(TEXT("\r"), TEXT("\n"));
		StdOut.ParseIntoArray(DeviceStrings, TEXT("\n"), true);

		TArray<FString> CurrentDeviceIds;
		for (int32 StringIndex = 0; StringIndex < DeviceStrings.Num(); ++StringIndex)
		{
			const FString& DeviceString = DeviceStrings[StringIndex];

			if(!DeviceString.StartsWith("[DD] FOUND: "))
			{
				continue;
			}

			// grab the device serial number
			int32 idIndex = DeviceString.Find(TEXT("ID: "));
			int32 nameIndex = DeviceString.Find(TEXT("NAME: "));
			if (idIndex < 0 || nameIndex < 0)
			{
				continue;
			}

			FString SerialNumber = DeviceString.Mid(idIndex + 4, nameIndex - 1 - (idIndex + 4));
			CurrentDeviceIds.Add(SerialNumber);

			// move on to next device if this one is already a known device
			if (ConnectedDeviceIds.Find(SerialNumber) != INDEX_NONE)
			{
				ConnectedDeviceIds.Remove(SerialNumber);
				continue;
			}

			// parse device name
			FString DeviceName = DeviceString.Mid(nameIndex + 6, DeviceString.Len() - (nameIndex + 6));

			// create an FIOSDevice
			FDeviceNotificationCallbackInformation CallbackInfo;
			CallbackInfo.DeviceName = DeviceName;
			CallbackInfo.UDID = SerialNumber;
			CallbackInfo.msgType = 1;
			DeviceNotification.Broadcast(&CallbackInfo);
		}

		// remove all devices no longer found
		for (int32 DeviceIndex = 0; DeviceIndex < ConnectedDeviceIds.Num(); ++DeviceIndex)
		{
			FDeviceNotificationCallbackInformation CallbackInfo;
			CallbackInfo.UDID = ConnectedDeviceIds[DeviceIndex];
			CallbackInfo.msgType = 2;
			DeviceNotification.Broadcast(&CallbackInfo);
		}
		ConnectedDeviceIds = CurrentDeviceIds;
	}

	bool Stopping;
	bool bCheckDevices;
	TArray<FString> ConnectedDeviceIds;
	FDeviceNotification DeviceNotification;
};

/* FIOSDeviceHelper structors
 *****************************************************************************/
static TMap<FIOSDevice*, FIOSLaunchDaemonPong> ConnectedDevices;
static FDeviceQueryTask* QueryTask = NULL;
static FRunnableThread* QueryThread = NULL;
static TArray<FDeviceNotificationCallbackInformation> NotificationMessages;
static FTickerDelegate TickDelegate;

bool FIOSDeviceHelper::MessageTickDelegate(float DeltaTime)
{
	for (int Index = 0; Index < NotificationMessages.Num(); ++Index)
	{
		FDeviceNotificationCallbackInformation cbi = NotificationMessages[Index];
		FIOSDeviceHelper::DeviceCallback(&cbi);
	}
	NotificationMessages.Empty();

	return true;
}

void FIOSDeviceHelper::Initialize(bool bIsTVOS)
{
	// Create a dummy device to hand over
	const FString DummyDeviceName(FString::Printf(bIsTVOS ? TEXT("All_tvOS_On_%s") : TEXT("All_iOS_On_%s"), FPlatformProcess::ComputerName()));
	
	FIOSLaunchDaemonPong Event;
	Event.DeviceID = FString::Printf(bIsTVOS ? TEXT("TVOS@%s") : TEXT("IOS@%s"), *DummyDeviceName);
	Event.bCanReboot = false;
	Event.bCanPowerOn = false;
	Event.bCanPowerOff = false;
	Event.DeviceName = DummyDeviceName;
	Event.DeviceType = bIsTVOS ? TEXT("AppleTV") : TEXT("");
	FIOSDeviceHelper::OnDeviceConnected().Broadcast(Event);

	if(!bIsTVOS)
	{
		// add the message pump
		TickDelegate = FTickerDelegate::CreateStatic(MessageTickDelegate);
		FTicker::GetCoreTicker().AddTicker(TickDelegate, 5.0f);

		// kick off a thread to query for connected devices
		QueryTask = new FDeviceQueryTask();
		QueryTask->OnDeviceNotification().AddStatic(FIOSDeviceHelper::DeviceCallback);
		QueryThread = FRunnableThread::Create(QueryTask, TEXT("FIOSDeviceHelper.QueryTask"), 128 * 1024, TPri_Normal);
	}
}

void FIOSDeviceHelper::DeviceCallback(void* CallbackInfo)
{
	struct FDeviceNotificationCallbackInformation* cbi = (FDeviceNotificationCallbackInformation*)CallbackInfo;

	if (!IsInGameThread())
	{
		NotificationMessages.Add(*cbi);
	}
	else
	{
		switch(cbi->msgType)
		{
		case 1:
			FIOSDeviceHelper::DoDeviceConnect(CallbackInfo);
			break;

		case 2:
			FIOSDeviceHelper::DoDeviceDisconnect(CallbackInfo);
			break;
		}
	}
}

void FIOSDeviceHelper::DoDeviceConnect(void* CallbackInfo)
{
	// connect to the device
	struct FDeviceNotificationCallbackInformation* cbi = (FDeviceNotificationCallbackInformation*)CallbackInfo;
	FIOSDevice* Device = new FIOSDevice(cbi->UDID, cbi->DeviceName);

	// fire the event
	FIOSLaunchDaemonPong Event;
	Event.DeviceID = FString::Printf(TEXT("IOS@%s"), *(cbi->UDID));
	Event.DeviceName = cbi->DeviceName;
	Event.bCanReboot = false;
	Event.bCanPowerOn = false;
	Event.bCanPowerOff = false;
	FIOSDeviceHelper::OnDeviceConnected().Broadcast(Event);

	// add to the device list
	ConnectedDevices.Add(Device, Event);
}

void FIOSDeviceHelper::DoDeviceDisconnect(void* CallbackInfo)
{
	struct FDeviceNotificationCallbackInformation* cbi = (FDeviceNotificationCallbackInformation*)CallbackInfo;
	FIOSDevice* device = NULL;
	for (auto DeviceIterator = ConnectedDevices.CreateIterator(); DeviceIterator; ++DeviceIterator)
	{
		if (DeviceIterator.Key()->SerialNumber() == cbi->UDID)
		{
			device = DeviceIterator.Key();
			break;
		}
	}
	if (device != NULL)
	{
		// extract the device id from the connected list
		FIOSLaunchDaemonPong Event = ConnectedDevices.FindAndRemoveChecked(device);

		// fire the event
		FIOSDeviceHelper::OnDeviceDisconnected().Broadcast(Event);

		// delete the device
		delete device;
	}
}

bool FIOSDeviceHelper::InstallIPAOnDevice(const FTargetDeviceId& DeviceId, const FString& IPAPath)
{
    return false;
}

void FIOSDeviceHelper::EnableDeviceCheck(bool OnOff)
{
	QueryTask->Enable(OnOff);
}