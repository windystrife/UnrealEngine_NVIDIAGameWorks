// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TextureAlignMode : ModuleRules
{
	public TextureAlignMode(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
				"SlateCore",
				"Slate",
				"UnrealEd",
				"RenderCore",
				"LevelEditor"
			}
			);

        DynamicallyLoadedModuleNames.AddRange(
            new string[] {
                "BspMode"
			}
        );
	}
}
