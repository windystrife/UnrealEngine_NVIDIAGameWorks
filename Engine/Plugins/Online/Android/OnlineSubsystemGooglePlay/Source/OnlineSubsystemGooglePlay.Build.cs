// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class OnlineSubsystemGooglePlay : ModuleRules
{
	public OnlineSubsystemGooglePlay(ReadOnlyTargetRules Target) : base(Target)
    {
		Definitions.Add("ONLINESUBSYSTEMGOOGLEPLAY_PACKAGE=1");
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateIncludePaths.AddRange(
			new string[] {
				"Private",    
				"Runtime/Launch/Private"
			}
			);

		PrivateDependencyModuleNames.AddRange(
			new string[] { 
                "Core", 
                "CoreUObject", 
                "Engine", 
                "Sockets",
				"OnlineSubsystem", 
                "Http",
				"AndroidRuntimeSettings",
				"Launch",
				"GpgCppSDK"
            }
			);

		PublicIncludePathModuleNames.AddRange(
			new string[] {
				"AndroidPermission"
			}
			);
	}
}
