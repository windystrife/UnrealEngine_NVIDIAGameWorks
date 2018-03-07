// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MovieSceneCapture : ModuleRules
{
	public MovieSceneCapture(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePaths.AddRange(
			new string[] {
				"Runtime/MovieSceneCapture/Private"
			}
		);

		if (Target.bBuildDeveloperTools)
		{
			PublicIncludePathModuleNames.Add("ImageWrapper");
			DynamicallyLoadedModuleNames.Add("ImageWrapper");
		}

		PublicDependencyModuleNames.AddRange(
			new string[] {
				"LevelSequence",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
                "AssetRegistry",
				"AVIWriter",
                "Core",
				"CoreUObject",
				"Engine",
				"InputCore",
				"Json",
				"JsonUtilities",
				"MovieScene",
                "MovieSceneTracks",
                "RenderCore",
				"RHI",
				"ShaderCore",
				"Slate",
				"SlateCore",
			}
		);
    }
}
