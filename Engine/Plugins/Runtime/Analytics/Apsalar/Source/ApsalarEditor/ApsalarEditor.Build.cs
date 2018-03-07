// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ApsalarEditor : ModuleRules
{
    public ApsalarEditor(ReadOnlyTargetRules Target) : base(Target)
	{
        PrivateIncludePaths.Add("ApsalarEditor/Private");

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
