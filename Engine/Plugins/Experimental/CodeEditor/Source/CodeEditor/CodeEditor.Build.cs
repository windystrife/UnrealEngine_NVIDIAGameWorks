// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class CodeEditor : ModuleRules
	{
		public CodeEditor(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateIncludePaths.AddRange(
				new string[] {
					"Developer/CodeEditor/Private",
				}
				);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"SlateCore",
					"Slate",
					"AssetTools",
					"UnrealEd",
					"EditorStyle",
					"PropertyEditor",
					"Kismet",  // for FWorkflowCentricApplication
					"InputCore",
					"DirectoryWatcher",
					"LevelEditor"
				}
				);
		}
	}
}
