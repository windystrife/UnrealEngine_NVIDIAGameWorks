// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class LevelSequenceEditor : ModuleRules
{
	public LevelSequenceEditor(ReadOnlyTargetRules Target) : base(Target)
	{
        DynamicallyLoadedModuleNames.AddRange(
            new string[] {
				"AssetTools",
				"SceneOutliner",
				"PlacementMode",
			}
        );
        
        PrivateDependencyModuleNames.AddRange(
			new string[] {
				"AppFramework",
                "LevelSequence",
				"BlueprintGraph",
                "CinematicCamera",
				"Core",
				"CoreUObject",
                "EditorStyle",
                "Engine",
                "InputCore",
				"LevelEditor",
				"MovieScene",
                "MovieSceneTools",
				"MovieSceneTracks",
                "PropertyEditor",
				"Sequencer",
                "Slate",
                "SlateCore",
                "UnrealEd",
                "VREditor"
			}
		);

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"AssetTools",
                "MovieSceneTools",
				"SceneOutliner",
				"PlacementMode",
                "Settings",
                "MovieSceneCaptureDialog",
			}
		);

        PrivateIncludePaths.AddRange(
            new string[] {
				"LevelSequenceEditor/Private",
				"LevelSequenceEditor/Private/AssetTools",
                "LevelSequenceEditor/Private/Customizations",
				"LevelSequenceEditor/Private/Factories",
                "LevelSequenceEditor/Private/Misc",
				"LevelSequenceEditor/Private/Styles",
			}
        );
	}
}
