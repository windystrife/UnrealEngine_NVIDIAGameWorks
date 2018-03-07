// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class IOSTargetPlatform : ModuleRules
{
	public IOSTargetPlatform(ReadOnlyTargetRules Target) : base(Target)
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
				"Projects"
			}
		);

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
                "MessagingCommon",
				"TargetDeviceServices",
			}
		);

		PlatformSpecificDynamicallyLoadedModuleNames.Add("LaunchDaemonMessages");

		PrivateIncludePaths.Add("Developer/IOS/IOSTargetPlatform/Private");

		//This is somehow necessary for getting iOS to build on, at least, windows. It seems like the target platform is included for cooking, and thus it requirtes a bunch of other info.
		PublicIncludePaths.AddRange(
			new string[]
			{
				"Runtime/Core/Public/Apple",
				"Runtime/Core/Public/IOS",
			}
		);

		if (Target.bCompileAgainstEngine)
		{
			PrivateDependencyModuleNames.Add("Engine");
		}

		if (Target.Platform == UnrealTargetPlatform.Mac)
		{
            PublicAdditionalLibraries.Add("/System/Library/PrivateFrameworks/MobileDevice.framework/Versions/Current/MobileDevice");
		}
	}
}
