// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "OculusHMDPrivate.h"
#include "IHeadMountedDisplayVulkanExtensions.h"

#if OCULUS_HMD_SUPPORTED_PLATFORMS

namespace OculusHMD
{


//-------------------------------------------------------------------------------------------------
// FVulkanExtensions
//-------------------------------------------------------------------------------------------------

class FVulkanExtensions : public IHeadMountedDisplayVulkanExtensions, public TSharedFromThis<FVulkanExtensions, ESPMode::ThreadSafe>
{
public:
	FVulkanExtensions() {}
	virtual ~FVulkanExtensions() {}

	// IHeadMountedDisplayVulkanExtensions
	virtual bool GetVulkanInstanceExtensionsRequired(TArray<const ANSICHAR*>& Out) override;
	virtual bool GetVulkanDeviceExtensionsRequired(struct VkPhysicalDevice_T *pPhysicalDevice, TArray<const ANSICHAR*>& Out) override;
};

} // namespace OculusHMD

#endif //OCULUS_HMD_SUPPORTED_PLATFORMS