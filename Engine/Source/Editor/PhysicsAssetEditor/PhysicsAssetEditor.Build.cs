// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class PhysicsAssetEditor : ModuleRules
{
	public PhysicsAssetEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicIncludePaths.Add("Editor/UnrealEd/Public");

        PrivateIncludePaths.AddRange(
            new string[] {
                "Editor/PhysicsAssetEditor/Private",
                "Editor/PhysicsAssetEditor/Private/PhysicsAssetGraph",
            }
        );

        PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
                "InputCore",
				"RenderCore",
				"Slate",
				"SlateCore",
                "EditorStyle",
				"LevelEditor",
				"UnrealEd",
                "Kismet",
                "Persona",
                "SkeletonEditor",
                "GraphEditor",
                "AnimGraph",
                "AnimGraphRuntime",
                "AdvancedPreviewScene",
            }
        );

		DynamicallyLoadedModuleNames.AddRange(
			new string[] {
				"MainFrame",
				"PropertyEditor",
                "MeshUtilities",
			}
		);
	}
}
