// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TaskGraph : ModuleRules
{
	public TaskGraph(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Slate",
				"SlateCore",
				"EditorStyle",
				"Engine",
				"InputCore"
			}
		);

		if (Target.Type == TargetType.Editor)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"WorkspaceMenuStructure",
					"DesktopPlatform"
				}
			);
		}
	}
}
