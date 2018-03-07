// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UMGEditor : ModuleRules
{
	public UMGEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePaths.AddRange(
			new string[] {
				"Editor/UMGEditor/Private", // For PCH includes (because they don't work with relative paths, yet)
				"Editor/UMGEditor/Private/Templates",
				"Editor/UMGEditor/Private/Extensions",
				"Editor/UMGEditor/Private/Customizations",
				"Editor/UMGEditor/Private/BlueprintModes",
				"Editor/UMGEditor/Private/TabFactory",
				"Editor/UMGEditor/Private/Designer",
				"Editor/UMGEditor/Private/Hierarchy",
				"Editor/UMGEditor/Private/Palette",
				"Editor/UMGEditor/Private/Details",
				"Editor/UMGEditor/Private/DragDrop",
                "Editor/UMGEditor/Private/Utility",
			});

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"AssetTools",
				"UMG",
			});

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"ApplicationCore",
				"InputCore",
				"Slate",
				"Engine",
				"AssetTools",
				"UnrealEd", // for FAssetEditorManager
				"KismetWidgets",
				"KismetCompiler",
				"BlueprintGraph",
				"GraphEditor",
				"Kismet",  // for FWorkflowCentricApplication
				"PropertyEditor",
				"UMG",
				"EditorStyle",
				"Slate",
				"SlateCore",
				"MessageLog",
				"MovieScene",
				"MovieSceneTools",
                "MovieSceneTracks",
				"Sequencer",
				"DetailCustomizations",
                "Settings",
				"RenderCore",
                "TargetPlatform"
			}
			);
	}
}
