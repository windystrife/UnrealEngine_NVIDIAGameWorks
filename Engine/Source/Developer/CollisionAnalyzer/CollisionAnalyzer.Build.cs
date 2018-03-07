// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class CollisionAnalyzer : ModuleRules
{
	public CollisionAnalyzer(ReadOnlyTargetRules Target) : base(Target)
	{
        PrivateIncludePathModuleNames.AddRange(
            new string[] {
				"DesktopPlatform",
				"MainFrame",
			}
        );

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
                "InputCore",
				"Slate",
				"SlateCore",
                "EditorStyle",
				"Engine",
				"WorkspaceMenuStructure",
			}
		);

        DynamicallyLoadedModuleNames.AddRange(
            new string[] {
				"DesktopPlatform",
				"MainFrame",
			}
        );
	}
}
