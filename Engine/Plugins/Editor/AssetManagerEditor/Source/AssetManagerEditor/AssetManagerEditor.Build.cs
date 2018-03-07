// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AssetManagerEditor : ModuleRules
{
	public AssetManagerEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[] { 
				"Core",
				"CoreUObject",
				"Engine",
				"TargetPlatform"
			}
		);
		
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Slate",
				"SlateCore",
				"InputCore",
				"UnrealEd",
				"AssetRegistry",
				"Json",
				"CollectionManager",
				"ContentBrowser",
				"WorkspaceMenuStructure",
				"EditorStyle",
				"AssetTools",
				"PropertyEditor",
				"GraphEditor",
				"BlueprintGraph",
				"KismetCompiler",
				"ReferenceViewer",
				"SandboxFile"
			}
		);
	}
}
