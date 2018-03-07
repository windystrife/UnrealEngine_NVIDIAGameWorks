// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ProjectTargetPlatformEditor : ModuleRules
{
	public ProjectTargetPlatformEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"Slate",
                "SlateCore",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"InputCore",
				"EditorStyle",
				"TargetPlatform",
				"DesktopPlatform",
				"Settings",
				"UnrealEd",
				"Projects",
			}
		);

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"GameProjectGeneration",
			}
		);

		DynamicallyLoadedModuleNames.AddRange(
			new string[] {
				"GameProjectGeneration",
			}
		);

		PrivateIncludePaths.AddRange(
			new string[]
			{
				"Editor/ProjectTargetPlatformEditor/Private",
				"Editor/ProjectTargetPlatformEditor/Private/Widgets",
			}
		);
	}
}
