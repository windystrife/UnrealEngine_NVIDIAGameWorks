// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

namespace UnrealBuildTool.Rules
{
	public class GameplayAbilitiesEditor : ModuleRules
	{
		public GameplayAbilitiesEditor(ReadOnlyTargetRules Target) : base(Target)
		{
			var EngineDir = Path.GetFullPath(Target.RelativeEnginePath);

            // These nodes are not public so are hard to subclass

			PrivateIncludePaths.AddRange(
				new string[] {
					"GameplayAbilitiesEditor/Private",
					 Path.Combine(EngineDir, @"Source/Editor/GraphEditor/Private"),
					 Path.Combine(EngineDir, @"Source/Editor/Kismet/Private"),
					 Path.Combine(EngineDir, @"Source/Developer/AssetTools/Private")
				});

			PublicDependencyModuleNames.Add("GameplayTasks");

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					// ... add private dependencies that you statically link with here ...
					"Core",
					"CoreUObject",
					"Engine",
					"AssetTools",
					"ClassViewer",
					"GameplayTags",
					"GameplayTagsEditor",
					"GameplayAbilities",
					"GameplayTasksEditor",
					"InputCore",
					"PropertyEditor",
					"Slate",
					"SlateCore",
					"EditorStyle",
					"BlueprintGraph",
					"Kismet",
					"KismetCompiler",
					"GraphEditor",
					"MainFrame",
					"UnrealEd",
					"WorkspaceMenuStructure",
					"ContentBrowser",
					"EditorWidgets",
					"SourceControl",
				}
			);
		}
	}
}
