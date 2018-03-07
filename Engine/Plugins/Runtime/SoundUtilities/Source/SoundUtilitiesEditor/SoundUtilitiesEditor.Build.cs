// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class SoundUtilitiesEditor : ModuleRules
	{
		public SoundUtilitiesEditor(ReadOnlyTargetRules Target) : base(Target)
        {
			PublicDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"CoreUObject",
					"Engine",
					"UnrealEd",
					"AudioEditor",
					"SoundUtilities",
					"AudioMixer",
                    "EditorStyle",
                    "Slate",
                    "SlateCore",
                    "ContentBrowser",
				}
			);

			PrivateIncludePathModuleNames.AddRange(
			new string[] {
					"AssetTools",
                    "AudioEditor",
            });
		}
	}
}