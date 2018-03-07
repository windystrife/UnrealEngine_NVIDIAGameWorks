// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class HeadMountedDisplay : ModuleRules
{
    public HeadMountedDisplay(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicIncludePaths.AddRange(
			new string[] {
				"Runtime/HeadMountedDisplay/Public"
			}
		);

        PrivateIncludePaths.AddRange(
            new string[] {
                "Runtime/Renderer/Private"
            }
        );

        PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
                "InputCore",
				"Slate",
				"SlateCore",
                "RHI",
                "Renderer",
                "ShaderCore",
                "RenderCore",
                "UtilityShaders",
                "Analytics"
            }
        );

		if (Target.bBuildEditor == true)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"UnrealEd"
				});
		}
	}
}
