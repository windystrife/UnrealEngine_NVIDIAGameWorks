// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AdjustEditor : ModuleRules
{
    public AdjustEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PrivateIncludePaths.Add("AdjustEditor/Private");

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
