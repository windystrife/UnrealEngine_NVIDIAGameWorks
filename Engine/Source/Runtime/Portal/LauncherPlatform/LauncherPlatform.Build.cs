// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class LauncherPlatform : ModuleRules
{
    public LauncherPlatform(ReadOnlyTargetRules Target) : base(Target)
    {
        PrivateIncludePaths.Add("Runtime/Portal/LauncherPlatform/Private");

        PrivateDependencyModuleNames.AddRange(
            new string[] {
                "Core",
            }
        );

        if (Target.Platform != UnrealTargetPlatform.Linux && Target.Platform != UnrealTargetPlatform.Win32 && Target.Platform != UnrealTargetPlatform.Win64 && Target.Platform != UnrealTargetPlatform.Mac)
        {
            PrecompileForTargets = PrecompileTargetsType.None;
        }
    }
}
