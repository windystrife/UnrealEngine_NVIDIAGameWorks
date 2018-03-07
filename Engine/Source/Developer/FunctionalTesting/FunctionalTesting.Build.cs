// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class FunctionalTesting : ModuleRules
{
    public FunctionalTesting(ReadOnlyTargetRules Target) : base(Target)
    {
        PrivateDependencyModuleNames.AddRange(
            new string[] {
                "Core",
                "CoreUObject",
                "Engine",
                "ShaderCore",
                "Slate",
                "MessageLog",
                "AIModule",
                "RenderCore",
                "AssetRegistry",
                "RHI",
                "UMG",
				"AutomationController"
            }
        );

        PrivateIncludePaths.AddRange(
            new string[]
            {
                "MessageLog/Public",
                "Stats/Public",
                "Developer/FunctionalTesting/Private",
            }
        );

        if (Target.bBuildEditor == true)
        {
            PrivateDependencyModuleNames.AddRange(
                new string[] {
                    "SourceControl"
                }
            );
        }
    }
}
