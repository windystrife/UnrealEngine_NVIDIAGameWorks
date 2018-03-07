// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;

public class XInput : ModuleRules
{
	public XInput(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		string DirectXSDKDir = Target.UEThirdPartySourceDirectory + "Windows/DirectX";

		// Ensure correct include and link paths for xinput so the correct dll is loaded (xinput1_3.dll)
		PublicSystemIncludePaths.Add(DirectXSDKDir + "/include");
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicLibraryPaths.Add(DirectXSDKDir + "/Lib/x64");
		}
		else if (Target.Platform == UnrealTargetPlatform.Win32)
		{
			PublicLibraryPaths.Add(DirectXSDKDir + "/Lib/x86");
		}
		PublicAdditionalLibraries.Add("XInput.lib");
	}
}

