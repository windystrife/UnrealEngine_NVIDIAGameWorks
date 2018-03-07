// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class ProjectSettingsViewer : ModuleRules
	{
		public ProjectSettingsViewer(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"CoreUObject",
					"Engine",
					"EngineSettings",
					"SettingsEditor",
					"Slate",
					"SlateCore",
					"UnrealEd",
					"MoviePlayer",
                    "AIModule",
					"ProjectTargetPlatformEditor",
                    "EditorStyle",
				}
			);

			PrivateIncludePaths.AddRange(
				new string[]
				{
					"Editor/ProjectSettingsViewer/Private",
				}
			);
		}
	}
}
