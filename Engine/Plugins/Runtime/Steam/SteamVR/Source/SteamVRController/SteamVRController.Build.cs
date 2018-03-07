// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SteamVRController : ModuleRules
{
    public SteamVRController(ReadOnlyTargetRules Target) : base(Target)
    {
        PrivateIncludePathModuleNames.AddRange(new string[]
		{
			"TargetPlatform",
            "SteamVR"
		});

        PrivateDependencyModuleNames.AddRange(new string[]
        {
			"Core",
			"CoreUObject",
			"ApplicationCore",
			"Engine",
			"InputDevice",
            "InputCore",
			"HeadMountedDisplay",
            "SteamVR"
		});

// 		DynamicallyLoadedModuleNames.AddRange(new string[]
// 		{
// 			"SteamVR",
// 		});

		AddEngineThirdPartyPrivateStaticDependencies(Target, "OpenVR");

        if ( Target.Platform == UnrealTargetPlatform.Win64 || Target.Platform == UnrealTargetPlatform.Win32 || (Target.Platform == UnrealTargetPlatform.Linux && Target.Architecture.StartsWith("x86_64")) )
        {
            AddEngineThirdPartyPrivateStaticDependencies(Target, "OpenGL");
            PrivateDependencyModuleNames.Add("OpenGLDrv");
        }
    }
}
