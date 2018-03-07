// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ScreenShotComparisonTools : ModuleRules
{
	public ScreenShotComparisonTools(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"AutomationMessages",
                "EditorStyle",
				"ImageWrapper",
				"Json",
				"JsonUtilities",
				"Slate",
                "UnrealEdMessages",
            }
        );

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
                "MessagingCommon",
            }
		);

		PrivateIncludePaths.AddRange(
			new string[] {
				"Developer/ScreenShotComparisonTools/Private"
			}
		);
	}
}
