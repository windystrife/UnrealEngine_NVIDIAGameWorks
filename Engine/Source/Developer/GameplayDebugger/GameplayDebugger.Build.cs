// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
    public class GameplayDebugger : ModuleRules
    {
        public GameplayDebugger(ReadOnlyTargetRules Target) : base(Target)
        {
            PublicDependencyModuleNames.AddRange(
                new string[] {
                    "Core",
                    "CoreUObject",
                    "Engine",
                });

            PrivateDependencyModuleNames.AddRange(
                new string[] {
                    "RenderCore",
                    "InputCore",
                    "SlateCore",
                    "Slate",
                });

            PrivateIncludePaths.AddRange(
                new string[] {
                    "Developer/GameplayDebugger/Private",
                    "Developer/Settings/Public",
                });

            if (Target.bBuildEditor)
			{
                PrivateDependencyModuleNames.AddRange(
                    new string[] {
                        "EditorStyle",
                        "UnrealEd",
                        "LevelEditor",
                        "PropertyEditor",
                    });
			}
        }
    }
}
