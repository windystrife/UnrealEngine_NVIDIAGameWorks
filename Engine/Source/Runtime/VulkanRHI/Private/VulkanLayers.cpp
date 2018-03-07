// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanLayers.cpp: Vulkan device layers implementation.
=============================================================================*/

#include "VulkanRHIPrivate.h"
#include "IHeadMountedDisplayModule.h"

#if PLATFORM_LINUX
#include <SDL.h>
#endif

TAutoConsoleVariable<int32> GValidationCvar(
	TEXT("r.Vulkan.EnableValidation"),
	0,
	TEXT("0 to disable validation layers (default)\n")
	TEXT("1 to enable errors\n")
	TEXT("2 to enable errors & warnings\n")
	TEXT("3 to enable errors, warnings & performance warnings\n")
	TEXT("4 to enable errors, warnings, performance & information messages\n")
	TEXT("5 to enable all messages"),
	ECVF_ReadOnly | ECVF_RenderThreadSafe
);

#if VULKAN_HAS_DEBUGGING_ENABLED

	#if VULKAN_ENABLE_DRAW_MARKERS
		#define RENDERDOC_LAYER_NAME	"VK_LAYER_RENDERDOC_Capture"
	#endif

// List of validation layers which we want to activate for the instance
static const ANSICHAR* GRequiredLayersInstance[] =
{
	nullptr,
};

// Disable to be able to selectively choose which validation layers you want
#define VULKAN_ENABLE_STANDARD_VALIDATION	0

// List of validation layers which we want to activate for the instance
static const ANSICHAR* GValidationLayersInstance[] =
{
#if VULKAN_ENABLE_STANDARD_VALIDATION
	"VK_LAYER_LUNARG_standard_validation",
#else
	"VK_LAYER_GOOGLE_threading",
	"VK_LAYER_LUNARG_parameter_validation",
	"VK_LAYER_LUNARG_object_tracker",
#if defined(VK_HEADER_VERSION) && (VK_HEADER_VERSION < 42)
	"VK_LAYER_LUNARG_image",
#endif
	"VK_LAYER_LUNARG_core_validation",
#if defined(VK_HEADER_VERSION) && (VK_HEADER_VERSION < 51)
	"VK_LAYER_LUNARG_swapchain",
#endif
	"VK_LAYER_GOOGLE_unique_objects",
#endif

	//"VK_LAYER_LUNARG_screenshot",
	//"VK_LAYER_NV_optimus",
	//"VK_LAYER_LUNARG_vktrace",		// Useful for future
	nullptr
};

// List of validation layers which we want to activate for the device
static const ANSICHAR* GRequiredLayersDevice[] =
{
	nullptr,
};

// List of validation layers which we want to activate for the device
static const ANSICHAR* GValidationLayersDevice[] =
{
	// Only have device validation layers on SDKs below 13
#if defined(VK_HEADER_VERSION) && (VK_HEADER_VERSION < 13)
#if VULKAN_ENABLE_STANDARD_VALIDATION
	"VK_LAYER_LUNARG_standard_validation",
#else
	"VK_LAYER_GOOGLE_threading",
	"VK_LAYER_LUNARG_parameter_validation",
	"VK_LAYER_LUNARG_object_tracker",
	"VK_LAYER_LUNARG_image",
	"VK_LAYER_LUNARG_core_validation",
	"VK_LAYER_LUNARG_swapchain",
	"VK_LAYER_GOOGLE_unique_objects",
	"VK_LAYER_LUNARG_core_validation",
#endif	// Standard Validation
#endif	// Header < 13
	//"VK_LAYER_LUNARG_screenshot",
	//"VK_LAYER_NV_optimus",
	//"VK_LAYER_NV_nsight",
	//"VK_LAYER_LUNARG_vktrace",		// Useful for future
	nullptr
};
#endif // VULKAN_HAS_DEBUGGING_ENABLED

// Instance Extensions to enable
static const ANSICHAR* GInstanceExtensions[] =
{
#if !PLATFORM_LINUX
	VK_KHR_SURFACE_EXTENSION_NAME,
#endif
#if PLATFORM_ANDROID
	VK_KHR_ANDROID_SURFACE_EXTENSION_NAME,
#elif PLATFORM_LINUX
//	VK_KHR_XLIB_SURFACE_EXTENSION_NAME,
#elif PLATFORM_WINDOWS
	VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#endif
#if VULKAN_HAS_DEBUGGING_ENABLED
	VK_EXT_DEBUG_REPORT_EXTENSION_NAME,
#endif
#if VULKAN_ENABLE_DESKTOP_HMD_SUPPORT
	VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME,
	VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
#endif
	nullptr
};

// Device Extensions to enable
static const ANSICHAR* GDeviceExtensions[] =
{
	//	VK_KHR_SURFACE_EXTENSION_NAME,			// Not supported, even if it's reported as a valid extension... (SDK/driver bug?)
#if PLATFORM_ANDROID
	VK_KHR_SURFACE_EXTENSION_NAME,
	VK_KHR_ANDROID_SURFACE_EXTENSION_NAME,
#else
	//	VK_KHR_WIN32_SURFACE_EXTENSION_NAME,	// Not supported, even if it's reported as a valid extension... (SDK/driver bug?)
#endif
	VK_KHR_SWAPCHAIN_EXTENSION_NAME,

	//"VK_KHX_device_group",
#if SUPPORTS_MAINTENANCE_LAYER
	VK_KHR_MAINTENANCE1_EXTENSION_NAME,
#endif
	VK_KHR_SAMPLER_MIRROR_CLAMP_TO_EDGE_EXTENSION_NAME,
	nullptr
};

TSharedPtr< IHeadMountedDisplayVulkanExtensions, ESPMode::ThreadSafe > FVulkanDynamicRHI::HMDVulkanExtensions;

struct FLayerExtension
{
	FLayerExtension()
	{
		FMemory::Memzero(LayerProps);
	}

	VkLayerProperties LayerProps;
	TArray<VkExtensionProperties> ExtensionProps;
};

static inline void GetInstanceLayerExtensions(const ANSICHAR* LayerName, FLayerExtension& OutLayer)
{
	VkResult Result;
	do
	{
		//@TODO: Currently unsupported on device, so just make sure it doesn't cause problems
		uint32 Count = 0;
		Result = VulkanRHI::vkEnumerateInstanceExtensionProperties(LayerName, &Count, nullptr);
		check(Result >= VK_SUCCESS);

		if (Count > 0)
		{
			OutLayer.ExtensionProps.Empty(Count);
			OutLayer.ExtensionProps.AddUninitialized(Count);
			Result = VulkanRHI::vkEnumerateInstanceExtensionProperties(LayerName, &Count, OutLayer.ExtensionProps.GetData());
			check(Result >= VK_SUCCESS);
		}
	}
	while (Result == VK_INCOMPLETE);
}

static inline void GetDeviceLayerExtensions(VkPhysicalDevice Device, const ANSICHAR* LayerName, FLayerExtension& OutLayer)
{
	VkResult Result;
	do
	{
		uint32 Count = 0;
		Result = VulkanRHI::vkEnumerateDeviceExtensionProperties(Device, LayerName, &Count, nullptr);
		check(Result >= VK_SUCCESS);

		if (Count > 0)
		{
			OutLayer.ExtensionProps.Empty(Count);
			OutLayer.ExtensionProps.AddUninitialized(Count);
			Result = VulkanRHI::vkEnumerateDeviceExtensionProperties(Device, LayerName, &Count, OutLayer.ExtensionProps.GetData());
			check(Result >= VK_SUCCESS);
		}
	}
	while (Result == VK_INCOMPLETE);
}


void FVulkanDynamicRHI::GetInstanceLayersAndExtensions(TArray<const ANSICHAR*>& OutInstanceExtensions, TArray<const ANSICHAR*>& OutInstanceLayers)
{
	TArray<FLayerExtension> GlobalLayers;
	FLayerExtension GlobalExtensions;

	VkResult Result;

	// Global extensions
	FMemory::Memzero(GlobalExtensions.LayerProps);
	GetInstanceLayerExtensions(nullptr, GlobalExtensions);

	for (int32 Index = 0; Index < GlobalExtensions.ExtensionProps.Num(); ++Index)
	{
		UE_LOG(LogVulkanRHI, Display, TEXT("- Found Instance Layer %s"), ANSI_TO_TCHAR(GlobalExtensions.ExtensionProps[Index].extensionName));
	}


	// Now per layer
	TArray<VkLayerProperties> GlobalLayerProperties;
	do
	{
		uint32 InstanceLayerCount = 0;
		Result = VulkanRHI::vkEnumerateInstanceLayerProperties(&InstanceLayerCount, nullptr);
		check(Result >= VK_SUCCESS);

		if (InstanceLayerCount > 0)
		{
			GlobalLayers.Empty(InstanceLayerCount);
			GlobalLayerProperties.AddZeroed(InstanceLayerCount);
			Result = VulkanRHI::vkEnumerateInstanceLayerProperties(&InstanceLayerCount, &GlobalLayerProperties[GlobalLayerProperties.Num() - InstanceLayerCount]);
			check(Result >= VK_SUCCESS);
		}
	}
	while (Result == VK_INCOMPLETE);

	for (int32 Index = 0; Index < GlobalLayerProperties.Num(); ++Index)
	{
		FLayerExtension* Layer = new(GlobalLayers) FLayerExtension;
		Layer->LayerProps = GlobalLayerProperties[Index];
		GetInstanceLayerExtensions(GlobalLayerProperties[Index].layerName, *Layer);
		UE_LOG(LogVulkanRHI, Display, TEXT("- Found global layer %s"), ANSI_TO_TCHAR(GlobalLayerProperties[Index].layerName));
	}

#if VULKAN_HAS_DEBUGGING_ENABLED
	// Verify that all required instance layers are available
	for (uint32 LayerIndex = 0; LayerIndex < ARRAY_COUNT(GRequiredLayersInstance); ++LayerIndex)
	{
		const ANSICHAR* CurrValidationLayer = GRequiredLayersInstance[LayerIndex];
		if (CurrValidationLayer)
		{
			bool bValidationFound = false;
			for (int32 Index = 0; Index < GlobalLayers.Num(); ++Index)
			{
				if (!FCStringAnsi::Strcmp(GlobalLayers[Index].LayerProps.layerName, CurrValidationLayer))
				{
					bValidationFound = true;
					OutInstanceLayers.Add(CurrValidationLayer);
					break;
				}
			}

			if (!bValidationFound)
			{
				UE_LOG(LogVulkanRHI, Warning, TEXT("Unable to find Vulkan required instance layer '%s'"), ANSI_TO_TCHAR(CurrValidationLayer));
			}
		}
	}

	if (GValidationCvar.GetValueOnAnyThread() > 0)
	{
		// Verify that all requested debugging device-layers are available
		for (uint32 LayerIndex = 0; GValidationLayersInstance[LayerIndex] != nullptr; ++LayerIndex)
		{
			bool bValidationFound = false;
			const ANSICHAR* CurrValidationLayer = GValidationLayersInstance[LayerIndex];
			for (int32 Index = 0; Index < GlobalLayers.Num(); ++Index)
			{
				if (!FCStringAnsi::Strcmp(GlobalLayers[Index].LayerProps.layerName, CurrValidationLayer))
				{
					bValidationFound = true;
					OutInstanceLayers.Add(CurrValidationLayer);
					break;
				}
			}

			if (!bValidationFound)
			{
				UE_LOG(LogVulkanRHI, Warning, TEXT("Unable to find Vulkan instance validation layer '%s'"), ANSI_TO_TCHAR(CurrValidationLayer));
			}
		}
	}

#if VULKAN_ENABLE_API_DUMP
	{
		bool bApiDumpFound = false;
		const char* ApiDumpName = "VK_LAYER_LUNARG_api_dump";
		for (int32 Index = 0; Index < GlobalLayers.Num(); ++Index)
		{
			if (!FCStringAnsi::Strcmp(GlobalLayers[Index].LayerProps.layerName, ApiDumpName))
			{
				bApiDumpFound = true;
				OutInstanceLayers.Add(ApiDumpName);
				break;
			}
		}
		if (!bApiDumpFound)
		{
			UE_LOG(LogVulkanRHI, Warning, TEXT("Unable to find Vulkan instance layer %s"), ANSI_TO_TCHAR(ApiDumpName));
		}
	}
#endif	// VULKAN_ENABLE_API_DUMP
#endif	// VULKAN_HAS_DEBUGGING_ENABLED

#if PLATFORM_LINUX
	uint32_t Count = 0;
	auto RequiredExtensions = SDL_VK_GetRequiredInstanceExtensions(&Count);
	for(int32 i = 0; i < Count; i++)
	{
		OutInstanceExtensions.Add(RequiredExtensions[i]);
	}
#endif

	// Check to see if the HMD requires any specific Vulkan extensions to operate
	if (IHeadMountedDisplayModule::IsAvailable())
	{
		HMDVulkanExtensions = IHeadMountedDisplayModule::Get().GetVulkanExtensions();
	
		if (HMDVulkanExtensions.IsValid())
		{
			if (!HMDVulkanExtensions->GetVulkanInstanceExtensionsRequired(OutInstanceExtensions))
			{
				UE_LOG(LogVulkanRHI, Warning, TEXT("Trying to use Vulkan with an HMD, but required extensions aren't supported!"));
			}
		}
	}

	for (int32 i = 0; i < GlobalExtensions.ExtensionProps.Num(); i++)
	{
		for (int32 j = 0; GInstanceExtensions[j] != nullptr; j++)
		{
			if (!FCStringAnsi::Strcmp(GlobalExtensions.ExtensionProps[i].extensionName, GInstanceExtensions[j]))
			{
				OutInstanceExtensions.Add(GInstanceExtensions[j]);
				break;
			}
		}
	}

	if (OutInstanceExtensions.Num() > 0)
	{
		UE_LOG(LogVulkanRHI, Display, TEXT("Using instance extensions"));
		for (const ANSICHAR* Extension : OutInstanceExtensions)
		{
			UE_LOG(LogVulkanRHI, Display, TEXT("* %s"), ANSI_TO_TCHAR(Extension));
		}
	}

	if (OutInstanceLayers.Num() > 0)
	{
		UE_LOG(LogVulkanRHI, Display, TEXT("Using instance layers"));
		for (const ANSICHAR* Layer : OutInstanceLayers)
		{
			UE_LOG(LogVulkanRHI, Display, TEXT("* %s"), ANSI_TO_TCHAR(Layer));
		}
	}
}

void FVulkanDevice::GetDeviceExtensions(TArray<const ANSICHAR*>& OutDeviceExtensions, TArray<const ANSICHAR*>& OutDeviceLayers, bool& bOutDebugMarkers)
{
	bOutDebugMarkers = false;

	// Setup device layer properties
	TArray<VkLayerProperties> LayerProperties;
	{
		uint32 Count = 0;
		VERIFYVULKANRESULT(VulkanRHI::vkEnumerateDeviceLayerProperties(Gpu, &Count, nullptr));
		LayerProperties.AddZeroed(Count);
		VERIFYVULKANRESULT(VulkanRHI::vkEnumerateDeviceLayerProperties(Gpu, &Count, (VkLayerProperties*)LayerProperties.GetData()));
		check(Count == LayerProperties.Num());
	}

	for (int32 Index = 0; Index < LayerProperties.Num(); ++Index)
	{
		UE_LOG(LogVulkanRHI, Display, TEXT("- Found Device Layer %s"), ANSI_TO_TCHAR(LayerProperties[Index].layerName));
	}

#if VULKAN_HAS_DEBUGGING_ENABLED
	bool bRenderDocFound = false;
	#if VULKAN_ENABLE_DRAW_MARKERS
		bool bDebugExtMarkerFound = false;
		for (int32 Index = 0; Index < LayerProperties.Num(); ++Index)
		{
			if (!FCStringAnsi::Strcmp(LayerProperties[Index].layerName, RENDERDOC_LAYER_NAME))
			{
				bRenderDocFound = true;
				break;
			}
			else if (!FCStringAnsi::Strcmp(LayerProperties[Index].layerName, VK_EXT_DEBUG_MARKER_EXTENSION_NAME))
			{
				bDebugExtMarkerFound = true;
				break;
			}
		}
	#endif

	// Verify that all required device layers are available
	// ARRAY_COUNT is unnecessary, but MSVC analyzer doesn't seem to be happy with just a null check
	for (uint32 LayerIndex = 0; LayerIndex < ARRAY_COUNT(GRequiredLayersDevice) && GRequiredLayersDevice[LayerIndex] != nullptr; ++LayerIndex)
	{
		const ANSICHAR* CurrValidationLayer = GRequiredLayersDevice[LayerIndex];
		if (CurrValidationLayer)
		{
			bool bValidationFound = false;
			for (int32 Index = 0; Index < LayerProperties.Num(); ++Index)
			{
				if (!FCStringAnsi::Strcmp(LayerProperties[Index].layerName, CurrValidationLayer))
				{
					bValidationFound = true;
					OutDeviceLayers.Add(CurrValidationLayer);
					break;
				}
			}

			if (!bValidationFound)
			{
				UE_LOG(LogVulkanRHI, Warning, TEXT("Unable to find Vulkan required device layer '%s'"), ANSI_TO_TCHAR(CurrValidationLayer));
			}
		}
	}

	// Verify that all requested debugging device-layers are available. Skip validation layers under RenderDoc
	if (!bRenderDocFound && GValidationCvar.GetValueOnAnyThread() > 0)
	{
		for (uint32 LayerIndex = 0; GValidationLayersDevice[LayerIndex] != nullptr; ++LayerIndex)
		{
			bool bValidationFound = false;
			const ANSICHAR* CurrValidationLayer = GValidationLayersDevice[LayerIndex];
			for (int32 Index = 0; Index < LayerProperties.Num(); ++Index)
			{
				if (!FCStringAnsi::Strcmp(LayerProperties[Index].layerName, CurrValidationLayer))
				{
					bValidationFound = true;
					OutDeviceLayers.Add(CurrValidationLayer);
					break;
				}
			}

			if (!bValidationFound)
			{
				UE_LOG(LogVulkanRHI, Warning, TEXT("Unable to find Vulkan device validation layer '%s'"), ANSI_TO_TCHAR(CurrValidationLayer));
			}
		}
	}
#endif	// VULKAN_HAS_DEBUGGING_ENABLED

	FLayerExtension Extensions;
	FMemory::Memzero(Extensions.LayerProps);
	GetDeviceLayerExtensions(Gpu, nullptr, Extensions);

	for (int32 Index = 0; Index < Extensions.ExtensionProps.Num(); ++Index)
	{
		UE_LOG(LogVulkanRHI, Display, TEXT("- Found Device Extension %s"), ANSI_TO_TCHAR(Extensions.ExtensionProps[Index].extensionName));
	}

	if ( FVulkanDynamicRHI::HMDVulkanExtensions.IsValid() )
	{
		if ( !FVulkanDynamicRHI::HMDVulkanExtensions->GetVulkanDeviceExtensionsRequired( Gpu, OutDeviceExtensions ) )
		{
			UE_LOG( LogVulkanRHI, Warning, TEXT( "Trying to use Vulkan with an HMD, but required extensions aren't supported on the selected device!" ) );
		}
	}

	// ARRAY_COUNT is unnecessary, but MSVC analyzer doesn't seem to be happy with just a null check
	for (uint32 Index = 0; Index < ARRAY_COUNT(GDeviceExtensions) && GDeviceExtensions[Index] != nullptr; ++Index)
	{
		for (int32 i = 0; i < Extensions.ExtensionProps.Num(); i++)
		{
			if (!FCStringAnsi::Strcmp(GDeviceExtensions[Index], Extensions.ExtensionProps[i].extensionName))
			{
				OutDeviceExtensions.Add(GDeviceExtensions[Index]);
				break;
			}
		}
	}

#if VULKAN_HAS_DEBUGGING_ENABLED
	#if VULKAN_ENABLE_DRAW_MARKERS
	{
		for (int32 i = 0; i < Extensions.ExtensionProps.Num(); i++)
		{
			if (!FCStringAnsi::Strcmp(Extensions.ExtensionProps[i].extensionName, VK_EXT_DEBUG_MARKER_EXTENSION_NAME))
			{
				OutDeviceExtensions.Add(VK_EXT_DEBUG_MARKER_EXTENSION_NAME);
				bOutDebugMarkers = true;
				break;
			}
		}
	}
	#endif
#endif
	if (OutDeviceExtensions.Num() > 0)
	{
		UE_LOG(LogVulkanRHI, Display, TEXT("Using device extensions"));
		for (const ANSICHAR* Extension : OutDeviceExtensions)
		{
			UE_LOG(LogVulkanRHI, Display, TEXT("* %s"), ANSI_TO_TCHAR(Extension));
		}
	}

	if (OutDeviceLayers.Num() > 0)
	{
		UE_LOG(LogVulkanRHI, Display, TEXT("Using device layers"));
		for (const ANSICHAR* Layer : OutDeviceLayers)
		{
			UE_LOG(LogVulkanRHI, Display, TEXT("* %s"), ANSI_TO_TCHAR(Layer));
		}
	}
}


void FVulkanDevice::ParseOptionalDeviceExtensions(const TArray<const ANSICHAR *>& DeviceExtensions)
{
	FMemory::Memzero(OptionalDeviceExtensions);

	auto HasExtension = [&DeviceExtensions](const ANSICHAR* InName) -> bool
	{
		return DeviceExtensions.ContainsByPredicate(
			[&InName](const ANSICHAR* InExtension) -> bool
			{
				return FCStringAnsi::Strcmp(InExtension, InName) == 0;
			}
		);
	};
#if SUPPORTS_MAINTENANCE_LAYER
	OptionalDeviceExtensions.HasKHRMaintenance1 = HasExtension(VK_KHR_MAINTENANCE1_EXTENSION_NAME);
#endif
	OptionalDeviceExtensions.HasMirrorClampToEdge = HasExtension(VK_KHR_SAMPLER_MIRROR_CLAMP_TO_EDGE_EXTENSION_NAME);

#if VULKAN_ENABLE_DESKTOP_HMD_SUPPORT
	OptionalDeviceExtensions.HasKHRExternalMemoryCapabilities = HasExtension(VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME);
	OptionalDeviceExtensions.HasKHRGetPhysicalDeviceProperties2 = HasExtension(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
#endif

#if !PLATFORM_ANDROID
	// Verify the assumption on FVulkanSamplerState::FVulkanSamplerState()!
	ensure(OptionalDeviceExtensions.HasMirrorClampToEdge != 0);
#endif
}
