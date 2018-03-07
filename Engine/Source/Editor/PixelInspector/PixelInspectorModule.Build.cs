// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class PixelInspectorModule : ModuleRules
{
    public PixelInspectorModule(ReadOnlyTargetRules Target) : base(Target)
	{
        PublicDependencyModuleNames.AddRange
        (
            new string[] {
				"Core",
                "InputCore",
                "CoreUObject",
				"RHI",
				"RenderCore",
				"Slate",
                "EditorStyle",
                "Engine",
                "UnrealEd",
                "PropertyEditor",
			}
        );
        
        PrivateDependencyModuleNames.AddRange(
             new string[] {
					"Engine",
                    "UnrealEd"
				}
         );

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"LevelEditor"
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

            CircularlyReferencedDependentModules.AddRange(
                new string[] {
                    "UnrealEd"
                }
            );
        }
	}
}
