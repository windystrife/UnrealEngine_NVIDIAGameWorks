// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class OnlineSubsystemOculus : ModuleRules
{
	public OnlineSubsystemOculus(ReadOnlyTargetRules Target) : base(Target)
    {		
		Definitions.Add("ONLINESUBSYSTEMOCULUS_PACKAGE=1");
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PrivateIncludePaths.AddRange(
			new string[] {
				"OnlineSubsystemOculus/Private",
			}
			);

		PublicDependencyModuleNames.AddRange(
			new string[] {
				"OnlineSubsystemUtils"
			}
			);
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
				"Sockets",
				"OnlineSubsystem",
				"Projects",
				"PacketHandler",
                "Voice",
            }
			);

		PublicDependencyModuleNames.AddRange(new string[] { "LibOVRPlatform" });

		if (Target.Platform == UnrealTargetPlatform.Win64 || Target.Platform == UnrealTargetPlatform.Win32)
		{
			if (Target.Platform == UnrealTargetPlatform.Win32)
			{
				PublicDelayLoadDLLs.Add("LibOVRPlatform32_1.dll");
			}
			else
			{
				PublicDelayLoadDLLs.Add("LibOVRPlatform64_1.dll");
			}
		}
		else if (Target.Platform != UnrealTargetPlatform.Android)
		{
			PrecompileForTargets = PrecompileTargetsType.None;
		}
	}
}
