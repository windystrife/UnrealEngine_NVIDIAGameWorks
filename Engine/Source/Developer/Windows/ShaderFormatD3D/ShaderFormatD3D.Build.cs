// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ShaderFormatD3D : ModuleRules
{
	public ShaderFormatD3D(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePathModuleNames.Add("TargetPlatform");
		PrivateIncludePathModuleNames.Add("D3D11RHI"); 

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"ShaderCore",
				"ShaderPreprocessor",
				"ShaderCompilerCommon",
			}
			);

		AddEngineThirdPartyPrivateStaticDependencies(Target, "DX11");

        // NVCHANGE_BEGIN: Add VXGI
        if ((Target.Platform == UnrealTargetPlatform.Win64) || (Target.Platform == UnrealTargetPlatform.Win32))
        {
            PublicDependencyModuleNames.Add("VXGI");
        }
        // NVCHANGE_END: Add VXGI
    }
}
