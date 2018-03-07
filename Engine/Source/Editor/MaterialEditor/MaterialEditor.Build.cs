// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MaterialEditor : ModuleRules
{
	public MaterialEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePaths.AddRange(
			new string[] {
				"Editor/MaterialEditor/Private"
			}
		);

		PrivateIncludePathModuleNames.AddRange(
			new string[] 
			{
				"AssetRegistry", 
				"AssetTools",
				"Kismet",
				"EditorWidgets"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
                "AppFramework",
				"Core",
				"CoreUObject",
				"ApplicationCore",
				"InputCore",
				"Engine",
				"Slate",
				"SlateCore",
                "EditorStyle",
				"ShaderCore",
				"RenderCore",
				"RHI",
				"UnrealEd",
                "MaterialUtilities",
                "PropertyEditor",
				"GraphEditor",
                "AdvancedPreviewScene",
			}
		);

		DynamicallyLoadedModuleNames.AddRange(
			new string[] {
				"AssetTools",
				"MainFrame",
				"SceneOutliner",
				"ClassViewer",
				"ContentBrowser",
				"WorkspaceMenuStructure"
			}
		);
	}
}
