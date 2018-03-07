// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class FbxAutomationTestBuilder : ModuleRules
{
    public FbxAutomationTestBuilder(ReadOnlyTargetRules Target) : base(Target)
	{
        PublicDependencyModuleNames.AddRange
        (
            new string[] {
				"Core",
                "InputCore",
                "CoreUObject",
				"Slate",
                "EditorStyle",
                "Engine",
                "UnrealEd",
                "PropertyEditor",
				"LevelEditor"
            }
        );
        
        PrivateDependencyModuleNames.AddRange(
             new string[] {
					"Engine",
                    "UnrealEd",
                    "WorkspaceMenuStructure"
                }
         );

        if (Target.bBuildEditor == true)
        {
            PrivateDependencyModuleNames.AddRange(
                new string[] {
                    "UnrealEd",
    				"SlateCore",
    				"Slate",
                }
            );
        }
	}
}
