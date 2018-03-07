// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class LogVisualizer : ModuleRules
{
	public LogVisualizer(ReadOnlyTargetRules Target) : base(Target)
	{
        PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"DesktopPlatform",
				"MainFrame",
                "SequencerWidgets",
				"LevelEditor"
			}
		);

		PublicIncludePaths.AddRange(
			new string[] {
				"Runtime/Engine/Classes",
                "Editor/WorkspaceMenuStructure/Public"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"ApplicationCore",
                "InputCore",
				"Json",
				"Slate",
				"SlateCore",
                "EditorStyle",
				"Engine",
				"UnrealEd",
                "SequencerWidgets",
			}
		);

		DynamicallyLoadedModuleNames.AddRange(
			new string[] {
				"DesktopPlatform",
				"MainFrame",
			}
		);
	}
}
