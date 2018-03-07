// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;

public class DirectSound : ModuleRules
{
	public DirectSound(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		string DirectXSDKDir = Target.UEThirdPartySourceDirectory + "Windows/DirectX";
		PublicSystemIncludePaths.Add( DirectXSDKDir + "/include");

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicLibraryPaths.Add( DirectXSDKDir + "/Lib/x64");
		}
		else if (Target.Platform == UnrealTargetPlatform.Win32)
		{
			PublicLibraryPaths.Add( DirectXSDKDir + "/Lib/x86");
		}

		PublicAdditionalLibraries.AddRange(
				new string[] {
 				"dxguid.lib",
 				"dsound.lib"
 				}
			);
	}
}

	
