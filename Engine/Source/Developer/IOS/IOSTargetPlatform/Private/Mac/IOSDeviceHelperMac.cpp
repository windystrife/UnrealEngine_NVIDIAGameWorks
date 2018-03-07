// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved

#include "IOSTargetPlatform.h"

#include <CoreFoundation/CoreFoundation.h>

struct AMDeviceNotificationCallbackInformation
{
	void 		*deviceHandle;
	uint32_t	msgType;
};

struct AFCCommConnection
{
};

#ifdef __cplusplus
extern "C" {
#endif

int AMDeviceConnect (void *device);
int AMDeviceValidatePairing (void *device);
int AMDeviceStartSession (void *device);
int AMDeviceStopSession (void *device);
int AMDeviceDisconnect (void *device);
int AMDeviceNotificationSubscribe(void*, int, int, int, void**);
int AMDeviceStartService(void*, CFStringRef, void**, int);
CFStringRef AMDeviceCopyValue(void*, int unknown, CFStringRef cfstring);
int AMDeviceSecureUpgradeApplication(void*, void*, CFURLRef, void*, void*, void*);
    
uint32 AFCConnectionOpen(void*, uint32, void**);
uint32 AFCConnectionClose(void*);
uint32 AFCDirectoryCreate(void*, const char*);
uint32 AFCFileRefOpen(void*, const char*, uint64, uint64*);
uint32 AFCFileRefClose(void*, uint64);
uint32 AFCFileRefWrite(void*, uint64, void*, uint32);
#ifdef __cplusplus
}
#endif

class AFC
{
public:
    static int StartService(void* DeviceHandle, void** OutHandle)
    {
        int Result = AMDeviceStartService(DeviceHandle, CFSTR("com.apple.afc"), OutHandle, 0);
        return Result;
    }
    
    static int ConnectionOpen(void* ServiceHandle, AFCCommConnection** OutConn)
    {
        void* Conn = NULL;
        int result = AFCConnectionOpen(ServiceHandle, 0, &Conn);
        *OutConn = (AFCCommConnection*)Conn;
        return result;
    }
    
    static int ConnectionClose(AFCCommConnection* Connection)
    {
        return AFCConnectionClose(Connection);
    }
    
    static uint32 DirectoryCreate(AFCCommConnection* Connection, FString Dir)
    {
        const char* IPAPath = TCHAR_TO_UTF8(*Dir);
        uint Result = AFCDirectoryCreate(Connection, IPAPath);
        return Result;
    }
    
    static uint32 FileRefOpen(AFCCommConnection* Connection, FString Path, int mode, uint64* OutHandle)
    {
        const char* IPAPath = TCHAR_TO_UTF8(*Path);
        uint Result = AFCFileRefOpen(Connection, IPAPath, mode, OutHandle);
        return Result;
    }
    
    static uint32 FileRefClose(AFCCommConnection* Connection, uint64 Handle)
    {
        return AFCFileRefClose(Connection, Handle);
    }
    
    static uint32 FileRefWrite(AFCCommConnection* Connection, uint64 Handle, void* Data, uint32 Length)
    {
        return AFCFileRefWrite(Connection, Handle, Data, Length);
    }
};

class InstallProxy
{
public:
    static int StartService(void* DeviceHandle, void** OutHandle)
    {
        int Result = AMDeviceStartService(DeviceHandle, CFSTR("com.apple.mobile.installation_proxy"), OutHandle, 0);
        return Result;
    }
    
    static int SecureUpgradeApplication(void* ServiceConnection, void* DeviceHandle, CFURLRef UrlPath, void* Options, void* InstallCallback, void* UserData)
    {
        return AMDeviceSecureUpgradeApplication(ServiceConnection, DeviceHandle, UrlPath, Options, InstallCallback, UserData);
    }
};

class IOSDevice
{
public:
    IOSDevice(void* device)
    : DeviceHandle(device)
    , AFCHandle(NULL)
    , InstallHandle(NULL)
    {
    }
    
    bool Connect()
    {
        bool Result = false;
		int AppLogLevel = 7;
		CFPreferencesSetAppValue(CFSTR("LogLevel"), CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &AppLogLevel), CFSTR("com.apple.MobileDevice"));

		// connect to the device
        int32 rc = AMDeviceConnect(DeviceHandle);
        if (!rc)
        {
            // validate the pairing
            rc = AMDeviceValidatePairing(DeviceHandle);
            if (!rc)
            {
                // start a session
                rc = AMDeviceStartSession(DeviceHandle);
                Result = !rc;
            }
            else
            {
                UE_LOG(LogTemp, Display, TEXT("Couldn't validate pairing to device"));
            }
        }
        else
        {
            UE_LOG(LogTemp, Display, TEXT("Couldn't connect to device"));
        }
        return Result;
    }
    
    bool Disconnect()
    {
        bool Result = false;
        // close the session
        int32 rc = AMDeviceStopSession(DeviceHandle);
        if (!rc)
        {
            // disconnect from the device
            rc = AMDeviceDisconnect(DeviceHandle);
            Result = !rc;
        }
        else
        {
            UE_LOG(LogTemp, Display, TEXT("Couldn't stop session"));
        }
        return Result;
    }
    
    bool CopyFileToPublicStaging(const FString& SourceFile)
    {
        FString IpaFilename = FPaths::GetCleanFilename(SourceFile);
        return CopyFileToDevice(SourceFile, FString(TEXT("/PublicStaging/")) + IpaFilename, 1024*1024);
    }
    
    bool TryUpgrade(const FString& IPAPath)
    {
        // reconnect to the device and start the Installation service
        Connect();
        InstallProxy::StartService(DeviceHandle, &InstallHandle);

        CFURLRef UrlPath = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, FPlatformString::TCHARToCFString(*IPAPath), kCFURLWindowsPathStyle, 0);
        int Result = InstallProxy::SecureUpgradeApplication(0, DeviceHandle, UrlPath, 0, 0, 0);
        return Result == 0;
    }
    
    void* Handle() const
    {
        return DeviceHandle;
    }
    
    bool CreateDirectory(FString Dir)
    {
        return !(AFC::DirectoryCreate(AFCConnection, Dir) != 0);
    }
    
private:
    
    bool CopyFileToDevice(const FString& IPAPath, FString PathOnDevice, int PacketSize)
    {
        bool Result = false;
        // reconnect to the device and start the AFC service
        Connect();
        if (!AFC::StartService(DeviceHandle, &AFCHandle))
        {
            if (!AFC::ConnectionOpen(AFCHandle, &AFCConnection))
            {
        
                // ensure the directory on the phone exists
                FString DirectoryOnDevice = FPaths::GetPath(PathOnDevice);
                CreateDirectory(DirectoryOnDevice + TEXT("/"));
        
                // transfer the file
                IPlatformFile& PlatformFile = IPlatformFile::GetPlatformPhysical();
                IFileHandle* SourceFile = PlatformFile.OpenRead(*IPAPath);
                if (SourceFile != NULL)
                {
                    uint64 DestinationHandle = 0;
                    if (AFC::FileRefOpen(AFCConnection, PathOnDevice, 3, &DestinationHandle) == 0)
                    {
                        int TotalBytes = 0;
                        int PacketCount = SourceFile->Size() / PacketSize;
                        uint8* buffer = new uint8[PacketSize];
                        for (int Index = 0; Index < PacketCount; ++Index)
                        {
                            if (SourceFile->Read(buffer, PacketSize))
                            {
                                TotalBytes += PacketSize;
                                AFC::FileRefWrite(AFCConnection, DestinationHandle, buffer, PacketSize);
                            }
                        }
                
                        if (SourceFile->Read(buffer, SourceFile->Size() - TotalBytes))
                        {
                            AFC::FileRefWrite(AFCConnection, DestinationHandle, buffer, SourceFile->Size() - TotalBytes);
                        }
                
                        // flush the destination and close
                        AFC::FileRefClose(AFCConnection, DestinationHandle);
                        
                        Result = true;
                    }
                    delete SourceFile;
                }
        
                // stop the AFC service and disconnect from the device
                AFC::ConnectionClose(AFCConnection);
                AFCConnection = NULL;
                Disconnect();
            }
        }
        return Result;
    }
    
	void* DeviceHandle;
    void* AFCHandle;
    AFCCommConnection* AFCConnection;
    void* InstallHandle;
};


/* FIOSDeviceHelper structors
 *****************************************************************************/

static TMap<IOSDevice*, FIOSLaunchDaemonPong> ConnectedDevices;

void FIOSDeviceHelper::Initialize(bool bIsTVOS)
{
    static bool bIsInitialized = false;
    
    if (!bIsInitialized)
    {
        void *subscribe;
        int32 rc = AMDeviceNotificationSubscribe((void*)FIOSDeviceHelper::DeviceCallback, 0, 0, 0, &subscribe);
        if (rc < 0)
        {
            //@todo right out to the log that we can't subscribe
        }
        bIsInitialized = true;
    }
}

void FIOSDeviceHelper::DeviceCallback(void* CallbackInfo)
{
    struct AMDeviceNotificationCallbackInformation* cbi = (AMDeviceNotificationCallbackInformation*)CallbackInfo;
    
    void* deviceHandle = cbi->deviceHandle;
    
    switch(cbi->msgType)
    {
        case 1:
            FIOSDeviceHelper::DoDeviceConnect(deviceHandle);
            break;
            
        case 2:
            FIOSDeviceHelper::DoDeviceDisconnect(deviceHandle);
            break;
            
        case 3:
            break;
    }
}

void FIOSDeviceHelper::DoDeviceConnect(void* deviceHandle)
{
    // connect to the device
    IOSDevice* Device = new IOSDevice(deviceHandle);
    bool Connected = Device->Connect();
    if (Connected)
    {
        // get the needed data
        CFStringRef deviceName = AMDeviceCopyValue(deviceHandle, 0, CFSTR("DeviceName"));
        CFStringRef deviceId = AMDeviceCopyValue(deviceHandle, 0, CFSTR("UniqueDeviceID"));
		CFStringRef productType = AMDeviceCopyValue(deviceHandle, 0, CFSTR("ProductType"));

        // fire the event
        TCHAR idBuffer[128];
		TCHAR nameBuffer[256];
		TCHAR productBuffer[128];
		if (deviceId != NULL)
		{
			FPlatformString::CFStringToTCHAR(deviceId, idBuffer);
		}
		else
		{
			idBuffer[0] = 0;
		}
		if (deviceName != NULL)
		{
			FPlatformString::CFStringToTCHAR(deviceName, nameBuffer);
		}
		else
		{
			nameBuffer[0] = 0;
		}
		if (productType != NULL)
		{
			FPlatformString::CFStringToTCHAR(productType, productBuffer);
		}
		else
		{
			productBuffer[0] = 0;
		}
		FIOSLaunchDaemonPong Event;
        Event.DeviceID = FString::Printf(TEXT("%s@%s"), (FString(productBuffer).Contains("AppleTV") ? TEXT("TVOS") : TEXT("IOS")), idBuffer);
		Event.DeviceName = FString::Printf(TEXT("%s"), nameBuffer);
        Event.bCanReboot = false;
        Event.bCanPowerOn = false;
        Event.bCanPowerOff = false;
		Event.DeviceType = FString::Printf(TEXT("%s"), productBuffer);
		FIOSDeviceHelper::OnDeviceConnected().Broadcast(Event);
                
        // add to the device list
        ConnectedDevices.Add(Device, Event);
        
        // disconnect the device for now
        Device->Disconnect();
    }
    else
    {
        UE_LOG(LogTemp, Display, TEXT("Couldn't connect to device"));
    }
}

void FIOSDeviceHelper::DoDeviceDisconnect(void* deviceHandle)
{
    IOSDevice* device = NULL;
    for (auto DeviceIterator = ConnectedDevices.CreateIterator(); DeviceIterator; ++DeviceIterator)
    {
        if (DeviceIterator.Key()->Handle() == deviceHandle)
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
    // check for valid path
    if (IPAPath.Len() == 0)
    {
        return false;
    }
    
    // check for valid device
    IOSDevice* device = NULL;
    FIOSLaunchDaemonPong DeviceMessage;
    for (auto DeviceIterator = ConnectedDevices.CreateIterator(); DeviceIterator; ++DeviceIterator)
    {
        DeviceMessage = DeviceIterator.Value();
        if (DeviceMessage.DeviceID == DeviceId.ToString())
        {
            device = DeviceIterator.Key();
            break;
        }
    }
    if (device == NULL)
    {
        return false;
    }
    
    // we have the device and a IPA path
    // copy to the stage
    if (device->CopyFileToPublicStaging(IPAPath))
    {
        // install on the device
        return device->TryUpgrade(IPAPath);
    }
    return false;
}

void FIOSDeviceHelper::EnableDeviceCheck(bool OnOff)
{

}