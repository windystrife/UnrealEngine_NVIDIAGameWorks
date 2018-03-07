// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ActorPickerMode : ModuleRules
{
    public ActorPickerMode(ReadOnlyTargetRules Target) : base(Target)
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
			}
		);
	}
}
