// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ClassViewer : ModuleRules
{
	public ClassViewer(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"AssetRegistry",
                "AssetTools",
				"EditorWidgets",
				"GameProjectGeneration",
                "PropertyEditor",
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
                "EditorStyle",
				"UnrealEd",
				"PropertyEditor",
                "Settings",
            }
		);

		DynamicallyLoadedModuleNames.AddRange(
			new string[] {
				"AssetRegistry",
                "AssetTools",
				"EditorWidgets",
				"GameProjectGeneration",
			}
		);
	}
}
