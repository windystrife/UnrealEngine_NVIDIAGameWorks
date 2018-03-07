// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class FunctionalTestingEditor : ModuleRules
{
    public FunctionalTestingEditor(ReadOnlyTargetRules Target) : base(Target)
	{
        PublicDependencyModuleNames.AddRange
        (
            new string[] {
				"Core",
                "InputCore",
                "CoreUObject",
                "SlateCore",
                "Slate",
                "EditorStyle",
                "Engine",
                "AssetRegistry"
			}
        );
        
        PrivateDependencyModuleNames.AddRange(
             new string[] {
                "Engine",
                "UnrealEd",
                "LevelEditor",
                "SessionFrontend",
                "FunctionalTesting",
                "PlacementMode",
                "WorkspaceMenuStructure",
                "ScreenShotComparisonTools",
				"AssetTools"
            }
         );
	}
}
