// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class OnlineSubsystemUtils : ModuleRules
{
	public OnlineSubsystemUtils(ReadOnlyTargetRules Target) : base(Target)
    {
		Definitions.Add("ONLINESUBSYSTEMUTILS_PACKAGE=1");
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PrivateIncludePaths.Add("OnlineSubsystemUtils/Private");

		PrivateDependencyModuleNames.AddRange(
			new string[] { 
				"Core", 
				"CoreUObject",
				"Engine", 
				"EngineSettings",
                "ImageCore",
				"Sockets",
				"Voice",
                "PacketHandler",
				"Json"
			}
		);

        PublicDependencyModuleNames.Add("OnlineSubsystem");
	}
}
