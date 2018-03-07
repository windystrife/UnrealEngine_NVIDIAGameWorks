// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;

public class DX12 : ModuleRules
{
	public DX12(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		string DirectXSDKDir = Target.UEThirdPartySourceDirectory + "Windows/DirectX";
		PublicSystemIncludePaths.Add(DirectXSDKDir + "/include");

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicLibraryPaths.Add(DirectXSDKDir + "/Lib/x64");

            PublicDelayLoadDLLs.Add("WinPixEventRuntime.dll");
            PublicAdditionalLibraries.Add("WinPixEventRuntime.lib");
            RuntimeDependencies.Add(new RuntimeDependency("$(EngineDir)/Binaries/ThirdParty/Windows/DirectX/x64/WinPixEventRuntime.dll"));
        }
		else if (Target.Platform == UnrealTargetPlatform.Win32)
		{
			PublicLibraryPaths.Add(DirectXSDKDir + "/Lib/x86");
		}

		// Always delay-load D3D12
		PublicDelayLoadDLLs.AddRange( new string[] {
			"d3d12.dll"
			} );

		PublicAdditionalLibraries.AddRange(
			new string[] {
                "d3d12.lib"
			}
			);
	}
}

