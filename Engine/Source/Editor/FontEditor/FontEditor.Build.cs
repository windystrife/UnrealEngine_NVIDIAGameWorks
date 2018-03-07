// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class FontEditor : ModuleRules
{
	public FontEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
                "AppFramework",
				"AssetRegistry",
				"Core",
				"CoreUObject",
				"ContentBrowser",
				"DesktopPlatform",
				"DesktopWidgets",
				"Engine",
                "InputCore",
				"Slate",
				"SlateCore",
                "EditorStyle",
				"UnrealEd",
				"PropertyEditor",
			}
		);

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"DesktopPlatform",
				"MainFrame",
				"UnrealEd",
			}
		);

		DynamicallyLoadedModuleNames.AddRange(
			new string[] {
				"MainFrame",
				"WorkspaceMenuStructure",
				"MainFrame",
			}
		);
	}
}
