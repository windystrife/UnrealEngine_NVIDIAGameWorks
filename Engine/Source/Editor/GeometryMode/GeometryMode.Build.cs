// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class GeometryMode : ModuleRules
{
	public GeometryMode(ReadOnlyTargetRules Target) : base(Target)
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
				"RenderCore",
				"LevelEditor"
			}
		);

		DynamicallyLoadedModuleNames.AddRange(
            new string[] {
				"PropertyEditor",
                "BspMode"
			}
        );
	}
}
