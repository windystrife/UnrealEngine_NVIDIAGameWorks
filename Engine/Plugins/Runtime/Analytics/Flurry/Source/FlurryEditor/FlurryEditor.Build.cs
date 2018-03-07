// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class FlurryEditor : ModuleRules
{
    public FlurryEditor(ReadOnlyTargetRules Target) : base(Target)
	{
        PrivateIncludePaths.Add("FlurryEditor/Private");

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
                "Analytics",
                "AnalyticsVisualEditing",
                "Engine",
				"Projects"
			}
			);

		PrivateIncludePathModuleNames.AddRange(
			new string[]
            {
				"Settings"
			}
		);
	}
}
