// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;

public class DX9 : ModuleRules
{
	public DX9(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		string DirectXSDKDir = Target.UEThirdPartySourceDirectory + "Windows/DirectX";
		PublicSystemIncludePaths.Add(DirectXSDKDir + "/include");

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicLibraryPaths.Add(DirectXSDKDir + "/Lib/x64");
		}
		else if (Target.Platform == UnrealTargetPlatform.Win32)
		{
			PublicLibraryPaths.Add(DirectXSDKDir + "/Lib/x86");
		}

		PublicAdditionalLibraries.AddRange(
			new string[] {
				"d3d9.lib",
				"dxguid.lib",
				"d3dcompiler.lib",
				(Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT) ? "d3dx9d.lib" : "d3dx9.lib",
				"dinput8.lib",
				"X3DAudio.lib",
				"xapobase.lib",
				"XAPOFX.lib"
			}
			);
	}
}

