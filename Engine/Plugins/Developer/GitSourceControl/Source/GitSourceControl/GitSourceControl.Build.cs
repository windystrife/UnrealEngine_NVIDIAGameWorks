// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class GitSourceControl : ModuleRules
{
	public GitSourceControl(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"Slate",
				"SlateCore",
				"InputCore",
				"EditorStyle",
				"SourceControl",
			}
		);

		if (Target.bBuildEditor == true)
		{
			// needed to enable/disable this via experimental settings
			PrivateDependencyModuleNames.Add("CoreUObject");
			PrivateDependencyModuleNames.Add("UnrealEd");
		}
	}
}
