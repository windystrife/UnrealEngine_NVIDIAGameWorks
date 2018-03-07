// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class D3D11RHI : ModuleRules
{
	public D3D11RHI(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePaths.Add("Runtime/Windows/D3D11RHI/Private");

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"Engine",
				"RHI",
				"RenderCore",
				"ShaderCore",
				"UtilityShaders",
// WaveWorks Start
                "WaveWorks",
// WaveWorks End
			}
			);

		AddEngineThirdPartyPrivateStaticDependencies(Target, "DX11");
        AddEngineThirdPartyPrivateStaticDependencies(Target, "NVAPI");
		AddEngineThirdPartyPrivateStaticDependencies(Target, "AMD_AGS");
        AddEngineThirdPartyPrivateStaticDependencies(Target, "NVAftermath");
        // NVCHANGE_BEGIN: Add VXGI
        if ((Target.Platform == UnrealTargetPlatform.Win64) || (Target.Platform == UnrealTargetPlatform.Win32))
        {
            AddEngineThirdPartyPrivateStaticDependencies(Target, "VXGI");
        }
        // NVCHANGE_END: Add VXGI

        // NVCHANGE_BEGIN: Add HBAO+
        if ((Target.Platform == UnrealTargetPlatform.Win64) || (Target.Platform == UnrealTargetPlatform.Win32))
        {
            AddEngineThirdPartyPrivateStaticDependencies(Target, "GFSDK_SSAO");
        }
        // NVCHANGE_END: Add HBAO+

        if (Target.Configuration != UnrealTargetConfiguration.Shipping)
		{
			PrivateIncludePathModuleNames.AddRange(new string[] { "TaskGraph" });
		}

		// @third party code - BEGIN HairWorks
		AddEngineThirdPartyPrivateStaticDependencies(Target, "HairWorks");
		// @third party code - END HairWorks
	}
}
