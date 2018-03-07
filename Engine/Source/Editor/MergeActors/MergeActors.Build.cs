// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MergeActors : ModuleRules
{
	public MergeActors(ReadOnlyTargetRules Target) : base(Target)
	{
        PrivateIncludePaths.AddRange(
            new string[] {
				"Editor/MergeActors/Private",
				"Editor/MergeActors/Private/MeshMergingTool",
				"Editor/MergeActors/Private/MeshProxyTool"
			}
        );

        PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"AssetRegistry",
                "ContentBrowser",
                "Documentation",
                "LevelEditor",
                "MeshUtilities",
                "PropertyEditor",
                "RawMesh",
                "WorkspaceMenuStructure",
                "MeshReductionInterface",
                "MeshMergeUtilities",
            }
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
                "InputCore",
				"Slate",
				"SlateCore",
                "EditorStyle",
				"UnrealEd"
            }
		);

		DynamicallyLoadedModuleNames.AddRange(
			new string[] {
				"AssetRegistry",
                "ContentBrowser",
                "Documentation",
                "LevelEditor",
                "MeshUtilities",
                "MeshMergeUtilities",
                "MeshReductionInterface",
            }
		);
	}
}
