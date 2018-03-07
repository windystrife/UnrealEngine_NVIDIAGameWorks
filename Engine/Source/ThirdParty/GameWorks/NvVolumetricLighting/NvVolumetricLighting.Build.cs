// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System;

public class NvVolumetricLighting : ModuleRules
{
    public NvVolumetricLighting(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;
	
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
            Definitions.Add("WITH_NVVOLUMETRICLIGHTING=1");
            Definitions.Add("NV_PLATFORM_D3D11=1");

            if (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT)
            {
                Definitions.Add("NV_DEBUG=1");
                Definitions.Add("NV_CHECKED=1");
            }
        }
		
        string NvVolumetricLightingDir = Target.UEThirdPartySourceDirectory + "GameWorks/NvVolumetricLighting/";

        PublicIncludePaths.Add(NvVolumetricLightingDir + "include");

		// Libraries and DLLs for windows platform
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
            PublicLibraryPaths.Add(NvVolumetricLightingDir + "lib");

            if (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT)
            {
                PublicAdditionalLibraries.Add("NvVolumetricLighting.win64.D.lib");
                PublicDelayLoadDLLs.Add("NvVolumetricLighting.win64.D.dll");

                string[] RuntimeDependenciesX64 =
                {
                    "NvVolumetricLighting.win64.D.dll",
                };

                string NvVolumetricLightingBinariesDir = String.Format("$(EngineDir)/Binaries/ThirdParty/GameWorks/NvVolumetricLighting/");
                foreach (string RuntimeDependency in RuntimeDependenciesX64)
                {
                    RuntimeDependencies.Add(new RuntimeDependency(NvVolumetricLightingBinariesDir + RuntimeDependency));
                }
            }
            else
            {
                PublicAdditionalLibraries.Add("NvVolumetricLighting.win64.lib");
                PublicDelayLoadDLLs.Add("NvVolumetricLighting.win64.dll");

                string[] RuntimeDependenciesX64 =
                {
                    "NvVolumetricLighting.win64.dll",
                };

                string NvVolumetricLightingBinariesDir = String.Format("$(EngineDir)/Binaries/ThirdParty/GameWorks/NvVolumetricLighting/");
                foreach (string RuntimeDependency in RuntimeDependenciesX64)
                {
                    RuntimeDependencies.Add(new RuntimeDependency(NvVolumetricLightingBinariesDir + RuntimeDependency));
                }
            }
		}
	}
}
