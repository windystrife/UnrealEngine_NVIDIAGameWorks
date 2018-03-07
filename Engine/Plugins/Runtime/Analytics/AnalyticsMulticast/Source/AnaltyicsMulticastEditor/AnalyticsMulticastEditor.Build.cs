// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AnalyticsMulticastEditor : ModuleRules
{
    public AnalyticsMulticastEditor(ReadOnlyTargetRules Target) : base(Target)
	{
        PrivateIncludePaths.Add("AnalyticsMulticastEditor/Private");

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
                "Analytics",
                "AnalyticsVisualEditing",
				"Slate",
				"SlateCore",
				"Engine",
				"UnrealEd", // for FAssetEditorManager
				"PropertyEditor",
				"WorkspaceMenuStructure",
				"EditorStyle",
				"EditorWidgets",
				"Projects"
			}
			);

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"Settings",
				"IntroTutorials",
                "AssetTools"
			}
		);

        DynamicallyLoadedModuleNames.AddRange(
            new string[] {
                "AssetTools"
            }
        );

	}
}
