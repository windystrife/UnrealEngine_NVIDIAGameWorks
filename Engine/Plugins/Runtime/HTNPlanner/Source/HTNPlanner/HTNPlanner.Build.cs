// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
    public class HTNPlanner : ModuleRules
    {
        public HTNPlanner(ReadOnlyTargetRules Target) : base(Target)
        {
            PublicIncludePaths.AddRange(
                    new string[] {
                        "Runtime/HTNPlanner/Public",
                    }
                    );

            PrivateIncludePaths.AddRange(
                new string[] {
                        "Runtime/AIModule/Public",
                        "Runtime/AIModule/Private",
                        "Runtime/Engine/Private",
                }
                );

            PublicDependencyModuleNames.AddRange(
                new string[] {
                        "Core",
                        "CoreUObject",
                        "Engine",
                        "GameplayTags",
                        "GameplayTasks",
                        "AIModule"
                }
                );

            DynamicallyLoadedModuleNames.AddRange(
                new string[] {
                    // ... add any modules that your module loads dynamically here ...
                }
                );

            if (Target.bBuildEditor == true)
            {
                PrivateDependencyModuleNames.Add("UnrealEd");
            }

            if (Target.bBuildDeveloperTools || (Target.Configuration != UnrealTargetConfiguration.Shipping && Target.Configuration != UnrealTargetConfiguration.Test))
            {
                PrivateDependencyModuleNames.Add("GameplayDebugger");
                Definitions.Add("WITH_GAMEPLAY_DEBUGGER=1");
            }
            else
            {
                Definitions.Add("WITH_GAMEPLAY_DEBUGGER=0");
            }
        }
    }
}
