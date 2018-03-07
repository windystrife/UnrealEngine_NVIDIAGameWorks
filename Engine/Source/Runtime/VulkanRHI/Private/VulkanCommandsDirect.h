// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanCommandsDirect.h: Enumerate the VulkanDynamicAPI.  Doing this in a header to avoid static analysis warnings.
=============================================================================*/

#pragma once 

#if VULKAN_COMMANDWRAPPERS_ENABLE
	#error "VulkanCommandsDirect should not be used when CommandWrappers are enabled."
#endif
#if !VULKAN_DYNAMICALLYLOADED
	#error "VulkanCommandsDirect is for the Dynamic api."
#endif

// Bring functions from VulkanDynamicAPI to VulkanRHI
#define VK_DYNAMICAPI_TO_VULKANRHI(Type,Func) using VulkanDynamicAPI::Func;
namespace VulkanRHI
{
	ENUM_VK_ENTRYPOINTS_ALL(VK_DYNAMICAPI_TO_VULKANRHI);
}