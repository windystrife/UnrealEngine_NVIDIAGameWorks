// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class WorkspaceMenuStructure : ModuleRules
{
	public WorkspaceMenuStructure(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePaths.Add("Editor/WorkspaceMenuStructure/Private");

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"EditorStyle",
				"SlateCore",
			}
		);
	}
}
