// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SteamVRHMD.h"
#include "SteamVRPrivate.h"

#include "Misc/App.h"
#include "Misc/CoreDelegates.h"
#include "Misc/EngineVersion.h"
#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "SceneViewport.h"
#include "PostProcess/PostProcessHMD.h"
#include "Classes/SteamVRFunctionLibrary.h"
#include "Engine/GameEngine.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "IHeadMountedDisplayVulkanExtensions.h"
#include "Debug/DebugDrawService.h"

#include "EngineAnalytics.h"
#include "Runtime/Analytics/Analytics/Public/Interfaces/IAnalyticsProvider.h"

#include "Engine/Canvas.h"
#include "CanvasItem.h"

#if WITH_EDITOR
#include "Editor/UnrealEd/Classes/Editor/EditorEngine.h"
#endif

#ifndef ARRAYSIZE
#define ARRAYSIZE( a ) ( sizeof( ( a ) ) / sizeof( ( a )[ 0 ] ) )
#endif

#if PLATFORM_MAC
// For FResourceBulkDataInterface
#include "Containers/ResourceArray.h"
#include <Metal/Metal.h>
#endif

static TAutoConsoleVariable<int32> CShowDebug(TEXT("vr.SteamVR.ShowDebug"), 0, TEXT("If non-zero, will draw debugging info to the canvas"));

// Adaptive Pixel Density
static TAutoConsoleVariable<float> CUseAdaptivePDMin(TEXT("vr.SteamVR.PixelDensityMin"), 0.7f, TEXT("Minimum pixel density, as a float"));
static TAutoConsoleVariable<float> CUseAdaptivePDMax(TEXT("vr.SteamVR.PixelDensityMax"), 1.0f, TEXT("Maximum pixel density, as a float"));
static TAutoConsoleVariable<float> CAdaptiveGPUTimeThreshold(TEXT("vr.SteamVR.AdaptiveGPUTimeThreshold"), 11.1f, TEXT("Time, in ms, to aim for stabilizing the GPU frame time at"));
static TAutoConsoleVariable<float> CDebugAdaptiveGPUTime(TEXT("vr.SteamVR.AdaptiveDebugGPUTime"), 0.f, TEXT("Added to the the GPU frame timing, in ms, for testing"));
static TAutoConsoleVariable<int32> CDebugAdaptiveOutput(TEXT("vr.SteamVR.PixelDensityAdaptiveDebugOutput"), 0, TEXT("If non-zero, the adaptive pixel density will print debugging info to the log."));
static TAutoConsoleVariable<int32> CDebugAdaptiveCycle(TEXT("vr.SteamVR.PixelDensityAdaptiveDebugCycle"), 0, TEXT("If non-zero, the adaptive pixel density will cycle from max to min pixel density, and then jump to max."));
static TAutoConsoleVariable<int32> CDebugAdaptivePostProcess(TEXT("vr.SteamVR.PixelDensityAdaptivePostProcess"), 1, TEXT("If non-zero, when the adaptive density changes, we'll disable TAA for a few frames to clear the buffers."));

// Visibility mesh
static TAutoConsoleVariable<int32> CUseSteamVRVisibleAreaMesh(TEXT("vr.SteamVR.UseVisibleAreaMesh"), 1, TEXT("If non-zero, SteamVR will use the visible area mesh in addition to the hidden area mesh optimization.  This may be problematic on beta versions of platforms."));

/** Helper function for acquiring the appropriate FSceneViewport */
FSceneViewport* FindSceneViewport()
{
	if (!GIsEditor)
	{
		UGameEngine* GameEngine = Cast<UGameEngine>(GEngine);
		return GameEngine->SceneViewport.Get();
	}
#if WITH_EDITOR
	else
	{
		UEditorEngine* EditorEngine = CastChecked<UEditorEngine>( GEngine );
		FSceneViewport* PIEViewport = (FSceneViewport*)EditorEngine->GetPIEViewport();
		if( PIEViewport != nullptr && PIEViewport->IsStereoRenderingAllowed() )
		{
			// PIE is setup for stereo rendering
			return PIEViewport;
		}
		else
		{
			// Check to see if the active editor viewport is drawing in stereo mode
			// @todo vreditor: Should work with even non-active viewport!
			FSceneViewport* EditorViewport = (FSceneViewport*)EditorEngine->GetActiveViewport();
			if( EditorViewport != nullptr && EditorViewport->IsStereoRenderingAllowed() )
			{
				return EditorViewport;
			}
		}
	}
#endif
	return nullptr;
}

#if STEAMVR_SUPPORTED_PLATFORMS
// Wrapper around vr::IVRSystem::GetStringTrackedDeviceProperty
static FString GetFStringTrackedDeviceProperty(vr::IVRSystem* VRSystem, uint32 DeviceIndex, vr::ETrackedDeviceProperty Property, vr::TrackedPropertyError* ErrorPtr = nullptr)
{
	check(VRSystem != nullptr);

	vr::TrackedPropertyError Error;
	TArray<char> Buffer;
	Buffer.AddUninitialized(vr::k_unMaxPropertyStringSize);

	int Size = VRSystem->GetStringTrackedDeviceProperty(DeviceIndex, Property, Buffer.GetData(), Buffer.Num(), &Error);
	if (Error == vr::TrackedProp_BufferTooSmall)
	{
		Buffer.AddUninitialized(Size - Buffer.Num());
		Size = VRSystem->GetStringTrackedDeviceProperty(DeviceIndex, Property, Buffer.GetData(), Buffer.Num(), &Error);
	}

	if (ErrorPtr)
	{
		*ErrorPtr = Error;
	}

	if (Error == vr::TrackedProp_Success)
	{
		return UTF8_TO_TCHAR(Buffer.GetData());
	}
	else
	{
		return UTF8_TO_TCHAR(VRSystem->GetPropErrorNameFromEnum(Error));
	}
}
#endif //STEAMVR_SUPPORTED_PLATFORMS

//---------------------------------------------------
// SteamVR Plugin Implementation
//---------------------------------------------------

class FSteamVRPlugin : public ISteamVRPlugin
{
	/** IHeadMountedDisplayModule implementation */
	virtual TSharedPtr< class IXRTrackingSystem, ESPMode::ThreadSafe > CreateTrackingSystem() override;

	FString GetModuleKeyName() const
	{
		return FString(TEXT("SteamVR"));
	}

public:
	FSteamVRPlugin()
#if !STEAMVR_SUPPORTED_PLATFORMS
	{
	}

#else //STEAMVR_SUPPORTED_PLATFORMS
		: VRSystem(nullptr)
	{
		LoadOpenVRModule();
	}

	virtual void StartupModule() override
	{
		IHeadMountedDisplayModule::StartupModule();
	
		LoadOpenVRModule();
	}
	
	virtual void ShutdownModule() override
	{
		IHeadMountedDisplayModule::ShutdownModule();
		
		UnloadOpenVRModule();
	}

	virtual vr::IVRSystem* GetVRSystem() const override
	{
		return VRSystem;
	}
	
	bool LoadOpenVRModule()
	{
#if PLATFORM_WINDOWS
#if PLATFORM_64BITS
		FString RootOpenVRPath;
		TCHAR VROverridePath[MAX_PATH];
		FPlatformMisc::GetEnvironmentVariable(TEXT("VR_OVERRIDE"), VROverridePath, MAX_PATH);
		
		if (FCString::Strlen(VROverridePath) > 0)
		{
			RootOpenVRPath = FString::Printf(TEXT("%s\\bin\\win64\\"), VROverridePath);
		}
		else
		{
			RootOpenVRPath = FPaths::EngineDir() / FString::Printf(TEXT("Binaries/ThirdParty/OpenVR/%s/Win64/"), OPENVR_SDK_VER);
		}
		
		FPlatformProcess::PushDllDirectory(*RootOpenVRPath);
		OpenVRDLLHandle = FPlatformProcess::GetDllHandle(*(RootOpenVRPath + "openvr_api.dll"));
		FPlatformProcess::PopDllDirectory(*RootOpenVRPath);
#else
		FString RootOpenVRPath = FPaths::EngineDir() / FString::Printf(TEXT("Binaries/ThirdParty/OpenVR/%s/Win32/"), OPENVR_SDK_VER);
		FPlatformProcess::PushDllDirectory(*RootOpenVRPath);
		OpenVRDLLHandle = FPlatformProcess::GetDllHandle(*(RootOpenVRPath + "openvr_api.dll"));
		FPlatformProcess::PopDllDirectory(*RootOpenVRPath);
#endif
#elif PLATFORM_MAC
		FString RootOpenVRPath = FPaths::EngineDir() / FString::Printf(TEXT("Binaries/ThirdParty/OpenVR/%s/osx32/"), OPENVR_SDK_VER);
		UE_LOG(LogHMD, Log, TEXT("Tried to load %s"), *(RootOpenVRPath + "libopenvr_api.dylib"));
		OpenVRDLLHandle = FPlatformProcess::GetDllHandle(*(RootOpenVRPath + "libopenvr_api.dylib"));
#elif PLATFORM_LINUX
		FString RootOpenVRPath = FPaths::EngineDir() / FString::Printf(TEXT("Binaries/ThirdParty/OpenVR/%s/linux64/"), OPENVR_SDK_VER);
		OpenVRDLLHandle = FPlatformProcess::GetDllHandle(*(RootOpenVRPath + "libopenvr_api.so"));
#else
		#error "SteamVRHMD is not supported for this platform."
#endif	//PLATFORM_WINDOWS
		
		if (!OpenVRDLLHandle)
		{
			UE_LOG(LogHMD, Log, TEXT("Failed to load OpenVR library."));
			return false;
		}
		
		//@todo steamvr: Remove GetProcAddress() workaround once we update to Steamworks 1.33 or higher
		FSteamVRHMD::VRIsHmdPresentFn = (pVRIsHmdPresent)FPlatformProcess::GetDllExport(OpenVRDLLHandle, TEXT("VR_IsHmdPresent"));
		FSteamVRHMD::VRGetGenericInterfaceFn = (pVRGetGenericInterface)FPlatformProcess::GetDllExport(OpenVRDLLHandle, TEXT("VR_GetGenericInterface"));
		
		// Note:  If this fails to compile, it's because you merged a new OpenVR version, and didn't put in the module hacks marked with @epic in openvr.h
		vr::VR_SetGenericInterfaceCallback(FSteamVRHMD::VRGetGenericInterfaceFn);

		// Verify that we've bound correctly to the DLL functions
		if (!FSteamVRHMD::VRIsHmdPresentFn || !FSteamVRHMD::VRGetGenericInterfaceFn)
		{
			UE_LOG(LogHMD, Log, TEXT("Failed to GetProcAddress() on openvr_api.dll"));
			UnloadOpenVRModule();
			
			return false;
		}
		
		return true;
	}
	
	void UnloadOpenVRModule()
	{
		if (OpenVRDLLHandle != nullptr)
		{
			FPlatformProcess::FreeDllHandle(OpenVRDLLHandle);
			OpenVRDLLHandle = nullptr;
		}
	}

	virtual bool IsHMDConnected() override
	{
		return FSteamVRHMD::VRIsHmdPresentFn ? (bool)(*FSteamVRHMD::VRIsHmdPresentFn)() : false;
	}

	virtual void Reset() override
	{
		VRSystem = nullptr;
	}
	
	uint64 GetGraphicsAdapterLuid() override
	{
#if PLATFORM_MAC
		
		id<MTLDevice> SelectedDevice = nil;
		
		// @TODO  currently, for mac, GetGraphicsAdapterLuid() is used to return a device index (how the function 
		//        "GetGraphicsAdapter" used to work), not a ID... eventually we want the HMD module to return the 
		//        MTLDevice's registryID, but we cannot fully handle that until we drop support for 10.12
		//  NOTE: this is why we  use -1 as a sentinel value representing "no device" (instead of 0, which is used in the LUID case)
		{
			// HACK:  Temporarily stand up the VRSystem to get a graphics adapter.  We're pretty sure we're going to use SteamVR if we get in here, but not guaranteed
			vr::EVRInitError VRInitErr = vr::VRInitError_None;
			// Attempt to initialize the VRSystem device
			vr::IVRSystem* TempVRSystem = vr::VR_Init(&VRInitErr, vr::VRApplication_Scene);
			if ((TempVRSystem == nullptr) || (VRInitErr != vr::VRInitError_None))
			{
				UE_LOG(LogHMD, Log, TEXT("Failed to initialize OpenVR with code %d"), (int32)VRInitErr);
				return (uint64)-1;
			}
			
			// Make sure that the version of the HMD we're compiled against is correct.  This will fill out the proper vtable!
			TempVRSystem = (vr::IVRSystem*)(*FSteamVRHMD::VRGetGenericInterfaceFn)(vr::IVRSystem_Version, &VRInitErr);
			if ((TempVRSystem == nullptr) || (VRInitErr != vr::VRInitError_None))
			{
				return (uint64)-1;
			}
			
			TempVRSystem->GetOutputDevice((uint64_t*)((void*)&SelectedDevice), vr::TextureType_IOSurface);
			
			vr::VR_Shutdown();
		}

		if(SelectedDevice == nil)
		{
			return (uint64)-1;
		}

		TArray<FMacPlatformMisc::FGPUDescriptor> const& GPUs = FPlatformMisc::GetGPUDescriptors();
		check(GPUs.Num() > 0);

		int32 DeviceIndex = -1;
		TArray<FString> NameComponents;
		bool bFoundDefault = false;
		for (uint32 i = 0; i < GPUs.Num(); i++)
		{
			FMacPlatformMisc::FGPUDescriptor const& GPU = GPUs[i];
			if (([SelectedDevice.name rangeOfString : @"Nvidia" options : NSCaseInsensitiveSearch].location != NSNotFound && GPU.GPUVendorId == 0x10DE)
				|| ([SelectedDevice.name rangeOfString : @"AMD" options : NSCaseInsensitiveSearch].location != NSNotFound && GPU.GPUVendorId == 0x1002)
				|| ([SelectedDevice.name rangeOfString : @"Intel" options : NSCaseInsensitiveSearch].location != NSNotFound && GPU.GPUVendorId == 0x8086))
			{
				NameComponents.Empty();
				bool bMatchesName = FString(GPU.GPUName).TrimStartAndEnd().ParseIntoArray(NameComponents, TEXT(" ")) > 0;
				for (FString& Component : NameComponents)
				{
					bMatchesName &= FString(SelectedDevice.name).Contains(Component);
				}
				if ((SelectedDevice.headless == GPU.GPUHeadless || GPU.GPUVendorId != 0x1002) && bMatchesName)
				{
					DeviceIndex = i;
					bFoundDefault = true;
					break;
				}
			}
		}
		if (!bFoundDefault)
		{
			UE_LOG(LogHMD, Warning, TEXT("Couldn't find Metal device %s in GPU descriptors from IORegistry - VR device selection may be wrong."), *FString(SelectedDevice.name));
		}
		return (uint64)DeviceIndex;
#else
		return 0;
#endif
	}

	virtual TSharedPtr< IHeadMountedDisplayVulkanExtensions, ESPMode::ThreadSafe > GetVulkanExtensions() override
	{
		FSteamVRHMD* SteamVRHMD = new FSteamVRHMD(this);
		if (!SteamVRHMD->IsInitialized())
		{
			delete SteamVRHMD;
			return nullptr;
		}
		return TSharedPtr< IHeadMountedDisplayVulkanExtensions, ESPMode::ThreadSafe >(SteamVRHMD);
	}

private:
	vr::IVRSystem* VRSystem;
	
	void* OpenVRDLLHandle;
#endif // STEAMVR_SUPPORTED_PLATFORMS
};

IMPLEMENT_MODULE( FSteamVRPlugin, SteamVR )

TSharedPtr< class IXRTrackingSystem, ESPMode::ThreadSafe > FSteamVRPlugin::CreateTrackingSystem()
{
#if STEAMVR_SUPPORTED_PLATFORMS
	TSharedPtr< FSteamVRHMD, ESPMode::ThreadSafe > SteamVRHMD( new FSteamVRHMD(this) );
	if( SteamVRHMD->IsInitialized() )
	{
		VRSystem = SteamVRHMD->GetVRSystem();
		return SteamVRHMD;
	}
#endif//STEAMVR_SUPPORTED_PLATFORMS
	return nullptr;
}


//---------------------------------------------------
// SteamVR IHeadMountedDisplay Implementation
//---------------------------------------------------

#if STEAMVR_SUPPORTED_PLATFORMS

pVRIsHmdPresent FSteamVRHMD::VRIsHmdPresentFn = nullptr;
pVRGetGenericInterface FSteamVRHMD::VRGetGenericInterfaceFn = nullptr;

bool FSteamVRHMD::GetVulkanInstanceExtensionsRequired(TArray<const ANSICHAR*>& Out)
{
	if (VRCompositor == nullptr)
	{
		UE_LOG(LogHMD, Warning, TEXT("VRCompositor is null"));
		return false;
	}
 
	static ANSICHAR* InstanceExtensionsBuf = nullptr;
 
	uint32_t BufSize = VRCompositor->GetVulkanInstanceExtensionsRequired(nullptr, 0);
	if (BufSize == 0)
	{
		return true; // No particular extensions required
	}
	if (InstanceExtensionsBuf != nullptr)
	{
		FMemory::Free(InstanceExtensionsBuf);
	}
	InstanceExtensionsBuf = (ANSICHAR*)FMemory::Malloc(BufSize);
	VRCompositor->GetVulkanInstanceExtensionsRequired(InstanceExtensionsBuf, BufSize);
 
	ANSICHAR * Context = nullptr;
	ANSICHAR * Tok = FCStringAnsi::Strtok(InstanceExtensionsBuf, " ", &Context);
	while (Tok != nullptr)
	{
		Out.Add(Tok);
		Tok = FCStringAnsi::Strtok(nullptr, " ", &Context);
	}

	return true;
}

bool FSteamVRHMD::GetVulkanDeviceExtensionsRequired(VkPhysicalDevice_T *pPhysicalDevice, TArray<const ANSICHAR*>& Out)
{
	if ( VRCompositor == nullptr )
	{
		UE_LOG(LogHMD, Warning, TEXT("VRCompositor is null"));
		return false;
	}
 
	static ANSICHAR* DeviceExtensionsBuf = nullptr;
 
	uint32_t BufSize = VRCompositor->GetVulkanDeviceExtensionsRequired(pPhysicalDevice, nullptr, 0);
	if (BufSize == 0)
	{
		return true; // No particular extensions required
	}
	if (DeviceExtensionsBuf != nullptr)
	{
		FMemory::Free(DeviceExtensionsBuf);
	}
	DeviceExtensionsBuf = (ANSICHAR*)FMemory::Malloc(BufSize);
	VRCompositor->GetVulkanDeviceExtensionsRequired(pPhysicalDevice, DeviceExtensionsBuf, BufSize);
 
	ANSICHAR * Context = nullptr;
	ANSICHAR * Tok = FCStringAnsi::Strtok(DeviceExtensionsBuf, " ", &Context);
	while (Tok != nullptr)
	{
		Out.Add(Tok);
		Tok = FCStringAnsi::Strtok(nullptr, " ", &Context);
	}

	return true;
}

bool FSteamVRHMD::IsHMDConnected()
{
	return FSteamVRHMD::VRIsHmdPresentFn ? (bool)(*FSteamVRHMD::VRIsHmdPresentFn)() : false;
}

bool FSteamVRHMD::IsHMDEnabled() const
{
	return bHmdEnabled;
}

EHMDWornState::Type FSteamVRHMD::GetHMDWornState()
{
	//HmdWornState is set in OnStartGameFrame's event loop
	return HmdWornState;
}

void FSteamVRHMD::EnableHMD(bool enable)
{
	bHmdEnabled = enable;

	if (!bHmdEnabled)
	{
		EnableStereo(false);
	}
}

EHMDDeviceType::Type FSteamVRHMD::GetHMDDeviceType() const
{
	return EHMDDeviceType::DT_SteamVR;
}

bool FSteamVRHMD::GetHMDMonitorInfo(MonitorInfo& MonitorDesc) 
{
	if (IsInitialized())
	{
		int32 X, Y;
		uint32 Width, Height;
		GetWindowBounds(&X, &Y, &Width, &Height);

		MonitorDesc.MonitorName = DisplayId;
		MonitorDesc.MonitorId	= 0;
		MonitorDesc.DesktopX	= X;
		MonitorDesc.DesktopY	= Y;
		MonitorDesc.ResolutionX = Width;
		MonitorDesc.ResolutionY = Height;

		return true;
	}
	else
	{
		MonitorDesc.MonitorName = "";
		MonitorDesc.MonitorId = 0;
		MonitorDesc.DesktopX = MonitorDesc.DesktopY = MonitorDesc.ResolutionX = MonitorDesc.ResolutionY = 0;
	}

	return false;
}

void FSteamVRHMD::GetFieldOfView(float& OutHFOVInDegrees, float& OutVFOVInDegrees) const
{
	OutHFOVInDegrees = 0.0f;
	OutVFOVInDegrees = 0.0f;
}

bool FSteamVRHMD::DoesSupportPositionalTracking() const
{
	return true;
}

bool FSteamVRHMD::HasValidTrackingPosition()
{
	return bHaveVisionTracking;
}

bool FSteamVRHMD::GetTrackingSensorProperties(int32 SensorId, FQuat& OutOrientation, FVector& OutOrigin, FXRSensorProperties& OutSensorProperties)
{
	OutOrigin = FVector::ZeroVector;
	OutOrientation = FQuat::Identity;
	OutSensorProperties = FXRSensorProperties();

	if (VRSystem == nullptr)
	{
		return false;
	}

	uint32 SteamDeviceID = static_cast<uint32>(SensorId);
	if (SteamDeviceID >= vr::k_unMaxTrackedDeviceCount)
	{
		return false;
	}
	
	const FTrackingFrame& TrackingFrame = GetTrackingFrame();

	if (!TrackingFrame.bPoseIsValid[SteamDeviceID])
	{
		return false;
	}

	OutOrigin = TrackingFrame.DevicePosition[SteamDeviceID];
	OutOrientation = TrackingFrame.DeviceOrientation[SteamDeviceID];

	OutSensorProperties.LeftFOV = VRSystem->GetFloatTrackedDeviceProperty(SteamDeviceID, vr::Prop_FieldOfViewLeftDegrees_Float);
	OutSensorProperties.RightFOV = VRSystem->GetFloatTrackedDeviceProperty(SteamDeviceID, vr::Prop_FieldOfViewRightDegrees_Float);
	OutSensorProperties.TopFOV = VRSystem->GetFloatTrackedDeviceProperty(SteamDeviceID, vr::Prop_FieldOfViewTopDegrees_Float);
	OutSensorProperties.BottomFOV = VRSystem->GetFloatTrackedDeviceProperty(SteamDeviceID, vr::Prop_FieldOfViewBottomDegrees_Float);

	const float WorldToMetersScale = TrackingFrame.WorldToMetersScale;

	OutSensorProperties.NearPlane = VRSystem->GetFloatTrackedDeviceProperty(SteamDeviceID, vr::Prop_TrackingRangeMinimumMeters_Float) * WorldToMetersScale;
	OutSensorProperties.FarPlane = VRSystem->GetFloatTrackedDeviceProperty(SteamDeviceID, vr::Prop_TrackingRangeMaximumMeters_Float) * WorldToMetersScale;

	OutSensorProperties.CameraDistance = FVector::Dist(FVector::ZeroVector, OutOrigin);
	return true;
}

void FSteamVRHMD::SetInterpupillaryDistance(float NewInterpupillaryDistance)
{
}

float FSteamVRHMD::GetInterpupillaryDistance() const
{
	return 0.064f;
}

bool FSteamVRHMD::GetCurrentPose(int32 DeviceId, FQuat& CurrentOrientation, FVector& CurrentPosition)
{
	uint32 SteamDeviceID = static_cast<uint32>(DeviceId);
	bool bHasValidPose = false;

	const FTrackingFrame& TrackingFrame = GetTrackingFrame();
	if (SteamDeviceID < vr::k_unMaxTrackedDeviceCount)
	{
		CurrentOrientation = TrackingFrame.DeviceOrientation[SteamDeviceID];
		CurrentPosition = TrackingFrame.DevicePosition[SteamDeviceID];

		bHasValidPose = TrackingFrame.bPoseIsValid[SteamDeviceID] && TrackingFrame.bDeviceIsConnected[SteamDeviceID];
	}
	else
	{
		CurrentOrientation = FQuat::Identity;
		CurrentPosition = FVector::ZeroVector;
	}

	return bHasValidPose;
}

void FSteamVRHMD::RefreshPoses()
{
	if (VRSystem == nullptr)
	{
		return;
	}

	FTrackingFrame& TrackingFrame = const_cast<FTrackingFrame&>(GetTrackingFrame());
		TrackingFrame.FrameNumber = GFrameNumberRenderThread;

		vr::TrackedDevicePose_t Poses[vr::k_unMaxTrackedDeviceCount];
	if (IsInRenderingThread())
		{
			vr::EVRCompositorError PoseError = VRCompositor->WaitGetPoses(Poses, ARRAYSIZE(Poses) , NULL, 0);
		}
		else
		{
		check(IsInGameThread());
			VRSystem->GetDeviceToAbsoluteTrackingPose(VRCompositor->GetTrackingSpace(), 0.0f, Poses, ARRAYSIZE(Poses));
		}

		bHaveVisionTracking = false;
	TrackingFrame.WorldToMetersScale = GameWorldToMetersScale;
		for (uint32 i = 0; i < vr::k_unMaxTrackedDeviceCount; ++i)
		{
		bHaveVisionTracking |= Poses[i].eTrackingResult == vr::ETrackingResult::TrackingResult_Running_OK;

			TrackingFrame.bDeviceIsConnected[i] = Poses[i].bDeviceIsConnected;
			TrackingFrame.bPoseIsValid[i] = Poses[i].bPoseIsValid;
		TrackingFrame.RawPoses[i] = Poses[i].mDeviceToAbsoluteTracking;

	}
	ConvertRawPoses(TrackingFrame);
	
}

void FSteamVRHMD::GetWindowBounds(int32* X, int32* Y, uint32* Width, uint32* Height)
{
	if (vr::IVRExtendedDisplay *VRExtDisplay = vr::VRExtendedDisplay())
	{
		VRExtDisplay->GetWindowBounds(X, Y, Width, Height);
	}
	else
	{
		*X = 0;
		*Y = 0;
		*Width = WindowMirrorBoundsWidth;
		*Height = WindowMirrorBoundsHeight;
	}
}

bool FSteamVRHMD::IsInsideBounds()
{
	if (VRChaperone)
	{
		const FTrackingFrame& TrackingFrame = GetTrackingFrame();
		vr::HmdMatrix34_t VRPose = TrackingFrame.RawPoses[vr::k_unTrackedDeviceIndex_Hmd];
		FMatrix Pose = ToFMatrix(VRPose);
		
		const FVector HMDLocation(Pose.M[3][0], 0.f, Pose.M[3][2]);

		bool bLastWasNegative = false;

		// Since the order of the soft bounds are points on a plane going clockwise, wind around the sides, checking the crossproduct of the affine side to the affine HMD position.  If they're all on the same side, we're in the bounds
		for (uint8 i = 0; i < 4; ++i)
		{
			const FVector PointA = ChaperoneBounds.Bounds.Corners[i];
			const FVector PointB = ChaperoneBounds.Bounds.Corners[(i + 1) % 4];

			const FVector AffineSegment = PointB - PointA;
			const FVector AffinePoint = HMDLocation - PointA;
			const FVector CrossProduct = FVector::CrossProduct(AffineSegment, AffinePoint);

			const bool bIsNegative = (CrossProduct.Y < 0);

			// If the cross between the point and the side has flipped, that means we're not consistent, and therefore outside the bounds
			if ((i > 0) && (bLastWasNegative != bIsNegative))
			{
				return false;
			}

			bLastWasNegative = bIsNegative;
		}

		return true;
	}

	return false;
}

/** Helper function to convert bounds from SteamVR space to scaled Unreal space*/
TArray<FVector> ConvertBoundsToUnrealSpace(const FBoundingQuad& InBounds, const float WorldToMetersScale)
{
	TArray<FVector> Bounds;

	for (int32 i = 0; i < ARRAYSIZE(InBounds.Corners); ++i)
	{
		const FVector SteamVRCorner = InBounds.Corners[i];
		const FVector UnrealVRCorner(-SteamVRCorner.Z, SteamVRCorner.X, SteamVRCorner.Y);
		Bounds.Add(UnrealVRCorner * WorldToMetersScale);
	}

	return Bounds;
}

TArray<FVector> FSteamVRHMD::GetBounds() const
{
	return ConvertBoundsToUnrealSpace(ChaperoneBounds.Bounds, GetWorldToMetersScale());
}

void FSteamVRHMD::SetTrackingOrigin(EHMDTrackingOrigin::Type NewOrigin)
{
	if(VRCompositor)
	{
		vr::TrackingUniverseOrigin NewSteamOrigin;

		switch (NewOrigin)
		{
			case EHMDTrackingOrigin::Eye:
				NewSteamOrigin = vr::TrackingUniverseOrigin::TrackingUniverseSeated;
				break;
			case EHMDTrackingOrigin::Floor:
			default:
				NewSteamOrigin = vr::TrackingUniverseOrigin::TrackingUniverseStanding;
				break;
		}

		VRCompositor->SetTrackingSpace(NewSteamOrigin);
	}
}

EHMDTrackingOrigin::Type FSteamVRHMD::GetTrackingOrigin()
{
	if(VRCompositor)
	{
		const vr::TrackingUniverseOrigin CurrentOrigin = VRCompositor->GetTrackingSpace();

		switch(CurrentOrigin)
		{
		case vr::TrackingUniverseOrigin::TrackingUniverseSeated:
			return EHMDTrackingOrigin::Eye;
		case vr::TrackingUniverseOrigin::TrackingUniverseStanding:
		default:
			return EHMDTrackingOrigin::Floor;
		}
	}

	// By default, assume standing
	return EHMDTrackingOrigin::Floor;
}


void FSteamVRHMD::RecordAnalytics()
{
	if (FEngineAnalytics::IsAvailable())
	{
		// prepare and send analytics data
		TArray<FAnalyticsEventAttribute> EventAttributes;

		IHeadMountedDisplay::MonitorInfo MonitorInfo;
		GetHMDMonitorInfo(MonitorInfo);

		uint64 MonitorId = MonitorInfo.MonitorId;

		char Buf[128];
		vr::TrackedPropertyError Error;
		FString DeviceName = "SteamVR - Default Device Name";
		VRSystem->GetStringTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_ModelNumber_String, Buf, sizeof(Buf), &Error);
		if (Error == vr::TrackedProp_Success)
		{
			DeviceName = FString(UTF8_TO_TCHAR(Buf));
		}
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("DeviceName"), DeviceName));
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("DisplayDeviceName"), *MonitorInfo.MonitorName));
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("DisplayId"), MonitorId));
		FString MonResolution(FString::Printf(TEXT("(%d, %d)"), MonitorInfo.ResolutionX, MonitorInfo.ResolutionY));
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("Resolution"), MonResolution));

		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("InterpupillaryDistance"), GetInterpupillaryDistance()));


		FString OutStr(TEXT("Editor.VR.DeviceInitialised"));
		FEngineAnalytics::GetProvider().RecordEvent(OutStr, EventAttributes);
	}
}

#if PLATFORM_MAC
class FIOSurfaceResourceWrapper : public FResourceBulkDataInterface
{
public:
	FIOSurfaceResourceWrapper(CFTypeRef InSurface)
	: Surface(InSurface)
	{
		check(InSurface);
		CFRetain(Surface);
	}

	virtual const void* GetResourceBulkData() const override
	{
		return Surface;
	}
	
	virtual uint32 GetResourceBulkDataSize() const override
	{
		return 0;
	}
	
	virtual void Discard() override
	{
		delete this;
	}
	
	virtual EBulkDataType GetResourceType() const override
	{
		return EBulkDataType::VREyeBuffer;
	}
	
	virtual ~FIOSurfaceResourceWrapper()
	{
		CFRelease(Surface);
		Surface = nullptr;
	}

private:
	CFTypeRef Surface;
};
#endif

bool FSteamVRHMD::AllocateRenderTargetTexture(uint32 Index, uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, uint32 InTexFlags, uint32 InTargetableTextureFlags, FTexture2DRHIRef& OutTargetableTexture, FTexture2DRHIRef& OutShaderResourceTexture, uint32 NumSamples)
{
	if (!IsStereoEnabled())
	{
		return false;
	}
	
#if PLATFORM_MAC
	const uint32 SwapChainLength = 3;
	
	MetalBridge* MetalBridgePtr = (MetalBridge*)pBridge.GetReference();
	
	MetalBridgePtr->TextureSet = new FRHITextureSet2D(SwapChainLength, PF_B8G8R8A8, SizeX, SizeY, 1, NumSamples, InTexFlags, FClearValueBinding(FLinearColor::Transparent));
	
	for (uint32 SwapChainIter = 0; SwapChainIter < SwapChainLength; ++SwapChainIter)
	{
		IOSurfaceRef Surface = MetalBridgePtr->GetSurface(SizeX, SizeY);
		check(Surface != nil);
		
		FRHIResourceCreateInfo CreateInfo;
		CreateInfo.BulkData = new FIOSurfaceResourceWrapper(Surface);
		CFRelease(Surface);
		CreateInfo.ResourceArray = nullptr;
		
		FTexture2DRHIRef TargetableTexture, ShaderResourceTexture;
		RHICreateTargetableShaderResource2D(SizeX, SizeY, PF_B8G8R8A8, 1, TexCreate_None, TexCreate_RenderTargetable, false, CreateInfo, TargetableTexture, ShaderResourceTexture, NumSamples);
		check(TargetableTexture == ShaderResourceTexture);
		static_cast<FRHITextureSet2D*>(MetalBridgePtr->TextureSet.GetReference())->AddTexture(TargetableTexture, SwapChainIter);
	}
	
	OutTargetableTexture = OutShaderResourceTexture = MetalBridgePtr->TextureSet;

	return true;
#else
	FRHIResourceCreateInfo CreateInfo;
	RHICreateTargetableShaderResource2D(SizeX, SizeY, PF_B8G8R8A8, 1, TexCreate_None, TexCreate_RenderTargetable, false, CreateInfo, OutTargetableTexture, OutShaderResourceTexture);

	return true;
#endif
}

void FSteamVRHMD::PoseToOrientationAndPosition(const vr::HmdMatrix34_t& InPose, const float WorldToMetersScale, FQuat& OutOrientation, FVector& OutPosition) const
{
	FMatrix Pose = ToFMatrix(InPose);
	if (!((FMath::Abs(1.f - Pose.GetScaledAxis(EAxis::X).SizeSquared()) <= KINDA_SMALL_NUMBER) && (FMath::Abs(1.f - Pose.GetScaledAxis(EAxis::Y).SizeSquared()) <= KINDA_SMALL_NUMBER) && (FMath::Abs(1.f - Pose.GetScaledAxis(EAxis::Z).SizeSquared()) <= KINDA_SMALL_NUMBER)))
	{
		// When running an oculus rift through steamvr the tracking reference seems to have a slightly scaled matrix, about 99%.  We need to strip that so we can build the quaternion without hitting an ensure.
		Pose.RemoveScaling(KINDA_SMALL_NUMBER);
	}
	FQuat Orientation(Pose);

	OutOrientation.X = -Orientation.Z;
	OutOrientation.Y = Orientation.X;
	OutOrientation.Z = Orientation.Y;
	OutOrientation.W = -Orientation.W;

	FVector Position = ((FVector(-Pose.M[3][2], Pose.M[3][0], Pose.M[3][1]) - BaseOffset) * WorldToMetersScale);
	OutPosition = BaseOrientation.Inverse().RotateVector(Position);

	OutOrientation = BaseOrientation.Inverse() * OutOrientation;
	OutOrientation.Normalize();
}

void FSteamVRHMD::ConvertRawPoses(FSteamVRHMD::FTrackingFrame& TrackingFrame) const
{
	for (uint32 i = 0; i < vr::k_unMaxTrackedDeviceCount; ++i)
	{
		#if 0
		FMatrix Pose = ToFMatrix(TrackingFrame.RawPoses[i]);
		if (!((FMath::Abs(1.f - Pose.GetScaledAxis(EAxis::X).SizeSquared()) <= KINDA_SMALL_NUMBER) && (FMath::Abs(1.f - Pose.GetScaledAxis(EAxis::Y).SizeSquared()) <= KINDA_SMALL_NUMBER) && (FMath::Abs(1.f - Pose.GetScaledAxis(EAxis::Z).SizeSquared()) <= KINDA_SMALL_NUMBER)))
		{
			// When running an oculus rift through steamvr the tracking reference seems to have a slightly scaled matrix, about 99%.  We need to strip that so we can build the quaternion without hitting an ensure.
			Pose.RemoveScaling(KINDA_SMALL_NUMBER);
		}
		FQuat Orientation(Pose);
		Orientation = FQuat(-Orientation.Z, Orientation.X, Orientation.Y, -Orientation.W);

		FVector Position = ((FVector(-Pose.M[3][2], Pose.M[3][0], Pose.M[3][1]) - BaseOffset) * TrackingFrame.WorldToMetersScale);
		TrackingFrame.DevicePosition[i] = BaseOrientation.Inverse().RotateVector(Position);

		TrackingFrame.DeviceOrientation[i] = BaseOrientation.Inverse() * Orientation;
		TrackingFrame.DeviceOrientation[i].Normalize();
#endif
		PoseToOrientationAndPosition(TrackingFrame.RawPoses[i], TrackingFrame.WorldToMetersScale, TrackingFrame.DeviceOrientation[i], TrackingFrame.DevicePosition[i]);
	}
}

float FSteamVRHMD::GetWorldToMetersScale() const
{
	const FTrackingFrame& TrackingFrame = GetTrackingFrame();
	return TrackingFrame.bPoseIsValid[vr::k_unTrackedDeviceIndex_Hmd] ? TrackingFrame.WorldToMetersScale : 100.0f;
}

EXRTrackedDeviceType FSteamVRHMD::GetTrackedDeviceType(int32 DeviceId) const
{
	check(VRSystem != nullptr);
	vr::TrackedDeviceClass DeviceClass = VRSystem->GetTrackedDeviceClass(DeviceId);

	switch (DeviceClass)
	{
	case vr::TrackedDeviceClass_HMD:
		return EXRTrackedDeviceType::HeadMountedDisplay;
	case vr::TrackedDeviceClass_Controller:
		return EXRTrackedDeviceType::Controller;
	case vr::TrackedDeviceClass_TrackingReference:
		return EXRTrackedDeviceType::TrackingReference;
	case vr::TrackedDeviceClass_GenericTracker:
		return EXRTrackedDeviceType::Other;
	default:
		return EXRTrackedDeviceType::Invalid;
	}
}


bool FSteamVRHMD::EnumerateTrackedDevices(TArray<int32>& TrackedIds, EXRTrackedDeviceType DeviceType)
{
	TrackedIds.Empty();
	if (VRSystem == nullptr)
	{
		return false;
	}

	const FTrackingFrame& TrackingFrame = GetTrackingFrame();
	for (uint32 i = 0; i < vr::k_unMaxTrackedDeviceCount; ++i)
	{
		// Add only devices with a currently valid tracked pose
		if (TrackingFrame.bPoseIsValid[i] &&
			(DeviceType == EXRTrackedDeviceType::Any || GetTrackedDeviceType(i) == DeviceType))
		{
			TrackedIds.Add(i);
		}
	}
	return TrackedIds.Num() > 0;
}

ETrackingStatus FSteamVRHMD::GetControllerTrackingStatus(int32 DeviceId) const
{
	ETrackingStatus TrackingStatus = ETrackingStatus::NotTracked;

	const FTrackingFrame& TrackingFrame = GetTrackingFrame();
	if (DeviceId < vr::k_unMaxTrackedDeviceCount && TrackingFrame.bPoseIsValid[DeviceId] && TrackingFrame.bDeviceIsConnected[DeviceId])
	{
		TrackingStatus = ETrackingStatus::Tracked;
	}

	return TrackingStatus;
}

bool FSteamVRHMD::IsTracking(int32 DeviceId)
{
	uint32 SteamDeviceId = static_cast<uint32>(DeviceId);
	bool bHasTrackedPose = false;
	if (VRSystem != nullptr)
	{
		const FTrackingFrame& TrackingFrame = GetTrackingFrame();
		if (SteamDeviceId < vr::k_unMaxTrackedDeviceCount)
		{
			bHasTrackedPose = TrackingFrame.bPoseIsValid[SteamDeviceId];
		}
	}
	return bHasTrackedPose;
}

bool FSteamVRHMD::IsChromaAbCorrectionEnabled() const
{
	return true;
}

void FSteamVRHMD::OnEndPlay(FWorldContext& InWorldContext)
{
	if (!GEnableVREditorHacks)
	{
		EnableStereo(false);
	}
}

FString FSteamVRHMD::GetVersionString() const
{
	if (VRSystem == nullptr)
	{
		return FString();
	}

	const FString Manufacturer = GetFStringTrackedDeviceProperty(VRSystem, vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_ManufacturerName_String);
	const FString Model = GetFStringTrackedDeviceProperty(VRSystem, vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_ModelNumber_String);
	const FString Serial = GetFStringTrackedDeviceProperty(VRSystem, vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_SerialNumber_String);
	const FString DriverId = GetFStringTrackedDeviceProperty(VRSystem, vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_TrackingSystemName_String);
	const FString DriverVersion = GetFStringTrackedDeviceProperty(VRSystem, vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_DriverVersion_String);

	return FString::Printf(TEXT("%s, Driver: %s, Serial: %s, HMD Device: %s %s, Driver version: %s"), *FEngineVersion::Current().ToString(), *DriverId, *Serial, *Manufacturer, *Model, *DriverVersion);
}

bool FSteamVRHMD::OnStartGameFrame(FWorldContext& WorldContext)
{
	static const double kShutdownTimeout = 4.0; // How many seconds to allow the renderer to exit stereo mode before shutting down the VR subsystem
	
	if (VRSystem == nullptr)
	{
		return false;
	}

	if (bStereoEnabled != bStereoDesired)
	{
		bStereoEnabled = EnableStereo(bStereoDesired);
	}

	FQuat Orientation;
	FVector Position;
	GameWorldToMetersScale = WorldContext.World()->GetWorldSettings()->WorldToMeters;
	RefreshPoses();
	GetCurrentPose(IXRTrackingSystem::HMDDeviceId, Orientation, Position);

	bool bShouldShutdown = false;
	if (bIsQuitting)
	{
		if (QuitTimestamp < FApp::GetCurrentTime())
		{
			bShouldShutdown = true;
			bIsQuitting = false;
		}
	}

	// We must be sure the rendertargetsize is calculated already
	if (FrameSettings.bNeedsUpdate)
	{
		UpdateStereoRenderingParams();
	}
	// And then transfer the settings for this frame to the render thread
	{
		FScopeLock Lock(&FrameSettingsLock);
		pBridge->UpdateFrameSettings(FrameSettings);
	}

	// Poll SteamVR events
	vr::VREvent_t VREvent;
	while (VRSystem->PollNextEvent(&VREvent, sizeof(VREvent)))
	{
		switch (VREvent.eventType)
		{
		case vr::VREvent_Quit:
			if (IsStereoEnabled())
			{
				// If we're currently in stereo mode, allow a few seconds while we disable stereo rendering before shutting down the VR system
				EnableStereo(false);
				QuitTimestamp = FApp::GetCurrentTime() + kShutdownTimeout;
				bIsQuitting = true;
			}
			else if (!bIsQuitting)
			{
				// If we're not currently in stereo mode (and not already counting down, we can shut down the VR system immediately
				bShouldShutdown = true;
			}
			break;
		case vr::VREvent_InputFocusCaptured:
			FCoreDelegates::ApplicationWillEnterBackgroundDelegate.Broadcast();
			break;
		case vr::VREvent_InputFocusReleased:
			FCoreDelegates::ApplicationHasEnteredForegroundDelegate.Broadcast();
			break;
		case vr::VREvent_TrackedDeviceUserInteractionStarted:
			// if the event was sent by the HMD
			if (VREvent.trackedDeviceIndex == vr::k_unTrackedDeviceIndex_Hmd)
			{
				// Save the position we are currently at and put us in the state where we could move to a worn state
				bShouldCheckHMDPosition = true;
				HMDStartLocation = Position;
			}
			break;
		case vr::VREvent_TrackedDeviceUserInteractionEnded:
			// if the event was sent by the HMD. 
			if (VREvent.trackedDeviceIndex == vr::k_unTrackedDeviceIndex_Hmd) 
			{
				// Don't check to see if we might be wearing the HMD anymore.
				bShouldCheckHMDPosition = false;
				// Don't change our state to "not worn" unless we are currently wearing it.
				if (HmdWornState == EHMDWornState::Worn)
				{
					HmdWornState = EHMDWornState::NotWorn;
					FCoreDelegates::VRHeadsetRemovedFromHead.Broadcast();
				}
			}
			break;
		case vr::VREvent_ChaperoneDataHasChanged:
		case vr::VREvent_ChaperoneUniverseHasChanged:
		case vr::VREvent_ChaperoneTempDataHasChanged:
		case vr::VREvent_ChaperoneSettingsHaveChanged:
			// if the event was sent by the HMD. 
			if ((VREvent.trackedDeviceIndex == vr::k_unTrackedDeviceIndex_Hmd) && (VRChaperone != nullptr))
			{
				ChaperoneBounds = FChaperoneBounds(VRChaperone);
			}
			break;
		}
	}


	// SteamVR gives 5 seconds from VREvent_Quit till its process is killed
	if (bShouldShutdown)
	{
		bShouldCheckHMDPosition = false;
		Shutdown();

#if WITH_EDITOR
		if (GIsEditor)
		{
			FSceneViewport* SceneVP = FindSceneViewport();
			if (SceneVP && SceneVP->IsStereoRenderingAllowed())
			{
				TSharedPtr<SWindow> Window = SceneVP->FindWindow();
				Window->RequestDestroyWindow();
			}
		}
		else
#endif//WITH_EDITOR
		{
			// ApplicationWillTerminateDelegate will fire from inside of the RequestExit
			FPlatformMisc::RequestExit(false);
		}
	}

	// If the HMD is being interacted with, but we haven't decided the HMD is worn yet.  
	if (bShouldCheckHMDPosition)
	{
		if (FVector::Dist(HMDStartLocation, Position) > HMDWornMovementThreshold)
		{
			HmdWornState = EHMDWornState::Worn;
			FCoreDelegates::VRHeadsetPutOnHead.Broadcast();
			bShouldCheckHMDPosition = false;
		}
	}


	return true;
}

void FSteamVRHMD::ResetOrientationAndPosition(float yaw)
{
	ResetOrientation(yaw);
	ResetPosition();
}

void FSteamVRHMD::ResetOrientation(float Yaw)
{
	const FTrackingFrame& TrackingFrame = GetTrackingFrame();

	FRotator ViewRotation;
	ViewRotation = FRotator(TrackingFrame.DeviceOrientation[vr::k_unTrackedDeviceIndex_Hmd]);
	ViewRotation.Pitch = 0;
	ViewRotation.Roll = 0;
	ViewRotation.Yaw += BaseOrientation.Rotator().Yaw;

	if (Yaw != 0.f)
	{
		// apply optional yaw offset
		ViewRotation.Yaw -= Yaw;
		ViewRotation.Normalize();
	}

	BaseOrientation = ViewRotation.Quaternion();
}
void FSteamVRHMD::ResetPosition()
{
	const FTrackingFrame& TrackingFrame = GetTrackingFrame();
	FMatrix Pose = ToFMatrix(TrackingFrame.RawPoses[vr::k_unTrackedDeviceIndex_Hmd]);
	BaseOffset = FVector(-Pose.M[3][2], Pose.M[3][0], Pose.M[3][1]);
}

void FSteamVRHMD::SetBaseRotation(const FRotator& BaseRot)
{
	BaseOrientation = BaseRot.Quaternion();
}
FRotator FSteamVRHMD::GetBaseRotation() const
{
	return FRotator::ZeroRotator;
}

void FSteamVRHMD::SetBaseOrientation(const FQuat& BaseOrient)
{
	BaseOrientation = BaseOrient;
}

FQuat FSteamVRHMD::GetBaseOrientation() const
{
	return BaseOrientation;
}

bool FSteamVRHMD::IsStereoEnabled() const
{
	return VRSystem && bStereoEnabled && bHmdEnabled;
}

bool FSteamVRHMD::EnableStereo(bool bStereo)
{
	if( bStereoEnabled == bStereo )
	{
		return false;
	}

	if (bStereo && bIsQuitting)
	{
		// Cancel shutting down the vr subsystem if re-enabling stereo before we're done counting down
		bIsQuitting = false;
	}

	if (VRSystem == nullptr && (!bStereo || !Startup()))
	{
		return false;
	}

	bStereoDesired = (IsHMDEnabled()) ? bStereo : false;

	// Set the viewport to match that of the HMD display
	FSceneViewport* SceneVP = FindSceneViewport();
	if (SceneVP)
	{
		TSharedPtr<SWindow> Window = SceneVP->FindWindow();
		if (Window.IsValid() && SceneVP->GetViewportWidget().IsValid())
		{
			int32 ResX = 2160;
			int32 ResY = 1200;

			MonitorInfo MonitorDesc;
			if (GetHMDMonitorInfo(MonitorDesc))
			{
				ResX = MonitorDesc.ResolutionX;
				ResY = MonitorDesc.ResolutionY;
			}
			FSystemResolution::RequestResolutionChange(ResX, ResY, EWindowMode::WindowedFullscreen);

			if( bStereo )
			{
				int32 PosX, PosY;
				uint32 Width, Height;
				GetWindowBounds( &PosX, &PosY, &Width, &Height );
				SceneVP->SetViewportSize( Width, Height );
				bStereoEnabled = bStereoDesired;
			}
			else
			{
				// Note: Setting before resize to ensure we don't try to allocate a new vr rt.
				bStereoEnabled = bStereoDesired;

				FRHIViewport* const ViewportRHI = SceneVP->GetViewportRHI();
				if (ViewportRHI != nullptr)
				{
					ViewportRHI->SetCustomPresent(nullptr);
				}

				FVector2D size = SceneVP->FindWindow()->GetSizeInScreen();
				SceneVP->SetViewportSize( size.X, size.Y );
				Window->SetViewportSizeDrivenByWindow( true );
			}
		}
	}

	// Uncap fps to enable FPS higher than 62
	GEngine->bForceDisableFrameRateSmoothing = bStereoEnabled;
	
	return bStereoEnabled;
}

void FSteamVRHMD::AdjustViewRect(EStereoscopicPass StereoPass, int32& X, int32& Y, uint32& SizeX, uint32& SizeY) const
{
	//@todo steamvr: get the actual rects from steamvr
	SizeX = SizeX / 2;
	if( StereoPass == eSSP_RIGHT_EYE )
	{
		X += SizeX;
	}
}

bool FSteamVRHMD::GetRelativeEyePose(int32 DeviceId, EStereoscopicPass Eye, FQuat& OutOrientation, FVector& OutPosition)
{
	if (DeviceId != IXRTrackingSystem::HMDDeviceId || !(Eye == eSSP_LEFT_EYE || Eye == eSSP_RIGHT_EYE))
	{
		return false;
	}
	auto Frame = GetTrackingFrame();

	vr::Hmd_Eye HmdEye = (Eye == eSSP_LEFT_EYE) ? vr::Eye_Left : vr::Eye_Right;
		vr::HmdMatrix34_t HeadFromEye = VRSystem->GetEyeToHeadTransform(HmdEye);

		// grab the eye position, currently ignoring the rotation supplied by GetHeadFromEyePose()
	OutPosition = FVector(-HeadFromEye.m[2][3], HeadFromEye.m[0][3], HeadFromEye.m[1][3]) * Frame.WorldToMetersScale;
		FQuat Orientation(ToFMatrix(HeadFromEye));

	OutOrientation.X = -Orientation.Z;
	OutOrientation.Y = Orientation.X;
	OutOrientation.Z = Orientation.Y;
	OutOrientation.W = -Orientation.W;

	return true;
}

void FSteamVRHMD::CalculateStereoViewOffset(const enum EStereoscopicPass StereoPassType, FRotator& ViewRotation, const float WorldToMeters, FVector& ViewLocation)
{
	// Needed to transform world locked stereo layers
	PlayerLocation = ViewLocation;

	// Forward to the base implementation (that in turn will call the DefaultXRCamera implementation)
	FHeadMountedDisplayBase::CalculateStereoViewOffset(StereoPassType, ViewRotation, WorldToMeters, ViewLocation);
}

FMatrix FSteamVRHMD::GetStereoProjectionMatrix(const enum EStereoscopicPass StereoPassType) const
{
	check(IsStereoEnabled());

	vr::Hmd_Eye HmdEye = (StereoPassType == eSSP_LEFT_EYE) ? vr::Eye_Left : vr::Eye_Right;
	float Left, Right, Top, Bottom;

	VRSystem->GetProjectionRaw(HmdEye, &Right, &Left, &Top, &Bottom);
	Bottom *= -1.0f;
	Top *= -1.0f;
	Right *= -1.0f;
	Left *= -1.0f;

	float ZNear = GNearClippingPlane;

	float SumRL = (Right + Left);
	float SumTB = (Top + Bottom);
	float InvRL = (1.0f / (Right - Left));
	float InvTB = (1.0f / (Top - Bottom));

#if 1
	FMatrix Mat = FMatrix(
		FPlane((2.0f * InvRL), 0.0f, 0.0f, 0.0f),
		FPlane(0.0f, (2.0f * InvTB), 0.0f, 0.0f),
		FPlane((SumRL * InvRL), (SumTB * InvTB), 0.0f, 1.0f),
		FPlane(0.0f, 0.0f, ZNear, 0.0f)
		);
#else
	vr::HmdMatrix44_t SteamMat = VRSystem->GetProjectionMatrix(HmdEye, ZNear, 10000.0f, vr::TextureType_DirectX);
	FMatrix Mat = ToFMatrix(SteamMat);

	Mat.M[3][3] = 0.0f;
	Mat.M[2][3] = 1.0f;
	Mat.M[2][2] = 0.0f;
	Mat.M[3][2] = ZNear;
#endif

	return Mat;

}

void FSteamVRHMD::GetOrthoProjection(int32 RTWidth, int32 RTHeight, float OrthoDistance, FMatrix OrthoProjection[2]) const
{
	const float HudOffset = 50.0f;
	OrthoProjection[0] = FTranslationMatrix(FVector(HudOffset, 0.f, 0.f));
	OrthoProjection[1] = FTranslationMatrix(FVector(-HudOffset + RTWidth * .5, 0.f, 0.f));
}

void FSteamVRHMD::GetEyeRenderParams_RenderThread(const FRenderingCompositePassContext& Context, FVector2D& EyeToSrcUVScaleValue, FVector2D& EyeToSrcUVOffsetValue) const
{
	if (Context.View.StereoPass == eSSP_LEFT_EYE)
	{
		EyeToSrcUVOffsetValue.X = 0.0f;
		EyeToSrcUVOffsetValue.Y = 0.0f;

		EyeToSrcUVScaleValue.X = 0.5f;
		EyeToSrcUVScaleValue.Y = 1.0f;
	}
	else
	{
		EyeToSrcUVOffsetValue.X = 0.5f;
		EyeToSrcUVOffsetValue.Y = 0.0f;

		EyeToSrcUVScaleValue.X = 0.5f;
		EyeToSrcUVScaleValue.Y = 1.0f;
	}
}

bool FSteamVRHMD::GetHMDDistortionEnabled() const
{
	return false;
}

void FSteamVRHMD::BeginRendering_GameThread()
{
	check(IsInGameThread());
	SpectatorScreenController->BeginRenderViewFamily();
}

void FSteamVRHMD::BeginRendering_RenderThread(const FTransform& NewRelativeTransform, FRHICommandListImmediate& RHICmdList, FSceneViewFamily& ViewFamily)
{
	check(IsInRenderingThread());
	FHeadMountedDisplayBase::BeginRendering_RenderThread(NewRelativeTransform, RHICmdList, ViewFamily);
	GetActiveRHIBridgeImpl()->BeginRendering();

	check(SpectatorScreenController);
	SpectatorScreenController->UpdateSpectatorScreenMode_RenderThread();

	// Update PlayerOrientation used by StereoLayers positioning
	const FSceneView* MainView = ViewFamily.Views[0];
	const FQuat ViewOrientation = MainView->ViewRotation.Quaternion();
	PlayerOrientation = ViewOrientation * MainView->BaseHmdOrientation.Inverse();
}


void FSteamVRHMD::UpdateViewportRHIBridge(bool /* bUseSeparateRenderTarget */, const class FViewport& Viewport, FRHIViewport* const ViewportRHI)
{
	check(IsInGameThread());

	GetActiveRHIBridgeImpl()->UpdateViewport(Viewport, ViewportRHI);
	GetActiveRHIBridgeImpl()->IncrementFrameNumber();
}

void FSteamVRHMD::DrawDebug(class UCanvas* Canvas, class APlayerController*)
{
	if(CShowDebug.GetValueOnGameThread())
	{
		if (Canvas == nullptr)
		{
			return;
		}
		
		static const FColor TextColor(0,255,0);
		// Pick a larger font on console.
		UFont* const Font = FPlatformProperties::SupportsWindowedMode() ? GEngine->GetSmallFont() : GEngine->GetMediumFont();
		const int32 RowHeight = FMath::TruncToInt(Font->GetMaxCharHeight() * 1.1f);
		
		float ClipX = Canvas->ClipX;
		float ClipY = Canvas->ClipY;
		float LeftPos = 0;
		
		ClipX -= 100;
		LeftPos = ClipX * 0.3f;
		float TopPos = ClipY * 0.4f;
		
		int32 X = (int32)LeftPos;
		int32 Y = (int32)TopPos;
		
		FString Str;
		Str = FString::Printf(TEXT("PD: %.2f"), FrameSettings.CurrentPixelDensity);
		Canvas->Canvas->DrawShadowedString(X, Y, *Str, Font, TextColor);
		
		Y += RowHeight;
		
		static const auto PostProcessAAQualityCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.PostProcessAAQuality"));
		Str = FString::Printf(TEXT("AA: %d"), PostProcessAAQualityCVar->GetInt());
		Canvas->Canvas->DrawShadowedString(X, Y, *Str, Font, TextColor);
		
	}
}

FSteamVRHMD::BridgeBaseImpl* FSteamVRHMD::GetActiveRHIBridgeImpl()
{
	return pBridge;
}

void FSteamVRHMD::CalculateRenderTargetSize(const class FViewport& Viewport, uint32& InOutSizeX, uint32& InOutSizeY)
{
	check(IsInGameThread());

	if (!IsStereoEnabled())
	{
		return;
	}
	
	InOutSizeX = FrameSettings.RenderTargetSize.X;
	InOutSizeY = FrameSettings.RenderTargetSize.Y;

	check(InOutSizeX != 0 && InOutSizeY != 0);
}

bool FSteamVRHMD::NeedReAllocateViewportRenderTarget(const FViewport& Viewport)
{
	check(IsInGameThread());

	if (IsStereoEnabled())
	{
		const uint32 InSizeX = Viewport.GetSizeXY().X;
		const uint32 InSizeY = Viewport.GetSizeXY().Y;
		const FIntPoint RenderTargetSize = Viewport.GetRenderTargetTextureSizeXY();

		uint32 NewSizeX = InSizeX, NewSizeY = InSizeY;
		CalculateRenderTargetSize(Viewport, NewSizeX, NewSizeY);
		if (NewSizeX != RenderTargetSize.X || NewSizeY != RenderTargetSize.Y)
		{
			return true;
		}
	}
	return false;
}

FSteamVRHMD::FSteamVRHMD(ISteamVRPlugin* InSteamVRPlugin) :
	CUseAdaptivePD(TEXT("vr.SteamVR.PixelDensityAdaptive"), TEXT("SteamVR adaptive pixel density support.  0 to disable, 1 to enable"), FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateRaw(this, &FSteamVRHMD::AdaptivePixelDensityCommandHandler)),
	bHmdEnabled(true),
	HmdWornState(EHMDWornState::Unknown),
	bStereoDesired(false),
	bStereoEnabled(false),
	bHaveVisionTracking(false),
	WindowMirrorBoundsWidth(2160),
	WindowMirrorBoundsHeight(1200),
	HMDWornMovementThreshold(50.0f),
	HMDStartLocation(FVector::ZeroVector),
	BaseOrientation(FQuat::Identity),
	BaseOffset(FVector::ZeroVector),
	bIsQuitting(false),
	QuitTimestamp(),
	bShouldCheckHMDPosition(false),
	RendererModule(nullptr),
	SteamVRPlugin(InSteamVRPlugin),
	VRSystem(nullptr),
	VRCompositor(nullptr),
	VROverlay(nullptr),
	VRChaperone(nullptr)
{
	if (Startup())
	{
		SetupOcclusionMeshes();
	}
}

FSteamVRHMD::~FSteamVRHMD()
{
	Shutdown();
}

bool FSteamVRHMD::IsInitialized() const
{
	return (VRSystem != nullptr);
}

bool FSteamVRHMD::Startup()
{
	// grab a pointer to the renderer module for displaying our mirror window
	static const FName RendererModuleName("Renderer");
	RendererModule = FModuleManager::GetModulePtr<IRendererModule>(RendererModuleName);

	vr::EVRInitError VRInitErr = vr::VRInitError_None;
	// Attempt to initialize the VRSystem device
	VRSystem = vr::VR_Init(&VRInitErr, vr::VRApplication_Scene);
	if ((VRSystem == nullptr) || (VRInitErr != vr::VRInitError_None))
	{
		UE_LOG(LogHMD, Log, TEXT("Failed to initialize OpenVR with code %d"), (int32)VRInitErr);
		return false;
	}

	// Make sure that the version of the HMD we're compiled against is correct.  This will fill out the proper vtable!
	VRSystem = (vr::IVRSystem*)(*FSteamVRHMD::VRGetGenericInterfaceFn)(vr::IVRSystem_Version, &VRInitErr);
	if ((VRSystem == nullptr) || (VRInitErr != vr::VRInitError_None))
	{
		UE_LOG(LogHMD, Log, TEXT("Failed to initialize OpenVR (version mismatch) with code %d"), (int32)VRInitErr);
		return false;
	}

	// attach to the compositor	
	//VRCompositor = (vr::IVRCompositor*)vr::VR_GetGenericInterface(vr::IVRCompositor_Version, &HmdErr);
	int CompositorConnectRetries = 10;
	do
	{
		VRCompositor = (vr::IVRCompositor*)(*VRGetGenericInterfaceFn)(vr::IVRCompositor_Version, &VRInitErr);
		
		// If SteamVR was not running when VR_Init was called above, the system may take a few seconds to initialize.
		// retry a few times before giving up if we get a Compositor connection error.
		// This is a temporary workaround an issue that will be solved in a future version of SteamVR, where VR_Init will block until everything is ready,
		// It's only triggered in cases where SteamVR is available, but was not running prior to calling VR_Init above.
		if ((--CompositorConnectRetries > 0) && (VRInitErr == vr::VRInitError_IPC_CompositorConnectFailed || VRInitErr == vr::VRInitError_IPC_CompositorInvalidConnectResponse))
		{
			UE_LOG(LogHMD, Warning, TEXT("Failed to get Compositor connnection (%d) retrying... (%d attempt(s) left)"), (int32)VRInitErr, CompositorConnectRetries);
			FPlatformProcess::Sleep(1);
		}
		else
		{
			break;
		}
	} while (true);
	
	if ((VRCompositor != nullptr) && (VRInitErr == vr::VRInitError_None))
	{
		VROverlay = (vr::IVROverlay*)(*VRGetGenericInterfaceFn)(vr::IVROverlay_Version, &VRInitErr);
	}

	if ((VROverlay != nullptr) && (VRInitErr == vr::VRInitError_None))
	{
		// grab info about the attached display
		FString DriverId = GetFStringTrackedDeviceProperty(VRSystem, vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_TrackingSystemName_String);
		DisplayId = GetFStringTrackedDeviceProperty(VRSystem, vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_SerialNumber_String);

		// determine our ideal screen percentage
		uint32 RecommendedWidth, RecommendedHeight;
		VRSystem->GetRecommendedRenderTargetSize(&RecommendedWidth, &RecommendedHeight);
		RecommendedWidth *= 2;
		
		FrameSettings.RecommendedWidth = RecommendedWidth;
		FrameSettings.RecommendedHeight = RecommendedHeight;
		FrameSettings.RenderTargetSize.X = RecommendedWidth;
		FrameSettings.RenderTargetSize.Y = RecommendedHeight;

		int32 ScreenX, ScreenY;
		uint32 ScreenWidth, ScreenHeight;
		GetWindowBounds(&ScreenX, &ScreenY, &ScreenWidth, &ScreenHeight);

		float WidthPercentage = ((float)RecommendedWidth / (float)ScreenWidth) * 100.0f;
		float HeightPercentage = ((float)RecommendedHeight / (float)ScreenHeight) * 100.0f;

		float ScreenPercentage = FMath::Max(WidthPercentage, HeightPercentage);
		IdealScreenPercentage = ScreenPercentage;

		/*
		//@todo steamvr: move out of here
		static IConsoleVariable* CScrPercVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage"));

		if (FMath::RoundToInt(CScrPercVar->GetFloat()) != FMath::RoundToInt(ScreenPercentage))
		{
			CScrPercVar->Set(ScreenPercentage);
		}
		*/

		UpdateStereoRenderingParams();

		// Set up the adaptive buckets for pixel density, and start at the highest
		AdaptivePixelDensityBuckets.Add(0.60f);
		AdaptivePixelDensityBuckets.Add(0.65f);
		AdaptivePixelDensityBuckets.Add(0.70f);
		AdaptivePixelDensityBuckets.Add(0.75f);
		AdaptivePixelDensityBuckets.Add(0.80f);
		AdaptivePixelDensityBuckets.Add(0.85f);
		AdaptivePixelDensityBuckets.Add(0.90f);
		AdaptivePixelDensityBuckets.Add(0.95f);
		AdaptivePixelDensityBuckets.Add(1.00f);
		CurrentAdaptiveBucket = AdaptivePixelDensityBuckets.Num() - 1;

		PreviousFrameTimes.AddZeroed(PreviousFrameBufferSize);
		CurrentFrameTimesBufferIndex = 0;

		// disable vsync
		static IConsoleVariable* CVSyncVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.VSync"));
		CVSyncVar->Set(false);

		// enforce finishcurrentframe
		static IConsoleVariable* CFCFVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.finishcurrentframe"));
		CFCFVar->Set(false);

		// Grab the chaperone
		vr::EVRInitError ChaperoneErr = vr::VRInitError_None;
		VRChaperone = (vr::IVRChaperone*)(*VRGetGenericInterfaceFn)(vr::IVRChaperone_Version, &ChaperoneErr);
		if ((VRChaperone != nullptr) && (ChaperoneErr == vr::VRInitError_None))
		{
			ChaperoneBounds = FChaperoneBounds(VRChaperone);
		}
		else
		{
			UE_LOG(LogHMD, Log, TEXT("Failed to initialize Chaperone.  Error: %d"), (int32)ChaperoneErr);
		}

		vr::EVRInitError RenderModelsErr = vr::VRInitError_None;
		VRRenderModels = (vr::IVRRenderModels*)(*VRGetGenericInterfaceFn)(vr::IVRRenderModels_Version, &RenderModelsErr);

#if PLATFORM_MAC
		if (IsMetalPlatform(GMaxRHIShaderPlatform))
		{
			pBridge = new MetalBridge(this);
		}
#else
		if ( IsPCPlatform( GMaxRHIShaderPlatform ) )
		{
			if ( IsVulkanPlatform( GMaxRHIShaderPlatform ) )
			{
				pBridge = new VulkanBridge( this );
			}
			else if ( IsOpenGLPlatform( GMaxRHIShaderPlatform ) )
			{
				pBridge = new OpenGLBridge( this );
			}
#if PLATFORM_WINDOWS
			else
			{
				pBridge = new D3D11Bridge( this );
			}
#endif
			ensure( pBridge != nullptr );
		}
#endif

		LoadFromIni();

		SplashTicker = MakeShareable(new FSteamSplashTicker(this));
		SplashTicker->RegisterForMapLoad();

		CreateSpectatorScreenController();

		DrawDebugDelegateHandle = UDebugDrawService::Register(TEXT("SteamVR"), FDebugDrawDelegate::CreateRaw(this, &FSteamVRHMD::DrawDebug));

		UE_LOG(LogHMD, Log, TEXT("SteamVR initialized.  Driver: %s  Display: %s"), *DriverId, *DisplayId);
		return true;
	}
	
	UE_LOG(LogHMD, Log, TEXT("SteamVR failed to initialize.  Err: %d"), (int32)VRInitErr);

	VRSystem = nullptr;
	return false;

}

void FSteamVRHMD::LoadFromIni()
{
	const TCHAR* SteamVRSettings = TEXT("SteamVR.Settings");
	int32 i;

	if (GConfig->GetInt(SteamVRSettings, TEXT("WindowMirrorBoundsWidth"), i, GEngineIni))
	{
		WindowMirrorBoundsWidth = i;
	}

	if (GConfig->GetInt(SteamVRSettings, TEXT("WindowMirrorBoundsHeight"), i, GEngineIni))
	{
		WindowMirrorBoundsHeight = i;
	}

	float ConfigFloat = 0.0f;

	if (GConfig->GetFloat(SteamVRSettings, TEXT("HMDWornMovementThreshold"), ConfigFloat, GEngineIni))
	{
		HMDWornMovementThreshold = ConfigFloat;
	}
}

void FSteamVRHMD::Shutdown()
{
	if (DrawDebugDelegateHandle.IsValid())
	{
		UDebugDrawService::Unregister(DrawDebugDelegateHandle);
		DrawDebugDelegateHandle.Reset();
	}

	if (VRSystem != nullptr)
	{
		SplashTicker->UnregisterForMapLoad();
		SplashTicker = nullptr;

		// shut down our headset
		VRSystem = nullptr;
		VRCompositor = nullptr;
		VROverlay = nullptr;
		VRChaperone = nullptr;
		VRRenderModels = nullptr;

		SteamVRPlugin->Reset();

		vr::VR_Shutdown();
	}
}

static void SetupHiddenAreaMeshes(vr::IVRSystem* const VRSystem, FHMDViewMesh Result[2], const vr::EHiddenAreaMeshType MeshType)
{
	const vr::HiddenAreaMesh_t LeftEyeMesh = VRSystem->GetHiddenAreaMesh(vr::Hmd_Eye::Eye_Left, MeshType);
	const vr::HiddenAreaMesh_t RightEyeMesh = VRSystem->GetHiddenAreaMesh(vr::Hmd_Eye::Eye_Right, MeshType);

	const uint32 VertexCount = LeftEyeMesh.unTriangleCount * 3;
	check(LeftEyeMesh.unTriangleCount == RightEyeMesh.unTriangleCount);

	// Copy mesh data from SteamVR format to ours, then initialize the meshes.
	if (VertexCount > 0)
	{
		FVector2D* const LeftEyePositions = new FVector2D[VertexCount];
		FVector2D* const RightEyePositions = new FVector2D[VertexCount];

		uint32 DataIndex = 0;
		for (uint32 TriangleIter = 0; TriangleIter < LeftEyeMesh.unTriangleCount; ++TriangleIter)
		{
			for (uint32 VertexIter = 0; VertexIter < 3; ++VertexIter)
			{
				const vr::HmdVector2_t& LeftSrc = LeftEyeMesh.pVertexData[DataIndex];
				const vr::HmdVector2_t& RightSrc = RightEyeMesh.pVertexData[DataIndex];

				FVector2D& LeftDst = LeftEyePositions[DataIndex];
				FVector2D& RightDst = RightEyePositions[DataIndex];

				LeftDst.X = LeftSrc.v[0];
				LeftDst.Y = LeftSrc.v[1];

				RightDst.X = RightSrc.v[0];
				RightDst.Y = RightSrc.v[1];

				++DataIndex;
			}
		}
		
		const FHMDViewMesh::EHMDMeshType MeshTransformType = (MeshType == vr::EHiddenAreaMeshType::k_eHiddenAreaMesh_Standard) ? FHMDViewMesh::MT_HiddenArea : FHMDViewMesh::MT_VisibleArea;
		Result[0].BuildMesh(LeftEyePositions, VertexCount, MeshTransformType);
		Result[1].BuildMesh(RightEyePositions, VertexCount, MeshTransformType);

		delete[] LeftEyePositions;
		delete[] RightEyePositions;
	}
}


void FSteamVRHMD::SetupOcclusionMeshes()
{	
	SetupHiddenAreaMeshes(VRSystem, HiddenAreaMeshes, vr::EHiddenAreaMeshType::k_eHiddenAreaMesh_Standard);
	
	if(CUseSteamVRVisibleAreaMesh.GetValueOnAnyThread() > 0)
	{
		SetupHiddenAreaMeshes(VRSystem, VisibleAreaMeshes, vr::EHiddenAreaMeshType::k_eHiddenAreaMesh_Inverse);
	}
}

const FSteamVRHMD::FTrackingFrame& FSteamVRHMD::GetTrackingFrame() const
{
	if (IsInRenderingThread())
	{
		return RenderTrackingFrame;
	}
	else
	{
		return GameTrackingFrame;
	}
}

FAutoConsoleVariableSink FSteamVRHMD::ConsoleVariableSink(FConsoleCommandDelegate::CreateStatic(&FSteamVRHMD::ConsoleSinkHandler));
void FSteamVRHMD::ConsoleSinkHandler()
{
	if (GEngine && GEngine->XRSystem.IsValid())
	{
		static FName SteamVRName(TEXT("SteamVR"));

		if (GEngine->XRSystem->GetSystemName() == SteamVRName)
		{
			FSteamVRHMD* HMD = static_cast<FSteamVRHMD*>(GEngine->XRSystem.Get());
			static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataFloat(TEXT("r.ScreenPercentage"));
			const float CurrentScreenPercentage = CVar->GetValueOnGameThread();
			if(CurrentScreenPercentage != (HMD->FrameSettings.CurrentPixelDensity * HMD->IdealScreenPercentage))
			{
				HMD->FrameSettings.bNeedsUpdate = true;
			}
		}
	}
}

void FSteamVRHMD::AdaptivePixelDensityCommandHandler(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
{
	if (Args.Num() > 0)
	{
		int32 Value = FCString::Atoi(*Args[0]);
		FrameSettings.bAdaptivePixelDensity = (Value != 0);
		FrameSettings.bNeedsUpdate = true;
	}
}

float FSteamVRHMD::GetPixelDensity() const
{
	static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataFloat(TEXT("r.ScreenPercentage"));
	float CurrentScreenPercentage= CVar->GetValueOnGameThread();

	return CurrentScreenPercentage / IdealScreenPercentage;
}

void FSteamVRHMD::SetPixelDensity(float NewPD)
{
	static IConsoleVariable* CScrPercVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage"));
	CScrPercVar->Set(NewPD * IdealScreenPercentage, EConsoleVariableFlags(CScrPercVar->GetFlags() & ECVF_SetByMask));
}

int32 FSteamVRHMD::CalculateScalabilityFactor()
{
	int32 RetVal = 0;

	const float GPUTarget = CAdaptiveGPUTimeThreshold->GetFloat();

	// Gather the GPU timing information.  This isn't hooked up on Panda yet, so we have to use the RHIGetGPUFrameCycles call as a substitute
#if PLATFORM_MAC
	const uint32 GPUCycles = RHIGetGPUFrameCycles();
	const float CurrentFrameTime = FPlatformTime::ToMilliseconds(GPUCycles) + CDebugAdaptiveGPUTime->GetFloat();
#else
	vr::Compositor_FrameTiming FrameTiming;
	FrameTiming.m_nSize = sizeof(vr::Compositor_FrameTiming);
	VRCompositor->GetFrameTiming(&FrameTiming);
	const float CurrentFrameTime = FrameTiming.m_flPreSubmitGpuMs + CDebugAdaptiveGPUTime->GetFloat();
#endif

	// Get the historical frame data
	int32 PreviousFrameIndex = CurrentFrameTimesBufferIndex - 1;
	PreviousFrameIndex = (PreviousFrameIndex < 0) ? (PreviousFrameBufferSize + PreviousFrameIndex) : PreviousFrameIndex;
	const float PreviousFrameTime = PreviousFrameTimes[CurrentFrameTimesBufferIndex];

	int32 TwoPreviousFrameTimeIndex = CurrentFrameTimesBufferIndex - 2;
	TwoPreviousFrameTimeIndex = (TwoPreviousFrameTimeIndex < 0) ? (PreviousFrameBufferSize + TwoPreviousFrameTimeIndex) : TwoPreviousFrameTimeIndex;
	const float TwoPreviousFrameTime = PreviousFrameTimes[TwoPreviousFrameTimeIndex];

	// Record the current frame into the buffer
	PreviousFrameTimes[(CurrentFrameTimesBufferIndex++) % PreviousFrameBufferSize] = CurrentFrameTime;
	if (CurrentFrameTimesBufferIndex >= PreviousFrameBufferSize)
	{
		CurrentFrameTimesBufferIndex = 0;
	}

	// If we're frame locked, bail after updating our buffers
	if (FrameSettings.PixelDensityAdaptiveLockedFrames-- > 0)
	{
		return RetVal;
	}

	// Adapted from Alex Vlachos' GDC presentation "Advanced VR Rendering Performance" (GDC 2016)

	// If the current frame is above 90% of the total time, drop two buckets
	if (CurrentFrameTime > 0.9f * GPUTarget)
	{
		RetVal = -2;
	}
	else
	{
		// If the last three frames were below 70% of the total time, raise one bucket
		const float SeventyPercentTargetTime = 0.7f * GPUTarget;
		if (TwoPreviousFrameTime < SeventyPercentTargetTime
			&& PreviousFrameTime < SeventyPercentTargetTime
			&& CurrentFrameTime < SeventyPercentTargetTime)
		{
			RetVal = 1;
		}

		// If the last frame was above 85%, and the predicted next frame is above 90%, drop two buckets
		const float PredictedFrameTime = 2.f * CurrentFrameTime - PreviousFrameTime;
		if ((CurrentFrameTime > 0.85f * GPUTarget) && (PredictedFrameTime > 0.9f * GPUTarget))
		{
			RetVal = -2;
		}
	}

	// If we've changed, give it two frames to settle down before making any more adjustments
	if (RetVal != 0)
	{
		FrameSettings.PixelDensityAdaptiveLockedFrames = 2;
	}

	const bool bOutputDebug = (CDebugAdaptiveOutput.GetValueOnAnyThread() > 0);
	if(bOutputDebug)
	{
		UE_LOG(LogHMD, Log, TEXT("FrameTime: %2.1f, FrameTime - 1: %2.1f, Frametime - 2: %2.1f"), CurrentFrameTime, PreviousFrameTime, TwoPreviousFrameTime);
	}

	return RetVal;
}

void FSteamVRHMD::UpdateStereoRenderingParams()
{
	FScopeLock Lock(&FrameSettingsLock);

	if (FrameSettings.bAdaptivePixelDensity)
	{
		// If we changed AA modes because of a PD switch, restore it here
		static const auto PostProcessAAQualityCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.PostProcessAAQuality"));
		if ((FrameSettings.PostProcessAARestoreValue != INDEX_NONE) && (FrameSettings.PixelDensityAdaptiveLockedFrames <= 0))
		{
			PostProcessAAQualityCVar->Set(FrameSettings.PostProcessAARestoreValue);
			FrameSettings.PostProcessAARestoreValue = INDEX_NONE;
		}

		// Update values for our PD range, in case they've been changed
		FrameSettings.PixelDensityMin = CUseAdaptivePDMin.GetValueOnAnyThread();
		FrameSettings.PixelDensityMax = CUseAdaptivePDMax.GetValueOnAnyThread();
		const bool bDebugAdaptive = (CDebugAdaptiveCycle.GetValueOnAnyThread() > 0);

		if(bDebugAdaptive)
		{
			FrameSettings.CurrentPixelDensity -= 0.005;
			if (FrameSettings.CurrentPixelDensity < FrameSettings.PixelDensityMin)
			{
				FrameSettings.CurrentPixelDensity = FrameSettings.PixelDensityMax;
			}
		}
		else
		{
			// Determine if we need to scale up or down based on the most recent frames.  This will tell us if we need to move up or down a bucket
			const int32 PerformanceDelta = CalculateScalabilityFactor();
			CurrentAdaptiveBucket = FMath::Clamp(CurrentAdaptiveBucket + PerformanceDelta, 0, AdaptivePixelDensityBuckets.Num() - 1);
			
			// If we've actually changed, we need to disable TAA to avoid artifacting, and then restore it the next frame.
			if (FrameSettings.CurrentPixelDensity != AdaptivePixelDensityBuckets[CurrentAdaptiveBucket])
			{
				// If desired, turn off TAA for a few frames because of the buffer resizing
				if((CDebugAdaptivePostProcess.GetValueOnGameThread() != 0) && (FrameSettings.PostProcessAARestoreValue == INDEX_NONE))
				{
					FrameSettings.PostProcessAARestoreValue = PostProcessAAQualityCVar->GetInt();
					PostProcessAAQualityCVar->Set(2);
				}
				
				FrameSettings.CurrentPixelDensity = AdaptivePixelDensityBuckets[CurrentAdaptiveBucket];
			}
		}
		
		const bool bOutputDebug = (CDebugAdaptiveOutput.GetValueOnAnyThread() > 0);
		if(bOutputDebug)
		{
			UE_LOG(LogHMD, Log, TEXT("---> PDAdaptive: %1.2f"), FrameSettings.CurrentPixelDensity);
		}
	}
	else
	{
		const float CurrentPixelDensity = GetPixelDensity();
		FrameSettings.CurrentPixelDensity = CurrentPixelDensity;
		FrameSettings.PixelDensityMin = CurrentPixelDensity;
		FrameSettings.PixelDensityMax = CurrentPixelDensity;
	}

	const float PD = FrameSettings.CurrentPixelDensity;
	const float PDMax = FrameSettings.PixelDensityMax;

	const uint32 ViewRecommendedWidth = FMath::CeilToInt(PD * FrameSettings.RecommendedWidth / 2.f);
	const uint32 ViewRecommendedHeight = FMath::CeilToInt(PD * FrameSettings.RecommendedHeight);

	const uint32 ViewMaximumWidth = FMath::CeilToInt(PDMax * FrameSettings.RecommendedWidth / 2.f);
	const uint32 ViewMaxiumumHeight = FMath::CeilToInt(PDMax * FrameSettings.RecommendedHeight);

	const uint32 TotalWidth = FMath::CeilToInt(PDMax * FrameSettings.RecommendedWidth);

	// Left Eye Viewport and Max Viewport
	FrameSettings.EyeViewports[0].Min = FIntPoint(0, 0);
	FrameSettings.EyeViewports[0].Max = FIntPoint(ViewRecommendedWidth, ViewRecommendedHeight);

	// Right Eye Viewport and Max Viewport
	FrameSettings.EyeViewports[1].Min = FIntPoint(TotalWidth - ViewRecommendedWidth, 0);
	FrameSettings.EyeViewports[1].Max = FIntPoint(TotalWidth, ViewRecommendedHeight);

	if (FrameSettings.bAdaptivePixelDensity)
	{
		FrameSettings.MaxViewports[0].Min = FIntPoint(0, 0);
		FrameSettings.MaxViewports[0].Max = FIntPoint(ViewMaximumWidth, ViewMaxiumumHeight);

		FrameSettings.MaxViewports[1].Min = FIntPoint(TotalWidth - ViewMaximumWidth, 0);
		FrameSettings.MaxViewports[1].Max = FIntPoint(TotalWidth, ViewMaxiumumHeight);
	}
	else
	{
		FrameSettings.MaxViewports[0] = FrameSettings.EyeViewports[0];
		FrameSettings.MaxViewports[1] = FrameSettings.EyeViewports[1];
	}

	FrameSettings.RenderTargetSize.X = TotalWidth;
	FrameSettings.RenderTargetSize.Y = ViewMaxiumumHeight;

	SetPixelDensity(FrameSettings.CurrentPixelDensity);

	FrameSettings.bNeedsUpdate = FrameSettings.bAdaptivePixelDensity;
}

#endif //STEAMVR_SUPPORTED_PLATFORMS
