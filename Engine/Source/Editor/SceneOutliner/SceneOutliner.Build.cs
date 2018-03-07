// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SceneOutliner : ModuleRules
{
	public SceneOutliner(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePathModuleNames.AddRange(
			new string[] {
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
			}
		);

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"LevelEditor"
			}
		);
	}
}
