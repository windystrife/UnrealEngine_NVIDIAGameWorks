// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Paper2DEditor : ModuleRules
{
	public Paper2DEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePaths.Add("Paper2DEditor/Private");

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"ApplicationCore",
				"Json",
				"Slate",
				"SlateCore",
				"Engine",
				"InputCore",
				"UnrealEd", // for FAssetEditorManager
				"KismetWidgets",
				"Kismet",  // for FWorkflowCentricApplication
				"PropertyEditor",
				"RenderCore",
				"Paper2D",
				"ContentBrowser",
				"WorkspaceMenuStructure",
				"EditorStyle",
				"MeshPaint",
				"EditorWidgets",
				"Projects"
			});

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"Settings",
				"IntroTutorials",
				"AssetTools",
				"LevelEditor"
			});

		DynamicallyLoadedModuleNames.AddRange(
			new string[] {
				"AssetTools"
			});

	}
}
