// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class LevelSequence : ModuleRules
{
	public LevelSequence(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePaths.Add("Runtime/LevelSequence/Private");

		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
				"MovieScene",
				"MovieSceneTracks",
				"UMG",
			}
		);

		if (Target.bBuildEditor == true)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"ActorPickerMode",
					"PropertyEditor",
				}
			);

			PrivateIncludePathModuleNames.AddRange(
				new string[] {
					"SceneOutliner"
				}
			);
		}
	}
}
