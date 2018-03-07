// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "OculusHMD_VulkanExtensions.h"

#if OCULUS_HMD_SUPPORTED_PLATFORMS
#include "OculusHMDPrivateRHI.h"

namespace OculusHMD
{


//-------------------------------------------------------------------------------------------------
// FVulkanExtensions
//-------------------------------------------------------------------------------------------------

bool FVulkanExtensions::GetVulkanInstanceExtensionsRequired(TArray<const ANSICHAR*>& Out)
{
#if OCULUS_HMD_SUPPORTED_PLATFORMS_VULKAN && PLATFORM_WINDOWS
	uint32_t PropertyCount;
	VulkanRHI::vkEnumerateInstanceExtensionProperties(nullptr, &PropertyCount, nullptr);

	TArray<VkExtensionProperties> Properties;
	Properties.SetNum(PropertyCount);
	VulkanRHI::vkEnumerateInstanceExtensionProperties(nullptr, &PropertyCount, Properties.GetData());

	static const char* ExtensionNames[] =
	{
		VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME,
		VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
	};

	static uint32_t ExtensionCount = sizeof(ExtensionNames) / sizeof(ExtensionNames[0]);
	uint32_t ExtensionsFound = 0;

	for (uint32_t ExtensionIndex = 0; ExtensionIndex < ExtensionCount; ExtensionIndex++)
	{
		for (uint32_t PropertyIndex = 0; PropertyIndex < PropertyCount; PropertyIndex++)
		{
			const char* PropertyExtensionName = Properties[PropertyIndex].extensionName;

			if (!FCStringAnsi::Strcmp(PropertyExtensionName, ExtensionNames[ExtensionIndex]))
			{
				Out.Add(ExtensionNames[ExtensionIndex]);
				ExtensionsFound++;
				break;
			}
		}
	}

	return ExtensionsFound == ExtensionCount;
#endif
	return true;
}


bool FVulkanExtensions::GetVulkanDeviceExtensionsRequired(struct VkPhysicalDevice_T *pPhysicalDevice, TArray<const ANSICHAR*>& Out)
{
#if OCULUS_HMD_SUPPORTED_PLATFORMS_VULKAN && PLATFORM_WINDOWS
	uint32_t PropertyCount;
	VulkanRHI::vkEnumerateDeviceExtensionProperties((VkPhysicalDevice) pPhysicalDevice, nullptr, &PropertyCount, nullptr);

	TArray<VkExtensionProperties> Properties;
	Properties.SetNum(PropertyCount);
	VulkanRHI::vkEnumerateDeviceExtensionProperties((VkPhysicalDevice) pPhysicalDevice, nullptr, &PropertyCount, Properties.GetData());

	static const char* ExtensionNames[] =
	{
		VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME,
		VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME,
	};

	static uint32_t ExtensionCount = sizeof(ExtensionNames) / sizeof(ExtensionNames[0]);
	uint32_t ExtensionsFound = 0;

	for (uint32_t ExtensionIndex = 0; ExtensionIndex < ExtensionCount; ExtensionIndex++)
	{
		for (uint32_t PropertyIndex = 0; PropertyIndex < PropertyCount; PropertyIndex++)
		{
			const char* PropertyExtensionName = Properties[PropertyIndex].extensionName;

			if (!FCStringAnsi::Strcmp(PropertyExtensionName, ExtensionNames[ExtensionIndex]))
			{
				Out.Add(ExtensionNames[ExtensionIndex]);
				ExtensionsFound++;
				break;
			}
		}
	}

	return ExtensionsFound == ExtensionCount;
#endif
	return true;
}


} // namespace OculusHMD

#endif //OCULUS_HMD_SUPPORTED_PLATFORMS
