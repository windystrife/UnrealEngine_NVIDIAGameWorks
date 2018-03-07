// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Qos : ModuleRules
{
	public Qos(ReadOnlyTargetRules Target) : base(Target)
	{
		Definitions.Add("QOS_PACKAGE=1");
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateIncludePaths.AddRange(
			new string[] {
				"Private",
			}
			);

		PrivateDependencyModuleNames.AddRange(
			new string[] { 
				"Core", 
				"CoreUObject",
				"Engine", 
                "Icmp",
				"OnlineSubsystem",
				"OnlineSubsystemUtils"
			}
		);

		DynamicallyLoadedModuleNames.AddRange(
			new string[] {
				"Analytics",
				"AnalyticsET"
			}
		);

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"Analytics"
			}
		);

		if (Target.Platform == UnrealTargetPlatform.Win32 ||
			Target.Platform == UnrealTargetPlatform.Win64)
		{
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
		}
	}
}
