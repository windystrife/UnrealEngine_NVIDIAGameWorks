// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System;

public class RHI : ModuleRules
{
	public RHI(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.Add("Core");
		PrivateDependencyModuleNames.Add("ApplicationCore");

        // NVCHANGE_BEGIN: Nvidia Volumetric Lighting
        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            PublicDependencyModuleNames.Add("NvVolumetricLighting");
        }
        // NVCHANGE_END: Nvidia Volumetric Lighting

        if (Target.bCompileAgainstEngine)
		{
            DynamicallyLoadedModuleNames.Add("NullDrv");

            // NVCHANGE_BEGIN: Add VXGI
            if ((Target.Platform == UnrealTargetPlatform.Win64) || (Target.Platform == UnrealTargetPlatform.Win32))
            {
                PublicDependencyModuleNames.Add("VXGI");
            }
            // NVCHANGE_END: Add VXGI

            // NVCHANGE_BEGIN: Add HBAO+
            if ((Target.Platform == UnrealTargetPlatform.Win64) || (Target.Platform == UnrealTargetPlatform.Win32))
            {
                PublicDependencyModuleNames.Add("GFSDK_SSAO");
            }
            // NVCHANGE_END: Add HBAO+

            // UEBuildAndroid.cs adds VulkanRHI for Android builds if it is enabled
            if ((Target.Platform == UnrealTargetPlatform.Win32) || (Target.Platform == UnrealTargetPlatform.Win64))
			{
				DynamicallyLoadedModuleNames.Add("D3D11RHI");

				//#todo-rco: D3D12 requires different SDK headers not compatible with WinXP
				DynamicallyLoadedModuleNames.Add("D3D12RHI");
            }

			if ((Target.Platform == UnrealTargetPlatform.Win64) ||
				(Target.Platform == UnrealTargetPlatform.Win32) ||
				(Target.Platform == UnrealTargetPlatform.Linux && Target.Architecture.StartsWith("x86_64")))	// temporary, not all archs can support Vulkan atm
			{
				DynamicallyLoadedModuleNames.Add("VulkanRHI");
			}

			if ((Target.Platform == UnrealTargetPlatform.Win32) ||
				(Target.Platform == UnrealTargetPlatform.Win64) ||
                (Target.Platform == UnrealTargetPlatform.Linux && Target.Type != TargetRules.TargetType.Server) ||  // @todo should servers on all platforms skip this?
                (Target.Platform == UnrealTargetPlatform.HTML5))
			{
				DynamicallyLoadedModuleNames.Add("OpenGLDrv");
			}
        }

		if (Target.Configuration != UnrealTargetConfiguration.Shipping)
		{
			PrivateIncludePathModuleNames.AddRange(new string[] { "TaskGraph" });
		}

		PrivateIncludePaths.Add("Runtime/RHI/Private");
	}
}
