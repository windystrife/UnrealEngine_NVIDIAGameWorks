// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MovieSceneCaptureDialog : ModuleRules
{
	public MovieSceneCaptureDialog(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePaths.AddRange(
			new string[] {
				"Editor/MovieSceneCaptureDialog/Private"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"EditorStyle",
				"Engine",
				"InputCore",
				"Json",
				"JsonUtilities",
				"MovieScene",
				"MovieSceneCapture",
				"PropertyEditor",
				"SessionServices",
				"Slate",
				"SlateCore",
				"UnrealEd",
			}
		);

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"LevelEditor"
			}
		);
	}
}
