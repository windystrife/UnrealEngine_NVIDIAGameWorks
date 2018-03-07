// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AndroidDeviceDetectionModule.cpp: Implements the FAndroidDeviceDetectionModule class.
=============================================================================*/

#include "CoreTypes.h"
#include "HAL/UnrealMemory.h"
#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "Containers/StringConv.h"
#include "Containers/Map.h"
#include "GenericPlatform/GenericPlatformStackWalk.h"
#include "HAL/PlatformProcess.h"
#include "Logging/LogMacros.h"
#include "HAL/FileManager.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "HAL/ThreadSafeCounter.h"
#include "Misc/ScopeLock.h"
#include "Modules/ModuleManager.h"
#include "Interfaces/IAndroidDeviceDetection.h"
#include "Interfaces/IAndroidDeviceDetectionModule.h"
#include "ITcpMessagingModule.h"

#define LOCTEXT_NAMESPACE "FAndroidDeviceDetectionModule" 

DEFINE_LOG_CATEGORY_STATIC(AndroidDeviceDetectionLog, Log, All);

class FAndroidDeviceDetectionRunnable : public FRunnable
{
public:
	FAndroidDeviceDetectionRunnable(TMap<FString,FAndroidDeviceInfo>& InDeviceMap, FCriticalSection* InDeviceMapLock, FCriticalSection* InADBPathCheckLock) :
		StopTaskCounter(0),
		DeviceMap(InDeviceMap),
		DeviceMapLock(InDeviceMapLock),
		ADBPathCheckLock(InADBPathCheckLock),
		HasADBPath(false),
		ForceCheck(false)
	{
		TcpMessagingModule = FModuleManager::LoadModulePtr<ITcpMessagingModule>("TcpMessaging");
	}

public:

	// FRunnable interface.
	virtual bool Init(void) 
	{ 
		return true; 
	}

	virtual void Exit(void) 
	{
	}

	virtual void Stop(void)
	{
		StopTaskCounter.Increment();
	}

	virtual uint32 Run(void)
	{
		int LoopCount = 10;

		while (StopTaskCounter.GetValue() == 0)
		{
			// query every 10 seconds
			if (LoopCount++ >= 10 || ForceCheck)
			{
				// Make sure we have an ADB path before checking
				FScopeLock PathLock(ADBPathCheckLock);
				if (HasADBPath)
					QueryConnectedDevices();

				LoopCount = 0;
				ForceCheck = false;
			}

			FPlatformProcess::Sleep(1.0f);
		}

		return 0;
	}

	void UpdateADBPath(FString &InADBPath)
	{
		ADBPath = InADBPath;
		HasADBPath = !ADBPath.IsEmpty();
		// Force a check next time we go around otherwise it can take over 10sec to find devices
		ForceCheck = HasADBPath;	

		// If we have no path then clean the existing devices out
		if (!HasADBPath && DeviceMap.Num() > 0)
		{
			DeviceMap.Reset();
		}
	}

private:

	bool ExecuteAdbCommand( const FString& CommandLine, FString* OutStdOut, FString* OutStdErr ) const
	{
		// execute the command
		int32 ReturnCode;
		FString DefaultError;

		// make sure there's a place for error output to go if the caller specified nullptr
		if (!OutStdErr)
		{
			OutStdErr = &DefaultError;
		}

		FPlatformProcess::ExecProcess(*ADBPath, *CommandLine, &ReturnCode, OutStdOut, OutStdErr);

		if (ReturnCode != 0)
		{
			FPlatformMisc::LowLevelOutputDebugStringf(TEXT("The Android SDK command '%s' failed to run. Return code: %d, Error: %s\n"), *CommandLine, ReturnCode, **OutStdErr);

			return false;
		}

		return true;
	}

	void QueryConnectedDevices()
	{
		// grab the list of devices via adb
		FString StdOut;
		if (!ExecuteAdbCommand(TEXT("devices -l"), &StdOut, nullptr))
		{
			return;
		}

		// separate out each line
		TArray<FString> DeviceStrings;
		StdOut = StdOut.Replace(TEXT("\r"), TEXT("\n"));
		StdOut.ParseIntoArray(DeviceStrings, TEXT("\n"), true);

		// list of any existing port forwardings, filled in when we find a device we need to add.
		TArray<FString> PortForwardings;

		// a list containing all devices found this time, so we can remove anything not in this list
		TArray<FString> CurrentlyConnectedDevices;

		for (int32 StringIndex = 0; StringIndex < DeviceStrings.Num(); ++StringIndex)
		{
			const FString& DeviceString = DeviceStrings[StringIndex];

			// skip over non-device lines
			if (DeviceString.StartsWith("* ") || DeviceString.StartsWith("List "))
			{
				continue;
			}

			// grab the device serial number
			int32 TabIndex;

			// use either tab or space as separator
			if (!DeviceString.FindChar(TCHAR('\t'), TabIndex))
			{
				if (!DeviceString.FindChar(TCHAR(' '), TabIndex))
				{
					continue;
				}
			}

			FAndroidDeviceInfo NewDeviceInfo;

			NewDeviceInfo.SerialNumber = DeviceString.Left(TabIndex);
			const FString DeviceState = DeviceString.Mid(TabIndex + 1).TrimStart();

			NewDeviceInfo.bAuthorizedDevice = DeviceState != TEXT("unauthorized");

			// add it to our list of currently connected devices
			CurrentlyConnectedDevices.Add(NewDeviceInfo.SerialNumber);

			// move on to next device if this one is already a known device that has either already been authorized or the authorization
			// status has not changed
			if (DeviceMap.Contains(NewDeviceInfo.SerialNumber) && 
				(DeviceMap[NewDeviceInfo.SerialNumber].bAuthorizedDevice || !NewDeviceInfo.bAuthorizedDevice))
			{					
				continue;
			}

			if (!NewDeviceInfo.bAuthorizedDevice)
			{
				//note: AndroidTargetDevice::GetName() does not fetch this value, do not rely on this
				NewDeviceInfo.DeviceName = TEXT("Unauthorized - enable USB debugging");
			}
			else
			{
				// grab the Android version
				const FString AndroidVersionCommand = FString::Printf(TEXT("-s %s shell getprop ro.build.version.release"), *NewDeviceInfo.SerialNumber);
				if (!ExecuteAdbCommand(*AndroidVersionCommand, &NewDeviceInfo.HumanAndroidVersion, nullptr))
				{
					continue;
				}
				NewDeviceInfo.HumanAndroidVersion = NewDeviceInfo.HumanAndroidVersion.Replace(TEXT("\r"), TEXT("")).Replace(TEXT("\n"), TEXT(""));
				NewDeviceInfo.HumanAndroidVersion.TrimStartAndEndInline();

				// grab the Android SDK version
				const FString SDKVersionCommand = FString::Printf(TEXT("-s %s shell getprop ro.build.version.sdk"), *NewDeviceInfo.SerialNumber);
				FString SDKVersionString;
				if (!ExecuteAdbCommand(*SDKVersionCommand, &SDKVersionString, nullptr))
				{
					continue;
				}
				NewDeviceInfo.SDKVersion = FCString::Atoi(*SDKVersionString);
				if (NewDeviceInfo.SDKVersion <= 0)
				{
					NewDeviceInfo.SDKVersion = INDEX_NONE;
				}

				// get the GL extensions string (and a bunch of other stuff)
				const FString ExtensionsCommand = FString::Printf(TEXT("-s %s shell dumpsys SurfaceFlinger"), *NewDeviceInfo.SerialNumber);
				if (!ExecuteAdbCommand(*ExtensionsCommand, &NewDeviceInfo.GLESExtensions, nullptr))
				{
					continue;
				}

				// grab the GL ES version
				FString GLESVersionString;
				const FString GLVersionCommand = FString::Printf(TEXT("-s %s shell getprop ro.opengles.version"), *NewDeviceInfo.SerialNumber);
				if (!ExecuteAdbCommand(*GLVersionCommand, &GLESVersionString, nullptr))
				{
					continue;
				}
				NewDeviceInfo.GLESVersion = FCString::Atoi(*GLESVersionString);

				// parse the device model
				FParse::Value(*DeviceString, TEXT("model:"), NewDeviceInfo.Model);
				if (NewDeviceInfo.Model.IsEmpty())
				{
					FString ModelCommand = FString::Printf(TEXT("-s %s shell getprop ro.product.model"), *NewDeviceInfo.SerialNumber);
					FString RoProductModel;
					ExecuteAdbCommand(*ModelCommand, &RoProductModel, nullptr);
					const TCHAR* Ptr = *RoProductModel;
					FParse::Line(&Ptr, NewDeviceInfo.Model);
				}

				// parse the device name
				FParse::Value(*DeviceString, TEXT("device:"), NewDeviceInfo.DeviceName);
				if (NewDeviceInfo.DeviceName.IsEmpty())
				{
					FString DeviceCommand = FString::Printf(TEXT("-s %s shell getprop ro.product.device"), *NewDeviceInfo.SerialNumber);
					FString RoProductDevice;
					ExecuteAdbCommand(*DeviceCommand, &RoProductDevice, nullptr);
					const TCHAR* Ptr = *RoProductDevice;
					FParse::Line(&Ptr, NewDeviceInfo.DeviceName);
				}

				// establish port forwarding if we're doing messaging
				if (TcpMessagingModule != nullptr)
				{
					// fill in the port forwarding array if needed
					if (PortForwardings.Num() == 0)
					{
						FString ForwardList;
						if (ExecuteAdbCommand(TEXT("forward --list"), &ForwardList, nullptr))
						{
							ForwardList = ForwardList.Replace(TEXT("\r"), TEXT("\n"));
							ForwardList.ParseIntoArray(PortForwardings, TEXT("\n"), true);
						}
					}

					// check if this device already has port forwarding enabled for message bus, eg from another editor session
					for (FString& FwdString : PortForwardings)
					{
						const TCHAR* Ptr = *FwdString;
						FString FwdSerialNumber, FwdHostPortString, FwdDevicePortString;
						uint16 FwdHostPort, FwdDevicePort;
						if (FParse::Token(Ptr, FwdSerialNumber, false) && FwdSerialNumber == NewDeviceInfo.SerialNumber &&
							FParse::Token(Ptr, FwdHostPortString, false) && FParse::Value(*FwdHostPortString, TEXT("tcp:"), FwdHostPort) &&
							FParse::Token(Ptr, FwdDevicePortString, false) && FParse::Value(*FwdDevicePortString, TEXT("tcp:"), FwdDevicePort) && FwdDevicePort == 6666)
						{
							NewDeviceInfo.HostMessageBusPort = FwdHostPort;
							break;
						}
					}

					// if not, setup TCP port forwarding for message bus on first available TCP port above 6666
					if (NewDeviceInfo.HostMessageBusPort == 0)
					{
						uint16 HostMessageBusPort = 6666;
						bool bFoundPort;
						do
						{
							bFoundPort = true;
							for (auto It = DeviceMap.CreateConstIterator(); It; ++It)
							{
								if (HostMessageBusPort == It.Value().HostMessageBusPort)
								{
									bFoundPort = false;
									HostMessageBusPort++;
									break;
								}
							}
						} while (!bFoundPort);

						FString DeviceCommand = FString::Printf(TEXT("-s %s forward tcp:%d tcp:6666"), *NewDeviceInfo.SerialNumber, HostMessageBusPort);
						ExecuteAdbCommand(*DeviceCommand, nullptr, nullptr);
						NewDeviceInfo.HostMessageBusPort = HostMessageBusPort;
					}

					TcpMessagingModule->AddOutgoingConnection(FString::Printf(TEXT("127.0.0.1:%d"), NewDeviceInfo.HostMessageBusPort));
				}
			}

			// add the device to the map
			{
				FScopeLock ScopeLock(DeviceMapLock);

				FAndroidDeviceInfo& SavedDeviceInfo = DeviceMap.Add(NewDeviceInfo.SerialNumber);
				SavedDeviceInfo = NewDeviceInfo;
			}
		}

		// loop through the previously connected devices list and remove any that aren't still connected from the updated DeviceMap
		TArray<FString> DevicesToRemove;

		for (auto It = DeviceMap.CreateConstIterator(); It; ++It)
		{
			if (!CurrentlyConnectedDevices.Contains(It.Key()))
			{
				if (TcpMessagingModule && It.Value().HostMessageBusPort != 0)
				{
					TcpMessagingModule->RemoveOutgoingConnection(FString::Printf(TEXT("127.0.0.1:%d"), It.Value().HostMessageBusPort));
				}

				DevicesToRemove.Add(It.Key());
			}
		}

		{
			// enter the critical section and remove the devices from the map
			FScopeLock ScopeLock(DeviceMapLock);

			for (auto It = DevicesToRemove.CreateConstIterator(); It; ++It)
			{
				DeviceMap.Remove(*It);
			}
		}
	}

private:

	// path to the adb command
	FString ADBPath;

	// > 0 if we've been asked to abort work in progress at the next opportunity
	FThreadSafeCounter StopTaskCounter;

	TMap<FString,FAndroidDeviceInfo>& DeviceMap;
	FCriticalSection* DeviceMapLock;

	FCriticalSection* ADBPathCheckLock;
	bool HasADBPath;
	bool ForceCheck;

	ITcpMessagingModule* TcpMessagingModule;
};

class FAndroidDeviceDetection : public IAndroidDeviceDetection
{
public:

	FAndroidDeviceDetection() :
		DetectionThread(nullptr),
		DetectionThreadRunnable(nullptr)
	{
		// create and fire off our device detection thread
		DetectionThreadRunnable = new FAndroidDeviceDetectionRunnable(DeviceMap, &DeviceMapLock, &ADBPathCheckLock);
		DetectionThread = FRunnableThread::Create(DetectionThreadRunnable, TEXT("FAndroidDeviceDetectionRunnable"));

		// get the SDK binaries folder and throw it to the runnable
		UpdateADBPath();
	}

	virtual ~FAndroidDeviceDetection()
	{
		if (DetectionThreadRunnable && DetectionThread)
		{
			DetectionThreadRunnable->Stop();
			DetectionThread->WaitForCompletion();
		}
	}

	virtual const TMap<FString,FAndroidDeviceInfo>& GetDeviceMap() override
	{
		return DeviceMap;
	}

	virtual FCriticalSection* GetDeviceMapLock() override
	{
		return &DeviceMapLock;
	}

	virtual FString GetADBPath() override
	{
		FScopeLock PathUpdateLock(&ADBPathCheckLock);
		return ADBPath;
	}

	virtual void UpdateADBPath() override
	{
		FScopeLock PathUpdateLock(&ADBPathCheckLock);
		TCHAR AndroidDirectory[32768] = { 0 };
		FPlatformMisc::GetEnvironmentVariable(TEXT("ANDROID_HOME"), AndroidDirectory, 32768);

		ADBPath.Empty();
		
#if PLATFORM_MAC || PLATFORM_LINUX
		if (AndroidDirectory[0] == 0)
		{
#if PLATFORM_LINUX
			// didn't find ANDROID_HOME, so parse the .bashrc file on Linux
			FArchive* FileReader = IFileManager::Get().CreateFileReader(*FString("~/.bashrc"));
#else
			// didn't find ANDROID_HOME, so parse the .bash_profile file on MAC
			FArchive* FileReader = IFileManager::Get().CreateFileReader(*FString([@"~/.bash_profile" stringByExpandingTildeInPath]));
#endif
			if (FileReader)
			{
				const int64 FileSize = FileReader->TotalSize();
				ANSICHAR* AnsiContents = (ANSICHAR*)FMemory::Malloc(FileSize + 1);
				FileReader->Serialize(AnsiContents, FileSize);
				FileReader->Close();
				delete FileReader;

				AnsiContents[FileSize] = 0;
				TArray<FString> Lines;
				FString(ANSI_TO_TCHAR(AnsiContents)).ParseIntoArrayLines(Lines);
				FMemory::Free(AnsiContents);

				for (int32 Index = Lines.Num()-1; Index >=0; Index--)
				{
					if (AndroidDirectory[0] == 0 && Lines[Index].StartsWith(TEXT("export ANDROID_HOME=")))
					{
						FString Directory;
						Lines[Index].Split(TEXT("="), NULL, &Directory);
						Directory = Directory.Replace(TEXT("\""), TEXT(""));
						FCString::Strcpy(AndroidDirectory, *Directory);
						setenv("ANDROID_HOME", TCHAR_TO_ANSI(AndroidDirectory), 1);
					}
				}
			}
		}
#endif

		if (AndroidDirectory[0] != 0)
		{
#if PLATFORM_WINDOWS
			ADBPath = FString::Printf(TEXT("%s\\platform-tools\\adb.exe"), AndroidDirectory);
#else
			ADBPath = FString::Printf(TEXT("%s/platform-tools/adb"), AndroidDirectory);
#endif

			// if it doesn't exist then just clear the path as we might set it later
			if (!FPaths::FileExists(*ADBPath))
			{
				ADBPath.Empty();
			}
		}
		DetectionThreadRunnable->UpdateADBPath(ADBPath);
	}

private:

	// path to the adb command (local)
	FString ADBPath;

	FRunnableThread* DetectionThread;
	FAndroidDeviceDetectionRunnable* DetectionThreadRunnable;

	TMap<FString,FAndroidDeviceInfo> DeviceMap;
	FCriticalSection DeviceMapLock;
	FCriticalSection ADBPathCheckLock;
};


/**
 * Holds the target platform singleton.
 */
static FAndroidDeviceDetection* AndroidDeviceDetectionSingleton = nullptr;


/**
 * Module for detecting android devices.
 */
class FAndroidDeviceDetectionModule : public IAndroidDeviceDetectionModule
{
public:

	/**
	 * Destructor.
	 */
	~FAndroidDeviceDetectionModule( )
	{
		if (AndroidDeviceDetectionSingleton != nullptr)
		{
			delete AndroidDeviceDetectionSingleton;
		}

		AndroidDeviceDetectionSingleton = nullptr;
	}

	virtual IAndroidDeviceDetection* GetAndroidDeviceDetection() override
	{
		if (AndroidDeviceDetectionSingleton == nullptr)
		{
			AndroidDeviceDetectionSingleton = new FAndroidDeviceDetection();
		}

		return AndroidDeviceDetectionSingleton;
	}
};


#undef LOCTEXT_NAMESPACE


IMPLEMENT_MODULE( FAndroidDeviceDetectionModule, AndroidDeviceDetection);
