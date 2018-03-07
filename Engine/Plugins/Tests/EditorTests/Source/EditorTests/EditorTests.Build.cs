// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class EditorTests : ModuleRules
{
	public EditorTests(ReadOnlyTargetRules Target) : base(Target)
	{

		PublicIncludePaths.AddRange(
			new string[] {
				"EditorTests/Public"
				// ... add public include paths required here ...
			}
		);


		PrivateIncludePaths.AddRange(
			new string[] {
				"EditorTests/Private",
				// ... add other private include paths required here ...
			}
		);


		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				// ... add other public dependencies that you statically link with here ...
			}
		);


		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Projects",
				"ApplicationCore",
				"InputCore",
				"LevelEditor",
				"CoreUObject",
				"ShaderCore",
				"Engine",
				"Slate",
				"SlateCore",
				"AssetTools",
				"MainFrame",
				"MaterialEditor",
				"JsonUtilities",
				"Analytics",
				"ContentBrowser",
				"EditorStyle",
				"SourceControl",
				"RHI",
				"BlueprintGraph",
				"AddContentDialog",
				"GraphEditor",
				"DirectoryWatcher",
				"Projects",
				"UnrealEd",
				"AudioEditor",
				"AnimGraphRuntime",
				"MeshMergeUtilities",
				"MaterialBaking"
			}
		);


		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
		);
	}
}
