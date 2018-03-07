// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	IOSTargetPlatform.cpp: Implements the FIOSTargetPlatform class.
=============================================================================*/

#include "IOSTargetPlatform.h"
#include "IProjectManager.h"
#include "InstalledPlatformInfo.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Misc/App.h"
#include "Misc/MonitoredProcess.h"
#include "Logging/MessageLog.h"
#if PLATFORM_WINDOWS
#include "WindowsHWrapper.h"
#endif
#if WITH_ENGINE
#include "TextureResource.h"
#endif

/* FIOSTargetPlatform structors
 *****************************************************************************/

FIOSTargetPlatform::FIOSTargetPlatform(bool bInIsTVOS)
	: bIsTVOS(bInIsTVOS)
{
    if (bIsTVOS)
    {
        this->PlatformInfo = PlatformInfo::FindPlatformInfo("TVOS");
    }
#if WITH_ENGINE
	FConfigCacheIni::LoadLocalIniFile(EngineSettings, TEXT("Engine"), true, *PlatformName());
	TextureLODSettings = nullptr; // TextureLODSettings are registered by the device profile.
	StaticMeshLODSettings.Initialize(EngineSettings);
#endif // #if WITH_ENGINE

	// Initialize Ticker for device discovery
	TickDelegate = FTickerDelegate::CreateRaw(this, &FIOSTargetPlatform::HandleTicker);
	TickDelegateHandle = FTicker::GetCoreTicker().AddTicker(TickDelegate, 10.0f);
	
	// initialize the connected device detector
	DeviceHelper.OnDeviceConnected().AddRaw(this, &FIOSTargetPlatform::HandleDeviceConnected);
	DeviceHelper.OnDeviceDisconnected().AddRaw(this, &FIOSTargetPlatform::HandleDeviceDisconnected);
	DeviceHelper.Initialize(bIsTVOS);
}


FIOSTargetPlatform::~FIOSTargetPlatform()
{
	FTicker::GetCoreTicker().RemoveTicker(TickDelegateHandle);
}


/* ITargetPlatform interface
 *****************************************************************************/

void FIOSTargetPlatform::EnableDeviceCheck(bool OnOff)
{
	FIOSDeviceHelper::EnableDeviceCheck(OnOff);
}

void FIOSTargetPlatform::GetAllDevices( TArray<ITargetDevicePtr>& OutDevices ) const
{
	OutDevices.Reset();

	for (auto Iter = Devices.CreateConstIterator(); Iter; ++Iter)
	{
		OutDevices.Add(Iter.Value());
	}
}


ITargetDevicePtr FIOSTargetPlatform::GetDefaultDevice() const
{
	if (Devices.Num() > 0)
	{
		// first device is the default
		auto Iter = Devices.CreateConstIterator();
		if(Iter)
		{
			return Iter.Value();
		}
	}

	return NULL;
}


ITargetDevicePtr FIOSTargetPlatform::GetDevice( const FTargetDeviceId& DeviceId )
{
	return Devices.FindRef(DeviceId);
}

static FString OutputMessage;
static void OnOutput(FString Message)
{
    OutputMessage += Message;
    UE_LOG(LogTemp, Display, TEXT("%s\n"), *Message);
}

bool FIOSTargetPlatform::IsSdkInstalled(bool bProjectHasCode, FString& OutTutorialPath) const
{
#if PLATFORM_MAC
	OutTutorialPath = FString("Shared/Tutorials/InstallingXCodeTutorial");

	// run xcode-select and get the location of Xcode
	FString CmdExe = TEXT("/usr/bin/xcode-select");
	FString CommandLine = FString::Printf(TEXT("--print-path"));
	TSharedPtr<FMonitoredProcess> IPPProcess = MakeShareable(new FMonitoredProcess(CmdExe, CommandLine, true));
	OutputMessage = TEXT("");
	IPPProcess->OnOutput().BindStatic(&OnOutput);
	IPPProcess->Launch();
	while (IPPProcess->Update())
	{
		FPlatformProcess::Sleep(0.01f);
	}
	int RetCode = IPPProcess->GetReturnCode();
	UE_LOG(LogTemp, Display, TEXT("%s"), *OutputMessage);

	bool biOSSDKInstalled = IFileManager::Get().DirectoryExists(*OutputMessage);
#else
	OutTutorialPath = FString("/Engine/Tutorial/Mobile/InstallingiTunesTutorial.InstallingiTunesTutorial");

	// On windows we check if itunes is installed - Perhaps someday make this its own check instead of piggy packing on the SDK check which will create a unintuitive error message when it fails

	// The logic here is to assume the correct Apple dll does not exist and then check the various locations it could be in, setting this to true when it is found
	// Code is structured for clarity not performance
	// See Engine\Source\Programs\IOS\MobileDeviceInterface\MobileDevice.cs for reference
	bool biOSSDKInstalled = false; 

	HKEY hKey;
	TCHAR dllPath[256];
	unsigned long pathSize = 256;
	
	// Add future version checks here

	// Check for iTunes 12
	if(!biOSSDKInstalled
		&& RegOpenKeyEx(HKEY_LOCAL_MACHINE, TEXT("SOFTWARE\\Wow6432Node\\Apple Inc.\\Apple Mobile Device Support\\Shared"), 0, KEY_READ, &hKey) == ERROR_SUCCESS
		&& RegQueryValueEx(hKey, TEXT("MobileDeviceDLL"), 0, NULL, (BYTE*)dllPath, &pathSize) == ERROR_SUCCESS
		&&  IFileManager::Get().FileSize(*FString(dllPath)) != INDEX_NONE)
		{
		biOSSDKInstalled = true;
	}
	
	// Check for iTunes 11
	if(!biOSSDKInstalled
		&& RegOpenKeyEx(HKEY_LOCAL_MACHINE, TEXT("SOFTWARE\\Wow6432Node\\Apple Inc.\\Apple Mobile Device Support\\Shared"), 0, KEY_READ, &hKey) == ERROR_SUCCESS
		&& RegQueryValueEx(hKey, TEXT("iTunesMobileDeviceDLL"), 0, NULL, (BYTE*)dllPath, &pathSize) == ERROR_SUCCESS
		&&  IFileManager::Get().FileSize(*FString(dllPath)) != INDEX_NONE)
			{
		biOSSDKInstalled = true;
	}

#endif
	return biOSSDKInstalled;
}

int32 FIOSTargetPlatform::CheckRequirements(const FString& ProjectPath, bool bProjectHasCode, FString& OutTutorialPath, FString& OutDocumentationPath, FText& CustomizedLogMessage) const
{
	OutDocumentationPath = TEXT("Platforms/iOS/QuickStart/6");

	int32 bReadyToBuild = ETargetPlatformReadyStatus::Ready; // @todo How do we check that the iOS SDK is installed when building from Windows? Is that even possible?
	if (!IsSdkInstalled(bProjectHasCode, OutTutorialPath))
	{
		bReadyToBuild |= ETargetPlatformReadyStatus::SDKNotFound;
	}
#if PLATFORM_MAC
	OutTutorialPath = FString("/Engine/Tutorial/Installation/InstallingXCodeTutorial.InstallingXCodeTutorial");
    // shell to certtool
#else
	if (!FInstalledPlatformInfo::Get().IsValidPlatform(GetPlatformInfo().BinaryFolderName, EProjectType::Code))
	{
		if (bProjectHasCode)
		{
			OutTutorialPath = FString("/Engine/Tutorial/Mobile/iOSonPCRestrictions.iOSonPCRestrictions");
			bReadyToBuild |= ETargetPlatformReadyStatus::CodeUnsupported;
		}
		if (IProjectManager::Get().IsNonDefaultPluginEnabled())
		{
			OutTutorialPath = FString("/Engine/Tutorial/Mobile/iOSonPCValidPlugins.iOSonPCValidPlugins");
			bReadyToBuild |= ETargetPlatformReadyStatus::PluginsUnsupported;
		}
	}
#endif

	// shell to IPP and get the status of the provision and cert

	bool bForDistribtion = false;
	GConfig->GetBool(TEXT("/Script/UnrealEd.ProjectPackagingSettings"), TEXT("ForDistribution"), bForDistribtion, GGameIni);

	FString BundleIdentifier;
	GConfig->GetString(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("BundleIdentifier"), BundleIdentifier, GEngineIni);
	BundleIdentifier = BundleIdentifier.Replace(TEXT("[PROJECT_NAME]"), FApp::GetProjectName());
	BundleIdentifier = BundleIdentifier.Replace(TEXT("_"), TEXT(""));
#if PLATFORM_MAC
    FString CmdExe = TEXT("/bin/sh");
    FString ScriptPath = FPaths::ConvertRelativePathToFull(FPaths::EngineDir() / TEXT("Build/BatchFiles/Mac/RunMono.sh"));
    FString IPPPath = FPaths::ConvertRelativePathToFull(FPaths::EngineDir() / TEXT("Binaries/DotNET/IOS/IPhonePackager.exe"));
	FString CommandLine = FString::Printf(TEXT("\"%s\" \"%s\" Validate Engine -project \"%s\" -bundlename \"%s\" %s"), *ScriptPath, *IPPPath, *ProjectPath, *(BundleIdentifier), (bForDistribtion ? TEXT("-distribution") : TEXT("")) );
#else
	FString CmdExe = FPaths::ConvertRelativePathToFull(FPaths::EngineDir() / TEXT("Binaries/DotNET/IOS/IPhonePackager.exe"));
	FString CommandLine = FString::Printf(TEXT("Validate Engine -project \"%s\" -bundlename \"%s\" %s"), *ProjectPath, *(BundleIdentifier), (bForDistribtion ? TEXT("-distribution") : TEXT("")) );
	FString RemoteServerName;
	GConfig->GetString(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("RemoteServerName"), RemoteServerName, GEngineIni);
	if (RemoteServerName.Len() == 0)
	{
		bReadyToBuild |= ETargetPlatformReadyStatus::RemoveServerNameEmpty;
	}

#endif
	TSharedPtr<FMonitoredProcess> IPPProcess = MakeShareable(new FMonitoredProcess(CmdExe, CommandLine, true));
	OutputMessage = TEXT("");
	IPPProcess->OnOutput().BindStatic(&OnOutput);
	IPPProcess->Launch();
	while(IPPProcess->Update())
	{
		FPlatformProcess::Sleep(0.01f);
	}
	int RetCode = IPPProcess->GetReturnCode();
    UE_LOG(LogTemp, Display, TEXT("%s"), *OutputMessage);
	if (RetCode == 14)
	{
		OutTutorialPath = FString("/Engine/Tutorial/Mobile/CreatingInfoPlist.CreatingInfoPlist");
		bReadyToBuild |= ETargetPlatformReadyStatus::ManifestNotFound;
	}
	else if (RetCode == 13)
	{
		OutTutorialPath = FString("/Engine/Tutorial/Mobile/CreatingSigningCertAndProvisionTutorial.CreatingSigningCertAndProvisionTutorial");
		bReadyToBuild |= ETargetPlatformReadyStatus::SigningKeyNotFound;
		bReadyToBuild |= ETargetPlatformReadyStatus::ProvisionNotFound;
	}
	else if (RetCode == 12)
	{
		OutTutorialPath = FString("/Engine/Tutorial/Mobile/CreatingSigningCertAndProvisionTutorial.CreatingSigningCertAndProvisionTutorial");
		bReadyToBuild |= ETargetPlatformReadyStatus::SigningKeyNotFound;
	}
	else if (RetCode == 11)
	{
		OutTutorialPath = FString("/Engine/Tutorial/Mobile/CreatingSigningCertAndProvisionTutorial.CreatingSigningCertAndProvisionTutorial");
		bReadyToBuild |= ETargetPlatformReadyStatus::ProvisionNotFound;
	}

	return bReadyToBuild;
}


/* FIOSTargetPlatform implementation
 *****************************************************************************/

void FIOSTargetPlatform::PingNetworkDevices()
{
	// disabled for now because we find IOS devices from the USB, this is a relic from ULD, but it may be needed in the future
/*	if (!MessageEndpoint.IsValid())
	{
		MessageEndpoint = FMessageEndpoint::Builder("FIOSTargetPlatform")
			.Handling<FIOSLaunchDaemonPong>(this, &FIOSTargetPlatform::HandlePongMessage);
	}

	if (MessageEndpoint.IsValid())
	{
		MessageEndpoint->Publish(new FIOSLaunchDaemonPing(), EMessageScope::Network);
	}

	// remove disconnected & timed out devices
	FDateTime Now = FDateTime::UtcNow();

	for (auto DeviceIt = Devices.CreateIterator(); DeviceIt; ++DeviceIt)
	{
		FIOSTargetDevicePtr Device = DeviceIt->Value;

		if (Now > Device->LastPinged + FTimespan::FromSeconds(60.0))
		{
			DeviceIt.RemoveCurrent();
			DeviceLostEvent.Broadcast(Device.ToSharedRef());
		}
	}*/
}


/* FIOSTargetPlatform callbacks
 *****************************************************************************/

void FIOSTargetPlatform::HandlePongMessage( const FIOSLaunchDaemonPong& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context )
{
	FTargetDeviceId DeviceId;
	FTargetDeviceId::Parse(Message.DeviceID, DeviceId);

	FIOSTargetDevicePtr& Device = Devices.FindOrAdd(DeviceId);

	if (!Device.IsValid())
	{
		Device = MakeShareable(new FIOSTargetDevice(*this));

		Device->SetFeature(ETargetDeviceFeatures::Reboot, Message.bCanReboot);
		Device->SetFeature(ETargetDeviceFeatures::PowerOn, Message.bCanPowerOn);
		Device->SetFeature(ETargetDeviceFeatures::PowerOff, Message.bCanPowerOff);
		Device->SetDeviceId(DeviceId);
		Device->SetDeviceName(Message.DeviceName);
		Device->SetDeviceType(Message.DeviceType);
		Device->SetDeviceEndpoint(Context->GetSender());
		Device->SetIsSimulated(Message.DeviceID.Contains(TEXT("Simulator")));

		DeviceDiscoveredEvent.Broadcast(Device.ToSharedRef());
	}

	Device->LastPinged = FDateTime::UtcNow();
}

void FIOSTargetPlatform::HandleDeviceConnected(const FIOSLaunchDaemonPong& Message)
{
	FTargetDeviceId DeviceId;
	FTargetDeviceId::Parse(Message.DeviceID, DeviceId);
	
	FIOSTargetDevicePtr& Device = Devices.FindOrAdd(DeviceId);
	
	if (!Device.IsValid())
	{
		if ((Message.DeviceType.Contains(TEXT("AppleTV")) && bIsTVOS) || (!Message.DeviceType.Contains(TEXT("AppleTV")) && !bIsTVOS))
		{
			Device = MakeShareable(new FIOSTargetDevice(*this));

			Device->SetFeature(ETargetDeviceFeatures::Reboot, Message.bCanReboot);
			Device->SetFeature(ETargetDeviceFeatures::PowerOn, Message.bCanPowerOn);
			Device->SetFeature(ETargetDeviceFeatures::PowerOff, Message.bCanPowerOff);
			Device->SetDeviceId(DeviceId);
			Device->SetDeviceName(Message.DeviceName);
			Device->SetDeviceType(Message.DeviceType);
			Device->SetIsSimulated(Message.DeviceID.Contains(TEXT("Simulator")));

			DeviceDiscoveredEvent.Broadcast(Device.ToSharedRef());
		}
		else
		{
			return;
		}
	}
	
	// Add a very long time period to prevent the devices from getting disconnected due to a lack of pong messages
	Device->LastPinged = FDateTime::UtcNow() + FTimespan::FromDays(100.0);
}


void FIOSTargetPlatform::HandleDeviceDisconnected(const FIOSLaunchDaemonPong& Message)
{
	FTargetDeviceId DeviceId;
	FTargetDeviceId::Parse(Message.DeviceID, DeviceId);
	
	FIOSTargetDevicePtr& Device = Devices.FindOrAdd(DeviceId);
	
	if (Device.IsValid())
	{
		DeviceLostEvent.Broadcast(Device.ToSharedRef());
		Devices.Remove(DeviceId);
	}
}

bool FIOSTargetPlatform::HandleTicker(float DeltaTime)
{
	PingNetworkDevices();

	return true;
}


/* ITargetPlatform interface
 *****************************************************************************/

static bool SupportsES2()
{
	// default to supporting ES2
	bool bSupportsOpenGLES2 = true;
	GConfig->GetBool(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("bSupportsOpenGLES2"), bSupportsOpenGLES2, GEngineIni);
	return bSupportsOpenGLES2;
}

static bool SupportsMetal()
{
	// default to NOT supporting metal
	bool bSupportsMetal = false;
	GConfig->GetBool(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("bSupportsMetal"), bSupportsMetal, GEngineIni);
	return bSupportsMetal;
}

static bool SupportsMetalMRT()
{
	// default to NOT supporting metal MRT
	bool bSupportsMetalMRT = false;
	GConfig->GetBool(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("bSupportsMetalMRT"), bSupportsMetalMRT, GEngineIni);
	return bSupportsMetalMRT;
}

static bool CookPVRTC()
{
	// default to using PVRTC
	bool bCookPVRTCTextures = true;
	GConfig->GetBool(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("bCookPVRTCTextures"), bCookPVRTCTextures, GEngineIni);
	return bCookPVRTCTextures;
}

static bool CookASTC()
{
	// default to not using ASTC
	bool bCookASTCTextures = true;
	GConfig->GetBool(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("bCookASTCTextures"), bCookASTCTextures, GEngineIni);
	return bCookASTCTextures;
}

bool FIOSTargetPlatform::SupportsFeature( ETargetPlatformFeatures Feature ) const
{
	switch (Feature)
	{
		case ETargetPlatformFeatures::Packaging:
			return true;

		case ETargetPlatformFeatures::MobileRendering:
		case ETargetPlatformFeatures::LowQualityLightmaps:
			return SupportsES2() || SupportsMetal();
			
		case ETargetPlatformFeatures::DeferredRendering:
		case ETargetPlatformFeatures::HighQualityLightmaps:
			return SupportsMetalMRT();

		default:
			break;
	}
	
	return TTargetPlatformBase<FIOSPlatformProperties>::SupportsFeature(Feature);
}


#if WITH_ENGINE


void FIOSTargetPlatform::GetAllPossibleShaderFormats( TArray<FName>& OutFormats ) const
{
	static FName NAME_GLSL_ES2_IOS(TEXT("GLSL_ES2_IOS"));
	static FName NAME_SF_METAL(TEXT("SF_METAL"));
	static FName NAME_SF_METAL_MRT(TEXT("SF_METAL_MRT"));

	if (bIsTVOS)
	{
		if (SupportsMetalMRT())
		{
			OutFormats.AddUnique(NAME_SF_METAL_MRT);
		}

		// because we are currently using IOS settings, we will always use metal, even if Metal isn't listed as being supported
		// however, if MetalMRT is specific and Metal is set to false, then we will just use MetalMRT
		if (SupportsMetal() || !SupportsMetalMRT())
		{
			OutFormats.AddUnique(NAME_SF_METAL);
		}
	}
	else
	{
		if (SupportsES2())
		{
			OutFormats.AddUnique(NAME_GLSL_ES2_IOS);
		}

		if (SupportsMetal())
		{
			OutFormats.AddUnique(NAME_SF_METAL);
		}

		if (SupportsMetalMRT())
		{
			OutFormats.AddUnique(NAME_SF_METAL_MRT);
		}
	}
}

void FIOSTargetPlatform::GetAllTargetedShaderFormats( TArray<FName>& OutFormats ) const
{
	GetAllPossibleShaderFormats(OutFormats);
}

// we remap some of the defaults (with PVRTC and ASTC formats)
static FName FormatRemap[] =
{
	// original				PVRTC						ASTC
	FName(TEXT("DXT1")),	FName(TEXT("PVRTC2")),		FName(TEXT("ASTC_RGB")),
	FName(TEXT("DXT5")),	FName(TEXT("PVRTC4")),		FName(TEXT("ASTC_RGBA")),
	FName(TEXT("DXT5n")),	FName(TEXT("PVRTCN")),		FName(TEXT("ASTC_NormalAG")),
	FName(TEXT("BC5")),		FName(TEXT("PVRTCN")),		FName(TEXT("ASTC_NormalRG")),
	FName(TEXT("AutoDXT")),	FName(TEXT("AutoPVRTC")),	FName(TEXT("ASTC_RGBAuto")),
	FName(TEXT("BC4")),		FName(TEXT("G8")),			FName(TEXT("G8")),
};
static FName NameBGRA8(TEXT("BGRA8"));
static FName NameG8 = FName(TEXT("G8"));

void FIOSTargetPlatform::GetTextureFormats( const UTexture* Texture, TArray<FName>& OutFormats ) const
{
	check(Texture);

	static FName NamePOTERROR(TEXT("POTERROR"));

	FName TextureFormatName = NAME_None;

	// forward rendering only needs one channel for shadow maps
	if (Texture->LODGroup == TEXTUREGROUP_Shadowmap && !SupportsMetalMRT())
	{
		TextureFormatName = NameG8;
	}

	// if we didn't assign anything specially, then use the defaults
	if (TextureFormatName == NAME_None)
	{
		TextureFormatName = GetDefaultTextureFormatName(this, Texture, EngineSettings, false);
	}

	// perform any remapping away from defaults
	bool bFoundRemap = false;
	bool bIncludePVRTC = !bIsTVOS && CookPVRTC();
	bool bIncludeASTC = bIsTVOS || CookASTC();

	if (Texture->bForcePVRTC4 && CookPVRTC())
	{
		OutFormats.AddUnique(FName(TEXT("PVRTC4")));
		OutFormats.AddUnique(FName(TEXT("PVRTCN")));
		return;
	}

	for (int32 RemapIndex = 0; RemapIndex < ARRAY_COUNT(FormatRemap); RemapIndex += 3)
	{
		if (TextureFormatName == FormatRemap[RemapIndex])
		{
			// we found a remapping
			bFoundRemap = true;
			// include the formats we want (use ASTC first so that it is preferred at runtime if they both exist and it's supported)
			if (bIncludeASTC)
			{
				OutFormats.AddUnique(FormatRemap[RemapIndex + 2]);
			}
			if (bIncludePVRTC)
			{
				// handle non-power of 2 textures
				if (!Texture->Source.IsPowerOfTwo() && Texture->PowerOfTwoMode == ETexturePowerOfTwoSetting::None)
				{
					// option 1: Uncompress, but users will get very large textures unknowningly
					// OutFormats.AddUnique(NameBGRA8);
					// option 2: Use an "error message" texture so they see it in game
					OutFormats.AddUnique(NamePOTERROR);
				}
				else
				{
					OutFormats.AddUnique(FormatRemap[RemapIndex + 1]);
				}
			}
		}
	}

	// if we didn't already remap above, add it now
	if (!bFoundRemap)
	{
		OutFormats.Add(TextureFormatName);
	}
}

void FIOSTargetPlatform::GetAllTextureFormats(TArray<FName>& OutFormats) const 
{
	bool bFoundRemap = false;
	bool bIncludePVRTC = !bIsTVOS && CookPVRTC();
	bool bIncludeASTC = bIsTVOS || CookASTC();

	GetAllDefaultTextureFormats(this, OutFormats, false);

	for (int32 RemapIndex = 0; RemapIndex < ARRAY_COUNT(FormatRemap); RemapIndex += 3)
	{
		OutFormats.Remove(FormatRemap[RemapIndex+0]);
	}

	// include the formats we want (use ASTC first so that it is preferred at runtime if they both exist and it's supported)
	if (bIncludeASTC)
	{
		for (int32 RemapIndex = 0; RemapIndex < ARRAY_COUNT(FormatRemap); RemapIndex += 3)
		{
			OutFormats.AddUnique(FormatRemap[RemapIndex + 2]);
		}
	}
	if (bIncludePVRTC)
	{
		for (int32 RemapIndex = 0; RemapIndex < ARRAY_COUNT(FormatRemap); RemapIndex += 3)
		{
			OutFormats.AddUnique(FormatRemap[RemapIndex + 1]);
		}
	}
}


const UTextureLODSettings& FIOSTargetPlatform::GetTextureLODSettings() const
{
	return *TextureLODSettings;
}


FName FIOSTargetPlatform::GetWaveFormat( const class USoundWave* Wave ) const
{
	static FName NAME_ADPCM(TEXT("ADPCM"));
	return NAME_ADPCM;
}


void FIOSTargetPlatform::GetAllWaveFormats(TArray<FName>& OutFormat) const
{
	static FName NAME_ADPCM(TEXT("ADPCM"));
	OutFormat.Add(NAME_ADPCM);
}

#endif // WITH_ENGINE

