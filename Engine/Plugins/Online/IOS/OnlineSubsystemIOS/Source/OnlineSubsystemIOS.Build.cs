// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class OnlineSubsystemIOS : ModuleRules
{
    public OnlineSubsystemIOS(ReadOnlyTargetRules Target) : base(Target)
    {
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		PrivateIncludePaths.AddRange( 
            new string[] {
                "Private",             
                }
                );

        Definitions.Add("ONLINESUBSYSTEMIOS_PACKAGE=1");

		PrivateDependencyModuleNames.AddRange(
            new string[] { 
                "Core", 
                "CoreUObject", 
                "Engine", 
                "Sockets",
				"OnlineSubsystem", 
                "Http",
            }
            );

		PublicWeakFrameworks.Add("Cloudkit");
		if (Target.Platform == UnrealTargetPlatform.IOS)
		{
			PublicWeakFrameworks.Add("MultipeerConnectivity");
		}
	}
}
