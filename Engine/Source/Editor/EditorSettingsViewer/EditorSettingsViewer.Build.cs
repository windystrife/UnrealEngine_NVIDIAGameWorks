// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class EditorSettingsViewer : ModuleRules
	{
		public EditorSettingsViewer(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[] {
					"Core",
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"CoreUObject",
					"Engine",
					"GraphEditor",
					"InputBindingEditor",
					"MessageLog",
					"SettingsEditor",
					"Slate",
					"SlateCore",
					"UnrealEd",
                    "InternationalizationSettings",
					"BlueprintGraph",
                    "EditorStyle",
                    "Analytics",
                    "VREditor"
				}
			);

			PrivateIncludePathModuleNames.AddRange(
				new string[] {
					"Settings",
				}
			);

			PrivateIncludePaths.AddRange(
				new string[] {
					"Editor/EditorSettingsViewer/Private",
				}
			);
		}
	}
}
