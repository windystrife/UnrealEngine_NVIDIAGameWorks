// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ShaderCore : ModuleRules
{
	public ShaderCore(ReadOnlyTargetRules Target) : base(Target)
	{
        PrivateIncludePaths.AddRange( new string[] { "Developer/DerivedDataCache/Public" });

		PublicDependencyModuleNames.AddRange(new string[] { "RHI", "RenderCore" });

		PrivateDependencyModuleNames.AddRange(new string[] { "Core" });

		PrivateIncludePathModuleNames.AddRange(new string[] { "DerivedDataCache", "TargetPlatform" });

        // NVCHANGE_BEGIN: Add VXGI
        if ((Target.Platform == UnrealTargetPlatform.Win64) || (Target.Platform == UnrealTargetPlatform.Win32))
        {
            PublicDependencyModuleNames.Add("VXGI");
        }
        // NVCHANGE_END: Add VXGI
    }
}