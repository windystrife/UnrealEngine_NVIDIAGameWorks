// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ScreenShotComparison : ModuleRules
{
	public ScreenShotComparison(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
                "EditorStyle",
                "InputCore",
				"ScreenShotComparisonTools",
				"Slate",
				"SlateCore",
				"ImageWrapper",
				"CoreUObject",
				"DesktopWidgets",
				"SourceControl",
				"AutomationMessages",
				"Json",
				"JsonUtilities",
				"DirectoryWatcher"
			}
		);

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"SessionServices",
			}
		);

		PrivateIncludePaths.AddRange(
			new string[] {
				"Developer/ScreenShotComparison/Private",
				"Developer/ScreenShotComparison/Private/Widgets",
				"Developer/ScreenShotComparison/Private/Models",
			}
		);
	}
}
