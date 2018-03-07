// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ContentBrowser : ModuleRules
{
	public ContentBrowser(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"AssetRegistry",
				"AssetTools",
				"CollectionManager",
				"EditorWidgets",
				"GameProjectGeneration",
                "MainFrame",
				"PackagesDialog",
				"SourceControl",
				"SourceControlWindows",
                "ReferenceViewer",
                "SizeMap",
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
				"SourceControl",
				"SourceControlWindows",
				"WorkspaceMenuStructure",
				"UnrealEd",
				"EditorWidgets",
				"Projects",
				"AddContentDialog",
				"DesktopPlatform",
			}
		);

		DynamicallyLoadedModuleNames.AddRange(
			new string[] {
				"PropertyEditor",
				"PackagesDialog",
				"AssetRegistry",
				"AssetTools",
				"CollectionManager",
				"GameProjectGeneration",
                "MainFrame",
                "ReferenceViewer",
                "SizeMap",
			}
		);
		
		PublicIncludePathModuleNames.AddRange(
            new string[] {                
                "IntroTutorials"
            }
        );
	}
}
