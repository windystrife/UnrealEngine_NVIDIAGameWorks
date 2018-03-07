// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "OculusHMDModule.h"
#include "OculusHMD.h"
#include "OculusHMDPrivateRHI.h"
#include "Containers/StringConv.h"
#include "Misc/EngineVersion.h"

//-------------------------------------------------------------------------------------------------
// FOculusHMDModule
//-------------------------------------------------------------------------------------------------

FOculusHMDModule::FOculusHMDModule()
{
#if OCULUS_HMD_SUPPORTED_PLATFORMS
	bPreInit = false;
	bPreInitCalled = false;
	OVRPluginHandle = nullptr;
	GraphicsAdapterLuid = 0;
#endif
}


void FOculusHMDModule::ShutdownModule()
{
#if OCULUS_HMD_SUPPORTED_PLATFORMS
	if (OVRPluginHandle)
	{
		FPlatformProcess::FreeDllHandle(OVRPluginHandle);
		OVRPluginHandle = nullptr;
	}
#endif
}

#if OCULUS_HMD_SUPPORTED_PLATFORMS && PLATFORM_ANDROID
extern bool AndroidThunkCpp_IsGearVRApplication();
#endif

FString FOculusHMDModule::GetModuleKeyName() const
{
	return FString(TEXT("OculusHMD"));
}

void FOculusHMDModule::GetModuleAliases(TArray<FString>& AliasesOut) const
{
	// old name for this module (was renamed in 4.17)
	AliasesOut.Add(TEXT("OculusRift"));
	// the old "GearVR" module was merged with this one (also in 4.17)
	AliasesOut.Add(TEXT("GearVR"));

	AliasesOut.Add(TEXT("Oculus"));
	AliasesOut.Add(TEXT("Rift"));
}

bool FOculusHMDModule::PreInit()
{
#if OCULUS_HMD_SUPPORTED_PLATFORMS
	if (!bPreInitCalled)
	{
		bPreInit = false;
		bPreInitCalled = true;

#if PLATFORM_ANDROID
		if (!AndroidThunkCpp_IsGearVRApplication())
		{
			UE_LOG(LogHMD, Log, TEXT("App is not packaged for GearVR"));
			return false;
		}
#endif

		// Only init module when running Game or Editor, and Oculus service is running
		if (!IsRunningDedicatedServer() && OculusHMD::IsOculusServiceRunning())
		{
#if PLATFORM_WINDOWS
			// Load OVRPlugin
			OVRPluginHandle = GetOVRPluginHandle();

			if (!OVRPluginHandle)
			{
				UE_LOG(LogHMD, Log, TEXT("Failed loading OVRPlugin %s"), TEXT(OVRP_VERSION_STR));
				return false;
			}
#endif
			// Initialize OVRPlugin
			if (OVRP_FAILURE(ovrp_PreInitialize2()))
			{
				UE_LOG(LogHMD, Log, TEXT("Failed initializing OVRPlugin %s"), TEXT(OVRP_VERSION_STR));
				return false;
			}

#if PLATFORM_WINDOWS
			const LUID* displayAdapterId;
			if (OVRP_SUCCESS(ovrp_GetDisplayAdapterId2((const void**) &displayAdapterId)) && displayAdapterId)
			{
				SetGraphicsAdapterLuid(*(const uint64*) displayAdapterId);
			}

			const WCHAR* audioInDeviceId;
			if (OVRP_SUCCESS(ovrp_GetAudioInDeviceId2((const void**) &audioInDeviceId)) && audioInDeviceId)
			{
				GConfig->SetString(TEXT("Oculus.Settings"), TEXT("AudioInputDevice"), audioInDeviceId, GEngineIni);
			}

			const WCHAR* audioOutDeviceId;
			if (OVRP_SUCCESS(ovrp_GetAudioOutDeviceId2((const void**) &audioOutDeviceId)) && audioOutDeviceId)
			{
				GConfig->SetString(TEXT("Oculus.Settings"), TEXT("AudioOutputDevice"), audioOutDeviceId, GEngineIni);
			}
#endif

			bPreInit = true;
		}
	}

	return bPreInit;
#else
	return false;
#endif // OCULUS_HMD_SUPPORTED_PLATFORMS
}


bool FOculusHMDModule::IsHMDConnected()
{
#if OCULUS_HMD_SUPPORTED_PLATFORMS
	if (!IsRunningDedicatedServer() && OculusHMD::IsOculusHMDConnected())
	{
		return true;
	}
#endif // OCULUS_HMD_SUPPORTED_PLATFORMS
	return false;
}


uint64 FOculusHMDModule::GetGraphicsAdapterLuid()
{
#if OCULUS_HMD_SUPPORTED_PLATFORMS_D3D11 || OCULUS_HMD_SUPPORTED_PLATFORMS_D3D12
	if (!GraphicsAdapterLuid)
	{
		int GraphicsAdapter;

		if (GConfig->GetInt(TEXT("Oculus.Settings"), TEXT("GraphicsAdapter"), GraphicsAdapter, GEngineIni) &&
			GraphicsAdapter >= 0)
		{
			TRefCountPtr<IDXGIFactory> DXGIFactory;
			TRefCountPtr<IDXGIAdapter> DXGIAdapter;
			DXGI_ADAPTER_DESC DXGIAdapterDesc;

			if (SUCCEEDED(CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)DXGIFactory.GetInitReference())) &&
				SUCCEEDED(DXGIFactory->EnumAdapters(GraphicsAdapter, DXGIAdapter.GetInitReference())) &&
				SUCCEEDED(DXGIAdapter->GetDesc(&DXGIAdapterDesc)))
			{
				FMemory::Memcpy(&GraphicsAdapterLuid, &DXGIAdapterDesc.AdapterLuid, sizeof(GraphicsAdapterLuid));
			}
		}
	}
#endif

#if OCULUS_HMD_SUPPORTED_PLATFORMS
	return GraphicsAdapterLuid;
#else
	return 0;
#endif
}


FString FOculusHMDModule::GetAudioInputDevice()
{
	FString AudioInputDevice;
#if OCULUS_HMD_SUPPORTED_PLATFORMS
	GConfig->GetString(TEXT("Oculus.Settings"), TEXT("AudioInputDevice"), AudioInputDevice, GEngineIni);
#endif
	return AudioInputDevice;
}


FString FOculusHMDModule::GetAudioOutputDevice()
{
	FString AudioOutputDevice;
#if OCULUS_HMD_SUPPORTED_PLATFORMS
	GConfig->GetString(TEXT("Oculus.Settings"), TEXT("AudioOutputDevice"), AudioOutputDevice, GEngineIni);
#endif
	return AudioOutputDevice;
}


TSharedPtr< class IXRTrackingSystem, ESPMode::ThreadSafe > FOculusHMDModule::CreateTrackingSystem()
{
#if OCULUS_HMD_SUPPORTED_PLATFORMS
	if (PreInit())
	{
		OculusHMD::FOculusHMDPtr OculusHMD = FSceneViewExtensions::NewExtension<OculusHMD::FOculusHMD>();

		if (OculusHMD->Startup())
		{
			HeadMountedDisplay = OculusHMD;
			return OculusHMD;
		}
	}
	HeadMountedDisplay = nullptr;
#endif
	return nullptr;
}


TSharedPtr< IHeadMountedDisplayVulkanExtensions, ESPMode::ThreadSafe >  FOculusHMDModule::GetVulkanExtensions()
{
#if OCULUS_HMD_SUPPORTED_PLATFORMS
	if (!VulkanExtensions.IsValid())
	{
		VulkanExtensions = MakeShareable(new OculusHMD::FVulkanExtensions);
	}

	return VulkanExtensions;
#endif
	return nullptr;
}


#if OCULUS_HMD_SUPPORTED_PLATFORMS
void* FOculusHMDModule::GetOVRPluginHandle()
{
	void* OVRPluginHandle = nullptr;

#if PLATFORM_WINDOWS
#if PLATFORM_64BITS
	FString BinariesPath = FPaths::EngineDir() / FString(TEXT("Binaries/ThirdParty/Oculus/OVRPlugin/OVRPlugin/Win64"));
#else
	FString BinariesPath = FPaths::EngineDir() / FString(TEXT("Binaries/ThirdParty/Oculus/OVRPlugin/OVRPlugin/Win32"));
#endif

	FPlatformProcess::PushDllDirectory(*BinariesPath);
	OVRPluginHandle = FPlatformProcess::GetDllHandle(*(BinariesPath / "OVRPlugin.dll"));
	FPlatformProcess::PopDllDirectory(*BinariesPath);
#endif

	return OVRPluginHandle;
}


bool FOculusHMDModule::PoseToOrientationAndPosition(const FQuat& InOrientation, const FVector& InPosition, FQuat& OutOrientation, FVector& OutPosition) const
{
	OculusHMD::CheckInGameThread();

	OculusHMD::FOculusHMD* OculusHMD = static_cast<OculusHMD::FOculusHMD*>(HeadMountedDisplay.Pin().Get());

	if (OculusHMD)
	{
		ovrpPosef InPose;
		InPose.Orientation = OculusHMD::ToOvrpQuatf(InOrientation);
		InPose.Position = OculusHMD::ToOvrpVector3f(InPosition);
		OculusHMD::FPose OutPose;

		if (OculusHMD->ConvertPose(InPose, OutPose))
		{
			OutOrientation = OutPose.Orientation;
			OutPosition = OutPose.Position;
			return true;
		}
	}

	return false;
}


void FOculusHMDModule::SetGraphicsAdapterLuid(uint64 InLuid)
{
	GraphicsAdapterLuid = InLuid;

#if OCULUS_HMD_SUPPORTED_PLATFORMS_D3D11 || OCULUS_HMD_SUPPORTED_PLATFORMS_D3D12
	TRefCountPtr<IDXGIFactory> DXGIFactory;

	if (SUCCEEDED(CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)DXGIFactory.GetInitReference())))
	{
		for (int32 adapterIndex = 0;; adapterIndex++)
		{
			TRefCountPtr<IDXGIAdapter> DXGIAdapter;
			DXGI_ADAPTER_DESC DXGIAdapterDesc;

			if (FAILED(DXGIFactory->EnumAdapters(adapterIndex, DXGIAdapter.GetInitReference())) ||
				FAILED(DXGIAdapter->GetDesc(&DXGIAdapterDesc)))
			{
				break;
			}

			if (!FMemory::Memcmp(&GraphicsAdapterLuid, &DXGIAdapterDesc.AdapterLuid, sizeof(GraphicsAdapterLuid)))
			{
				// Remember this adapterIndex so we use the right adapter, even when we startup without HMD connected
				GConfig->SetInt(TEXT("Oculus.Settings"), TEXT("GraphicsAdapter"), adapterIndex, GEngineIni);
				break;
			}
		}
	}
#endif // OCULUS_HMD_SUPPORTED_PLATFORMS_D3D11 || OCULUS_HMD_SUPPORTED_PLATFORMS_D3D12
}
#endif // OCULUS_HMD_SUPPORTED_PLATFORMS

IMPLEMENT_MODULE(FOculusHMDModule, OculusHMD)
