// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SmartSnapping : ModuleRules
{
	public SmartSnapping(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
				"UnrealEd",
				"SlateCore",
				"Slate",
				"LevelEditor",
				"ViewportSnapping"
			});
	}
}
