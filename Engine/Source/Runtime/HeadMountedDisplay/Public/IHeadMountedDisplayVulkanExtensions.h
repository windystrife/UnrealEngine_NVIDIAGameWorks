// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
* Query Vulkan extensions required by the HMD.
*
* Knowledge of these extensions is required during Vulkan RHI initialization.
*/
class IHeadMountedDisplayVulkanExtensions
{
public:
	virtual bool GetVulkanInstanceExtensionsRequired(TArray<const ANSICHAR*>& Out) = 0;
	virtual bool GetVulkanDeviceExtensionsRequired(struct VkPhysicalDevice_T *pPhysicalDevice, TArray<const ANSICHAR*>& Out) = 0;
};