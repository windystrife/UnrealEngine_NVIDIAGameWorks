// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanRHI.cpp: Vulkan device RHI implementation.
=============================================================================*/

#include "VulkanRHIPrivate.h"
#include "HardwareInfo.h"
#include "VulkanShaderResources.h"
#include "VulkanSwapChain.h"
#include "VulkanResources.h"
#include "VulkanPendingState.h"
#include "VulkanContext.h"
#include "Misc/CommandLine.h"
#include "GenericPlatformDriver.h"
#include "Modules/ModuleManager.h"
#include "VulkanPipelineState.h"

#if VULKAN_ENABLE_DESKTOP_HMD_SUPPORT
#include "Runtime/HeadMountedDisplay/Public/IHeadMountedDisplayModule.h"
#endif

#define LOCTEXT_NAMESPACE "VulkanRHI"

#ifdef VK_API_VERSION
// Check the SDK is least the API version we want to use
static_assert(VK_API_VERSION >= UE_VK_API_VERSION, "Vulkan SDK is older than the version we want to support (UE_VK_API_VERSION). Please update your SDK.");
#elif !defined(VK_HEADER_VERSION)
	#error No VulkanSDK defines?
#endif

#if defined(VK_HEADER_VERSION) && VK_HEADER_VERSION < 8 && (VK_API_VERSION < VK_MAKE_VERSION(1, 0, 3))
	#include <vulkan/vk_ext_debug_report.h>
#endif

///////////////////////////////////////////////////////////////////////////////

#if PLATFORM_ANDROID || PLATFORM_LINUX

// Vulkan function pointers

#define DEFINE_VK_ENTRYPOINTS(Type,Func) Type VulkanDynamicAPI::Func = NULL;
ENUM_VK_ENTRYPOINTS_ALL(DEFINE_VK_ENTRYPOINTS)

#endif

///////////////////////////////////////////////////////////////////////////////

TAutoConsoleVariable<int32> GRHIThreadCvar(
	TEXT("r.Vulkan.RHIThread"),
	1,
	TEXT("0 to only use Render Thread\n")
	TEXT("1 to use ONE RHI Thread\n")
	TEXT("2 to use multiple RHI Thread\n")
);

#if VULKAN_CUSTOM_MEMORY_MANAGER_ENABLED
VkAllocationCallbacks GCallbacks;
#endif

struct FVulkanMemManager
{
	static VKAPI_ATTR void* Alloc(
		void*                                       pUserData,
		size_t                                      size,
		size_t                                      alignment,
		VkSystemAllocationScope                           allocScope)
	{
		FVulkanMemManager* This = (FVulkanMemManager*)pUserData;
		This->MaxAllocSize = FMath::Max(This->MaxAllocSize, size);
		This->UsedMemory += size;
		void* Data = FMemory::Malloc(size, alignment);
		This->Allocs.Add(Data, size);
		return Data;
	}

	static VKAPI_ATTR void Free(
		void*                                       pUserData,
		void*                                       pMem)
	{
		FVulkanMemManager* This = (FVulkanMemManager*)pUserData;
		size_t Size = This->Allocs.FindAndRemoveChecked(pMem);
		This->UsedMemory -= Size;
		FMemory::Free(pMem);
	}

	static VKAPI_ATTR void* Realloc(
		void*										pUserData,
		void*										pOriginal,
		size_t										size,
		size_t										alignment,
		VkSystemAllocationScope						allocScope)
	{
		FVulkanMemManager* This = (FVulkanMemManager*)pUserData;
		size_t Size = This->Allocs.FindAndRemoveChecked(pOriginal);
		This->UsedMemory -= Size;
		void* Data = FMemory::Realloc(pOriginal, size, alignment);
		This->Allocs.Add(Data, size);
		This->UsedMemory += size;
		This->MaxAllocSize = FMath::Max(This->MaxAllocSize, size);
		return Data;
	}

	static VKAPI_ATTR void InternalAllocationNotification(
		void*										pUserData,
		size_t										size,
		VkInternalAllocationType					allocationType,
		VkSystemAllocationScope						allocationScope)
	{
		//@TODO
	}

	static VKAPI_ATTR void InternalFreeNotification(
		void*										pUserData,
		size_t										size,
		VkInternalAllocationType					allocationType,
		VkSystemAllocationScope						allocationScope)
	{
		//@TODO
	}

	FVulkanMemManager() :
		MaxAllocSize(0),
		UsedMemory(0)
	{
	}

	TMap<void*, size_t> Allocs;
	size_t MaxAllocSize;
	size_t UsedMemory;
};

static FVulkanMemManager GVulkanMemMgr;

static inline int32 CountSetBits(int32 n)
{
	uint32 Count = 0;
	while (n)
	{
		n &= (n-1);
		Count++;
	}

	return Count;
}


DEFINE_LOG_CATEGORY(LogVulkan)

#if PLATFORM_ANDROID || PLATFORM_LINUX
#include <dlfcn.h>

static void *VulkanLib = nullptr;
static bool bAttemptedLoad = false;

static bool LoadVulkanLibrary()
{
	if (bAttemptedLoad)
	{
		return (VulkanLib != nullptr);
	}
	bAttemptedLoad = true;

	// try to load libvulkan.so
#if PLATFORM_LINUX
	VulkanLib = dlopen("libvulkan.so.1", RTLD_NOW | RTLD_LOCAL);
#else
	VulkanLib = dlopen("libvulkan.so", RTLD_NOW | RTLD_LOCAL);
#endif
	if (VulkanLib == nullptr)
	{
		return false;
	}

	bool bFoundAllEntryPoints = true;
#define CHECK_VK_ENTRYPOINTS(Type,Func) if (VulkanDynamicAPI::Func == NULL) { bFoundAllEntryPoints = false; UE_LOG(LogRHI, Warning, TEXT("Failed to find entry point for %s"), TEXT(#Func)); }

	// Initialize all of the entry points we have to query manually
#define GET_VK_ENTRYPOINTS(Type,Func) VulkanDynamicAPI::Func = (Type)dlsym(VulkanLib, #Func);
	ENUM_VK_ENTRYPOINTS_BASE(GET_VK_ENTRYPOINTS);
	ENUM_VK_ENTRYPOINTS_BASE(CHECK_VK_ENTRYPOINTS);
	if (!bFoundAllEntryPoints)
	{
		dlclose(VulkanLib);
		VulkanLib = nullptr;
		return false;
	}

	ENUM_VK_ENTRYPOINTS_OPTIONAL(GET_VK_ENTRYPOINTS);
	//ENUM_VK_ENTRYPOINTS_OPTIONAL(CHECK_VK_ENTRYPOINTS);

	return true;
}

static bool LoadVulkanInstanceFunctions(VkInstance inInstance)
{
	bool bFoundAllEntryPoints = true;
#define CHECK_VK_ENTRYPOINTS(Type,Func) if (VulkanDynamicAPI::Func == NULL) { bFoundAllEntryPoints = false; UE_LOG(LogRHI, Warning, TEXT("Failed to find entry point for %s"), TEXT(#Func)); }

#define GETINSTANCE_VK_ENTRYPOINTS(Type, Func) VulkanDynamicAPI::Func = (Type)VulkanDynamicAPI::vkGetInstanceProcAddr(inInstance, #Func);
	ENUM_VK_ENTRYPOINTS_INSTANCE(GETINSTANCE_VK_ENTRYPOINTS);
	ENUM_VK_ENTRYPOINTS_INSTANCE(CHECK_VK_ENTRYPOINTS);

	return bFoundAllEntryPoints;
}

static void FreeVulkanLibrary()
{
	if (VulkanLib != nullptr)
	{
#define CLEAR_VK_ENTRYPOINTS(Type,Func) VulkanDynamicAPI::Func = nullptr;
		ENUM_VK_ENTRYPOINTS_ALL(CLEAR_VK_ENTRYPOINTS);

		dlclose(VulkanLib);
		VulkanLib = nullptr;
	}
	bAttemptedLoad = false;
}

#elif PLATFORM_WINDOWS

#include "AllowWindowsPlatformTypes.h"
static HMODULE GVulkanDLLModule = nullptr;
static bool LoadVulkanLibrary()
{
	// Try to load the vulkan dll, as not everyone has the sdk installed
	GVulkanDLLModule = ::LoadLibraryW(TEXT("vulkan-1.dll"));
	return GVulkanDLLModule != nullptr;
}

// Annoyingly, some functions do not have static bindings
namespace VulkanRHI
{
	PFN_vkGetPhysicalDeviceProperties2KHR GVkGetPhysicalDeviceProperties2KHR = nullptr;
}

static bool LoadVulkanInstanceFunctions(VkInstance inInstance)
{
	if (!GVulkanDLLModule)
	{
		return false;
	}

	PFN_vkGetInstanceProcAddr GetInstanceProcAddr =  (PFN_vkGetInstanceProcAddr) FPlatformProcess::GetDllExport(GVulkanDLLModule, TEXT("vkGetInstanceProcAddr"));

	if (!GetInstanceProcAddr)
	{
		return false;
	}

#pragma warning(push)
#pragma warning(disable : 4191) // warning C4191: 'type cast': unsafe conversion

	VulkanRHI::GVkGetPhysicalDeviceProperties2KHR = (PFN_vkGetPhysicalDeviceProperties2KHR) GetInstanceProcAddr(inInstance, "vkGetPhysicalDeviceProperties2KHR");

#pragma warning(pop) // restore 4191

	return true;
}

static void FreeVulkanLibrary()
{
	if (GVulkanDLLModule != nullptr)
	{
		::FreeLibrary(GVulkanDLLModule);
		GVulkanDLLModule = nullptr;
	}
}
#include "HideWindowsPlatformTypes.h"

#else
#error Unsupported!
#endif // PLATFORM_ANDROID

bool FVulkanDynamicRHIModule::IsSupported()
{
	return true;
}

FDynamicRHI* FVulkanDynamicRHIModule::CreateRHI(ERHIFeatureLevel::Type InRequestedFeatureLevel)
{
	if (!GIsEditor &&
		(PLATFORM_ANDROID ||
			InRequestedFeatureLevel == ERHIFeatureLevel::ES3_1 || InRequestedFeatureLevel == ERHIFeatureLevel::ES2 ||
			FParse::Param(FCommandLine::Get(), TEXT("featureleveles31")) || FParse::Param(FCommandLine::Get(), TEXT("featureleveles2"))))
	{
		GMaxRHIFeatureLevel = ERHIFeatureLevel::ES3_1;
		GMaxRHIShaderPlatform = PLATFORM_ANDROID ? SP_VULKAN_ES3_1_ANDROID : SP_VULKAN_PCES3_1;
	}
	else if (InRequestedFeatureLevel == ERHIFeatureLevel::SM4)
	{
		GMaxRHIFeatureLevel = ERHIFeatureLevel::SM4;
		GMaxRHIShaderPlatform = SP_VULKAN_SM4;
	}
	else
	{
		GMaxRHIFeatureLevel = ERHIFeatureLevel::SM5;
		GMaxRHIShaderPlatform = SP_VULKAN_SM5;
	}

	// VULKAN_USE_MSAA_RESOLVE_ATTACHMENTS=0 requires separate MSAA and resolve textures
	check(RHISupportsSeparateMSAAAndResolveTextures(GMaxRHIShaderPlatform) == (!VULKAN_USE_MSAA_RESOLVE_ATTACHMENTS));

	return new FVulkanDynamicRHI();
}

IMPLEMENT_MODULE(FVulkanDynamicRHIModule, VulkanRHI);


FVulkanCommandListContext::FVulkanCommandListContext(FVulkanDynamicRHI* InRHI, FVulkanDevice* InDevice, FVulkanQueue* InQueue, bool bInIsImmediate)
	: RHI(InRHI)
	, Device(InDevice)
	, Queue(InQueue)
	, bIsImmediate(bInIsImmediate)
	, bSubmitAtNextSafePoint(false)
	, bAutomaticFlushAfterComputeShader(true)
	, UniformBufferUploader(nullptr)
	, PendingNumVertices(0)
	, PendingVertexDataStride(0)
	, PendingPrimitiveIndexType(VK_INDEX_TYPE_MAX_ENUM)
	, PendingPrimitiveType(0)
	, PendingNumPrimitives(0)
	, PendingMinVertexIndex(0)
	, PendingIndexDataStride(0)
	, TempFrameAllocationBuffer(InDevice)
	, CommandBufferManager(nullptr)
	, PendingGfxState(nullptr)
	, PendingComputeState(nullptr)
	, FrameCounter(0)
	, GpuProfiler(this, InDevice)
{
	FrameTiming = new FVulkanGPUTiming(this, InDevice);
	FrameTiming->Initialize();

	// Create CommandBufferManager, contain all active buffers
	CommandBufferManager = new FVulkanCommandBufferManager(InDevice, this);

	// Create Pending state, contains pipeline states such as current shader and etc..
	PendingGfxState = new FVulkanPendingGfxState(Device, *this);
	PendingComputeState = new FVulkanPendingComputeState(Device, *this);

	// Add an initial pool
	FVulkanDescriptorPool* Pool = new FVulkanDescriptorPool(Device);
	DescriptorPools.Add(Pool);

	UniformBufferUploader = new FVulkanUniformBufferUploader(Device, VULKAN_UB_RING_BUFFER_SIZE);
}

FVulkanCommandListContext::~FVulkanCommandListContext()
{
	check(CommandBufferManager != nullptr);
	delete CommandBufferManager;
	CommandBufferManager = nullptr;

	TransitionState.Destroy(*Device);

	delete UniformBufferUploader;
	delete PendingGfxState;
	delete PendingComputeState;

	TempFrameAllocationBuffer.Destroy();

	for (int32 Index = 0; Index < DescriptorPools.Num(); ++Index)
	{
		delete DescriptorPools[Index];
	}
	DescriptorPools.Reset(0);
}


FVulkanDynamicRHI::FVulkanDynamicRHI()
	: Instance(VK_NULL_HANDLE)
	, Device(nullptr)
	, DrawingViewport(nullptr)
	, SavePipelineCacheCmd(nullptr)
	, RebuildPipelineCacheCmd(nullptr)
#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
	, DumpMemoryCmd(nullptr)
#endif
#if VULKAN_HAS_DEBUGGING_ENABLED
	, bSupportsDebugCallbackExt(false)
	, MsgCallback(VK_NULL_HANDLE)
#endif
	, PresentCount(0)
{
	// This should be called once at the start 
	check(IsInGameThread());
	check(!GIsThreadedRendering);

	GRHIRequiresEarlyBackBufferRenderTarget = false;
	GPoolSizeVRAMPercentage = 0;
	GTexturePoolSize = 0;
	GConfig->GetInt(TEXT("TextureStreaming"), TEXT("PoolSizeVRAMPercentage"), GPoolSizeVRAMPercentage, GEngineIni);
}

void FVulkanDynamicRHI::Init()
{
	if (!LoadVulkanLibrary())
	{
#if PLATFORM_LINUX
		// be more verbose on Linux
		FPlatformMisc::MessageBoxExt(EAppMsgType::Ok, *LOCTEXT("UnableToInitializeVulkanLinux", "Unable to load Vulkan library and/or acquire the necessary function pointers. Make sure an up-to-date libvulkan.so.1 is installed.").ToString(),
									 *LOCTEXT("UnableToInitializeVulkanLinuxTitle", "Unable to initialize Vulkan.").ToString());
#endif // PLATFORM_LINUX
		UE_LOG(LogVulkanRHI, Fatal, TEXT("Failed to find all required Vulkan entry points; make sure your driver supports Vulkan!"));
	}

	InitInstance();
}

void FVulkanDynamicRHI::Shutdown()
{
	check(IsInGameThread() && IsInRenderingThread());
	check(Device);

	Device->PrepareForDestroy();

	EmptyCachedBoundShaderStates();

	FVulkanVertexDeclaration::EmptyCache();

	if (GIsRHIInitialized)
	{
		// Reset the RHI initialized flag.
		GIsRHIInitialized = false;

		check(!GIsCriticalError);

		// Ask all initialized FRenderResources to release their RHI resources.
		for (TLinkedList<FRenderResource*>::TIterator ResourceIt(FRenderResource::GetResourceList());ResourceIt;ResourceIt.Next())
		{
			FRenderResource* Resource = *ResourceIt;
			check(Resource->IsInitialized());
			Resource->ReleaseRHI();
		}

		for (TLinkedList<FRenderResource*>::TIterator ResourceIt(FRenderResource::GetResourceList());ResourceIt;ResourceIt.Next())
		{
			ResourceIt->ReleaseDynamicRHI();
		}

#if 0
		for (auto* Framebuffer : FrameBuffers)
		{
			Framebuffer->Destroy(*Device);
			delete Framebuffer;
		}
#endif

		//EmptyVulkanSamplerStateCache();

		// Flush all pending deletes before destroying the device.
		FRHIResource::FlushPendingDeletes();

		// And again since some might get on a pending queue
		FRHIResource::FlushPendingDeletes();

		//ReleasePooledTextures();
	}

	Device->Destroy();

	delete Device;
	Device = nullptr;

#if VULKAN_HAS_DEBUGGING_ENABLED
	RemoveDebugLayerCallback();
#endif

	VulkanRHI::vkDestroyInstance(Instance, nullptr);

	IConsoleManager::Get().UnregisterConsoleObject(SavePipelineCacheCmd);
	IConsoleManager::Get().UnregisterConsoleObject(RebuildPipelineCacheCmd);

#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
	IConsoleManager::Get().UnregisterConsoleObject(DumpMemoryCmd);
#endif

	FreeVulkanLibrary();

#if VULKAN_ENABLE_DUMP_LAYER
	VulkanRHI::FlushDebugWrapperLog();
#endif
}

void FVulkanDynamicRHI::CreateInstance()
{
	VkApplicationInfo App;
	FMemory::Memzero(App);
	App.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	App.pApplicationName = "UE4";
	App.applicationVersion = 0;
	App.pEngineName = "UE4";
	App.engineVersion = 15;
	App.apiVersion = UE_VK_API_VERSION;

	VkInstanceCreateInfo InstInfo;
	FMemory::Memzero(InstInfo);
	InstInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	InstInfo.pNext = nullptr;
	InstInfo.pApplicationInfo = &App;

#if VULKAN_CUSTOM_MEMORY_MANAGER_ENABLED
	GCallbacks.pUserData = &GVulkanMemMgr;
	GCallbacks.pfnAllocation = &FVulkanMemManager::Alloc;
	GCallbacks.pfnReallocation = &FVulkanMemManager::Realloc;
	GCallbacks.pfnFree = &FVulkanMemManager::Free;
	GCallbacks.pfnInternalAllocation = &FVulkanMemManager::InternalAllocationNotification;
	GCallbacks.pfnInternalFree = &FVulkanMemManager::InternalFreeNotification;

//@TODO: pass GCallbacks into funcs that take them. currently it's just a nullptr
#endif

	GetInstanceLayersAndExtensions(InstanceExtensions, InstanceLayers);

	InstInfo.enabledExtensionCount = InstanceExtensions.Num();
	InstInfo.ppEnabledExtensionNames = InstInfo.enabledExtensionCount > 0 ? (const ANSICHAR* const*)InstanceExtensions.GetData() : nullptr;
	
	InstInfo.enabledLayerCount = InstanceLayers.Num();
	InstInfo.ppEnabledLayerNames = InstInfo.enabledLayerCount > 0 ? InstanceLayers.GetData() : nullptr;
#if VULKAN_HAS_DEBUGGING_ENABLED
	bSupportsDebugCallbackExt = InstanceExtensions.ContainsByPredicate([](const ANSICHAR* Key)
		{ 
			return Key && !FCStringAnsi::Strcmp(Key, VK_EXT_DEBUG_REPORT_EXTENSION_NAME); 
		});
#endif

	VkResult Result = VulkanRHI::vkCreateInstance(&InstInfo, nullptr, &Instance);
	
	if (Result == VK_ERROR_INCOMPATIBLE_DRIVER)
	{
		FPlatformMisc::MessageBoxExt(EAppMsgType::Ok, TEXT(
			"Cannot find a compatible Vulkan driver (ICD).\n\nPlease look at the Getting Started guide for \
			additional information."), TEXT("Incompatible Vulkan driver found!"));
	}
	else if(Result == VK_ERROR_EXTENSION_NOT_PRESENT)
	{
		FPlatformMisc::MessageBoxExt(EAppMsgType::Ok, TEXT(
			"Vulkan driver doesn't contain specified extension;\n\
			make sure your layers path is set appropriately."), TEXT("Incomplete Vulkan driver found!"));
	}
	else if (Result != VK_SUCCESS)
	{
		FPlatformMisc::MessageBoxExt(EAppMsgType::Ok, TEXT(
			"Vulkan failed to create instace (apiVersion=0x%x)\n\nDo you have a compatible Vulkan \
			 driver (ICD) installed?\nPlease look at \
			 the Getting Started guide for additional information."), TEXT("No Vulkan driver found!"));
	}

	VERIFYVULKANRESULT(Result);

	if (!LoadVulkanInstanceFunctions(Instance))
	{
		FPlatformMisc::MessageBoxExt(EAppMsgType::Ok, TEXT(
			"Failed to find all required Vulkan entry points! Try updating your driver."), TEXT("No Vulkan entry points found!"));
	}

#if VULKAN_HAS_DEBUGGING_ENABLED
	SetupDebugLayerCallback();
#endif
}

void FVulkanDynamicRHI::InitInstance()
{
	check(IsInGameThread());

	// Wait for the rendering thread to go idle.
	SCOPED_SUSPEND_RENDERING_THREAD(false);

	if (!Device)
	{
		check(!GIsRHIInitialized);

#if PLATFORM_ANDROID
		// Want to see the actual crash report on Android so unregister signal handlers
		FPlatformMisc::SetCrashHandler((void(*)(const FGenericCrashContext& Context)) -1);
		FPlatformMisc::SetOnReInitWindowCallback(FVulkanDynamicRHI::RecreateSwapChain);
#endif

		GRHISupportsAsyncTextureCreation = false;

#if VULKAN_ENABLE_DESKTOP_HMD_SUPPORT
		// Allow HMD to override which graphics adapter is chosen, so we pick the adapter where the HMD is connected
		uint64 HmdGraphicsAdapterLuid  = IHeadMountedDisplayModule::IsAvailable() ? IHeadMountedDisplayModule::Get().GetGraphicsAdapterLuid() : 0;
#endif

		{
			CreateInstance();

			uint32 GpuCount = 0;
			VERIFYVULKANRESULT_EXPANDED(VulkanRHI::vkEnumeratePhysicalDevices(Instance, &GpuCount, nullptr));
			check(GpuCount >= 1);

			TArray<VkPhysicalDevice> PhysicalDevices;
			PhysicalDevices.AddZeroed(GpuCount);
			VERIFYVULKANRESULT_EXPANDED(VulkanRHI::vkEnumeratePhysicalDevices(Instance, &GpuCount, PhysicalDevices.GetData()));

#if VULKAN_ENABLE_DESKTOP_HMD_SUPPORT
			FVulkanDevice* HmdDevice = nullptr;
			uint32 HmdDeviceIndex = 0;
#endif
			FVulkanDevice* DiscreteDevice = nullptr;
			uint32 DiscreteDeviceIndex = 0;

			UE_LOG(LogVulkanRHI, Display, TEXT("Found %d device(s)"), GpuCount);
			for (uint32 Index = 0; Index < GpuCount; ++Index)
			{
				FVulkanDevice* NewDevice = new FVulkanDevice(PhysicalDevices[Index]);
				Devices.Add(NewDevice);

				bool bIsDiscrete = NewDevice->QueryGPU(Index);

#if VULKAN_ENABLE_DESKTOP_HMD_SUPPORT
				if (!HmdDevice && HmdGraphicsAdapterLuid != 0 && 
					NewDevice->GetOptionalExtensions().HasKHRGetPhysicalDeviceProperties2 && 
					FMemory::Memcmp(&HmdGraphicsAdapterLuid, &NewDevice->GetDeviceIdProperties().deviceLUID, VK_LUID_SIZE_KHR) == 0)
				{
					HmdDevice = NewDevice;
					HmdDeviceIndex = Index;
				}
#endif

				if (!DiscreteDevice && bIsDiscrete)
				{
					DiscreteDevice = NewDevice;
					DiscreteDeviceIndex = Index;
				}
			}

			uint32 DeviceIndex;

#if VULKAN_ENABLE_DESKTOP_HMD_SUPPORT
			if (HmdDevice)
			{
				Device = HmdDevice;
				DeviceIndex = HmdDeviceIndex;
			} 
			else
#endif
			if (DiscreteDevice)
			{
				Device = DiscreteDevice;
				DeviceIndex = DiscreteDeviceIndex;
			}
			else
			{
				Device = Devices[0];
				DeviceIndex = 0;
			}

			check(Device);
			Device->InitGPU(DeviceIndex);
		}

		bool bDeviceSupportsGeometryShaders = Device->GetFeatures().geometryShader != 0;
		bool bDeviceSupportsTessellation = Device->GetFeatures().tessellationShader != 0;

		const VkPhysicalDeviceProperties& Props = Device->GetDeviceProperties();

		// Initialize the RHI capabilities.
		GRHIVendorId = Device->GetDeviceProperties().vendorID;
		GRHIAdapterName = ANSI_TO_TCHAR(Props.deviceName);
		if (PLATFORM_ANDROID)
		{
			GRHIAdapterInternalDriverVersion = FString::Printf(TEXT("%d.%d.%d"), VK_VERSION_MAJOR(Props.apiVersion), VK_VERSION_MINOR(Props.apiVersion), VK_VERSION_PATCH(Props.apiVersion));
		}
		else if (IsRHIDeviceNVIDIA())
		{
			union UNvidiaDriverVersion
			{
				struct
				{
#if PLATFORM_LITTLE_ENDIAN
					uint32 Tertiary		: 6;
					uint32 Secondary	: 8;
					uint32 Minor		: 8;
					uint32 Major		: 10;
#else
					uint32 Major		: 10;
					uint32 Minor		: 8;
					uint32 Secondary	: 8;
					uint32 Tertiary		: 6;
#endif
				};
				uint32 Packed;
			};
			UNvidiaDriverVersion NvidiaVersion;
			static_assert(sizeof(NvidiaVersion) == sizeof(Props.driverVersion), "Mismatched Nvidia pack driver version!");
			NvidiaVersion.Packed = Props.driverVersion;
			GRHIAdapterUserDriverVersion = FString::Printf(TEXT("%d.%d"), NvidiaVersion.Major, NvidiaVersion.Minor);

			// Ignore GRHIAdapterInternalDriverVersion for now as the device name doesn't match
		}
		GRHISupportsFirstInstance = true;
		GSupportsRenderTargetFormat_PF_G8 = false;	// #todo-rco
		GSupportsQuads = false;	// Not supported in Vulkan
		GRHISupportsTextureStreaming = true;
		GSupportsTimestampRenderQueries = !PLATFORM_ANDROID;
		GRHIRequiresEarlyBackBufferRenderTarget = false;
		GSupportsGenerateMips = true;
#if VULKAN_ENABLE_DUMP_LAYER
		// Disable RHI thread by default if the dump layer is enabled
		GRHISupportsRHIThread = false;
#else
		GRHISupportsRHIThread = GRHIThreadCvar->GetInt() != 0;
		GRHISupportsParallelRHIExecute = GRHIThreadCvar->GetInt() > 1;
#endif

		GSupportsVolumeTextureRendering = true;

		// Indicate that the RHI needs to use the engine's deferred deletion queue.
		GRHINeedsExtraDeletionLatency = true;

		GMaxShadowDepthBufferSizeX =  FPlatformMath::Min<int32>(Props.limits.maxImageDimension2D, GMaxShadowDepthBufferSizeX);
		GMaxShadowDepthBufferSizeY =  FPlatformMath::Min<int32>(Props.limits.maxImageDimension2D, GMaxShadowDepthBufferSizeY);
		GMaxTextureDimensions = Props.limits.maxImageDimension2D;
		GMaxTextureMipCount = FPlatformMath::CeilLogTwo( GMaxTextureDimensions ) + 1;
		GMaxTextureMipCount = FPlatformMath::Min<int32>( MAX_TEXTURE_MIP_COUNT, GMaxTextureMipCount );
		GMaxCubeTextureDimensions = Props.limits.maxImageDimensionCube;
		GMaxTextureArrayLayers = Props.limits.maxImageArrayLayers;
		GRHISupportsBaseVertexIndex = true;
		GSupportsSeparateRenderTargetBlendState = true;

		GSupportsDepthFetchDuringDepthTest = !PLATFORM_ANDROID;

		GShaderPlatformForFeatureLevel[ERHIFeatureLevel::ES2] = GMaxRHIFeatureLevel == ERHIFeatureLevel::ES2 ? GMaxRHIShaderPlatform : SP_NumPlatforms;
		GShaderPlatformForFeatureLevel[ERHIFeatureLevel::ES3_1] = GMaxRHIFeatureLevel == ERHIFeatureLevel::ES3_1 ? GMaxRHIShaderPlatform : SP_NumPlatforms;
		GShaderPlatformForFeatureLevel[ERHIFeatureLevel::SM4] = (GMaxRHIFeatureLevel == ERHIFeatureLevel::SM4 && bDeviceSupportsGeometryShaders) ? GMaxRHIShaderPlatform : SP_NumPlatforms;
		GShaderPlatformForFeatureLevel[ERHIFeatureLevel::SM5] = (GMaxRHIFeatureLevel == ERHIFeatureLevel::SM5 && bDeviceSupportsTessellation) ? GMaxRHIShaderPlatform : SP_NumPlatforms;

		GRHIRequiresRenderTargetForPixelShaderUAVs = true;

		GDynamicRHI = this;

		// Notify all initialized FRenderResources that there's a valid RHI device to create their RHI resources for now.
		for (TLinkedList<FRenderResource*>::TIterator ResourceIt(FRenderResource::GetResourceList()); ResourceIt; ResourceIt.Next())
		{
			ResourceIt->InitRHI();
		}
		// Dynamic resources can have dependencies on static resources (with uniform buffers) and must initialized last!
		for (TLinkedList<FRenderResource*>::TIterator ResourceIt(FRenderResource::GetResourceList()); ResourceIt; ResourceIt.Next())
		{
			ResourceIt->InitDynamicRHI();
		}

		FHardwareInfo::RegisterHardwareInfo(NAME_RHI, TEXT("Vulkan"));

		GProjectionSignY = 1.0f;

		// Release the early HMD interface used to query extra extensions - if any was used
		HMDVulkanExtensions = nullptr;

		GIsRHIInitialized = true;

		SavePipelineCacheCmd = IConsoleManager::Get().RegisterConsoleCommand(
			TEXT("r.Vulkan.SavePipelineCache"),
			TEXT("Save pipeline cache."),
			FConsoleCommandDelegate::CreateStatic(SavePipelineCache),
			ECVF_Default
			);

		RebuildPipelineCacheCmd = IConsoleManager::Get().RegisterConsoleCommand(
			TEXT("r.Vulkan.RebuildPipelineCache"),
			TEXT("Rebuilds pipeline cache."),
			FConsoleCommandDelegate::CreateStatic(RebuildPipelineCache),
			ECVF_Default
			);

#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
		DumpMemoryCmd = IConsoleManager::Get().RegisterConsoleCommand(
			TEXT("r.Vulkan.DumpMemory"),
			TEXT("Dumps memory map."),
			FConsoleCommandDelegate::CreateStatic(DumpMemory),
			ECVF_Default
			);
#endif
	}
}

void FVulkanCommandListContext::RHIBeginFrame()
{
	check(IsImmediate());
	RHIPrivateBeginFrame();

	//FRCLog::Printf(FString::Printf(TEXT("FVulkanCommandListContext::RHIBeginFrame()")));
	PendingGfxState->GetGlobalUniformPool().BeginFrame();
	PendingComputeState->GetGlobalUniformPool().BeginFrame();

	GpuProfiler.BeginFrame();
}


void FVulkanCommandListContext::RHIBeginScene()
{
	//FRCLog::Printf(FString::Printf(TEXT("FVulkanCommandListContext::RHIBeginScene()")));
}

void FVulkanCommandListContext::RHIEndScene()
{
	//FRCLog::Printf(FString::Printf(TEXT("FVulkanCommandListContext::RHIEndScene()")));
}

void FVulkanCommandListContext::RHIBeginDrawingViewport(FViewportRHIParamRef ViewportRHI, FTextureRHIParamRef RenderTargetRHI)
{
	//FRCLog::Printf(FString::Printf(TEXT("FVulkanCommandListContext::RHIBeginDrawingViewport\n")));
	check(ViewportRHI);
	FVulkanViewport* Viewport = ResourceCast(ViewportRHI);
	RHI->DrawingViewport = Viewport;
}

void FVulkanCommandListContext::RHIEndDrawingViewport(FViewportRHIParamRef ViewportRHI, bool bPresent, bool bLockToVsync)
{
	//FRCLog::Printf(FString::Printf(TEXT("FVulkanCommandListContext::RHIEndDrawingViewport()")));
	check(IsImmediate());
	FVulkanViewport* Viewport = ResourceCast(ViewportRHI);
	check(Viewport == RHI->DrawingViewport);

	//#todo-rco: Unbind all pending state
/*
	check(bPresent);
	RHI->Present();
*/
	FVulkanCmdBuffer* CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();
	check(!CmdBuffer->HasEnded());
	if (CmdBuffer->IsInsideRenderPass())
	{
		TransitionState.EndRenderPass(CmdBuffer);
	}

	WriteEndTimestamp(CmdBuffer);

	bool bNativePresent = Viewport->Present(CmdBuffer, Queue, Device->GetPresentQueue(), bLockToVsync);
	if (bNativePresent)
	{
		//#todo-rco: Check for r.FinishCurrentFrame
	}

	RHI->DrawingViewport = nullptr;

	ReadAndCalculateGPUFrameTime();
	WriteBeginTimestamp(CommandBufferManager->GetActiveCmdBuffer());
}

void FVulkanCommandListContext::RHIEndFrame()
{
	check(IsImmediate());
	//FRCLog::Printf(FString::Printf(TEXT("FVulkanCommandListContext::RHIEndFrame()")));

	
	GetGPUProfiler().EndFrame();

	Device->GetStagingManager().ProcessPendingFree(false, true);
	Device->GetResourceHeapManager().ReleaseFreedPages();


	++FrameCounter;
}

void FVulkanCommandListContext::RHIPushEvent(const TCHAR* Name, FColor Color)
{
	FString EventName = Name;
	EventStack.Add(Name);

	//only valid on immediate context currently.  needs to be fixed for parallel rhi execute
	if (IsImmediate())
	{
		//FRCLog::Printf(FString::Printf(TEXT("RHIPushEvent(%s)"), Name));
#if VULKAN_ENABLE_DUMP_LAYER
		VulkanRHI::PrintfBegin(FString::Printf(TEXT("vkCmdDbgMarkerBeginEXT(%s)"), Name));
#endif
#if VULKAN_ENABLE_DRAW_MARKERS
		if (auto CmdDbgMarkerBegin = Device->GetCmdDbgMarkerBegin())
		{
			VkDebugMarkerMarkerInfoEXT Info;
			FMemory::Memzero(Info);
			Info.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT;
			Info.pMarkerName = TCHAR_TO_ANSI(Name);
			Info.color[0] = Color.R;
			Info.color[1] = Color.G;
			Info.color[2] = Color.B;
			Info.color[3] = Color.A;
			CmdDbgMarkerBegin(GetCommandBufferManager()->GetActiveCmdBuffer()->GetHandle(), &Info);
		}
#endif

		GpuProfiler.PushEvent(Name, Color);
	}
}

void FVulkanCommandListContext::RHIPopEvent()
{
	//only valid on immediate context currently.  needs to be fixed for parallel rhi execute
	if (IsImmediate())
	{
		//FRCLog::Printf(TEXT("RHIPopEvent"));
#if VULKAN_ENABLE_DUMP_LAYER
		VulkanRHI::PrintfBegin(TEXT("vkCmdDbgMarkerEndEXT()"));
#endif
#if VULKAN_ENABLE_DRAW_MARKERS
		if (auto CmdDbgMarkerEnd = Device->GetCmdDbgMarkerEnd())
		{
			CmdDbgMarkerEnd(GetCommandBufferManager()->GetActiveCmdBuffer()->GetHandle());
		}
#endif

		GpuProfiler.PopEvent();
	}

	check(EventStack.Num() > 0);
	EventStack.Pop(false);
}

void FVulkanDynamicRHI::RHIGetSupportedResolution( uint32 &Width, uint32 &Height )
{
}

bool FVulkanDynamicRHI::RHIGetAvailableResolutions(FScreenResolutionArray& Resolutions, bool bIgnoreRefreshRate)
{
	return false;
}

void FVulkanDynamicRHI::RHIFlushResources()
{
}

void FVulkanDynamicRHI::RHIAcquireThreadOwnership()
{
}

void FVulkanDynamicRHI::RHIReleaseThreadOwnership()
{
}

void* FVulkanDynamicRHI::RHIGetNativeDevice()
{
	return (void*)Device->GetInstanceHandle();
}

IRHICommandContext* FVulkanDynamicRHI::RHIGetDefaultContext()
{
	return &Device->GetImmediateContext();
}

IRHIComputeContext* FVulkanDynamicRHI::RHIGetDefaultAsyncComputeContext()
{
	return &Device->GetImmediateComputeContext();
}

IRHICommandContextContainer* FVulkanDynamicRHI::RHIGetCommandContextContainer(int32 Index, int32 Num)
{
	if (GRHIThreadCvar.GetValueOnAnyThread() > 1)
	{
		return new FVulkanCommandContextContainer(Device);
	}

	return nullptr;
}

void FVulkanDynamicRHI::RHISubmitCommandsAndFlushGPU()
{
	Device->SubmitCommandsAndFlushGPU();
}

FTexture2DRHIRef FVulkanDynamicRHI::RHICreateTexture2DFromResource(EPixelFormat Format, uint32 SizeX, uint32 SizeY, uint32 NumMips, uint32 NumSamples, VkImage Resource, uint32 Flags)
{
	return new FVulkanTexture2D(*Device, Format, SizeX, SizeY, NumMips, NumSamples, Resource, Flags, FRHIResourceCreateInfo());
}

FTexture2DArrayRHIRef FVulkanDynamicRHI::RHICreateTexture2DArrayFromResource(EPixelFormat Format, uint32 SizeX, uint32 SizeY, uint32 ArraySize, uint32 NumMips, VkImage Resource, uint32 Flags)
{
	return new FVulkanTexture2DArray(*Device, Format, SizeX, SizeY, ArraySize, NumMips, Resource, Flags, nullptr, FClearValueBinding());
}

FTextureCubeRHIRef FVulkanDynamicRHI::RHICreateTextureCubeFromResource(EPixelFormat Format, uint32 Size, bool bArray, uint32 ArraySize, uint32 NumMips, VkImage Resource, uint32 Flags)
{
	return new FVulkanTextureCube(*Device, Format, Size, bArray, ArraySize, NumMips, Resource, Flags, nullptr, FClearValueBinding());
}

void FVulkanDynamicRHI::RHIAliasTextureResources(FTextureRHIParamRef DestTextureRHI, FTextureRHIParamRef SrcTextureRHI)
{
	if (DestTextureRHI && SrcTextureRHI)
	{
		FVulkanTextureBase* DestTextureBase = (FVulkanTextureBase*) DestTextureRHI->GetTextureBaseRHI();
		FVulkanTextureBase* SrcTextureBase = (FVulkanTextureBase*) SrcTextureRHI->GetTextureBaseRHI();

		if (DestTextureBase && SrcTextureBase)
		{
			DestTextureBase->AliasTextureResources(SrcTextureBase);
		}
	}
}


FVulkanBuffer::FVulkanBuffer(FVulkanDevice& InDevice, uint32 InSize, VkFlags InUsage, VkMemoryPropertyFlags InMemPropertyFlags, bool bInAllowMultiLock, const char* File, int32 Line) :
	Device(InDevice),
	Buf(VK_NULL_HANDLE),
	Allocation(nullptr),
	Size(InSize),
	Usage(InUsage),
	BufferPtr(nullptr),
	bAllowMultiLock(bInAllowMultiLock),
	LockStack(0)
{
	VkBufferCreateInfo BufInfo;
	FMemory::Memzero(BufInfo);
	BufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	BufInfo.size = Size;
	BufInfo.usage = Usage;
	VERIFYVULKANRESULT_EXPANDED(VulkanRHI::vkCreateBuffer(Device.GetInstanceHandle(), &BufInfo, nullptr, &Buf));

	VkMemoryRequirements MemoryRequirements;
	VulkanRHI::vkGetBufferMemoryRequirements(Device.GetInstanceHandle(), Buf, &MemoryRequirements);

	Allocation = InDevice.GetMemoryManager().Alloc(MemoryRequirements.size, MemoryRequirements.memoryTypeBits, InMemPropertyFlags, File ? File : __FILE__, Line ? Line : __LINE__);
	check(Allocation);
	VERIFYVULKANRESULT_EXPANDED(VulkanRHI::vkBindBufferMemory(Device.GetInstanceHandle(), Buf, Allocation->GetHandle(), 0));
}

FVulkanBuffer::~FVulkanBuffer()
{
	// The buffer should be unmapped
	check(BufferPtr == nullptr);

	Device.GetDeferredDeletionQueue().EnqueueResource(FDeferredDeletionQueue::EType::Buffer, Buf);
	Buf = VK_NULL_HANDLE;

	Device.GetMemoryManager().Free(Allocation);
	Allocation = nullptr;
}

void* FVulkanBuffer::Lock(uint32 InSize, uint32 InOffset)
{
	check(InSize + InOffset <= Size);

	// The buffer should be unmapped, before it can be mapped 
	// @todo vulkan: RingBuffer can be mapped multiple times

	uint32 BufferPtrOffset = 0;
	if (bAllowMultiLock)
	{
		if (LockStack == 0)
		{
			// lock the whole range
			BufferPtr = Allocation->Map(GetSize(), 0);
		}
		// offset the whole range by the requested offset
		BufferPtrOffset = InOffset;
		LockStack++;
	}
	else
	{
		check(BufferPtr == nullptr);
		BufferPtr = Allocation->Map(InSize, InOffset);
	}

	return (uint8*)BufferPtr + BufferPtrOffset;
}

void FVulkanBuffer::Unlock()
{
	// The buffer should be mapped, before it can be unmapped
	check(BufferPtr != nullptr);

	// for multi-lock, if not down to 0, do nothing
	if (bAllowMultiLock && --LockStack > 0)
	{
		return;
	}

	Allocation->Unmap();
	BufferPtr = nullptr;
}


FVulkanDescriptorSetsLayout::FVulkanDescriptorSetsLayout(FVulkanDevice* InDevice) :
	Device(InDevice)
{
}

FVulkanDescriptorSetsLayout::~FVulkanDescriptorSetsLayout()
{
	VulkanRHI::FDeferredDeletionQueue& DeletionQueue = Device->GetDeferredDeletionQueue();
	for (VkDescriptorSetLayout& Handle : LayoutHandles)
	{
		DeletionQueue.EnqueueResource(VulkanRHI::FDeferredDeletionQueue::EType::DescriptorSetLayout, Handle);
	}

	LayoutHandles.Reset(0);
}

void FVulkanDescriptorSetsLayoutInfo::AddDescriptor(int32 DescriptorSetIndex, const VkDescriptorSetLayoutBinding& Descriptor, int32 BindingIndex)
{
	// Increment type usage
	LayoutTypes[Descriptor.descriptorType]++;

	if (DescriptorSetIndex >= SetLayouts.Num())
	{
		SetLayouts.SetNum(DescriptorSetIndex + 1, false);
	}

	FSetLayout& DescSetLayout = SetLayouts[DescriptorSetIndex];

	VkDescriptorSetLayoutBinding* Binding = new(DescSetLayout.LayoutBindings) VkDescriptorSetLayoutBinding;
	*Binding = Descriptor;

	// Verify this descriptor doesn't already exist
	for (int32 Index = 0; Index < BindingIndex; ++Index)
	{
		ensure(DescSetLayout.LayoutBindings[Index].binding != BindingIndex || &DescSetLayout.LayoutBindings[Index] != Binding);
	}

	//#todo-rco: Needs a change for the hashing!
	ensure(!Descriptor.pImmutableSamplers);

	Hash = FCrc::MemCrc32(&Binding, sizeof(Binding), Hash);
}

void FVulkanDescriptorSetsLayout::Compile()
{
	check(LayoutHandles.Num() == 0);
	check(Device);

	// Check if we obey limits
	const VkPhysicalDeviceLimits& Limits = Device->GetLimits();
	
	// Check for maxDescriptorSetSamplers
	check(		LayoutTypes[VK_DESCRIPTOR_TYPE_SAMPLER]
			+	LayoutTypes[VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER]
			<	Limits.maxDescriptorSetSamplers);

	// Check for maxDescriptorSetUniformBuffers
	check(		LayoutTypes[VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER]
			+	LayoutTypes[VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC]
			<	Limits.maxDescriptorSetUniformBuffers);

	// Check for maxDescriptorSetUniformBuffersDynamic
	check(		LayoutTypes[VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC]
			<	Limits.maxDescriptorSetUniformBuffersDynamic);

	// Check for maxDescriptorSetStorageBuffers
	check(		LayoutTypes[VK_DESCRIPTOR_TYPE_STORAGE_BUFFER]
			+	LayoutTypes[VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC]
			<	Limits.maxDescriptorSetStorageBuffers);

	// Check for maxDescriptorSetStorageBuffersDynamic
	check(		LayoutTypes[VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC]
			<	Limits.maxDescriptorSetStorageBuffersDynamic);

	// Check for maxDescriptorSetSampledImages
	check(LayoutTypes[VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER]
			+	LayoutTypes[VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE]
			+	LayoutTypes[VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER]
			<	Limits.maxDescriptorSetSampledImages);

	// Check for maxDescriptorSetStorageImages
	check(		LayoutTypes[VK_DESCRIPTOR_TYPE_STORAGE_IMAGE]
			+	LayoutTypes[VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER]
			<	Limits.maxDescriptorSetStorageImages);

	LayoutHandles.Empty(SetLayouts.Num());

	for (FSetLayout& Layout : SetLayouts)
	{
		VkDescriptorSetLayoutCreateInfo DescriptorLayoutInfo;
		FMemory::Memzero(DescriptorLayoutInfo);
		DescriptorLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		DescriptorLayoutInfo.pNext = nullptr;
		DescriptorLayoutInfo.bindingCount = Layout.LayoutBindings.Num();
		DescriptorLayoutInfo.pBindings = Layout.LayoutBindings.GetData();

		VkDescriptorSetLayout* LayoutHandle = new(LayoutHandles) VkDescriptorSetLayout;
		VERIFYVULKANRESULT(VulkanRHI::vkCreateDescriptorSetLayout(Device->GetInstanceHandle(), &DescriptorLayoutInfo, nullptr, LayoutHandle));
	}
}


FVulkanDescriptorSets::FVulkanDescriptorSets(FVulkanDevice* InDevice, const FVulkanDescriptorSetsLayout& InLayout, FVulkanCommandListContext* InContext)
	: Device(InDevice)
	, Pool(nullptr)
	, Layout(InLayout)
{
	const TArray<VkDescriptorSetLayout>& LayoutHandles = Layout.GetHandles();
	if (LayoutHandles.Num() > 0)
	{
		VkDescriptorSetAllocateInfo DescriptorSetAllocateInfo;
		FMemory::Memzero(DescriptorSetAllocateInfo);
		DescriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		// Pool will be filled in by FVulkanCommandListContext::AllocateDescriptorSets
		//DescriptorSetAllocateInfo.descriptorPool = Pool->GetHandle();
		DescriptorSetAllocateInfo.descriptorSetCount = LayoutHandles.Num();
		DescriptorSetAllocateInfo.pSetLayouts = LayoutHandles.GetData();

		Sets.AddZeroed(LayoutHandles.Num());

		Pool = InContext->AllocateDescriptorSets(DescriptorSetAllocateInfo, InLayout, Sets.GetData());
		Pool->TrackAddUsage(Layout);
	}
}

FVulkanDescriptorSets::~FVulkanDescriptorSets()
{
	Pool->TrackRemoveUsage(Layout);

	if (Sets.Num() > 0)
	{
		VERIFYVULKANRESULT(VulkanRHI::vkFreeDescriptorSets(Device->GetInstanceHandle(), Pool->GetHandle(), Sets.Num(), Sets.GetData()));
	}
}

void FVulkanBufferView::Create(FVulkanBuffer& Buffer, EPixelFormat Format, uint32 InOffset, uint32 InSize)
{
	Offset = InOffset;
	Size = InSize;
	check(Format != PF_Unknown);
	const FPixelFormatInfo& FormatInfo = GPixelFormats[Format];
	check(FormatInfo.Supported);

	VkBufferViewCreateInfo ViewInfo;
	FMemory::Memzero(ViewInfo);
	ViewInfo.sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO;
	ViewInfo.buffer = Buffer.GetBufferHandle();
	ViewInfo.format = (VkFormat)FormatInfo.PlatformFormat;
	ViewInfo.offset = Offset;
	ViewInfo.range = Size;
	Flags = Buffer.GetFlags() & VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT;
	check(Flags);

	VERIFYVULKANRESULT(VulkanRHI::vkCreateBufferView(GetParent()->GetInstanceHandle(), &ViewInfo, nullptr, &View));
	INC_DWORD_STAT(STAT_VulkanNumBufferViews);
}

void FVulkanBufferView::Create(FVulkanResourceMultiBuffer* Buffer, EPixelFormat Format, uint32 InOffset, uint32 InSize)
{
	check(Format != PF_Unknown);
	const FPixelFormatInfo& FormatInfo = GPixelFormats[Format];
	check(FormatInfo.Supported);
	Create((VkFormat)FormatInfo.PlatformFormat, Buffer, InOffset, InSize);
}


void FVulkanBufferView::Create(VkFormat Format, FVulkanResourceMultiBuffer* Buffer, uint32 InOffset, uint32 InSize)
{
	Offset = InOffset;
	Size = InSize;
	check(Format != VK_FORMAT_UNDEFINED);

	VkBufferViewCreateInfo ViewInfo;
	FMemory::Memzero(ViewInfo);
	ViewInfo.sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO;
	ViewInfo.buffer = Buffer->GetHandle();
	ViewInfo.format = Format;
	ViewInfo.offset = Offset;
	ViewInfo.range = Size;
	Flags = Buffer->GetBufferUsageFlags() & (VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT);
	check(Flags);

	VERIFYVULKANRESULT(VulkanRHI::vkCreateBufferView(GetParent()->GetInstanceHandle(), &ViewInfo, nullptr, &View));
	INC_DWORD_STAT(STAT_VulkanNumBufferViews);
}

void FVulkanBufferView::Destroy()
{
	if (View != VK_NULL_HANDLE)
	{
		DEC_DWORD_STAT(STAT_VulkanNumBufferViews);
		Device->GetDeferredDeletionQueue().EnqueueResource(FDeferredDeletionQueue::EType::BufferView, View);
		View = VK_NULL_HANDLE;
	}
}

FVulkanRenderPass::FVulkanRenderPass(FVulkanDevice& InDevice, const FVulkanRenderTargetLayout& InRTLayout) :
	Layout(InRTLayout),
#if VULKAN_KEEP_CREATE_INFO
	RTLayout(InRTLayout),
#endif
	RenderPass(VK_NULL_HANDLE),
	NumUsedClearValues(InRTLayout.GetNumUsedClearValues()),
	Device(InDevice)
{
	INC_DWORD_STAT(STAT_VulkanNumRenderPasses);

#if !VULKAN_KEEP_CREATE_INFO
	const FVulkanRenderTargetLayout& RTLayout = InRTLayout;
	VkSubpassDescription SubpassDesc;
#endif
	FMemory::Memzero(SubpassDesc);
	SubpassDesc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	SubpassDesc.flags = 0;
	SubpassDesc.inputAttachmentCount = 0;
	SubpassDesc.pInputAttachments = nullptr;
	SubpassDesc.colorAttachmentCount = RTLayout.GetNumColorAttachments();
	SubpassDesc.pColorAttachments = RTLayout.GetColorAttachmentReferences();
	SubpassDesc.pResolveAttachments = RTLayout.GetResolveAttachmentReferences();
	SubpassDesc.pDepthStencilAttachment = RTLayout.GetDepthStencilAttachmentReference();
	SubpassDesc.preserveAttachmentCount = 0;
	SubpassDesc.pPreserveAttachments = nullptr;

#if !VULKAN_KEEP_CREATE_INFO
	VkRenderPassCreateInfo CreateInfo;
#endif
	FMemory::Memzero(CreateInfo);
	CreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	CreateInfo.pNext = nullptr;
	CreateInfo.attachmentCount = RTLayout.GetNumAttachmentDescriptions();
	CreateInfo.pAttachments = RTLayout.GetAttachmentDescriptions();
	CreateInfo.subpassCount = 1;
	CreateInfo.pSubpasses = &SubpassDesc;
	CreateInfo.dependencyCount = 0;
	CreateInfo.pDependencies = nullptr;

	VERIFYVULKANRESULT_EXPANDED(VulkanRHI::vkCreateRenderPass(Device.GetInstanceHandle(), &CreateInfo, nullptr, &RenderPass));
}

FVulkanRenderPass::~FVulkanRenderPass()
{
	DEC_DWORD_STAT(STAT_VulkanNumRenderPasses);

	Device.GetDeferredDeletionQueue().EnqueueResource(FDeferredDeletionQueue::EType::RenderPass, RenderPass);
	RenderPass = VK_NULL_HANDLE;
}


void VulkanSetImageLayout(
	VkCommandBuffer CmdBuffer,
	VkImage Image,
	VkImageLayout OldLayout,
	VkImageLayout NewLayout,
	const VkImageSubresourceRange& SubresourceRange)
{
	VkImageMemoryBarrier ImageBarrier;
	FMemory::Memzero(ImageBarrier);
	ImageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	ImageBarrier.oldLayout = OldLayout;
	ImageBarrier.newLayout = NewLayout;
	ImageBarrier.image = Image;
	ImageBarrier.subresourceRange = SubresourceRange;
	ImageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	ImageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

	ImageBarrier.srcAccessMask = VulkanRHI::GetAccessMask(OldLayout);
	ImageBarrier.dstAccessMask = VulkanRHI::GetAccessMask(NewLayout);

	VkPipelineStageFlags SourceStages = VulkanRHI::GetStageFlags(OldLayout);
	VkPipelineStageFlags DestStages = VulkanRHI::GetStageFlags(NewLayout);

	VulkanRHI::vkCmdPipelineBarrier(CmdBuffer, SourceStages, DestStages, 0, 0, nullptr, 0, nullptr, 1, &ImageBarrier);
}

void VulkanResolveImage(VkCommandBuffer Cmd, FTextureRHIParamRef SourceTextureRHI, FTextureRHIParamRef DestTextureRHI)
{
	FVulkanTextureBase* Src = FVulkanTextureBase::Cast(SourceTextureRHI);
	FVulkanTextureBase* Dst = FVulkanTextureBase::Cast(DestTextureRHI);

	const VkImageAspectFlags AspectMask = Src->Surface.GetPartialAspectMask();
	check(AspectMask == Dst->Surface.GetPartialAspectMask());

	VkImageResolve ResolveDesc;
	FMemory::Memzero(ResolveDesc);
	ResolveDesc.srcSubresource.aspectMask = AspectMask;
	ResolveDesc.srcSubresource.baseArrayLayer = 0;
	ResolveDesc.srcSubresource.mipLevel = 0;
	ResolveDesc.srcSubresource.layerCount = 1;
	ResolveDesc.srcOffset.x = 0;
	ResolveDesc.srcOffset.y = 0;
	ResolveDesc.srcOffset.z = 0;
	ResolveDesc.dstSubresource.aspectMask = AspectMask;
	ResolveDesc.dstSubresource.baseArrayLayer = 0;
	ResolveDesc.dstSubresource.mipLevel = 0;
	ResolveDesc.dstSubresource.layerCount = 1;
	ResolveDesc.dstOffset.x = 0;
	ResolveDesc.dstOffset.y = 0;
	ResolveDesc.dstOffset.z = 0;
	ResolveDesc.extent.width = Src->Surface.Width;
	ResolveDesc.extent.height = Src->Surface.Height;
	ResolveDesc.extent.depth = 1;

	VulkanRHI::vkCmdResolveImage(Cmd,
		Src->Surface.Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		Dst->Surface.Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1, &ResolveDesc);
}

FVulkanRingBuffer::FVulkanRingBuffer(FVulkanDevice* InDevice, uint64 TotalSize, VkFlags Usage, VkMemoryPropertyFlags MemPropertyFlags)
	: VulkanRHI::FDeviceChild(InDevice)
	, BufferSize(TotalSize)
	, BufferOffset(0)
	, MinAlignment(0)
{
	FRHIResourceCreateInfo CreateInfo;
	BufferSuballocation = InDevice->GetResourceHeapManager().AllocateBuffer(TotalSize, Usage, MemPropertyFlags, __FILE__, __LINE__);
	MinAlignment = BufferSuballocation->GetBufferAllocation()->GetAlignment();
}

FVulkanRingBuffer::~FVulkanRingBuffer()
{
	delete BufferSuballocation;
}

uint64 FVulkanRingBuffer::AllocateMemory(uint64 Size, uint32 Alignment)
{
	Alignment = FMath::Max(Alignment, MinAlignment);
	uint64 AllocOffset = Align<uint64>(BufferOffset, Alignment);

	// wrap around if needed
	if (AllocOffset + Size >= BufferSize)
	{
		//UE_LOG(LogVulkanRHI, Display, TEXT("Wrapped around the ring buffer. Frame = %lld, size = %lld"), Device->FrameCounter, BufferSize);
		AllocOffset = 0;
	}

	// point to location after this guy
	BufferOffset = AllocOffset + Size;

	return AllocOffset;
}

void FVulkanDynamicRHI::SavePipelineCache()
{
	FString CacheFile = GetPipelineCacheFilename();

	FVulkanDynamicRHI* RHI = (FVulkanDynamicRHI*)GDynamicRHI;
	RHI->Device->PipelineStateCache->Save(CacheFile);
}

void FVulkanDynamicRHI::RebuildPipelineCache()
{
	FVulkanDynamicRHI* RHI = (FVulkanDynamicRHI*)GDynamicRHI;
	RHI->Device->PipelineStateCache->RebuildCache();
}

#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
void FVulkanDynamicRHI::DumpMemory()
{
	FVulkanDynamicRHI* RHI = (FVulkanDynamicRHI*)GDynamicRHI;
	RHI->Device->GetMemoryManager().DumpMemory();
	RHI->Device->GetResourceHeapManager().DumpMemory();
	RHI->Device->GetStagingManager().DumpMemory();
}
#endif

void FVulkanDynamicRHI::RecreateSwapChain(void* NewNativeWindow)
{
	if (NewNativeWindow)
	{
		FlushRenderingCommands();
		FVulkanDynamicRHI* RHI = (FVulkanDynamicRHI*)GDynamicRHI;
		TArray<FVulkanViewport*> Viewports = RHI->Viewports;
		ENQUEUE_RENDER_COMMAND(VulkanRecreateSwapChain)(
			[Viewports, NewNativeWindow](FRHICommandListImmediate& RHICmdList)
			{
				for (auto& Viewport : Viewports)
				{
					Viewport->RecreateSwapchain(NewNativeWindow);
				}
			});
		FlushRenderingCommands();
	}
}

void FVulkanDynamicRHI::VulkanSetImageLayout( VkCommandBuffer CmdBuffer, VkImage Image, VkImageLayout OldLayout, VkImageLayout NewLayout, const VkImageSubresourceRange& SubresourceRange )
{
	::VulkanSetImageLayout( CmdBuffer, Image, OldLayout, NewLayout, SubresourceRange );
}

#undef LOCTEXT_NAMESPACE
