// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class CurveAssetEditor : ModuleRules
{
	public CurveAssetEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicIncludePathModuleNames.Add("LevelEditor");
        PublicIncludePathModuleNames.Add("WorkspaceMenuStructure");

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
                "EditorStyle",
				"UnrealEd",
				"InputCore"
			}
		);

		DynamicallyLoadedModuleNames.Add("WorkspaceMenuStructure");
	}
}
