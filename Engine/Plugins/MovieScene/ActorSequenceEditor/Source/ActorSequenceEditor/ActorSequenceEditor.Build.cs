// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ActorSequenceEditor : ModuleRules
{
	public ActorSequenceEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"ActorSequence",
				"Core",
				"CoreUObject",
				"Engine",
				"InputCore",
				"Kismet",
				"MovieScene",
				"MovieSceneTools",
				"Sequencer",
				"EditorStyle",
				"Slate",
				"SlateCore",
				"UnrealEd",
			}
		);

		PrivateIncludePaths.AddRange(
			new string[] {
				"ActorSequenceEditor/Private",
			}
		);

		var DynamicModuleNames = new string[] {
			"LevelEditor",
			"PropertyEditor",
		};

		foreach (var Name in DynamicModuleNames)
		{
			PrivateIncludePathModuleNames.Add(Name);
			DynamicallyLoadedModuleNames.Add(Name);
		}
	}
}
