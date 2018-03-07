// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class HardwareTargeting : ModuleRules
{
	public HardwareTargeting(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"CoreUObject",
				"Engine",
				"InputCore",
				"SlateCore",
				"Slate",
				"EditorStyle",
				"UnrealEd",
				"Settings",
				"EngineSettings",
			}
		);
	}
}
