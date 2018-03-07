// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class DataTableEditor : ModuleRules
{
	public DataTableEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicIncludePathModuleNames.Add("LevelEditor");

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
                "InputCore",
				"Slate",
				"SlateCore",
                "EditorStyle",
                "PropertyEditor",
				"UnrealEd",
				"Json"
			}
			);

		DynamicallyLoadedModuleNames.Add("WorkspaceMenuStructure");
	}
}
