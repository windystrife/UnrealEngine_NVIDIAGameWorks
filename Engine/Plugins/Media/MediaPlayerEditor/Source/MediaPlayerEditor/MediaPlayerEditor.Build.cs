// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MediaPlayerEditor : ModuleRules
{
	public MediaPlayerEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		DynamicallyLoadedModuleNames.AddRange(
			new string[] {
				"AssetTools",
				"MainFrame",
				"Media",
				"WorkspaceMenuStructure",
			});

		PrivateIncludePaths.AddRange(
			new string[] {
				"MediaPlayerEditor/Private",
				"MediaPlayerEditor/Private/AssetTools",
				"MediaPlayerEditor/Private/Customizations",
				"MediaPlayerEditor/Private/Factories",
				"MediaPlayerEditor/Private/Models",
				"MediaPlayerEditor/Private/Shared",
				"MediaPlayerEditor/Private/Toolkits",
				"MediaPlayerEditor/Private/Widgets",
			});

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"AudioMixer",
				"ContentBrowser",
				"Core",
				"CoreUObject",
				"ApplicationCore",
				"DesktopPlatform",
				"DesktopWidgets",
				"EditorStyle",
				"EditorWidgets",
				"Engine",
				"InputCore",
				"MediaAssets",
				"MediaUtils",
				"PropertyEditor",
				"RenderCore",
				"RHI",
				"ShaderCore",
				"Slate",
				"SlateCore",
				"TextureEditor",
				"UnrealEd",
			});

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"AssetTools",
				"Media",
				"UnrealEd",
				"WorkspaceMenuStructure",
			});
	}
}
