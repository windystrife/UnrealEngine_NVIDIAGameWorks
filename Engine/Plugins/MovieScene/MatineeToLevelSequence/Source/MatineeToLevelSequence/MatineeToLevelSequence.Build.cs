// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MatineeToLevelSequence : ModuleRules
{
	public MatineeToLevelSequence(ReadOnlyTargetRules Target) : base(Target)
	{
        DynamicallyLoadedModuleNames.AddRange(
            new string[] {
				"AssetTools",
			}
        );
        
        PrivateDependencyModuleNames.AddRange(
			new string[] {
                "LevelSequence",
				"Core",
				"CoreUObject",
                "EditorStyle",
                "Engine",
				"LevelEditor",
				"MovieScene",
                "MovieSceneTools",
				"MovieSceneTracks",
                "Slate",
                "SlateCore",
                "UnrealEd",
            }
        );

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"AssetTools",
                "MovieSceneTools",
                "Settings",
			}
		);

        PrivateIncludePaths.AddRange(
            new string[] {
                "LevelSequenceEditor/Private",
            }
        );
    }
}
