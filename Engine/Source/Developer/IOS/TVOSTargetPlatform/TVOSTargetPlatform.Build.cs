// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TVOSTargetPlatform : ModuleRules
{
	public TVOSTargetPlatform(ReadOnlyTargetRules Target) : base(Target)
	{
		BinariesSubFolder = "IOS";
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"TargetPlatform",
				"DesktopPlatform",
				"LaunchDaemonMessages",
				"IOSTargetPlatform",
				"Projects"
			}
		);

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
			"Messaging",
			"TargetDeviceServices",
		}
		);

		PlatformSpecificDynamicallyLoadedModuleNames.Add("LaunchDaemonMessages");

		if (Target.bCompileAgainstEngine)
		{
			PrivateDependencyModuleNames.Add("Engine");
		}

		PrivateIncludePaths.AddRange(
			new string[] {
			"Developer/IOS/IOSTargetPlatform/Private"
			}
		);
	}
}
