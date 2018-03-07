// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class OnlineSubsystem : ModuleRules
{
	public OnlineSubsystem(ReadOnlyTargetRules Target) : base(Target)
    {
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Json",
			}
		);

		PublicIncludePaths.Add("OnlineSubsystem/Public/Interfaces");

        PrivateIncludePaths.Add("OnlineSubsystem/Private");

        Definitions.Add("ONLINESUBSYSTEM_PACKAGE=1");
		Definitions.Add("DEBUG_LAN_BEACON=0");

		PrivateDependencyModuleNames.AddRange(
			new string[] { 
				"Core", 
				"CoreUObject",
				"ImageCore",
				"Sockets",
				"JsonUtilities"
			}
		);


	}

  
}
