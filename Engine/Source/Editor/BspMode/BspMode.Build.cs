// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class BspMode : ModuleRules
{
	public BspMode(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
                "InputCore",
				"Slate",
				"SlateCore",
                "EditorStyle",
				"UnrealEd",
				"LevelEditor",
			}
		);

        DynamicallyLoadedModuleNames.AddRange(
            new string[] {
				"PropertyEditor",
				"PlacementMode",
			}
        );

        PrivateIncludePathModuleNames.AddRange(
            new string[] {
				"PlacementMode",
			}
        );
	}
}
