// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AnimationEditor : ModuleRules
{
	public AnimationEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
                "InputCore",
				"Slate",
				"SlateCore",
                "EditorStyle",
                "UnrealEd",
                "Persona",
                "SkeletonEditor",
                "Kismet",
                "AnimGraph",
                "SequenceRecorder",
            }
		);

        PrivateIncludePathModuleNames.AddRange(
            new string[] {
                "PropertyEditor",
            }
        );

        DynamicallyLoadedModuleNames.AddRange(
            new string[] {
                "PropertyEditor",
            }
        );
    }
}
