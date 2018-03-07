// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class CurveTableEditor : ModuleRules
{
	public CurveTableEditor(ReadOnlyTargetRules Target) : base(Target)
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
				"UnrealEd"
			}
		);

		DynamicallyLoadedModuleNames.Add("WorkspaceMenuStructure");
	}
}
