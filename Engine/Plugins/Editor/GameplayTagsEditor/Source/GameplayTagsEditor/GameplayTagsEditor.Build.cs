// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class GameplayTagsEditor : ModuleRules
	{
		public GameplayTagsEditor(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateIncludePaths.AddRange(
				new string[] {
					"Editor/GameplayTagsEditor/Private",
					"Developer/AssetTools/Private",
				}
			);

			PublicIncludePathModuleNames.AddRange(
				new string[] {
					"AssetTools",
					"AssetRegistry",
				}
			);


			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					// ... add private dependencies that you statically link with here ...
					"Core",
					"CoreUObject",
					"Engine",
					"AssetTools",
					"AssetRegistry",
					"GameplayTags",
					"InputCore",
					"Slate",
					"SlateCore",
					"EditorStyle",
					"BlueprintGraph",
					"KismetCompiler",
					"GraphEditor",
					"ContentBrowser",
					"MainFrame",
					"UnrealEd",
					"SourceControl",
				}
			);

			PrivateIncludePathModuleNames.AddRange(
				new string[] {
					"Settings"
				}
			);
		}
	}
}
