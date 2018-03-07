// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class AnimGraph : ModuleRules
{
	public AnimGraph(ReadOnlyTargetRules Target) : base(Target)
	{
        PrivateIncludePaths.AddRange(
            new string[] {
                "Editor/AnimGraph/Private",
            }
        );

        PublicDependencyModuleNames.AddRange(
			new string[] { 
				"Core", 
				"CoreUObject", 
				"Engine", 
				"Slate",
				"AnimGraphRuntime",
				"BlueprintGraph",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"InputCore",
				"SlateCore",
				"UnrealEd",
                "GraphEditor",
				"PropertyEditor",
				"EditorStyle",
                "ContentBrowser",
            }
		);

        CircularlyReferencedDependentModules.AddRange(
			new string[] {
                "UnrealEd",
                "GraphEditor",
            }
		);

        PrivateIncludePathModuleNames.AddRange(
            new string[] {
                "Persona",
                "SkeletonEditor",
                "AdvancedPreviewScene",
            }
        );
    }
}
