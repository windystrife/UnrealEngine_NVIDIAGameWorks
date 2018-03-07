// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class SteamController : ModuleRules
{
    public SteamController(ReadOnlyTargetRules Target) : base(Target)
    {
        string SteamVersion = "Steamv139";
        bool bSteamSDKFound = Directory.Exists(Target.UEThirdPartySourceDirectory + "Steamworks/" + SteamVersion) == true;

        Definitions.Add("STEAMSDK_FOUND=" + (bSteamSDKFound ? "1" : "0"));

        PrivateIncludePathModuleNames.Add("TargetPlatform");

        PrivateDependencyModuleNames.AddRange(new string[]
        {
			"Core",
			"CoreUObject",
			"Engine",
		});

        PublicDependencyModuleNames.Add("InputDevice");
        PublicDependencyModuleNames.Add("InputCore");

        AddEngineThirdPartyPrivateStaticDependencies(Target,
            "Steamworks"
        );
    }
}
