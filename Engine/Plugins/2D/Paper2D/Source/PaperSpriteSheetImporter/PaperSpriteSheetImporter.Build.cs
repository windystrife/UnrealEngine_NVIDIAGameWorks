// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class PaperSpriteSheetImporter : ModuleRules
{
	public PaperSpriteSheetImporter(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Json",
				"Slate",
				"SlateCore",
				"Engine",
				"Paper2D",
				"UnrealEd",
				"Paper2DEditor",
				"AssetTools",
				"ContentBrowser",
                "EditorStyle"
			});

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"AssetTools",
				"AssetRegistry"
			});

		DynamicallyLoadedModuleNames.AddRange(
			new string[] {
				"AssetRegistry"
			});
	}
}
