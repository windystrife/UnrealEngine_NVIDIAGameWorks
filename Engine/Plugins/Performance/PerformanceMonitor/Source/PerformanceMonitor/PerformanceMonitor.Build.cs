// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
    public class PerformanceMonitor : ModuleRules
    {
        public PerformanceMonitor(ReadOnlyTargetRules Target) : base(Target)
        {
            PrivateIncludePaths.AddRange
            (
                new string[] {
				"PerformanceMonitor/Private",
			}
            );

            PublicDependencyModuleNames.AddRange
            (
                new string[] {
				"Core",
                "Engine",
                "InputCore",
				"RHI",
				"RenderCore",
			}
            );

            if (Target.bBuildEditor)
            {
                PrivateDependencyModuleNames.AddRange(
                    new string[] {
					
				}
                );
            }

        }
    }
}